//===- DPItem.h -- Context/path sensitive classes----------------------------//
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
 * DPItem.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef DPITEM_H_
#define DPITEM_H_

#include "Util/PathCondAllocator.h"
#include "MemoryModel/ConditionalPT.h"
#include "MSSA/SVFGNode.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>    // std::sort

/*!
 * Dynamic programming item for CFL researchability search
 * This serves as a base class for CFL-reachability formulation by matching parentheses.
 * Extend this class for further sophisticated CFL-reachability items (e.g. field, flow, path)
 */
class DPItem {
protected:
    NodeID cur;
    static u64_t maximumBudget;

public:
    /// Constructor
    DPItem(NodeID c) : cur(c) {
    }
    /// Copy constructor
    DPItem(const DPItem& dps) : cur(dps.cur) {
    }
    /// Destructor
    virtual ~DPItem() {
    }
    inline NodeID getCurNodeID() const {
        return cur;
    }
    inline void setCurNodeID(NodeID c) {
        cur = c;
    }
    /// set max step budge per query
    static inline void setMaxBudget(u32_t max) {
        maximumBudget = max;
    }
    static inline u32_t getMaxBudget() {
        return maximumBudget;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const DPItem& rhs) const {
        return cur < rhs.cur;
    }
    /// Overloading Operator=
    inline DPItem& operator= (const DPItem& rhs) {
        if(*this!=rhs) {
            cur = rhs.cur;
        }
        return *this;
    }
    /// Overloading Operator==
    inline bool operator== (const DPItem& rhs) const {
        return (cur == rhs.cur);
    }
    /// Overloading Operator!=
    inline bool operator!= (const DPItem& rhs) const {
        return !(*this == rhs);
    }

    inline void dump() const {
        llvm::outs() << "var " << cur << "\n";
    }
};

class SVFGNode;
typedef SVFGNode LocCond;

/*!
 * FlowSensitive DPItem
 */
class LocDPItem : public DPItem {


protected:
    const LocCond* curloc;

public:
    /// Constructor
    LocDPItem(NodeID c, const LocCond* locCond) : DPItem(c), curloc(locCond) {
    }
    /// Copy constructor
    LocDPItem(const LocDPItem& dps) :
        DPItem(dps), curloc(dps.curloc) {
    }
    /// Destructor
    virtual ~LocDPItem() {
    }
    /// Get context
    inline const LocCond* getLoc() const {
        return curloc;
    }
    /// Set location
    inline void setLoc(const LocCond* l) {
        curloc = l;
    }
    /// Set location and pointer id
    inline void setLocVar(const LocCond* l,NodeID v) {
        curloc = l;
        cur = v;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const LocDPItem& rhs) const {
        if (cur != rhs.cur)
            return cur < rhs.cur;
        else
            return curloc < rhs.curloc;
    }
    /// Overloading operator==
    inline LocDPItem& operator= (const LocDPItem& rhs)  {
        if(*this!=rhs) {
            DPItem::operator=(rhs);
            curloc = rhs.getLoc();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const LocDPItem& rhs) const {
        return (cur == rhs.cur && curloc == rhs.getLoc());
    }
    /// Overloading operator!=
    inline bool operator!= (const LocDPItem& rhs) const {
        return !(*this==rhs);
    }
    inline void dump() const {
        llvm::outs() << "svfg node " << curloc->getId()  << ", var " << cur << "\n";
    }
};

/*!
 * Context Condition
 */
class ContextCond {
public:
    typedef CallStrCxt::const_iterator const_iterator;
    /// Constructor
    ContextCond():concreteCxt(true) {
    }
    /// Copy Constructor
    ContextCond(const ContextCond& cond): context(cond.getContexts()), concreteCxt(cond.isConcreteCxt()) {
    }
    /// Destructor
    virtual ~ContextCond() {
    }
    /// Get context
    inline const CallStrCxt& getContexts() const {
        return context;
    }
    /// Get context
    inline CallStrCxt& getContexts() {
        return context;
    }
    /// Whether it is an concrete context
    inline bool isConcreteCxt() const {
        return concreteCxt;
    }
    /// Whether it is an concrete context
    inline void setNonConcreteCxt() {
        concreteCxt = false;
    }
    /// Whether contains callstring cxt
    inline bool containCallStr(NodeID cxt) const {
        return std::find(context.begin(),context.end(),cxt) != context.end();
    }
    /// Get context size
    inline u32_t cxtSize() const {
        return context.size();
    }
    /// set max context limit
    static inline void setMaxCxtLen(u32_t max) {
        maximumCxtLen = max;
    }
    /// Push context
    inline virtual bool pushContext(NodeID ctx) {

        if(context.size() < maximumCxtLen) {
            context.push_back(ctx);

            if(context.size() > maximumCxt)
                maximumCxt = context.size();
            return true;
        }
        else { /// handle out of context limit case
            if(!context.empty()) {
                setNonConcreteCxt();
                context.erase(context.begin());
                context.push_back(ctx);
            }
            return false;
        }
    }

    /// Match context
    inline virtual bool matchContext(NodeID ctx) {
        /// if context is empty, then it is the unbalanced parentheses match
        if(context.empty())
            return true;
        /// otherwise, we perform balanced parentheses matching
        else if(context.back() == ctx) {
            context.pop_back();
            return true;
        }
        return false;
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const ContextCond& rhs) const {
        return context < rhs.context;
    }
    /// Overloading operator[]
    inline NodeID operator[] (const u32_t index) const {
        assert(index < context.size());
        return context[index];
    }
    /// Overloading operator=
    inline ContextCond& operator= (const ContextCond& rhs) {
        if(*this!=rhs) {
            context = rhs.getContexts();
            concreteCxt = rhs.isConcreteCxt();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const ContextCond& rhs) const {
        return (context == rhs.getContexts());
    }
    /// Overloading operator!=
    inline bool operator!= (const ContextCond& rhs) const {
        return !(*this==rhs);
    }
    /// Begin iterators
    inline const_iterator begin() const {
        return context.begin();
    }
    /// End iterators
    inline const_iterator end() const {
        return context.end();
    }
    /// Dump context condition
    inline std::string toString() const {
        std::string str;
        llvm::raw_string_ostream rawstr(str);
        rawstr << "[:";
        for(CallStrCxt::const_iterator it = context.begin(), eit = context.end(); it!=eit; ++it) {
            rawstr << *it << " ";
        }
        rawstr << " ]";
        return rawstr.str();
    }
protected:
    CallStrCxt context;
private:
    static u32_t maximumCxtLen;
    bool concreteCxt;
public:
    static u32_t maximumCxt;
};

/*!
 * Context-, flow- sensitive DPItem
 */
typedef CondVar<ContextCond> CxtVar;
typedef CondStdSet<CxtVar> CxtPtSet;
class CxtLocDPItem : public LocDPItem {
private:
    ContextCond context;
public:
    /// Constructor
    CxtLocDPItem(const CxtVar& var, const LocCond* locCond) : LocDPItem(var.get_id(),locCond), context(var.get_cond()) {
    }
    /// Copy constructor
    CxtLocDPItem(const CxtLocDPItem& dps) :
        LocDPItem(dps), context(dps.context) {
    }
    /// Destructor
    virtual ~CxtLocDPItem() {
    }
    /// Get context var
    inline CxtVar getCondVar() const {
        CxtVar var(context,cur);
        return var;
    }
    /// Get context
    inline const ContextCond& getCond() const {
        return context;
    }
    /// Get context
    inline ContextCond& getCond() {
        return context;
    }
    /// Push context
    inline bool pushContext(NodeID cxt) {
        return context.pushContext(cxt);
    }

    /// Match context
    inline bool matchContext(NodeID cxt) {
        return context.matchContext(cxt);
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtLocDPItem& rhs) const {
        if (cur != rhs.cur)
            return cur < rhs.cur;
        else if(curloc != rhs.getLoc())
            return curloc < rhs.getLoc();
        else
            return context < rhs.context;
    }
    /// Overloading operator=
    inline CxtLocDPItem& operator= (const CxtLocDPItem& rhs) {
        if(*this!=rhs) {
            LocDPItem::operator=(rhs);
            context = rhs.getCond();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const CxtLocDPItem& rhs) const {
        return (cur == rhs.cur && curloc == rhs.getLoc() && context == rhs.context);
    }
    /// Overloading operator==
    inline bool operator!= (const CxtLocDPItem& rhs) const {
        return !(*this==rhs);
    }
    inline void dump() const {
        llvm::outs() << "svfg node " << curloc->getId()  << ", var " << cur << " ";
        llvm::outs() << context.toString()  <<"\n";
    }
};


class VFPathCond : public ContextCond {

public:
    typedef PathCondAllocator::Condition PathCond;
    typedef std::vector<std::pair<NodeID,NodeID> > EdgeSet;

public:
    /// Constructor
    VFPathCond(PathCond* p = PathCondAllocator::trueCond()) : ContextCond(), path(p) {
    }
    /// Copy Constructor
    VFPathCond(const VFPathCond& cond): ContextCond(cond), path(cond.getPaths()),edges(cond.getVFEdges()) {
    }
    /// Destructor
    virtual ~VFPathCond() {
    }
    /// set max path limit
    static inline void setMaxPathLen(u32_t max) {
        maximumPathLen = max;
    }
    inline u32_t getMaxPathLen() const {
        return maximumPathLen;
    }
    /// Return paths
    inline PathCond* getPaths() const {
        return path;
    }
    /// Return paths
    inline const EdgeSet& getVFEdges() const {
        return edges;
    }
    /// Set paths
    inline void setPaths(PathCond* p, const EdgeSet& e) {
        path = p;
        edges = e;
    }
    /// Get path length
    inline u32_t pathLen() const {
        return edges.size();
    }
    /// Add SVFG Edge
    inline void addVFEdge(NodeID from, NodeID to) {
        //	assert(!hasVFEdge(from,to) && "Edge exit?");
        if(edges.size() > maximumPath)
            maximumPath = edges.size();

        edges.push_back(std::make_pair(from,to));
    }
    /// Has SVFG Edge
    inline bool hasVFEdge(NodeID from, NodeID to) const {
        return std::find(edges.begin(),edges.end(),std::make_pair(from,to)) != edges.end();
    }
    /// Whether Node dst has incoming edge
    inline bool hasIncomingEdge(NodeID node) const {
        for(EdgeSet::const_iterator it = edges.begin(), eit =edges.end(); it!=eit; ++it) {
            if(it->second == node)
                return true;
        }
        return false;
    }
    /// Whether Node dst has outgoing edge
    inline bool hasOutgoingEdge(NodeID node) const {
        for(EdgeSet::const_iterator it = edges.begin(), eit =edges.end(); it!=eit; ++it) {
            if(it->first == node)
                return true;
        }
        return false;
    }
    inline bool addPath(PathCondAllocator* allocator, PathCond* c, NodeID from, NodeID to) {
        if(pathLen() < maximumPathLen) {
            if(!hasVFEdge(from,to)) {
                /// drop condition when existing a loop (vf cycle)
                if(hasOutgoingEdge(from))
                    c = allocator->getTrueCond();
            }
            addVFEdge(from,to);
            return condAnd(allocator,c);
        }
        //	DBOUT(DDDA, llvm::outs() << "\t\t!!path length beyond limits \n");
        return true;
    }
    /// Condition operatoration
    //@{
    inline bool condAnd(PathCondAllocator* allocator, PathCond* c) {
        path = allocator->condAnd(path,c);
        return path != allocator->getFalseCond();
    }
    inline void condOr(PathCondAllocator* allocator, PathCond* c) {
        path = allocator->condOr(path,c);
    }
    //@}

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const VFPathCond& rhs) const {
        if(path != rhs.path)
            return path < rhs.path;
        else
            return context < rhs.context;
    }
    /// Overloading operator=
    inline VFPathCond& operator= (const VFPathCond& rhs) {
        if(*this != rhs) {
            ContextCond::operator=(rhs);
            path = rhs.getPaths();
            edges = rhs.getVFEdges();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const VFPathCond& rhs) const {
        return (context == rhs.getContexts() && path == rhs.getPaths());
    }
    /// Overloading operator!=
    inline bool operator!= (const VFPathCond& rhs) const {
        return !(*this==rhs);
    }
    /// Get value-flow edge traces
    inline std::string vfEdgesTrace() const {
        std::string str;
        llvm::raw_string_ostream rawstr(str);
        for(EdgeSet::const_iterator it = edges.begin(), eit = edges.end(); it!=eit; ++it) {
            rawstr << "(" << it->first << "," << it->second << ")";
        }
        return rawstr.str();
    }
    /// Dump context condition
    inline std::string toString() const {
        std::string str;
        llvm::raw_string_ostream rawstr(str);
        rawstr << "[:";
        for(CallStrCxt::const_iterator it = context.begin(), eit = context.end(); it!=eit; ++it) {
            rawstr << *it << " ";
        }
        rawstr << " | ";
        rawstr << "" << path << "] " << vfEdgesTrace() ;
        return rawstr.str();
    }

private:
    PathCond* path;
    EdgeSet edges;
    static u32_t maximumPathLen;
public:
    static u32_t maximumPath;

};


/*!
 * Path-sensitive DPItem
 */
typedef CondVar<VFPathCond> VFPathVar;
typedef CondStdSet<VFPathVar> VFPathPtSet;
class PathDPItem : public LocDPItem {
private:
    VFPathCond vfpath;
public:
    typedef VFPathCond::PathCond PathCond;

    /// Constructor
    PathDPItem(const VFPathVar& var, const LocCond* locCond) :
        LocDPItem(var.get_id(),locCond), vfpath(var.get_cond()) {
    }
    /// Copy constructor
    PathDPItem(const PathDPItem& dps) :
        LocDPItem(dps),vfpath(dps.getCond()) {
    }
    /// Destructor
    virtual ~PathDPItem() {
    }
    inline VFPathVar getCondVar() const {
        VFPathVar var(vfpath,cur);
        return var;
    }
    /// Get value-flow paths
    inline const VFPathCond& getCond() const {
        return vfpath;
    }
    /// Get value-flow paths
    inline VFPathCond& getCond() {
        return vfpath;
    }
    /// Add a value-flow path (avoid adding duplicated paths)
    inline bool addVFPath(PathCondAllocator* allocator, PathCond* c, NodeID from, NodeID to) {
        return vfpath.addPath(allocator,c,from,to);
    }
    /// Push context
    inline bool pushContext(NodeID cxt) {
        return vfpath.pushContext(cxt);
    }
    /// Match context
    bool matchContext(NodeID cxt) {
        return vfpath.matchContext(cxt);
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const PathDPItem& rhs) const {
        if (cur != rhs.getCurNodeID())
            return cur < rhs.getCurNodeID();
        else if(curloc != rhs.getLoc())
            return curloc < rhs.getLoc();
        else
            return vfpath < rhs.getCond();
    }
    /// Overloading operator=
    inline PathDPItem& operator= (const PathDPItem& rhs) {
        if(*this!=rhs) {
            LocDPItem::operator=(rhs);
            vfpath = rhs.getCond();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const PathDPItem& rhs) const {
        return (cur == rhs.cur && curloc == rhs.getLoc() && vfpath==rhs.getCond());
    }
    /// Overloading operator!=
    inline bool operator!= (const PathDPItem& rhs) const {
        return !(*this==rhs);
    }
    /// Dump dpm info
    inline void dump() const {
        llvm::outs() << "svfg node " << curloc->getId()  << ", var " << cur << " ";
        llvm::outs() << vfpath.toString() << "\n";
    }
};


/*!
 * Context DPItem
 */
typedef CondVar<ContextCond> CxtVar;
class CxtDPItem : public DPItem {
private:
    ContextCond context;

public:
    /// Constructor
    CxtDPItem(NodeID c, const ContextCond& cxt) : DPItem(c),context(cxt) {
    }
    CxtDPItem(const CxtVar& var) : DPItem(var.get_id()),context(var.get_cond()) {
    }
    /// Copy constructor
    CxtDPItem(const CxtDPItem& dps) :
        DPItem(dps.getCurNodeID()), context(dps.context) {
    }
    /// Destructor
    virtual ~CxtDPItem() {
    }

    /// Get context
    inline const ContextCond& getContexts() const {
        return context;
    }
    /// Push context
    inline void pushContext(NodeID cxt) {
        context.pushContext(cxt);
    }

    /// Match context
    inline virtual bool matchContext(NodeID cxt) {
        return context.matchContext(cxt);
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtDPItem& rhs) const {
        if (cur != rhs.cur)
            return cur < rhs.cur;
        else
            return context < rhs.context;
    }

};


#endif /* DPITEM_H_ */
