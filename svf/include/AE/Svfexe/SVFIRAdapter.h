//===- SVFIRAdapter.h -- SVFIR to abstract-domain symbols ----*- C++ -*-===//

#ifndef SVF_AE_SVFIR_ADAPTER_H
#define SVF_AE_SVFIR_ADAPTER_H

#include "AE/Core/BoxProgramState.h"

#include <map>
#include <vector>

namespace SVF
{

class FunObjVar;
class ObjVar;
class SVFIR;
class ValVar;

/// Owns the IR-specific identity mapping. Abstract-domain states only see
/// Variable and Location; they never depend on SVF NodeID or SVFIR classes.
class SVFIRAdapter
{
public:
    explicit SVFIRAdapter(const SVFIR& svfir);

    bool contains(const ValVar& value) const;
    bool contains(const ObjVar& object) const;

    AbstractDomain::Variable variable(const ValVar& value) const;
    const ValVar* value(AbstractDomain::Variable variable) const;
    const AbstractDomain::VariableDeclaration& declaration(
        AbstractDomain::Variable variable) const;
    AbstractDomain::Location location(const ObjVar& object) const;
    AbstractDomain::Variable contentVariable(const ObjVar& object) const;
    const ObjVar* contentObject(AbstractDomain::Variable variable) const;
    const ObjVar& object(AbstractDomain::Location location) const;

    const AbstractDomain::VariableEnvironment& environment(
        const FunObjVar* function = nullptr) const;
    const AbstractDomain::VariableEnvironment& scalarEnvironment(
        const FunObjVar* function = nullptr) const;
    const AbstractDomain::VariableEnvironment& allScalarEnvironment() const
    {
        return allScalarEnvironment_;
    }

    const AbstractDomain::MemoryLayout& memoryLayout() const
    {
        return memoryLayout_;
    }

    AbstractDomain::LinearExpression linearExpression(
        const std::vector<std::pair<const ValVar*, AbstractDomain::Rational>>&
            terms,
        AbstractDomain::Rational constant = {}) const;
    AbstractDomain::TreeExpression treeExpression(const ValVar& value) const;

private:
    std::map<const ValVar*, AbstractDomain::Variable> variables_;
    std::vector<const ValVar*> valuesByVariableId_;
    std::map<AbstractDomain::Variable, AbstractDomain::VariableDeclaration>
        declarations_;
    std::map<const ObjVar*, AbstractDomain::Location> locations_;
    std::map<AbstractDomain::Location, const ObjVar*> objects_;
    std::map<const ObjVar*, AbstractDomain::Variable> contentVariables_;
    std::vector<const ObjVar*> contentObjectsByVariableId_;
    AbstractDomain::VariableEnvironment globalEnvironment_;
    AbstractDomain::VariableEnvironment allScalarEnvironment_;
    AbstractDomain::MemoryLayout memoryLayout_;
};

} // namespace SVF

#endif // SVF_AE_SVFIR_ADAPTER_H
