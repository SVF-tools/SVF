/*
 * Steensgaard.h
 *
 *  Created on: 2 Feb. 2021
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_WPA_STEENSGAARD_H_
#define INCLUDE_WPA_STEENSGAARD_H_

#include "WPA/Andersen.h"

namespace SVF
{
/*!
 * Equivalence-based Pointer Analysis
 */
using WPAConstraintSolver = WPASolver<ConstraintGraph *>;

class Steensgaard:  public AndersenBase
{

public:

    using NodeToEquivClassMap = Map<NodeID, NodeID>;
    using NodeToSubsMap = Map<NodeID, Set<NodeID> >;

    /// Constructor
    Steensgaard(PAG* _pag)
        :  AndersenBase(_pag, Steensgaard_WPA, true)
    {
    }

    /// Create an singleton instance
    static Steensgaard* createSteensgaard(PAG* _pag)
    {
        if(steens==nullptr)
        {
            steens = new Steensgaard(_pag);
            steens->analyze();
            return steens;
        }
        return steens;
    }
    static void releaseSteensgaard()
    {
        if (steens)
            delete steens;
        steens = nullptr;
    }

    void solveWorklist() override;

    void processAllAddr();

    void ecUnion(NodeID id, NodeID ec);

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const Steensgaard *)
    {
        return true;
    }
    static inline bool classof(const AndersenBase *pta)
    {
        return (pta->getAnalysisTy() == Steensgaard_WPA);
    }
    static inline bool classof(const PointerAnalysis *pta)
    {
        return (pta->getAnalysisTy() == Steensgaard_WPA);
    }
    //@}

    /// Operation of points-to set
    inline const PointsTo& getPts(NodeID id) override
    {
        return getPTDataTy()->getPts(getEC(id));
    }
    /// pts(id) = pts(id) U target
    inline bool unionPts(NodeID id, const PointsTo& target) override
    {
        id = getEC(id);
        return getPTDataTy()->unionPts(id, target);
    }
    /// pts(id) = pts(id) U pts(ptd)
    inline bool unionPts(NodeID id, NodeID ptd) override
    {
        id = getEC(id);
        ptd = getEC(ptd);
        return getPTDataTy()->unionPts(id,ptd);
    }

    /// API for equivalence class operations
    /// Every constraint node maps to an unique equivalence class EC
    /// An equivalence class has a set of sub constraint nodes.
    inline NodeID getEC(NodeID id) const
    {
        auto it = nodeToECMap.find(id);
        if(it==nodeToECMap.end())
            return id;
        else
            return it->second;
    }
    void setEC(NodeID node, NodeID rep);

    inline Set<NodeID>& getSubNodes(NodeID id)
    {
        nodeToSubsMap[id].insert(id);
        return nodeToSubsMap[id];
    }
    inline void addSubNode(NodeID node, NodeID sub)
    {
        nodeToSubsMap[node].insert(sub);
    }

private:

    static Steensgaard* steens; // static instance
    NodeToEquivClassMap nodeToECMap;
    NodeToSubsMap nodeToSubsMap;
};

} // namespace SVF


#endif /* INCLUDE_WPA_STEENSGAARD_H_ */
