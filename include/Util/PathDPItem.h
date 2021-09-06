//===- PathDPItem.h -- path sensitive classes----------------------------//
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

#ifndef PATHDPITEM_H_
#define PATHDPITEM_H_

#include "Util/DPItem.h"

namespace SVF
{

class VFPathCond : public ContextCond
{

public:
    typedef CondExpr PathCond;
    typedef std::vector<std::pair<NodeID,NodeID> > EdgeSet;

public:
    /// Constructor
    VFPathCond(PathCond* p = CondManager::getTrueCond()) : ContextCond(), path(p)
    {
    }
    /// Copy Constructor
    VFPathCond(const VFPathCond& cond): ContextCond(cond), path(cond.getPaths()),edges(cond.getVFEdges())
    {
    }
    /// Destructor
    virtual ~VFPathCond()
    {
    }
    /// Return paths
    inline PathCond* getPaths() const
    {
        return path;
    }
    /// Return paths
    inline const EdgeSet& getVFEdges() const
    {
        return edges;
    }
    /// Set paths
    inline void setPaths(PathCond* p, const EdgeSet& e)
    {
        path = p;
        edges = e;
    }
    /// Get path length
    inline u32_t pathLen() const
    {
        return edges.size();
    }
    /// Add SVFG Edge
    inline void addVFEdge(NodeID from, NodeID to)
    {
        //	assert(!hasVFEdge(from,to) && "Edge exit?");
        if(edges.size() > maximumPath)
            maximumPath = edges.size();

        edges.push_back(std::make_pair(from,to));
    }
    /// Has SVFG Edge
    inline bool hasVFEdge(NodeID from, NodeID to) const
    {
        return std::find(edges.begin(),edges.end(),std::make_pair(from,to)) != edges.end();
    }
    /// Whether Node dst has incoming edge
    inline bool hasIncomingEdge(NodeID node) const
    {
        for(EdgeSet::const_iterator it = edges.begin(), eit =edges.end(); it!=eit; ++it)
        {
            if(it->second == node)
                return true;
        }
        return false;
    }
    /// Whether Node dst has outgoing edge
    inline bool hasOutgoingEdge(NodeID node) const
    {
        for(EdgeSet::const_iterator it = edges.begin(), eit =edges.end(); it!=eit; ++it)
        {
            if(it->first == node)
                return true;
        }
        return false;
    }
    inline bool addPath(CondManager* allocator, PathCond* c, NodeID from, NodeID to)
    {
        if(pathLen() < maximumPathLen)
        {
            if(!hasVFEdge(from,to))
            {
                /// drop condition when existing a loop (vf cycle)
                if(hasOutgoingEdge(from))
                    c = allocator->getTrueCond();
            }
            addVFEdge(from,to);
            return condAnd(allocator,c);
        }
        //	DBOUT(DDDA, SVFUtil::outs() << "\t\t!!path length beyond limits \n");
        return true;
    }
    /// Condition operatoration
    //@{
    inline bool condAnd(CondManager* allocator, PathCond* c)
    {
        path = allocator->AND(path,c);
        return path != allocator->getFalseCond();
    }
    inline void condOr(CondManager* allocator, PathCond* c)
    {
        path = allocator->OR(path,c);
    }
    //@}

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const VFPathCond& rhs) const
    {
        if(path != rhs.path)
            return path < rhs.path;
        else
            return context < rhs.context;
    }
    /// Overloading operator=
    inline VFPathCond& operator= (const VFPathCond& rhs)
    {
        if(*this != rhs)
        {
            ContextCond::operator=(rhs);
            path = rhs.getPaths();
            edges = rhs.getVFEdges();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const VFPathCond& rhs) const
    {
        return (context == rhs.getContexts() && path == rhs.getPaths());
    }
    /// Overloading operator!=
    inline bool operator!= (const VFPathCond& rhs) const
    {
        return !(*this==rhs);
    }
    /// Get value-flow edge traces
    inline std::string vfEdgesTrace() const
    {
        std::string str;
        raw_string_ostream rawstr(str);
        for(EdgeSet::const_iterator it = edges.begin(), eit = edges.end(); it!=eit; ++it)
        {
            rawstr << "(" << it->first << "," << it->second << ")";
        }
        return rawstr.str();
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
        rawstr << " | ";
        rawstr << "" << path << "] " << vfEdgesTrace() ;
        return rawstr.str();
    }

private:
    PathCond* path;
    EdgeSet edges;

};


/*!
 * Path-sensitive DPItem
 */
typedef CondVar<VFPathCond> VFPathVar;
typedef CondStdSet<VFPathVar> VFPathPtSet;

template<class LocCond>
class PathStmtDPItem : public StmtDPItem<LocCond>
{
private:
    VFPathCond vfpath;
public:
    typedef VFPathCond::PathCond PathCond;

    /// Constructor
    PathStmtDPItem(const VFPathVar& var, const LocCond* locCond) :
        StmtDPItem<LocCond>(var.get_id(),locCond), vfpath(var.get_cond())
    {
    }
    /// Copy constructor
    PathStmtDPItem(const PathStmtDPItem<LocCond>& dps) :
        StmtDPItem<LocCond>(dps),vfpath(dps.getCond())
    {
    }
    /// Destructor
    virtual ~PathStmtDPItem()
    {
    }
    inline VFPathVar getCondVar() const
    {
        VFPathVar var(this->vfpath,this->cur);
        return var;
    }
    /// Get value-flow paths
    inline const VFPathCond& getCond() const
    {
        return this->vfpath;
    }
    /// Get value-flow paths
    inline VFPathCond& getCond()
    {
        return this->vfpath;
    }
    /// Add a value-flow path (avoid adding duplicated paths)
    inline bool addVFPath(CondManager* allocator, PathCond* c, NodeID from, NodeID to)
    {
        return this->vfpath.addPath(allocator,c,from,to);
    }
    /// Push context
    inline bool pushContext(NodeID cxt)
    {
        return this->vfpath.pushContext(cxt);
    }
    /// Match context
    bool matchContext(NodeID cxt)
    {
        return this->vfpath.matchContext(cxt);
    }

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const PathStmtDPItem<LocCond>& rhs) const
    {
        if (this->cur != rhs.getCurNodeID())
            return this->cur < rhs.getCurNodeID();
        else if(this->curloc != rhs.getLoc())
            return this->curloc < rhs.getLoc();
        else
            return this->vfpath < rhs.getCond();
    }
    /// Overloading operator=
    inline PathStmtDPItem<LocCond>& operator= (const PathStmtDPItem<LocCond>& rhs)
    {
        if(*this!=rhs)
        {
            StmtDPItem<LocCond>::operator=(rhs);
            this->vfpath = rhs.getCond();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const PathStmtDPItem<LocCond>& rhs) const
    {
        return (this->cur == rhs.cur && this->curloc == rhs.getLoc() && this->vfpath==rhs.getCond());
    }
    /// Overloading operator!=
    inline bool operator!= (const PathStmtDPItem<LocCond>& rhs) const
    {
        return !(*this==rhs);
    }
    /// Dump dpm info
    inline void dump() const
    {
        SVFUtil::outs() << "statement " << *(this->curloc)  << ", var " << this->cur << " ";
        SVFUtil::outs() << this->vfpath.toString() << "\n";
    }
};




} // End namespace SVF



#endif /* DPITEM_H_ */