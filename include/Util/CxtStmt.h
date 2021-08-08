//===- CxtStmt.h -- Context- and Thread-Sensitive Statement-------------------//
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
 * CxtStmt.h
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_UTIL_CXTSTMT_H_
#define INCLUDE_UTIL_CXTSTMT_H_

#include "Util/BasicTypes.h"

namespace SVF
{

/*!
 * Context-sensitive thread statement <c,s>
 */
class CxtStmt
{
public:
    /// Constructor
    CxtStmt(const CallStrCxt& c, const Instruction* f) :cxt(c), inst(f)
    {
    }
    /// Copy constructor
    CxtStmt(const CxtStmt& ctm) : cxt(ctm.getContext()),inst(ctm.getStmt())
    {
    }
    /// Destructor
    virtual ~CxtStmt()
    {
    }
    /// Return current context
    inline const CallStrCxt& getContext() const
    {
        return cxt;
    }
    /// Return current statement
    inline const Instruction* getStmt() const
    {
        return inst;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtStmt& rhs) const
    {
        if(inst!=rhs.getStmt())
            return inst < rhs.getStmt();
        else
            return cxt < rhs.getContext();
    }
    /// Overloading operator=
    inline CxtStmt& operator= (const CxtStmt& rhs)
    {
        if(*this!=rhs)
        {
            inst = rhs.getStmt();
            cxt = rhs.getContext();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const CxtStmt& rhs) const
    {
        return (inst == rhs.getStmt() && cxt == rhs.getContext());
    }
    /// Overloading operator==
    inline bool operator!= (const CxtStmt& rhs) const
    {
        return !(*this==rhs);
    }
    /// Return context in string format
    inline std::string cxtToStr() const
    {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "[:";
        for(CallStrCxt::const_iterator it = cxt.begin(), eit = cxt.end(); it!=eit; ++it)
        {
            rawstr << *it << " ";
        }
        rawstr << " ]";
        return rawstr.str();
    }
    /// Dump CxtStmt
    inline void dump() const
    {
        SVFUtil::outs() << "[ Current Stmt: " << SVFUtil::getSourceLoc(inst) << " " << *inst << "\t Contexts: " << cxtToStr() << "  ]\n";
    }

protected:
    CallStrCxt cxt;
    const Instruction* inst;
};


/*!
 * Context-sensitive thread statement <t,c,s>
 */
class CxtThreadStmt : public CxtStmt
{
public:
    /// Constructor
    CxtThreadStmt(NodeID t, const CallStrCxt& c, const Instruction* f) :CxtStmt(c,f), tid(t)
    {
    }
    /// Copy constructor
    CxtThreadStmt(const CxtThreadStmt& ctm) :CxtStmt(ctm), tid(ctm.getTid())
    {
    }
    /// Destructor
    virtual ~CxtThreadStmt()
    {
    }
    /// Return current context
    inline NodeID getTid() const
    {
        return tid;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtThreadStmt& rhs) const
    {
        if (tid != rhs.getTid())
            return tid < rhs.getTid();
        else if(inst!=rhs.getStmt())
            return inst < rhs.getStmt();
        else
            return cxt < rhs.getContext();
    }
    /// Overloading operator=
    inline CxtThreadStmt& operator= (const CxtThreadStmt& rhs)
    {
        if(*this!=rhs)
        {
            CxtStmt::operator=(rhs);
            tid = rhs.getTid();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const CxtThreadStmt& rhs) const
    {
        return (tid == rhs.getTid() && inst == rhs.getStmt() && cxt == rhs.getContext());
    }
    /// Overloading operator==
    inline bool operator!= (const CxtThreadStmt& rhs) const
    {
        return !(*this==rhs);
    }
    /// Dump CxtThreadStmt
    inline void dump() const
    {
        SVFUtil::outs() << "[ Current Thread id: " << tid << "  Stmt: " << SVFUtil::getSourceLoc(inst) << " " << *inst << "\t Contexts: " << cxtToStr() << "  ]\n";
    }

private:
    NodeID tid;
};


/*!
 * Context-sensitive thread <c,t>
 */
class CxtThread
{
public:
    /// Constructor
    CxtThread(const CallStrCxt& c, const CallInst* fork) : cxt(c), forksite(fork), inloop(false), incycle(false)
    {
    }
    /// Copy constructor
    CxtThread(const CxtThread& ct) :
        cxt(ct.getContext()), forksite(ct.getThread()), inloop(ct.isInloop()), incycle(ct.isIncycle())
    {
    }
    /// Destructor
    virtual ~CxtThread()
    {
    }
    /// Return context of the thread
    inline const CallStrCxt& getContext() const
    {
        return cxt;
    }
    /// Return forksite
    inline const CallInst* getThread() const
    {
        return forksite;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtThread& rhs) const
    {
        if (forksite != rhs.getThread())
            return forksite < rhs.getThread();
        else
            return cxt < rhs.getContext();
    }
    /// Overloading operator=
    inline CxtThread& operator= (const CxtThread& rhs)
    {
        if(*this!=rhs)
        {
            forksite = rhs.getThread();
            cxt = rhs.getContext();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const CxtThread& rhs) const
    {
        return (forksite == rhs.getThread() && cxt == rhs.getContext());
    }
    /// Overloading operator==
    inline bool operator!= (const CxtThread& rhs) const
    {
        return !(*this==rhs);
    }
    /// Return context in string format
    inline std::string cxtToStr() const
    {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "[:";
        for(CallStrCxt::const_iterator it = cxt.begin(), eit = cxt.end(); it!=eit; ++it)
        {
            rawstr << *it << " ";
        }
        rawstr << " ]";
        return rawstr.str();
    }

    /// inloop, incycle attributes
    //@{
    inline void setInloop(bool in)
    {
        inloop = in;
    }
    inline bool isInloop() const
    {
        return inloop;
    }
    inline void setIncycle(bool in)
    {
        incycle = in;
    }
    inline bool isIncycle() const
    {
        return incycle;
    }
    //@}

    /// Dump CxtThread
    inline void dump() const
    {
        std::string loop = inloop?", inloop":"";
        std::string cycle = incycle?", incycle":"";

        if(forksite)
            SVFUtil::outs() << "[ Thread: $" << SVFUtil::getSourceLoc(forksite) << "$ " << *forksite  << "\t Contexts: " << cxtToStr()
                            << loop << cycle <<"  ]\n";
        else
            SVFUtil::outs() << "[ Thread: " << "main   "  << "\t Contexts: " << cxtToStr()
                            << loop << cycle <<"  ]\n";
    }
protected:
    CallStrCxt cxt;
    const CallInst* forksite;
    bool inloop;
    bool incycle;
};


/*!
 * Context-sensitive procedure <c,m>
 * c represent current context
 * m represent current procedure
 */
class CxtProc
{
public:
    /// Constructor
    CxtProc(const CallStrCxt& c, const SVFFunction* f) :
        cxt(c), fun(f)
    {
    }
    /// Copy constructor
    CxtProc(const CxtProc& ctm) :
        cxt(ctm.getContext()), fun(ctm.getProc())
    {
    }
    /// Destructor
    virtual ~CxtProc()
    {
    }
    /// Return current procedure
    inline const SVFFunction* getProc() const
    {
        return fun;
    }
    /// Return current context
    inline const CallStrCxt& getContext() const
    {
        return cxt;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator<(const CxtProc& rhs) const
    {
        if (fun != rhs.getProc())
            return fun < rhs.getProc();
        else
            return cxt < rhs.getContext();
    }
    /// Overloading operator=
    inline CxtProc& operator=(const CxtProc& rhs)
    {
        if (*this != rhs)
        {
            fun = rhs.getProc();
            cxt = rhs.getContext();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator==(const CxtProc& rhs) const
    {
        return (fun == rhs.getProc() && cxt == rhs.getContext());
    }
    /// Overloading operator==
    inline bool operator!=(const CxtProc& rhs) const
    {
        return !(*this == rhs);
    }
    /// Return context in string format
    inline std::string cxtToStr() const
    {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "[:";
        for (CallStrCxt::const_iterator it = cxt.begin(), eit = cxt.end(); it != eit; ++it)
        {
            rawstr << *it << " ";
        }
        rawstr << " ]";
        return rawstr.str();
    }
    /// Dump CxtProc
    inline void dump() const
    {
        SVFUtil::outs() << "[ Proc: " << fun->getName() << "\t Contexts: " << cxtToStr() << "  ]\n";
    }

protected:
    CallStrCxt cxt;
    const SVFFunction* fun;
};


/*!
 * Context-sensitive procedure <t,c,m>
 * t represent current thread during traversing
 * c represent current context
 * m represent current procedure
 */
class CxtThreadProc : public CxtProc
{
public:
    /// Constructor
    CxtThreadProc(NodeID t, const CallStrCxt& c, const SVFFunction* f) :CxtProc(c,f),tid(t)
    {
    }
    /// Copy constructor
    CxtThreadProc(const CxtThreadProc& ctm) : CxtProc(ctm.getContext(),ctm.getProc()), tid(ctm.getTid())
    {
    }
    /// Destructor
    virtual ~CxtThreadProc()
    {
    }
    /// Return current thread id
    inline NodeID getTid() const
    {
        return tid;
    }
    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const CxtThreadProc& rhs) const
    {
        if (tid != rhs.getTid())
            return tid < rhs.getTid();
        else if(fun!=rhs.getProc())
            return fun < rhs.getProc();
        else
            return cxt < rhs.getContext();
    }
    /// Overloading operator=
    inline CxtThreadProc& operator= (const CxtThreadProc& rhs)
    {
        if(*this!=rhs)
        {
            tid = rhs.getTid();
            fun = rhs.getProc();
            cxt = rhs.getContext();
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const CxtThreadProc& rhs) const
    {
        return (tid == rhs.getTid() && fun == rhs.getProc() && cxt == rhs.getContext());
    }
    /// Overloading operator==
    inline bool operator!= (const CxtThreadProc& rhs) const
    {
        return !(*this==rhs);
    }
    /// Dump CxtThreadProc
    inline void dump() const
    {
        SVFUtil::outs() << "[ Current Thread id: " << tid << "  Proc: " << fun->getName() << "\t Contexts: " << cxtToStr() << "  ]\n";
    }

private:
    NodeID tid;
};

} // End namespace SVF
// Specialise has for class defined in this header file
template <> struct std::hash<SVF::CxtThread> {
	size_t operator()(const SVF::CxtThread& cs) const {
		std::hash<SVF::CallStrCxt> h;
		return h(cs.getContext());
	}
};
template <> struct std::hash<SVF::CxtThreadProc> {
	size_t operator()(const SVF::CxtThreadProc& ctp) const {
		std::hash<SVF::NodeID> h;
		return h(ctp.getTid());
	}
};
template <> struct std::hash<SVF::CxtThreadStmt> {
	size_t operator()(const SVF::CxtThreadStmt& cts) const {
		std::hash<SVF::NodeID> h;
		return h(cts.getTid());
	}
};
template <> struct std::hash<SVF::CxtStmt> {
	size_t operator()(const SVF::CxtStmt& cs) const {
		std::hash<SVF::Instruction*> h;
		SVF::Instruction* inst = const_cast<SVF::Instruction*> (cs.getStmt());
		return h(inst);
	}
};
template <> struct std::hash<SVF::CxtProc> {
	size_t operator()(const SVF::CxtProc& cs) const {
		std::hash<SVF::SVFFunction*> h;
		SVF::SVFFunction* fun = const_cast<SVF::SVFFunction*> (cs.getProc());
		return h(fun);
	}
};
#endif /* INCLUDE_UTIL_CXTSTMT_H_ */
