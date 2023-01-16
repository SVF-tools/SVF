#include "SVFIR/DumpHelper.h"
#include <sstream>

using namespace SVF;

TypeIndex DumpInfo::getTypeIndex(const SVFType* type)
{
    if (type == nullptr)
    {
        return 0;
    }
    auto pair = typeToIndex.emplace(type, 1 + allTypes.size());
    if (pair.second)
    {
        // The type was NOT recorded before. It gets inserted.
        allTypes.push_back(type);
    }
    return pair.first->second;
}

ValueIndex DumpInfo::getValueIndex(const SVFValue* value)
{
    if (value == nullptr)
    {
        return 0;
    }
    auto pair = valueToIndex.emplace(value, 1 + allValues.size());
    if (pair.second)
    {
        // The type was NOT recorded before. It gets inserted.
        allValues.push_back(value);
    }
    return pair.first->second;
}


const char* DumpInfo::getStrOfIndex(std::size_t index)
{
    // Invariant: forall i: allIndices[i] == hex(i) /\ len(allIndices) == N
    for (std::size_t i = allIndices.size(); i <= index; ++i)
    {
        std::stringstream ss;
        ss << std::hex << i;
        allIndices.push_back(ss.str());
    }
    
    return allIndices[index].c_str();
    // Postcondition: ensures len(allIndices) >= index + 1
}

const char* DumpInfo::getStrValueIndex(const SVFValue* value)
{
    return getStrOfIndex(getValueIndex(value));
}

const char* DumpInfo::getStrTypeIndex(const SVFType* type)
{
    return getStrOfIndex(getTypeIndex(type));
}