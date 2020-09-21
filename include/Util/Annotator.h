/*
 * Annotator.h
 *
 *  Created on: May 4, 2014
 *      Author: Yulei Sui
 */

#ifndef ANNOTATOR_H_
#define ANNOTATOR_H_

#include "Util/BasicTypes.h"
#include <vector>

namespace SVF
{

/*!
 * Program annotator to write meta data information on LLVM IR
 */
class Annotator
{

public:
    /// Constructor
    Annotator()
    {
        SB_SLICESOURCE = "SOURCE_";
        SB_SLICESINK = "SINK_";
        SB_FESIBLE = "FESIBLE_";
        SB_INFESIBLE = "INFESIBLE_";

        DR_NOT_CHECK = "DRNOTCHECK_";
        DR_CHECK = "DRCHECK_";
    }

    /// Destructor
    virtual ~Annotator()
    {

    }

    /// SB Has flag methods
    //@{
    inline bool hasSBSourceFlag(Instruction *inst) const
    {
        std::vector<Value *> values;
        return evalMDTag(inst, inst, SB_SLICESOURCE, values);
    }
    inline bool hasSBSinkFlag(Instruction *inst) const
    {
        std::vector<Value *> values;
        return evalMDTag(inst, inst, SB_SLICESINK, values);
    }
    //@}

    /// Race Detection Has flag methods
    //@{
    inline bool hasDRNotCheckFlag(Instruction *inst) const
    {
        //std::vector<Value *> values;
        //return evalMDTag(inst, inst, DR_NOT_CHECK, values);
        if (inst->getMetadata(DR_NOT_CHECK))
            return true;
        else
            return false;
    }
    inline bool hasDRNotCheckFlag(const Instruction *inst) const
    {
        //std::vector<Value *> values;
        //return evalMDTag(inst, inst, DR_NOT_CHECK, values);
        if (inst->getMetadata(DR_NOT_CHECK))
            return true;
        else
            return false;
    }

    inline bool hasDRCheckFlag(Instruction *inst) const
    {
        //std::vector<Value *> values;
        //return evalMDTag(inst, inst, DR_CHECK, values);
        if (inst->getMetadata(DR_CHECK))
            return true;
        else
            return false;
    }
    inline bool hasDRCheckFlag(const Instruction *inst) const
    {
        //std::vector<Value *> values;
        //return evalMDTag(inst, inst, DR_CHECK, values);
        if (inst->getMetadata(DR_CHECK))
            return true;
        else
            return false;
    }
    //@}

    /// Simple add/remove meta data information
    //@{
    inline void addMDTag(Instruction *inst, std::string str)
    {
        addMDTag(inst, inst, str);
    }
    inline void removeMDTag(Instruction *inst, std::string str)
    {
        removeMDTag(inst, inst, str);
    }
    //@}

    /// manipulate llvm meta data on instructions for a specific value
    //@{
    /// add flag to llvm metadata
    inline void addMDTag(Instruction *inst, Value *val, std::string str)
    {
        assert(!val->getType()->isVoidTy() && "expecting non-void value for MD!");
        std::vector<Value *> values;
        //std::vector<llvm::Metadata *> metavalues;
        // add the flag if we did not see it before
        if (evalMDTag(inst, val, str, values) == false)
        {

            values.push_back(val);
            //llvm::ArrayRef<llvm::Metadata*> ar(metavalues);
            // FIXME: delete the old MDNode
            inst->setMetadata(str, MDNode::get(inst->getContext(), llvm::None));
            //inst->setMetadata(str, llvm::MDNode::get(inst->getContext(), ar));
        }
    }

    /// remove flag from llvm metadata
    inline void removeMDTag(Instruction *inst, Value *val, std::string str)
    {
        assert(!val->getType()->isVoidTy() && "expecting non-void value for MD!");
        std::vector<Value *> values;

        // remove the flag if it is there
        if (evalMDTag(inst, val, str, values) == true)
        {
            llvm::ArrayRef<Value *> ar(values);
            // FIXME: delete the old MDNode
            //inst->setMetadata(str, llvm::MDNode::get(inst->getContext(), ar));
        }
    }
    //@}

private:

    /// evaluate llvm metadata
    inline bool evalMDTag(const Instruction *inst, const Value *val, std::string str,
                          std::vector<Value *>&) const
    {

        assert(val && "value should not be null");

        bool hasFlag = false;
        if (MDNode *mdNode = inst->getMetadata(str))
        {
            /// When mdNode has operands and value is not null
            for (unsigned k = 0; k < mdNode->getNumOperands(); ++k)
            {
                //Value *v = mdNode->getOperand(k);
                // if (v == val)
                //    hasFlag = true;
                //else
                //    values.push_back(v);
            }
        }
        return hasFlag;
    }

protected:

    /// Saber annotations
    //@{
    const char* SB_SLICESOURCE;
    const char* SB_SLICESINK;
    const char* SB_FESIBLE;
    const char* SB_INFESIBLE;
    //@}

    /// Race Detection annotations
    //@{
    const char* DR_NOT_CHECK;
    const char* DR_CHECK;
    //@}
};

} // End namespace SVF

#endif /* ANNOTATOR_H_ */
