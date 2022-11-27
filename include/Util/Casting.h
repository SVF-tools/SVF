/*
 * Casting.h
 *
 *  Created on: 19Sep.,2018
 *      Author: yulei
 */

#ifndef INCLUDE_UTIL_CASTING_H_
#define INCLUDE_UTIL_CASTING_H_


//===- llvm/Support/Casting.h - Allow flexible, checked, casts --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the SVFUtil::isa<X>(), cast<X>(), dyn_cast<X>(), cast_or_null<X>(),
// and dyn_cast_or_null<X>() templates.
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <memory>
#include <type_traits>

//===-- Part of llvm/Support/Compiler.h - Compiler abstraction support --*- C++ -*-===//

// Only use __has_cpp_attribute in C++ mode. GCC defines __has_cpp_attribute in
// C mode, but the :: in __has_cpp_attribute(scoped::attribute) is invalid.
#ifndef LLVM_HAS_CPP_ATTRIBUTE
#if defined(__cplusplus) && defined(__has_cpp_attribute)
# define LLVM_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
# define LLVM_HAS_CPP_ATTRIBUTE(x) 0
#endif
#endif

/// LLVM_NODISCARD - Warn if a type or return value is discarded.

// Use the 'nodiscard' attribute in C++17 or newer mode.
#if defined(__cplusplus) && __cplusplus > 201402L && LLVM_HAS_CPP_ATTRIBUTE(nodiscard)
#define LLVM_NODISCARD [[nodiscard]]
#elif LLVM_HAS_CPP_ATTRIBUTE(clang::warn_unused_result)
#define LLVM_NODISCARD [[clang::warn_unused_result]]
// Clang in C++14 mode claims that it has the 'nodiscard' attribute, but also
// warns in the pedantic mode that 'nodiscard' is a C++17 extension (PR33518).
// Use the 'nodiscard' attribute in C++14 mode only with GCC.
// TODO: remove this workaround when PR33518 is resolved.
#elif defined(__GNUC__) && LLVM_HAS_CPP_ATTRIBUTE(nodiscard)
#define LLVM_NODISCARD [[nodiscard]]
#else
#define LLVM_NODISCARD
#endif

namespace SVF
{

namespace SVFUtil
{

//===- Part of llvm/Support/type_traits.h - Simplfied type traits -------*- C++ -*-===//

/// If T is a pointer to X, return a pointer to const X. If it is not,
/// return const T.
template<typename T, typename Enable = void>
struct add_const_past_pointer
{
    using type = const T;
};

template <typename T>
struct add_const_past_pointer<T, std::enable_if_t<std::is_pointer<T>::value>>
{
    using type = const std::remove_pointer_t<T> *;
};

/// If T is a pointer, just return it. If it is not, return T&.
template<typename T, typename Enable = void>
struct add_lvalue_reference_if_not_pointer
{
    using type = T &;
};

template <typename T>
struct add_lvalue_reference_if_not_pointer<
    T, std::enable_if_t<std::is_pointer<T>::value>>
{
    using type = T;
};

//===----------------------------------------------------------------------===//
//                          SVFUtil::isa<x> Support Templates
//===----------------------------------------------------------------------===//

// Define a template that can be specialized by smart pointers to reflect the
// fact that they are automatically dereferenced, and are not involved with the
// template selection process...  the default implementation is a noop.
//
template<typename From> struct simplify_type
{
    using SimpleType = From; // The real type this represents...

    // An accessor to get the real value...
    static SimpleType &getSimplifiedValue(From &Val)
    {
        return Val;
    }
};

template<typename From> struct simplify_type<const From>
{
    using NonConstSimpleType = typename simplify_type<From>::SimpleType;
    using SimpleType =
        typename SVFUtil::add_const_past_pointer<NonConstSimpleType>::type;
    using RetType =
        typename SVFUtil::add_lvalue_reference_if_not_pointer<SimpleType>::type;

    static RetType getSimplifiedValue(const From& Val)
    {
        return simplify_type<From>::getSimplifiedValue(const_cast<From&>(Val));
    }
};

// The core of the implementation of SVFUtil::isa<X> is here; To and From should be
// the names of classes.  This template can be specialized to customize the
// implementation of SVFUtil::isa<> without rewriting it from scratch.
template <typename To, typename From, typename Enabler = void>
struct isa_impl
{
    static inline bool doit(const From &Val)
    {
        return To::classof(&Val);
    }
};

/// \brief Always allow upcasts, and perform no dynamic check for them.
template <typename To, typename From>
struct isa_impl<To, From, std::enable_if_t<std::is_base_of<To, From>::value>>
{
    static inline bool doit(const From &)
    {
        return true;
    }
};

template <typename To, typename From> struct isa_impl_cl
{
    static inline bool doit(const From &Val)
    {
        return isa_impl<To, From>::doit(Val);
    }
};

template <typename To, typename From> struct isa_impl_cl<To, const From>
{
    static inline bool doit(const From &Val)
    {
        return isa_impl<To, From>::doit(Val);
    }
};

template <typename To, typename From>
struct isa_impl_cl<To, const std::unique_ptr<From>>
{
    static inline bool doit(const std::unique_ptr<From> &Val)
    {
        assert(Val && "SVFUtil::isa<> used on a null pointer");
        return isa_impl_cl<To, From>::doit(*Val);
    }
};

template <typename To, typename From> struct isa_impl_cl<To, From*>
{
    static inline bool doit(const From *Val)
    {
        assert(Val && "SVFUtil::isa<> used on a null pointer");
        return isa_impl<To, From>::doit(*Val);
    }
};

template <typename To, typename From> struct isa_impl_cl<To, From*const>
{
    static inline bool doit(const From *Val)
    {
        assert(Val && "SVFUtil::isa<> used on a null pointer");
        return isa_impl<To, From>::doit(*Val);
    }
};

template <typename To, typename From> struct isa_impl_cl<To, const From*>
{
    static inline bool doit(const From *Val)
    {
        assert(Val && "SVFUtil::isa<> used on a null pointer");
        return isa_impl<To, From>::doit(*Val);
    }
};

template <typename To, typename From> struct isa_impl_cl<To, const From*const>
{
    static inline bool doit(const From *Val)
    {
        assert(Val && "SVFUtil::isa<> used on a null pointer");
        return isa_impl<To, From>::doit(*Val);
    }
};

template<typename To, typename From, typename SimpleFrom>
struct isa_impl_wrap
{
    // When From != SimplifiedType, we can simplify the type some more by using
    // the simplify_type template.
    static bool doit(const From &Val)
    {
        return isa_impl_wrap<To, SimpleFrom,
               typename simplify_type<SimpleFrom>::SimpleType>::doit(
                   simplify_type<const From>::getSimplifiedValue(Val));
    }
};

template<typename To, typename FromTy>
struct isa_impl_wrap<To, FromTy, FromTy>
{
    // When From == SimpleType, we are as simple as we are going to get.
    static bool doit(const FromTy &Val)
    {
        return isa_impl_cl<To,FromTy>::doit(Val);
    }
};

// SVFUtil::isa<X> - Return true if the parameter to the template is an instance of the
// template type argument.  Used like this:
//
//  if (SVFUtil::isa<Type>(myVal)) { ... }
//  if (SVFUtil::isa<Type0, Type1, Type2>(myVal)) { ... }
//
template <class X, class Y> LLVM_NODISCARD inline bool isa(const Y &Val)
{
    return isa_impl_wrap<X, const Y,
           typename simplify_type<const Y>::SimpleType>::doit(Val);
}

template <typename First, typename Second, typename... Rest, typename Y>
LLVM_NODISCARD inline bool isa(const Y &Val)
{
    return SVFUtil::isa<First>(Val) || SVFUtil::isa<Second, Rest...>(Val);
}

//===----------------------------------------------------------------------===//
//                          cast<x> Support Templates
//===----------------------------------------------------------------------===//

template<class To, class From> struct cast_retty;

// Calculate what type the 'cast' function should return, based on a requested
// type of To and a source type of From.
template<class To, class From> struct cast_retty_impl
{
    using ret_type = To &;       // Normal case, return Ty&
};
template<class To, class From> struct cast_retty_impl<To, const From>
{
    using ret_type = const To &; // Normal case, return Ty&
};

template<class To, class From> struct cast_retty_impl<To, From*>
{
    using ret_type = To *;       // Pointer arg case, return Ty*
};

template<class To, class From> struct cast_retty_impl<To, const From*>
{
    using ret_type = const To *; // Constant pointer arg case, return const Ty*
};

template<class To, class From> struct cast_retty_impl<To, const From*const>
{
    using ret_type = const To *; // Constant pointer arg case, return const Ty*
};

template <class To, class From>
struct cast_retty_impl<To, std::unique_ptr<From>>
{
private:
    using PointerType = typename cast_retty_impl<To, From *>::ret_type;
    using ResultType = typename std::remove_pointer<PointerType>::type;

public:
    using ret_type = std::unique_ptr<ResultType>;
};

template<class To, class From, class SimpleFrom>
struct cast_retty_wrap
{
    // When the simplified type and the from type are not the same, use the type
    // simplifier to reduce the type, then reuse cast_retty_impl to get the
    // resultant type.
    using ret_type = typename cast_retty<To, SimpleFrom>::ret_type;
};

template<class To, class FromTy>
struct cast_retty_wrap<To, FromTy, FromTy>
{
    // When the simplified type is equal to the from type, use it directly.
    using ret_type = typename cast_retty_impl<To,FromTy>::ret_type;
};

template<class To, class From>
struct cast_retty
{
    using ret_type = typename cast_retty_wrap<
                     To, From, typename simplify_type<From>::SimpleType>::ret_type;
};

// Ensure the non-simple values are converted using the simplify_type template
// that may be specialized by smart pointers...
//
template<class To, class From, class SimpleFrom> struct cast_convert_val
{
    // This is not a simple type, use the template to simplify it...
    static typename cast_retty<To, From>::ret_type doit(From &Val)
    {
        return cast_convert_val<To, SimpleFrom,
               typename simplify_type<SimpleFrom>::SimpleType>::doit(
                   simplify_type<From>::getSimplifiedValue(Val));
    }
};

template<class To, class FromTy> struct cast_convert_val<To,FromTy,FromTy>
{
    // This _is_ a simple type, just cast it.
    static typename cast_retty<To, FromTy>::ret_type doit(const FromTy &Val)
    {
        typename cast_retty<To, FromTy>::ret_type Res2
            = (typename cast_retty<To, FromTy>::ret_type)const_cast<FromTy&>(Val);
        return Res2;
    }
};

template <class X> struct is_simple_type
{
    static const bool value =
        std::is_same<X, typename simplify_type<X>::SimpleType>::value;
};

// cast<X> - Return the argument parameter cast to the specified type.  This
// casting operator asserts that the type is correct, so it does not return null
// on failure.  It does not allow a null argument (use cast_or_null for that).
// It is typically used like this:
//
//  cast<Instruction>(myVal)->getParent()
//
template <class X, class Y>
inline std::enable_if_t<!is_simple_type<Y>::value,
       typename cast_retty<X, const Y>::ret_type>
       cast(const Y &Val)
{
    assert(SVFUtil::isa<X>(Val) && "cast<Ty>() argument of incompatible type!");
    return cast_convert_val<
           X, const Y, typename simplify_type<const Y>::SimpleType>::doit(Val);
}

template <class X, class Y>
inline typename cast_retty<X, Y>::ret_type cast(Y &Val)
{
    assert(SVFUtil::isa<X>(Val) && "cast<Ty>() argument of incompatible type!");
    return cast_convert_val<X, Y,
           typename simplify_type<Y>::SimpleType>::doit(Val);
}

template <class X, class Y>
inline typename cast_retty<X, Y *>::ret_type cast(Y *Val)
{
    assert(SVFUtil::isa<X>(Val) && "cast<Ty>() argument of incompatible type!");
    return cast_convert_val<X, Y*,
           typename simplify_type<Y*>::SimpleType>::doit(Val);
}

template <class X, class Y>
inline typename cast_retty<X, std::unique_ptr<Y>>::ret_type
        cast(std::unique_ptr<Y> &&Val)
{
    assert(SVFUtil::isa<X>(Val.get()) && "cast<Ty>() argument of incompatible type!");
    using ret_type = typename cast_retty<X, std::unique_ptr<Y>>::ret_type;
    return ret_type(
               cast_convert_val<X, Y *, typename simplify_type<Y *>::SimpleType>::doit(
                   Val.release()));
}

// dyn_cast<X> - Return the argument parameter cast to the specified type.  This
// casting operator returns null if the argument is of the wrong type, so it can
// be used to test for a type as well as cast if successful.  This should be
// used in the context of an if statement like this:
//
//  if (const Instruction* I = dyn_cast<Instruction>(myVal)) { ... }
//

template <class X, class Y>
LLVM_NODISCARD inline std::enable_if_t<
!is_simple_type<Y>::value, typename cast_retty<X, const Y>::ret_type>
dyn_cast(const Y &Val)
{
    return SVFUtil::isa<X>(Val) ? SVFUtil::cast<X>(Val) : nullptr;
}

template <class X, class Y>
LLVM_NODISCARD inline typename cast_retty<X, Y>::ret_type dyn_cast(Y &Val)
{
    return SVFUtil::isa<X>(Val) ? SVFUtil::cast<X>(Val) : nullptr;
}

template <class X, class Y>
LLVM_NODISCARD inline typename cast_retty<X, Y *>::ret_type dyn_cast(Y *Val)
{
    return SVFUtil::isa<X>(Val) ? SVFUtil::cast<X>(Val) : nullptr;
}

} // End namespace SVFUtil

} // End namespace SVF

#endif /* INCLUDE_UTIL_CASTING_H_ */
