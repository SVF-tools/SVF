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
    typedef Set<const Function*> FunSet;
    typedef std::vector<const Function*> FunVec;
    typedef Set<const Instruction*> CallInstSet;
    typedef FIFOWorkList<const Function*> FunWorkList;
    typedef FIFOWorkList<const BasicBlock*> BBWorkList;

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
    inline bool isSpawnerFun(const Function* fun) const
    {
        return spawners.find(fun) != spawners.end();
    }
    inline bool isSpawneeFun(const Function* fun) const
    {
        return spawnees.find(fun) != spawnees.end();
    }
    inline bool isFollowerFun(const Function* fun) const
    {
        return followers.find(fun) != followers.end();
    }
    inline bool addSpawnerFun(const Function* fun)
    {
        if (fun->isDeclaration())
            return false;
        return spawners.insert(fun).second;
    }
    inline bool addSpawneeFun(const Function* fun)
    {
        if (fun->isDeclaration())
            return false;
        return spawnees.insert(fun).second;
    }
    inline bool addFollowerFun(const Function* fun)
    {
        if (fun->isDeclaration())
            return false;
        return followers.insert(fun).second;
    }
    //@}

    /// Add/search spawn sites which directly or indirectly create a thread
    //@{
    inline bool addSpawnsite(const Instruction* callInst)
    {
        return spawnCallSites.insert(callInst).second;
    }
    inline bool isSpawnsite(const Instruction* callInst)
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

    CallICFGNode* getCallICFGNode(const Instruction* inst)
    {
        return pta->getICFG()->getCallICFGNode(inst);
    }
    const SVFFunction* getSVFFun(const Function* fun) const
    {
        return LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
    }
    /// Interface to query whether two function may happen-in-parallel
    virtual bool mayHappenInParallel(const Instruction* i1, const Instruction* i2) const;
    bool mayHappenInParallelBetweenFunctions(const Function* fun1, const Function* fun2) const;
    //bool mayHappenInParallel(const Function* fun1, const Function* fun2) const;
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
    inline FunSet::const_iterator spawnersBegin(const Function* fun) const
    {
        return spawners.begin();
    }
    inline FunSet::const_iterator spawnersEnd(const Function* fun) const
    {
        return spawners.end();
    }
    inline FunSet::const_iterator spawneesBegin(const Function* fun) const
    {
        return spawnees.begin();
    }
    inline FunSet::const_iterator spawneesEnd(const Function* fun) const
    {
        return spawnees.end();
    }
    inline FunSet::const_iterator followersBegin(const Function* fun) const
    {
        return followers.begin();
    }
    inline FunSet::const_iterator followersEnd(const Function* fun) const
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
