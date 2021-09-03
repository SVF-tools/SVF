//===- SVFGNode.h -- Sparse value-flow graph node-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFGNode.h
 *
 *  Created on: 13 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_MSSA_SVFGNODE_H_
#define INCLUDE_MSSA_SVFGNODE_H_

#include "Graphs/VFGNode.h"
#include "Graphs/SVFGEdge.h"

namespace SVF
{

/*!
 * Memory region VFGNode (for address-taken objects)
 */
class MRSVFGNode : public VFGNode
{
protected:
    PointsTo cpts;

    // This constructor can only be used by derived classes
    MRSVFGNode(NodeID id, VFGNodeK k) : VFGNode(id, k) {}

public:
    /// Return points-to of the MR
    inline const PointsTo& getPointsTo() const
    {
        return cpts;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MRSVFGNode *)
    {
        return true;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == FPIN ||
               node->getNodeKind() == FPOUT ||
               node->getNodeKind() == APIN ||
               node->getNodeKind() == APOUT ||
               node->getNodeKind() == MPhi ||
               node->getNodeKind() == MIntraPhi ||
               node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == FPIN ||
               node->getNodeKind() == FPOUT ||
               node->getNodeKind() == APIN ||
               node->getNodeKind() == APOUT ||
               node->getNodeKind() == MPhi ||
               node->getNodeKind() == MIntraPhi ||
               node->getNodeKind() == MInterPhi;
    }
    //@}

    virtual const std::string toString() const;
};

/*
 * SVFG Node stands for entry chi node (address-taken variables)
 */
class FormalINSVFGNode : public MRSVFGNode
{
private:
    const MemSSA::ENTRYCHI* chi;
    const SVFFunction* fun;

public:
    /// Constructor
    FormalINSVFGNode(NodeID id, const MRVer* resVer, const SVFFunction* func): MRSVFGNode(id, FPIN)
    {
        cpts = resVer->getMR()->getPointsTo();
        fun = func;
    }
    /// EntryCHI
    inline const MemSSA::ENTRYCHI* getEntryChi() const
    {
        return chi;
    }

    inline const SVFFunction* getFunction() const
    {
        return fun;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FormalINSVFGNode *)
    {
        return true;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == FPIN;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == FPIN;
    }
    //@}

    virtual const std::string toString() const;
};


/*
 * SVFG Node stands for return mu node (address-taken variables)
 */
class FormalOUTSVFGNode : public MRSVFGNode
{
private:
    const MemSSA::RETMU* mu;
    const SVFFunction* fun;
    const MRVer* ver;

public:
    /// Constructor
    FormalOUTSVFGNode(NodeID id, const MRVer* ver, const SVFFunction* func);

    /// RetMU
    inline const MemSSA::RETMU* getRetMU() const
    {
        return mu;
    }

    inline const SVFFunction* getFunction() const
    {
        return fun;
    }

    inline const MRVer* getVer() const
    {
        return ver;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FormalOUTSVFGNode *)
    {
        return true;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == FPOUT;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == FPOUT;
    }
    //@}

    virtual const std::string toString() const;
};


/*
 * SVFG Node stands for callsite mu node (address-taken variables)
 */
class ActualINSVFGNode : public MRSVFGNode
{
private:
    const MemSSA::CALLMU* mu;
    const CallBlockNode* cs;
    const MRVer* ver;
public:
    /// Constructor
    ActualINSVFGNode(NodeID id, const CallBlockNode* c, const MRVer* mrver):
        MRSVFGNode(id, APIN), cs(c)
    {
        cpts = mrver->getMR()->getPointsTo();
        ver = mrver;
    }
    /// Callsite
    inline const CallBlockNode* getCallSite() const
    {
        return cs;
    }
    /// CallMU
    inline const MemSSA::CALLMU* getCallMU() const
    {
        return mu;
    }
    /// Ver
    inline const MRVer* getVer() const
    {
        return ver;
    }


    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualINSVFGNode *)
    {
        return true;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == APIN;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == APIN;
    }
    //@}

    virtual const std::string toString() const;
};


/*
 * SVFG Node stands for callsite chi node (address-taken variables)
 */
class ActualOUTSVFGNode : public MRSVFGNode
{
private:
    const MemSSA::CALLCHI* chi;
    const CallBlockNode* cs;

public:
    /// Constructor
    ActualOUTSVFGNode(NodeID id, const CallBlockNode* cal, const MRVer* resVer):
        MRSVFGNode(id, APOUT), cs(cal)
    {
        cpts = resVer->getMR()->getPointsTo();
    }
    /// Callsite
    inline const CallBlockNode* getCallSite() const
    {
        return cs;
    }
    /// CallCHI
    inline const MemSSA::CALLCHI* getCallCHI() const
    {
        return chi;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualOUTSVFGNode *)
    {
        return true;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == APOUT;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == APOUT;
    }
    //@}

    virtual const std::string toString() const;
};


/*
 * SVFG Node stands for a memory ssa phi node or formalIn or ActualOut
 */
class MSSAPHISVFGNode : public MRSVFGNode
{
public:
    typedef Map<u32_t,const MRVer*> OPVers;

protected:
    const MemSSA::MDEF* res;
    OPVers opVers;

public:
    /// Constructor
    MSSAPHISVFGNode(NodeID id, const MemSSA::MDEF* def,VFGNodeK k = MPhi): MRSVFGNode(id, k), res(def)
    {
        cpts = def->getMR()->getPointsTo();
    }

    MSSAPHISVFGNode(NodeID id, const PointsTo pointsTo,VFGNodeK k = MPhi): MRSVFGNode(id, k)
    {
        cpts = pointsTo;
    }

    /// MSSA phi operands
    //@{
    inline const MRVer* getOpVer(u32_t pos) const
    {
        OPVers::const_iterator it = opVers.find(pos);
        assert(it!=opVers.end() && "version is nullptr, did not rename?");
        return it->second;
    }
    inline void setOpVer(u32_t pos, const MRVer* node)
    {
        opVers[pos] = node;
    }
    inline const MemSSA::MDEF* getRes() const
    {
        return res;
    }
    inline u32_t getOpVerNum() const
    {
        return opVers.size();
    }
    inline OPVers::const_iterator opVerBegin() const
    {
        return opVers.begin();
    }
    inline OPVers::const_iterator opVerEnd() const
    {
        return opVers.end();
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MSSAPHISVFGNode *)
    {
        return true;
    }
    static inline bool classof(const MRSVFGNode *node)
    {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    static inline bool classof(const VFGNode *node)
    {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    //@}

    virtual const std::string toString() const;
};

/*
 * Intra MSSA PHI
 */
class IntraMSSAPHISVFGNode : public MSSAPHISVFGNode
{

public:
    /// Constructor
    IntraMSSAPHISVFGNode(NodeID id, const PointsTo pointsTo): MSSAPHISVFGNode(id, pointsTo, MIntraPhi)
    {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraMSSAPHISVFGNode *)
    {
        return true;
    }
    static inline bool classof(const MSSAPHISVFGNode * node)
    {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const MRSVFGNode *node)
    {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == MIntraPhi;
    }
    //@}

    virtual const std::string toString() const;
};


/*
 * Inter MSSA PHI (formalIN/ActualOUT)
 */
class InterMSSAPHISVFGNode : public MSSAPHISVFGNode
{

public:
    /// Constructor interPHI for formal parameter
    InterMSSAPHISVFGNode(NodeID id, const FormalINSVFGNode* fi) : MSSAPHISVFGNode(id, fi->getEntryChi(), MInterPhi),fun(fi->getFun()),callInst(nullptr) {}
    /// Constructor interPHI for actual return
    InterMSSAPHISVFGNode(NodeID id, const ActualOUTSVFGNode* ao) : MSSAPHISVFGNode(id, ao->getCallCHI(), MInterPhi), fun(nullptr),callInst(ao->getCallSite()) {}

    inline bool isFormalINPHI() const
    {
        return (fun!=nullptr) && (callInst == nullptr);
    }

    inline bool isActualOUTPHI() const
    {
        return (fun==nullptr) && (callInst != nullptr);
    }

    inline const SVFFunction* getFun() const
    {
        assert(isFormalINPHI() && "expect a formal parameter phi");
        return fun;
    }

    inline const CallBlockNode* getCallSite() const
    {
        assert(isActualOUTPHI() && "expect a actual return phi");
        return callInst;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterMSSAPHISVFGNode *)
    {
        return true;
    }
    static inline bool classof(const MSSAPHISVFGNode * node)
    {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const MRSVFGNode *node)
    {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == MInterPhi;
    }
    //@}

    virtual const std::string toString() const;

private:
    const SVFFunction* fun;
    const CallBlockNode* callInst;
};

/*
 * Dummy node which encodes propagation of an object/version pair.
 */
class DummyVersionPropSVFGNode : public VFGNode
{
public:
    DummyVersionPropSVFGNode(NodeID id, NodeID object, Version version)
        : VFGNode(id, DummyVProp), object(object), version(version)
    { }

    NodeID getObject(void) const { return object; }
    Version getVersion(void) const { return version; }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const DummyVersionPropSVFGNode *)
    {
        return true;
    }

    static inline bool classof(const VFGNode *node)
    {
        return node->getNodeKind() == DummyVProp;
    }

    static inline bool classof(const GenericVFGNodeTy *node)
    {
        return node->getNodeKind() == DummyVProp;
    }
    //@}

private:
    const NodeID object;
    const Version version;
};


} // End namespace SVF

#endif /* INCLUDE_MSSA_SVFGNODE_H_ */
