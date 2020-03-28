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

#include "Util/SVFModule.h"
#include "Graphs/GenericGraph.h"
#include "Util/WorkList.h"

class SVFModule;
class CHNode;

typedef GenericEdge<CHNode> GenericCHEdgeTy;
class CHEdge: public GenericCHEdgeTy {
public:
    typedef enum {
        INHERITANCE = 0x1, // inheritance relation
        INSTANTCE = 0x2 // template-instance relation
    } CHEDGETYPE;

    typedef GenericNode<CHNode,CHEdge>::GEdgeSetTy CHEdgeSetTy;

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

    typedef std::vector<const Function*> FuncVector;

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

    void addVirtualFunctionVector(FuncVector vfuncvec) {
        virtualFunctionVectors.push_back(vfuncvec);
    }
    const std::vector<FuncVector> &getVirtualFunctionVectors() const {
        return virtualFunctionVectors;
    }
    void getVirtualFunctions(u32_t idx, FuncVector &virtualFunctions) const;

    const GlobalValue *getVTable() const {
        return vtable;
    }

    void setVTable(const GlobalValue *vtbl) {
        vtable = vtbl;
    }

private:
    const GlobalValue* vtable;
    std::string className;
    size_t flags;
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
    std::vector<std::vector<const Function*>> virtualFunctionVectors;
};

/// class hierarchy graph
typedef GenericGraph<CHNode,CHEdge> GenericCHGraphTy;
class CHGraph: public GenericCHGraphTy {
public:
    typedef std::set<const CHNode*> CHNodeSetTy;
    typedef FIFOWorkList<const CHNode*> WorkList;
    typedef std::map<std::string, CHNodeSetTy> NameToCHNodesMap;
    typedef std::map<CallSite, CHNodeSetTy> CallSiteToCHNodesMap;
    typedef std::set<const GlobalValue*> VTableSet;
    typedef std::set<const Function*> VFunSet;
    typedef std::map<CallSite, VTableSet> CallSiteToVTableSetMap;
    typedef std::map<CallSite, VFunSet> CallSiteToVFunSetMap;

    typedef enum {
        CONSTRUCTOR = 0x1, // connect node based on constructor
        DESTRUCTOR = 0x2 // connect node based on destructor
    } RELATIONTYPE;

    CHGraph(SVFModule* svfModule): svfMod(svfModule), classNum(0), vfID(0), buildingCHGTime(0) {
    }
    ~CHGraph();

    void buildCHG();
    void buildInternalMaps();
    void buildCHGNodes(const GlobalValue *V);
    void buildCHGNodes(const Function *F);
    void buildCHGEdges(const Function *F);
    void connectInheritEdgeViaCall(const Function *caller, CallSite cs);
    void connectInheritEdgeViaStore(const Function *caller, const StoreInst* store);
    void addEdge(const std::string className,
                 const std::string baseClassName,
                 CHEdge::CHEDGETYPE edgeType);
    CHNode *getNode(const std::string name) const;
    CHNode *createNode(const std::string name);
    void buildClassNameToAncestorsDescendantsMap();
    void buildVirtualFunctionToIDMap();
    void buildCSToCHAVtblsAndVfnsMap();
    void readInheritanceMetadataFromModule(const Module &M);
    void analyzeVTables(const Module &M);
    const CHGraph::CHNodeSetTy& getInstancesAndDescendants(const std::string className);
    const CHNodeSetTy& getCSClasses(CallSite cs);
    void getVFnsFromVtbls(CallSite cs,VTableSet &vtbls, VFunSet &virtualFunctions) const;
    void dump(const std::string& filename);
    void printCH();

    inline s32_t getVirtualFunctionID(const Function *vfn) const {
		std::map<const Function*, s32_t>::const_iterator it =
				virtualFunctionToIDMap.find(vfn);
		if (it != virtualFunctionToIDMap.end())
			return it->second;
		else
			return -1;
	}
	inline const Function *getVirtualFunctionBasedonID(s32_t id) const {
		std::map<const Function*, s32_t>::const_iterator it, eit;
		for (it = virtualFunctionToIDMap.begin(), eit =
				virtualFunctionToIDMap.end(); it != eit; ++it) {
			if (it->second == id)
				return it->first;
		}
		return NULL;
	}

	inline void addInstances(const std::string templateName, CHNode* node) {
		NameToCHNodesMap::iterator it = templateNameToInstancesMap.find(
				templateName);
		if (it != templateNameToInstancesMap.end())
			it->second.insert(node);
		else
			templateNameToInstancesMap[templateName].insert(node);
	}
	inline const CHNodeSetTy &getDescendants(const std::string className) {
		return classNameToDescendantsMap[className];
	}
	inline const CHNodeSetTy &getInstances(const std::string className) {
		return templateNameToInstancesMap[className];
	}

	inline const bool csHasVtblsBasedonCHA(CallSite cs) const {
		CallSiteToVTableSetMap::const_iterator it = csToCHAVtblsMap.find(cs);
		return it != csToCHAVtblsMap.end();
	}
	inline const bool csHasVFnsBasedonCHA(CallSite cs) const {
		CallSiteToVFunSetMap::const_iterator it = csToCHAVFnsMap.find(cs);
		return it != csToCHAVFnsMap.end();
	}
	inline const VTableSet &getCSVtblsBasedonCHA(CallSite cs) const {
		CallSiteToVTableSetMap::const_iterator it = csToCHAVtblsMap.find(cs);
		assert(it != csToCHAVtblsMap.end() && "cs does not have vtabls based on CHA.");
		return it->second;
	}
	inline const VFunSet &getCSVFsBasedonCHA(CallSite cs) const {
		CallSiteToVFunSetMap::const_iterator it = csToCHAVFnsMap.find(cs);
		assert(it != csToCHAVFnsMap.end() && "cs does not have vfns based on CHA.");
		return it->second;
	}

private:
    SVFModule* svfMod;
    u32_t classNum;
    s32_t vfID;
    double buildingCHGTime;
    std::map<const std::string, CHNode *> classNameToNodeMap;
    NameToCHNodesMap classNameToDescendantsMap;
    NameToCHNodesMap classNameToAncestorsMap;
    NameToCHNodesMap classNameToInstAndDescsMap;
    NameToCHNodesMap templateNameToInstancesMap;
    CallSiteToCHNodesMap csToClassesMap;

    std::map<const Function*, s32_t> virtualFunctionToIDMap;
    CallSiteToVTableSetMap csToCHAVtblsMap;
    CallSiteToVFunSetMap csToCHAVFnsMap;
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
