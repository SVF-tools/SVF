//===----- CHA.h -- Base class of pointer analyses ---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * CHA.h
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#ifndef CHA_H_
#define CHA_H_

#include "MemoryModel/GenericGraph.h"
#include "Util/SVFModule.h"

class SVFModule;
class CHNode;

typedef GenericEdge<CHNode> GenericCHEdgeTy;
class CHEdge: public GenericCHEdgeTy {
public:
    typedef enum {
        INHERITANCE = 0x1, // inheritance relation
        INSTANTCE = 0x2 // template-instance relation
    } CHEDGETYPE;

    CHEdge(CHNode *s, CHNode *d, CHEDGETYPE et, GEdgeFlag k = 0):
        GenericCHEdgeTy(s,d,k) {
        edgeType = et;
    }

    CHEDGETYPE getEdgeType() const {
        return edgeType;
    }

private:
    CHEDGETYPE edgeType;
};

typedef GenericNode<CHNode,CHEdge> GenericCHNodeTy;
class CHNode: public GenericCHNodeTy {
public:
    typedef enum {
        PURE_ABSTRACT = 0x1, // pure virtual abstract class
        MULTI_INHERITANCE = 0x2, // multi inheritance class
        TEMPLATE = 0x04 // template class
    } CLASSATTR;

    CHNode (const std::string name, NodeID i = 0, GNodeK k = 0):
        GenericCHNodeTy(i, k), vtable(NULL), className(name), flags(0) {
    }
    ~CHNode() {
    }
    std::string getName() const {
        return className;
    }
    /// Flags
    //@{
    inline void setFlag(CLASSATTR mask) {
        flags |= mask;
    }
    inline bool hasFlag(CLASSATTR mask) const {
        return (flags & mask) == mask;
    }
    //@}

    /// Attribute
    //@{
    inline void setPureAbstract() {
        setFlag(PURE_ABSTRACT);
    }
    inline void setMultiInheritance() {
        setFlag(MULTI_INHERITANCE);
    }
    inline void setTemplate() {
        setFlag(TEMPLATE);
    }
    inline bool isPureAbstract() const {
        return hasFlag(PURE_ABSTRACT);
    }
    inline bool isMultiInheritance() const {
        return hasFlag(MULTI_INHERITANCE);
    }
    inline bool isTemplate() const {
        return hasFlag(TEMPLATE);
    }
    //@}

    void setArgsizeToVFunMap(s32_t argsize,
                             std::set<const llvm::Function*>functions) {
        argsizeToVFunMap[argsize] = functions;
    }
    void addVirtualFunction(const llvm::Function* vfunc) {
        allVirtualFunctions.insert(vfunc);
    }
    const std::set<const llvm::Function*> &getAllVirtualFunctions() const {
        return allVirtualFunctions;
    }
    const std::set<const llvm::Function*> getVirtualFunctions(s32_t argsize) const {
        const std::map<s32_t, std::set<const llvm::Function*>>::const_iterator it =
                    argsizeToVFunMap.find(argsize);
        if (it != argsizeToVFunMap.end())
            return it->second;
        else
            return std::set<const llvm::Function*>();
    }

    void addVirtualFunctionVector(std::vector<const llvm::Function*> vfuncvec) {
        virtualFunctionVectors.push_back(vfuncvec);
    }
    const std::vector<std::vector<const llvm::Function*>> &getVirtualFunctionVectors() const {
        return virtualFunctionVectors;
    }
    void getVirtualFunctions(u32_t idx, std::set<const llvm::Function*> &virtualFunctions) const;

    const llvm::Value *getVTable() const {
        return vtable;
    }

    void setVTable(const llvm::Value *vtbl) {
        vtable = vtbl;
    }

private:
    const llvm::Value *vtable;
    std::string className;
    size_t flags;
    std::set<const llvm::Function*> allVirtualFunctions;
    /*
     * virtual functions inherited from different classes are separately stored
     * to model different vtables inherited from different fathers.
     *
     * Example:
     * class C: public A, public B
     * vtableC = {Af1, Af2, ..., inttoptr, Bg1, Bg2, ...} ("inttoptr"
     * instruction works as the delimiter for separating virtual functions
     * inherited from different classes)
     *
     * virtualFunctionVectors = {{Af1, Af2, ...}, {Bg1, Bg2, ...}}
     */
    std::vector<std::vector<const llvm::Function*>> virtualFunctionVectors;
    std::map<s32_t, std::set<const llvm::Function*>> argsizeToVFunMap;
};

/// class hierarchy graph
typedef GenericGraph<CHNode,CHEdge> GenericCHGraphTy;
class CHGraph: public GenericCHGraphTy {
public:
    typedef std::set<const CHNode*> CHNodeSetTy;
    typedef enum {
        CONSTRUCTOR = 0x1, // connect node based on constructor
        DESTRUCTOR = 0x2 // connect node based on destructor
    } RELATIONTYPE;

    CHGraph(): classNum(0), vfID(0), buildingCHGTime(0) {
    }
    ~CHGraph();

    void buildCHG(const SVFModule svfModule);
    void constructCHGraphFromIR(const llvm::Module &M);
    void buildInternalMaps();
    void buildCHGOnFunction(const llvm::Function *F);
    void buildCHGOnBasicBlock(const llvm::BasicBlock *B,
                              const std::string className,
                              RELATIONTYPE t);
    void addEdge(const std::string className,
                 const std::string baseClassName,
                 CHEdge::CHEDGETYPE edgeType);
    CHNode *getNode(const std::string name) const;
    CHNode *getOrCreateNode(const std::string name);
    void addToNodeList(CHNode* node);
    /// Dump the graph
    void dump(const std::string& filename);
    void collectAncestorsDescendants(const CHNode *node);
    void buildClassNameToAncestorsDescendantsMap();
    void buildClassNameToNamesMap();
    void buildVirtualFunctionToIDMap();
    s32_t getVirtualFunctionID(const llvm::Function *vfn) const;
    const llvm::Function *getVirtualFunctionBasedonID(s32_t id) const;
    void buildTemplateNameToInstancesMap();
    void buildArgsizeToVFunMap();
    bool hasDescendants(const std::string className) const;
    bool hasAncestors(const std::string name) const;
    bool hasInstances(const std::string name) const;
    const CHNodeSetTy &getDescendants(const std::string className) const;
    const CHNodeSetTy &getAncestors(const std::string name) const;
    const CHNodeSetTy &getInstances(const std::string name) const;
    std::set<std::string> getDescendantsNames(const std::string className) const;
    std::set<std::string> getAncestorsNames(const std::string className) const;
    std::set<std::string> getInstancesNames(const std::string className) const;
    void readInheritanceMetadataFromModule(const llvm::Module &M);
    void analyzeVTables(const llvm::Module &M);
    std::string getClassNameOfThisPtr(llvm::CallSite cs) const;
    std::string getFunNameOfVCallSite(llvm::CallSite cs) const;
    CHNodeSetTy getTemplateInstancesAndDescendants(const std::string className) const;
    void getCSClasses(llvm::CallSite cs, CHNodeSetTy &chClasses) const;

    bool VCallInCtorOrDtor(llvm::CallSite cs) const;
    void getVFnsFromVtbls(llvm::CallSite cs,
                          const std::set<const llvm::Value*> &vtbls,
                          std::set<const llvm::Function*> &virtualFunctions) const;

    void collectVirtualCallSites();
    void buildCSToCHAVtblsAndVfnsMap();
    const bool csHasVtblsBasedonCHA(llvm::CallSite cs) const;
    const bool csHasVFnsBasedonCHA(llvm::CallSite cs) const;
    const std::set<const llvm::Value*> &getCSVtblsBasedonCHA(llvm::CallSite cs) const;
    const std::set<const llvm::Function*> &getCSVFsBasedonCHA(llvm::CallSite cs) const;
private:
    u32_t classNum;
    s32_t vfID;
    double buildingCHGTime;
    std::map<std::string, CHNodeSetTy> classNameToDescendantsMap;
    std::map<std::string, CHNodeSetTy> classNameToAncestorsMap;
    std::map<std::string, CHNodeSetTy> templateNameToInstancesMap;
    std::map<std::string, std::set<std::string>> classNameToDescendantsNamesMap;
    std::map<std::string, std::set<std::string>> classNameToAncestorsNamesMap;
    std::map<std::string, std::set<std::string>> templateNameToInstancesNamesMap;
    std::map<const llvm::Function*, s32_t> virtualFunctionToIDMap;

    SVFModule svfMod;
    std::set<llvm::CallSite> virtualCallSites;
    std::map<llvm::CallSite, std::set<const llvm::Value*>> csToCHAVtblsMap;
    std::map<llvm::CallSite, std::set<const llvm::Function*>> csToCHAVFnsMap;
};


namespace llvm {
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<CHNode*> : public GraphTraits<GenericNode<CHNode,CHEdge>*  > {
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<CHNode*> > : public GraphTraits<Inverse<GenericNode<CHNode,CHEdge>* > > {
};

template<> struct GraphTraits<CHGraph*> : public GraphTraits<GenericGraph<CHNode,CHEdge>* > {
    typedef CHNode *NodeRef;
};

}

#endif /* CHA_H_ */
