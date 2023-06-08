//===- SVFIR.h -- IR of SVF ---------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SVFIR.h
 *
 *  Created on: 31, 12, 2021
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_H_
#define INCLUDE_SVFIR_H_

#include "Graphs/IRGraph.h"

namespace SVF
{
class CommonCHGraph;
/*!
 * SVF Intermediate representation, representing variables and statements as a Program Assignment Graph (PAG)
 * Variables as nodes and statements as edges.
 * SymID and NodeID are equal here (same numbering).
 */
class SVFIR : public IRGraph
{
    friend class SVFIRBuilder;
    friend class ExternalPAG;
    friend class PAGBuilderFromFile;
    friend class TypeBasedHeapCloning;
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    typedef Set<const CallICFGNode*> CallSiteSet;
    typedef OrderedMap<const CallICFGNode*,NodeID> CallSiteToFunPtrMap;
    typedef Map<NodeID,CallSiteSet> FunPtrToCallSitesMap;
    typedef Map<NodeID,NodeBS> MemObjToFieldsMap;
    typedef std::vector<const SVFStmt*> SVFStmtList;
    typedef std::vector<const SVFVar*> SVFVarList;
    typedef Map<const SVFVar*,PhiStmt*> PHINodeMap;
    typedef Map<const SVFFunction*,SVFVarList> FunToArgsListMap;
    typedef Map<const CallICFGNode*,SVFVarList> CSToArgsListMap;
    typedef Map<const RetICFGNode*,const SVFVar*> CSToRetMap;
    typedef Map<const SVFFunction*,const SVFVar*> FunToRetMap;
    typedef Map<const SVFFunction*,SVFStmtSet> FunToPAGEdgeSetMap;
    typedef Map<const ICFGNode*,SVFStmtList> ICFGNode2SVFStmtsMap;
    typedef Map<NodeID, NodeID> NodeToNodeMap;
    typedef std::pair<NodeID, APOffset> NodeOffset;
    typedef std::pair<NodeID, AccessPath> NodeAccessPath;
    typedef Map<NodeOffset,NodeID> NodeOffsetMap;
    typedef Map<NodeAccessPath,NodeID> NodeAccessPathMap;
    typedef Map<const SVFValue*, NodeAccessPathMap> GepValueVarMap;
    typedef std::pair<const SVFType*, std::vector<AccessPath>> SVFTypeLocSetsPair;
    typedef Map<NodeID, SVFTypeLocSetsPair> TypeLocSetsMap;
    typedef Map<NodePair,NodeID> NodePairSetMap;

private:
    /// ValueNodes - This map indicates the Node that a particular SVFValue* is
    /// represented by.  This contains entries for all pointers.
    ICFGNode2SVFStmtsMap icfgNode2SVFStmtsMap;	///< Map an ICFGNode to its SVFStmts
    ICFGNode2SVFStmtsMap icfgNode2PTASVFStmtsMap;	///< Map an ICFGNode to its PointerAnalysis related SVFStmts
    GepValueVarMap GepValObjMap;	///< Map a pair<base,off> to a gep value node id
    TypeLocSetsMap typeLocSetsMap;	///< Map an arg to its base SVFType* and all its field location sets
    NodeOffsetMap GepObjVarMap;	///< Map a pair<base,off> to a gep obj node id
    MemObjToFieldsMap memToFieldsMap;	///< Map a mem object id to all its fields
    SVFStmtSet globSVFStmtSet;	///< Global PAGEdges without control flow information
    PHINodeMap phiNodeMap;	///< A set of phi copy edges
    FunToArgsListMap funArgsListMap;	///< Map a function to a list of all its formal parameters
    CSToArgsListMap callSiteArgsListMap;	///< Map a callsite to a list of all its actual parameters
    CSToRetMap callSiteRetMap;	///< Map a callsite to its callsite returns PAGNodes
    FunToRetMap funRetMap;	///< Map a function to its unique function return PAGNodes
    CallSiteToFunPtrMap indCallSiteToFunPtrMap; ///< Map an indirect callsite to its function pointer
    FunPtrToCallSitesMap funPtrToCallSitesMap;	///< Map a function pointer to the callsites where it is used
    /// Valid pointers for pointer analysis resolution connected by SVFIR edges (constraints)
    /// this set of candidate pointers can change during pointer resolution (e.g. adding new object nodes)
    OrderedNodeSet candidatePointers;
    SVFModule* svfModule; /// SVF Module
    ICFG* icfg; // ICFG
    CommonCHGraph* chgraph; // class hierarchy graph
    CallSiteSet callSiteSet; /// all the callsites of a program

    static std::unique_ptr<SVFIR> pag;	///< Singleton pattern here to enable instance of SVFIR can only be created once.

    /// Constructor
    SVFIR(bool buildFromFile);

    /// Clean up memory
    void destroy();

public:

    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline SVFIR* getPAG(bool buildFromFile = false)
    {
        if (pag == nullptr)
        {
            pag = std::unique_ptr<SVFIR>(new SVFIR(buildFromFile));
        }
        return pag.get();
    }
    static void releaseSVFIR()
    {
        pag = nullptr;
    }
    //@}
    /// Return memToFieldsMap
    inline MemObjToFieldsMap& getMemToFieldsMap()
    {
        return memToFieldsMap;
    }
    /// Return GepObjVarMap
    inline NodeOffsetMap& getGepObjNodeMap()
    {
        return GepObjVarMap;
    }
    /// Return valid pointers
    inline OrderedNodeSet& getAllValidPtrs()
    {
        return candidatePointers;
    }
    /// Initialize candidate pointers
    void initialiseCandidatePointers();

    /// Destructor
    virtual ~SVFIR()
    {
        destroy();
    }
    /// SVFIR build configurations
    //@{
    /// Whether to handle blackhole edge
    static void handleBlackHole(bool b);
    //@}
    /// Set/Get LLVM Module
    inline void setModule(SVFModule* mod)
    {
        svfModule = mod;
    }
    inline SVFModule* getModule()
    {
        assert(svfModule && "empty SVFModule! Build SVF IR first!");
        return svfModule;
    }
    /// Set/Get ICFG
    inline void setICFG(ICFG* i)
    {
        icfg = i;
    }
    inline ICFG* getICFG() const
    {
        assert(icfg->totalICFGNode>0 && "empty ICFG! Build SVF IR first!");
        return icfg;
    }
    /// Set/Get CHG
    inline void setCHG(CommonCHGraph* c)
    {
        chgraph = c;
    }
    inline CommonCHGraph* getCHG()
    {
        assert(chgraph && "empty ICFG! Build SVF IR first!");
        return chgraph;
    }
    /// Get/set methods to get SVFStmts based on their kinds and ICFGNodes
    //@{
    /// Get edges set according to its kind
    inline SVFStmt::SVFStmtSetTy& getSVFStmtSet(SVFStmt::PEDGEK kind)
    {
        return KindToSVFStmtSetMap[kind];
    }
    /// Get PTA edges set according to its kind
    inline SVFStmt::SVFStmtSetTy& getPTASVFStmtSet(SVFStmt::PEDGEK kind)
    {
        return KindToPTASVFStmtSetMap[kind];
    }
    /// Whether this instruction has SVFIR Edge
    inline bool hasSVFStmtList(const ICFGNode* inst) const
    {
        return icfgNode2SVFStmtsMap.find(inst) != icfgNode2SVFStmtsMap.end();
    }
    inline bool hasPTASVFStmtList(const ICFGNode* inst) const
    {
        return icfgNode2PTASVFStmtsMap.find(inst) !=
               icfgNode2PTASVFStmtsMap.end();
    }
    /// Given an instruction, get all its PAGEdges
    inline SVFStmtList& getSVFStmtList(const ICFGNode* inst)
    {
        return icfgNode2SVFStmtsMap[inst];
    }
    /// Given an instruction, get all its PTA PAGEdges
    inline SVFStmtList& getPTASVFStmtList(const ICFGNode* inst)
    {
        return icfgNode2PTASVFStmtsMap[inst];
    }
    /// Add a SVFStmt into instruction map
    inline void addToSVFStmtList(ICFGNode* inst, SVFStmt* edge)
    {
        edge->setICFGNode(inst);
        icfgNode2SVFStmtsMap[inst].push_back(edge);
        if (edge->isPTAEdge())
            icfgNode2PTASVFStmtsMap[inst].push_back(edge);
    }
    /// Add a base SVFType* and all its field location sets to an arg NodeId
    inline void addToTypeLocSetsMap(NodeID argId, SVFTypeLocSetsPair& locSets)
    {
        typeLocSetsMap[argId]=locSets;
    }
    /// Given an arg NodeId, get its base SVFType* and all its field location sets
    inline SVFTypeLocSetsPair& getTypeLocSetsMap(NodeID argId)
    {
        return typeLocSetsMap[argId];
    }
    /// Get global PAGEdges (not in a procedure)
    inline SVFStmtSet& getGlobalSVFStmtSet()
    {
        return globSVFStmtSet;
    }
    /// Get all callsites
    inline const CallSiteSet& getCallSiteSet() const
    {
        return callSiteSet;
    }
    /// Whether this SVFVar is a result operand a of phi node
    inline bool isPhiNode(const SVFVar* node) const
    {
        return phiNodeMap.find(node) != phiNodeMap.end();
    }

    /// Function has arguments list
    inline bool hasFunArgsList(const SVFFunction* func) const
    {
        return (funArgsListMap.find(func) != funArgsListMap.end());
    }
    /// Get function arguments list
    inline FunToArgsListMap& getFunArgsMap()
    {
        return funArgsListMap;
    }
    /// Get function arguments list
    inline const SVFVarList& getFunArgsList(const SVFFunction*  func) const
    {
        FunToArgsListMap::const_iterator it = funArgsListMap.find(func);
        assert(it != funArgsListMap.end() && "this function doesn't have arguments");
        return it->second;
    }
    /// Callsite has argument list
    inline bool hasCallSiteArgsMap(const CallICFGNode* cs) const
    {
        return (callSiteArgsListMap.find(cs) != callSiteArgsListMap.end());
    }
    /// Get callsite argument list
    inline CSToArgsListMap& getCallSiteArgsMap()
    {
        return callSiteArgsListMap;
    }
    /// Get callsite argument list
    inline const SVFVarList& getCallSiteArgsList(const CallICFGNode* cs) const
    {
        CSToArgsListMap::const_iterator it = callSiteArgsListMap.find(cs);
        assert(it != callSiteArgsListMap.end() && "this call site doesn't have arguments");
        return it->second;
    }
    /// Get callsite return
    inline CSToRetMap& getCallSiteRets()
    {
        return callSiteRetMap;
    }
    /// Get callsite return
    inline const SVFVar* getCallSiteRet(const RetICFGNode* cs) const
    {
        CSToRetMap::const_iterator it = callSiteRetMap.find(cs);
        assert(it != callSiteRetMap.end() && "this call site doesn't have return");
        return it->second;
    }
    inline bool callsiteHasRet(const RetICFGNode* cs) const
    {
        return callSiteRetMap.find(cs) != callSiteRetMap.end();
    }
    /// Get function return list
    inline FunToRetMap& getFunRets()
    {
        return funRetMap;
    }
    /// Get function return list
    inline const SVFVar* getFunRet(const SVFFunction*  func) const
    {
        FunToRetMap::const_iterator it = funRetMap.find(func);
        assert(it != funRetMap.end() && "this function doesn't have return");
        return it->second;
    }
    inline bool funHasRet(const SVFFunction* func) const
    {
        return funRetMap.find(func) != funRetMap.end();
    }
    //@}

    /// Node and edge statistics
    //@{
    inline u32_t getFieldValNodeNum() const
    {
        return GepValObjMap.size();
    }
    inline u32_t getFieldObjNodeNum() const
    {
        return GepObjVarMap.size();
    }
    //@}

    /// Due to constaint expression, curInst is used to distinguish different instructions (e.g., memorycpy) when creating GepValVar.
    NodeID getGepValVar(const SVFValue* curInst, NodeID base,
                        const AccessPath& ap) const;

    /// Add/get indirect callsites
    //@{
    inline const CallSiteToFunPtrMap& getIndirectCallsites() const
    {
        return indCallSiteToFunPtrMap;
    }
    inline NodeID getFunPtr(const CallICFGNode* cs) const
    {
        CallSiteToFunPtrMap::const_iterator it = indCallSiteToFunPtrMap.find(cs);
        assert(it!=indCallSiteToFunPtrMap.end() && "indirect callsite not have a function pointer?");
        return it->second;
    }
    inline const CallSiteSet& getIndCallSites(NodeID funPtr) const
    {
        FunPtrToCallSitesMap::const_iterator it = funPtrToCallSitesMap.find(funPtr);
        assert(it!=funPtrToCallSitesMap.end() && "function pointer not used at any indirect callsite?");
        return it->second;
    }
    inline bool isIndirectCallSites(const CallICFGNode* cs) const
    {
        return (indCallSiteToFunPtrMap.find(cs) != indCallSiteToFunPtrMap.end());
    }
    inline bool isFunPtr(NodeID id) const
    {
        return (funPtrToCallSitesMap.find(id) != funPtrToCallSitesMap.end());
    }
    //@}
    /// Get an edge according to src, dst and kind
    //@{
    inline SVFStmt* getIntraPAGEdge(NodeID src, NodeID dst, SVFStmt::PEDGEK kind)
    {
        return getIntraPAGEdge(getGNode(src), getGNode(dst), kind);
    }
    inline SVFStmt* getIntraPAGEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind)
    {
        SVFStmt edge(src, dst, kind, false);
        const SVFStmt::SVFStmtSetTy& edgeSet = getSVFStmtSet(kind);
        SVFStmt::SVFStmtSetTy::const_iterator it = edgeSet.find(&edge);
        assert(it != edgeSet.end() && "can not find pag edge");
        return (*it);
    }
    //@}

    /// Get memory object - Return memory object according to pag node id
    /// return whole allocated memory object if this node is a gep obj node
    /// return nullptr is this node is not a ObjVar type
    //@{
    inline const MemObj* getObject(NodeID id) const
    {
        const SVFVar* node = getGNode(id);
        if (const ObjVar* objPN = SVFUtil::dyn_cast<ObjVar>(node))
            return getObject(objPN);
        else
            return nullptr;
    }
    inline const MemObj*getObject(const ObjVar* node) const
    {
        return node->getMemObj();
    }
    //@}

    /// Get a field SVFIR Object node according to base mem obj and offset
    NodeID getGepObjVar(const MemObj* obj, const APOffset& ap);
    /// Get a field obj SVFIR node according to a mem obj and a given offset
    NodeID getGepObjVar(NodeID id, const APOffset& ap) ;
    /// Get a field-insensitive obj SVFIR node according to a mem obj
    //@{
    inline NodeID getFIObjVar(const MemObj* obj) const
    {
        return obj->getId();
    }
    inline NodeID getFIObjVar(NodeID id) const
    {
        return getBaseObjVar(id);
    }
    //@}

    /// Get black hole and constant id
    //@{
    inline bool isBlkPtr(NodeID id) const
    {
        return (SymbolTableInfo::isBlkPtr(id));
    }
    inline bool isNullPtr(NodeID id) const
    {
        return (SymbolTableInfo::isNullPtr(id));
    }
    inline bool isBlkObjOrConstantObj(NodeID id) const
    {
        return (isBlkObj(id) || isConstantObj(id));
    }
    inline bool isBlkObj(NodeID id) const
    {
        return SymbolTableInfo::isBlkObj(id);
    }
    inline bool isConstantObj(NodeID id) const
    {
        const MemObj* obj = getObject(id);
        assert(obj && "not an object node?");
        return SymbolTableInfo::isConstantObj(id) ||
               obj->isConstDataOrConstGlobal();
    }
    /// Whether an object can point to any other object or any of its fields is a pointer type.
    bool isNonPointerObj(NodeID id) const;
    //@}

    /// Base and Offset methods for Value and Object node
    //@{
    /// Get a base pointer node given a field pointer
    NodeID getBaseValVar(NodeID nodeId);
    inline NodeID getBaseObjVar(NodeID id) const
    {
        return getBaseObj(id)->getId();
    }
    inline const MemObj* getBaseObj(NodeID id) const
    {
        const SVFVar* node = pag->getGNode(id);
        assert(SVFUtil::isa<ObjVar>(node) && "need an object node");
        const ObjVar* obj = SVFUtil::cast<ObjVar>(node);
        return obj->getMemObj();
    }
    //@}

    /// Get all fields of an object
    //@{
    NodeBS& getAllFieldsObjVars(const MemObj* obj);
    NodeBS& getAllFieldsObjVars(NodeID id);
    NodeBS getFieldsAfterCollapse(NodeID id);
    //@}
    inline NodeID addDummyValNode()
    {
        return addDummyValNode(NodeIDAllocator::get()->allocateValueId());
    }
    inline NodeID addDummyObjNode(const SVFType* type)
    {
        return addDummyObjNode(NodeIDAllocator::get()->allocateObjectId(), type);
    }
    /// Whether a node is a valid pointer
    //@{
    bool isValidPointer(NodeID nodeId) const;

    bool isValidTopLevelPtr(const SVFVar* node);
    //@}

    /// Print SVFIR
    void print();

private:

    /// Map a SVFStatement type to a set of corresponding SVF statements
    inline void addToStmt2TypeMap(SVFStmt* edge)
    {
        bool added = KindToSVFStmtSetMap[edge->getEdgeKind()].insert(edge).second;
        (void)added; // Suppress warning of unused variable under release build
        assert(added && "duplicated edge, not added!!!");
        if (edge->isPTAEdge())
        {
            totalPTAPAGEdge++;
            KindToPTASVFStmtSetMap[edge->getEdgeKind()].insert(edge);
        }
    }
    /// Get/set method for function/callsite arguments and returns
    //@{
    /// Add function arguments
    inline void addFunArgs(const SVFFunction* fun, const SVFVar* arg)
    {
        FunEntryICFGNode* funEntryBlockNode = icfg->getFunEntryICFGNode(fun);
        funEntryBlockNode->addFormalParms(arg);
        funArgsListMap[fun].push_back(arg);
    }
    /// Add function returns
    inline void addFunRet(const SVFFunction* fun, const SVFVar* ret)
    {
        FunExitICFGNode* funExitBlockNode = icfg->getFunExitICFGNode(fun);
        funExitBlockNode->addFormalRet(ret);
        funRetMap[fun] = ret;
    }
    /// Add callsite arguments
    inline void addCallSiteArgs(CallICFGNode* callBlockNode,const SVFVar* arg)
    {
        callBlockNode->addActualParms(arg);
        callSiteArgsListMap[callBlockNode].push_back(arg);
    }
    /// Add callsite returns
    inline void addCallSiteRets(RetICFGNode* retBlockNode,const SVFVar* arg)
    {
        retBlockNode->addActualRet(arg);
        callSiteRetMap[retBlockNode]= arg;
    }
    /// Add indirect callsites
    inline void addIndirectCallsites(const CallICFGNode* cs,NodeID funPtr)
    {
        bool added = indCallSiteToFunPtrMap.emplace(cs, funPtr).second;
        (void) added;
        funPtrToCallSitesMap[funPtr].insert(cs);
        assert(added && "adding the same indirect callsite twice?");
    }

    /// add node into SVFIR
    //@{
    /// Add a value (pointer) node
    inline NodeID addValNode(const SVFValue* val, NodeID i)
    {
        SVFVar *node = new ValVar(val,i);
        return addValNode(val, node, i);
    }
    /// Add a memory obj node
    inline NodeID addObjNode(const SVFValue* val, NodeID i)
    {
        const MemObj* mem = getMemObj(val);
        assert(mem->getId() == i && "not same object id?");
        return addFIObjNode(mem);
    }
    /// Add a unique return node for a procedure
    inline NodeID addRetNode(const SVFFunction* val, NodeID i)
    {
        SVFVar *node = new RetPN(val,i);
        return addRetNode(val, node, i);
    }
    /// Add a unique vararg node for a procedure
    inline NodeID addVarargNode(const SVFFunction* val, NodeID i)
    {
        SVFVar *node = new VarArgPN(val,i);
        return addNode(node,i);
    }

    /// Add a temp field value node, this method can only invoked by getGepValVar
    NodeID addGepValNode(const SVFValue* curInst,const SVFValue* val, const AccessPath& ap, NodeID i, const SVFType* type);
    /// Add a field obj node, this method can only invoked by getGepObjVar
    NodeID addGepObjNode(const MemObj* obj, const APOffset& apOffset);
    /// Add a field-insensitive node, this method can only invoked by getFIGepObjNode
    NodeID addFIObjNode(const MemObj* obj);
    //@}

    ///  Add a dummy value/object node according to node ID (llvm value is null)
    //@{
    inline NodeID addDummyValNode(NodeID i)
    {
        return addValNode(nullptr, new DummyValVar(i), i);
    }
    inline NodeID addDummyObjNode(NodeID i, const SVFType* type)
    {
        const MemObj* mem = addDummyMemObj(i, type);
        return addObjNode(nullptr, new DummyObjVar(i,mem), i);
    }
    inline const MemObj* addDummyMemObj(NodeID i, const SVFType* type)
    {
        return getSymbolInfo()->createDummyObj(i,type);
    }
    inline NodeID addBlackholeObjNode()
    {
        return addObjNode(
                   nullptr, new DummyObjVar(getBlackHoleNode(), getBlackHoleObj()),
                   getBlackHoleNode());
    }
    inline NodeID addConstantObjNode()
    {
        return addObjNode(nullptr,
                          new DummyObjVar(getConstantNode(), getConstantObj()),
                          getConstantNode());
    }
    inline NodeID addBlackholePtrNode()
    {
        return addDummyValNode(getBlkPtr());
    }
    //@}

    /// Add a value (pointer) node
    inline NodeID addValNode(const SVFValue*, SVFVar *node, NodeID i)
    {
        assert(hasGNode(i) == false &&
               "This NodeID clashes here. Please check NodeIDAllocator. Switch "
               "Strategy::DBUG to SEQ or DENSE");
        return addNode(node, i);
    }
    /// Add a memory obj node
    inline NodeID addObjNode(const SVFValue*, SVFVar *node, NodeID i)
    {
        assert(hasGNode(i) == false &&
               "This NodeID clashes here. Please check NodeIDAllocator. Switch "
               "Strategy::DBUG to SEQ or DENSE");
        return addNode(node, i);
    }
    /// Add a unique return node for a procedure
    inline NodeID addRetNode(const SVFFunction*, SVFVar *node, NodeID i)
    {
        return addNode(node,i);
    }
    /// Add a unique vararg node for a procedure
    inline NodeID addVarargNode(const SVFFunction*, SVFVar *node, NodeID i)
    {
        return addNode(node,i);
    }

    /// Add global PAGEdges (not in a procedure)
    inline void addGlobalPAGEdge(const SVFStmt* edge)
    {
        globSVFStmtSet.insert(edge);
    }
    /// Add callsites
    inline void addCallSite(const CallICFGNode* call)
    {
        callSiteSet.insert(call);
    }
    /// Add an edge into SVFIR
    //@{
    /// Add Address edge
    AddrStmt* addAddrStmt(NodeID src, NodeID dst);
    /// Add Copy edge
    CopyStmt* addCopyStmt(NodeID src, NodeID dst);
    /// Add phi node information
    PhiStmt*  addPhiStmt(NodeID res, NodeID opnd, const ICFGNode* pred);
    /// Add SelectStmt
    SelectStmt*  addSelectStmt(NodeID res, NodeID op1, NodeID op2, NodeID cond);
    /// Add Copy edge
    CmpStmt* addCmpStmt(NodeID op1, NodeID op2, NodeID dst, u32_t predict);
    /// Add Copy edge
    BinaryOPStmt* addBinaryOPStmt(NodeID op1, NodeID op2, NodeID dst,
                                  u32_t opcode);
    /// Add Unary edge
    UnaryOPStmt* addUnaryOPStmt(NodeID src, NodeID dst, u32_t opcode);
    /// Add BranchStmt
    BranchStmt* addBranchStmt(NodeID br, NodeID cond,
                              const BranchStmt::SuccAndCondPairVec& succs);
    /// Add Load edge
    LoadStmt* addLoadStmt(NodeID src, NodeID dst);
    /// Add Store edge
    StoreStmt* addStoreStmt(NodeID src, NodeID dst, const IntraICFGNode* val);
    /// Add Call edge
    CallPE* addCallPE(NodeID src, NodeID dst, const CallICFGNode* cs,
                      const FunEntryICFGNode* entry);
    /// Add Return edge
    RetPE* addRetPE(NodeID src, NodeID dst, const CallICFGNode* cs,
                    const FunExitICFGNode* exit);
    /// Add Gep edge
    GepStmt* addGepStmt(NodeID src, NodeID dst, const AccessPath& ap,
                        bool constGep);
    /// Add Offset(Gep) edge
    GepStmt* addNormalGepStmt(NodeID src, NodeID dst, const AccessPath& ap);
    /// Add Variant(Gep) edge
    GepStmt* addVariantGepStmt(NodeID src, NodeID dst, const AccessPath& ap);
    /// Add Thread fork edge for parameter passing
    TDForkPE* addThreadForkPE(NodeID src, NodeID dst, const CallICFGNode* cs,
                              const FunEntryICFGNode* entry);
    /// Add Thread join edge for parameter passing
    TDJoinPE* addThreadJoinPE(NodeID src, NodeID dst, const CallICFGNode* cs,
                              const FunExitICFGNode* exit);
    //@}

    /// Set a pointer points-to black hole (e.g. int2ptr)
    SVFStmt* addBlackHoleAddrStmt(NodeID node);
};

typedef SVFIR PAG;

} // End namespace SVF



#endif /* INCLUDE_SVFIR_H_ */
