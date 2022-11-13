#ifndef SVFINSTRUCTION_H_
#define SVFINSTRUCTION_H_

#include "Util/BasicTypes.h"

namespace SVF
{
    class SVFFunction;

    class SVFInstruction: public SVFValue
    {
    private:
        const BasicBlock* parentBB;
        bool isCallBase;

    public:
        SVFInstruction(const std::string instructionName, const BasicBlock* parentBB): SVFValue(instructionName,SVFValue::SVFInst), parentBB(parentBB)
        {
        }

        inline const BasicBlock* getParent() const
        {
            return parentBB;
        }

        inline const bool isCallBase() const
        {
            return isCallBase;
        }

        inline const void setIsCallBase(const bool isCallBase)
        {
            this->isCallBase = isCallBase;
        }

    };

};

#endif