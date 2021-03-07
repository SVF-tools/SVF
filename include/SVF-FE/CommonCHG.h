//===----- CommonCHG.h -- Base class of CHG implementations ------------------//
// A common base to CHGraph and DCHGraph.

/*
 * CommonCHG.h
 *
 *  Created on: Aug 24, 2019
 *      Author: Mohamad Barbar
 */

#ifndef COMMONCHG_H_
#define COMMONCHG_H_

#include "Util/BasicTypes.h"

namespace SVF
{

using VTableSet = Set<const GlobalValue *>;
using VFunSet = Set<const SVFFunction *>;

/// Common base for class hierarchy graph. Only implements what PointerAnalysis needs.
class CommonCHGraph
{
public:
    virtual ~CommonCHGraph() { };
    enum CHGKind
    {
        Standard,
        DI
    };

    virtual bool csHasVFnsBasedonCHA(CallSite cs) = 0;
    virtual const VFunSet &getCSVFsBasedonCHA(CallSite cs) = 0;
    virtual bool csHasVtblsBasedonCHA(CallSite cs) = 0;
    virtual const VTableSet &getCSVtblsBasedonCHA(CallSite cs) = 0;
    virtual void getVFnsFromVtbls(CallSite cs, const VTableSet &vtbls, VFunSet &virtualFunctions) = 0;

    CHGKind getKind() const
    {
        return kind;
    }

protected:
    CHGKind kind;
};

} // End namespace SVF

#endif /* COMMONCHG_H_ */

