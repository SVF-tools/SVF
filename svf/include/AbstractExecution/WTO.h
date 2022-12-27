//===- WTO.h -- Weakest Topological Order Analysis-------------------------//
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
 * WTO.h
 *
 * The implementation is based on F. Bourdoncle's paper:
 * "Efficient chaotic iteration strategies with widenings", Formal
 * Methods in Programming and Their Applications, 1993, pages 128-141.
 *
 *  Created on: Jul 3, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#ifndef WTO_H_
#define WTO_H_

#include "SVFIR/SVFType.h"
#include "SVFIR/SVFValue.h"
#include "Graphs/CFBasicBlockG.h"


namespace SVF
{

class CFBasicBlockGWTO;

class CFBasicBlockGWTONode;

class CFBasicBlockGWTOCycle;

/*!
 * Cycle depth of a WTO Component
 *
 * The cycle depth is represented as **a list of cycle's heads**,
 * **from the outermost to the innermost**.
 *
 * e.g., consider the following nested cycle:
 *
 * -->1 --> 2 --> 3 --> 4
 *    \                /
 *     <-- 6 <-- 5 <--
 *         \    /
 *          >7>
 *
 *  where C1: (1 2 3 4 5 6 7) is the outer cycle, where 1 is the head,
 *  and C2: (5 6 7) is the inner cycle, where 5 is the head.
 *
 *  The cycle depth is as following:
 *  ---------------------------------
 *  |     Node NO.    | Cycle Depth |
 *  --------------------------------
 *  | 1 (head of C1)  |     [ ]     |
 *  --------------------------------
 *  |     2, 3, 4     |    [1]      |
 *  --------------------------------
 *  | 5 (head of C2)  |    [1]      |
 *  --------------------------------
 *  |       6, 7      |    [1, 5]   |
 *  --------------------------------
 */
template <typename NodeRef>
class WTOCycleDepth
{
public:
    typedef std::vector<NodeRef> NodeRefList;
    typedef typename NodeRefList::const_iterator Iterator;

private:
    NodeRefList _heads;

public:
    /// Default Constructor
    WTOCycleDepth() = default;

    /// Default Copy Constructor
    WTOCycleDepth(const WTOCycleDepth &) = default;

    /// Default Move Constructor
    WTOCycleDepth(WTOCycleDepth &&) = default;

    /// Default Copy Operator=
    WTOCycleDepth &operator=(const WTOCycleDepth &) = default;

    /// Default Move Operator=
    WTOCycleDepth &operator=(WTOCycleDepth &&) = default;

    /// Default Destructor
    ~WTOCycleDepth() = default;

    /// Add a cycle head to the end of the head list
    void push_back(NodeRef head)
    {
        _heads.push_back(head);
    }

    Iterator begin() const
    {
        return _heads.cbegin();
    }

    Iterator end() const
    {
        return _heads.cend();
    }

    /// Convert the wto-cycle-depth to a string
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

private:
    /// Compare the given wto-cycle-depth
    int compare(const WTOCycleDepth &other) const
    {
        if (this == &other)
        {
            return 0; // 0 - equals
        }

        auto this_it = begin();
        auto other_it = other.begin();
        while (this_it != end())
        {
            if (other_it == other.end())
            {
                return 1; // `this` is inside `other`
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
            return -1; // `other` is inside `this`
        }
    }

public:

    /// Overloading operators
    //@{
    /// Return the common prefix of the given wto-cycle-depth
    WTOCycleDepth operator^(const WTOCycleDepth &other) const
    {
        WTOCycleDepth res;
        for (auto this_it = begin(), other_it = other.begin();
                this_it != end() && other_it != other.end();
                ++this_it, ++other_it)
        {
            if (*this_it == *other_it)
            {
                res.push_back(*this_it);
            }
            else
            {
                break;
            }
        }
        return res;
    }

    /// Less than other's depth - `other` is inside `this`
    bool operator<(const WTOCycleDepth &other) const
    {
        return compare(other) == -1;
    }

    /// Less than or Equal with other's depth
    bool operator<=(const WTOCycleDepth &other) const
    {
        return compare(other) <= 0;
    }

    /// Equal with other's depth
    bool operator==(const WTOCycleDepth &other) const
    {
        return compare(other) == 0;
    }

    /// Greater than other's depth - `this` is inside `other`
    bool operator>(const WTOCycleDepth &other) const
    {
        return compare(other) == 1;
    }

    /// Greater than or Equal with other's depth
    bool operator>=(const WTOCycleDepth &other) const
    {
        return operator<=(other);
    }

    /// Dump into CMD
    friend std::ostream &operator<<(std::ostream &o, const WTOCycleDepth &CFBasicBlockGWTO)
    {
        o << CFBasicBlockGWTO.toString();
        return o;
    }
    //@}

}; // end class WTOCycleDepth

/*!
 * Weak topological order (WTO) visitor
 */
class WTOVisitor
{

public:
    /// Default Constructor
    WTOVisitor() = default;

    /// Default Copy Constructor
    WTOVisitor(const WTOVisitor &) noexcept = default;

    /// Default Move Constructor
    WTOVisitor(WTOVisitor &&) noexcept = default;

    /// Default Copy Operator=
    WTOVisitor &operator=(const WTOVisitor &) noexcept = default;

    /// Default Move Operator=
    WTOVisitor &operator=(WTOVisitor &&) noexcept = default;

    /// Default Destructor
    virtual ~WTOVisitor() = default;

    /// Visit WTO Node
    virtual void visit(const CFBasicBlockGWTONode &) = 0;

    /// Visit WTO Cycle
    virtual void visit(const CFBasicBlockGWTOCycle &) = 0;

}; // end class WTOVisitor

/*!
 * Base class for a WTO component for CFBasicBlockG
 *
 * A WTO component can be either a node or cycle
 */
class CFBasicBlockGWTOComp
{

public:
    enum WtoCT
    {
        Node, Cycle
    };

public:
    /// Default Constructor
    CFBasicBlockGWTOComp(WtoCT k) : _type(k) {};

    /// Copy Constructor
    CFBasicBlockGWTOComp(const CFBasicBlockGWTOComp &) noexcept = default;

    /// Move Constructor
    CFBasicBlockGWTOComp(CFBasicBlockGWTOComp &&) noexcept = default;

    /// Copy Operator=
    CFBasicBlockGWTOComp &operator=(const CFBasicBlockGWTOComp &) noexcept = default;

    /// Move Operator=
    CFBasicBlockGWTOComp &operator=(CFBasicBlockGWTOComp &&) noexcept = default;

    /// Default Destructor
    virtual ~CFBasicBlockGWTOComp() = default;

    /// Accept a visitor
    virtual void accept(WTOVisitor *) const = 0;

    /// Return the WTO Kind (node or cycle)
    inline WtoCT getKind() const
    {
        return _type;
    }

    virtual std::string toString() const = 0;

    friend std::ostream &operator<<(std::ostream &o, const CFBasicBlockGWTOComp &CFBasicBlockGWTO)
    {
        o << CFBasicBlockGWTO.toString();
        return o;
    }

private:

    WtoCT _type;

}; // end class CFBasicBlockGWTOComp


/*!
 * WTO node for CFBasicBlockG
 */
class CFBasicBlockGWTONode final : public CFBasicBlockGWTOComp
{
private:
    const CFBasicBlockNode *_node;

public:
    /// Constructor
    explicit CFBasicBlockGWTONode(const CFBasicBlockNode *node) : CFBasicBlockGWTOComp(CFBasicBlockGWTOComp::Node), _node(node) {}

    /// Return the graph node
    const CFBasicBlockNode *node() const
    {
        return _node;
    }

    /// Accept a visitor
    void accept(WTOVisitor *v) const override
    {
        v->visit(*this);
    }

    /// Convert the node to string
    std::string toString() const override
    {
        return std::string(_node->getName().data());
    }

    /// ClassOf
    //@{
    static inline bool classof(const CFBasicBlockGWTONode *)
    {
        return true;
    }

    static inline bool classof(const CFBasicBlockGWTOComp *c)
    {
        return c->getKind() == CFBasicBlockGWTOComp::Node;
    }
    ///@}

}; // end class CFBasicBlockGWTONode


/*!
 * WTO cycle for CFBasicBlockG
 */
class CFBasicBlockGWTOCycle final : public CFBasicBlockGWTOComp
{
private:
    typedef std::list<const CFBasicBlockGWTOComp *> WtoComponentRefList;
    typedef WtoComponentRefList::const_iterator Iterator;

private:
    /// Head of the cycle
    const CFBasicBlockNode *_head;

    /// List of components
    WtoComponentRefList _components;

public:
    /// Constructor
    CFBasicBlockGWTOCycle(const CFBasicBlockNode *head, WtoComponentRefList components)
        : CFBasicBlockGWTOComp(CFBasicBlockGWTOComp::Cycle), _head(head), _components(std::move(components)) {}

    /// Return the head of the cycle
    const CFBasicBlockNode *head() const
    {
        return _head;
    }

    Iterator begin() const
    {
        return _components.cbegin();
    }

    Iterator end() const
    {
        return _components.cend();
    }

    /// Accept a visitor
    void accept(WTOVisitor *v) const override
    {
        v->visit(*this);
    }

    /// ClassOf
    //@{
    static inline bool classof(const CFBasicBlockGWTOCycle *)
    {
        return true;
    }

    static inline bool classof(const CFBasicBlockGWTOComp *c)
    {
        return c->getKind() == CFBasicBlockGWTOComp::Cycle;
    }
    ///@}

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

}; // end class CFBasicBlockGWTOCycle


/*!
 * Weak topological order for CFBasicBlockG
 */
class CFBasicBlockGWTO
{

public:
    typedef WTOCycleDepth<const CFBasicBlockNode*> CFBasicBlockGWTOCycleDepth;
    typedef Set<const CFBasicBlockNode *> NodeRefSet;

protected:
    typedef std::list<const CFBasicBlockGWTOComp *> WTOCompRefList;
    typedef Set <const CFBasicBlockGWTOComp *> WTOCompRefSet;
    typedef Map<const CFBasicBlockNode *, const CFBasicBlockGWTOCycle *> NodeRefToWTOCycleMap;
    typedef Map<const CFBasicBlockNode *, NodeRefSet> NodeRefToNodeRefSetMap;

    typedef u32_t CycleDepthNumber;
    typedef Map<const CFBasicBlockNode *, CycleDepthNumber> NodeRefToCycleDepthNumber;
    typedef std::vector<const CFBasicBlockNode *> CFBasicBlockNodes;
    typedef std::shared_ptr<CFBasicBlockGWTOCycleDepth> CFBasicBlockGWTOCycleDepthPtr;
    typedef Map<const CFBasicBlockNode *, CFBasicBlockGWTOCycleDepthPtr> NodeRefToWTOCycleDepthPtr;

public:
    typedef typename WTOCompRefList::const_iterator Iterator;

protected:
    WTOCompRefList _components;
    WTOCompRefSet _allComponents;
    NodeRefToWTOCycleMap _headToCycle;
    NodeRefToNodeRefSetMap _headToTails;
    NodeRefToWTOCycleDepthPtr _nodeToDepth;
    NodeRefToCycleDepthNumber _nodeToCDN;
    CycleDepthNumber _num;
    CFBasicBlockNodes _stack;

public:
    explicit CFBasicBlockGWTO() : _num(0) {}

    explicit CFBasicBlockGWTO(const CFBasicBlockNode *entry) : _num(0)
    {
        build(entry);
    }

    CFBasicBlockGWTO(const CFBasicBlockGWTO &other) = default;

    CFBasicBlockGWTO(CFBasicBlockGWTO &&other) = default;

    CFBasicBlockGWTO &operator=(const CFBasicBlockGWTO &other) = default;

    CFBasicBlockGWTO &operator=(CFBasicBlockGWTO &&other) = default;

    ~CFBasicBlockGWTO()
    {
        for (const auto &component: _allComponents)
        {
            delete component;
        }
    }

    Iterator begin() const
    {
        return _components.cbegin();
    }

    Iterator end() const
    {
        return _components.cend();
    }

    bool isHead(const CFBasicBlockNode *node) const
    {
        return _headToCycle.find(node) != _headToCycle.end();
    }

    typename NodeRefToWTOCycleMap::const_iterator headBegin() const
    {
        return _headToCycle.cbegin();
    }

    typename NodeRefToWTOCycleMap::const_iterator headEnd() const
    {
        return _headToCycle.cend();
    }

    const NodeRefSet &getTails(const CFBasicBlockNode *node) const
    {
        auto it = _headToTails.find(node);
        assert(it != _headToTails.end() && "node not found");
        return it->second;
    }


    /// Get the wto-cycle-depth of the given node
    const CFBasicBlockGWTOCycleDepth &getWTOCycleDepth(const CFBasicBlockNode *n) const
    {
        auto it = _nodeToDepth.find(n);
        assert(it != _nodeToDepth.end() && "node not found");
        return *(it->second);
    }

    /// Whether the given node is in the node to cycle getCDN
    inline bool inNodeToCycleDepth(const CFBasicBlockNode *n) const
    {
        auto it = _nodeToDepth.find(n);
        return it != _nodeToDepth.end();
    }

    /// Accept the given visitor
    void accept(WTOVisitor* v)
    {
        for (const auto &c: _components)
        {
            c->accept(v);
        }
    }

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

    friend std::ostream &operator<<(std::ostream &o, const CFBasicBlockGWTO &CFBasicBlockGWTO)
    {
        o << CFBasicBlockGWTO.toString();
        return o;
    }

protected:

    inline void build(const CFBasicBlockNode *entry)
    {
        visit(entry, _components);
        _nodeToCDN.clear();
        _stack.clear();
        buildNodeToWTOCycleDepth();
        build_tails();
    }

    /// Visitor to build the WTO cycle getCDN of each node
    class WTOCycleDepthBuilder final
        : public WTOVisitor
    {
    private:
        CFBasicBlockGWTOCycleDepthPtr _wtoCycleDepth;
        NodeRefToWTOCycleDepthPtr &_nodeToWTOCycleDepth;

    public:
        explicit WTOCycleDepthBuilder(NodeRefToWTOCycleDepthPtr &nodeToWTOCycleDepth)
            : _wtoCycleDepth(std::make_shared<CFBasicBlockGWTOCycleDepth>()),
              _nodeToWTOCycleDepth(nodeToWTOCycleDepth) {}

        void visit(const CFBasicBlockGWTOCycle &cycle) override
        {
            const CFBasicBlockNode *head = cycle.head();
            CFBasicBlockGWTOCycleDepthPtr previous_nesting = _wtoCycleDepth;
            _nodeToWTOCycleDepth.insert(std::make_pair(head, _wtoCycleDepth));
            _wtoCycleDepth = std::make_shared<CFBasicBlockGWTOCycleDepth>(*_wtoCycleDepth);
            _wtoCycleDepth->push_back(head);
            for (auto it = cycle.begin(), et = cycle.end(); it != et; ++it)
            {
                (*it)->accept(this);
            }
            _wtoCycleDepth = previous_nesting;
        }

        void visit(const CFBasicBlockGWTONode &node) override
        {
            _nodeToWTOCycleDepth.insert(
                std::make_pair(node.node(), _wtoCycleDepth));
        }

    }; // end class WTOCycleDepthBuilder

    /// Visitor to build the tails of each head/loop
    class TailBuilder : public WTOVisitor
    {
    protected:
        NodeRefSet &_tails;
        const CFBasicBlockGWTOCycleDepth &_headWTOCycleDepth;
        const CFBasicBlockNode *_head;
        NodeRefToWTOCycleDepthPtr &_nodeToWTOCycleDepth;

    public:

        explicit TailBuilder(NodeRefToWTOCycleDepthPtr &nodeToWTOCycleDepth, NodeRefSet &tails, const CFBasicBlockNode *head,
                             const CFBasicBlockGWTOCycleDepth &headWTOCycleDepth) : _tails(tails), _headWTOCycleDepth(headWTOCycleDepth), _head(head), _nodeToWTOCycleDepth(
                                     nodeToWTOCycleDepth)
        {
        }

        void visit(const CFBasicBlockGWTOCycle &cycle) override
        {
            for (auto it = cycle.begin(), et = cycle.end(); it != et; ++it)
            {
                (*it)->accept(this);
            }
        }

        virtual void visit(const CFBasicBlockGWTONode &node) override
        {
            for (const auto &edge: node.node()->getOutEdges())
            {
                const CFBasicBlockNode *succ = edge->getDstNode();
                const CFBasicBlockGWTOCycleDepth &succNesting = getWTOCycleDepth(succ);
                if (succ != _head && succNesting <= _headWTOCycleDepth)
                {
                    _tails.insert(node.node());
                }
            }
        }

    protected:
        /// Get the wto-cycle-depth of the given node
        const CFBasicBlockGWTOCycleDepth &getWTOCycleDepth(const CFBasicBlockNode *n) const
        {
            auto it = _nodeToWTOCycleDepth.find(n);
            assert(it != _nodeToWTOCycleDepth.end() && "node not found");
            return *(it->second);
        }

    };

protected:
    /// Get the cycle depth number of the given node
    CycleDepthNumber getCDN(const CFBasicBlockNode *n) const
    {
        auto it = _nodeToCDN.find(n);
        if (it != _nodeToCDN.end())
        {
            return it->second;
        }
        else
        {
            return 0;
        }
    }

    /// Set the cycle depth number of the given node
    void setCDN(const CFBasicBlockNode *n, const CycleDepthNumber &Depth)
    {
        auto res = _nodeToCDN.insert(std::make_pair(n, Depth));
        if (!res.second)
        {
            (res.first)->second = Depth;
        }
    }

    /// Pop a node from the stack
    const CFBasicBlockNode *pop()
    {
        assert(!_stack.empty() && "empty stack");
        const CFBasicBlockNode *top = _stack.back();
        _stack.pop_back();
        return top;
    }

    /// Push a node on the stack
    void push(const CFBasicBlockNode *n)
    {
        _stack.push_back(n);
    }

    const CFBasicBlockGWTONode *newNode(const CFBasicBlockNode *node)
    {
        const CFBasicBlockGWTONode *ptr = new CFBasicBlockGWTONode(node);
        _allComponents.insert(ptr);
        return ptr;
    }

    const CFBasicBlockGWTOCycle *newCycle(const CFBasicBlockNode *node, const WTOCompRefList &partition)
    {
        const CFBasicBlockGWTOCycle *ptr = new CFBasicBlockGWTOCycle(node, std::move(partition));
        _allComponents.insert(ptr);
        return ptr;
    }

    /// Create the cycle component for the given node
    const CFBasicBlockGWTOCycle *component(const CFBasicBlockNode *node)
    {
        WTOCompRefList partition;
        for (auto it = node->getOutEdges().begin(), et = node->getOutEdges().end(); it != et; ++it)
        {
            const CFBasicBlockNode *succ = (*it)->getDstNode();
            if (getCDN(succ) == 0)
            {
                visit(succ, partition);
            }
        }
        const CFBasicBlockGWTOCycle *ptr = newCycle(node, partition);
        _headToCycle.emplace(node, ptr);
        return ptr;
    }

    /// Main algorithm to build WTO
    virtual CycleDepthNumber visit(const CFBasicBlockNode *node, WTOCompRefList &partition)
    {
        CycleDepthNumber head(0);
        CycleDepthNumber min(0);
        bool loop;

        push(node);
        _num += CycleDepthNumber(1);
        head = _num;
        setCDN(node, head);
        loop = false;
        for (auto it = node->getOutEdges().begin(), et = node->getOutEdges().end(); it != et; ++it)
        {
            const CFBasicBlockNode *succ = (*it)->getDstNode();
            CycleDepthNumber succ_dfn = getCDN(succ);
            if (succ_dfn == CycleDepthNumber(0))
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
        if (head == getCDN(node))
        {
            setCDN(node, UINT_MAX);
            const CFBasicBlockNode *element = pop();
            if (loop)
            {
                while (element != node)
                {
                    setCDN(element, 0);
                    element = pop();
                }
                partition.push_front(component(node));
            }
            else
            {
                partition.push_front(newNode(node));
            }
        }
        return head;
    }

    /// Build the node to WTO cycle depth table
    void buildNodeToWTOCycleDepth()
    {
        WTOCycleDepthBuilder builder(_nodeToDepth);
        for (auto it = begin(), et = end(); it != et; ++it)
        {
            (*it)->accept(&builder);
        }
    }

    /// Build the tails for each cycle
    virtual void build_tails()
    {
        for (const auto &head: _headToCycle)
        {
            NodeRefSet tails;
            TailBuilder builder(_nodeToDepth, tails, head.first, getWTOCycleDepth(head.first));
            for (auto it = head.second->begin(), eit = head.second->end(); it != eit; ++it)
            {
                (*it)->accept(&builder);
            }
            _headToTails.emplace(head.first, tails);
        }
    }

}; // end class CFBasicBlockGWTO

} // end namespace SVF
#endif /* WTO_H_ */
