//===- PTAType.h -- PTAType class---------------------------------------------//
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
 * PTAType.h
 *
 *  Created on: Oct 06, 2016
 *      Author: Xiaokang Fan
 */

#ifndef PTATYPE_H_
#define PTATYPE_H_

#include <set>
#include <map>
#include "Util/BasicTypes.h"

namespace SVF
{

class PAGNode;
class PAG;

class PTAType
{
public:
    /// Constructor
    PTAType(const Type *ty): type(ty) {}

    /// Get the contained llvm type
    inline const Type *getLLVMType() const
    {
        return type;
    }

    /// Dump the type
    inline void dump() const
    {
        type->dump();
    }

    /// Operator overloading
    //@{
    inline bool operator==(const PTAType &ty) const
    {
        return type == ty.getLLVMType();
    }

    inline bool operator!=(const PTAType &ty) const
    {
        return type != ty.getLLVMType();
    }

    inline bool operator<(const PTAType &ty) const
    {
        return type < ty.getLLVMType();
    }

    inline bool operator>(const PTAType &ty) const
    {
        return type > ty.getLLVMType();
    }
    //@}

private:
    const Type *type;
};

class TypeSet
{
public:

    typedef OrderedSet<PTAType> TypeSetTy;

    typedef TypeSetTy::iterator iterator;
    typedef TypeSetTy::const_iterator const_iterator;

    // Iterators
    //@{
    inline iterator begin()
    {
        return typeSet.begin();
    }
    inline iterator end()
    {
        return typeSet.end();
    }
    inline const_iterator begin() const
    {
        return typeSet.begin();
    }
    inline const_iterator end() const
    {
        return typeSet.end();
    }
    //@}

    /// Number of types contained
    inline u32_t size() const
    {
        return typeSet.size();
    }

    /// Add a ptatype
    inline bool addType(const PTAType &type)
    {
        std::pair<iterator, bool> ret = typeSet.insert(type);
        return ret.second;
    }

    /// Contain a ptatype or not
    inline bool containType(const PTAType &type) const
    {
        return typeSet.find(type) != typeSet.end();
    }

    /// Intersect with another typeset or not
    // Algorithm set_intersection
    // Complexity: 2 * (N1 + N2) - 1
    // N1, N2: number of element in the two typeset
    inline bool intersect(const TypeSet *typeset) const
    {
        if (size() == 1)
        {
            const_iterator first = begin();
            return typeset->containType(*first);
        }
        else if (typeset->size() == 1)
        {
            const_iterator first = typeset->begin();
            return containType(*first);
        }
        else
        {
            const_iterator first1 = typeset->begin(), first2 = begin(),
                           last1 = typeset->end(), last2 = end();
            const_iterator largest1 = last1, largest2 = last2;
            largest1--;
            largest2--;
            if (*largest1 < *first2 || *largest2 < *first1)
                return false;

            while (first1 != last1 && first2 != last2)
            {
                if (*first1 < *first2)
                    first1++;
                else if (*first2 < *first1)
                    first2++;
                else
                    return true;
            }
            return false;
        }
    }

    /// Dump all types in the typeset
    inline void dumpTypes() const
    {
        for (const_iterator it = begin(), eit = end(); it != eit; ++it)
        {
            const PTAType &type = *it;
            type.dump();
        }
    }

private:
    TypeSetTy typeSet;
};

class TypeSystem
{
public:

    typedef Map<NodeID, TypeSet*> VarToTypeSetMapTy;

    typedef OrderedMap<PTAType, NodeBS> TypeToVarsMapTy;

    typedef typename VarToTypeSetMapTy::iterator iterator;
    typedef typename VarToTypeSetMapTy::const_iterator const_iterator;

    /// Iterators
    //@{
    inline iterator begin()
    {
        return VarToTypeSetMap.begin();
    }
    inline iterator end()
    {
        return VarToTypeSetMap.end();
    }
    inline const_iterator begin() const
    {
        return VarToTypeSetMap.begin();
    }
    inline const_iterator end() const
    {
        return VarToTypeSetMap.end();
    }
    //}@

    /// Constructor
    TypeSystem(const PAG *pag)
    {
        translateLLVMTypeToPTAType(pag);
    }

    /// Has typeset or not
    inline bool hasTypeSet(NodeID var) const
    {
        const_iterator it = VarToTypeSetMap.find(var);
        return it != VarToTypeSetMap.end();
    }

    /// Get a var's typeset
    inline const TypeSet *getTypeSet(NodeID var) const
    {
        const_iterator it = VarToTypeSetMap.find(var);
        assert(it != VarToTypeSetMap.end() && "Can not find typeset for var");
        return it->second;
    }

    /// Add a ptatype for a var
    /// Return true if the ptatype is new for this var
    inline bool addTypeForVar(NodeID var, const PTAType &type)
    {
        iterator it = VarToTypeSetMap.find(var);
        if (it != VarToTypeSetMap.end())
        {
            TypeSet *typeSet = it->second;
            return typeSet->addType(type);
        }
        else
        {
            TypeSet *typeSet = new TypeSet;
            typeSet->addType(type);
            VarToTypeSetMap[var] = typeSet;
            return true;
        }
    }

    /// Add a ptatype for a var
    /// Return true if the ptatype is new for this var
    inline bool addTypeForVar(NodeID var, const Type *type)
    {
        PTAType ptaTy(type);
        return addTypeForVar(var, ptaTy);
    }

    void addVarForType(NodeID var, const PTAType &type)
    {
        TypeToVarsMapTy::iterator it = typeToVarsMap.find(type);
        if (it == typeToVarsMap.end())
        {
            NodeBS nodes;
            nodes.set(var);
            typeToVarsMap[type] = nodes;
        }
        else
        {
            NodeBS &nodes = it->second;
            nodes.set(var);
        }
    }

    void addVarForType(NodeID var, const Type *type)
    {
        PTAType ptaTy(type);
        return addVarForType(var, ptaTy);
    }

    inline bool hasVarsForType(const PTAType &type) const
    {
        TypeToVarsMapTy::const_iterator it = typeToVarsMap.find(type);
        return it != typeToVarsMap.end();
    }

    inline NodeBS &getVarsForType(const PTAType &type)
    {
        TypeToVarsMapTy::iterator it = typeToVarsMap.find(type);
        assert(it != typeToVarsMap.end() && "Can not find vars for type");
        return it->second;
    }

    //// Debugging function
    //@{
    /// Print each var's id and all its types
    void printTypeSystem() const
    {
        for (const_iterator it = VarToTypeSetMap.begin(),
                eit = VarToTypeSetMap.end(); it != eit; ++it)
        {
            SVFUtil::errs() << "Var: " << it->first << '\n';
            SVFUtil::errs() << "types:\n";
            const TypeSet *typeSet = it->second;
            typeSet->dumpTypes();
            SVFUtil::errs() << '\n';
        }
    }
    //@}

private:
    /*
     * Translate llvm type into ptatype and build the pagnode to ptatype map
     *
     * Kinds of PAGNode:
     * ValPN: GepValPN, DummyValPN
     * ObjPN: GepObjPN, FIObjPN, DummyObjPN
     * RetPN
     * VarArgPN
     */
    void translateLLVMTypeToPTAType(const PAG *pag)
    {
        for (PAG::const_iterator it = pag->begin(); it != pag->end(); ++it)
        {
            const PAGNode *pagNode = it->second;
            if (pagNode->hasValue() == false)
                continue;

            const Value *value = pagNode->getValue();
            const Type *valType = value->getType();

            const Type *nodeType = valType;

            if (const GepValPN *gepvalnode = SVFUtil::dyn_cast<GepValPN>(pagNode))
            {
                nodeType = gepvalnode->getType();
            }
            else if (SVFUtil::isa<RetPN>(pagNode))
            {
                const llvm::PointerType *ptrTy = SVFUtil::dyn_cast<llvm::PointerType>(valType);
                const llvm::FunctionType *funTy = SVFUtil::dyn_cast<llvm::FunctionType>(ptrTy->getElementType());
                nodeType = funTy->getReturnType();
            }

            PTAType ptaType(nodeType);
            if (addTypeForVar(pagNode->getId(), ptaType))
                addVarForType(pagNode->getId(), ptaType);
        }
    }

private:
    VarToTypeSetMapTy VarToTypeSetMap;
    OrderedSet<PTAType> allPTATypes;
    TypeToVarsMapTy typeToVarsMap;
};

} // End namespace SVF

#endif /* PTATYPE_H_ */
