/*  [ExtAPI.h] Info about external llvm::Functions
 *  v. 005, 2008-08-15
 *------------------------------------------------------------------------------
 *  Copyright 2008 Andrey Petrov
 *  - apetrov87@gmail.com, apetrov@cs.utexas.edu
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
*/

#ifndef __ExtAPI_H
#define __ExtAPI_H

#include "Util/BasicTypes.h"
#include <map>
#include <set>
#include <string>

namespace SVF
{

//------------------------------------------------------------------------------
class ExtAPI
{
public:
    //External Function types
    //Assume a call in the form LHS= F(arg0, arg1, arg2, arg3).
    enum extf_t
    {
        EFT_NOOP= 0,      //no effect on pointers
        EFT_ALLOC,        //returns a ptr to a newly allocated object
        EFT_REALLOC,      //like L_A0 if arg0 is a non-null ptr, else ALLOC
        EFT_FREE,      	//free memory arg0 and all pointers passing into free function
        EFT_NOSTRUCT_ALLOC, //like ALLOC but only allocates non-struct data
        EFT_STAT,         //retval points to an unknown static var X
        EFT_STAT2,        //ret -> X -> Y (X, Y - external static vars)
        EFT_L_A0,         //copies arg0, arg1, or arg2 into LHS
        EFT_L_A1,
        EFT_L_A2,
        EFT_L_A8,
        EFT_L_A0__A0R_A1,   //stores arg1 into *arg0 and returns arg0 (currently only for memset)
        EFT_L_A0__A0R_A1R,  //copies the data that arg1 points to into the location
        //  arg0 points to; note that several fields may be
        //  copied at once if both point to structs.
        //  Returns arg0.
        EFT_A1R_A0R,      //copies *arg0 into *arg1, with non-ptr return
        EFT_A3R_A1R_NS,   //copies *arg1 into *arg3 (non-struct copy only)
        EFT_A1R_A0,       //stores arg0 into *arg1
        EFT_A2R_A1,       //stores arg1 into *arg2
        EFT_A4R_A1,       //stores arg1 into *arg4
        EFT_L_A0__A2R_A0, //stores arg0 into *arg2 and returns it
        EFT_L_A0__A1_A0,  //store arg1 into arg0's base and returns arg0
        EFT_A0R_NEW,      //stores a pointer to an allocated object in *arg0
        EFT_A1R_NEW,      //as above, into *arg1, etc.
        EFT_A2R_NEW,
        EFT_A4R_NEW,
        EFT_A11R_NEW,
        EFT_STD_RB_TREE_INSERT_AND_REBALANCE,  // Some complex effects
        EFT_STD_RB_TREE_INCREMENT,  // Some complex effects
        EFT_STD_LIST_HOOK,  // Some complex effects

        CPP_EFT_A0R_A1,   //stores arg1 into *arg0
        CPP_EFT_A0R_A1R,  //copies *arg1 into *arg0
        CPP_EFT_A1R,      //load arg1
        EFT_CXA_BEGIN_CATCH,  //__cxa_begin_catch
        CPP_EFT_DYNAMIC_CAST, // dynamic_cast
        EFT_OTHER         //not found in the list
    };
private:

    //Each Function name is mapped to its extf_t
    //  (hash_map and map are much slower).
    llvm::StringMap<extf_t> info;
    //A cache of is_ext results for all SVFFunction*'s (hash_map is fastest).
    Map<const SVFFunction*, bool> isext_cache;

    void init();                          //fill in the map (see ExtAPI.cpp)

    ExtAPI()
    {
        init();
        isext_cache.clear();
    }

    // Singleton pattern here to enable instance of PAG can only be created once.
    static ExtAPI* extAPI;

public:

    /// Singleton design here to make sure we only have one instance during whole analysis
    static ExtAPI* getExtAPI()
    {
        if (extAPI == nullptr)
        {
            extAPI = new ExtAPI();
        }
        return extAPI;
    }

    //Return the extf_t of (F).
    extf_t get_type(const SVFFunction* F) const
    {
        assert(F);
        std::string funName = F->getName().str();
        if(F->isIntrinsic())
        {
            funName = "llvm." + F->getName().split('.').second.split('.').first.str();
        }
        llvm::StringMap<extf_t>::const_iterator it= info.find(funName);
        if(it == info.end() || !F->isDeclaration())
            return EFT_OTHER;
        else
            return it->second;
    }

    //Does (F) have a static var X (unavailable to us) that its return points to?
    bool has_static(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        return t==EFT_STAT || t==EFT_STAT2;
    }
    //Assuming hasStatic(F), does (F) have a second static Y where X -> Y?
    bool has_static2(const SVFFunction* F) const
    {
        return get_type(F) == EFT_STAT2;
    }
    //Does (F) allocate a new object and return it?
    bool is_alloc(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        return t==EFT_ALLOC || t==EFT_NOSTRUCT_ALLOC;
    }
    //Does (F) allocate a new object and assign it to one of its arguments?
    bool is_arg_alloc(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        return t>=EFT_A0R_NEW && t<=EFT_A11R_NEW;
    }
    //Get the position of argument which holds the new object
    int get_alloc_arg_pos(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        switch(t)
        {
        case EFT_A0R_NEW:
            return 0;
        case EFT_A1R_NEW:
            return 1;
        case EFT_A2R_NEW:
            return 2;
        case EFT_A4R_NEW:
            return 4;
        case EFT_A11R_NEW:
            return 11;
        default:
            assert(false && "Not an alloc call via argument.");
            return -1;
        }
    }
    //Does (F) allocate only non-struct objects?
    bool no_struct_alloc(const SVFFunction* F) const
    {
        return get_type(F) == EFT_NOSTRUCT_ALLOC;
    }
    //Does (F) not free/release any memory?
    bool is_dealloc(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        return t == EFT_FREE;
    }
    //Does (F) not do anything with the known pointers?
    bool is_noop(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        return t == EFT_NOOP || t == EFT_FREE;
    }
    //Does (F) reallocate a new object?
    bool is_realloc(const SVFFunction* F) const
    {
        extf_t t= get_type(F);
        return t==EFT_REALLOC;
    }
    //Should (F) be considered "external" (either not defined in the program
    //  or a user-defined version of a known alloc or no-op)?
    bool is_ext(const SVFFunction* F)
    {
        assert(F);
        //Check the cache first; everything below is slower.
        Map<const SVFFunction*, bool>::iterator i_iec= isext_cache.find(F);
        if(i_iec != isext_cache.end())
            return i_iec->second;

        bool res;
        if(F->isDeclaration() || F->isIntrinsic())
        {
            res= 1;
        }
        else
        {
            extf_t t= get_type(F);
            res= t==EFT_ALLOC || t==EFT_REALLOC || t==EFT_NOSTRUCT_ALLOC
                 || t==EFT_NOOP || t==EFT_FREE;
        }
        isext_cache[F]= res;
        return res;
    }
};

} // End namespace SVF

#endif
