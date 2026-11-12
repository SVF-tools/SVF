//===- SparseObjGraph.cpp - Static packed Obj routes -----------------------===//

#include "Graphs/SparseObjGraph.h"

#include "Graphs/GraphPrinter.h"
#include "Graphs/SVFG.h"
#include "MemoryModel/PointerAnalysis.h"

#include <deque>

using namespace SVF;

namespace
{
std::string baseName(const SVFIR& svfir, NodeID base)
{
    const SVFVar* var = const_cast<SVFIR&>(svfir).getGNode(base);
    std::string name = var ? var->getName() : "";
    return name.empty() ? "o" + std::to_string(base) : name;
}

std::string labelText(const SVFIR& svfir, const NodeBS& bases)
{
    std::string out;
    for (NodeID base : bases)
        out += (out.empty() ? "" : ",") + baseName(svfir, base);
    return out;
}
} // namespace

std::string SparseObjEdge::toString() const
{
    std::ostringstream out;
    out << "edge " << getSrcID() << " -> " << getDstID() << " bases {";
    bool first = true;
    for (NodeID base : *bases)
    {
        out << (first ? "" : ",") << base;
        first = false;
    }
    out << "}";
    return out.str();
}

std::string SparseObjNode::toString() const
{
    std::ostringstream out;
    out << "node " << getId() << " ICFG " << location->getId();
    if (branch)
        out << " branch " << branch->getSrcNode()->getId()
            << " -> " << branch->getDstNode()->getId()
            << " label " << branch->getSuccessorCondValue();
    return out.str();
}

SparseObjGraph::SparseObjGraph(const SVFG& svfg, ICFG& graph,
                               PointerAnalysis& pta, const SVFIR& ir)
    : icfg(graph), svfir(ir)
{
    // Step 1: collect, for each base Obj, the ICFG locations of its events.
    Map<NodeID, Set<const ICFGNode*>> eventsByBase;
    for (auto it = svfg.begin(); it != svfg.end(); ++it)
    {
        const VFGNode* vfg = it->second;
        const ICFGNode* location = vfg->getICFGNode();
        // Only in-function SVFG nodes participate here; global entry routes
        // are connected below.
        if (location && location->getFun())
        {
            // An indirect call edge identifies the packed Obj passed to the
            // callee: C: f(&x) -> P: callee entry. Register both C and P as
            // events for x so intra-function routing stops and resumes there;
            // the interprocedural C -> P route is added here.
            for (const VFGEdge* edge : vfg->getInEdges())
            {
                if (const auto* call = SVFUtil::dyn_cast<CallIndSVFGEdge>(edge))
                {
                    const ICFGNode* caller = call->getSrcNode()->getICFGNode();
                    if (caller)
                    {
                        for (NodeID obj : call->getPointsTo())
                        {
                            if (!svfir.isNullOrBlkObj(obj))
                            {
                                const NodeID base = svfir.getBaseObject(obj)->getId();
                                eventsByBase[base].insert(caller);
                                eventsByBase[base].insert(location);
                                addRoute(base, caller, location, nullptr);
                            }
                        }
                    }
                }
            }

            // MRSVFGNode already supplies points-to objects. Group each by
            // base without modifying the SVFG or strong/weak update rules.
            const ValVar* pointer = nullptr;
            if (const auto* memory = SVFUtil::dyn_cast<MRSVFGNode>(vfg))
            {
                for (NodeID obj : memory->getPointsTo())
                    if (!svfir.isNullOrBlkObj(obj))
                        eventsByBase[svfir.getBaseObject(obj)->getId()].insert(location);
            }
            else if (const auto* store = SVFUtil::dyn_cast<StoreVFGNode>(vfg))
                pointer = store->getLHSVar();
            else if (const auto* load = SVFUtil::dyn_cast<LoadVFGNode>(vfg))
                pointer = load->getRHSVar();

            // Resolve ordinary load/store targets with points-to analysis.
            // If p points to a[1], record a's base so a[0] and a[1] travel
            // through this location as one packed Obj.
            if (pointer)
            {
                for (NodeID obj : pta.getPts(pointer->getId()))
                    if (!svfir.isNullOrBlkObj(obj))
                        eventsByBase[svfir.getBaseObject(obj)->getId()].insert(location);
            }
        }
    }

    // Step 2: connect callee Obj outputs to the caller return location.
    // For C: f(&x); R: return from the call, a RetIndSVFGEdge for x gives
    // callee-output -> R, where AE can read the computed Obj value.
    for (auto it = icfg.begin(); it != icfg.end(); ++it)
    {
        if (const auto* ret = SVFUtil::dyn_cast<RetICFGNode>(it->second))
        {
            for (const VFGNode* target : ret->getVFGNodes())
            {
                for (const VFGEdge* edge : target->getInEdges())
                {
                    if (const auto* output = SVFUtil::dyn_cast<RetIndSVFGEdge>(edge))
                    {
                        const ICFGNode* source = output->getSrcNode()->getICFGNode();
                        if (source)
                        {
                            for (NodeID obj : output->getPointsTo())
                            {
                                if (!svfir.isNullOrBlkObj(obj))
                                {
                                    const NodeID base = svfir.getBaseObject(obj)->getId();
                                    returnedGroups[ret].set(base);
                                    if (source->getFun())
                                        returnedGroupsByCallee[ret][source->getFun()].set(base);
                                    eventsByBase[base].insert(ret);
                                    if (source->getFun())
                                        eventsByBase[base].insert(source);
                                    addRoute(base, source, ret, nullptr);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Index events by location and by function: a base is routed inside a
    // function only if it has an event there (as in the per-base walk, whose
    // sources are the base's events and the entries of their functions).
    Map<const ICFGNode*, NodeBS> eventsAt;
    Map<const FunObjVar*, NodeBS> funBases;
    Set<NodeID> globalBases;
    for (const auto& [base, events] : eventsByBase)
    {
        if (svfir.getBaseObject(base)->isGlobalObj())
            globalBases.insert(base);
        for (const ICFGNode* event : events)
        {
            eventsAt[event].set(base);
            funBases[event->getFun()].set(base);
        }
    }

    // Connect global objects from global initialization to function entries.
    if (!globalBases.empty())
    {
        NodeBS globals;
        for (NodeID base : globalBases)
            globals.set(base);
        const ICFGNode* global = icfg.getGlobalICFGNode();
        for (const ICFGEdge* edge : global->getOutEdges())
        {
            const ICFGNode* entry = edge->getDstNode();
            if (!SVFUtil::isa<FunEntryICFGNode>(entry))
                continue;
            const auto bases = funBases.find(entry->getFun());
            if (bases == funBases.end())
                continue;
            NodeBS label = globals & bases->second;
            if (!label.empty())
                addRoute(label, global, entry, nullptr);
        }
    }

    // Step 3: connect consecutive Obj events in ICFG order, one walk per
    // function carrying base sets.
    for (const auto& [fun, bases] : funBases)
        connectFunction(fun, bases, eventsAt);

    finalize();
}

SparseObjGraph::~SparseObjGraph() = default;

bool SparseObjGraph::isBranch(const ICFGNode* node) const
{
    // Only conditional intra-function ICFG edges count as branches.
for (const ICFGEdge* out : node->getOutEdges())
{
    const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(out);
        if (intra && intra->getCondition())
            return true;
    }
    return false;
}

bool SparseObjGraph::isControlBoundary(const ICFGNode* node) const
{
    return isBranch(node) || SVFUtil::isa<CallICFGNode>(node) ||
    SVFUtil::isa<RetICFGNode>(node);
}

void SparseObjGraph::addRoute(NodeID base, const ICFGNode* source,
                              const ICFGNode* target,
                              const IntraCFGEdge* branch)
{
    NodeBS bases;
    bases.set(base);
    addRoute(bases, source, target, branch);
}

void SparseObjGraph::addRoute(const NodeBS& bases, const ICFGNode* source,
                              const ICFGNode* target,
                              const IntraCFGEdge* branch)
{
    // B: if (z > 0) { use(x); use(z); }. For base z the route is
    // B -> (B, true edge) -> Uz; AE refines B's state on the true edge
    // once and then reads every base labeled on (B, true edge) -> Uz.
    const NodeKey from{source, nullptr};
    const NodeKey to{target, nullptr};
    if (branch)
    {
        const NodeKey via{source, branch};
        routeLabels[ {from, via}] |= bases;
        routeLabels[ {via, to}] |= bases;
    }
    else
        routeLabels[ {from, to}] |= bases;
}

void SparseObjGraph::connectFunction(
    const FunObjVar* fun, const NodeBS& funBases,
    const Map<const ICFGNode*, NodeBS>& eventsAt)
{
    // A source starts a route segment for some bases: their events, the
    // function entry, and the control boundaries they reach. Each source
    // processes only newly added bases; per base this is exactly the per-base
    // walk, since a base's walk does not depend on other bases.
    doneSources.clear();
    pendingSources.clear();
    sourceWorklist.clear();

    addSource(icfg.getFunEntryICFGNode(fun), funBases);
    for (const auto& event : eventsAt)
        if (event.first->getFun() == fun)
            addSource(event.first, event.second);

    while (!sourceWorklist.empty())
    {
        const ICFGNode* source = sourceWorklist.front();
        sourceWorklist.pop_front();
        NodeBS fresh = std::move(pendingSources[source]);
        pendingSources.erase(source);
        fresh.intersectWithComplement(doneSources[source]);
        if (fresh.empty())
            continue;
        doneSources[source] |= fresh;

        if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(source))
        {
            connectCallBypass(call, fresh);
            continue;
        }
        for (const ICFGEdge* out : source->getOutEdges())
        {
            const IntraCFGEdge* intra = SVFUtil::dyn_cast<IntraCFGEdge>(out);
            if (!intra)
                continue;
            const IntraCFGEdge* branch = intra->getCondition() ? intra : nullptr;
            connectSegment(fun, source, intra->getDstNode(), branch, fresh,
                           eventsAt);
        }
    }
}

void SparseObjGraph::addSource(const ICFGNode* node, const NodeBS& bases)
{
    NodeBS fresh = bases;
    const auto done = doneSources.find(node);
    if (done != doneSources.end())
        fresh.intersectWithComplement(done->second);
    if (fresh.empty())
        return;
    NodeBS& waiting = pendingSources[node];
    if (waiting.empty())
        sourceWorklist.push_back(node);
    waiting |= fresh;
}

void SparseObjGraph::connectCallBypass(const CallICFGNode* call,
                                       const NodeBS& bases)
{
    // C may call f_write(&x) or f_noop(&x). If a callee has no return edge
    // for x, retain C -> R so that candidate carries the pre-call value; a
    // candidate with a return edge uses callee-output -> R.
    const RetICFGNode* ret = call->getRetICFGNode();
    NodeBS returned;
    const auto all = returnedGroups.find(ret);
    if (all != returnedGroups.end())
        returned = all->second;
    const auto byCallee = returnedGroupsByCallee.find(ret);
    for (const ICFGEdge* out : call->getOutEdges())
    {
        if (!SVFUtil::isa<CallCFGEdge>(out))
            continue;
        const NodeBS* calleeBases = nullptr;
        if (byCallee != returnedGroupsByCallee.end())
        {
            const auto callee = byCallee->second.find(out->getDstNode()->getFun());
            if (callee != byCallee->second.end())
                calleeBases = &callee->second;
        }
        if (calleeBases)
            returned &= *calleeBases;
        else
            returned.clear();
    }

    NodeBS bypass = bases;
    bypass.intersectWithComplement(returned);
    if (bypass.empty())
        return;
    // The return site is a control boundary: every bypassing base stops
    // there and starts its next segment.
    addRoute(bypass, call, ret, nullptr);
    addSource(ret, bypass);
}

void SparseObjGraph::connectSegment(
    const FunObjVar* fun, const ICFGNode* source, const ICFGNode* start,
    const IntraCFGEdge* branch, const NodeBS& bases,
    const Map<const ICFGNode*, NodeBS>& eventsAt)
{
    // Cross ordinary ICFG nodes between events, carrying the set of bases
    // still travelling. A base stops at its next event; every base stops at
    // a branch, call, or return.
    Map<const ICFGNode*, NodeBS> arrived;
    std::deque<std::pair<const ICFGNode*, NodeBS>> walk;
    walk.emplace_back(start, bases);
    while (!walk.empty())
    {
        const ICFGNode* current = walk.front().first;
        NodeBS travelling = std::move(walk.front().second);
        walk.pop_front();
        if (!current || current->getFun() != fun)
            continue;
        NodeBS& seen = arrived[current];
        travelling.intersectWithComplement(seen);
        if (travelling.empty())
            continue;
        seen |= travelling;

        if (isControlBoundary(current))
        {
            addRoute(travelling, source, current, branch);
            addSource(current, travelling);
            continue;
        }
        NodeBS stop = travelling & getEvents(eventsAt, current);
        if (!stop.empty())
        {
            addRoute(stop, source, current, branch);
            travelling.intersectWithComplement(stop);
        }
        if (travelling.empty())
            continue;
        for (const ICFGEdge* next : current->getOutEdges())
            if (const IntraCFGEdge* intra = SVFUtil::dyn_cast<IntraCFGEdge>(next))
                walk.emplace_back(intra->getDstNode(), travelling);
    }
}

const NodeBS& SparseObjGraph::getEvents(
    const Map<const ICFGNode*, NodeBS>& eventsAt, const ICFGNode* node)
{
    static const NodeBS empty;
    const auto it = eventsAt.find(node);
    return it == eventsAt.end() ? empty : it->second;
}

SparseObjGraph::Node* SparseObjGraph::getOrCreateNode(
    const ICFGNode* location, const IntraCFGEdge* branch)
{
    Node*& node = nodes[ {location, branch}];
    if (!node)
    {
        node = new Node(getTotalNodeNum(), location, branch);
        addGNode(node->getId(), node);
    }
    return node;
}

const NodeBS* SparseObjGraph::intern(const NodeBS& bases)
{
    u64_t hash = 1469598103934665603ULL;
    for (NodeID base : bases)
        hash = (hash ^ base) * 1099511628211ULL;
    auto& bucket = labels[hash];
    for (const auto& label : bucket)
        if (*label == bases)
            return label.get();
    bucket.push_back(std::make_unique<NodeBS>(bases));
    return bucket.back().get();
}

void SparseObjGraph::finalize()
{
    for (const auto& [key, bases] : routeLabels)
    {
        Node* source = getOrCreateNode(key.first.first, key.first.second);
        Node* target = getOrCreateNode(key.second.first, key.second.second);
        Edge* edge = new Edge(source, target, intern(bases));
        const bool addedOut = source->addOutgoingEdge(edge);
        const bool addedIn = target->addIncomingEdge(edge);
        assert(addedOut && addedIn);
        (void)addedOut;
        (void)addedIn;
        incEdgeNum();
        if (!target->branch)
            inputs[target->location].push_back(edge);
    }
    routeLabels.clear();
}

const std::vector<const SparseObjGraph::Edge*>&
SparseObjGraph::getInputs(const ICFGNode* node) const
{
    static const std::vector<const Edge*> empty;
    const auto it = inputs.find(node);
    return it == inputs.end() ? empty : it->second;
}

std::string SparseObjGraph::toString() const
{
    std::ostringstream out;
    for (const auto& entry : *this)
        out << entry.second->toString() << '\n';
    for (const auto& entry : *this)
        for (const Edge* edge : entry.second->getOutEdges())
            out << edge->toString() << '\n';
    return out.str();
}

namespace SVF
{

template<>
struct DOTGraphTraits<SparseObjGraph*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool simple = false) : DefaultDOTGraphTraits(simple) {}

    static std::string getGraphName(SparseObjGraph*)
    {
        return "Sparse object routes";
    }

    static std::string getNodeLabel(SparseObjNode* node, SparseObjGraph*)
    {
        std::ostringstream out;
        const ICFGNode* loc = node->location;
        if (node->branch)
        {
            out << "branch ICFG" << loc->getId() << " -> ICFG"
                << node->branch->getDstNode()->getId() << " ["
                << (node->branch->getSuccessorCondValue() ? "true" : "false") << "]";
            return out.str();
        }
        out << "ICFG" << loc->getId();
        if (loc->getFun())
            out << " @" << loc->getFun()->getName();
        if (SVFUtil::isa<FunEntryICFGNode>(loc)) out << " (entry)";
        else if (SVFUtil::isa<FunExitICFGNode>(loc)) out << " (exit)";
        else if (SVFUtil::isa<CallICFGNode>(loc)) out << " (call)";
        else if (SVFUtil::isa<RetICFGNode>(loc)) out << " (ret)";
        else if (SVFUtil::isa<GlobalICFGNode>(loc)) out << " (global)";
        std::string kinds;
        for (const SVFStmt* stmt : loc->getSVFStmts())
        {
            const char* k = SVFUtil::isa<StoreStmt>(stmt) ? "store"
                            : SVFUtil::isa<LoadStmt>(stmt) ? "load"
                            : SVFUtil::isa<BranchStmt>(stmt) ? "branch" : nullptr;
            if (k && kinds.find(k) == std::string::npos)
                kinds += std::string(kinds.empty() ? "" : "+") + k;
        }
        std::string line;
        {
            const std::string src = loc->getSourceLoc();
            const size_t ln = src.find("\"ln\": ");
            if (ln != std::string::npos)
                line = src.substr(ln + 6, src.find_first_of(",}", ln + 6) - ln - 6);
        }
        if (!line.empty() && line != "0")
            out << " line " << line;
        if (!kinds.empty())
            out << "\n" << kinds;
        return out.str();
    }

    static std::string getNodeAttributes(SparseObjNode* node, SparseObjGraph*)
    {
        return node->branch ? "shape=diamond,style=filled,fillcolor=\"#ffedd5\""
               : "shape=box,style=filled,fillcolor=\"#dbeafe\"";
    }

    template <class EdgeIter>
    static std::string getEdgeAttributes(SparseObjNode* node, EdgeIter it,
                                         SparseObjGraph* graph)
    {
        const SparseObjEdge* edge = *(it.getCurrent());
        std::string label = labelText(graph->getSVFIR(), edge->getBases());
        std::string attr = "label=\"{" + label + "}\"";
        const SparseObjNode* target = edge->getDstNode();
        if (node->location->getFun() != target->location->getFun())
            attr += ",color=blue,style=dashed,fontcolor=blue";
        else if (node->branch || target->branch)
            attr += ",color=\"#c2410c\",fontcolor=\"#c2410c\"";
        return attr;
    }
};

} // namespace SVF

void SparseObjGraph::dump(const std::string& file)
{
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), file, this);
}
