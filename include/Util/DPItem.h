//===- DPItem.h -- Context/path sensitive classes----------------------------//
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
 * DPItem.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef DPITEM_H_
#define DPITEM_H_

#include "Util/Conditions.h"
#include "MemoryModel/ConditionalPT.h"
#include <algorithm>    // std::sort

namespace SVF
{

/*!
 * Dynamic programming item for CFL researchability search
 * This serves as a base class for CFL-reachability formulation by matching parentheses.
 * Extend this class for further sophisticated CFL-reachability items (e.g. field, flow, path)
 */
class DPItem
{
protected:
    NodeID cur;
    static u64_t maximumBudget;

public:
    /// Constructor
    DPItem(NodeID c) : cur(c)
    {
    }
    /// Copy constructor
    DPItem(const DPItem& dps) : cur(dps.cur)
    {
    }
    /// Destructor
    virtual ~DPItem()
    {
    }
    inline NodeID getCurNodeID() const
    {
        return cur;
    }
    inline void setCurNodeID(NodeID c)
    {
        cur = c;
    }
    /// set max step budge per query
    static inline void setMaxBudget(u32_t max)
    {
        maximumBudget = max;
    }
    static inline u32_t getMaxBudget()
    {
        return maximumBudget;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const DPItem& rhs) const
    {
        return cur < rhs.cur;
    }
    /// Overloading Operator=
    inline DPItem& operator= (const DPItem& rhs)
    {
        if(*this!=rhs)
        {
            cur = rhs.cur;
        }
        return *this;
    }
    /// Overloading Operator==
    inline bool operator== (const DPItem& rhs) const
    {
        return (cur == rhs.cur);
    }
    /// Overloading Operator!=
    inline bool operator!= (const DPItem& rhs) const
    {
        return !(*this == rhs);
    }

    inline void dump() const
    {
        SVFUtil::outs() << "var " << cur << "\n";
    }
};


/*!
 * FlowSensitive DPItem
 */
template<class LocCond>
class StmtDPItem : public DPItem
{


protected:
    const LocCond* curloc;

public:
    /// Constructor
    StmtDPItem(NodeID c, const LocCond* locCond) : DPItem(c), curloc(locCond)
    {
    }
    /// Copy constructor
    StmtDPItem(const StmtDPItem& dps) :
        DPItem(dps), curloc(dps.curloc)
    {
    }
    /// Destructor
    virtual ~StmtDPItem()
    {
    }
    /// Get context
    inline const LocCond* getLoc() const
    {
        return this->curloc;
    }
    /// Set location
    inline void setLoc(const LocCond* l)
    {
        this->curloc = l;
    }
    /// Set location and pointer id
    inline void setLocVar(const LocCond* l,NodeID v)
    {
        this->curloc = l;
        this->cur = v;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const StmtDPItem& rhs) const
    {
        if (this->cur != rhs.cur)
            return this->cur < rhs.cur;
        else
            return this->curloc < rhs.curloc;
    }
    /// Overloading operator==
    inline StmtDPItem& operator= (const StmtDPItem& rhs)
    {
        if(*this!=rhs)
        {
            DPItem::operator=(rhs);
            this->curloc = rhs.getLoc();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const StmtDPItem& rhs) const
    {
        return (this->cur == rhs.cur && this->curloc == rhs.getLoc());
    }
    /// Overloading operator!=
    inline bool operator!= (const StmtDPItem& rhs) const
    {
        return !(*this==rhs);
    }
    inline void dump() const
    {
        SVFUtil::outs() << "statement " << *(this->curloc)  << ", var " << this->cur << "\n";
    }
};

/*!
 * Context Condition
 */
class ContextCond
{
public:
    typedef CallStrCxt::const_iterator const_iterator;
    /// Constructor
    ContextCond():concreteCxt(true)
    {
    }
    /// Copy Constructor
    ContextCond(const ContextCond& cond): context(cond.getContexts()), concreteCxt(cond.isConcreteCxt())
    {
    }
    /// Destructor
    virtual ~ContextCond()
    {
    }
    /// Get context
    inline const CallStrCxt& getContexts() const
    {
        return context;
    }
    /// Get context
    inline CallStrCxt& getContexts()
    {
        return context;
    }
    /// Whether it is an concrete context
    inline bool isConcreteCxt() const
    {
        return concreteCxt;
    }
    /// Whether it is an concrete context
    inline void setNonConcreteCxt()
    {
        concreteCxt = false;
    }
    /// Whether contains callstring cxt
    inline bool containCallStr(NodeID cxt) const
    {
        return std::find(context.begin(),context.end(),cxt) != context.end();
    }
    /// Get context size
    inline u32_t cxtSize() const
    {
        return context.size();
    }
    /// set max context limit
    static inline void setMaxCxtLen(u32_t max)
    {
        maximumCxtLen = max;
    }
    /// set max path limit
    static inline void setMaxPathLen(u32_t max)
    {
        maximumPathLen = max;
    }
    inline u32_t getMaxPathLen() const
    {
        return maximumPathLen;
    }
    /// Push context
    inline virtual bool pushContext(NodeID ctx)
    {

        if(context.size() < maximumCxtLen)
        {
            context.push_back(ctx);

            if(context.size() > maximumCxt)
                maximumCxt = context.size();
            return true;
        }
        else   /// handle out of context limit case
        {
            if(!context.empty())
            {
                setNonConcreteCxt();
                context.erase(context.begin());
                context.push_back(ctx);
            }
            return false;
        }
    }

    /// Match context
    inline virtual bool matchContext(NodeID ctx)
    {
        /// if context is empty, then it is the unbalanced parentheses match
        if(context.empty())
            return true;
        /// otherwise, we perform balanced parentheses matching
        else if(context.back() == ctx)
        {
            context.pop_back();
            return true;
        }
        return false;
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const ContextCond& rhs) const
    {
        return context < rhs.context;
    }
    /// Overloading operator[]
    inline NodeID operator[] (const u32_t index) const
    {
        assert(index < context.size());
        return context[index];
    }
    /// Overloading operator=
    inline ContextCond& operator= (const ContextCond& rhs)
    {
        if(*this!=rhs)
        {
            context = rhs.getContexts();
            concreteCxt = rhs.isConcreteCxt();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const ContextCond& rhs) const
    {
        return (context == rhs.getContexts());
    }
    /// Overloading operator!=
    inline bool operator!= (const ContextCond& rhs) const
    {
        return !(*this==rhs);
    }
    /// Begin iterators
    inline const_iterator begin() const
    {
        return context.begin();
    }
    /// End iterators
    inline const_iterator end() const
    {
        return context.end();
    }
    /// Dump context condition
    inline std::string toString() const
    {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "[:";
        for(CallStrCxt::const_iterator it = context.begin(), eit = context.end(); it!=eit; ++it)
        {
            rawstr << *it << " ";
        }
        rawstr << " ]";
        return rawstr.str();
    }
protected:
    CallStrCxt context;
    static u32_t maximumCxtLen;
    static u32_t maximumPathLen;
    bool concreteCxt;
public:
    static u32_t maximumCxt;
    static u32_t maximumPath;
};

/*!
 * Context-, flow- sensitive DPItem
 */
typedef CondVar<ContextCond> CxtVar;
typedef CondStdSet<CxtVar> CxtPtSet;

template<class LocCond>
class CxtStmtDPItem : public StmtDPItem<LocCond>
{
private:
    ContextCond context;
public:
    /// Constructor
    CxtStmtDPItem(const CxtVar& var, const LocCond* locCond) : StmtDPItem<LocCond>(var.get_id(),locCond), context(var.get_cond())
    {
    }
    /// Copy constructor
    CxtStmtDPItem(const CxtStmtDPItem<LocCond>& dps) :
        StmtDPItem<LocCond>(dps), context(dps.context)
    {
    }
    /// Destructor
    virtual ~CxtStmtDPItem()
    {
    }
    /// Get context var
    inline CxtVar getCondVar() const
    {
        CxtVar var(this->context,this->cur);
        return var;
    }
    /// Get context
    inline const ContextCond& getCond() const
    {
        return this->context;
    }
    /// Get context
    inline ContextCond& getCond()
    {
        return this->context;
    }
    /// Push context
    inline bool pushContext(NodeID cxt)
    {
        return this->context.pushContext(cxt);
    }

    /// Match context
    inline bool matchContext(NodeID cxt)
    {
        return this->context.matchContext(cxt);
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtStmtDPItem<LocCond>& rhs) const
    {
        if (this->cur != rhs.cur)
            return this->cur < rhs.cur;
        else if(this->curloc != rhs.getLoc())
            return this->curloc < rhs.getLoc();
        else
            return this->context < rhs.context;
    }
    /// Overloading operator=
    inline CxtStmtDPItem<LocCond>& operator= (const CxtStmtDPItem<LocCond>& rhs)
    {
        if(*this!=rhs)
        {
            StmtDPItem<LocCond>::operator=(rhs);
            this->context = rhs.getCond();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const CxtStmtDPItem<LocCond>& rhs) const
    {
        return (this->cur == rhs.cur && this->curloc == rhs.getLoc() && this->context == rhs.context);
    }
    /// Overloading operator==
    inline bool operator!= (const CxtStmtDPItem<LocCond>& rhs) const
    {
        return !(*this==rhs);
    }
    inline void dump() const
    {
        SVFUtil::outs() << "statement " << *(this->curloc)  << ", var " << this->cur << " ";
        SVFUtil::outs() << this->context.toString()  <<"\n";
    }
};

/*!
 * Context DPItem
 */
typedef CondVar<ContextCond> CxtVar;
class CxtDPItem : public DPItem
{
private:
    ContextCond context;

public:
    /// Constructor
    CxtDPItem(NodeID c, const ContextCond& cxt) : DPItem(c),context(cxt)
    {
    }
    CxtDPItem(const CxtVar& var) : DPItem(var.get_id()),context(var.get_cond())
    {
    }
    /// Copy constructor
    CxtDPItem(const CxtDPItem& dps) :
        DPItem(dps.getCurNodeID()), context(dps.context)
    {
    }
    /// Destructor
    virtual ~CxtDPItem()
    {
    }

    /// Get context
    inline const ContextCond& getContexts() const
    {
        return context;
    }
    /// Push context
    inline void pushContext(NodeID cxt)
    {
        context.pushContext(cxt);
    }

    /// Match context
    inline virtual bool matchContext(NodeID cxt)
    {
        return context.matchContext(cxt);
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtDPItem& rhs) const
    {
        if (cur != rhs.cur)
            return cur < rhs.cur;
        else
            return context < rhs.context;
    }
    /// Overloading Operator=
    inline CxtDPItem& operator= (const CxtDPItem& rhs)
    {
        if(*this!=rhs)
        {
            cur = rhs.cur;
            context = rhs.context;
        }
        return *this;
    }
    /// Overloading Operator==
    inline bool operator== (const CxtDPItem& rhs) const
    {
        return (cur == rhs.cur) && (context == rhs.context);
    }
    /// Overloading Operator!=
    inline bool operator!= (const CxtDPItem& rhs) const
    {
        return !(*this == rhs);
    }

};
}

/// Specialise hash for CxtDPItem.
template <>
struct std::hash<SVF::CxtDPItem>
{
    size_t operator()(const SVF::CxtDPItem &cdpi) const
    {
        SVF::Hash<std::pair<SVF::NodeID, SVF::ContextCond>> h;
        return h(std::make_pair(cdpi.getCurNodeID(), cdpi.getContexts()));
    }
};

/// Specialise hash for StmtDPItem.
template <typename LocCond>
struct std::hash<SVF::StmtDPItem<LocCond>>
{
    size_t operator()(const SVF::StmtDPItem<LocCond> &sdpi) const
    {
        SVF::Hash<std::pair<SVF::NodeID, const LocCond *>> h;
        return h(std::make_pair(sdpi.getCurNodeID(), sdpi.getLoc()));
    }
};

/// Specialise hash for CxtStmtDPItem.
template<class LocCond>
struct std::hash<SVF::CxtStmtDPItem<LocCond>>
{
    size_t operator()(const SVF::CxtStmtDPItem<LocCond> &csdpi) const
    {
        SVF::Hash<std::pair<SVF::NodeID, std::pair<const LocCond *, SVF::ContextCond>>> h;
        return h(std::make_pair(csdpi.getCurNodeID(),
                                std::make_pair(csdpi.getLoc(), csdpi.getCond())));
    }
};

/// Specialise hash for ContextCond.
template <>
struct std::hash<const SVF::ContextCond>
{
    size_t operator()(const SVF::ContextCond &cc) const
    {
        std::hash<SVF::CallStrCxt> h;
        return h(cc.getContexts());
    }
};

template <>
struct std::hash<SVF::ContextCond>
{
    size_t operator()(const SVF::ContextCond &cc) const
    {
        std::hash<SVF::CallStrCxt> h;
        return h(cc.getContexts());
    }
};
#endif /* DPITEM_H_ */
