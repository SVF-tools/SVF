#include "SVFIR/SVFType.h"
#include <sstream>

namespace SVF
{

SVFType* SVFType::svfI8Ty = nullptr;
SVFType* SVFType::svfPtrTy = nullptr;

__attribute__((weak))
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
    os << "ptr";
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
    os << *getReturnType() << "(";

    // Print parameters
    for (size_t i = 0; i < params.size(); ++i)
    {
        os << *params[i];
        // Add comma after all params except the last one
        if (i != params.size() - 1)
        {
            os << ", ";
        }
    }

    // Add varargs indicator if needed
    if (isVarArg())
    {
        if (!params.empty())
        {
            os << ", ";
        }
        os << "...";
    }

    os << ")";
}

void SVFStructType::print(std::ostream& os) const
{
    os << "S." << name << " {";

    // Print fields
    for (size_t i = 0; i < fields.size(); ++i)
    {
        os << *fields[i];
        // Add comma after all fields except the last one
        if (i != fields.size() - 1)
        {
            os << ", ";
        }
    }
    os << "}";
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
