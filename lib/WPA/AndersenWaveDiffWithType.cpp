#include "WPA/Andersen.h"
#include "MemoryModel/PTAType.h"

using namespace SVF;
using namespace SVFUtil;

AndersenWaveDiffWithType* AndersenWaveDiffWithType::diffWaveWithType = nullptr;


/// process "bitcast" CopyCGEdge
void AndersenWaveDiffWithType::processCast(const ConstraintEdge *edge)
{
    NodeID srcId = edge->getSrcID();
    NodeID dstId = edge->getDstID();
    if (pag->hasNonlabeledEdge(pag->getPAGNode(srcId), pag->getPAGNode(dstId), PAGEdge::Copy))
    {
        const Value *val = pag->getIntraPAGEdge(srcId, dstId, PAGEdge::Copy)->getValue();
        if (val)
        {
            if (const auto *castInst = SVFUtil::dyn_cast<CastInst>(val))
            {
                updateObjType(castInst->getType(), getPts(edge->getSrcID()));
            }
            else if (const auto* ce = SVFUtil::dyn_cast<ConstantExpr>(val))
            {
                if (ce->getOpcode() == Instruction::BitCast)
                {
                    updateObjType(ce->getType(), getPts(edge->getSrcID()));
                }
            }
        }
    }
}

/// update type of objects when process "bitcast" CopyCGEdge
void AndersenWaveDiffWithType::updateObjType(const Type *type, const PointsTo &objs)
{
    for (const auto& obj : objs)
    {
        if (typeSystem->addTypeForVar(obj, type))
        {
            typeSystem->addVarForType(obj, type);
            processTypeMismatchedGep(obj, type);
        }
    }
}

/// process mismatched gep edges
void AndersenWaveDiffWithType::processTypeMismatchedGep(NodeID obj, const Type *type)
{
    auto it = typeMismatchedObjToEdges.find(obj);
    if (it == typeMismatchedObjToEdges.end())
        return;
    Set<const GepCGEdge*> &edges = it->second;
    Set<const GepCGEdge*> processed;

    PTAType ptaTy(type);
    NodeBS &nodesOfType = typeSystem->getVarsForType(ptaTy);

    for (const auto *edge : edges)
    {
        if (const auto *normalGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge))
        {
            if (!nodesOfType.test(normalGepEdge->getSrcID()))
                continue;
            PointsTo tmpPts;
            tmpPts.set(obj);
            Andersen::processGepPts(tmpPts, normalGepEdge);
            processed.insert(normalGepEdge);
        }
    }

    for (const auto *nit : processed)
        edges.erase(nit);
}

/// match types for Gep Edges
bool AndersenWaveDiffWithType::matchType(NodeID ptrid, NodeID objid, const NormalGepCGEdge *normalGepEdge)
{
    if (!typeSystem->hasTypeSet(ptrid) || !typeSystem->hasTypeSet(objid))
        return true;
    const TypeSet *ptrTypeSet = typeSystem->getTypeSet(ptrid);
    const TypeSet *objTypeSet = typeSystem->getTypeSet(objid);
    if (ptrTypeSet->intersect(objTypeSet))
    {
        return true;
    }

    recordTypeMismatchedGep(objid, normalGepEdge);
    return false;
}

/// add type for newly created GepObjNode
void AndersenWaveDiffWithType::addTypeForGepObjNode(NodeID id, const NormalGepCGEdge* normalGepEdge)
{
    NodeID srcId = normalGepEdge->getSrcID();
    NodeID dstId = normalGepEdge->getDstID();
    if (pag->hasNonlabeledEdge(pag->getPAGNode(srcId), pag->getPAGNode(dstId), PAGEdge::NormalGep))
    {
        const Value *val = pag->getIntraPAGEdge(srcId, dstId, PAGEdge::NormalGep)->getValue();
        if (val)
        {
            PTAType ptaTy(val->getType());
            if(typeSystem->addTypeForVar(id, ptaTy))
                typeSystem->addVarForType(id, ptaTy);
        }
    }
}

NodeStack& AndersenWaveDiffWithType::SCCDetect()
{
    Andersen::SCCDetect();

    /// merge types of nodes in SCC
    const NodeBS &repNodes = getSCCDetector()->getRepNodes();
    for (const auto& repNode : repNodes)
    {
        NodeBS subNodes = getSCCDetector()->subNodes(repNode);
        mergeTypeOfNodes(subNodes);
    }

    return getSCCDetector()->topoNodeStack();
}

/// merge types of nodes in a cycle
void AndersenWaveDiffWithType::mergeTypeOfNodes(const NodeBS &nodes)
{

    /// collect types in a cycle
    OrderedSet<PTAType> typesInSCC;
    for (const auto& node : nodes)
    {
        if (typeSystem->hasTypeSet(node))
        {
            const TypeSet *typeSet = typeSystem->getTypeSet(node);
            for (auto ptaTy : *typeSet)
            {
                 typesInSCC.insert(ptaTy);
            }
        }
    }

    /// merge types of nodes in a cycle
    for (const auto& node : nodes)
    {
        for (auto ptaTy : typesInSCC)
        {
             if (typeSystem->addTypeForVar(node, ptaTy))
                typeSystem->addVarForType(node, ptaTy);
        }
    }

}
