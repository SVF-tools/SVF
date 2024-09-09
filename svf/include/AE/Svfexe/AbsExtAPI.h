#pragma once
#include "AE/Core/AbstractState.h"
#include "AE/Core/ICFGWTO.h"
#include "AE/Svfexe/AEDetector.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "Util/SVFBugReport.h"
#include "WPA/Andersen.h"

namespace SVF {
class AbstractInterpretation;
class AbsExtAPI
{
public:
    enum ExtAPIType { UNCLASSIFIED, MEMCPY, MEMSET, STRCPY, STRCAT };

    AbsExtAPI(Map<const ICFGNode*, AbstractState>&);

    void initExtFunMap();
    std::string strRead(AbstractState& as, const SVFVar* rhs);
    void handleExtAPI(const CallICFGNode *call);
    void handleStrcpy(const CallICFGNode *call);
    IntervalValue getStrlen(AbstractState& as, const SVF::SVFVar *strValue);
    void handleStrcat(const SVF::CallICFGNode *call);

    void handleMemcpy(AbstractState& as, const SVF::SVFVar *dst, const SVF::SVFVar *src, IntervalValue len,  u32_t start_idx);
    void handleMemset(AbstractState& as, const SVFVar* dst,
                      IntervalValue elem, IntervalValue len);
    IntervalValue getRangeLimitFromType(const SVFType* type);
    AbstractState& getAbsStateFromTrace(const ICFGNode* node)
    {
        const ICFGNode* repNode = icfg->getRepNode(node);
        if (abstractTrace.count(repNode) == 0)
        {
            assert(0 && "No preAbsTrace for this node");
        }
        else
        {
            return abstractTrace[repNode];
        }
    }

    const SVFVar* getSVFVar(const SVFValue* val);

protected:
    SVFIR* svfir;
    ICFG* icfg;
    Map<const ICFGNode*, AbstractState>& abstractTrace;
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map;
};
}