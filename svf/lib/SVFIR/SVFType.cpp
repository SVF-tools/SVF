#include "SVFIR/SVFType.h"

namespace SVF
{

std::ostream& operator<<(std::ostream& OS, const SVFType& type)
{
    type.print(OS);
    return OS;
}

void SVFPointerType::print(std::ostream& OS) const
{
    getPtrElementType()->print(OS);
    OS << "*";
}

void SVFIntegerType::print(std::ostream& OS) const
{
    // Making it more informative?
    OS << "I";
}

void SVFFunctionType::print(std::ostream& OS) const
{
    OS << *getReturnType() << "()";
}

void SVFStructType::print(std::ostream& OS) const
{
    OS << "S." << name;
}

void SVFArrayType::print(std::ostream& OS) const
{
    OS << "[" << numOfElement << "x" << *typeOfElement << "]";
}

void SVFOtherType::print(std::ostream& OS) const
{
    OS << repr;
}

} // namespace SVF
