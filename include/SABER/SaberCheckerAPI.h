//===- SaberCheckerAPI.h -- API for checkers in Saber-------------------------//
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
 * SaberCheckerAPI.h
 *
 *  Created on: Apr 23, 2014
 *      Author: Yulei Sui
 */

#ifndef SABERCHECKERAPI_H_
#define SABERCHECKERAPI_H_

#include "Util/SVFUtil.h"
#include "Graphs/ICFGNode.h"

namespace SVF
{

/*
 * Saber Checker API class contains interfaces for various bug checking
 * memory leak detection e.g., alloc free
 * incorrect file operation detection, e.g., fopen, fclose
 */
class SaberCheckerAPI
{

public:
    enum CHECKER_TYPE
    {
        CK_DUMMY = 0,		/// dummy type
        CK_ALLOC,		/// memory allocation
        CK_FREE,      /// memory deallocation
        CK_FOPEN,		/// File open
        CK_FCLOSE		/// File close
    };

    typedef Map<std::string, CHECKER_TYPE> TDAPIMap;

private:
    /// API map, from a string to threadAPI type
    TDAPIMap tdAPIMap;

    /// Constructor
    SaberCheckerAPI ()
    {
        init();
    }

    /// Initialize the map
    void init();

    /// Static reference
    static SaberCheckerAPI* ckAPI;

    /// Get the function type of a function
    inline CHECKER_TYPE getType(const SVFFunction* F) const
    {
        if(F)
        {
            TDAPIMap::const_iterator it= tdAPIMap.find(F->getName());
            if(it != tdAPIMap.end())
                return it->second;
        }
        return CK_DUMMY;
    }

public:
    /// Return a static reference
    static SaberCheckerAPI* getCheckerAPI()
    {
        if(ckAPI == nullptr)
        {
            ckAPI = new SaberCheckerAPI();
        }
        return ckAPI;
    }

    /// Return true if this call is a memory allocation
    //@{
    inline bool isMemAlloc(const SVFFunction* fun) const
    {
        return getType(fun) == CK_ALLOC;
    }
    inline bool isMemAlloc(const Instruction *inst) const
    {
        return getType(SVFUtil::getCallee(inst)) == CK_ALLOC;
    }
    inline bool isMemAlloc(const CallICFGNode* cs) const
    {
        return isMemAlloc(cs->getCallSite());
    }
    //@}

    /// Return true if this call is a memory deallocation
    //@{
    inline bool isMemDealloc(const SVFFunction* fun) const
    {
        return getType(fun) == CK_FREE;
    }
    inline bool isMemDealloc(const Instruction *inst) const
    {
        return getType(SVFUtil::getCallee(inst)) == CK_FREE;
    }
    inline bool isMemDealloc(const CallICFGNode* cs) const
    {
        return isMemDealloc(cs->getCallSite());
    }
    //@}

    /// Return true if this call is a file open
    //@{
    inline bool isFOpen(const SVFFunction* fun) const
    {
        return getType(fun) == CK_FOPEN;
    }
    inline bool isFOpen(const Instruction *inst) const
    {
        return getType(SVFUtil::getCallee(inst)) == CK_FOPEN;
    }
    inline bool isFOpen(const CallICFGNode* cs) const
    {
        return isFOpen(cs->getCallSite());
    }
    //@}

    /// Return true if this call is a file close
    //@{
    inline bool isFClose(const SVFFunction* fun) const
    {
        return getType(fun) == CK_FCLOSE;
    }
    inline bool isFClose(const Instruction *inst) const
    {
        return getType(SVFUtil::getCallee(inst)) == CK_FCLOSE;
    }
    inline bool isFClose(const CallICFGNode* cs) const
    {
        return isFClose(cs->getCallSite());
    }
    //@}

};

} // End namespace SVF

#endif /* SABERCHECKERAPI_H_ */
