#include "Util/BasicTypes.h"
#include "Util/SVFUtil.h"
#include <llvm/IR/Instructions.h>

using namespace SVF;
using namespace SVFUtil;

SVFFunction::SVFFunction(const Function* f): SVFValue(f,SVFValue::SVFFunc),
        isDecl(f->isDeclaration()), isIntri(f->isIntrinsic()), fun(f), exitBB(nullptr), isUncalled(false), isNotRet(false), varArg(f->isVarArg())
{
}

SVFFunction::~SVFFunction()
{
    for(const SVFBasicBlock* bb : allBBs)
        delete bb;
    for(const SVFArgument* arg : allArgs)
        delete arg;
}

u32_t SVFFunction::arg_size() const
{
    return allArgs.size();
}

const SVFArgument* SVFFunction::getArg(u32_t idx) const
{
    assert (idx < allArgs.size() && "getArg() out of range!");
    return allArgs[idx];
}

bool SVFFunction::isVarArg() const
{
    return varArg;
}

const std::vector<const SVFBasicBlock*>& SVFFunction::getLoopInfo(const SVFBasicBlock* bb) const
{
    Map<const SVFBasicBlock*, std::vector<const SVFBasicBlock*>>::const_iterator mapIter = bb2LoopMap.find(bb);
    if(mapIter != bb2LoopMap.end())
        return mapIter->second;
    else
    {
        assert(SVFFunction::hasLoopInfo(bb) && "loopinfo does not exit(bb not in a loop)");
        abort();
    }
}

void SVFFunction::getExitBlocksOfLoop(const SVFBasicBlock* bb, Set<const SVFBasicBlock*>& exitbbs) const
{
    if (SVFFunction::hasLoopInfo(bb))
    {
        const std::vector<const SVFBasicBlock*> blocks = getLoopInfo(bb);
        if (!blocks.empty())
        {
            for (const SVFBasicBlock* block : blocks)
            {
                for (const SVFBasicBlock* succ : block->getSuccessors())
                {
                    if ((std::find(blocks.begin(), blocks.end(), succ)==blocks.end()))
                        exitbbs.insert(succ);
                }
            }
        }
    }
}

bool SVFFunction::dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
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

    const Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& dtBBsMap = getDomTreeMap();
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const Set<const SVFBasicBlock*> & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }

    return false;
}

bool SVFFunction::postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
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

    const Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& dtBBsMap = getPostDomTreeMap();
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const Set<const SVFBasicBlock*> & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }
    return false;
}

bool SVFFunction::isLoopHeader(const SVFBasicBlock* bb) const
{
    if (hasLoopInfo(bb))
    {
        const std::vector<const SVFBasicBlock*>& blocks = getLoopInfo(bb);
        assert(!blocks.empty() && "no available loop info?");
        return blocks.front() == bb;
    }
    return false;
}

SVFBasicBlock::SVFBasicBlock(const BasicBlock* b, const SVFFunction* f): 
    SVFValue(b,SVFValue::SVFBB), bb(b), fun(f)
{
    name = b->hasName() ? b->getName().str(): "";
}

SVFBasicBlock::~SVFBasicBlock()
{
    for(const SVFInstruction* inst : allInsts)
        delete inst;
}

/*!
 * Get position of a successor basic block
 */
u32_t SVFBasicBlock::getBBSuccessorPos(const SVFBasicBlock* Succ)
{
    u32_t i = 0;
    for (const SVFBasicBlock* SuccBB: succBBs)
    {
        if (SuccBB == Succ)
            return i;
        i++;
    }
    assert(false && "Didn't find succesor edge?");
    return 0;
}

u32_t SVFBasicBlock::getBBSuccessorPos(const SVFBasicBlock* Succ) const
{
    u32_t i = 0;
    for (const SVFBasicBlock* SuccBB: succBBs)
    {
        if (SuccBB == Succ)
            return i;
        i++;
    }
    assert(false && "Didn't find succesor edge?");
    return 0;
}


/*!
 * Return a position index from current bb to it successor bb
 */
u32_t SVFBasicBlock::getBBPredecessorPos(const SVFBasicBlock* succbb)
{
    u32_t pos = 0;
    for (const SVFBasicBlock* PredBB : succbb->getPredecessors())
    {
        if(PredBB == this)
            return pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}

SVFInstruction::SVFInstruction(const llvm::Instruction* i, const SVFBasicBlock* b, bool isRet): 
    SVFValue(i, SVFInst), inst(i), bb(b), fun(bb->getParent()), terminator(i->isTerminator()), ret(isRet)
{
}