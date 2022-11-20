//===- SVFBasicTypes.h -- Basic types used in SVF-------------------------------//
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
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */


#ifndef INCLUDE_UTIL_SVFBASICTYPES_H_
#define INCLUDE_UTIL_SVFBASICTYPES_H_

#include <llvm/Support/CommandLine.h>	// for command line options
#include <llvm/IR/Instructions.h>

#include <Util/SparseBitVector.h>

#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <stack>
#include <deque>

namespace SVF
{

typedef llvm::Type Type;

typedef std::ostream OutStream;

typedef unsigned u32_t;
typedef signed s32_t;
typedef unsigned long long u64_t;
typedef signed long long s64_t;

typedef u32_t NodeID;
typedef u32_t EdgeID;
typedef unsigned SymID;
typedef unsigned CallSiteID;
typedef unsigned ThreadID;

typedef SparseBitVector<> NodeBS;
typedef unsigned PointsToID;

/// provide extra hash function for std::pair handling
template <class T> struct Hash;

template <class S, class T> struct Hash<std::pair<S, T>>
{
    // Pairing function from: http://szudzik.com/ElegantPairing.pdf
    static size_t szudzik(size_t a, size_t b)
    {
        return a > b ? b * b + a : a * a + a + b;
    }

    size_t operator()(const std::pair<S, T> &t) const
    {
        Hash<decltype(t.first)> first;
        Hash<decltype(t.second)> second;
        return szudzik(first(t.first), second(t.second));
    }
};

template <class T> struct Hash
{
    size_t operator()(const T &t) const
    {
        std::hash<T> h;
        return h(t);
    }
};

template <typename Key, typename Hash = Hash<Key>, typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
using Set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;

template<typename Key, typename Value, typename Hash = Hash<Key>,
         typename KeyEqual = std::equal_to<Key>,
         typename Allocator = std::allocator<std::pair<const Key, Value>>>
using Map = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

template<typename Key, typename Compare = std::less<Key>, typename Allocator = std::allocator<Key>>
using OrderedSet = std::set<Key, Compare, Allocator>;

template<typename Key, typename Value, typename Compare = std::less<Key>,
typename Allocator = std::allocator<std::pair<const Key, Value>>>
using OrderedMap = std::map<Key, Value, Compare, Allocator>;

typedef std::pair<NodeID, NodeID> NodePair;
typedef OrderedSet<NodeID> OrderedNodeSet;
typedef Set<NodeID> NodeSet;
typedef Set<NodePair> NodePairSet;
typedef Map<NodePair,NodeID> NodePairMap;
typedef std::vector<NodeID> NodeVector;
typedef std::vector<EdgeID> EdgeVector;
typedef std::stack<NodeID> NodeStack;
typedef std::list<NodeID> NodeList;
typedef std::deque<NodeID> NodeDeque;
typedef NodeSet EdgeSet;
typedef std::vector<u32_t> CallStrCxt;
typedef unsigned Version;
typedef Set<Version> VersionSet;
typedef std::pair<NodeID, Version> VersionedVar;
typedef Set<VersionedVar> VersionedVarSet;

class SVFType;
class SVFPointerType;

/*!
 * Flatterned type information of StructType, ArrayType and SingleValueType
 */
class StInfo
{

private:
    /// flattened field indices of a struct (ignoring arrays)
    std::vector<u32_t> fldIdxVec;
    /// flattened element indices including structs and arrays by considering strides
    std::vector<u32_t> elemIdxVec;
    /// Types of all fields of a struct
    Map<u32_t, const SVFType*> fldIdx2TypeMap;
    /// All field infos after flattening a struct
    std::vector<const SVFType*> finfo;
    /// stride represents the number of repetitive elements if this StInfo represent an ArrayType. stride is 1 by default.
    u32_t stride;
    /// number of elements after flattenning (including array elements)
    u32_t numOfFlattenElements;
    /// number of fields after flattenning (ignoring array elements)
    u32_t numOfFlattenFields;
    /// Type vector of fields
    std::vector<const SVFType*> flattenElementTypes;
    /// Max field limit

    StInfo(); ///< place holder
    StInfo(const StInfo& st); ///< place holder
    void operator=(const StInfo&); ///< place holder

public:
    /// Constructor
    StInfo(u32_t s) : stride(s), numOfFlattenElements(s), numOfFlattenFields(s)
    {
    }
    /// Destructor
    ~StInfo()
    {
    }

    ///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
    ///  OriginalFieldType of b with field_idx 1 : Struct A
    ///  FlatternedFieldType of b with field_idx 1 : int
    //{@
    const SVFType* getOriginalElemType(u32_t fldIdx) const;

    inline std::vector<u32_t>& getFlattenedFieldIdxVec()
    {
        return fldIdxVec;
    }
    inline std::vector<u32_t>& getFlattenedElemIdxVec()
    {
        return elemIdxVec;
    }
    inline std::vector<const SVFType*>& getFlattenElementTypes()
    {
        return flattenElementTypes;
    }
    inline std::vector<const SVFType*>& getFlattenFieldTypes()
    {
        return finfo;
    }
    inline const std::vector<u32_t>& getFlattenedFieldIdxVec() const
    {
        return fldIdxVec;
    }
    inline const std::vector<u32_t>& getFlattenedElemIdxVec() const
    {
        return elemIdxVec;
    }
    inline const std::vector<const SVFType*>& getFlattenElementTypes() const
    {
        return flattenElementTypes;
    }
    inline const std::vector<const SVFType*>& getFlattenFieldTypes() const
    {
        return finfo;
    }
    //@}

    /// Add field index and element index and their corresponding type
    void addFldWithType(u32_t fldIdx, const SVFType* type, u32_t elemIdx);

    /// Set number of fields and elements of an aggrate
    inline void setNumOfFieldsAndElems(u32_t nf, u32_t ne)
    {
        numOfFlattenFields = nf;
        numOfFlattenElements = ne;
    }

    /// Return number of elements after flattenning (including array elements)
    inline u32_t getNumOfFlattenElements() const
    {
        return numOfFlattenElements;
    }

    /// Return the number of fields after flattenning (ignoring array elements)
    inline u32_t getNumOfFlattenFields() const
    {
        return numOfFlattenFields;
    }
    /// Return the stride
    inline u32_t getStride() const
    {
        return stride;
    }
};

class SVFType
{

public:
    typedef s64_t GNodeK;

    enum SVFTyKind
    {
        SVFTy,
        SVFPointerTy,
        SVFIntergerTy,
        SVFFunctionTy,
        SVFStructTy,
        SVFArrayTy,
        SVFOtherTy,
    };

private:
    GNodeK kind;	///< used for classof 
    const Type* type;   ///< LLVM type
    const SVFPointerType* getPointerToTy; /// Return a pointer to the current type
    StInfo* typeinfo; /// < SVF's TypeInfo
    bool isSingleValTy; ///< The type represents a single value, not struct or array
    std::string toStr;  ///< string format of the type
protected:
    SVFType(const Type* ty, bool svt, SVFTyKind k) : kind(k), type(ty), getPointerToTy(nullptr), typeinfo(nullptr), isSingleValTy(svt), toStr("")
    {
    }

public:
    SVFType(void) = delete;
    virtual ~SVFType()
    {
    }

    inline GNodeK getKind() const
    {
        return kind;
    }

    inline virtual const std::string toString() const
    {
        return toStr;
    }

    inline void setPointerTo(const SVFPointerType* ty)  
    {
        getPointerToTy = ty;
    }

    inline const SVFPointerType* getPointerTo () const
    {
        assert(getPointerToTy && "set the getPointerToTy first");
        return getPointerToTy;
    }

    inline void setTypeInfo (StInfo* ti)  
    {
        typeinfo = ti;
    }

    inline StInfo* getTypeInfo()
    {
        assert(typeinfo && "set the type info first");
        return typeinfo;
    }

    inline const StInfo* getTypeInfo() const
    {
        assert(typeinfo && "set the type info first");
        return typeinfo;
    }

    inline const Type* getLLVMType() const
    {
        return type;
    }

    inline bool isPointerTy() const
    {
        return kind==SVFPointerTy;
    }

    inline bool isSingleValueType() const
    {
        return isSingleValTy;
    }
};

class SVFPointerType : public SVFType
{

private:
const SVFType* ptrElementType;

public:
    SVFPointerType(const Type* ty, const SVFType* pty) : SVFType(ty, true, SVFPointerTy), ptrElementType(pty)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFPointerTy;
    }
    inline const SVFType* getPtrElementType() const
    {
        return ptrElementType;
    }
};

class SVFIntergerType : public SVFType
{
public:
    SVFIntergerType(const Type* ty) : SVFType(ty, true, SVFIntergerTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFIntergerTy;
    }
};

class SVFFunctionType : public SVFType
{
private:
    const SVFType* retTy;

public:
    SVFFunctionType(const Type* ty, const SVFType* rt) : SVFType(ty,false, SVFFunctionTy), retTy(rt)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFFunctionTy;
    }
    const SVFType* getReturnType() const
    {
        return retTy;
    }
};

class SVFStructType : public SVFType
{
public:
    SVFStructType(const Type* ty) : SVFType(ty, false, SVFStructTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFStructTy;
    }
};

class SVFArrayType : public SVFType
{
public:
    SVFArrayType(const Type* ty) : SVFType(ty, false, SVFArrayTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFArrayTy;
    }
};

class SVFOtherType : public SVFType
{
public:
    SVFOtherType(const Type* ty, bool isSingleValueTy) : SVFType(ty, isSingleValueTy, SVFOtherTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFOtherTy;
    }
};


// TODO: be explicit that this is a pair of 32-bit unsigneds?
template <> struct Hash<NodePair>
{
    size_t operator()(const NodePair &p) const
    {
        // Make sure our assumptions are sound: use u32_t
        // and u64_t. If NodeID is not actually u32_t or size_t
        // is not u64_t we should be fine since we get a
        // consistent result.
        uint32_t first = (uint32_t)(p.first);
        uint32_t second = (uint32_t)(p.second);
        return ((uint64_t)(first) << 32) | (uint64_t)(second);
    }
};

/// LLVM debug macros, define type of your DEBUG model of each pass
#define DBOUT(TYPE, X) 	DEBUG_WITH_TYPE(TYPE, X)
#define DOSTAT(X) 	X
#define DOTIMESTAT(X) 	X

/// General debug flag is for each phase of a pass, it is often in a colorful output format
#define DGENERAL "general"

#define DPAGBuild "pag"
#define DMemModel "mm"
#define DMemModelCE "mmce"
#define DCOMModel "comm"
#define DDDA "dda"
#define DDumpPT "dumppt"
#define DRefinePT "sbpt"
#define DCache "cache"
#define DWPA "wpa"
#define DMSSA "mssa"
#define DInstrument "ins"
#define DAndersen "ander"
#define DSaber "saber"
#define DMTA "mta"
#define DCHA "cha"

/*
 * Number of clock ticks per second. A clock tick is the unit by which
 * processor time is measured and is returned by 'clock'.
 */
#define TIMEINTERVAL 1000
#define CLOCK_IN_MS() (clock() / (CLOCKS_PER_SEC / TIMEINTERVAL))

/// Size of native integer that we'll use for bit vectors, in bits.
#define NATIVE_INT_SIZE (sizeof(unsigned long long) * CHAR_BIT)

enum ModRefInfo
{
    ModRef,
    Ref,
    Mod,
    NoModRef,
};

enum AliasResult
{
    NoAlias,
    MayAlias,
    MustAlias,
    PartialAlias,
};

} // End namespace SVF


template <> struct std::hash<SVF::NodePair>
{
    size_t operator()(const SVF::NodePair &p) const
    {
        // Make sure our assumptions are sound: use u32_t
        // and u64_t. If NodeID is not actually u32_t or size_t
        // is not u64_t we should be fine since we get a
        // consistent result.
        uint32_t first = (uint32_t)(p.first);
        uint32_t second = (uint32_t)(p.second);
        return ((uint64_t)(first) << 32) | (uint64_t)(second);
    }
};

/// Specialise hash for SparseBitVectors.
template <unsigned N>
struct std::hash<SVF::SparseBitVector<N>>
{
    size_t operator()(const SVF::SparseBitVector<N> &sbv) const
    {
        SVF::Hash<std::pair<std::pair<size_t, size_t>, size_t>> h;
        return h(std::make_pair(std::make_pair(sbv.count(), sbv.find_first()), sbv.find_last()));
    }
};

template <typename T>
struct std::hash<std::vector<T>>
{
    size_t operator()(const std::vector<T> &v) const
    {
        // TODO: repetition with CBV.
        size_t h = v.size();

        SVF::Hash<T> hf;
        for (const T &t : v)
        {
            h ^= hf(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }
};

#endif /* INCLUDE_UTIL_SVFBASICTYPES_H_ */
