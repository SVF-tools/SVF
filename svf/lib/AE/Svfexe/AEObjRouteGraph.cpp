//===- AEObjRouteGraph.cpp - Static base/sub routes -------------------------===//

#include "AE/Svfexe/AEObjRouteGraph.h"

#include <deque>
#include <ostream>
#include <algorithm>

#include "MemoryModel/PointerAnalysis.h"
#include "SVFIR/SVFIR.h"
#include "Util/SVFUtil.h"

using namespace SVF;

AEObjRouteGraph::AEObjRouteGraph(const SVFG& svfg, ICFG& graph,
        PointerAnalysis& pta, const SVFIR& svfir) : icfg(graph)
{
    Map<NodeID, Set<const ICFGNode*>> eventsByBase;
    for (auto it = svfg.begin(); it != svfg.end(); ++it)
    {
        const VFGNode* vfg = it->second;
        const ICFGNode* location = vfg->getICFGNode();
        if (!location || !location->getFun())
            continue;

        for (const VFGEdge* edge : vfg->getInEdges())
        {
            const auto* call = SVFUtil::dyn_cast<CallIndSVFGEdge>(edge);
            if (!call)
                continue;
            const ICFGNode* caller = call->getSrcNode()->getICFGNode();
            if (!caller)
                continue;
            for (NodeID obj : call->getPointsTo())
            {
                if (svfir.isNullOrBlkObj(obj))
                    continue;
                const NodeID base = svfir.getBaseObject(obj)->getId();
                eventsByBase[base].insert(caller);
                eventsByBase[base].insert(location);
                recordOriginal(base, caller, call->getSrcNode());
                recordOriginal(base, location, vfg);
                addEdge(base, caller, location, nullptr);
            }
        }

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
            continue;
        }

        const ValVar* pointer = nullptr;
        if (const auto* store = SVFUtil::dyn_cast<StoreVFGNode>(vfg))
            pointer = store->getLHSVar();
        else if (const auto* load = SVFUtil::dyn_cast<LoadVFGNode>(vfg))
            pointer = load->getRHSVar();
        if (!pointer)
            continue;

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

    for (auto it = icfg.begin(); it != icfg.end(); ++it)
    {
        const auto* ret = SVFUtil::dyn_cast<RetICFGNode>(it->second);
        if (!ret)
            continue;
        for (const VFGNode* target : ret->getVFGNodes())
        {
            for (const VFGEdge* edge : target->getInEdges())
            {
                const auto* output = SVFUtil::dyn_cast<RetIndSVFGEdge>(edge);
                if (!output)
                    continue;
                const ICFGNode* source = output->getSrcNode()->getICFGNode();
                if (!source)
                    continue;
                for (NodeID obj : output->getPointsTo())
                {
                    if (svfir.isNullOrBlkObj(obj))
                        continue;
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

    for (const auto& [base, events] : eventsByBase)
    {
        Set<const ICFGNode*> sources = events;
        for (const ICFGNode* event : events)
            sources.insert(icfg.getFunEntryICFGNode(event->getFun()));

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

    // Branch sites discovered by connectFrom are not in the initial event set.
    for (const auto& owned : nodes)
    {
        Node* node = owned.get();
        if (!node->branch && isBranch(node->location))
            for (const VFGNode* vfg : node->location->getVFGNodes())
                if (SVFUtil::isa<BranchVFGNode>(vfg))
                    node->originals.push_back(vfg);
    }
}

bool AEObjRouteGraph::isBranch(const ICFGNode* node) const
{
    for (const ICFGEdge* out : node->getOutEdges())
    {
        const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(out);
        if (intra && intra->getCondition())
            return true;
    }
    return false;
}

AEObjRouteGraph::Node* AEObjRouteGraph::getGroupNode(
    NodeID base, const ICFGNode* location)
{
    Node*& node = groupNodes[base][location];
    if (!node)
    {
        nodes.emplace_back(new Node{static_cast<NodeID>(nodes.size()), base,
                                    location, nullptr, {}});
        node = nodes.back().get();
    }
    return node;
}

AEObjRouteGraph::Node* AEObjRouteGraph::getBranchNode(
    NodeID base, const IntraCFGEdge* branch)
{
    Node*& node = branchNodes[base][branch];
    if (!node)
    {
        nodes.emplace_back(new Node{static_cast<NodeID>(nodes.size()), base,
                                    branch->getSrcNode(), branch, {}});
        node = nodes.back().get();
    }
    return node;
}

void AEObjRouteGraph::recordOriginal(NodeID base,
        const ICFGNode* location, const VFGNode* original)
{
    Node* node = getGroupNode(base, location);
    if (std::find(node->originals.begin(), node->originals.end(), original) ==
            node->originals.end())
        node->originals.push_back(original);
}

void AEObjRouteGraph::addEdge(NodeID base, const ICFGNode* source,
                                   const ICFGNode* target,
                                   const IntraCFGEdge* branch)
{
    Node* from = getGroupNode(base, source);
    Node* to = getGroupNode(base, target);
    Node* input = branch ? getBranchNode(base, branch) : from;
    auto& targetInputs = inputs[target];
    for (const Edge& edge : targetInputs)
        if (edge.source == input && edge.target == to)
            return;
    targetInputs.push_back({input, to});
}

void AEObjRouteGraph::connectFrom(
    NodeID base, const ICFGNode* source,
    const Set<const ICFGNode*>& events,
    Set<const ICFGNode*>& newSources)
{
    const FunObjVar* fun = source->getFun();
    if (!fun)
        return;

    std::vector<std::pair<const ICFGNode*, const IntraCFGEdge*>> first;
    if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(source))
    {
        const RetICFGNode* ret = call->getRetICFGNode();
        bool needsBypass = !returnedGroups[ret].count(base);
        for (const ICFGEdge* out : call->getOutEdges())
        {
            const auto* callEdge = SVFUtil::dyn_cast<CallCFGEdge>(out);
            if (callEdge && !returnedGroupsByCallee[ret][out->getDstNode()->getFun()].count(base))
                needsBypass = true;
        }
        // f_write(p) may return a new Obj value while f_noop(p) does not.
        // Keep the caller's value for the f_noop alternative.
        if (needsBypass)
            first.emplace_back(call->getRetICFGNode(), nullptr);
    }
    else
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
        Set<const ICFGNode*> visited;
        std::deque<const ICFGNode*> pending{start};
        while (!pending.empty())
        {
            const ICFGNode* current = pending.front();
            pending.pop_front();
            if (!current || current->getFun() != fun ||
                    !visited.insert(current).second)
                continue;
            const bool event = events.count(current) || isBranch(current) ||
                               SVFUtil::isa<CallICFGNode>(current) ||
                               SVFUtil::isa<RetICFGNode>(current);
            if (current == source || event)
            {
                addEdge(base, source, current, branch);
                if (event && !events.count(current))
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

const std::vector<AEObjRouteGraph::Edge>&
AEObjRouteGraph::getInputs(const ICFGNode* node) const
{
    static const std::vector<Edge> empty;
    const auto it = inputs.find(node);
    return it == inputs.end() ? empty : it->second;
}

void AEObjRouteGraph::dump(std::ostream& out) const
{
    for (const auto& owned : nodes)
    {
        const Node& node = *owned;
        out << "node " << node.id << " base " << node.base << " ICFG "
            << node.location->getId();
        if (node.branch)
            out << " branch " << node.branch->getSrcNode()->getId()
                << " -> " << node.branch->getDstNode()->getId();
        else
        {
            out << " SVFG";
            for (const VFGNode* original : node.originals)
                out << ' ' << original->getId();
        }
        out << '\n';
    }
    for (const auto& owned : nodes)
    {
        const Node& branch = *owned;
        if (branch.branch)
        {
            const Node* source = groupNodes.at(branch.base).at(branch.location);
            out << "edge " << source->id << " -> " << branch.id << '\n';
        }
    }
    for (const auto& entry : inputs)
        for (const Edge& edge : entry.second)
            out << "edge " << edge.source->id << " -> "
                << edge.target->id << '\n';
}

void AEObjRouteGraph::dumpDot(std::ostream& out) const
{
    out << "digraph AEObjRouteGraph {\n"
        << "  graph [rankdir=LR, label=\"AE base/sub value flow (static routes)\", "
           "labelloc=t];\n"
        << "  node [fontname=Helvetica, fontsize=10];\n"
        << "  edge [color=\"#475569\"];\n";

    for (const auto& owned : nodes)
    {
        const Node& node = *owned;
        out << "  n" << node.id;
        if (node.branch)
        {
            out << " [shape=diamond, style=filled, fillcolor=\"#ffedd5\", "
                   "label=\"Branch exit\\nbase " << node.base
                << "\\nICFG " << node.branch->getSrcNode()->getId()
                << " -> " << node.branch->getDstNode()->getId()
                << "\\nedge label " << node.branch->getSuccessorCondValue()
                << "\"];\n";
        }
        else
        {
            out << " [shape=box, style=filled, fillcolor=\""
                << (node.originals.empty() ? "#e5e7eb" : "#dbeafe")
                << "\", label=\"";
            out << (node.originals.empty() ? "Route junction" : "SVFG group")
                << "\\nbase " << node.base << "\\nICFG "
                << node.location->getId();
            if (!node.originals.empty())
            {
                out << "\\nSVFG";
                for (const VFGNode* original : node.originals)
                    out << ' ' << original->getId();
            }
            out << "\"];\n";
        }
    }

    for (const auto& owned : nodes)
    {
        const Node& node = *owned;
        if (node.branch)
        {
            const Node* source = groupNodes.at(node.base).at(node.location);
            out << "  n" << source->id << " -> n" << node.id << ";\n";
        }
    }
    for (const auto& entry : inputs)
        for (const Edge& edge : entry.second)
            out << "  n" << edge.source->id << " -> n"
                << edge.target->id << ";\n";
    out << "}\n";
}
