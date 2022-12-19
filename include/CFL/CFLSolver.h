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
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_CFL_CFLSolver_H_
#define INCLUDE_CFL_CFLSolver_H_

#include "Graphs/CFLGraph.h"
#include "CFL/CFLGrammar.h"
#include "Util/WorkList.h"
#include "CFL/CFLData.h"

using namespace std;

namespace SVF
{
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
    POCRSolver(CFLData* _cflData, CFLGraph* _graph, CFLGrammar* _grammar) : CFLSolver(_graph, _grammar), cflData(_cflData)
    {
    }
    /// Destructor
    virtual ~POCRSolver()
    {
    }

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