//===----- CFLSolver.h -- Context-free language reachability solver--------------//
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
 * CFLSolver.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui， Yuxiang Lei
 */

#ifndef INCLUDE_CFL_CFLSolver_H_
#define INCLUDE_CFL_CFLSolver_H_

#include "Graphs/CFLGraph.h"
#include "CFL/CFLGrammar.h"
#include "Util/WorkList.h"

using namespace std;

namespace SVF
{

typedef GrammarBase::Symbol Label;
/*!
 * Adjacency-list graph representation
 */
class CFLData
{
public:
    typedef std::map<const Label, NodeBS> TypeMap;                  // Label with SparseBitVector of NodeID
    typedef std::unordered_map<NodeID, TypeMap> DataMap;            // Each Node has a TypeMap
    typedef typename DataMap::iterator iterator;                    // iterator for each node
    typedef typename DataMap::const_iterator const_iterator;        // const iterator

protected:
    DataMap succMap;                                                // succ map for nodes contains Label: Edgeset
    DataMap predMap;                                                // pred map for nodes contains Label: edgeset
    const NodeBS emptyData;                                         // ??
    NodeBS diff;                                                    // ??

    // union/add data
    //@{
    inline bool addPred(const NodeID key, const NodeID src, const Label ty)
    {
        return predMap[key][ty].test_and_set(src);
    };

    inline bool addSucc(const NodeID key, const NodeID dst, const Label ty)
    {
        return succMap[key][ty].test_and_set(dst);
    };

    inline bool addPreds(const NodeID key, const NodeBS& data, const Label ty)
    {
        if (data.empty())
            return false;
        return predMap[key][ty] |= data;                            // union of sparsebitvector (add to LHS)
    }

    inline bool addSuccs(const NodeID key, const NodeBS& data, const Label ty)
    {
        if (data.empty())
            return false;
        return succMap[key][ty] |= data;                            // // union of sparsebitvector (add to LHS)
    }
    //@}

public:
    // Constructor
    CFLData()
    {}

    // Destructor
    virtual ~CFLData()
    {}

    virtual void clear()
    {
        succMap.clear();
        predMap.clear();
    }

    inline const_iterator begin() const
    {
        return succMap.begin();
    }

    inline const_iterator end() const
    {
        return succMap.end();
    }

    inline iterator begin()
    {
        return succMap.begin();
    }

    inline iterator end()
    {
        return succMap.end();
    }

    inline DataMap& getSuccMap()
    {
        return succMap;
    }

    inline DataMap& getPredMap()
    {
        return predMap;
    }

    inline TypeMap& getSuccMap(const NodeID key)
    {
        return succMap[key];
    }

    inline TypeMap& getPredMap(const NodeID key)
    {
        return predMap[key];
    }

    inline NodeBS& getSuccs(const NodeID key, const Label ty)
    {
        return succMap[key][ty];
    }

    inline NodeBS& getPreds(const NodeID key, const Label ty)
    {
        return predMap[key][ty];
    }

    // Alias data operations
    //@{
    inline bool addEdge(const NodeID src, const NodeID dst, const Label ty)
    {
        addSucc(src, dst, ty);
        return addPred(dst, src, ty);
    }

    /// add edges and return the set of added edges (dst) for src
    inline NodeBS addEdges(const NodeID src, const NodeBS& dstData, const Label ty)
    {
        NodeBS newDsts;
        if (addSuccs(src, dstData, ty))
        {
            for (const NodeID datum: dstData)
                if (addPred(datum, src, ty))
                    newDsts.set(datum);
        }
        return newDsts;
    }

    /// add edges and return the set of added edges (src) for dst
    inline NodeBS addEdges(const NodeBS& srcData, const NodeID dst, const Label ty)
    {
        NodeBS newSrcs;
        if (addPreds(dst, srcData, ty))
        {
            for (const NodeID datum: srcData)
                if (addSucc(datum, dst, ty))
                    newSrcs.set(datum);
        }
        return newSrcs;
    }

    /// find src -> find src[ty] -> find dst in set
    inline bool hasEdge(const NodeID src, const NodeID dst, const Label ty)
    {
        const_iterator iter1 = succMap.find(src);
        if (iter1 == succMap.end())
            return false;

        auto iter2 = iter1->second.find(ty);
        if (iter2 == iter1->second.end())
            return false;

        return iter2->second.test(dst);
    }

    /* This is a dataset version, to be modified to a cflData version */
    inline void clearEdges(const NodeID key)
    {
        succMap[key].clear();
        predMap[key].clear();
    }
    //@}
};

/*!
 * Hybrid graph representation for transitive relations
 * The implementation is based on
 * Yuxiang Lei, Yulei Sui, Shuo Ding, and Qirun Zhang.
 * Taming Transitive Redundancy for Context-Free Language Reachability.
 * ACM SIGPLAN Conference on Object-Oriented Programming, Systems, Languages, and Applications
 */
class HybridData
{
public:
    struct TreeNode
    {
        NodeID id;
        std::unordered_set<TreeNode*> children;

        TreeNode(NodeID nId) : id(nId)
        {}

        ~TreeNode()
        {
        }

        inline bool operator==(const TreeNode& rhs) const
        {
            return id == rhs.id;
        }

        inline bool operator<(const TreeNode& rhs) const
        {
            return id < rhs.id;
        }
    };


public:
    Map<NodeID, std::unordered_map<NodeID, TreeNode*>> indMap;   // indMap[v][u] points to node v in tree(u)

    HybridData()
    {}

    ~HybridData()
    {
        for (auto iter1: indMap)
        {
            for (auto iter2: iter1.second)
            {
                delete iter2.second;
                iter2.second = NULL;
            }
        }
    }

    bool hasInd(NodeID src, NodeID dst)
    {
        auto it = indMap.find(dst);
        if (it == indMap.end())
            return false;
        return (it->second.find(src) != it->second.end());
    }

    /// Add a node dst to tree(src)
    TreeNode* addInd(NodeID src, NodeID dst)
    {
        auto resIns = indMap[dst].insert(std::make_pair(src, new TreeNode(dst)));
        if (resIns.second)
            return resIns.first->second;
        return nullptr;
    }

    /// Get the node dst in tree(src)
    TreeNode* getNode(NodeID src, NodeID dst)
    {
        return indMap[dst][src];
    }

    /// add v into desc(x) as a child of u
    void insertEdge(TreeNode* u, TreeNode* v)
    {
        u->children.insert(v);
    }

    void addArc(NodeID src, NodeID dst)
    {
        if (!hasInd(src, dst))
        {
            for (auto iter: indMap[src])
            {
                meld(iter.first, getNode(iter.first, src), getNode(dst, dst));
            }
        }
    }

    void meld(NodeID x, TreeNode* uNode, TreeNode* vNode)
    {
        TreeNode* newVNode = addInd(x, vNode->id);
        if (!newVNode)
            return;

        insertEdge(uNode, newVNode);
        for (TreeNode* vChild: vNode->children)
        {
            meld(x, newVNode, vChild);
        }
    }
};



class CFLItem
{
public:
    NodeID _src;
    NodeID _dst;
    Label _type;

    //Constructor
    CFLItem(NodeID e1, NodeID e2, Label e3) :
            _src(e1), _dst(e2), _type(e3)
    {
    }


    //Destructor
    ~CFLItem()
    {}

    inline bool operator<(const CFLItem& rhs) const
    {
        if (_src < rhs._src)
            return true;
        else if (_src == rhs._src) {
            if (_dst < rhs._dst)
                return true;
            else if ((_dst == rhs._dst) && (_type < rhs._type))
                return true;
        }
        return false;
    }

    inline bool operator==(const CFLItem& rhs) const
    {
        return (_src == rhs._src) && (_dst == rhs._dst) && (_type == rhs._type);
    }

    NodeID src()
    {
        return _src;
    }

    NodeID dst()
    {
        return _dst;
    }

    const NodeID src() const
    {
        return _src;
    }

    const NodeID dst() const
    {
        return _dst;
    }

    Label type()
    {
        return _type;
    }
};
}
namespace std {
    template<>
    struct hash<SVF::CFLItem> {
        inline size_t operator()(const SVF::CFLItem& x) const {
            size_t s = 3;
            std::hash<SVF::u32_t> h;
            s ^= h(x._src) + 0x9e3779b9 + (s << 6) + (s >> 2);
            s ^= h(x._dst) + 0x9e3779b9 + (s << 6) + (s >> 2);
            s ^= h(SVF::u32_t(x._type)) + 0x9e3779b9 + (s << 6) + (s >> 2);
            return s;
        }
    };
}


namespace SVF
{



class CFLSolver
{

public:
    /// Define worklist
    typedef FIFOWorkList<const CFLEdge*> WorkList;
    typedef CFLGrammar::Production Production;
    typedef CFLGrammar::Symbol Symbol;

    static double numOfChecks;

    CFLSolver(CFLGraph* _graph, CFLGrammar* _grammar): graph(_graph), grammar(_grammar)
    {
    }

    virtual ~CFLSolver()
    {
        delete graph;
        delete grammar;
    }

    /// Initialize worklist
    virtual void initialize();

    /// Process CFLEdge
    virtual void processCFLEdge(const CFLEdge* Y_edge);

    /// Start solving
    virtual void solve();

    /// Return CFL Graph
    inline const CFLGraph* getGraph() const
    {
        return graph;
    }

    /// Return CFL Grammar
    inline const CFLGrammar* getGrammar() const
    {
        return grammar;
    }
    virtual inline bool pushIntoWorklist(const CFLEdge* item)
    {
        return worklist.push(item);
    }
    virtual inline bool isWorklistEmpty()
    {
        return worklist.empty();
    }

protected:
    /// Worklist operations
    //@{
    inline const CFLEdge* popFromWorklist()
    {
        return worklist.pop();
    }

    inline bool isInWorklist(const CFLEdge* item)
    {
        return worklist.find(item);
    }
    //@}

protected:
    CFLGraph* graph;
    CFLGrammar* grammar;
    /// Worklist for resolution
    WorkList worklist;

};

/// Solver Utilize CFLData
class POCRSolver : public CFLSolver
{
public:
    POCRSolver(CFLGraph* _graph, CFLGrammar* _grammar) : CFLSolver(_graph, _grammar), cflData(NULL)
    {
        if (!cflData)
        {
            cflData = new CFLData();
            // Build CFL Data
            buildCFLData();
        }
    }
    /// Destructor
    virtual ~POCRSolver()
    {
        delete cflData;
    }

    /// Init CFLData
    virtual void buildCFLData();

    virtual bool addEdge(const CFLNode* src, const CFLNode* dst, const Label ty)
    {
        return cflData->addEdge(src->getId(), dst->getId(), ty);
    }

    virtual bool addEdge(const NodeID srcId, const NodeID dstId, const Label ty)
    {
        return cflData->addEdge(srcId, dstId, ty);
    }

    /// Process CFLEdge
    virtual void processCFLItem(CFLItem item);
    
    virtual void initialize();

     // worklist operations
    //@{
    virtual inline CFLItem popFromWorklist()
    {
        return worklist.pop();
    }

    virtual inline bool pushIntoWorklist(CFLItem item)
    {
        return worklist.push(item);
    }

    virtual inline bool pushIntoWorklist(NodeID src, NodeID dst, Label ty)
    {
        return pushIntoWorklist(CFLItem(src, dst, ty));
    }

    virtual inline bool pushIntoWorklist(const CFLEdge* item)
    {
        return worklist.push(CFLItem(item->getSrcID(), item->getDstID(), item->getEdgeKind()));
    }

    virtual inline bool pushIntoWorklist(CFLNode* src, CFLNode* dst, Label ty)
    {
        return pushIntoWorklist(CFLItem(src->getId(), dst->getId(), ty));
    }

    virtual inline bool isInWorklist(CFLItem item)
    {
        return worklist.find(item);
    }

    virtual inline bool isInWorklist(NodeID src, NodeID dst, Label ty)
    {
        return isInWorklist(CFLItem(src, dst, ty));
    }

    virtual inline bool isWorklistEmpty()
    {
        return worklist.empty();
    }
    //@}
    
    /// Start solving
    virtual void solve();

    virtual NodeBS addEdges(const NodeID srcId, const NodeBS& dstData, const Label ty)
    {
        return cflData->addEdges(srcId, dstData, ty);
    }


    virtual NodeBS addEdges(const NodeBS& srcData, const NodeID dstId, const Label ty)
    {
        return cflData->addEdges(srcData, dstId, ty);
    }

    void rebuildCFLGraph()
    {
        for(auto iter : cflData->getSuccMap())
        {
            for( auto iter1 : iter.second)
            {
                for( auto iter2: iter1.second)
                    graph->addCFLEdge(graph->getGNode(iter.first), graph->getGNode(iter2), iter1.first);
            }
        }
    }

private:
    CFLData* cflData;
public:
    typedef FIFOWorkList<CFLItem> CFLItemWorkList;
    /// Worklist for resolution
    CFLItemWorkList worklist;

};
}

#endif /* INCLUDE_CFL_CFLSolver_H_*/