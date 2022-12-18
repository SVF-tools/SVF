/*
 * MTAResultValidator.cpp
 *
 *  Created on: 29/06/2015
 *      Author: Peng Di
 */

#include "Util/Options.h"
#include <string>
#include <sstream>

#include "MTAResultValidator.h"

using namespace SVF;
using namespace SVFUtil;

namespace SVF
{

// Subclassing RCResultValidator to define the abstract methods.
class MHPValidator : public RaceResultValidator
{
public:
    MHPValidator(MHP *mhp) :mhp(mhp)
    {
    }
    bool mayHappenInParallel(const Instruction* I1, const Instruction* I2)
    {
        const SVFInstruction* inst1 = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(I1);
        const SVFInstruction* inst2 = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(I2);
        return mhp->mayHappenInParallel(inst1, inst2);
    }
private:
    MHP *mhp;
};

} // End namespace SVF

void MTAResultValidator::analyze()
{

    // Initialize the validator and perform validation.
    MHPValidator validator(mhp);
    validator.init(mhp->getTCT()->getSVFModule());
    validator.analyze();


    std::string errstring;
    if (!collectCallsiteTargets())
        return;
    if (!collectCxtThreadTargets())
        return;

    errstring = getOutput("Validate CxtThread:", validateCxtThread());
    outs() << "======" << errstring << "======\n";

    if (!collectTCTTargets())
        return;
    errstring = getOutput("Validate TCT:     ", validateTCT());
    outs() << "======" << errstring << "======\n";

    if (!collectInterleavingTargets())
        return;
    errstring = getOutputforInterlevAnalysis("Validate Interleaving:", validateInterleaving());
    outs() << "======" << errstring << "======\n";
}

std::vector<std::string> &MTAResultValidator::split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> MTAResultValidator::split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}
NodeID MTAResultValidator::getIntArg(const Instruction* inst, u32_t arg_num)
{
    assert(LLVMUtil::isCallSite(inst) && "getFirstIntArg: inst is not a callinst");
    const CallBase* cs = LLVMUtil::getLLVMCallSite(inst);
    const ConstantInt* x = SVFUtil::dyn_cast<ConstantInt>(cs->getArgOperand(arg_num));
    assert((arg_num < cs->arg_size()) && "Does not has this argument");
    return (NodeID) x->getSExtValue();
}

std::vector<std::string> MTAResultValidator::getStringArg(const Instruction* inst, unsigned int arg_num)
{
    assert(LLVMUtil::isCallSite(inst) && "getFirstIntArg: inst is not a callinst");
    const CallBase* cs = LLVMUtil::getLLVMCallSite(inst);
    assert((arg_num < cs->arg_size()) && "Does not has this argument");
    const GetElementPtrInst* gepinst = SVFUtil::dyn_cast<GetElementPtrInst>(cs->getArgOperand(arg_num));
    const Constant* arrayinst = SVFUtil::dyn_cast<Constant>(gepinst->getOperand(0));
    const ConstantDataArray* cxtarray = SVFUtil::dyn_cast<ConstantDataArray>(arrayinst->getOperand(0));
    if (!cxtarray)
    {
        std::vector<std::string> strvec;
        return strvec;
    }
    const std::string vthdcxtstring = cxtarray->getAsCString().str();
    return split(vthdcxtstring, ',');
}

CallStrCxt MTAResultValidator::getCxtArg(const Instruction* inst, unsigned int arg_num)
{
    std::vector<std::string> x = getStringArg(inst, arg_num);
    CallStrCxt cxt;
    if (0 == x.size())
        return cxt;
    // Deal with the the second argument that records all callsites
    for (std::vector<std::string>::iterator i = x.begin(); i != x.end(); i++)
    {
        std::vector<std::string> y = split((*i), '.');
        y[0].erase(y[0].find("cs"), 2);

        const SVFFunction* callee = mod->getSVFFunction(y[1]);
        const SVFInstruction* svfInst = csnumToInstMap[atoi(y[0].c_str())];
        assert(callee && "callee error");
        CallICFGNode* cbn = mhp->getTCT()->getCallICFGNode(svfInst);
        CallSiteID csId = tcg->getCallSiteID(cbn, callee);
        cxt.push_back(csId);
    }
    return cxt;
}

const Instruction* MTAResultValidator::getPreviousMemoryAccessInst(const Instruction* I)
{
    I = I->getPrevNode();
    while (I)
    {
        if (SVFUtil::isa<LoadInst>(I) || SVFUtil::isa<StoreInst>(I))
            return I;
        I = I->getPrevNode();
    }
    return nullptr;
}

inline std::string MTAResultValidator::getOutput(const char *scenario, bool analysisRes)
{
    std::string ret(scenario);
    ret += "\t";

    if (analysisRes)
        ret += SVFUtil::sucMsg("SUCCESS");
    else
        ret += SVFUtil::errMsg("FAILURE");
    return ret;
}

inline std::string MTAResultValidator::getOutputforInterlevAnalysis(const char *scenario, INTERLEV_FLAG analysisRes)
{
    std::string ret(scenario);
    ret += "\t";
    switch (analysisRes)
    {
    case INTERLEV_TRUE:
        ret += SVFUtil::sucMsg("SUCCESS");
        break;
    case INTERLEV_UNSOUND:
        ret += SVFUtil::bugMsg2("UNSOUND");
        break;
    case INTERLEV_IMPRECISE:
        ret += SVFUtil::bugMsg1("IMPRECISE");
        break;
    default:
        ret += SVFUtil::errMsg("FAILURE");
    }
    return ret;
}

bool MTAResultValidator::matchCxt(const CallStrCxt cxt1, const CallStrCxt cxt2) const
{
    if (cxt1.size() != cxt2.size())
        return false;
    return std::equal(cxt1.begin(), cxt1.end(), cxt2.begin());
}

void MTAResultValidator::dumpCxt(const CallStrCxt& cxt) const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "[:";
    for (CallStrCxt::const_iterator it = cxt.begin(), eit = cxt.end(); it != eit; ++it)
    {
        rawstr << " ' " << *it << " ' ";
        rawstr << tcg->getCallSite(*it)->getCallSite()->toString();
        rawstr << "  call  " << tcg->getCallSite(*it)->getCaller()->getName() << "-->" << tcg->getCalleeOfCallSite(*it)->getName() << ", \n";
    }
    rawstr << " ]";
    outs() << "max cxt = " << cxt.size() << rawstr.str() << "\n";
}

void MTAResultValidator::dumpInterlev(NodeBS& lev)
{
    outs() << " [ ";
    for (NodeBS::iterator it = lev.begin(), eit = lev.end(); it != eit; it++)
    {
        NodeID id = *it;
        outs() << rthdTovthd[id] << ", ";
    }
    outs() << "]\n";
}

bool MTAResultValidator::collectCallsiteTargets()
{
    for (Module& M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            for (Function::const_iterator bi = (*F).begin(), ebi = (*F).end(); bi != ebi; ++bi)
            {
                const BasicBlock* bb = &*bi;
                if (!bb->getName().str().compare(0, 2, "cs"))
                {
                    NodeID csnum = atoi(bb->getName().str().substr(2).c_str());
                    const Instruction* inst = &bb->front();
                    while (1)
                    {
                        if (SVFUtil::isa<CallInst>(inst))
                        {
                            break;
                        }
                        inst = inst->getNextNode();
                        assert(inst && "Wrong cs label, cannot find callsite");
                    }
                    const SVFInstruction* svfInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(inst);
                    csnumToInstMap[csnum] = svfInst;
                }
            }
        }
    }
    return !csnumToInstMap.empty();
}

bool MTAResultValidator::collectCxtThreadTargets()
{
    const Function* F = nullptr;
    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for(auto it = M.begin(); it != M.end(); it++)
        {
            const std::string fName = (*it).getName().str();
            if(fName.find(CXT_THREAD) != std::string::npos)
            {
                F = &(*it);
                break;
            }
        }
    }
    if (!F)
        return false;

    // Push main thread into vthdToCxt;
    CallStrCxt main_cxt;
    vthdToCxt[0] = main_cxt;

    // Collect call sites of all CXT_THREAD function calls.

    for (Value::const_use_iterator it = F->use_begin(), ie = F->use_end(); it != ie; ++it)
    {
        const Use *u = &*it;
        const Value* user = u->getUser();
        const Instruction* inst = SVFUtil::dyn_cast<Instruction>(user);

        NodeID vthdnum = getIntArg(inst, 0);
        CallStrCxt cxt = getCxtArg(inst, 1);

        vthdToCxt[vthdnum] = cxt;
    }
    return true;
}

bool MTAResultValidator::collectTCTTargets()
{

    // Collect call sites of all TCT_ACCESS function calls.
    const Function* F = nullptr;
    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for(auto it = M.begin(); it != M.end(); it++)
        {
            const std::string fName = (*it).getName().str();
            if(fName.find(TCT_ACCESS) != std::string::npos)
            {
                F = &(*it);
                break;
            }
        }
    }
    if (!F)
        return false;

    for (Value::const_use_iterator it = F->use_begin(), ie = F->use_end(); it != ie; ++it)
    {
        const Use *u = &*it;
        const Value* user = u->getUser();
        const Instruction* inst = SVFUtil::dyn_cast<Instruction>(user);

        NodeID vthdnum = getIntArg(inst, 0);
        NodeID rthdnum = vthdTorthd[vthdnum];
        std::vector<std::string> x = getStringArg(inst, 1);

        for (std::vector<std::string>::iterator i = x.begin(); i != x.end(); i++)
        {
            rthdToChildren[rthdnum].insert(vthdTorthd[atoi((*i).c_str())]);
        }
    }
    return true;
}

bool MTAResultValidator::collectInterleavingTargets()
{

    // Collect call sites of all INTERLEV_ACCESS function calls.
    const Function* F = nullptr;
    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for(auto it = M.begin(); it != M.end(); it++)
        {
            const std::string fName = (*it).getName().str();
            if(fName.find(INTERLEV_ACCESS) != std::string::npos)
            {
                F = &(*it);
                break;
            }
        }
    }
    if (!F)
        return false;

    for (Value::const_use_iterator it = F->use_begin(), ie = F->use_end(); it != ie; ++it)
    {
        const Use *u = &*it;
        const Value* user = u->getUser();
        const Instruction* inst = SVFUtil::dyn_cast<Instruction>(user);

        NodeID vthdnum = getIntArg(inst, 0);
        NodeID rthdnum = vthdTorthd[vthdnum];
        CallStrCxt x = getCxtArg(inst, 1);
        std::vector<std::string> y = getStringArg(inst, 2);

        // Record given interleaving
        NodeBS lev;
        // Push thread itself into interleaving set
        lev.set(rthdnum);
        // Find rthd of given vthd and push it into interleaving set
        for (std::vector<std::string>::iterator i = y.begin(); i != y.end(); i++)
        {
            lev.set(vthdTorthd[atoi((*i).c_str())]);
        }

        const Instruction* memInst = getPreviousMemoryAccessInst(inst);
        const SVFInstruction* svfMemInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(memInst);
        CxtThreadStmt cts(rthdnum, x, svfMemInst);
        instToTSMap[svfMemInst].insert(cts);
        threadStmtToInterLeaving[cts] = lev;
    }
    return true;
}

bool MTAResultValidator::validateCxtThread()
{

    bool res = true;
    TCT* tct = mhp->getTCT();
    if (tct->getTCTNodeNum() != vthdToCxt.size())
    {
        res = false;
        if (Options::PrintValidRes())
        {
            outs() << errMsg("\nValidate CxtThread: The number of CxtThread is different from given result!!!\n");
            outs() << "Given threads:\t" << vthdToCxt.size() << "\nAnalysis result:\t" << tct->getTCTNodeNum() << "\n";
            assert(false && "test case failed!");
        }
    }

    Set<int> visitedvthd;

    for (NodeID i = 0; i < tct->getTCTNodeNum(); i++)
    {
        const CxtThread rthd = tct->getTCTNode(i)->getCxtThread();
        bool matched = false;
        for (Map<NodeID, CallStrCxt>::iterator j = vthdToCxt.begin(), ej = vthdToCxt.end(); j != ej; j++)
        {
            NodeID vthdid = (*j).first;
            if (matchCxt(rthd.getContext(), vthdToCxt[vthdid]))
            {
                if (visitedvthd.find(vthdid) != visitedvthd.end())
                {
                    res = false;
                    if (Options::PrintValidRes())
                    {
                        outs() << "\nValidate CxtThread: Repeat real CxtThread !!!\n";
                        rthd.dump();
                        tct->getTCTNode(vthdTorthd[vthdid])->getCxtThread().dump();
                    }
                }
                vthdTorthd[vthdid] = i;
                rthdTovthd[i] = vthdid;
                visitedvthd.insert(vthdid);
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            res = false;
            if (Options::PrintValidRes())
            {
                SVFUtil::errs() << errMsg("\nValidate CxtThread: Cannot match real CxtThread !!!\n");
                rthd.dump();
                assert(false && "test case failed!");
            }
        }
    }
    if (visitedvthd.size() != vthdToCxt.size())
    {
        res = false;
        if (Options::PrintValidRes())
        {
            SVFUtil::errs() << errMsg("\nValidate CxtThread: Some given CxtThreads cannot be found !!!\n");
            assert(false && "test case failed!");
            for (Map<NodeID, CallStrCxt>::iterator j = vthdToCxt.begin(), ej = vthdToCxt.end(); j != ej; j++)
            {
                NodeID vthdid = (*j).first;
                if (visitedvthd.find(vthdid) == visitedvthd.end())
                {
                    dumpCxt(vthdToCxt[vthdid]);
                }
            }
        }
    }
    return res;
}

bool MTAResultValidator::validateTCT()
{
    bool res = true;

    TCT* tct = mhp->getTCT();
    for (NodeID i = 0; i < tct->getTCTNodeNum(); i++)
    {
        bool res_node = true;
        TCTNode* pnode = tct->getTCTNode(i);
        for (TCT::ThreadCreateEdgeSet::const_iterator ci = tct->getChildrenBegin(pnode), cei = tct->getChildrenEnd(pnode); ci != cei;
                ci++)
        {
            NodeID tid = (*ci)->getDstID();
            if (rthdToChildren[i].find(tid) == rthdToChildren[i].end())
            {
                res = false;
                res_node = false;
            }
        }

        for (Set<NodeID>::iterator j = rthdToChildren[i].begin(), ej = rthdToChildren[i].end(); j != ej; j++)
        {
            NodeID gid = *j;
            if (!tct->hasGraphEdge(pnode, tct->getTCTNode(gid), TCTEdge::ThreadCreateEdge))
            {
                res = false;
                res_node = false;
            }
        }
        if ((!res_node) && Options::PrintValidRes())
        {
            outs() << errMsg("\nValidate TCT: Wrong at TID ") << rthdTovthd[i] << "\n";
            outs() << "Given children: \t";
            for (Set<NodeID>::iterator j = rthdToChildren[i].begin(), ej = rthdToChildren[i].end(); j != ej; j++)
            {
                NodeID gid = *j;
                outs() << rthdTovthd[gid] << ", ";
            }
            outs() << "\nAnalysis children:\t";
            for (TCT::ThreadCreateEdgeSet::const_iterator ci = tct->getChildrenBegin(pnode), cei = tct->getChildrenEnd(pnode); ci != cei;
                    ci++)
            {
                NodeID tid = (*ci)->getDstID();
                outs() << rthdTovthd[tid] << ", ";
            }
            outs() << "\n";
        }
    }
    return res;
}

MTAResultValidator::INTERLEV_FLAG MTAResultValidator::validateInterleaving()
{
    MTAResultValidator::INTERLEV_FLAG res = MTAResultValidator::INTERLEV_TRUE;

    for (MHP::InstToThreadStmtSetMap::iterator seti = instToTSMap.begin(), eseti = instToTSMap.end(); seti != eseti; ++seti)
    {
        const SVFInstruction* inst = (*seti).first;

        const MHP::CxtThreadStmtSet& tsSet = mhp->getThreadStmtSet(inst);

        if ((*seti).second.size() != tsSet.size())
        {
            if (Options::PrintValidRes())
            {
                outs() << errMsg("\n Validate Interleaving: Wrong at : ") << inst->getSourceLoc() << "\n";
                outs() << "Reason: The number of thread running on stmt is wrong\n";
                outs() << "\n----Given threads:\n";
                for (MHP::CxtThreadStmtSet::iterator thdlevi = (*seti).second.begin(), ethdlevi = (*seti).second.end(); thdlevi != ethdlevi;
                        ++thdlevi)
                {
                    outs() << "TID " << rthdTovthd[(*thdlevi).getTid()] << ": ";
                    dumpCxt((*thdlevi).getContext());
                }
                outs() << "\n----Analysis threads:\n";
                for (MHP::CxtThreadStmtSet::const_iterator it = tsSet.begin(), eit = tsSet.end(); it != eit; ++it)
                {
                    outs() << "TID " << rthdTovthd[(*it).getTid()] << ": ";
                    dumpCxt((*it).getContext());
                }
                outs() << "\n";
            }
            res = MTAResultValidator::INTERLEV_UNSOUND;
        }

        for (MHP::CxtThreadStmtSet::const_iterator it = tsSet.begin(), eit = tsSet.end(); it != eit; ++it)
        {
            const CxtThreadStmt& ts = *it;
            bool matched = false;
            for (MHP::CxtThreadStmtSet::iterator it2 = (*seti).second.begin(), eit2 = (*seti).second.end(); it2 != eit2; ++it2)
            {
                const CxtThreadStmt& ts2 = *it2;

                if (ts2.getTid() == ts.getTid() && matchCxt(ts2.getContext(), ts.getContext()))
                {
                    matched = true;
                    NodeBS lev = mhp->getInterleavingThreads(ts);
                    NodeBS lev2 = threadStmtToInterLeaving[ts2];
                    if (lev != lev2)
                    {
                        if (Options::PrintValidRes())
                        {
                            outs() << errMsg("\nValidate Interleaving: Wrong at: ") << inst->getSourceLoc() << "\n";
                            outs() << "Reason: thread interleaving on stmt is wrong\n";
                            dumpCxt(ts.getContext());
                            outs() << "Given result:    \tTID " << rthdTovthd[ts.getTid()];
                            dumpInterlev(lev2);
                            outs() << "Analysis result: \tTID " << rthdTovthd[ts.getTid()];
                            dumpInterlev(lev);
                        }

                        if (MTAResultValidator::INTERLEV_IMPRECISE > res)
                            res = MTAResultValidator::INTERLEV_IMPRECISE;

                        if (lev.count() >= lev2.count())
                        {
                            bool findeveryelement = true;
                            for (NodeBS::iterator it = lev2.begin(), eit = lev2.end(); it != eit; it++)
                            {
                                if (!lev.test(*it))
                                {
                                    findeveryelement = false;
                                    break;
                                }
                            }
                            if (!findeveryelement)
                                res = MTAResultValidator::INTERLEV_UNSOUND;
                        }
                        else
                            res = MTAResultValidator::INTERLEV_UNSOUND;
                    }
                }
            }

            if (!matched)
            {
                if (Options::PrintValidRes())
                {
                    outs() << errMsg("\nValidate Interleaving: Wrong at:") << inst->getSourceLoc() << "\n";
                    outs() << "Reason: analysis thread cxt is not matched by given thread cxt\n";
                    dumpCxt(ts.getContext());
                    NodeBS lev = mhp->getInterleavingThreads(ts);

                    outs() << "Analysis result: \tTID " << rthdTovthd[ts.getTid()];
                    dumpInterlev(lev);
                }
                res = MTAResultValidator::INTERLEV_UNSOUND;
            }
        }
    }
    return res;
}
