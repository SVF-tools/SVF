#include "SVFIR/SVFType.h"
#include <sstream>

namespace SVF
{
std::string SVFType::toString() const
{
    std::ostringstream os;
    print(os);
    return os.str();
}

std::ostream& operator<<(std::ostream& os, const SVFType& type)
{
    type.print(os);
    return os;
}

void SVFPointerType::print(std::ostream& os) const
{
    os << *getPtrElementType() << '*';
}

void SVFIntegerType::print(std::ostream& os) const
{
    if (signAndWidth < 0)
        os << 'i' << -signAndWidth;
    else
        os << 'u' << signAndWidth;
}

void SVFFunctionType::print(std::ostream& os) const
{
    os << *getReturnType() << "()";
}

void SVFStructType::print(std::ostream& os) const
{
    os << "S." << name;
}

void SVFArrayType::print(std::ostream& os) const
{
    os << '[' << numOfElement << 'x' << *typeOfElement << ']';
}

void SVFOtherType::print(std::ostream& os) const
{
    os << repr;
}

} // namespace SVF
