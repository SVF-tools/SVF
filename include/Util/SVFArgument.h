#ifndef SVFARGUMENTS_H_
#define SVFARGUMENTS_H_

#include "Util/BasicTypes.h"

namespace SVF
{

    class SVFArgument: public SVFValue
    {
    private:
        const Argument* arg;
        const SVFFunction* parentFunction;
        unsigned argNo;

    public:
        SVFArgument(const Argument* arg, const SVFFunction* parentFunction): SVFValue(arg->getName().str(),SVFValue::SVFArg),
        arg(arg), parentFunction(parentFunction),argNo(arg->getArgNo())
        {
        }

        inline const Argument* getLLVMArgument()
        {
            assert(arg && "no LLVM Argument found!");
            return arg;
        }

        unsigned getArgNo() const 
        {
            assert(parentFunction && "can't get number of unparented arg");
            return argNo;
        }

        inline const SVFFunction* getParent() const
        {
            return parentFunction;
        }

    };

};

#endif