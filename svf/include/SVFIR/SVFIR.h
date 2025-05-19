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
 */
class SVFIR : public IRGraph
{
    friend class SVFIRBuilder;
    friend class ExternalPAG;
    friend class PAGBuilderFromFile;
    friend class TypeBasedHeapCloning;
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class BVDataPTAImpl;
    friend class GraphDBClient;

public:
    typedef Set<const CallICFGNode*> CallSiteSet;
    typedef OrderedMap<const CallICFGNode*,NodeID> CallSiteToFunPtrMap;
    typedef Map<NodeID,CallSiteSet> FunPtrToCallSitesMap;
    typedef Map<NodeID,NodeBS> MemObjToFieldsMap;
    typedef std::vector<const SVFStmt*> SVFStmtList;
    typedef std::vector<const SVFVar*> SVFVarList;
    typedef Map<const SVFVar*,PhiStmt*> PHINodeMap;
    typedef Map<const FunObjVar*,SVFVarList> FunToArgsListMap;
    typedef Map<const CallICFGNode*,SVFVarList> CSToArgsListMap;
    typedef Map<const RetICFGNode*,const SVFVar*> CSToRetMap;
    typedef Map<const FunObjVar*,const SVFVar*> FunToRetMap;
    typedef Map<const FunObjVar*,SVFStmtSet> FunToPAGEdgeSetMap;
    typedef Map<const ICFGNode*,SVFStmtList> ICFGNode2SVFStmtsMap;
    typedef Map<NodeID, NodeID> NodeToNodeMap;
    typedef std::pair<NodeID, APOffset> NodeOffset;
    typedef std::pair<NodeID, AccessPath> NodeAccessPath;
    typedef Map<NodeOffset,NodeID> NodeOffsetMap;
    typedef Map<NodeAccessPath,NodeID> NodeAccessPathMap;
    typedef Map<NodeID, NodeAccessPathMap> GepValueVarMap;
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
    ICFG* icfg; // ICFG
    CommonCHGraph* chgraph; // class hierarchy graph
    CallSiteSet callSiteSet; /// all the callsites of a program
    CallGraph* callGraph; /// Callgraph with direct calls only; no change allowed after init and use callgraph in PointerAnalysis for indirect calls)

    static std::unique_ptr<SVFIR> pag;	///< Singleton pattern here to enable instance of SVFIR can only be created once.
    static std::string pagReadFromTxt;

    std::string moduleIdentifier;

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

    /// Get CG
    inline const CallGraph* getCallGraph()
    {
        assert(callGraph && "empty CallGraph! Build SVF IR first!");
        return callGraph;
    }

    const FunObjVar* getFunObjVar(const std::string& name);

    inline const std::string& getModuleIdentifier() const
    {
        if (pagReadFromTxt.empty())
        {
            assert(!moduleIdentifier.empty() &&
                   "No module found! Reading from a file other than LLVM-IR?");
            return moduleIdentifier;
        }
        else
        {
            return pagReadFromTxt;
        }
    }

    static inline std::string pagFileName()
    {
        return pagReadFromTxt;
    }

    static inline bool pagReadFromTXT()
    {
        return !pagReadFromTxt.empty();
    }

    static inline void setPagFromTXT(const std::string& txt)
    {
        pagReadFromTxt = txt;
    }

    inline void setModuleIdentifier(const std::string& moduleIdentifier)
    {
        this->moduleIdentifier = moduleIdentifier;
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
    inline bool hasFunArgsList(const FunObjVar* func) const
    {
        return (funArgsListMap.find(func) != funArgsListMap.end());
    }
    /// Get function arguments list
    inline FunToArgsListMap& getFunArgsMap()
    {
        return funArgsListMap;
    }
    /// Get function arguments list
    inline const SVFVarList& getFunArgsList(const FunObjVar*  func) const
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
    inline const SVFVar* getFunRet(const FunObjVar*  func) const
    {
        FunToRetMap::const_iterator it = funRetMap.find(func);
        assert(it != funRetMap.end() && "this function doesn't have return");
        return it->second;
    }
    inline bool funHasRet(const FunObjVar* func) const
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

    /// Due to constraint expression, curInst is used to distinguish different instructions (e.g., memorycpy) when creating GepValVar.
    NodeID getGepValVar(NodeID curInst, NodeID base,
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
    inline const BaseObjVar* getBaseObject(NodeID id) const
    {
        const SVFVar* node = getGNode(id);
        if(const GepObjVar* gepObjVar = SVFUtil::dyn_cast<GepObjVar>(node))
            return SVFUtil::dyn_cast<BaseObjVar>(
                       getGNode(gepObjVar->getBaseNode()));
        else
            return SVFUtil::dyn_cast<BaseObjVar>(node);
    }

    inline const ValVar* getBaseValVar(NodeID id) const
    {
        const SVFVar* node = getGNode(id);
        if(const GepValVar* gepVar = SVFUtil::dyn_cast<GepValVar>(node))
            return gepVar->getBaseNode();
        else
            return SVFUtil::dyn_cast<ValVar>(node);
    }
    //@}

    /// Get a field SVFIR Object node according to base mem obj and offset
    NodeID getGepObjVar(const BaseObjVar* baseObj, const APOffset& ap);
    /// Get a field obj SVFIR node according to a mem obj and a given offset
    NodeID getGepObjVar(NodeID id, const APOffset& ap) ;
    /// Get a field-insensitive obj SVFIR node according to a mem obj
    //@{
    inline NodeID getFIObjVar(const BaseObjVar* obj) const
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

    inline bool isBlkObjOrConstantObj(NodeID id) const
    {
        return (isBlkObj(id) || isConstantObj(id));
    }

    inline bool isConstantObj(NodeID id) const
    {
        const BaseObjVar* obj = getBaseObject(id);
        assert(obj && "not an object node?");
        return isConstantSym(id) ||
               obj->isConstDataOrConstGlobal();
    }
    //@}

    /// Base and Offset methods for Value and Object node
    //@{
    /// Get a base pointer node given a field pointer
    inline NodeID getBaseObjVar(NodeID id) const
    {
        return getBaseObject(id)->getId();
    }
    //@}

    /// Get all fields of an object
    //@{
    NodeBS& getAllFieldsObjVars(const BaseObjVar* obj);
    NodeBS& getAllFieldsObjVars(NodeID id);
    NodeBS getFieldsAfterCollapse(NodeID id);
    //@}
    inline NodeID addDummyValNode()
    {
        return addDummyValNode(NodeIDAllocator::get()->allocateValueId(), nullptr);
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

protected:
    /// Add a value (pointer) node
    inline NodeID addValNodeFromDB(ValVar* node)
    {
        assert(node && "node cannot be nullptr.");
        if (hasGNode(node->getId()))
        {
            ValVar* valvar = SVFUtil::cast<ValVar>(getGNode(node->getId()));
            valvar->updateSVFValVarFromDB(node->getType(), node->getICFGNode());
            return valvar->getId();
        }
        return addNode(node);
    }
    /// Add a memory obj node
    inline NodeID addObjNodeFromDB(ObjVar* node)
    {
        assert(node && "node cannot be nullptr.");
        if (hasGNode(node->getId()))
        {
            ObjVar* objVar = SVFUtil::cast<ObjVar>(getGNode(node->getId()));
            objVar->updateObjVarFromDB(node->getType());
            return objVar->getId();
        }
        return addNode(node);
    }

    inline NodeID addInitValNodeFromDB(ValVar* node)
    {
        return addValNode(node);
    }

    inline NodeID addBaseObjNodeFromDB(BaseObjVar* node)
    {
        memToFieldsMap[node->getId()].set(node->getId());
        return addObjNode(node);
    }

    inline NodeID addDummyObjNodeFromDB(DummyObjVar* node)
    {
        if (idToObjTypeInfoMap().find(node->getId()) == idToObjTypeInfoMap().end())
        {
            ObjTypeInfo* ti = node->getTypeInfo();
            idToObjTypeInfoMap()[node->getId()] = ti;
            return addObjNode(node);
        }
        else
        {
            return addObjNode(node);
        }
    }

    void addGepObjNodeFromDB(GepObjVar* gepObj);

private:

    /// Map a SVFStatement type to a set of corresponding SVF statements
    inline void addToStmt2TypeMap(SVFStmt* edge)
    {
        bool added = KindToSVFStmtSetMap[edge->getEdgeKind()].insert(edge).second;
        (void)added; // Suppress warning of unused variable under release build
        assert(added && "duplicated edge, not added!!!");
        /// this is a pointer-related SVFStmt if (1) both RHS and LHS are pointers or (2) this an int2ptr statment, i.e., LHS = int2ptr RHS
        if (edge->isPTAEdge() || (SVFUtil::isa<CopyStmt>(edge) && SVFUtil::cast<CopyStmt>(edge)->isInt2Ptr()))
        {
            totalPTAPAGEdge++;
            KindToPTASVFStmtSetMap[edge->getEdgeKind()].insert(edge);
        }
    }
    /// Get/set method for function/callsite arguments and returns
    //@{
    /// Add function arguments
    inline void addFunArgs(const FunObjVar* fun, const SVFVar* arg)
    {
        FunEntryICFGNode* funEntryBlockNode = icfg->getFunEntryICFGNode(fun);
        funEntryBlockNode->addFormalParms(arg);
        funArgsListMap[fun].push_back(arg);
    }

    inline void addFunArgsFromDB(FunEntryICFGNode* funEntryBlockNode, FunObjVar* fun, const SVFVar* arg)
    {
        funEntryBlockNode->addFormalParms(arg);
        funArgsListMap[fun].push_back(arg);
    }
    /// Add function returns
    inline void addFunRet(const FunObjVar* fun, const SVFVar* ret)
    {
        FunExitICFGNode* funExitBlockNode = icfg->getFunExitICFGNode(fun);
        funExitBlockNode->addFormalRet(ret);
        funRetMap[fun] = ret;
    }

    inline void addFunRetFromDB(FunExitICFGNode* funExitBlockNode, FunObjVar* fun, const SVFVar* ret)
    {
        funExitBlockNode->addFormalRet(ret);
        funRetMap[fun] = ret;
    }
    /// Add callsite arguments
    inline void addCallSiteArgs(CallICFGNode* callBlockNode,const ValVar* arg)
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
    inline NodeID addValNode(NodeID i, const SVFType* type, const ICFGNode* icfgNode)
    {
        SVFVar *node = new ValVar(i, type, icfgNode, ValVar::ValNode);
        return addValNode(node);
    }

    NodeID addFunValNode(NodeID i, const ICFGNode* icfgNode, const FunObjVar* funObjVar, const SVFType* type)
    {
        FunValVar* node = new FunValVar(i, icfgNode, funObjVar, type);
        return addValNode(node);
    }

    NodeID addArgValNode(NodeID i, u32_t argNo, const ICFGNode* icfgNode, const FunObjVar* callGraphNode, const SVFType* type)
    {
        ArgValVar* node =
            new ArgValVar(i, argNo, icfgNode, callGraphNode, type);
        return addValNode(node);
    }

    inline NodeID addConstantFPValNode(const NodeID i, double dval,
                                       const ICFGNode* icfgNode, const SVFType* type)
    {
        SVFVar* node = new ConstFPValVar(i, dval, icfgNode, type);
        return addNode(node);
    }

    inline NodeID addConstantIntValNode(NodeID i, const std::pair<s64_t, u64_t>& intValue,
                                        const ICFGNode* icfgNode, const SVFType* type)
    {
        SVFVar* node = new ConstIntValVar(i, intValue.first, intValue.second, icfgNode, type);
        return addNode(node);
    }

    inline NodeID addConstantNullPtrValNode(const NodeID i, const ICFGNode* icfgNode, const SVFType* type)
    {
        SVFVar* node = new ConstNullPtrValVar(i, icfgNode, type);
        return addNode(node);
    }

    inline NodeID addGlobalValNode(const NodeID i, const ICFGNode* icfgNode, const SVFType* svfType)
    {
        SVFVar* node = new GlobalValVar(i, icfgNode, svfType);
        return addNode(node);
    }

    inline NodeID addConstantAggValNode(const NodeID i, const ICFGNode* icfgNode, const SVFType* svfType)
    {
        SVFVar* node = new ConstAggValVar(i, icfgNode, svfType);
        return addNode(node);
    }

    inline NodeID addConstantDataValNode(const NodeID i, const ICFGNode* icfgNode, const SVFType* type)
    {
        SVFVar* node = new ConstDataValVar(i, icfgNode, type);
        return addNode(node);
    }


    /// Add a memory obj node
    inline NodeID addObjNode(NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        return addFIObjNode( i, ti, type, node);
    }

    /**
     * Creates and adds a heap object node to the SVFIR
     */
    inline NodeID addHeapObjNode(NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        HeapObjVar *heapObj = new HeapObjVar(i, ti, type, node);
        return addObjNode(heapObj);
    }

    /**
     * Creates and adds a stack object node to the SVFIR
     */
    inline NodeID addStackObjNode(NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        StackObjVar *stackObj = new StackObjVar(i, ti, type, node);
        return addObjNode(stackObj);
    }

    NodeID addFunObjNode(NodeID id,  ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[id].set(id);
        FunObjVar* funObj = new FunObjVar(id, ti, type, node);
        return addObjNode(funObj);
    }


    inline NodeID addConstantFPObjNode(NodeID i, ObjTypeInfo* ti, double dval, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        ConstFPObjVar* conObj = new ConstFPObjVar(i, dval, ti, type, node);
        return addObjNode(conObj);
    }


    inline NodeID addConstantIntObjNode(NodeID i, ObjTypeInfo* ti, const std::pair<s64_t, u64_t>& intValue, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        ConstIntObjVar* conObj =
            new ConstIntObjVar(i, intValue.first, intValue.second, ti, type, node);
        return addObjNode(conObj);
    }


    inline NodeID addConstantNullPtrObjNode(const NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        ConstNullPtrObjVar* conObj = new ConstNullPtrObjVar(i, ti, type, node);
        return addObjNode(conObj);
    }

    inline NodeID addGlobalObjNode(const NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        GlobalObjVar* gObj = new GlobalObjVar(i, ti, type, node);
        return addObjNode(gObj);
    }
    inline NodeID addConstantAggObjNode(const NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        ConstAggObjVar* conObj = new ConstAggObjVar(i, ti, type, node);
        return addObjNode(conObj);
    }
    inline NodeID addConstantDataObjNode(const NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        ConstDataObjVar* conObj = new ConstDataObjVar(i, ti, type, node);
        return addObjNode(conObj);
    }

    /// Add a unique return node for a procedure
    inline NodeID addRetNode(NodeID i, const FunObjVar* callGraphNode, const SVFType* type, const ICFGNode* icn)
    {
        SVFVar *node = new RetValPN(i, callGraphNode, type, icn);
        return addRetNode(callGraphNode, node);
    }
    /// Add a unique vararg node for a procedure
    inline NodeID addVarargNode(NodeID i, const FunObjVar* val, const SVFType* type, const ICFGNode* n)
    {
        SVFVar *node = new VarArgValPN(i, val, type, n);
        return addNode(node);
    }

    /// Add a temp field value node, this method can only invoked by getGepValVar
    NodeID addGepValNode(NodeID curInst, const ValVar* base, const AccessPath& ap, NodeID i, const SVFType* type, const ICFGNode* node);
    /// Add a field obj node, this method can only invoked by getGepObjVar
    NodeID addGepObjNode(const BaseObjVar* baseObj, const APOffset& apOffset, const NodeID gepId);
    /// Add a field-insensitive node, this method can only invoked by getFIGepObjNode
    NodeID addFIObjNode(NodeID i, ObjTypeInfo* ti, const SVFType* type, const ICFGNode* node)
    {
        memToFieldsMap[i].set(i);
        BaseObjVar* baseObj = new BaseObjVar(i, ti, type, node);
        return addObjNode(baseObj);
    }


    //@}

    ///  Add a dummy value/object node according to node ID (llvm value is null)
    //@{
    inline NodeID addDummyValNode(NodeID i, const ICFGNode* node)
    {
        return addValNode(new DummyValVar(i, node));
    }
    inline NodeID addDummyObjNode(NodeID i, const SVFType* type)
    {
        if (idToObjTypeInfoMap().find(i) == idToObjTypeInfoMap().end())
        {
            ObjTypeInfo* ti = createObjTypeInfo(type);
            idToObjTypeInfoMap()[i] = ti;
            return addObjNode(new DummyObjVar(i, ti, nullptr, type));
        }
        else
        {
            return addObjNode(new DummyObjVar(i, getObjTypeInfo(i), nullptr, type));
        }
    }

    inline NodeID addBlackholeObjNode()
    {
        return addObjNode(new DummyObjVar(getBlackHoleNode(), getObjTypeInfo(getBlackHoleNode()), nullptr));
    }
    inline NodeID addConstantObjNode()
    {
        return addObjNode(new DummyObjVar(getConstantNode(), getObjTypeInfo(getConstantNode()), nullptr));
    }
    inline NodeID addBlackholePtrNode()
    {
        return addDummyValNode(getBlkPtr(), nullptr);
    }
    //@}

    /// Add a value (pointer) node
    inline NodeID addValNode(SVFVar *node)
    {
        assert(node && "node cannot be nullptr.");
        assert(hasGNode(node->getId()) == false &&
               "This NodeID clashes here. Please check NodeIDAllocator. Switch "
               "Strategy::DBUG to SEQ or DENSE");
        return addNode(node);
    }
    /// Add a memory obj node
    inline NodeID addObjNode(SVFVar *node)
    {
        assert(node && "node cannot be nullptr.");
        assert(hasGNode(node->getId()) == false &&
               "This NodeID clashes here. Please check NodeIDAllocator. Switch "
               "Strategy::DBUG to SEQ or DENSE");
        return addNode(node);
    }
    /// Add a unique return node for a procedure
    inline NodeID addRetNode(const FunObjVar*, SVFVar *node)
    {
        return addNode(node);
    }
    /// Add a unique vararg node for a procedure
    inline NodeID addVarargNode(const FunObjVar*, SVFVar *node)
    {
        return addNode(node);
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
    void addAddrStmtFromDB(AddrStmt* edge);
    /// Add Copy edge
    CopyStmt* addCopyStmt(NodeID src, NodeID dst, CopyStmt::CopyKind type);
    void addCopyStmtFromDB(CopyStmt* edge);

    /// Add phi node information
    PhiStmt*  addPhiStmt(NodeID res, NodeID opnd, const ICFGNode* pred);
    void addPhiStmtFromDB(PhiStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add SelectStmt
    SelectStmt*  addSelectStmt(NodeID res, NodeID op1, NodeID op2, NodeID cond);
    void addSelectStmtFromDB(SelectStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add Copy edge
    CmpStmt* addCmpStmt(NodeID op1, NodeID op2, NodeID dst, u32_t predict);
    void addCmpStmtFromDB(CmpStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add Copy edge
    BinaryOPStmt* addBinaryOPStmt(NodeID op1, NodeID op2, NodeID dst,
                                  u32_t opcode);
    void addBinaryOPStmtFromDB(BinaryOPStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add Unary edge
    UnaryOPStmt* addUnaryOPStmt(NodeID src, NodeID dst, u32_t opcode);
    void addUnaryOPStmtFromDB(UnaryOPStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add BranchStmt
    BranchStmt* addBranchStmt(NodeID br, NodeID cond,
                              const BranchStmt::SuccAndCondPairVec& succs);
    void addBranchStmtFromDB(BranchStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add Load edge
    LoadStmt* addLoadStmt(NodeID src, NodeID dst);
    void addLoadStmtFromDB(LoadStmt* edge);
    /// Add Store edge
    StoreStmt* addStoreStmt(NodeID src, NodeID dst, const ICFGNode* val);
    void addStoreStmtFromDB(StoreStmt* edge, SVFVar* src, SVFVar* dst);
    /// Add Call edge
    CallPE* addCallPE(NodeID src, NodeID dst, const CallICFGNode* cs,
                      const FunEntryICFGNode* entry);
    void addCallPEFromDB(CallPE* edge, SVFVar* src, SVFVar* dst);
    /// Add Return edge
    RetPE* addRetPE(NodeID src, NodeID dst, const CallICFGNode* cs,
                    const FunExitICFGNode* exit);
    void addRetPEFromDB(RetPE* edge, SVFVar* src, SVFVar* dst);
    /// Add Gep edge
    GepStmt* addGepStmt(NodeID src, NodeID dst, const AccessPath& ap,
                        bool constGep);
    void addGepStmtFromDB(GepStmt* edge);
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
