#ifndef SVFARGUMENTS_H_
#define SVFARGUMENTS_H_

#include "Util/BasicTypes.h"

namespace SVF
{
    class SVFFunction;

    class SVFArgument: public SVFValue
    {
    private:
        const SVFFunction* parentFunction;
        const unsigned argNo;

    public:
        SVFArgument(const std::string argName, const SVFFunction* parentFunction, const unsigned argNo): SVFValue(argName,SVFValue::SVFArg), parentFunction(parentFunction),argNo(argNo)
        {
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