#include "Util/SVFBugReport.h"
#include "Graphs/ICFGNode.h"
#include <cassert>
#include <cstring>

int main()
{
    using namespace SVF;
    GlobalICFGNode node(0);
    GenericBug::EventStack events{SVFBugEvent(SVFBugEvent::SourceInst, &node)};
    FullBufferOverflowBug finite(events, 8, 8, 8, 9);
    cJSON* json = finite.getBugDescription();
    assert(cJSON_GetObjectItem(json, "AllocLowerBound")->valuedouble == 8);
    assert(cJSON_GetObjectItem(json, "AllocUpperBound")->valuedouble == 8);
    assert(cJSON_GetObjectItem(json, "AccessLowerBound")->valuedouble == 8);
    assert(cJSON_GetObjectItem(json, "AccessUpperBound")->valuedouble == 9);
    cJSON_Delete(json);

    FullBufferOverflowBug unknown(events, BoundedInt::minus_infinity(),
                                  BoundedInt::plus_infinity(),
                                  BoundedInt::minus_infinity(),
                                  BoundedInt::plus_infinity());
    json = unknown.getBugDescription();
    assert(std::strcmp(cJSON_GetObjectItem(json, "AllocLowerBound")->valuestring, "-oo") == 0);
    assert(std::strcmp(cJSON_GetObjectItem(json, "AllocUpperBound")->valuestring, "+oo") == 0);
    assert(std::strcmp(cJSON_GetObjectItem(json, "AccessLowerBound")->valuestring, "-oo") == 0);
    assert(std::strcmp(cJSON_GetObjectItem(json, "AccessUpperBound")->valuestring, "+oo") == 0);
    cJSON_Delete(json);
    unknown.printBugToTerminal();
}
