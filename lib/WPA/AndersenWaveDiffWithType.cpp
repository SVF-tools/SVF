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
            if (const CastInst *castInst = SVFUtil::dyn_cast<CastInst>(val))
            {
                updateObjType(castInst->getType(), getPts(edge->getSrcID()));
            }
            else if (const ConstantExpr *ce = SVFUtil::dyn_cast<ConstantExpr>(val))
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
    for (PointsTo::iterator it = objs.begin(), eit = objs.end(); it != eit; ++it)
    {
        if (typeSystem->addTypeForVar(*it, type))
        {
            typeSystem->addVarForType(*it, type);
            processTypeMismatchedGep(*it, type);
        }
    }
}

/// process mismatched gep edges
void AndersenWaveDiffWithType::processTypeMismatchedGep(NodeID obj, const Type *type)
{
    TypeMismatchedObjToEdgeTy::iterator it = typeMismatchedObjToEdges.find(obj);
    if (it == typeMismatchedObjToEdges.end())
        return;
    Set<const GepCGEdge*> &edges = it->second;
    Set<const GepCGEdge*> processed;

    PTAType ptaTy(type);
    NodeBS &nodesOfType = typeSystem->getVarsForType(ptaTy);

    for (Set<const GepCGEdge*>::iterator nit = edges.begin(), neit = edges.end(); nit != neit; ++nit)
    {
        if (const NormalGepCGEdge *normalGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(*nit))
        {
            if (!nodesOfType.test(normalGepEdge->getSrcID()))
                continue;
            PointsTo tmpPts;
            tmpPts.set(obj);
            Andersen::processGepPts(tmpPts, normalGepEdge);
            processed.insert(normalGepEdge);
        }
    }

    for (Set<const GepCGEdge*>::iterator nit = processed.begin(), neit = processed.end(); nit != neit; ++nit)
        edges.erase(*nit);
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
    else
    {
        recordTypeMismatchedGep(objid, normalGepEdge);
        return false;
    }
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
    for (NodeBS::iterator it = repNodes.begin(), eit = repNodes.end(); it != eit; ++it)
    {
        NodeBS subNodes = getSCCDetector()->subNodes(*it);
        mergeTypeOfNodes(subNodes);
    }

    return getSCCDetector()->topoNodeStack();
}

/// merge types of nodes in a cycle
void AndersenWaveDiffWithType::mergeTypeOfNodes(const NodeBS &nodes)
{

    /// collect types in a cycle
    OrderedSet<PTAType> typesInSCC;
    for (NodeBS::iterator it = nodes.begin(), eit = nodes.end(); it != eit; ++it)
    {
        if (typeSystem->hasTypeSet(*it))
        {
            const TypeSet *typeSet = typeSystem->getTypeSet(*it);
            for (TypeSet::const_iterator tyit = typeSet->begin(), tyeit = typeSet->end(); tyit != tyeit; ++tyit)
            {
                const PTAType &ptaTy = *tyit;
                typesInSCC.insert(ptaTy);
            }
        }
    }

    /// merge types of nodes in a cycle
    for (NodeBS::iterator it = nodes.begin(), eit = nodes.end(); it != eit; ++it)
    {
        for (OrderedSet<PTAType>::iterator tyit = typesInSCC.begin(), tyeit = typesInSCC.end(); tyit != tyeit; ++tyit)
        {
            const PTAType &ptaTy = *tyit;
            if (typeSystem->addTypeForVar(*it, ptaTy))
                typeSystem->addVarForType(*it, ptaTy);
        }
    }

}
