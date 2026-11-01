//===- AEObjRouteGraph.cpp - Static base/sub routes -------------------------===//

#include "AE/Svfexe/AEObjRouteGraph.h"

#include <cassert>
#include <deque>
#include <fstream>
#include <ostream>
#include <algorithm>
#include <sstream>

#include "MemoryModel/PointerAnalysis.h"
#include "SVFIR/SVFIR.h"
#include "Util/SVFUtil.h"

using namespace SVF;

std::string AEObjRouteEdge::toString() const
{
    std::ostringstream out;
    out << "edge " << getSrcID() << " -> " << getDstID();
    return out.str();
}

std::string AEObjRouteNode::toString() const
{
    std::ostringstream out;
    out << "node " << getId() << " base " << base << " ICFG "
        << location->getId();
    if (branch)
        out << " branch " << branch->getSrcNode()->getId()
            << " -> " << branch->getDstNode()->getId();
    else
    {
        out << " SVFG";
        for (const VFGNode* original : originals)
            out << ' ' << original->getId();
    }
    return out.str();
}

AEObjRouteGraph::AEObjRouteGraph(const SVFG& svfg, ICFG& graph,
        PointerAnalysis& pta, const SVFIR& svfir) : icfg(graph)
{
    //Step 1 collect key: All base Obj ID, value: all def-use sites related to the base id
    Map<NodeID, Set<const ICFGNode*>> eventsByBase;
    for (auto it = svfg.begin(); it != svfg.end(); ++it)
    {
        const VFGNode* vfg = it->second;
        const ICFGNode* location = vfg->getICFGNode();
        // Only in-function SVFG nodes participate here; global entry routes
        // are connected below.
        if (location && location->getFun())
        {
            // An indirect call edge identifies the Obj group passed to the
            // callee: C: f(&x) -> P: callee entry. Register both C and P as
            // events for x so intra-function routing stops and resumes there;
            // addEdge connects the interprocedural C -> P edge separately.
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
                                recordOriginal(base, caller, call->getSrcNode());
                                recordOriginal(base, location, vfg);
                                addEdge(base, caller, location, nullptr);
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
                {
                    if (!svfir.isNullOrBlkObj(obj))
                    {
                        const NodeID base = svfir.getBaseObject(obj)->getId();
                        eventsByBase[base].insert(location);
                        recordOriginal(base, location, vfg);
                    }
                }
            }
            else if (const auto* store = SVFUtil::dyn_cast<StoreVFGNode>(vfg))
                pointer = store->getLHSVar();
            else if (const auto* load = SVFUtil::dyn_cast<LoadVFGNode>(vfg))
                pointer = load->getRHSVar();

            // Resolve ordinary load/store targets with points-to analysis.
            // If p points to a[1], record a's base so a[0] and a[1] travel
            // through this location as one group.
            if (pointer)
            {
                for (NodeID obj : pta.getPts(pointer->getId()))
                {
                    if (!svfir.isNullOrBlkObj(obj))
                    {
                        const NodeID base = svfir.getBaseObject(obj)->getId();
                        eventsByBase[base].insert(location);
                        recordOriginal(base, location, vfg);
                    }
                }
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
                                    returnedGroups[ret].insert(base);
                                    if (source->getFun())
                                        returnedGroupsByCallee[ret][source->getFun()].insert(base);
                                    eventsByBase[base].insert(ret);
                                    if (source->getFun())
                                        eventsByBase[base].insert(source);
                                    recordOriginal(base, source, output->getSrcNode());
                                    recordOriginal(base, ret, target);
                                    addEdge(base, source, ret, nullptr);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Step 3: connect consecutive Obj events in ICFG order within each function.
    // D: a[0]=1; S: a[1]=7; N: use(a[0]) becomes D -> S -> N.
    // Branches, calls, and returns also stop a route, even if they were not
    // among the initial SVFG events.
    for (const auto& [base, events] : eventsByBase)
    {
        Set<const ICFGNode*> sources = events;
        for (const ICFGNode* event : events)
            sources.insert(icfg.getFunEntryICFGNode(event->getFun()));

        // Connect global objects from global initialization to function entries.
        if (svfir.getBaseObject(base)->isGlobalObj())
        {
            const ICFGNode* global = icfg.getGlobalICFGNode();
            for (const ICFGEdge* edge : global->getOutEdges())
            {
                const ICFGNode* entry = edge->getDstNode();
                if (SVFUtil::isa<FunEntryICFGNode>(entry) && sources.count(entry))
                    addEdge(base, global, entry, nullptr);
            }
        }

        // Branch/call/return sites found by connectFrom become sources for
        // the next segment. This queue is used only during graph construction.
        std::vector<const ICFGNode*> pending(sources.begin(), sources.end());
        for (size_t i = 0; i < pending.size(); ++i)
        {
            Set<const ICFGNode*> newSources;
            connectFrom(base, pending[i], events, newSources);
            for (const ICFGNode* event : newSources)
            {
                if (sources.insert(event).second)
                    pending.push_back(event);
            }
        }
    }

    // Some branch sites were found along the ICFG, not as initial SVFG
    // events. Keep their BranchVFGNode for mapping dump output to the SVFG.
    for (const auto& entry : *this)
    {
        Node* node = entry.second;
        if (!node->branch && isBranch(node->location))
            for (const VFGNode* vfg : node->location->getVFGNodes())
                if (SVFUtil::isa<BranchVFGNode>(vfg))
                    node->originals.push_back(vfg);
    }
}

bool AEObjRouteGraph::isBranch(const ICFGNode* node) const
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

AEObjRouteGraph::Node* AEObjRouteGraph::getOrCreateBaseObjNode(
    NodeID baseObjId, const ICFGNode* location)
{
    // One node per (base, ICFG location) may group several SVFG nodes.
    Node*& node = groupNodes[baseObjId][location];
    if (!node)
    {
        node = new Node(getTotalNodeNum(), baseObjId, location, nullptr);
        addGNode(node->getId(), node);
    }
    return node;
}

AEObjRouteGraph::Node* AEObjRouteGraph::getBranchNode(
    NodeID base, const IntraCFGEdge* branch)
{
    // B: if (x > 0) -> N: use(x). B's true and false edges have distinct
    // nodes; the nodes store edge identity, not AE's refined interval.
    Node*& node = branchNodes[base][branch];
    if (!node)
    {
        node = new Node(getTotalNodeNum(), base, branch->getSrcNode(), branch);
        addGNode(node->getId(), node);
    }
    return node;
}

// create link between baseID to subID
// e.g. Store a[1], 10;
// SVFGNode ID a(base):100, a[1](gep):101
// we record mapping from 100 to 101
// if base is normal objid, still record, but record the same node
void AEObjRouteGraph::recordOriginal(NodeID base,
        const ICFGNode* location, const VFGNode* original)
{
    Node* node = getOrCreateBaseObjNode(base, location);
    if (std::find(node->originals.begin(), node->originals.end(), original) ==
            node->originals.end())
        node->originals.push_back(original);
}

void AEObjRouteGraph::addRouteEdge(Node* source, Node* target)
{
    Edge candidate(source, target);
    if (source->hasOutgoingEdge(&candidate))
        return;

    Edge* edge = new Edge(source, target);
    const bool addedOut = source->addOutgoingEdge(edge);
    const bool addedIn = target->addIncomingEdge(edge);
    assert(addedOut && addedIn);
    (void)addedOut;
    (void)addedIn;
    incEdgeNum();
    if (!target->branch)
        inputs[target->location].push_back(edge);
}

void AEObjRouteGraph::addEdge(NodeID base, const ICFGNode* source,
                                   const ICFGNode* target,
                                   const IntraCFGEdge* branch)
{
    // B: if (z > 0) { use(x); use(z); }, with ordinary loads and no aliasing.
    // For base=z, connectFrom passes source=B, target=N (the z use), and
    // B's true edge. The intervening use(x) adds no z edge. The same route
    // applies when N immediately follows B. inputs[N] records the branch
    // node -> N; AE checks feasibility and refines z when reading that edge.
    Node* from = getOrCreateBaseObjNode(base, source);
    Node* to = getOrCreateBaseObjNode(base, target);
    Node* input = branch ? getBranchNode(base, branch) : from;
    if (branch)
        addRouteEdge(from, input);
    addRouteEdge(input, to);
}

void AEObjRouteGraph::connectFrom(
    NodeID base, const ICFGNode* source,
    const Set<const ICFGNode*>& events,
    Set<const ICFGNode*>& newSources)
{
    // From one Obj event, find the next event or control boundary in this
    // function. Feasibility is not checked here, so AE states share the route.
    const FunObjVar* fun = source->getFun();
    if (!fun)
        return;

    std::vector<std::pair<const ICFGNode*, const IntraCFGEdge*>> first;
    // inter-procedure
    if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(source))
    {
        const RetICFGNode* ret = call->getRetICFGNode();
        bool needsCallerToRetEdge = !returnedGroups[ret].count(base);
        for (const ICFGEdge* out : call->getOutEdges())
        {
            const auto* callEdge = SVFUtil::dyn_cast<CallCFGEdge>(out);
            if (callEdge && !returnedGroupsByCallee[ret][out->getDstNode()->getFun()].count(base))
                needsCallerToRetEdge = true;
        }
        // C may call f_write(&x) or f_noop(&x). If a callee has no return
        // edge for x, retain C -> R so that candidate carries the pre-call
        // value; a candidate with a return edge uses callee-output -> R.
        if (needsCallerToRetEdge)
            first.emplace_back(call->getRetICFGNode(), nullptr);
    }
    else
    // intra-procedure
    {
        for (const ICFGEdge* out : source->getOutEdges())
        {
            if (const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(out))
                first.emplace_back(intra->getDstNode(),
                                   intra->getCondition() ? intra : nullptr);
        }
    }

    for (const auto& [start, branch] : first)
    {
        // Cross ordinary ICFG nodes between events; stop at a branch, call,
        // return, or the next Obj event, which starts another route segment.
        // For base=z: B: if (z > 0) { Ux: use(x); Uz: use(z); }.
        // With ordinary non-aliasing loads, Ux is not a z event; traverse
        // past Ux and call addEdge(z, B, Uz, true edge) at Uz.
        Set<const ICFGNode*> visited;
        std::deque<const ICFGNode*> pending{start};
        while (!pending.empty())
        {
            const ICFGNode* current = pending.front();
            pending.pop_front();
            if (current && current->getFun() == fun &&
                    visited.insert(current).second)
            {
                const bool objSite = events.count(current);
                const bool controlBoundary = isBranch(current) ||
                                             SVFUtil::isa<CallICFGNode>(current) ||
                                             SVFUtil::isa<RetICFGNode>(current);
                if (objSite || controlBoundary)
                {
                    addEdge(base, source, current, branch);
                    if (!objSite)
                        newSources.insert(current);
                }
                else
                {
                    for (const ICFGEdge* out : current->getOutEdges())
                    {
                        if (const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(out))
                            pending.push_back(intra->getDstNode());
                    }
                }
            }
        }
    }
}

const std::vector<const AEObjRouteGraph::Edge*>&
AEObjRouteGraph::getInputs(const ICFGNode* node) const
{
    static const std::vector<const Edge*> empty;
    const auto it = inputs.find(node);
    return it == inputs.end() ? empty : it->second;
}

std::string AEObjRouteGraph::toString() const
{
    std::ostringstream out;
    for (const auto& entry : *this)
        out << entry.second->toString() << '\n';
    for (const auto& entry : *this)
        for (const Edge* edge : entry.second->getOutEdges())
            out << edge->toString() << '\n';
    return out.str();
}

void AEObjRouteGraph::dump(const std::string& file) const
{
    const std::string filename = file + ".dot";
    std::ofstream out(filename);
    if (!out)
    {
        SVFUtil::errs() << "Cannot write '" << filename << "'\n";
        return;
    }
    SVFUtil::outs() << "Writing '" << filename << "'...\n";
    dumpDot(out);
}

void AEObjRouteGraph::dumpDot(std::ostream& out) const
{
    // DOT uses the same static nodes and edges, with no AE runtime values.
    out << "digraph AEObjRouteGraph {\n"
        << "  graph [rankdir=LR, label=\"AE base/sub value flow (static routes)\", "
           "labelloc=t];\n"
        << "  node [fontname=Helvetica, fontsize=10];\n"
        << "  edge [color=\"#475569\"];\n";

    for (const auto& entry : *this)
    {
        const Node* node = entry.second;
        out << "  n" << node->getId();
        if (node->branch)
        {
            out << " [shape=diamond, style=filled, fillcolor=\"#ffedd5\", "
                   "label=\"Branch edge\\nbase " << node->base
                << "\\nICFG " << node->branch->getSrcNode()->getId()
                << " -> " << node->branch->getDstNode()->getId()
                << "\\nedge label " << node->branch->getSuccessorCondValue()
                << "\"];\n";
        }
        else
        {
            out << " [shape=box, style=filled, fillcolor=\""
                << (node->originals.empty() ? "#e5e7eb" : "#dbeafe")
                << "\", label=\"";
            out << (node->originals.empty() ? "Route junction" : "SVFG group")
                << "\\nbase " << node->base << "\\nICFG "
                << node->location->getId();
            if (!node->originals.empty())
            {
                out << "\\nSVFG";
                for (const VFGNode* original : node->originals)
                    out << ' ' << original->getId();
            }
            out << "\"];\n";
        }
    }

    for (const auto& entry : *this)
        for (const Edge* edge : entry.second->getOutEdges())
            out << "  n" << edge->getSrcID() << " -> n"
                << edge->getDstID() << ";\n";
    out << "}\n";
}
