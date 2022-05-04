//===----- CHG.h -- Class hirachary graph  ---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CHG.h (previously CHA.h)
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 *
 * Created on: Aug 24, 2019
 *      Author: Mohamad Barbar
 */

#ifndef CHA_H_
#define CHA_H_

#include "Util/SVFModule.h"
#include "Graphs/GenericGraph.h"
#include "Util/WorkList.h"

namespace SVF
{

class SVFModule;
class CHNode;

typedef Set<const GlobalValue*> VTableSet;
typedef Set<const SVFFunction*> VFunSet;

/// Common base for class hierarchy graph. Only implements what PointerAnalysis needs.
class CommonCHGraph
{
public:
    virtual ~CommonCHGraph() { };
    enum CHGKind
    {
        Standard,
        DI
    };

    virtual bool csHasVFnsBasedonCHA(CallSite cs) = 0;
    virtual const VFunSet &getCSVFsBasedonCHA(CallSite cs) = 0;
    virtual bool csHasVtblsBasedonCHA(CallSite cs) = 0;
    virtual const VTableSet &getCSVtblsBasedonCHA(CallSite cs) = 0;
    virtual void getVFnsFromVtbls(CallSite cs, const VTableSet &vtbls, VFunSet &virtualFunctions) = 0;

    CHGKind getKind(void) const
    {
        return kind;
    }

protected:
    CHGKind kind;
};


typedef GenericEdge<CHNode> GenericCHEdgeTy;
class CHEdge: public GenericCHEdgeTy
{
public:
    typedef enum
    {
        INHERITANCE = 0x1, // inheritance relation
        INSTANTCE = 0x2 // template-instance relation
    } CHEDGETYPE;

    typedef GenericNode<CHNode,CHEdge>::GEdgeSetTy CHEdgeSetTy;

    CHEdge(CHNode *s, CHNode *d, CHEDGETYPE et, GEdgeFlag k = 0):
        GenericCHEdgeTy(s,d,k)
    {
        edgeType = et;
    }

    CHEDGETYPE getEdgeType() const
    {
        return edgeType;
    }

private:
    CHEDGETYPE edgeType;
};

typedef GenericNode<CHNode,CHEdge> GenericCHNodeTy;
class CHNode: public GenericCHNodeTy
{
public:
    typedef enum
    {
        PURE_ABSTRACT = 0x1, // pure virtual abstract class
        MULTI_INHERITANCE = 0x2, // multi inheritance class
        TEMPLATE = 0x04 // template class
    } CLASSATTR;

    typedef std::vector<const SVFFunction*> FuncVector;

    CHNode (const std::string name, NodeID i = 0, GNodeK k = 0):
        GenericCHNodeTy(i, k), vtable(nullptr), className(name), flags(0)
    {
    }
    ~CHNode()
    {
    }
    std::string getName() const
    {
        return className;
    }
    /// Flags
    //@{
    inline void setFlag(CLASSATTR mask)
    {
        flags |= mask;
    }
    inline bool hasFlag(CLASSATTR mask) const
    {
        return (flags & mask) == mask;
    }
    //@}

    /// Attribute
    //@{
    inline void setPureAbstract()
    {
        setFlag(PURE_ABSTRACT);
    }
    inline void setMultiInheritance()
    {
        setFlag(MULTI_INHERITANCE);
    }
    inline void setTemplate()
    {
        setFlag(TEMPLATE);
    }
    inline bool isPureAbstract() const
    {
        return hasFlag(PURE_ABSTRACT);
    }
    inline bool isMultiInheritance() const
    {
        return hasFlag(MULTI_INHERITANCE);
    }
    inline bool isTemplate() const
    {
        return hasFlag(TEMPLATE);
    }
    //@}

    void addVirtualFunctionVector(FuncVector vfuncvec)
    {
        virtualFunctionVectors.push_back(vfuncvec);
    }
    const std::vector<FuncVector> &getVirtualFunctionVectors() const
    {
        return virtualFunctionVectors;
    }
    void getVirtualFunctions(u32_t idx, FuncVector &virtualFunctions) const;

    const GlobalValue *getVTable() const
    {
        return vtable;
    }

    void setVTable(const GlobalValue *vtbl)
    {
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
    std::vector<FuncVector> virtualFunctionVectors;
};

/// class hierarchy graph
typedef GenericGraph<CHNode,CHEdge> GenericCHGraphTy;
class CHGraph: public CommonCHGraph, public GenericCHGraphTy
{
    friend class CHGBuilder;

public:
    typedef Set<const CHNode*> CHNodeSetTy;
    typedef FIFOWorkList<const CHNode*> WorkList;
    typedef Map<std::string, CHNodeSetTy> NameToCHNodesMap;
    typedef Map<CallSite, CHNodeSetTy> CallSiteToCHNodesMap;
    typedef Map<CallSite, VTableSet> CallSiteToVTableSetMap;
    typedef Map<CallSite, VFunSet> CallSiteToVFunSetMap;

    typedef enum
    {
        CONSTRUCTOR = 0x1, // connect node based on constructor
        DESTRUCTOR = 0x2 // connect node based on destructor
    } RELATIONTYPE;

    CHGraph(SVFModule* svfModule): svfMod(svfModule), classNum(0), vfID(0), buildingCHGTime(0)
    {
        this->kind = Standard;
    }
    ~CHGraph() override = default;

    void addEdge(const std::string className,
                 const std::string baseClassName,
                 CHEdge::CHEDGETYPE edgeType);
    CHNode *getNode(const std::string name) const;
    void getVFnsFromVtbls(CallSite cs, const VTableSet &vtbls, VFunSet &virtualFunctions) override;
    void dump(const std::string& filename);
    void view();
    void printCH();

    inline u32_t getVirtualFunctionID(const SVFFunction* vfn) const
    {
        Map<const SVFFunction*, u32_t>::const_iterator it =
            virtualFunctionToIDMap.find(vfn);
        if (it != virtualFunctionToIDMap.end())
            return it->second;
        else
            return -1;
    }
    inline const SVFFunction* getVirtualFunctionBasedonID(u32_t id) const
    {
        Map<const SVFFunction*, u32_t>::const_iterator it, eit;
        for (it = virtualFunctionToIDMap.begin(), eit =
                    virtualFunctionToIDMap.end(); it != eit; ++it)
        {
            if (it->second == id)
                return it->first;
        }
        return nullptr;
    }

    inline void addInstances(const std::string templateName, CHNode* node)
    {
        NameToCHNodesMap::iterator it = templateNameToInstancesMap.find(
                                            templateName);
        if (it != templateNameToInstancesMap.end())
            it->second.insert(node);
        else
            templateNameToInstancesMap[templateName].insert(node);
    }
    inline const CHNodeSetTy &getDescendants(const std::string className)
    {
        return classNameToDescendantsMap[className];
    }
    inline const CHNodeSetTy &getInstances(const std::string className)
    {
        return templateNameToInstancesMap[className];
    }

    inline bool csHasVtblsBasedonCHA(CallSite cs) override
    {
        CallSiteToVTableSetMap::const_iterator it = csToCHAVtblsMap.find(cs);
        return it != csToCHAVtblsMap.end();
    }
    inline bool csHasVFnsBasedonCHA(CallSite cs) override
    {
        CallSiteToVFunSetMap::const_iterator it = csToCHAVFnsMap.find(cs);
        return it != csToCHAVFnsMap.end();
    }
    inline const VTableSet &getCSVtblsBasedonCHA(CallSite cs) override
    {
        CallSiteToVTableSetMap::const_iterator it = csToCHAVtblsMap.find(cs);
        assert(it != csToCHAVtblsMap.end() && "cs does not have vtabls based on CHA.");
        return it->second;
    }
    inline const VFunSet &getCSVFsBasedonCHA(CallSite cs) override
    {
        CallSiteToVFunSetMap::const_iterator it = csToCHAVFnsMap.find(cs);
        assert(it != csToCHAVFnsMap.end() && "cs does not have vfns based on CHA.");
        return it->second;
    }

    static inline bool classof(const CommonCHGraph *chg)
    {
        return chg->getKind() == Standard;
    }


private:
    SVFModule* svfMod;
    u32_t classNum;
    u32_t vfID;
    double buildingCHGTime;
    Map<std::string, CHNode *> classNameToNodeMap;
    NameToCHNodesMap classNameToDescendantsMap;
    NameToCHNodesMap classNameToAncestorsMap;
    NameToCHNodesMap classNameToInstAndDescsMap;
    NameToCHNodesMap templateNameToInstancesMap;
    CallSiteToCHNodesMap csToClassesMap;

    Map<const SVFFunction*, u32_t> virtualFunctionToIDMap;
    CallSiteToVTableSetMap csToCHAVtblsMap;
    CallSiteToVFunSetMap csToCHAVFnsMap;
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::CHNode*> : public GraphTraits<SVF::GenericNode<SVF::CHNode,SVF::CHEdge>*  >
{
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVF::CHNode*> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::CHNode,SVF::CHEdge>* > >
{
};

template<> struct GraphTraits<SVF::CHGraph*> : public GraphTraits<SVF::GenericGraph<SVF::CHNode,SVF::CHEdge>* >
{
    typedef SVF::CHNode *NodeRef;
};

} // End namespace llvm

#endif /* CHA_H_ */
