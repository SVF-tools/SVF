//===- SVFGNode.h -- Sparse value-flow graph node-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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
    NodeBS cpts;

    // This constructor can only be used by derived classes
    MRSVFGNode(NodeID id, VFGNodeK k) : VFGNode(id, k) {}

public:
    /// Return points-to of the MR
    inline const NodeBS& getPointsTo() const
    {
        return cpts;
    }

    inline const NodeBS getDefSVFVars() const override;

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

    virtual const std::string toString() const override;
};

/*
 * SVFG Node stands for entry chi node (address-taken variables)
 */
class FormalINSVFGNode : public MRSVFGNode
{
private:
    const MRVer* ver;
    const FunEntryICFGNode* funEntryNode;

public:
    /// Constructor
    FormalINSVFGNode(NodeID id, const MRVer* resVer, const FunEntryICFGNode* funEntry): MRSVFGNode(id, FPIN)
    {
        cpts = resVer->getMR()->getPointsTo();
        ver = resVer;
        funEntryNode = funEntry;
    }
    inline const MRVer* getMRVer() const
    {
        return ver;
    }
    inline const FunEntryICFGNode* getFunEntryNode() const
    {
        return funEntryNode;
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
    const MRVer* ver;
    const FunExitICFGNode* funExitNode;

public:
    /// Constructor
    FormalOUTSVFGNode(NodeID id, const MRVer* ver, const FunExitICFGNode* funExitNode);

    inline const FunExitICFGNode* getFunExitNode() const
    {
        return funExitNode;
    }

    inline const MRVer* getMRVer() const
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
    const CallICFGNode* cs;
    const MRVer* ver;
public:
    /// Constructor
    ActualINSVFGNode(NodeID id, const CallICFGNode* c, const MRVer* mrver):
        MRSVFGNode(id, APIN), cs(c)
    {
        cpts = mrver->getMR()->getPointsTo();
        ver = mrver;
    }
    /// Callsite
    inline const CallICFGNode* getCallSite() const
    {
        return cs;
    }

    /// Ver
    inline const MRVer* getMRVer() const
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
    const CallICFGNode* cs;
    const MRVer* ver;

public:
    /// Constructor
    ActualOUTSVFGNode(NodeID id, const CallICFGNode* cal, const MRVer* resVer):
        MRSVFGNode(id, APOUT), cs(cal)
    {
        cpts = resVer->getMR()->getPointsTo();
        ver = resVer;
    }
    /// Callsite
    inline const CallICFGNode* getCallSite() const
    {
        return cs;
    }
    /// Ver
    inline const MRVer* getMRVer() const
    {
        return ver;
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
    const MRVer* ver;
    OPVers opVers;

public:
    /// Constructor
    MSSAPHISVFGNode(NodeID id, const MRVer* resVer,VFGNodeK k = MPhi): MRSVFGNode(id, k)
    {
        cpts = resVer->getMR()->getPointsTo();
        ver = resVer;
    }
    /// Ver
    inline const MRVer* getResVer() const
    {
        return ver;
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
    IntraMSSAPHISVFGNode(NodeID id, const MRVer* resVer): MSSAPHISVFGNode(id, resVer, MIntraPhi)
    {
    }

    inline const MRVer* getMRVer() const
    {
        return getResVer();
    }

    inline OPVers::const_iterator opVerBegin() const
    {
        return opVers.begin();
    }

    inline OPVers::const_iterator opVerEnd() const
    {
        return opVers.end();
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
    InterMSSAPHISVFGNode(NodeID id, const FormalINSVFGNode* fi) : MSSAPHISVFGNode(id, fi->getMRVer(), MInterPhi),fun(fi->getFun()),callInst(nullptr) {}
    /// Constructor interPHI for actual return
    InterMSSAPHISVFGNode(NodeID id, const ActualOUTSVFGNode* ao) : MSSAPHISVFGNode(id, ao->getMRVer(), MInterPhi), fun(nullptr),callInst(ao->getCallSite()) {}

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

    inline const CallICFGNode* getCallSite() const
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
    const CallICFGNode* callInst;
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

    NodeID getObject(void) const
    {
        return object;
    }
    Version getVersion(void) const
    {
        return version;
    }

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

    const NodeBS getDefSVFVars() const override;

private:
    const NodeID object;
    const Version version;
};


} // End namespace SVF

#endif /* INCLUDE_MSSA_SVFGNODE_H_ */
