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

namespace llvm {

  template<typename ItTy = User::const_op_iterator> 
    class generic_bridge_gep_type_iterator : public std::iterator<std::forward_iterator_tag, Type *, ptrdiff_t> {
    
    typedef std::iterator<std::forward_iterator_tag,Type *, ptrdiff_t> super;
    ItTy OpIt;
    PointerIntPair<Type *,1> CurTy;
    unsigned AddrSpace;
    generic_bridge_gep_type_iterator() {}
  public:
    
    static generic_bridge_gep_type_iterator begin(Type *Ty, ItTy It) {
      generic_bridge_gep_type_iterator I;
      I.CurTy.setPointer(Ty);
      I.OpIt = It;
      return I;
    }

    static generic_bridge_gep_type_iterator begin(Type *Ty, unsigned AddrSpace,
				       ItTy It) {
      generic_bridge_gep_type_iterator I;
      I.CurTy.setPointer(Ty);
      I.CurTy.setInt(true);
      I.AddrSpace = AddrSpace;
      I.OpIt = It;
      return I;
    }

    static generic_bridge_gep_type_iterator end(ItTy It) {
      generic_bridge_gep_type_iterator I;
      I.OpIt = It;
      return I;
    }

    bool operator==(const generic_bridge_gep_type_iterator& x) const {
      return OpIt == x.OpIt;
    }

    bool operator!=(const generic_bridge_gep_type_iterator& x) const {
      return !operator==(x);
    }

    Type *operator*() const {
      if ( CurTy.getInt() )
	return CurTy.getPointer()->getPointerTo(AddrSpace);
      return CurTy.getPointer();
    }

    Type *getIndexedType() const {
      if ( CurTy.getInt() )
	return CurTy.getPointer();
      CompositeType *CT = cast<CompositeType>( CurTy.getPointer() );
      return CT->getTypeAtIndex(getOperand());
    }

    // non-standard operators, these may not need be bridged but seems it's 
    // predunt to do so...
    Type *operator->() const { return operator*(); }

    Value *getOperand() const { return const_cast<Value*>(&**OpIt); }

 
    generic_bridge_gep_type_iterator& operator++() {
      if ( CurTy.getInt() ) {
	CurTy.setInt(false);
      } else if ( CompositeType * CT = dyn_cast<CompositeType>(CurTy.getPointer()) ) {
	CurTy.setPointer(CT->getTypeAtIndex(getOperand()));
      } else {
	CurTy.setPointer(nullptr);
      }
      ++OpIt;
      return *this;
    }

     
    generic_bridge_gep_type_iterator operator++(int) {
      generic_bridge_gep_type_iterator tmp = *this; ++*this; return tmp;
    }

  };

  
  typedef generic_bridge_gep_type_iterator<> bridge_gep_iterator;

  inline bridge_gep_iterator bridge_gep_begin(const User* GEP) {
    auto *GEPOp = cast<GEPOperator>(GEP);
    return bridge_gep_iterator::begin(GEPOp->getSourceElementType(),
					cast<PointerType>(GEPOp->getPointerOperandType()->getScalarType())->getAddressSpace(),
					GEP->op_begin() + 1);
  }

  inline bridge_gep_iterator bridge_gep_end(const User* GEP) {
    return bridge_gep_iterator::end(GEP->op_end());
  }

  inline bridge_gep_iterator bridge_gep_begin(const User &GEP) {
    auto &GEPOp = cast<GEPOperator>(GEP);
    return bridge_gep_iterator::begin( GEPOp.getSourceElementType(),
				    cast<PointerType>(GEPOp.getPointerOperandType()->getScalarType())->getAddressSpace(),
				    GEP.op_begin() + 1);
  }

  inline bridge_gep_iterator bridge_gep_end(const User &GEP) {
    return bridge_gep_iterator::end(GEP.op_end());
  }

  template<typename T>
    inline generic_bridge_gep_type_iterator<const T*> bridge_gep_end( Type * /*Op0*/, ArrayRef<T> A ) {
    return generic_bridge_gep_type_iterator<const T*>::end(A.end());
  }

}

#endif
