#ifndef SVFFUNCTION_H_
#define SVFFUNCTION_H_

#include "Util/BasicTypes.h"

class SVFFunction
{
    private:
        std::string functionName;
    public:
        SVFFunction();
        SVFFunction(std::string functionName);
        std::string getFunctionName();
        void setFunctionName(std::string functionName);
};

#endif