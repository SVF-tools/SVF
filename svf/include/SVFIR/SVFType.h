//===- SVFBasicTypes.h -- Basic types used in SVF-----------------------------//
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
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_SVFTYPE_H_
#define INCLUDE_SVFIR_SVFTYPE_H_

#include "Util/SparseBitVector.h"
#include "Util/GeneralType.h"


namespace SVF
{
class SVFType;
class SVFPointerType;


/*!
 * Flattened type information of StructType, ArrayType and SingleValueType
 */
class StInfo
{

    friend class GraphDBClient;
    friend class IRGraph;

protected:
    StInfo (u32_t id, std::vector<u32_t> fldIdxVec, std::vector<u32_t> elemIdxVec, Map<u32_t, const SVFType*> fldIdx2TypeMap,
    std::vector<const SVFType*> finfo,u32_t stride,u32_t numOfFlattenElements,u32_t numOfFlattenFields, std::vector<const SVFType*> flattenElementTypes )
    :StInfoId(id), fldIdxVec(fldIdxVec), elemIdxVec(elemIdxVec), fldIdx2TypeMap(fldIdx2TypeMap), finfo(finfo), stride(stride), 
    numOfFlattenElements(numOfFlattenElements), numOfFlattenFields(numOfFlattenFields), flattenElementTypes(flattenElementTypes)
    {

    }

    inline const u32_t getStinfoId() const
    {
        return StInfoId;
    }

    inline const Map<u32_t, const SVFType*>& getFldIdx2TypeMap() const
    {
        return fldIdx2TypeMap;
    }

    inline void setStinfoId(u32_t id)
    {
        StInfoId = id;
    }

private:
    u32_t StInfoId;
    /// flattened field indices of a struct (ignoring arrays)
    std::vector<u32_t> fldIdxVec;
    /// flattened element indices including structs and arrays by considering
    /// strides
    std::vector<u32_t> elemIdxVec;
    /// Types of all fields of a struct
    Map<u32_t, const SVFType*> fldIdx2TypeMap;
    /// All field infos after flattening a struct
    std::vector<const SVFType*> finfo;
    /// stride represents the number of repetitive elements if this StInfo
    /// represent an ArrayType. stride is 1 by default.
    u32_t stride;
    /// number of elements after flattening (including array elements)
    u32_t numOfFlattenElements;
    /// number of fields after flattening (ignoring array elements)
    u32_t numOfFlattenFields;
    /// Type vector of fields
    std::vector<const SVFType*> flattenElementTypes;
    /// Max field limit

public:
    StInfo() = delete;
    StInfo(const StInfo& st) = delete;
    void operator=(const StInfo&) = delete;

    /// Constructor
    explicit StInfo(u32_t s)
        : stride(s), numOfFlattenElements(s), numOfFlattenFields(s)
    {
    }

    /// Destructor
    ~StInfo() = default;

    ///  struct A { int id; int salary; };
    ///  struct B { char name[20]; struct A a;}
    ///  B b;
    ///
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

    /// Set number of fields and elements of an aggregate
    inline void setNumOfFieldsAndElems(u32_t nf, u32_t ne)
    {
        numOfFlattenFields = nf;
        numOfFlattenElements = ne;
    }

    /// Return number of elements after flattening (including array elements)
    inline u32_t getNumOfFlattenElements() const
    {
        return numOfFlattenElements;
    }

    /// Return the number of fields after flattening (ignoring array elements)
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

    friend class LLVMModuleSet;

public:
    typedef s64_t GNodeK;

    enum SVFTyKind
    {
        SVFTy,
        SVFPointerTy,
        SVFIntegerTy,
        SVFFunctionTy,
        SVFStructTy,
        SVFArrayTy,
        SVFOtherTy,
    };

protected:

    /// set svfptrty and svfi8ty when initializing SVFType from db query results
    inline static void setSVFPtrType(SVFType* ptrTy)
    {
        svfPtrTy = ptrTy;
    }

    inline static void setSVFInt8Type(SVFType* i8Ty)
    {
        svfI8Ty = i8Ty;
    }


public:

    inline static SVFType* getSVFPtrType()
    {
        assert(svfPtrTy && "ptr type not set?");
        return svfPtrTy;
    }

    inline static SVFType* getSVFInt8Type()
    {
        assert(svfI8Ty && "int8 type not set?");
        return svfI8Ty;
    }

private:

    static SVFType* svfPtrTy; ///< ptr type
    static SVFType* svfI8Ty; ///< 8-bit int type

private:
    GNodeK kind; ///< used for classof
    StInfo* typeinfo;   ///< SVF's TypeInfo
    bool isSingleValTy; ///< The type represents a single value, not struct or
    u32_t byteSize; ///< LLVM Byte Size
    u32_t id;
    ///< array

protected:
    SVFType(bool svt, SVFTyKind k, u32_t i = 0, u32_t Sz = 1)
        : kind(k), typeinfo(nullptr),
          isSingleValTy(svt), byteSize(Sz), id(i)
    {
    }
public:
    SVFType(void) = delete;
    virtual ~SVFType() {}

    inline GNodeK getKind() const
    {
        return kind;
    }

    /// \note Use `os<<svfType` or `svfType.print(os)` when possible to avoid
    /// string concatenation.
    std::string toString() const;

    virtual void print(std::ostream& os) const = 0;


    u32_t getId() const
    {
        return id;
    }

    inline void setTypeInfo(StInfo* ti)
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

    /// if Type is not sized, byteSize is 0
    /// if Type is sized, byteSize is the LLVM Byte Size.
    inline u32_t getByteSize() const
    {
        return byteSize;
    }

    inline bool isPointerTy() const
    {
        return kind == SVFPointerTy;
    }

    inline bool isArrayTy() const
    {
        return kind == SVFArrayTy;
    }

    inline bool isStructTy() const
    {
        return kind == SVFStructTy;
    }

    inline bool isSingleValueType() const
    {
        return isSingleValTy;
    }
};

std::ostream& operator<<(std::ostream& os, const SVFType& type);

class SVFPointerType : public SVFType
{

    friend class GraphDBClient;
protected:
    SVFPointerType(u32_t id, u32_t byteSize, bool isSingleValTy)
        : SVFType(isSingleValTy, SVFPointerTy, id, byteSize)
    {
        
    }

public:
    SVFPointerType(u32_t i, u32_t byteSize = 1)
        : SVFType(true, SVFPointerTy, i, byteSize)
    {
    }

    static inline bool classof(const SVFType* node)
    {
        return node->getKind() == SVFPointerTy;
    }

    void print(std::ostream& os) const override;
};

class SVFIntegerType : public SVFType
{
    friend class GraphDBClient;

private:
    short signAndWidth; ///< For printing

protected:
    SVFIntegerType(u32_t i, u32_t byteSize, bool isSingleValTy,short signAndWidth)
        : SVFType(isSingleValTy, SVFIntegerTy, i, byteSize)
    {
        this->signAndWidth = signAndWidth;
    }

    short getSignAndWidth() const
    {
        return signAndWidth;
    }

public:
    SVFIntegerType(u32_t i, u32_t byteSize = 1) : SVFType(true, SVFIntegerTy, i, byteSize) {}
    static inline bool classof(const SVFType* node)
    {
        return node->getKind() == SVFIntegerTy;
    }

    void print(std::ostream& os) const override;

    void setSignAndWidth(short sw)
    {
        signAndWidth = sw;
    }

    bool isSigned() const
    {
        return signAndWidth < 0;
    }
};

class SVFFunctionType : public SVFType
{

    friend class GraphDBClient;
private:
    const SVFType* retTy;
    std::vector<const SVFType*> params;
    bool varArg;

protected:
    /**
     * Set return type of function, this is used when loading from DB
     */
    const void setReturnType(const SVFType* rt)
    {
        retTy = rt;
    }

    SVFFunctionType(u32_t id, bool svt, u32_t byteSize)
        : SVFType(svt, SVFFunctionTy, id, byteSize)
    {
    }

    void addParamType(const SVFType* type) 
    {
        params.push_back(type);
    }

public:
    SVFFunctionType(u32_t i, const SVFType* rt, const std::vector<const SVFType*>& p, bool isvararg)
        : SVFType(false, SVFFunctionTy,  i, 1), retTy(rt), params(p), varArg(isvararg)
    {
    }

    static inline bool classof(const SVFType* node)
    {
        return node->getKind() == SVFFunctionTy;
    }
    const SVFType* getReturnType() const
    {
        return retTy;
    }

    const std::vector<const SVFType*>& getParamTypes() const
    {
        return params;
    }

    bool isVarArg() const
    {
        return varArg;
    }

    void print(std::ostream& os) const override;
};

class SVFStructType : public SVFType
{
    friend class GraphDBClient;

protected:
    SVFStructType(u32_t i, bool svt, u32_t byteSize, std::string name) : SVFType(svt, SVFStructTy, i, byteSize),name(name) {}
    
    const std::string& getName() const
    {
        return name;
    }

    void addFieldsType(const SVFType* type) 
    {
        fields.push_back(type);
    }

private:
    /// @brief Field for printing & debugging
    std::string name;
    std::vector<const SVFType*> fields;

public:
    SVFStructType(u32_t i, std::vector<const SVFType *> &f, u32_t byteSize = 1) :
        SVFType(false, SVFStructTy, i, byteSize), fields(f)
    {
    }

    static inline bool classof(const SVFType* node)
    {
        return node->getKind() == SVFStructTy;
    }

    void print(std::ostream& os) const override;

    const std::string& getName()
    {
        return name;
    }

    void setName(const std::string& structName)
    {
        name = structName;
    }
    void setName(std::string&& structName)
    {
        name = std::move(structName);
    }

    const std::vector<const SVFType*>& getFieldTypes() const
    {
        return fields;
    }

};

class SVFArrayType : public SVFType
{
    friend class GraphDBClient;

protected:
    SVFArrayType(u32_t id, bool svt, u32_t byteSize, unsigned elemNum)
        : SVFType(svt, SVFArrayTy, id, byteSize), numOfElement(elemNum), typeOfElement(nullptr)
    {
        
    }
    const unsigned getNumOfElement() const
    {
        return numOfElement;
    }

private:
    unsigned numOfElement; /// For printing & debugging
    const SVFType* typeOfElement; /// For printing & debugging

public:
    SVFArrayType(u32_t i, u32_t byteSize = 1)
        : SVFType(false, SVFArrayTy, i, byteSize), numOfElement(0), typeOfElement(nullptr)
    {
    }

    static inline bool classof(const SVFType* node)
    {
        return node->getKind() == SVFArrayTy;
    }

    void print(std::ostream& os) const override;

    const SVFType* getTypeOfElement() const
    {
        return typeOfElement;
    }

    void setTypeOfElement(const SVFType* elemType)
    {
        typeOfElement = elemType;
    }

    void setNumOfElement(unsigned elemNum)
    {
        numOfElement = elemNum;
    }


};

class SVFOtherType : public SVFType
{
    friend class GraphDBClient;

protected:
    SVFOtherType(u32_t i, bool isSingleValueTy, u32_t byteSize, std::string repr) : SVFType(isSingleValueTy, SVFOtherTy, i, byteSize),repr(repr) {}
    const std::string& getRepr() const
    {
        return repr;
    }

private:
    std::string repr; /// Field representation for printing

public:
    SVFOtherType(u32_t i, bool isSingleValueTy, u32_t byteSize = 1) : SVFType(isSingleValueTy, SVFOtherTy, i, byteSize) {}

    static inline bool classof(const SVFType* node)
    {
        return node->getKind() == SVFOtherTy;
    }

    const std::string& getRepr()
    {
        return repr;
    }

    void setRepr(std::string&& r)
    {
        repr = std::move(r);
    }

    void setRepr(const std::string& r)
    {
        repr = r;
    }

    void print(std::ostream& os) const override;
};

// TODO: be explicit that this is a pair of 32-bit unsigneds?
template <> struct Hash<NodePair>
{
    size_t operator()(const NodePair& p) const
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

#if !defined NDBUG && defined USE_SVF_DBOUT
// TODO: This comes from the following link
// https://github.com/llvm/llvm-project/blob/75e33f71c2dae584b13a7d1186ae0a038ba98838/llvm/include/llvm/Support/Debug.h#L64
// The original LLVM implementation makes use of type. But we can get that info,
// so we can't simulate the full behaviour for now.
#    define SVF_DEBUG_WITH_TYPE(TYPE, X)                                       \
        do                                                                     \
        {                                                                      \
            X;                                                                 \
        } while (false)
#else
#    define SVF_DEBUG_WITH_TYPE(TYPE, X)                                       \
        do                                                                     \
        {                                                                      \
        } while (false)
#endif

/// LLVM debug macros, define type of your DBUG model of each pass
#define DBOUT(TYPE, X) SVF_DEBUG_WITH_TYPE(TYPE, X)
#define DOSTAT(X) X
#define DOTIMESTAT(X) X

/// General debug flag is for each phase of a pass, it is often in a colorful
/// output format
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
    size_t operator()(const SVF::NodePair& p) const
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
template <unsigned N> struct std::hash<SVF::SparseBitVector<N>>
{
    size_t operator()(const SVF::SparseBitVector<N>& sbv) const
    {
        SVF::Hash<std::pair<std::pair<size_t, size_t>, size_t>> h;
        return h(std::make_pair(std::make_pair(sbv.count(), sbv.find_first()),
                                sbv.find_last()));
    }
};

template <typename T> struct std::hash<std::vector<T>>
{
    size_t operator()(const std::vector<T>& v) const
    {
        // TODO: repetition with CBV.
        size_t h = v.size();

        SVF::Hash<T> hf;
        for (const T& t : v)
        {
            h ^= hf(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }
};

#endif /* INCLUDE_SVFIR_SVFTYPE_H_ */
