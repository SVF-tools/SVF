//===- WTO.h -- Weakest Topological Order Analysis-------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
 * WTO.h
 *
 *  Created on: Jul 3, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#ifndef WTO_H_
#define WTO_H_

#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SVFIR/SVFType.h"
#include "SVFIR/SVFValue.h"
#include "Graphs/CFBasicBlockG.h"

namespace SVF
{

class Wto;

class WtoVertex;

class WtoCycle;

/// \brief Weak topological order visitor
class WtoComponentVisitorG
{
public:
    typedef WtoVertex WtoVertexT;
    typedef WtoCycle WtoCycleT;

public:
    /// \brief Default constructor
    WtoComponentVisitorG() = default;

    /// \brief Copy constructor
    WtoComponentVisitorG(const WtoComponentVisitorG &) noexcept = default;

    /// \brief Move constructor
    WtoComponentVisitorG(WtoComponentVisitorG &&) noexcept = default;

    /// \brief Copy assignment operator
    WtoComponentVisitorG &operator=(const WtoComponentVisitorG &) noexcept = default;

    /// \brief Move assignment operator
    WtoComponentVisitorG &operator=(WtoComponentVisitorG &&) noexcept = default;

    /// \brief Visit the given vertex
    virtual void visit(const WtoVertexT &) = 0;

    /// \brief Visit the given cycle
    virtual void visit(const WtoCycleT &) = 0;

    /// \brief Destructor
    virtual ~WtoComponentVisitorG() = default;

}; // end class WtoComponentVisitorG





/// \brief Represents the nesting of a node
///
/// The nesting of a node is the list of cycles containing the node, from
/// the outermost to the innermost.
class WtoNestingG
{
public:
private:
    typedef std::vector<const CFBasicBlockNode*> NodeRefList;

public:
    typedef typename NodeRefList::const_iterator Iterator;

private:
    NodeRefList _nodes;

public:
    /// \brief Constructor
    WtoNestingG() = default;

    /// \brief Copy constructor
    WtoNestingG(const WtoNestingG &) = default;

    /// \brief Move constructor
    WtoNestingG(WtoNestingG &&) = default;

    /// \brief Copy assignment operator
    WtoNestingG &operator=(const WtoNestingG &) = default;

    /// \brief Move assignment operator
    WtoNestingG &operator=(WtoNestingG &&) = default;

    /// \brief Destructor
    ~WtoNestingG() = default;

    /// \brief Add a cycle head in the nesting
    void add(const CFBasicBlockNode *head)
    {
        _nodes.push_back(head);
    }

    /// \brief Begin iterator over the head of cycles
    Iterator begin() const
    {
        return _nodes.cbegin();
    }

    /// \brief End iterator over the head of cycles
    Iterator end() const
    {
        return _nodes.cend();
    }

    /// \brief Return the common prefix of the given nestings
    WtoNestingG operator^(const WtoNestingG &other) const
    {
        WtoNestingG res;
        for (auto this_it = begin(), other_it = other.begin();
             this_it != end() && other_it != other.end();
             ++this_it, ++other_it)
        {
            if (*this_it == *other_it)
            {
                res.add(*this_it);
            }
            else
            {
                break;
            }
        }
        return res;
    }

private:
    /// \brief Compare the given nestings
    int compare(const WtoNestingG &other) const
    {
        if (this == &other)
        {
            return 0; // equals
        }

        auto this_it = begin();
        auto other_it = other.begin();
        while (this_it != end())
        {
            if (other_it == other.end())
            {
                return 1; // `this` is nested within `other`
            }
            else if (*this_it == *other_it)
            {
                ++this_it;
                ++other_it;
            }
            else
            {
                return 2; // not comparable
            }
        }
        if (other_it == other.end())
        {
            return 0; // equals
        }
        else
        {
            return -1; // `other` is nested within `this`
        }
    }

public:
    bool operator<(const WtoNestingG &other) const
    {
        return compare(other) == -1;
    }

    bool operator<=(const WtoNestingG &other) const
    {
        return compare(other) <= 0;
    }

    bool operator==(const WtoNestingG &other) const
    {
        return compare(other) == 0;
    }

    bool operator>=(const WtoNestingG &other) const
    {
        return operator<=(other);
    }

    bool operator>(const WtoNestingG &other) const
    {
        return compare(other) == 1;
    }

    /// \brief Dump the nesting, for debugging purpose
    std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "[";
        for (auto it = begin(), et = end(); it != et;)
        {
            rawstr << (*it)->getName().data();
            ++it;
            if (it != et)
            {
                rawstr << ", ";
            }
        }
        rawstr << "]";
        return rawstr.str();
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend std::ostream &operator<<(std::ostream &o, const WtoNestingG &Wto)
    {
        o << Wto.toString();
        return o;
    }
    //@}

}; // end class WtoNestingG


/// \brief Base class for components of a weak topological order
///
/// This is either a vertex or a cycle.
class WtoComponent
{
public:
    /// Types of SVFIR statements
    /// Gep represents (base + offset) for field sensitivity
    /// ThreadFork/ThreadJoin is to model parameter passings between thread spawners and spawnees.
    enum WtoCT
    {
        Node, Cycle
    };

    /// \brief Default constructor
    WtoComponent(WtoCT k) : type(k) {};

    /// \brief Copy constructor
    WtoComponent(const WtoComponent &) noexcept = default;

    /// \brief Move constructor
    WtoComponent(WtoComponent &&) noexcept = default;

    /// \brief Copy assignment operator
    WtoComponent &operator=(const WtoComponent &) noexcept = default;

    /// \brief Move assignment operator
    WtoComponent &operator=(WtoComponent &&) noexcept = default;

    /// \brief Accept the given visitor
    virtual void accept(WtoComponentVisitorG *) const = 0;

    /// \brief Destructor
    virtual ~WtoComponent() = default;

    inline WtoCT getKind() const
    {
        return type;
    }

    virtual std::string toString() const = 0;

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend std::ostream &operator<<(std::ostream &o, const WtoComponent &Wto)
    {
        o << Wto.toString();
        return o;
    }
    //@}

    WtoCT type;
    WtoComponent* next{nullptr};
}; // end class WtoComponent


/// \brief Represents a vertex
class WtoVertex final : public WtoComponent
{
private:
    const CFBasicBlockNode *_node;

public:
    /// \brief Constructor
    explicit WtoVertex(const CFBasicBlockNode *node) : WtoComponent(WtoComponent::Node), _node(node) {}

    /// \brief Return the graph node
    const CFBasicBlockNode *node() const
    {
        return _node;
    }

    /// \brief Accept the given visitor
    void accept(WtoComponentVisitorG *v) const override
    {
        v->visit(*this);
    }

    /// \brief Dump the vertex, for debugging purpose
    std::string toString() const override
    {
        return std::string(_node->getName().data());
    }

    /// ClassOf
    //@{
    static inline bool classof(const WtoVertex *)
    {
        return true;
    }

    static inline bool classof(const WtoComponent *c)
    {
        return c->getKind() == WtoComponent::Node;
    }
    ///@}

}; // end class WtoVertex


/// \brief Represents a cycle
class WtoCycle final : public WtoComponent
{
public:
    typedef WtoComponent WtoComponentGT;

private:
    typedef const WtoComponentGT *WtoComponentPtr;
    typedef std::list<WtoComponentPtr> WtoComponentRefList;

public:
    /// \brief Iterator over the components
    typedef typename WtoComponentRefList::const_iterator Iterator;

private:
    /// \brief Head of the cycle
    const CFBasicBlockNode *_head;

    /// \brief List of components
    WtoComponentRefList _components;

public:
    /// \brief Constructor
    WtoCycle(const CFBasicBlockNode *head, WtoComponentRefList components)
            : WtoComponent(WtoComponent::Cycle), _head(head), _components(std::move(components)) {}

    /// \brief Return the head of the cycle
    const CFBasicBlockNode *head() const
    {
        return _head;
    }

    /// \brief Begin iterator over the components
    Iterator begin() const
    {
        return _components.cbegin();
    }

    /// \brief End iterator over the components
    Iterator end() const
    {
        return _components.cend();
    }

    /// \brief Accept the given visitor
    void accept(WtoComponentVisitorG *v) const override
    {
        v->visit(*this);
    }

    /// ClassOf
    //@{
    static inline bool classof(const WtoCycle *)
    {
        return true;
    }

    static inline bool classof(const WtoComponent *c)
    {
        return c->getKind() == WtoComponent::Cycle;
    }
    ///@}

    /// \brief Dump the cycle, for debugging purpose
    std::string toString() const override
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "(";
        rawstr << _head->getName().data() << ", ";
        for (auto it = begin(), et = end(); it != et;)
        {
            rawstr << (*it)->toString();
            ++it;
            if (it != et)
            {
                rawstr << ", ";
            }
        }
        rawstr << ")";
        return rawstr.str();
    }

}; // end class WtoCycle


/// \brief Weak Topological Ordering
class Wto
{

public:
    typedef WtoNestingG WtoNestingT;
    typedef WtoComponent WtoComponentT;
    typedef WtoVertex WtoVertexT;
    typedef WtoCycle WtoCycleT;
    typedef Set<const CFBasicBlockNode *> NodeRefList;

protected:
    typedef const WtoComponentT *WtoComponentPtr;
    typedef std::list<WtoComponentPtr> WtoComponentRefList;
    typedef Set <WtoComponentPtr> WtoComponentRefSet;
    typedef Map<const CFBasicBlockNode *, const WtoCycleT *> NodeRefToWtoCycleMap;
    typedef Map<const CFBasicBlockNode *, NodeRefList> NodeRefTONodeRefListMap;

    typedef u32_t Dfn;
    typedef Map<const CFBasicBlockNode *, Dfn> DfnTable;
    typedef std::vector<const CFBasicBlockNode *> Stack;
    typedef std::shared_ptr<WtoNestingT> WtoNestingPtr;
    typedef Map<const CFBasicBlockNode *, WtoNestingPtr> NestingTable;

public:
    /// \brief Iterator over the components
    typedef typename WtoComponentRefList::const_iterator Iterator;

protected:
    WtoComponentRefList _components;
    WtoComponentRefSet _allComponents;
    NodeRefToWtoCycleMap headRefToCycle;
    NodeRefTONodeRefListMap headRefToTails;
    NestingTable _nesting_table;
    DfnTable _dfn_table;
    Dfn _num;
    Stack _stack;

public:
    /// \brief Compute the weak topological order of the given graph
    explicit Wto() : _num(0) {}

    /// \brief Compute the weak topological order of the given graph
    explicit Wto(const CFBasicBlockNode *entry) : _num(0)
    {
        visit(entry, _components);
        //link_components(_components);
        _dfn_table.clear();
        _stack.clear();
        build_nesting();
        build_tails();
    }

    /// \brief No copy constructor
    Wto(const Wto &other) = default;

    /// \brief Move constructor
    Wto(Wto &&other) = default;

    /// \brief No copy assignment operator
    Wto &operator=(const Wto &other) = default;

    /// \brief Move assignment operator
    Wto &operator=(Wto &&other) = default;

    /// \brief Destructor
    ~Wto()
    {
        for (const auto &component: _allComponents)
        {
            delete component;
        }
    }

    /// \brief Begin iterator over the components
    /// \brief Begin iterator over the components
    Iterator begin() const
    {
        return _components.cbegin();
    }

    /// \brief End iterator over the components
    Iterator end() const
    {
        return _components.cend();
    }

    bool isHead(const CFBasicBlockNode *node) const
    {
        return headRefToCycle.find(node) != headRefToCycle.end();
    }

    typename NodeRefToWtoCycleMap::const_iterator headBegin() const
    {
        return headRefToCycle.cbegin();
    }

    /// \brief End iterator over the components
    typename NodeRefToWtoCycleMap::const_iterator headEnd() const
    {
        return headRefToCycle.cend();
    }

    const NodeRefList &getTails(const CFBasicBlockNode *node) const
    {
        auto it = headRefToTails.find(node);
        assert(it != headRefToTails.end() && "node not found");
        return it->second;
    }


    /// \brief Return the nesting of the given node
    const WtoNestingT &nesting(const CFBasicBlockNode *n) const
    {
        auto it = _nesting_table.find(n);
        assert(it != _nesting_table.end() && "node not found");
        return *(it->second);
    }

    /// \brief Return the nesting of the given node
    inline bool in_nesting_table(const CFBasicBlockNode *n) const
    {
        auto it = _nesting_table.find(n);
        return it != _nesting_table.end();
    }

    /// \brief Accept the given visitor
    void accept(WtoComponentVisitorG* v)
    {
        for (const auto &c: _components)
        {
            c->accept(v);
        }
    }

    /// \brief Dump the order, for debugging purpose
    std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "[";
        for (auto it = begin(), et = end(); it != et;)
        {
            rawstr << (*it)->toString();
            ++it;
            if (it != et)
            {
                rawstr << ", ";
            }
        }
        rawstr << "]";
        return rawstr.str();
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend std::ostream &operator<<(std::ostream &o, const Wto &Wto)
    {
        o << Wto.toString();
        return o;
    }
    //@}

protected:
    /// \brief Visitor to build the nestings of each node
    class NestingBuilder final
            : public WtoComponentVisitorG
    {
    private:
        WtoNestingPtr _nesting;
        NestingTable &_nesting_table;

    public:
        explicit NestingBuilder(NestingTable &nesting_table)
                : _nesting(std::make_shared<WtoNestingT>()),
                  _nesting_table(nesting_table) {}

        void visit(const WtoCycleT &cycle) override
        {
            const CFBasicBlockNode *head = cycle.head();
            WtoNestingPtr previous_nesting = _nesting;
            _nesting_table.insert(std::make_pair(head, _nesting));
            _nesting = std::make_shared<WtoNestingT>(*_nesting);
            _nesting->add(head);
            for (auto it = cycle.begin(), et = cycle.end(); it != et; ++it)
            {
                (*it)->accept(this);
            }
            _nesting = previous_nesting;
        }

        void visit(const WtoVertexT &vertex) override
        {
            _nesting_table.insert(
                    std::make_pair(vertex.node(), _nesting));
        }

    }; // end class NestingBuilder

    /// \brief Visitor to build the tails of each head/loop
    class TailBuilder : public WtoComponentVisitorG
    {
    protected:
        NodeRefList &_tails;
        const WtoNestingT &_headNesting;
        const CFBasicBlockNode *_head;
        NestingTable &_nesting_table;

    public:

        explicit TailBuilder(NestingTable &nesting_table, NodeRefList &tails, const CFBasicBlockNode *head,
                             const WtoNestingT &headNesting) : _nesting_table(
                nesting_table), _head(head), _tails(tails), _headNesting(headNesting)
        {
        }

        void visit(const WtoCycleT &cycle) override
        {
            for (auto it = cycle.begin(), et = cycle.end(); it != et; ++it)
            {
                (*it)->accept(this);
            }
        }

        virtual void visit(const WtoVertexT &vertex) override
        {
            for (const auto &edge: vertex.node()->getOutEdges())
            {
                const CFBasicBlockNode *succ = edge->getDstNode();
                const WtoNestingT &succNesting = nesting(succ);
                if (succ != _head && succNesting <= _headNesting)
                {
                    _tails.insert(vertex.node());
                }
            }
        }

    protected:
        /// \brief Return the nesting of the given node
        const WtoNestingT &nesting(const CFBasicBlockNode *n) const
        {
            auto it = _nesting_table.find(n);
            assert(it != _nesting_table.end() && "node not found");
            return *(it->second);
        }

    };

protected:
    /// \brief Return the depth-first number of the given node
    Dfn dfn(const CFBasicBlockNode *n) const
    {
        auto it = _dfn_table.find(n);
        if (it != _dfn_table.end())
        {
            return it->second;
        }
        else
        {
            return 0;
        }
    }

    /// \brief Set the depth-first number of the given node
    void set_dfn(const CFBasicBlockNode *n, const Dfn &dfn)
    {
        auto res = _dfn_table.insert(std::make_pair(n, dfn));
        if (!res.second)
        {
            (res.first)->second = dfn;
        }
    }

    /// \brief Pop a node from the stack
    const CFBasicBlockNode *pop()
    {
        assert(!_stack.empty() && "empty stack");
        const CFBasicBlockNode *top = _stack.back();
        _stack.pop_back();
        return top;
    }

    /// \brief Push a node on the stack
    void push(const CFBasicBlockNode *n)
    {
        _stack.push_back(n);
    }

    const WtoVertexT *newVertex(const CFBasicBlockNode *node)
    {
        const WtoVertexT *ptr = new WtoVertexT(node);
        _allComponents.insert(ptr);
        return ptr;
    }

    const WtoCycleT *newCycle(const CFBasicBlockNode *node, const WtoComponentRefList &partition)
    {
        const WtoCycleT *ptr = new WtoCycleT(node, std::move(partition));
        _allComponents.insert(ptr);
        return ptr;
    }

    /// \brief Create the cycle component for the given vertex
    const WtoCycleT *component(const CFBasicBlockNode *vertex)
    {
        WtoComponentRefList partition;
        for (auto it = vertex->getOutEdges().begin(), et = vertex->getOutEdges().end(); it != et; ++it)
        {
            const CFBasicBlockNode *succ = (*it)->getDstNode();
            if (dfn(succ) == 0)
            {
                visit(succ, partition);
            }
        }
        const WtoCycleT *ptr = newCycle(vertex, partition);
        headRefToCycle.emplace(vertex, ptr);
        return ptr;
    }

    /// \brief Visit the given node
    ///
    /// Algorithm to build a weak topological order of a graph
    virtual Dfn visit(const CFBasicBlockNode *vertex, WtoComponentRefList &partition)
    {
        Dfn head(0);
        Dfn min(0);
        bool loop;

        push(vertex);
        _num += Dfn(1);
        head = _num;
        set_dfn(vertex, head);
        loop = false;
        for (auto it = vertex->getOutEdges().begin(), et = vertex->getOutEdges().end(); it != et; ++it)
        {
            const CFBasicBlockNode *succ = (*it)->getDstNode();
            Dfn succ_dfn = dfn(succ);
            if (succ_dfn == Dfn(0))
            {
                min = visit(succ, partition);
            }
            else
            {
                min = succ_dfn;
            }
            if (min <= head)
            {
                head = min;
                loop = true;
            }
        }
        if (head == dfn(vertex))
        {
            set_dfn(vertex, UINT_MAX);
            const CFBasicBlockNode *element = pop();
            if (loop)
            {
                while (element != vertex)
                {
                    set_dfn(element, 0);
                    element = pop();
                }
                partition.push_front(component(vertex));
            }
            else
            {
                partition.push_front(newVertex(vertex));
            }
        }
        return head;
    }

    /// \brief Build the nesting table
    void build_nesting()
    {
        NestingBuilder builder(_nesting_table);
        for (auto it = begin(), et = end(); it != et; ++it)
        {
            (*it)->accept(&builder);
        }
    }

    /// \brief Build the tails for each loop
    virtual void build_tails()
    {
        for (const auto &head: headRefToCycle)
        {
            NodeRefList tails;
            TailBuilder builder(_nesting_table, tails, head.first, nesting(head.first));
            for (auto it = head.second->begin(), eit = head.second->end(); it != eit; ++it)
            {
                (*it)->accept(&builder);
            }
            headRefToTails.emplace(head.first, tails);
        }
    }

}; // end class Wto

}
#endif  /* WTO_H_ */