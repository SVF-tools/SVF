#include "SVFIR/SVFType.h"
#include <sstream>
#include "SVFIR/GraphDBClient.h"

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

std::string SVFFunctionType::toDBString() const
{
    std::string is_single_val_ty = isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFFunctionType {id:" + std::to_string(getId()) +
    ", svf_i8_type_id:" + std::to_string(getSVFInt8Type()->getId()) +
    ", svf_ptr_type_id:" + std::to_string(getSVFPtrType()->getId()) + 
    ", kind:" + std::to_string(getKind()) + 
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(getByteSize()) +
    ", params_types_vec:'" + GraphDBClient::getInstance().extractSVFTypes(getParamTypes()) +
    "', ret_ty_node_id:" + std::to_string(getReturnType()->getId()) + "})";
    return queryStatement;
}

std::string StInfo::toDBString() const
{
    const std::string queryStatement ="CREATE (n:StInfo {st_info_id:" + std::to_string(getStinfoId()) +
    ", fld_idx_vec:'" + GraphDBClient::getInstance().extractIdxs(getFlattenedFieldIdxVec()) +
    "', elem_idx_vec:'" + GraphDBClient::getInstance().extractIdxs(getFlattenedElemIdxVec()) + 
    "', finfo_types:'" + GraphDBClient::getInstance().extractSVFTypes(getFlattenFieldTypes()) + 
    "', flatten_element_types:'" + GraphDBClient::getInstance().extractSVFTypes(getFlattenElementTypes()) + 
    "', fld_idx_2_type_map:'" + GraphDBClient::getInstance().extractFldIdx2TypeMap(getFldIdx2TypeMap()) +
    "', stride:" + std::to_string(getStride()) +
    ", num_of_flatten_elements:" + std::to_string(getNumOfFlattenElements()) +
    ", num_of_flatten_fields:" + std::to_string(getNumOfFlattenFields()) + "})";
    return queryStatement;
}

std::string SVFStructType::toDBString() const
{
    std::string is_single_val_ty = isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFStructType {id:" + std::to_string(getId()) +
    ", svf_i8_type_id:" + std::to_string(getSVFInt8Type()->getId()) +
    ", svf_ptr_type_id:" + std::to_string(getSVFPtrType()->getId()) + 
    ", kind:" + std::to_string(getKind()) + 
    ", stinfo_node_id:" + std::to_string(getTypeInfo()->getStinfoId()) +
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(getByteSize()) +
    ", struct_name:'" + getName() + "'" +
    ", fields_id_vec:'" + GraphDBClient::getInstance().extractSVFTypes(getFieldTypes()) +
    "'})";
    return queryStatement;
}

} // namespace SVF