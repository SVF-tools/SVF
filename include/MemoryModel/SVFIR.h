//===- SVFIR.h -- IR of SVF ---------------------------------------------//
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
 * SVFIR.h
 *
 *  Created on: 31, 12, 2021
 *      Author: Yulei Sui
 */

#ifndef SVFIR_H_
#define SVFIR_H_

#include <set>
#include <map>
#include "Util/BasicTypes.h"

namespace SVF
{

class SVFStmt{

public:
    enum SVFSTMTK
    {
        Alloc, Copy, Phi, Store, Load, Call, FunRet, Gep, Cmp, Binary, Unary, Branch
    };

    SVFStmt(u32_t k): kind(k){

    }

    /// ClassOf
    //@{
    static inline bool classof(const SVFStmt*)
    {
        return true;
    }
    ///@}
    virtual const std::string toString() const;
    //@}
    
    /// Overloading operator << for printing
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const SVFStmt &stmt)
    {
        o << stmt.toString();
        return o;
    }
    //@}

    inline u32_t getKind() const {
        return kind;
    }

private:
    u32_t kind;
};

class AllocStmt : public SVFStmt{

public:

    AllocStmt(SymID src, SymID dst) : SVFStmt(SVFSTMTK::Alloc), srcVar(src), dstVar(dst){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AllocStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Alloc;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID srcVar;
    SymID dstVar;
};

class CopyStmt : public SVFStmt{

    CopyStmt(SymID src, SymID dst) : SVFStmt(SVFSTMTK::Copy), srcVar(src), dstVar(dst){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Copy;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID srcVar;
    SymID dstVar;
};

class PhiStmt : public SVFStmt{

    PhiStmt(SymID res, SymID op1, SymID op2) : SVFStmt(SVFSTMTK::Phi), resVar(res), op1Var(op1), op2Var(op2){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const PhiStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Phi;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID resVar;
    SymID op1Var;
    SymID op2Var;
};

class LoadStmt : public SVFStmt{

    LoadStmt(SymID src, SymID dst) : SVFStmt(SVFSTMTK::Load), srcVar(src), dstVar(dst){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Load;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID srcVar;
    SymID dstVar;
};

class StoreStmt : public SVFStmt{

    StoreStmt(SymID src, SymID dst) : SVFStmt(SVFSTMTK::Store), srcVar(src), dstVar(dst){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Store;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID srcVar;
    SymID dstVar;
};

class GepStmt : public SVFStmt{

    GepStmt(SymID res, SymID ptr, SymID offset) : SVFStmt(SVFSTMTK::Gep), resVar(res), ptrVar(ptr), offsetVar(offset){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Gep;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID resVar;
    SymID ptrVar;
    SymID offsetVar;
};

class CmpStmt : public SVFStmt{

    CmpStmt(SymID res, SymID op1, SymID op2) : SVFStmt(SVFSTMTK::Cmp), resVar(res), op1Var(op1), op2Var(op2){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CmpStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Cmp;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID resVar;
    SymID op1Var;
    SymID op2Var;
};

class BinaryStmt : public SVFStmt{

    BinaryStmt(SymID res, SymID op1, SymID op2) : SVFStmt(SVFSTMTK::Binary), resVar(res), op1Var(op1), op2Var(op2){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BinaryStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Binary;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID resVar;
    SymID op1Var;
    SymID op2Var;
};

class UnaryStmt : public SVFStmt{

    UnaryStmt(SymID res, SymID op) : SVFStmt(SVFSTMTK::Unary), resVar(res), opVar(op){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const UnaryStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Unary;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID resVar;
    SymID opVar;
};

class CallStmt : public SVFStmt{

    CallStmt(CallSite cs) : SVFStmt(SVFSTMTK::Call), callsite(cs){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Call;
    }
    //@}

    virtual const std::string toString() const;

private:
    CallSite callsite;
};

class FunRetStmt : public SVFStmt{

    FunRetStmt(SVFFunction* f) : SVFStmt(SVFSTMTK::FunRet), fun(f){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunRetStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::FunRet;
    }
    //@}

    virtual const std::string toString() const;

private:
    SVFFunction* fun;
};

class BranchStmt : public SVFStmt{

    BranchStmt(SymID cond, SVFStmt* b1, SVFStmt* b2) : SVFStmt(SVFSTMTK::Branch), condition(cond),br1(b1),br2(b2){
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BranchStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *stmt)
    {
        return stmt->getKind() == SVFSTMTK::Branch;
    }
    //@}

    virtual const std::string toString() const;

private:
    SymID condition;
    SVFStmt* br1;
    SVFStmt* br2;
};


class SVFIR{


};

} // End namespace SVF

#endif /* SVFIR_H_ */
