#ifndef INCLUDE_SVF_FE_GRAPH2JSON_H_
#define INCLUDE_SVF_FE_GRAPH2JSON_H_

#include "MemoryModel/SVFIR.h"
#include "Graphs/ICFG.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/ICFGEdge.h"
#include "Util/SVFUtil.h"

namespace SVF
{

class GraphWriter;
class ICFGPrinter : public ICFG
{
public:
    ICFGPrinter();

    void printICFGToJson(const std::string& filename);

    std::string getPAGNodeKindValue(int kind);

    std::string getPAGEdgeKindValue(int kind);

    std::string getICFGKind(const int kind);
};

} // End namespace SVF

#endif
