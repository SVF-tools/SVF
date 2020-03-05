#include "Util/SVFFunction.h"

using namespace SVFUtil;

SVFFunction::SVFFunction(){}

SVFFunction::SVFFunction(std::string functionName){
    this->functionName = functionName;
}

std::string SVFFunction::getFunctionName(){
    return this->functionName;
}

void SVFFunction::setFunctionName(std::string functionName){
    this->functionName = functionName;
}