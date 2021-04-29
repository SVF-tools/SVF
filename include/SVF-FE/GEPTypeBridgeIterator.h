// GEPTypeBridgeIterator
//
//
#ifndef SVF_GEPTYPEBRIDGEITERATOR_H
#define SVF_GEPTYPEBRIDGEITERATOR_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/User.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"

namespace llvm
{

template<typename ItTy = User::const_op_iterator>
class generic_bridge_gep_type_iterator : public std::iterator<std::forward_iterator_tag, Type *, ptrdiff_t>
{

    typedef std::iterator<std::forward_iterator_tag,Type *, ptrdiff_t> super;
    ItTy OpIt;
    PointerIntPair<Type *,1> CurTy;
    unsigned AddrSpace;
    generic_bridge_gep_type_iterator() {}
public:

    static generic_bridge_gep_type_iterator begin(Type *Ty, ItTy It)
    {
        generic_bridge_gep_type_iterator I;
        I.CurTy.setPointer(Ty);
        I.OpIt = It;
        return I;
    }

    static generic_bridge_gep_type_iterator begin(Type *Ty, unsigned AddrSpace,
            ItTy It)
    {
        generic_bridge_gep_type_iterator I;
        I.CurTy.setPointer(Ty);
        I.CurTy.setInt(true);
        I.AddrSpace = AddrSpace;
        I.OpIt = It;
        return I;
    }

    static generic_bridge_gep_type_iterator end(ItTy It)
    {
        generic_bridge_gep_type_iterator I;
        I.OpIt = It;
        return I;
    }

    bool operator==(const generic_bridge_gep_type_iterator& x) const
    {
        return OpIt == x.OpIt;
    }

    bool operator!=(const generic_bridge_gep_type_iterator& x) const
    {
        return !operator==(x);
    }

    Type *operator*() const
    {
        if ( CurTy.getInt() )
            return CurTy.getPointer()->getPointerTo(AddrSpace);
        return CurTy.getPointer();
    }

    Type *getIndexedType() const
    {
        assert(false && "needs to be refactored");
        if ( CurTy.getInt() )
            return CurTy.getPointer();
#if LLVM_VERSION_MAJOR >= 11
        Type * CT = CurTy.getPointer();
        if (auto ST = dyn_cast<StructType>(CT))
            return ST->getTypeAtIndex(getOperand());
        else if (auto Array = dyn_cast<ArrayType>(CT))
            return Array->getElementType();
        else if (auto Vector = dyn_cast<VectorType>(CT))
            return Vector->getElementType();
        else
            return CT;
#else
        CompositeType *CT = llvm::cast<CompositeType>( CurTy.getPointer() );
        return CT->getTypeAtIndex(getOperand());
#endif
    }

    // non-standard operators, these may not need be bridged but seems it's
    // predunt to do so...
    Type *operator->() const
    {
        return operator*();
    }

    Value *getOperand() const
    {
        return const_cast<Value*>(&**OpIt);
    }


    generic_bridge_gep_type_iterator& operator++()
    {
        if ( CurTy.getInt() )
        {
            CurTy.setInt(false);
        }
#if LLVM_VERSION_MAJOR >= 11
        else if ( Type * CT = CurTy.getPointer() )
        {
            if (auto ST = dyn_cast<StructType>(CT))
                CurTy.setPointer(ST->getTypeAtIndex(getOperand()));
            else if (auto Array = dyn_cast<ArrayType>(CT))
                CurTy.setPointer(Array->getElementType());
            else if (auto Vector = dyn_cast<VectorType>(CT))
                CurTy.setPointer(Vector->getElementType());
            else
                CurTy.setPointer(nullptr);
        }
#else
        else if ( CompositeType * CT = dyn_cast<CompositeType>(CurTy.getPointer()) )
        {
            CurTy.setPointer(CT->getTypeAtIndex(getOperand()));
        }
#endif
        else
        {
            CurTy.setPointer(nullptr);
        }
        ++OpIt;
        return *this;
    }


    generic_bridge_gep_type_iterator operator++(int)
    {
        generic_bridge_gep_type_iterator tmp = *this;
        ++*this;
        return tmp;
    }

};


typedef generic_bridge_gep_type_iterator<> bridge_gep_iterator;

inline bridge_gep_iterator bridge_gep_begin(const User* GEP)
{
    auto *GEPOp = llvm::cast<GEPOperator>(GEP);
    return bridge_gep_iterator::begin(GEPOp->getSourceElementType(),
                                      llvm::cast<PointerType>(GEPOp->getPointerOperandType()->getScalarType())->getAddressSpace(),
                                      GEP->op_begin() + 1);
}

inline bridge_gep_iterator bridge_gep_end(const User* GEP)
{
    return bridge_gep_iterator::end(GEP->op_end());
}

inline bridge_gep_iterator bridge_gep_begin(const User &GEP)
{
    auto &GEPOp = llvm::cast<GEPOperator>(GEP);
    return bridge_gep_iterator::begin( GEPOp.getSourceElementType(),
                                       llvm::cast<PointerType>(GEPOp.getPointerOperandType()->getScalarType())->getAddressSpace(),
                                       GEP.op_begin() + 1);
}

inline bridge_gep_iterator bridge_gep_end(const User &GEP)
{
    return bridge_gep_iterator::end(GEP.op_end());
}

template<typename T>
inline generic_bridge_gep_type_iterator<const T*> bridge_gep_end( Type * /*Op0*/, ArrayRef<T> A )
{
    return generic_bridge_gep_type_iterator<const T*>::end(A.end());
}

} // End namespace llvm

#endif
