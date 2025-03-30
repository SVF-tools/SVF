//===----- CHG.h -- Class hierarchy graph  ---------------------------//
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

#include "Util/ThreadAPI.h"
#include "Graphs/GenericGraph.h"
#include "Util/WorkList.h"

namespace SVF
{

class CHNode;
class GlobalObjVar;

typedef Set<const GlobalObjVar*> VTableSet;
typedef Set<const FunObjVar*> VFunSet;

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

    virtual bool csHasVFnsBasedonCHA(const CallICFGNode* cs) = 0;
    virtual const VFunSet &getCSVFsBasedonCHA(const CallICFGNode* cs) = 0;
    virtual bool csHasVtblsBasedonCHA(const CallICFGNode* cs) = 0;
    virtual const VTableSet &getCSVtblsBasedonCHA(const CallICFGNode* cs) = 0;
    virtual void getVFnsFromVtbls(const CallICFGNode* cs, const VTableSet& vtbls,
                                  VFunSet& virtualFunctions) = 0;

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
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    typedef enum
    {
        INHERITANCE = 0x1, // inheritance relation
        INSTANTCE = 0x2    // template-instance relation
    } CHEDGETYPE;

    typedef GenericNode<CHNode, CHEdge>::GEdgeSetTy CHEdgeSetTy;

    CHEdge(CHNode* s, CHNode* d, CHEDGETYPE et, GEdgeFlag k = 0)
        : GenericCHEdgeTy(s, d, k)
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

typedef GenericNode<CHNode, CHEdge> GenericCHNodeTy;
class CHNode: public GenericCHNodeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;

public:
    typedef enum
    {
        PURE_ABSTRACT = 0x1, // pure virtual abstract class
        MULTI_INHERITANCE = 0x2, // multi inheritance class
        TEMPLATE = 0x04 // template class
    } CLASSATTR;

    typedef std::vector<const FunObjVar*> FuncVector;

    CHNode (const std::string& name, NodeID i = 0, GNodeK k = CHNodeKd):
        GenericCHNodeTy(i, k), vtable(nullptr), className(name), flags(0)
    {
    }
    ~CHNode()
    {
    }
    virtual const std::string& getName() const
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

    const GlobalObjVar *getVTable() const
    {
        return vtable;
    }

    void setVTable(const GlobalObjVar *vtbl)
    {
        vtable = vtbl;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CHNode *)
    {
        return true;
    }

    static inline bool classof(const GenericCHNodeTy * node)
    {
        return node->getNodeKind() == CHNodeKd;
    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == CHNodeKd;
    }
    //@}

private:
    const GlobalObjVar* vtable;
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

protected:
    inline size_t getFlags() const
    {
        return flags;
    }
    inline void setFlags(size_t f)
    {
        flags = f;
    }
};

/// class hierarchy graph
typedef GenericGraph<CHNode, CHEdge> GenericCHGraphTy;
class CHGraph: public CommonCHGraph, public GenericCHGraphTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class CHGBuilder;

public:
    typedef Set<const CHNode*> CHNodeSetTy;
    typedef FIFOWorkList<const CHNode*> WorkList;
    typedef Map<std::string, CHNodeSetTy> NameToCHNodesMap;

    typedef Map<const ICFGNode*, CHNodeSetTy> CallNodeToCHNodesMap;
    typedef Map<const ICFGNode*, VTableSet> CallNodeToVTableSetMap;
    typedef Map<const ICFGNode*, VFunSet> CallNodeToVFunSetMap;

    typedef enum
    {
        CONSTRUCTOR = 0x1, // connect node based on constructor
        DESTRUCTOR = 0x2 // connect node based on destructor
    } RELATIONTYPE;

    CHGraph(): classNum(0), vfID(0), buildingCHGTime(0)
    {
        this->kind = Standard;
    }
    ~CHGraph() override = default;

    void addEdge(const std::string className,
                 const std::string baseClassName,
                 CHEdge::CHEDGETYPE edgeType);
    CHNode *getNode(const std::string name) const;
    void getVFnsFromVtbls(const CallICFGNode* cs, const VTableSet &vtbls, VFunSet &virtualFunctions) override;
    void dump(const std::string& filename);
    void view();
    void printCH();

    inline u32_t getVirtualFunctionID(const FunObjVar* vfn) const
    {
        Map<const FunObjVar*, u32_t>::const_iterator it =
            virtualFunctionToIDMap.find(vfn);
        if (it != virtualFunctionToIDMap.end())
            return it->second;
        else
            return -1;
    }
    inline const FunObjVar* getVirtualFunctionBasedonID(u32_t id) const
    {
        Map<const FunObjVar*, u32_t>::const_iterator it, eit;
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

    bool csHasVtblsBasedonCHA(const CallICFGNode* cs) override;
    bool csHasVFnsBasedonCHA(const CallICFGNode* cs) override;
    const VTableSet &getCSVtblsBasedonCHA(const CallICFGNode* cs) override;
    const VFunSet &getCSVFsBasedonCHA(const CallICFGNode* cs) override;

    static inline bool classof(const CommonCHGraph *chg)
    {
        return chg->getKind() == Standard;
    }


private:
    u32_t classNum;
    u32_t vfID;
    double buildingCHGTime;
    Map<std::string, CHNode*> classNameToNodeMap;
    NameToCHNodesMap classNameToDescendantsMap;
    NameToCHNodesMap classNameToAncestorsMap;
    NameToCHNodesMap classNameToInstAndDescsMap;
    NameToCHNodesMap templateNameToInstancesMap;
    CallNodeToCHNodesMap callNodeToClassesMap;

    Map<const FunObjVar*, u32_t> virtualFunctionToIDMap;

    CallNodeToVTableSetMap callNodeToCHAVtblsMap;
    CallNodeToVFunSetMap callNodeToCHAVFnsMap;
};

} // End namespace SVF

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template <>
struct GenericGraphTraits<SVF::CHNode*>
    : public GenericGraphTraits<SVF::GenericNode<SVF::CHNode, SVF::CHEdge>*>
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used
/// for inverse traversal.
template <>
struct GenericGraphTraits<Inverse<SVF::CHNode*>>
            : public GenericGraphTraits<
              Inverse<SVF::GenericNode<SVF::CHNode, SVF::CHEdge>*>>
{
};

template <>
struct GenericGraphTraits<SVF::CHGraph*>
    : public GenericGraphTraits<SVF::GenericGraph<SVF::CHNode, SVF::CHEdge>*>
{
    typedef SVF::CHNode* NodeRef;
};

} // End namespace llvm

#endif /* CHA_H_ */
