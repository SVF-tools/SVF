#include "Util/BasicTypes.h"
#include "Util/SVFUtil.h"
#include <llvm/IR/Instructions.h>

using namespace SVF;
using namespace SVFUtil;

const SVFLoopAndDomInfo::LoopBBs& SVFLoopAndDomInfo::getLoopInfo(const SVFBasicBlock* bb) const
{
    assert(hasLoopInfo(bb) && "loopinfo does not exit (bb not in a loop)");
    Map<const SVFBasicBlock*, LoopBBs>::const_iterator mapIter = bb2LoopMap.find(bb);
    return mapIter->second;
}

void SVFLoopAndDomInfo::getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const
{
    if (hasLoopInfo(bb))
    {
        const LoopBBs blocks = getLoopInfo(bb);
        if (!blocks.empty())
        {
            for (const SVFBasicBlock* block : blocks)
            {
                for (const SVFBasicBlock* succ : block->getSuccessors())
                {
                    if ((std::find(blocks.begin(), blocks.end(), succ)==blocks.end()))
                        exitbbs.push_back(succ);
                }
            }
        }
    }
}

bool SVFLoopAndDomInfo::dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
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

    const Map<const SVFBasicBlock*,BBSet>& dtBBsMap = getDomTreeMap();
    Map<const SVFBasicBlock*,BBSet>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const BBSet & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }

    return false;
}

bool SVFLoopAndDomInfo::postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
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

    const Map<const SVFBasicBlock*,BBSet>& dtBBsMap = getPostDomTreeMap();
    Map<const SVFBasicBlock*,BBSet>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const BBSet & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }
    return false;
}

bool SVFLoopAndDomInfo::isLoopHeader(const SVFBasicBlock* bb) const
{
    if (hasLoopInfo(bb))
    {
        const LoopBBs& blocks = getLoopInfo(bb);
        assert(!blocks.empty() && "no available loop info?");
        return blocks.front() == bb;
    }
    return false;
}

SVFFunction::SVFFunction(const Function* f, bool declare, bool intric, SVFLoopAndDomInfo* ld): SVFValue(f,SVFValue::SVFFunc),
    isDecl(declare), intricsic(intric),realDefFun(nullptr), exitBB(nullptr), loopAndDom(ld), isUncalled(false), isNotRet(false), varArg(f->isVarArg())
{
}

SVFFunction::~SVFFunction()
{
    for(const SVFBasicBlock* bb : allBBs)
        delete bb;
    for(const SVFArgument* arg : allArgs)
        delete arg;
    delete loopAndDom;
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

SVFBasicBlock::SVFBasicBlock(const BasicBlock* b, const SVFFunction* f):
    SVFValue(b,SVFValue::SVFBB), bb(b), fun(f), terminatorInst(nullptr)
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
        ++pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}
u32_t SVFBasicBlock::getBBPredecessorPos(const SVFBasicBlock* succbb) const
{
    u32_t pos = 0;
    for (const SVFBasicBlock* PredBB : succbb->getPredecessors())
    {
        if(PredBB == this)
            return pos;
        ++pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}

SVFInstruction::SVFInstruction(const llvm::Instruction* i, const SVFBasicBlock* b, bool isRet, SVFValKind k):
    SVFValue(i, k), inst(i), bb(b), fun(bb->getParent()), terminator(i->isTerminator()), ret(isRet)
{
}