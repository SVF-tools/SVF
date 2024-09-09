#pragma once
#include "AE/Core/AbstractState.h"
#include "AE/Core/ICFGWTO.h"
#include "AE/Svfexe/AEDetector.h"
#include "AE/Svfexe/AbsInterpretationUtils.h"
#include "Util/SVFBugReport.h"
#include "WPA/Andersen.h"

namespace SVF {
class AbstractInterpretation;
class AbsInterpretationUtils {// AbsExtAPI
public:
    enum ExtAPIType { UNCLASSIFIED, MEMCPY, MEMSET, STRCPY, STRCAT };

    AbsInterpretationUtils(Map<const ICFGNode*, AbstractState>&);

    void initExtFunMap();
    std::string strRead(AbstractState& as, const SVFValue* rhs);
    void handleExtAPI(const CallICFGNode *call);
    void handleStrcpy(const CallICFGNode *call);
    IntervalValue getStrlen(AbstractState& as, const SVF::SVFValue *strValue);
    void handleStrcat(const SVF::CallICFGNode *call);
    // SVFValue -> ICFGNode SVFVar
    void handleMemcpy(AbstractState& as, const SVF::SVFValue *dst, const SVF::SVFValue *src, IntervalValue len,  u32_t start_idx);
    void handleMemset(AbstractState& as, const SVFValue* dst,
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

protected:
    SVFIR* svfir;
    ICFG* icfg;
    Map<const ICFGNode*, AbstractState>& abstractTrace;
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map;
};
}