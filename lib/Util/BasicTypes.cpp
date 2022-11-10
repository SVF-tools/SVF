#include "Util/BasicTypes.h"
#include "Util/SVFUtil.h"
#include <llvm/IR/Instructions.h>

using namespace SVF;
using namespace SVFUtil;

SVFFunction::SVFFunction(Function* f): SVFValue(f->getName().str(),SVFValue::SVFFunc),
        isDecl(f->isDeclaration()), isIntri(f->isIntrinsic()), fun(f), exitBB(nullptr), isUncalled(false), isNotRet(false)
{
}

u32_t SVFFunction::arg_size() const
{
    return getLLVMFun()->arg_size();
}

const Value* SVFFunction::getArg(u32_t idx) const
{
    return getLLVMFun()->getArg(idx);
}

bool SVFFunction::isVarArg() const
{
    return getLLVMFun()->isVarArg();
}

const std::vector<const BasicBlock*>& SVFFunction::getLoopInfo(const BasicBlock* bb) const
{
    Map<const BasicBlock*, std::vector<const BasicBlock*>>::const_iterator mapIter = bb2LoopMap.find(bb);
    if(mapIter != bb2LoopMap.end())
        return mapIter->second;
    else
    {
        assert(SVFFunction::hasLoopInfo(bb) && "loopinfo does not exit(bb not in a loop)");
        abort();
    }
}

void SVFFunction::getExitBlocksOfLoop(const BasicBlock* bb, Set<const BasicBlock*>& exitbbs) const
{
    if (SVFFunction::hasLoopInfo(bb))
    {
        const std::vector<const BasicBlock*> blocks = getLoopInfo(bb);
        if (!blocks.empty())
        {
            for (const BasicBlock* block : blocks)
            {
                for (succ_const_iterator succIt = succ_begin(block); succIt != succ_end(block); succIt++)
                {
                    const BasicBlock* succ = *succIt;
                    if ((std::find(blocks.begin(), blocks.end(), succ)==blocks.end()))
                        exitbbs.insert(succ);
                }
            }
        }
    }
}

bool SVFFunction::dominate(const BasicBlock* bbKey, const BasicBlock* bbValue) const
{
    if (bbKey == bbValue)
        return true;

    // An unreachable node is dominated by anything.
    if (isUnreachable(bbValue))
    {
        return true;
    }

    // And dominates nothing.
    if (isUnreachable(bbKey))
    {
        return false;
    }

    const Map<const BasicBlock*,Set<const BasicBlock*>>& dtBBsMap = getDomTreeMap();
    Map<const BasicBlock*,Set<const BasicBlock*>>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const Set<const BasicBlock*> & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }

    return false;
}

bool SVFFunction::postDominate(const BasicBlock* bbKey, const BasicBlock* bbValue) const
{
    if (bbKey == bbValue)
        return true;

    // An unreachable node is dominated by anything.
    if (isUnreachable(bbValue))
    {
        return true;
    }

    // And dominates nothing.
    if (isUnreachable(bbKey))
    {
        return false;
    }

    const Map<const BasicBlock*,Set<const BasicBlock*>>& dtBBsMap = getPostDomTreeMap();
    Map<const BasicBlock*,Set<const BasicBlock*>>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const Set<const BasicBlock*> & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }
    return false;
}

bool SVFFunction::isLoopHeader(const BasicBlock* bb) const
{
    if (hasLoopInfo(bb))
    {
        const std::vector<const BasicBlock*>& blocks = getLoopInfo(bb);
        assert(!blocks.empty() && "no available loop info?");
        return blocks.front() == bb;
    }
    return false;
}

SVFBasicBlock::SVFBasicBlock(const BasicBlock* b, const SVFFunction* f): 
    SVFValue(b->getName().str(),SVFValue::SVFBB), bb(b), fun(f)
{
}

SVFInstruction::SVFInstruction(const llvm::Instruction* i, const SVFBasicBlock* b): 
    SVFValue(i->getName().str(), SVFInst), inst(i), bb(b), fun(bb->getParent()), 
    type(i->getType()), terminator(i->isTerminator())
{
}