//===- PCG.h -- Procedure creation graph-------------//
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
 * MHP.h
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui, Peng Di
 */

#ifndef PCG_H_
#define PCG_H_

#include "Util/ThreadAPI.h"
#include "Graphs/PTACallGraph.h"
#include "Util/WorkList.h"
#include "WPA/Andersen.h"
#include <set>
#include <vector>

#include "MHP.h"

namespace SVF
{

/*!
 * This class serves as a base may-happen in parallel analysis for multithreaded program
 * It distinguish thread spawner, spawnee, follower in procedure level by
 * modeling pthread_create, pthread_join, pthread_exit, pthread_cancel synchronization operations
 */
class PCG
{

public:
    typedef Set<const SVFFunction*> FunSet;
    typedef std::vector<const SVFFunction*> FunVec;
    typedef Set<const SVFInstruction*> CallInstSet;
    typedef FIFOWorkList<const SVFFunction*> FunWorkList;
    typedef FIFOWorkList<const SVFBasicBlock*> BBWorkList;

private:
    FunSet spawners;
    FunSet spawnees;
    FunSet followers;
    FunSet mhpfuns;
    PTACallGraph* callgraph;
    SVFModule* mod;
    PointerAnalysis* pta;
    ThreadAPI* tdAPI;

    /// Callsites direct or Indirect call a function which spawn a thread
    CallInstSet spawnCallSites;

    /// Add/Get methods for thread properties of a procedure
    //@{
    inline bool isSpawnerFun(const SVFFunction* fun) const
    {
        return spawners.find(fun) != spawners.end();
    }
    inline bool isSpawneeFun(const SVFFunction* fun) const
    {
        return spawnees.find(fun) != spawnees.end();
    }
    inline bool isFollowerFun(const SVFFunction* fun) const
    {
        return followers.find(fun) != followers.end();
    }
    inline bool addSpawnerFun(const SVFFunction* fun)
    {
        if (fun->isDeclaration())
            return false;
        return spawners.insert(fun).second;
    }
    inline bool addSpawneeFun(const SVFFunction* fun)
    {
        if (fun->isDeclaration())
            return false;
        return spawnees.insert(fun).second;
    }
    inline bool addFollowerFun(const SVFFunction* fun)
    {
        if (fun->isDeclaration())
            return false;
        return followers.insert(fun).second;
    }
    //@}

    /// Add/search spawn sites which directly or indirectly create a thread
    //@{
    inline bool addSpawnsite(const SVFInstruction* callInst)
    {
        return spawnCallSites.insert(callInst).second;
    }
    inline bool isSpawnsite(const SVFInstruction* callInst)
    {
        return spawnCallSites.find(callInst) != spawnCallSites.end();
    }
    //@}
    /// Spawn sites iterators
    //@{
    inline CallInstSet::const_iterator spawnSitesBegin() const
    {
        return spawnCallSites.begin();
    }
    inline CallInstSet::const_iterator spawnSitesEnd() const
    {
        return spawnCallSites.end();
    }
    //@}

public:

    /// Constructor
    PCG(PointerAnalysis* an) : pta(an)
    {
        mod = pta->getModule();
        tdAPI=ThreadAPI::getThreadAPI();
        callgraph = pta->getPTACallGraph();
    }

    /// We start the pass here
    virtual bool analyze();

    /// Destructor
    virtual ~PCG()
    {
    }

    CallICFGNode* getCallICFGNode(const SVFInstruction* inst)
    {
        return pta->getICFG()->getCallICFGNode(inst);
    }
    /// Interface to query whether two function may happen-in-parallel
    virtual bool mayHappenInParallel(const SVFInstruction* i1, const SVFInstruction* i2) const;
    bool mayHappenInParallelBetweenFunctions(const SVFFunction* fun1, const SVFFunction* fun2) const;
    inline const FunSet& getMHPFunctions() const
    {
        return mhpfuns;
    }

    /// Initialize spawner and spawnee sets with threadAPI
    void initFromThreadAPI(SVFModule* module);

    /// Infer spawner spawnee and followers sets by traversing on callGraph
    //@{
    void inferFromCallGraph();
    void collectSpawners();
    void collectSpawnees();
    void collectFollowers();
    void identifyFollowers();
    //@}

    /// Get spawners/spawnees/followers
    //@{
    inline const FunSet& getSpawners() const
    {
        return spawners;
    }
    inline const FunSet& getSpawnees() const
    {
        return spawnees;
    }
    inline const FunSet& getFollowers() const
    {
        return followers;
    }
    //@}

    /// Iterators for thread properties of a procedure
    //@{
    inline FunSet::const_iterator spawnersBegin(const SVFFunction* fun) const
    {
        return spawners.begin();
    }
    inline FunSet::const_iterator spawnersEnd(const SVFFunction* fun) const
    {
        return spawners.end();
    }
    inline FunSet::const_iterator spawneesBegin(const SVFFunction* fun) const
    {
        return spawnees.begin();
    }
    inline FunSet::const_iterator spawneesEnd(const SVFFunction* fun) const
    {
        return spawnees.end();
    }
    inline FunSet::const_iterator followersBegin(const SVFFunction* fun) const
    {
        return followers.begin();
    }
    inline FunSet::const_iterator followersEnd(const SVFFunction* fun) const
    {
        return followers.end();
    }
    //@}

    /// Thread interferenceAnalysis
    void interferenceAnalysis();

    /// Print analysis results
    void printResults();
    /*!
     * Print Thread sensitive properties for each function
     */
    void printTDFuns();
};

} // End namespace SVF

#endif /* PCG_H_ */
