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
 *  Created on: Jan 22, 2024
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */
#ifndef WTO_H_
#define WTO_H_

#include "SVFIR/SVFType.h"
#include "SVFIR/SVFValue.h"
#include <functional>

namespace SVF
{

template <typename GraphT> class WTO;

template <typename GraphT> class WTONode;

template <typename GraphT> class WTOCycle;

template <typename GraphT> class WTOComponentVisitor;

// Helper to test for the existence of a sub type
template <typename T, typename = void> struct has_nodetype : std::false_type
{
};

template <typename T>
struct has_nodetype<T, std::void_t<typename T::NodeType>> : std::true_type
{
};

template <typename T, typename = void> struct has_edgetype : std::false_type
{
};

template <typename T>
struct has_edgetype<T, std::void_t<typename T::EdgeType>> : std::true_type
{
};

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
template <typename GraphT> class WTOCycleDepth
{
public:
    static_assert(has_nodetype<GraphT>::value,
                  "GraphT must have a nested type named 'NodeType'");
    typedef typename GraphT::NodeType NodeT;

private:
    typedef std::vector<const NodeT*> NodeRefList;

public:
    typedef typename NodeRefList::const_iterator Iterator;

private:
    NodeRefList _heads;

public:
    /// Constructor
    WTOCycleDepth() = default;

    /// Copy constructor
    WTOCycleDepth(const WTOCycleDepth&) = default;

    /// Move constructor
    WTOCycleDepth(WTOCycleDepth&&) = default;

    /// Copy assignment operator
    WTOCycleDepth& operator=(const WTOCycleDepth&) = default;

    /// Move assignment operator
    WTOCycleDepth& operator=(WTOCycleDepth&&) = default;

    /// Destructor
    ~WTOCycleDepth() = default;

    /// Add a cycle head in the cycleDepth
    void add(const NodeT* head)
    {
        _heads.push_back(head);
    }

    /// Begin iterator over the head of cycles
    Iterator begin() const
    {
        return _heads.cbegin();
    }

    /// End iterator over the head of cycles
    Iterator end() const
    {
        return _heads.cend();
    }

    /// Return the common prefix of the given cycle depths
    WTOCycleDepth operator^(const WTOCycleDepth& other) const
    {
        WTOCycleDepth res;
        for (auto this_it = begin(), other_it = other.begin();
                this_it != end() && other_it != other.end(); ++this_it, ++other_it)
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
    /// Compare the given cycle depths
    int compare(const WTOCycleDepth& other) const
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
    bool operator<(const WTOCycleDepth& other) const
    {
        return compare(other) == -1;
    }

    bool operator<=(const WTOCycleDepth& other) const
    {
        return compare(other) <= 0;
    }

    bool operator==(const WTOCycleDepth& other) const
    {
        return compare(other) == 0;
    }

    bool operator>=(const WTOCycleDepth& other) const
    {
        return operator<=(other, *this);
    }

    bool operator>(const WTOCycleDepth& other) const
    {
        return compare(other) == 1;
    }

    /// Dump the cycleDepth, for debugging purpose
    [[nodiscard]] std::string toString() const
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
    friend std::ostream& operator<<(std::ostream& o,
                                    const WTOCycleDepth<GraphT>& wto)
    {
        o << wto.toString();
        return o;
    }
    //@}

}; // end class WTOCycleDepth

/*!
 * Base class for a WTO component
 *
 * A WTO component can be either a node or cycle
 */
template <typename GraphT> class WTOComponent
{
public:
    enum WTOCT
    {
        Node,
        Cycle
    };

    /// Default constructor
    explicit WTOComponent(WTOCT k) : _type(k) {};

    /// Copy constructor
    WTOComponent(const WTOComponent&) noexcept = default;

    /// Move constructor
    WTOComponent(WTOComponent&&) noexcept = default;

    /// Copy assignment operator
    WTOComponent& operator=(const WTOComponent&) noexcept = default;

    /// Move assignment operator
    WTOComponent& operator=(WTOComponent&&) noexcept = default;

    /// Accept the given visitor
    virtual void accept(WTOComponentVisitor<GraphT>&) const = 0;

    /// Destructor
    virtual ~WTOComponent() = default;

    inline WTOCT getKind() const
    {
        return _type;
    }

    [[nodiscard]] virtual std::string toString() const = 0;

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend std::ostream& operator<<(std::ostream& o,
                                    const WTOComponent<GraphT>& wto)
    {
        o << wto.toString();
        return o;
    }
    //@}

    WTOCT _type;

}; // end class WTOComponent

/*!
 * WTO node for GraphT
 */
template <typename GraphT> class WTONode final : public WTOComponent<GraphT>
{
public:
    static_assert(has_nodetype<GraphT>::value,
                  "GraphT must have a nested type named 'NodeType'");
    typedef typename GraphT::NodeType NodeT;

private:
    const NodeT* _node;

public:
    /// Constructor
    explicit WTONode(const NodeT* node)
        : WTOComponent<GraphT>(WTOComponent<GraphT>::Node), _node(node)
    {
    }

    /// Return the graph node
    const NodeT* getICFGNode() const
    {
        return _node;
    }

    /// Accept the given visitor
    void accept(WTOComponentVisitor<GraphT>& v) const override
    {
        v.visit(*this);
    }

    /// Dump the node, for debugging purpose
    [[nodiscard]] std::string toString() const override
    {
        // return _node->toString();
        return std::to_string(_node->getId());
    }

    /// ClassOf
    //@{
    static inline bool classof(const WTONode<GraphT>*)
    {
        return true;
    }

    static inline bool classof(const WTOComponent<GraphT>* c)
    {
        return c->getKind() == WTOComponent<GraphT>::Node;
    }
    ///@}

}; // end class WTONode

/*!
 * WTO cycle for GraphT
 */
template <typename GraphT> class WTOCycle final : public WTOComponent<GraphT>
{
public:
    static_assert(has_nodetype<GraphT>::value,
                  "GraphT must have a nested type named 'NodeType'");
    typedef typename GraphT::NodeType NodeT;
    typedef WTOComponent<GraphT> WTOComponentT;

private:
    typedef const WTOComponentT* WTOComponentPtr;
    typedef std::list<WTOComponentPtr> WTOComponentRefList;

public:
    /// Iterator over the components
    typedef typename WTOComponentRefList::const_iterator Iterator;

private:
    /// Head of the cycle
    const WTONode<GraphT>* _head;

    /// List of components
    WTOComponentRefList _components;

public:
    /// Constructor
    WTOCycle(const WTONode<GraphT>* head, WTOComponentRefList components)
        : WTOComponent<GraphT>(WTOComponent<GraphT>::Cycle), _head(head),
          _components(std::move(components))
    {
    }

    /// Return the head of the cycle
    const WTONode<GraphT>* head() const
    {
        return _head;
    }

    /// Get all wto components in WTO cycle
    const WTOComponentRefList& getWTOComponents() const
    {
        return _components;
    }

    /// Begin iterator over the components
    Iterator begin() const
    {
        return _components.cbegin();
    }

    /// End iterator over the components
    Iterator end() const
    {
        return _components.cend();
    }

    /// Accept the given visitor
    void accept(WTOComponentVisitor<GraphT>& v) const override
    {
        v.visit(*this);
    }

    /// ClassOf
    //@{
    static inline bool classof(const WTOCycle<GraphT>*)
    {
        return true;
    }

    static inline bool classof(const WTOComponent<GraphT>* c)
    {
        return c->getKind() == WTOComponent<GraphT>::Cycle;
    }
    ///@}

    /// Dump the cycle, for debugging purpose
    [[nodiscard]] std::string toString() const override
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "(";
        rawstr << _head->getICFGNode()->getId() << ", ";
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

}; // end class WTOCycle

/*!
 * Weak topological order (WTO) visitor
 */
template <typename GraphT> class WTOComponentVisitor
{
public:
    typedef WTONode<GraphT> WTONodeT;
    typedef WTOCycle<GraphT> WTOCycleT;

public:
    /// Default constructor
    WTOComponentVisitor() = default;

    /// Copy constructor
    WTOComponentVisitor(const WTOComponentVisitor&) noexcept = default;

    /// Move constructor
    WTOComponentVisitor(WTOComponentVisitor&&) noexcept = default;

    /// Copy assignment operator
    WTOComponentVisitor& operator=(const WTOComponentVisitor&) noexcept =
        default;

    /// Move assignment operator
    WTOComponentVisitor& operator=(WTOComponentVisitor&&) noexcept = default;

    /// Visit the given node
    virtual void visit(const WTONodeT&) = 0;

    /// Visit the given cycle
    virtual void visit(const WTOCycleT&) = 0;

    /// Destructor
    virtual ~WTOComponentVisitor() = default;

}; // end class WTOComponentVisitor

/*!
 * Weak topological order for GraphT
 */
template <typename GraphT> class WTO
{

public:
    static_assert(has_nodetype<GraphT>::value,
                  "GraphT must have a nested type named 'NodeType'");
    static_assert(has_edgetype<GraphT>::value,
                  "GraphT must have a nested type named 'EdgeType'");
    typedef typename GraphT::NodeType NodeT;
    typedef typename GraphT::EdgeType EdgeT;
    typedef WTOCycleDepth<GraphT> GraphTWTOCycleDepth;
    typedef WTOComponent<GraphT> WTOComponentT;
    typedef WTONode<GraphT> WTONodeT;
    typedef WTOCycle<GraphT> WTOCycleT;
    typedef Set<const NodeT*> NodeRefList;

protected:
    typedef const WTOComponentT* WTOComponentPtr;
    typedef std::list<WTOComponentPtr> WTOComponentRefList;
    typedef Set<WTOComponentPtr> WTOComponentRefSet;
    typedef Map<const NodeT*, const WTOCycleT*> NodeRefToWTOCycleMap;
    typedef Map<const NodeT*, NodeRefList> NodeRefTONodeRefListMap;

    typedef u32_t CycleDepthNumber;
    typedef Map<const NodeT*, CycleDepthNumber> NodeRefToCycleDepthNumber;
    typedef std::vector<const NodeT*> Stack;
    typedef std::shared_ptr<GraphTWTOCycleDepth> WTOCycleDepthPtr;
    typedef Map<const NodeT*, WTOCycleDepthPtr> NodeRefToWTOCycleDepthPtr;

public:
    /// Iterator over the components
    typedef typename WTOComponentRefList::const_iterator Iterator;

protected:
    WTOComponentRefList _components;
    WTOComponentRefSet _allComponents;
    NodeRefToWTOCycleMap headRefToCycle;
    NodeRefToWTOCycleDepthPtr _nodeToDepth;
    NodeRefToCycleDepthNumber _nodeToCDN;
    CycleDepthNumber _num;
    Stack _stack;
    GraphT* _graph;
    const NodeT* _entry;

public:

    /// Compute the weak topological order of the given graph
    explicit WTO(GraphT* graph, const NodeT* entry) : _num(0), _graph(graph), _entry(entry)
    {
    }

    /// No copy constructor
    WTO(const WTO& other) = default;

    /// Move constructor
    WTO(WTO&& other) = default;

    /// No copy assignment operator
    WTO& operator=(const WTO& other) = default;

    /// Move assignment operator
    WTO& operator=(WTO&& other) = default;

    /// Destructor
    ~WTO()
    {
        for (const auto& component : _allComponents)
        {
            delete component;
        }
    }

    /// Get all wto components in WTO
    const WTOComponentRefList& getWTOComponents() const
    {
        return _components;
    }

    /// Begin iterator over the components
    Iterator begin() const
    {
        return _components.cbegin();
    }

    /// End iterator over the components
    Iterator end() const
    {
        return _components.cend();
    }

    bool isHead(const NodeT* node) const
    {
        return headRefToCycle.find(node) != headRefToCycle.end();
    }

    typename NodeRefToWTOCycleMap::const_iterator headBegin() const
    {
        return headRefToCycle.cbegin();
    }

    /// End iterator over the components
    typename NodeRefToWTOCycleMap::const_iterator headEnd() const
    {
        return headRefToCycle.cend();
    }

    /// Return the cycleDepth of the given node
    const GraphTWTOCycleDepth& cycleDepth(const NodeT* n) const
    {
        auto it = _nodeToDepth.find(n);
        assert(it != _nodeToDepth.end() && "node not found");
        return *(it->second);
    }

    /// Return the cycleDepth of the given node
    inline bool in_cycleDepth_table(const NodeT* n) const
    {
        auto it = _nodeToDepth.find(n);
        return it != _nodeToDepth.end();
    }

    /// Accept the given visitor
    void accept(WTOComponentVisitor<GraphT>& v)
    {
        for (const auto& c : _components)
        {
            c->accept(v);
        }
    }

    /// Dump the order, for debugging purpose
    [[nodiscard]] std::string toString() const
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
    friend std::ostream& operator<<(std::ostream& o, const WTO<GraphT>& wto)
    {
        o << wto.toString();
        return o;
    }
    //@}

    void init()
    {
        visit(_entry, _components);
        _nodeToCDN.clear();
        _stack.clear();
        buildNodeToDepth();
    }

protected:

    /// Visitor to build the cycle depths of each node
    class WTOCycleDepthBuilder final : public WTOComponentVisitor<GraphT>
    {
    private:
        WTOCycleDepthPtr _wtoCycleDepth;
        NodeRefToWTOCycleDepthPtr& _nodeToWTOCycleDepth;

    public:
        explicit WTOCycleDepthBuilder(
            NodeRefToWTOCycleDepthPtr& nodeToWTOCycleDepth)
            : _wtoCycleDepth(std::make_shared<GraphTWTOCycleDepth>()),
              _nodeToWTOCycleDepth(nodeToWTOCycleDepth)
        {
        }

        void visit(const WTOCycleT& cycle) override
        {
            const NodeT* head = cycle.head()->getICFGNode();
            WTOCycleDepthPtr previous_cycleDepth = _wtoCycleDepth;
            _nodeToWTOCycleDepth.insert(std::make_pair(head, _wtoCycleDepth));
            _wtoCycleDepth =
                std::make_shared<GraphTWTOCycleDepth>(*_wtoCycleDepth);
            _wtoCycleDepth->add(head);
            for (auto it = cycle.begin(), et = cycle.end(); it != et; ++it)
            {
                (*it)->accept(*this);
            }
            _wtoCycleDepth = previous_cycleDepth;
        }

        void visit(const WTONodeT& node) override
        {
            _nodeToWTOCycleDepth.insert(
                std::make_pair(node.getICFGNode(), _wtoCycleDepth));
        }

    }; // end class WTOCycleDepthBuilder

protected:

    inline virtual void forEachSuccessor(const NodeT* node, std::function<void(const NodeT*)> func) const
    {
        for (const auto& e : node->getOutEdges())
        {
            func(e->getDstNode());
        }
    }

protected:
    /// Return the depth-first number of the given node
    CycleDepthNumber getCDN(const NodeT* n) const
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

    /// Set the depth-first number of the given node
    void setCDN(const NodeT* n, const CycleDepthNumber& dfn)
    {
        auto res = _nodeToCDN.insert(std::make_pair(n, dfn));
        if (!res.second)
        {
            (res.first)->second = dfn;
        }
    }

    /// Pop a node from the stack
    const NodeT* pop()
    {
        assert(!_stack.empty() && "empty stack");
        const NodeT* top = _stack.back();
        _stack.pop_back();
        return top;
    }

    /// Push a node on the stack
    void push(const NodeT* n)
    {
        _stack.push_back(n);
    }

    const WTONodeT* newNode(const NodeT* node)
    {
        const WTONodeT* ptr = new WTONodeT(node);
        _allComponents.insert(ptr);
        return ptr;
    }

    const WTOCycleT* newCycle(const WTONodeT* node,
                              const WTOComponentRefList& partition)
    {
        const WTOCycleT* ptr = new WTOCycleT(node, std::move(partition));
        _allComponents.insert(ptr);
        return ptr;
    }

    /// Create the cycle component for the given node
    virtual const WTOCycleT* component(const NodeT* node)
    {
        WTOComponentRefList partition;
        forEachSuccessor(node, [&](const NodeT* succ)
        {
            if (getCDN(succ) == 0)
            {
                visit(succ, partition);
            }
        });
        const WTONodeT* head = newNode(node);
        const WTOCycleT* ptr = newCycle(head, partition);
        headRefToCycle.emplace(node, ptr);
        return ptr;
    }

    /// Visit the given node
    ///
    /// Algorithm to build a weak topological order of a graph
    virtual CycleDepthNumber visit(const NodeT* node,
                                   WTOComponentRefList& partition)
    {
        CycleDepthNumber head(0);
        CycleDepthNumber min(0);
        bool loop;

        push(node);
        _num += CycleDepthNumber(1);
        head = _num;
        setCDN(node, head);
        loop = false;
        forEachSuccessor(node, [&](const NodeT* succ)
        {
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
        });

        if (head == getCDN(node))
        {
            setCDN(node, UINT_MAX);
            const NodeT* element = pop();
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
    void buildNodeToDepth()
    {
        WTOCycleDepthBuilder builder(_nodeToDepth);
        for (auto it = begin(), et = end(); it != et; ++it)
        {
            (*it)->accept(builder);
        }
    }

}; // end class WTO

} // namespace SVF

#endif /* WTO_H_ */