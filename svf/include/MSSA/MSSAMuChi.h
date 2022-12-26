//===- MSSAMuChi.h -- Mu/Chi on MSSA------------------------------------------//
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
 * MSSAMuChi.h
 *
 *  Created on: Oct 31, 2013
 *      Author: Yulei Sui
 */

#ifndef MSSAMUCHI_H_
#define MSSAMUCHI_H_

#include "MSSA/MemRegion.h"

namespace SVF
{

class MSSADEF;

/*!
 * Memory SSA Variable (in the form of SSA versions of each memory region )
 */
class MRVer
{

public:
    typedef MSSADEF MSSADef;
private:
    /// ver ID 0 is reserved
    static u32_t totalVERNum;
    const MemRegion* mr;
    MRVERSION version;
    MRVERID vid;
    MSSADef* def;
public:
    /// Constructor
    MRVer(const MemRegion* m, MRVERSION v, MSSADef* d) :
        mr(m), version(v), vid(totalVERNum++),def(d)
    {
    }

    /// Return the memory region
    inline const MemRegion* getMR() const
    {
        return mr;
    }

    /// Return SSA version
    inline MRVERSION getSSAVersion() const
    {
        return version;
    }

    /// Get MSSADef
    inline MSSADef* getDef() const
    {
        return def;
    }

    /// Get MRVERID
    inline MRVERID getID() const
    {
        return vid;
    }
};


/*!
 * Indirect Memory Read
 * 1) LoadMU at each store instruction
 * 2) CallMU at callsite
 * 3) RetMU at function return
 */
template<class Cond>
class MSSAMU
{

public:
    enum MUTYPE
    {
        LoadMSSAMU, CallMSSAMU, RetMSSAMU
    };

protected:
    MUTYPE type;
    const MemRegion* mr;
    MRVer* ver;
    Cond cond;
public:
    /// Constructor/Destructor for MU
    //@{
    MSSAMU(MUTYPE t, const MemRegion* m, Cond c) : type(t), mr(m), ver(nullptr), cond(c)
    {
    }
    virtual ~MSSAMU()
    {
    }
    //@}

    /// Return MR
    inline const MemRegion* getMR() const
    {
        return mr;
    }
    /// Return type
    inline MUTYPE getType() const
    {
        return type;
    }
    /// Set Ver
    inline void setVer(MRVer* v)
    {
        assert(v->getMR() == mr && "inserting different memory region?");
        ver = v;
    }
    /// Get Ver
    inline MRVer* getMRVer() const
    {
        assert(ver!=nullptr && "version is nullptr, did not rename?");
        return ver;
    }
    /// Return condition
    inline Cond getCond() const
    {
        return cond;
    }

    /// Avoid adding duplicated mus
    inline bool operator < (const MSSAMU & rhs) const
    {
        return mr > rhs.getMR();
    }
    /// Print MU
    virtual void dump()
    {
        SVFUtil::outs() << "MU(MR_" << mr->getMRID() << "V_" << ver->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};

/*!
 * LoadMU is annotated at each load instruction, representing a memory object is read here
 */
template<class Cond>
class LoadMU : public MSSAMU<Cond>
{

private:
    const LoadStmt* inst;
    const SVFBasicBlock* bb;

public:
    /// Constructor/Destructor for MU
    //@{
    LoadMU(const SVFBasicBlock* b,const LoadStmt* i, const MemRegion* m, Cond c = true) :
        MSSAMU<Cond>(MSSAMU<Cond>::LoadMSSAMU,m,c), inst(i), bb(b)
    {
    }
    virtual ~LoadMU()
    {

    }
    //@}

    /// Return load instruction
    inline const LoadStmt* getLoadStmt() const
    {
        return inst;
    }

    /// Return basic block
    inline const SVFBasicBlock* getBasicBlock() const
    {
        return bb;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadMU *)
    {
        return true;
    }
    static inline bool classof(const MSSAMU<Cond> *mu)
    {
        return mu->getType() == MSSAMU<Cond>::LoadMSSAMU;
    }
    //@}

    /// Print MU
    virtual void dump()
    {
        SVFUtil::outs() << "LDMU(MR_" << this->getMR()->getMRID() << "V_" << this->getMRVer()->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};

/*!
 * CallMU is annotated at callsite, representing a memory object is indirect read by callee
 */
template<class Cond>
class CallMU : public MSSAMU<Cond>
{

private:
    const CallICFGNode* callsite;

public:
    /// Constructor/Destructor for MU
    //@{
    CallMU(const CallICFGNode* cs, const MemRegion* m, Cond c = true) :
        MSSAMU<Cond>(MSSAMU<Cond>::CallMSSAMU,m,c), callsite(cs)
    {
    }
    virtual ~CallMU()
    {

    }
    //@}

    /// Return callsite
    inline const CallICFGNode* getCallSite() const
    {
        return callsite;
    }

    /// Return basic block
    inline const SVFBasicBlock* getBasicBlock() const
    {
        return callsite->getCallSite()->getParent();
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallMU *)
    {
        return true;
    }
    static inline bool classof(const MSSAMU<Cond> *mu)
    {
        return mu->getType() == MSSAMU<Cond>::CallMSSAMU;
    }
    //@}

    /// Print MU
    virtual void dump()
    {
        SVFUtil::outs() << "CALMU(MR_" << this->getMR()->getMRID() << "V_" << this->getMRVer()->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};


/*!
 * RetMU is annotated at function return, representing memory objects returns to callers
 */
template<class Cond>
class RetMU : public MSSAMU<Cond>
{
private:
    const SVFFunction* fun;
public:
    /// Constructor/Destructor for MU
    //@{
    RetMU(const SVFFunction* f, const MemRegion* m, Cond c = true) :
        MSSAMU<Cond>(MSSAMU<Cond>::RetMSSAMU,m,c), fun(f)
    {
    }
    virtual ~RetMU() {}
    //@}

    /// Return function
    inline const SVFFunction* getFunction() const
    {
        return fun;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetMU *)
    {
        return true;
    }
    static inline bool classof(const MSSAMU<Cond> *mu)
    {
        return mu->getType() == MSSAMU<Cond>::RetMSSAMU;
    }
    //@}

    /// Print MU
    virtual void dump()
    {
        SVFUtil::outs() << "RETMU(MR_" << this->getMR()->getMRID() << "V_" << this->getMRVer()->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};


/*!
 * Indirect Memory Definition
 * 1) MSSACHI indirect memory object is modified
 *   a) StoreCHI definition at store
 *   b) EntryCHI definition at function entry
 * 2) MSSAPHI memory object is defined at joint points of a control flow
 */
class MSSADEF
{

public:
    enum DEFTYPE
    {
        SSACHI,
        StoreMSSACHI,
        CallMSSACHI,
        EntryMSSACHI,
        SSAPHI
    };

protected:
    DEFTYPE type;
    const MemRegion* mr;
    MRVer* resVer;

public:
    /// Constructor/Destructer for MSSADEF
    //@{
    MSSADEF(DEFTYPE t, const MemRegion* m): type(t), mr(m), resVer(nullptr)
    {

    }
    virtual ~MSSADEF() {}
    //@}

    /// Return memory region
    inline const MemRegion* getMR() const
    {
        return mr;
    }

    /// Return type of this CHI
    inline DEFTYPE getType() const
    {
        return type;
    }

    /// Set result operand ver
    inline void setResVer(MRVer* v)
    {
        assert(v->getMR() == mr && "inserting different memory region?");
        resVer = v;
    }

    /// Set operand vers
    inline MRVer* getResVer() const
    {
        assert(resVer!=nullptr && "version is nullptr, did not rename?");
        return resVer;
    }

    /// Avoid adding duplicated chis and phis
    inline bool operator < (const MSSADEF & rhs) const
    {
        return mr > rhs.getMR();
    }

    /// Print MSSADef
    virtual void dump()
    {
        SVFUtil::outs() << "DEF(MR_" << mr->getMRID() << "V_" << resVer->getSSAVersion() << ")\n";
    }
};

/*!
 * Indirect Memory Write
 */
template<class Cond>
class MSSACHI : public MSSADEF
{

private:
    MRVer* opVer;
    Cond cond;
public:
    typedef typename MSSADEF::DEFTYPE CHITYPE;
    /// Constructor/Destructer for MSSACHI
    //@{
    MSSACHI(CHITYPE t, const MemRegion* m, Cond c): MSSADEF(t,m), opVer(nullptr), cond(c)
    {

    }
    virtual ~MSSACHI() {}
    //@}

    /// Set operand ver
    inline void setOpVer(MRVer* v)
    {
        assert(v->getMR() == this->getMR() && "inserting different memory region?");
        opVer = v;
    }

    /// Get operand ver
    inline MRVer* getOpVer() const
    {
        assert(opVer!=nullptr && "version is nullptr, did not rename?");
        return opVer;
    }

    /// Get condition
    inline Cond getCond() const
    {
        return cond;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MSSACHI * chi)
    {
        return true;
    }
    static inline bool classof(const MSSADEF *chi)
    {
        return chi->getType() == MSSADEF::EntryMSSACHI ||
               chi->getType() == MSSADEF::StoreMSSACHI ||
               chi->getType() == MSSADEF::SSACHI ;
    }
    //@}

    /// Print CHI
    virtual void dump()
    {
        SVFUtil::outs() << "MR_" << this->getMR()->getMRID() << "V_" << this->getResVer()->getSSAVersion() <<
                        " = CHI(MR_" << this->getMR()->getMRID() << "V_" << opVer->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};

/*!
 *
 *  StoreCHI is annotated at each store instruction, representing a memory object is modified here
 */
template<class Cond>
class StoreCHI : public MSSACHI<Cond>
{
private:
    const SVFBasicBlock* bb;
    const StoreStmt* inst;
public:
    /// Constructors for StoreCHI
    //@{
    StoreCHI(const SVFBasicBlock* b, const StoreStmt* i, const MemRegion* m, Cond c = true) :
        MSSACHI<Cond>(MSSADEF::StoreMSSACHI,m,c), bb(b), inst(i)
    {
    }
    virtual ~StoreCHI()
    {
    }
    //@}

    /// Get basic block
    inline const SVFBasicBlock* getBasicBlock() const
    {
        return bb;
    }

    /// Get store instruction
    inline const StoreStmt* getStoreStmt() const
    {
        return inst;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreCHI * chi)
    {
        return true;
    }
    static inline bool classof(const MSSACHI<Cond> * chi)
    {
        return chi->getType() == MSSADEF::StoreMSSACHI;
    }
    static inline bool classof(const MSSADEF *chi)
    {
        return chi->getType() == MSSADEF::StoreMSSACHI;
    }
    //@}

    /// Print CHI
    virtual void dump()
    {
        SVFUtil::outs() << this->getMR()->getMRID() << "V_" << this->getResVer()->getSSAVersion() <<
                        " = STCHI(MR_" << this->getMR()->getMRID() << "V_" << this->getOpVer()->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};


/*!
 *
 *  StoreCHI is annotated at each store instruction, representing a memory object is modified here
 */
template<class Cond>
class CallCHI : public MSSACHI<Cond>
{
private:
    const CallICFGNode* callsite;
public:
    /// Constructors for StoreCHI
    //@{
    CallCHI(const CallICFGNode* cs, const MemRegion* m, Cond c = true) :
        MSSACHI<Cond>(MSSADEF::CallMSSACHI,m,c), callsite(cs)
    {
    }
    virtual ~CallCHI()
    {
    }
    //@}

    /// Return basic block
    inline const SVFBasicBlock* getBasicBlock() const
    {
        return callsite->getCallSite()->getParent();
    }

    /// Return callsite
    inline const CallICFGNode* getCallSite() const
    {
        return callsite;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallCHI * chi)
    {
        return true;
    }
    static inline bool classof(const MSSACHI<Cond> * chi)
    {
        return chi->getType() == MSSADEF::CallMSSACHI;
    }
    static inline bool classof(const MSSADEF *chi)
    {
        return chi->getType() == MSSADEF::CallMSSACHI;
    }
    //@}

    /// Print CHI
    virtual void dump()
    {
        SVFUtil::outs() << this->getMR()->getMRID() << "V_" << this->getResVer()->getSSAVersion() <<
                        " = CALCHI(MR_" << this->getMR()->getMRID() << "V_" << this->getOpVer()->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};

/*!
 * EntryCHI is annotated at function entry, representing receiving memory objects from callers
 */
template<class Cond>
class EntryCHI : public MSSACHI<Cond>
{
private:
    const SVFFunction* fun;
public:
    /// Constructors for EntryCHI
    //@{
    EntryCHI(const SVFFunction* f, const MemRegion* m, Cond c = true) :
        MSSACHI<Cond>(MSSADEF::EntryMSSACHI,m,c),fun(f)
    {
    }
    virtual ~EntryCHI()
    {
    }
    //@}

    /// Return function
    inline const SVFFunction* getFunction() const
    {
        return fun;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const EntryCHI * chi)
    {
        return true;
    }
    static inline bool classof(const MSSACHI<Cond> * chi)
    {
        return chi->getType() == MSSADEF::EntryMSSACHI;
    }
    static inline bool classof(const MSSADEF *chi)
    {
        return chi->getType() == MSSADEF::EntryMSSACHI;
    }
    //@}

    /// Print CHI
    virtual void dump()
    {
        SVFUtil::outs() << this->getMR()->getMRID() << "V_" << this->getResVer()->getSSAVersion() <<
                        " = ENCHI(MR_" << this->getMR()->getMRID() << "V_" << this->getOpVer()->getSSAVersion() << ") \t" <<
                        this->getMR()->dumpStr() << "\n";
    }
};

/*
 * Memory SSA Select, similar to PHINode
 */
template<class Cond>
class MSSAPHI : public MSSADEF
{

public:
    typedef Map<u32_t,const MRVer*> OPVers;
private:
    const SVFBasicBlock* bb;
    OPVers opVers;
    Cond cond;
public:
    /// Constructors for PHI
    //@{
    MSSAPHI(const SVFBasicBlock* b, const MemRegion* m, Cond c = true) :
        MSSADEF(MSSADEF::SSAPHI,m), bb(b), cond(c)
    {
    }
    virtual ~MSSAPHI()
    {
    }
    //@}

    /// Set operand ver
    inline void setOpVer(const MRVer* v, u32_t pos)
    {
        assert(v->getMR() == this->getMR() && "inserting different memory region?");
        opVers[pos] = v;
    }

    /// Get operand ver
    inline const MRVer* getOpVer(u32_t pos) const
    {
        OPVers::const_iterator it = opVers.find(pos);
        assert(it!=opVers.end() && "version is nullptr, did not rename?");
        return it->second;
    }

    /// Get the number of operand ver
    inline u32_t getOpVerNum() const
    {
        return opVers.size();
    }

    /// Operand ver iterators
    //@{
    inline OPVers::const_iterator opVerBegin() const
    {
        return opVers.begin();
    }
    inline OPVers::const_iterator opVerEnd() const
    {
        return opVers.end();
    }
    //@}

    /// Return the basic block
    inline const SVFBasicBlock* getBasicBlock() const
    {
        return bb;
    }

    /// Return condition
    inline Cond getCond() const
    {
        return cond;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MSSAPHI * phi)
    {
        return true;
    }
    static inline bool classof(const MSSADEF *phi)
    {
        return phi->getType() == MSSADEF::SSAPHI ;
    }
    //@}

    /// Print PHI
    virtual void dump()
    {
        SVFUtil::outs() << this->getMR()->getMRID() << "V_" << this->getResVer()->getSSAVersion() <<
                        " = PHI(";
        for(OPVers::iterator it = opVers.begin(), eit = opVers.end(); it!=eit; ++it)
            SVFUtil::outs() << "MR_" << this->getMR()->getMRID() << "V_" << it->second->getSSAVersion() << ", ";

        SVFUtil::outs() << ")\n";
    }
};

std::ostream& operator<<(std::ostream &o, const MRVer& mrver);
} // End namespace SVF

#endif /* MSSAMUCHI_H_ */
