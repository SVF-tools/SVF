//===- PAG.h -- Program assignment graph--------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * PAG.h
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */


#ifndef PAG_H_
#define PAG_H_

#include "PAGEdge.h"
#include "PAGNode.h"
#include "Util/AnalysisUtil.h"

/*!
 * Program Assignment Graph for pointer analysis
 * SymID and NodeID are equal here (same numbering).
 */
class PAG : public GenericGraph<PAGNode,PAGEdge> {

public:
    typedef std::set<llvm::CallSite> CallSiteSet;
    typedef std::map<llvm::CallSite,NodeID> CallSiteToFunPtrMap;
    typedef std::map<NodeID,CallSiteSet> FunPtrToCallSitesMap;
    typedef llvm::DenseMap<NodeID,NodeBS> MemObjToFieldsMap;
    typedef std::set<const PAGEdge*> PAGEdgeSet;
    typedef std::list<const PAGEdge*> PAGEdgeList;
    typedef std::list<const PAGNode*> PAGNodeList;
    typedef std::list<std::pair<const PAGNode*, const llvm::BasicBlock*> > PNodeBBPairList;
    typedef std::map<const PAGNode*,PNodeBBPairList> PHINodeMap;
    typedef llvm::DenseMap<const llvm::Function*,PAGNodeList> FunToArgsListMap;
    typedef std::map<llvm::CallSite,PAGNodeList> CSToArgsListMap;
    typedef std::map<llvm::CallSite,const PAGNode*> CSToRetMap;
    typedef llvm::DenseMap<const llvm::Function*,const PAGNode*> FunToRetMap;
    typedef llvm::DenseMap<const llvm::Function*,PAGEdgeSet> FunToPAGEdgeSetMap;
    typedef llvm::DenseMap<const llvm::BasicBlock*,PAGEdgeList> BB2PAGEdgesMap;
    typedef llvm::DenseMap<const llvm::Instruction*,PAGEdgeList> Inst2PAGEdgesMap;
    typedef std::map<NodeID, NodeID> NodeToNodeMap;
    typedef std::pair<NodeID, Size_t> NodeOffset;
    typedef std::pair<NodeID, LocationSet> NodeLocationSet;
    typedef llvm::DenseMap<NodeOffset,NodeID,llvm::DenseMapInfo<std::pair<NodeID,Size_t> > > NodeOffsetMap;
    typedef std::map<NodeLocationSet,NodeID> NodeLocationSetMap;

private:
    SymbolTableInfo* symInfo;
    /// ValueNodes - This map indicates the Node that a particular Value* is
    /// represented by.  This contains entries for all pointers.
    PAGEdge::PAGKindToEdgeSetMapTy PAGEdgeKindToSetMap;  // < PAG edge map
    NodeLocationSetMap GepValNodeMap;	///< Map a pair<base,off> to a gep value node id
    NodeLocationSetMap GepObjNodeMap;	///< Map a pair<base,off> to a gep obj node id
    MemObjToFieldsMap memToFieldsMap;	///< Map a mem object id to all its fields
    Inst2PAGEdgesMap inst2PAGEdgesMap;	///< Map a instruction to its PAGEdges
    PAGEdgeSet globPAGEdgesSet;	///< Global PAGEdges without control flow information
    FunToPAGEdgeSetMap funToEntryPAGEdges; ///< Map a function to its function entry PAGEdges (e.g., blackhole--ptr edge, no caller functions)
    PHINodeMap phiNodeMap;	///< A set of phi copy edges in order to enable path sensitive analysis
    FunToArgsListMap funArgsListMap;	///< Map a function to a list of all its formal parameters
    CSToArgsListMap callSiteArgsListMap;	///< Map a callsite to a list of all its actual parameters
    CSToRetMap callSiteRetMap;	///< Map a callsite to its callsite returns PAGNodes
    FunToRetMap funRetMap;	///< Map a function to its unique function return PAGNodes
    static PAG* pag;	///< Singleton pattern here to enable instance of PAG can only be created once.
    CallSiteToFunPtrMap indCallSiteToFunPtrMap; ///< Map an indirect callsite to its function pointer
    FunPtrToCallSitesMap funPtrToCallSitesMap;	///< Map a function pointer to the callsites where it is used
    bool fromFile; ///< Whether the PAG is built according to user specified data from a txt file
    const llvm::BasicBlock* curBB;	///< Current basic block during PAG construction when visiting the module
    const llvm::Value* curVal;	///< Current Value during PAG construction when visiting the module

    /// Valid pointers for pointer analysis resolution connected by PAG edges (constraints)
    /// this set of candidate pointers can change during pointer resolution (e.g. adding new object nodes)
    NodeBS candidatePointers;

    /// Constructor
    PAG(bool buildFromFile) : fromFile(buildFromFile), curBB(NULL),curVal(NULL) {
        symInfo = SymbolTableInfo::Symbolnfo();
        storeInstNum = 0;
        loadInstNum = 0;
    }

    /// Clean up memory
    void destroy();

public:
    /// Return valid pointers
    inline NodeBS& getAllValidPtrs() {
        return candidatePointers;
    }
    /// Initialize candidate pointers
    inline void initialiseCandidatePointers() {
        // collect candidate pointers for demand-driven analysis
        for (iterator nIter = begin(); nIter != end(); ++nIter) {
            NodeID nodeId = nIter->first;
            // do not compute points-to for isolated node
            if (isValidPointer(nodeId) == false)
                continue;

            candidatePointers.test_and_set(nodeId);
        }
    }

    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline PAG* getPAG(bool buildFromFile = false) {
        if (pag == NULL) {
            pag = new PAG(buildFromFile);
        }
        return pag;
    }
    static void releasePAG() {
        if (pag)
            delete pag;
        pag = NULL;
    }
    //@}

    /// Destructor
    virtual ~PAG() {
        destroy();
    }

    /// Whether this PAG built from a txt file
    inline bool isBuiltFromFile() {
        return fromFile;
    }
    /// PAG build configurations
    //@{
    /// Whether to handle variant gep/field edge
    static void handleVGep(bool b);
    /// Whether to handle blackhole edge
    static void handleBlackHole(bool b);
    //@}
    /// Get LLVM Module
    static inline llvm::Module* getModule() {
        return SymbolTableInfo::Symbolnfo()->getModule();
    }
    /// Get/set methods to get control flow information of a PAGEdge
    //@{
    /// Set current basic block in order to keep track of control flow information
    inline void setCurrentLocation(const llvm::Value* val, const llvm::BasicBlock* bb) {
        curBB = bb;
        curVal = val;
    }
    inline const llvm::Value *getCurrentValue() const {
        return curVal;
    }
    inline const llvm::BasicBlock *getCurrentBB() const {
        return curBB;
    }
    /// Get Instruction to PAGEdge Map
    inline Inst2PAGEdgesMap& getInstToPAGEdgeMap() {
        return inst2PAGEdgesMap;
    }
    /// Whether this instruction has PAG Edge
    inline bool hasPAGEdgeList(const llvm::Instruction* inst) {
        return inst2PAGEdgesMap.find(inst)!=inst2PAGEdgesMap.end();
    }
    /// Given an instruction, get all its PAGEdges
    inline PAGEdgeList& getInstPAGEdgeList(const llvm::Instruction* inst) {
        return inst2PAGEdgesMap[inst];
    }
    /// Get function entry PAGEdges (not in a procedure)
    inline PAGEdgeSet& getGlobalPAGEdgeSet(const llvm::Function* fun) {
        return funToEntryPAGEdges[fun];
    }
    /// Get global PAGEdges (not in a procedure)
    inline PAGEdgeSet& getGlobalPAGEdgeSet() {
        return globPAGEdgesSet;
    }
    /// Add phi node information
    inline void addPhiNode(const PAGNode* res, const PAGNode* op,const llvm::BasicBlock* bb) {
        phiNodeMap[res].push_back(std::make_pair(op,bb));
    }
    /// Whether this PAGNode is a result operand a of phi node
    inline bool isPhiNode(const PAGNode* node) const {
        return phiNodeMap.find(node) != phiNodeMap.end();
    }
    /// Get all phi copy edges
    inline PHINodeMap& getPhiNodeMap() {
        return phiNodeMap;
    }
    //@}

    /// Get/set method for function/callsite arguments and returns
    //@{
    /// Add function arguments
    inline void addFunArgs(const llvm::Function* fun, const PAGNode* arg) {
        funArgsListMap[fun].push_back(arg);
    }
    /// Add function returns
    inline void addFunRet(const llvm::Function* fun, const PAGNode* ret) {
        funRetMap[fun] = ret;
    }
    /// Add callsite arguments
    inline void addCallSiteArgs(llvm::CallSite cs,const PAGNode* arg) {
        callSiteArgsListMap[cs].push_back(arg);
    }
    /// Add callsite returns
    inline void addCallSiteRets(llvm::CallSite cs,const PAGNode* arg) {
        callSiteRetMap[cs]= arg;
    }
    /// Function has arguments list
    inline bool hasFunArgsMap(const llvm::Function* func) const {
        return (funArgsListMap.find(func) != funArgsListMap.end());
    }
    /// Get function arguments list
    inline FunToArgsListMap& getFunArgsMap() {
        return funArgsListMap;
    }
    /// Get function arguments list
    inline const PAGNodeList& getFunArgsList(const llvm::Function * func) const {
        FunToArgsListMap::const_iterator it = funArgsListMap.find(func);
        assert(it != funArgsListMap.end() && "this function doesn't have arguments");
        return it->second;
    }
    /// Callsite has argument list
    inline bool hasCallSiteArgsMap(const llvm::CallSite cs) const {
        return (callSiteArgsListMap.find(cs) != callSiteArgsListMap.end());
    }
    /// Get callsite argument list
    inline CSToArgsListMap& getCallSiteArgsMap() {
        return callSiteArgsListMap;
    }
    /// Get callsite argument list
    inline const PAGNodeList& getCallSiteArgsList(const llvm::CallSite cs) const {
        CSToArgsListMap::const_iterator it = callSiteArgsListMap.find(cs);
        assert(it != callSiteArgsListMap.end() && "this call site doesn't have arguments");
        return it->second;
    }
    /// Get callsite return
    inline CSToRetMap& getCallSiteRets() {
        return callSiteRetMap;
    }
    /// Get callsite return
    inline const PAGNode* getCallSiteRet(const llvm::CallSite cs) const {
        CSToRetMap::const_iterator it = callSiteRetMap.find(cs);
        assert(it != callSiteRetMap.end() && "this call site doesn't have return");
        return it->second;
    }
    inline bool callsiteHasRet(const llvm::CallSite cs) const {
        return callSiteRetMap.find(cs) != callSiteRetMap.end();
    }
    /// Get function return list
    inline FunToRetMap& getFunRets() {
        return funRetMap;
    }
    /// Get function return list
    inline const PAGNode* getFunRet(const llvm::Function * func) const {
        FunToRetMap::const_iterator it = funRetMap.find(func);
        assert(it != funRetMap.end() && "this function doesn't have return");
        return it->second;
    }
    inline bool funHasRet(const llvm::Function* func) const {
        return funRetMap.find(func) != funRetMap.end();
    }
    //@}

    /// Node and edge statistics
    //@{
    inline Size_t getPAGNodeNum() const {
        return nodeNum;
    }
    inline Size_t getPAGEdgeNum() const {
        return edgeNum;
    }
    inline Size_t getValueNodeNum() const {
        return symInfo->valSyms().size();
    }
    inline Size_t getObjectNodeNum() const {
        return symInfo->idToObjMap().size();
    }
    inline Size_t getFieldValNodeNum() const {
        return GepValNodeMap.size();
    }
    inline Size_t getFieldObjNodeNum() const {
        return GepObjNodeMap.size();
    }
    //@}

    /// Get all callsites
    inline const CallSiteSet& getCallSiteSet() const {
        return symInfo->getCallSiteSet();
    }

    /// Add/get indirect callsites
    //@{
    inline const CallSiteToFunPtrMap& getIndirectCallsites() const {
        return indCallSiteToFunPtrMap;
    }
    inline void addIndirectCallsites(const llvm::CallSite cs,NodeID funPtr) {
        bool added = indCallSiteToFunPtrMap.insert(std::make_pair(cs,funPtr)).second;
        funPtrToCallSitesMap[funPtr].insert(cs);
        assert(added && "fail to add the indirect callsite?");
    }
    inline NodeID getFunPtr(const llvm::CallSite cs) const {
        CallSiteToFunPtrMap::const_iterator it = indCallSiteToFunPtrMap.find(cs);
        assert(it!=indCallSiteToFunPtrMap.end() && "indirect callsite not have a function pointer?");
        return it->second;
    }
    inline const CallSiteSet& getIndCallSites(NodeID funPtr) const {
        FunPtrToCallSitesMap::const_iterator it = funPtrToCallSitesMap.find(funPtr);
        assert(it!=funPtrToCallSitesMap.end() && "function pointer not used at any indirect callsite?");
        return it->second;
    }
    inline bool isIndirectCallSites(const llvm::CallSite cs) const {
        return (indCallSiteToFunPtrMap.find(cs) != indCallSiteToFunPtrMap.end());
    }
    inline bool isFunPtr(NodeID id) const {
        return (funPtrToCallSitesMap.find(id) != funPtrToCallSitesMap.end());
    }
    //@}

    /// Get a pag node according to its ID
    inline bool findPAGNode(NodeID id) const {
        return hasGNode(id);
    }

    /// Get edges set according to its kind
    inline PAGEdge::PAGEdgeSetTy& getEdgeSet(PAGEdge::PEDGEK kind) {
        return PAGEdgeKindToSetMap[kind];
    }

    /// Get an edge according to src, dst and kind
    //@{
    inline PAGEdge* getIntraPAGEdge(NodeID src, NodeID dst, PAGEdge::PEDGEK kind) {
        return getIntraPAGEdge(getPAGNode(src), getPAGNode(dst), kind);
    }
    inline PAGEdge* getIntraPAGEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind) {
        PAGEdge edge(src,dst,kind);
        const PAGEdge::PAGEdgeSetTy& edgeSet = getEdgeSet(kind);
        PAGEdge::PAGEdgeSetTy::const_iterator it = edgeSet.find(&edge);
        assert(it != edgeSet.end() && "can not find pag edge");
        return (*it);
    }
    //@}

    /// Get PAGNode ID
    inline PAGNode* getPAGNode(NodeID id) const {
        return getGNode(id);
    }

    /// Get PAG Node according to LLVM value
    //@{
    ///getNode - Return the node corresponding to the specified pointer.
    inline NodeID getValueNode(const llvm::Value *V) {
        return symInfo->getValSym(V);
    }
    inline bool hasValueNode(const llvm::Value* V) {
        return symInfo->hasValSym(V);
    }
    /// getObject - Return the obj node id refer to the memory object for the
    /// specified global, heap or alloca instruction according to llvm value.
    inline NodeID getObjectNode(const llvm::Value *V) {
        return symInfo->getObjSym(V);
    }
    /// getObject - return mem object id
    inline NodeID getObjectNode(const MemObj *mem) {
        return mem->getSymId();
    }
    /// Get memory object - Return memory object according to pag node id
    /// return whole allocated memory object if this node is a gep obj node
    /// return NULL is this node is not a ObjPN type
    //@{
    inline const MemObj*getObject(NodeID id) const {
        const PAGNode* node = getPAGNode(id);
        if(const ObjPN* objPN = llvm::dyn_cast<ObjPN>(node))
            return getObject(objPN);
        else
            return NULL;
    }
    inline const MemObj*getObject(const ObjPN* node) const {
        return node->getMemObj();
    }
    //@}

    /// GetReturnNode - Return the unique node representing the return value of a function
    inline NodeID getReturnNode(const llvm::Function *func) const {
        return symInfo->getRetSym(func);
    }
    /// getVarargNode - Return the unique node representing the variadic argument of a variadic function.
    inline NodeID getVarargNode(const llvm::Function *func) const {
        return symInfo->getVarargSym(func);
    }
    /// Get a field PAG Value node according to base value and offset
    NodeID getGepValNode(const llvm::Value* val, const LocationSet& ls, const llvm::Type *baseType, u32_t fieldidx);
    /// Get a field PAG Object node according to base mem obj and offset
    NodeID getGepObjNode(const MemObj* obj, const LocationSet& ls);
    /// Get a field obj PAG node according to a mem obj and a given offset
    NodeID getGepObjNode(NodeID id, const LocationSet& ls) ;
    /// Get a field-insensitive obj PAG node according to a mem obj
    //@{
    inline NodeID getFIObjNode(const MemObj* obj) const {
        return obj->getSymId();
    }
    inline NodeID getFIObjNode(NodeID id) const {
        PAGNode* node = pag->getPAGNode(id);
        assert(llvm::isa<ObjPN>(node) && "need an object node");
        ObjPN* obj = llvm::cast<ObjPN>(node);
        return getFIObjNode(obj->getMemObj());
    }
    //@}

    /// Get black hole and constant id
    //@{
    inline NodeID getBlackHoleNode() const {
        return symInfo->blackholeSymID();
    }
    inline NodeID getConstantNode() const {
        return symInfo->constantSymID();
    }
    inline NodeID getBlkPtr() const {
        return symInfo->blkPtrSymID();
    }
    inline NodeID getNullPtr() const {
        return symInfo->nullPtrSymID();
    }
    inline bool isBlkPtr(NodeID id) const {
        return (SymbolTableInfo::isBlkPtr(id));
    }
    inline bool isNullPtr(NodeID id) const {
        return (SymbolTableInfo::isNullPtr(id));
    }
    inline bool isBlkObjOrConstantObj(NodeID id) const {
        return (isBlkObj(id) || isConstantObj(id));
    }
    inline bool isBlkObj(NodeID id) const {
        PAGNode* node = getPAGNode(id);
        ObjPN* obj = llvm::dyn_cast<ObjPN>(node);
        assert(obj && "not an object node?");
        return (obj->getMemObj()->isBlackHoleObj());
    }
    inline bool isConstantObj(NodeID id) const {
        PAGNode* node = getPAGNode(id);
        ObjPN* obj = llvm::dyn_cast<ObjPN>(node);
        assert(obj && "not an object node?");
        return (obj->getMemObj()->isConstantObj());
    }
    inline bool isTaintedObj(NodeID id) const {
        PAGNode* node = getPAGNode(id);
        ObjPN* obj = llvm::dyn_cast<ObjPN>(node);
        assert(obj && "not an object node?");
        return (obj->getMemObj()->isTaintedObj());
    }
    inline bool isNonPointerObj(NodeID id) const {
        PAGNode* node = getPAGNode(id);
        if (FIObjPN* fiNode = llvm::dyn_cast<FIObjPN>(node)) {
            return (fiNode->getMemObj()->hasPtrObj() == false);
        }
        else if (GepObjPN* gepNode = llvm::dyn_cast<GepObjPN>(node)) {
            return (gepNode->getMemObj()->isNonPtrFieldObj(gepNode->getLocationSet()));
        }
        else if (llvm::isa<DummyObjPN>(node)) {
            return false;
        }
        else {
            assert(false && "expecting a object node");
            return false;
        }
    }
    inline const MemObj* getBlackHoleObj() const {
        return symInfo->getBlkObj();
    }
    inline const MemObj* getConstantObj() const {
        return symInfo->getConstantObj();
    }
    //@}

    /// Base and Offset methods for Value and Object node
    //@{
    /// Get a base pointer node given a field pointer
    NodeID getBaseValNode(NodeID nodeId);
    LocationSet getLocationSetFromBaseNode(NodeID nodeId);
    inline NodeID getBaseObjNode(NodeID id) const {
        return getBaseObj(id)->getSymId();
    }
    inline const MemObj* getBaseObj(NodeID id) const {
        const PAGNode* node = pag->getPAGNode(id);
        assert(llvm::isa<ObjPN>(node) && "need an object node");
        const ObjPN* obj = llvm::cast<ObjPN>(node);
        return obj->getMemObj();
    }
    //@}

    /// Get all fields of an object
    //@{
    NodeBS& getAllFieldsObjNode(const MemObj* obj);
    NodeBS& getAllFieldsObjNode(NodeID id);
    NodeBS getFieldsAfterCollapse(NodeID id);
    //@}

    /// add node into PAG
    //@{
    /// Add a PAG node into Node map
    inline NodeID addNode(PAGNode* node, NodeID i) {
        addGNode(i,node);
        return i;
    }
    /// Add a value (pointer) node
    inline NodeID addValNode(const llvm::Value* val, NodeID i) {
        PAGNode *node = new ValPN(val,i);
        return addValNode(val, node, i);
    }
    /// Add a memory obj node
    inline NodeID addObjNode(const llvm::Value* val, NodeID i) {
        MemObj* mem = symInfo->getObj(symInfo->getObjSym(val));
        assert(mem->getSymId() == i && "not same object id?");
        return addFIObjNode(mem, i);
    }
    /// Add a unique return node for a procedure
    inline NodeID addRetNode(const llvm::Function* val, NodeID i) {
        PAGNode *node = new RetPN(val,i);
        return addRetNode(val, node, i);
    }
    /// Add a unique vararg node for a procedure
    inline NodeID addVarargNode(const llvm::Function* val, NodeID i) {
        PAGNode *node = new VarArgPN(val,i);
        return addNode(node,i);
    }
    /// Add a temp field value node, this method can only invoked by getGepValNode
    NodeID addGepValNode(const llvm::Value* val, const LocationSet& ls, NodeID i, const llvm::Type *type, u32_t fieldidx);
    /// Add a field obj node, this method can only invoked by getGepObjNode
    NodeID addGepObjNode(const MemObj* obj, const LocationSet& ls, NodeID i);
    /// Add a field-insensitive node, this method can only invoked by getFIGepObjNode
    NodeID addFIObjNode(const MemObj* obj, NodeID i);
    //@}

    ///  Add a dummy value/object node according to node ID (llvm value is null)
    //@{
    inline NodeID addDummyValNode() {
        return addDummyValNode(nodeNum);
    }
    inline NodeID addDummyValNode(NodeID i) {
        return addValNode(NULL, new DummyValPN(i), i);
    }
    inline NodeID addDummyObjNode() {
        const MemObj* mem = SymbolTableInfo::Symbolnfo()->createDummyObj(nodeNum);
        return addObjNode(NULL, new DummyObjPN(nodeNum,mem), nodeNum);
    }
    inline NodeID addBlackholeObjNode() {
        return addObjNode(NULL, new DummyObjPN(getBlackHoleNode(),getBlackHoleObj()), getBlackHoleNode());
    }
    inline NodeID addConstantObjNode() {
        return addObjNode(NULL, new DummyObjPN(getConstantNode(),getConstantObj()), getConstantNode());
    }
    inline NodeID addBlackholePtrNode() {
        return addDummyValNode(getBlkPtr());
    }
    inline NodeID addNullPtrNode() {
        NodeID nullPtr = addDummyValNode(getNullPtr());
        /// let all undef value or non-determined pointers points-to black hole
        llvm::LLVMContext &cxt = getModule()->getContext();
        llvm::ConstantPointerNull *constNull = llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(cxt));
        setCurrentLocation(constNull, NULL);
        addBlackHoleAddrEdge(symInfo->blkPtrSymID());
        return nullPtr;
    }
    //@}

    /// Add a value (pointer) node
    inline NodeID addValNode(const llvm::Value* val, PAGNode *node, NodeID i) {
        return addNode(node,i);
    }
    /// Add a memory obj node
    inline NodeID addObjNode(const llvm::Value* val, PAGNode *node, NodeID i) {
        return addNode(node,i);
    }
    /// Add a unique return node for a procedure
    inline NodeID addRetNode(const llvm::Function* val, PAGNode *node, NodeID i) {
        return addNode(node,i);
    }
    /// Add a unique vararg node for a procedure
    inline NodeID addVarargNode(const llvm::Function* val, PAGNode *node, NodeID i) {
        return addNode(node,i);
    }

    /// Add an edge into PAG
    //@{
    /// Add a PAG edge
    bool addEdge(PAGNode* src, PAGNode* dst, PAGEdge* edge);
    void setCurrentBBAndValueForPAGEdge(PAGEdge* edge);

    //// Return true if this edge exits
    bool hasIntraEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind);
    bool hasInterEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind, const llvm::Instruction* cs);

    /// Add Address edge
    bool addAddrEdge(NodeID src, NodeID dst);
    /// Add Copy edge
    bool addCopyEdge(NodeID src, NodeID dst);
    /// Add Load edge
    bool addLoadEdge(NodeID src, NodeID dst);
    /// Add Store edge
    bool addStoreEdge(NodeID src, NodeID dst);
    /// Add Call edge
    bool addCallEdge(NodeID src, NodeID dst, const llvm::Instruction* cs);
    /// Add Return edge
    bool addRetEdge(NodeID src, NodeID dst, const llvm::Instruction* cs);
    /// Add Gep edge
    bool addGepEdge(NodeID src, NodeID dst, const LocationSet& ls, bool constGep);
    /// Add Offset(Gep) edge
    bool addNormalGepEdge(NodeID src, NodeID dst, const LocationSet& ls);
    /// Add Variant(Gep) edge
    bool addVariantGepEdge(NodeID src, NodeID dst);
    /// Add Thread fork edge for parameter passing
    bool addThreadForkEdge(NodeID src, NodeID dst, const llvm::Instruction* cs);
    /// Add Thread join edge for parameter passing
    bool addThreadJoinEdge(NodeID src, NodeID dst, const llvm::Instruction* cs);
    //@}

    /// Add global edges
    //{@
    /// Add global black hole Address edge
    bool addGlobalBlackHoleAddrEdge(NodeID node, const llvm::ConstantExpr *int2Ptrce);
    /// Add black hole Address edge for formal params
    bool addFormalParamBlackHoleAddrEdge(NodeID node, const llvm::Argument *arg);
    //@}

    /// Set a pointer points-to black hole (e.g. int2ptr)
    bool addBlackHoleAddrEdge(NodeID node);

    /// Get constant object node
    NodeID getNodeForConstantPointer(const llvm::Constant *C);

    /// Whether a node is a valid pointer
    //@{
    bool isValidPointer(NodeID nodeId) const;

    bool isValidTopLevelPtr(const PAGNode* node) {
        if (node->isTopLevelPtr()) {
            if (isValidPointer(node->getId()) && node->hasValue()) {
                if (analysisUtil::ArgInNoCallerFunction(node->getValue()))
                    return false;
                return true;
            }
        }
        return false;
    }
    //@}

    /// Return graph name
    inline std::string getGraphName() const {
        return "PAG";
    }

    /// Print PAG
    void print();

    /// Dump PAG
    void dump(std::string name);

    Size_t storeInstNum;		///< total num of store instructions
    Size_t loadInstNum;		///< total num of load instructions
};

namespace llvm {
/* !
 * GraphTraits specializations of PAG to be used for the generic graph algorithms.
 * Provide graph traits for tranversing from a PAG node using standard graph traversals.
 */
template<> struct GraphTraits<PAGNode*> : public GraphTraits<GenericNode<PAGNode,PAGEdge>*  > {
};

/// Inverse GraphTraits specializations for PAG node, it is used for inverse traversal.
template<> struct GraphTraits<Inverse<PAGNode *> > : public GraphTraits<Inverse<GenericNode<PAGNode,PAGEdge>* > > {
};

template<> struct GraphTraits<PAG*> : public GraphTraits<GenericGraph<PAGNode,PAGEdge>* > {
   typedef PAGNode *NodeRef;
};
}
#endif /* PAG_H_ */
