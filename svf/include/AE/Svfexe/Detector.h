#include <SVFIR/SVFIR.h>
#include <AE/Core/AbstractState.h>
#include "Util/SVFBugReport.h"

namespace SVF
{
class IDetector
{
public:
    virtual ~IDetector() = default;
    virtual void detect(AbstractState& as, const SVFStmt* stmt) = 0;
    virtual void reportBug() = 0;
};

class AEException : public std::exception {
public:
    AEException(const std::string& message)
        : msg_(message) {}

    virtual const char* what() const throw() {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

class BufOverflowDetector : public IDetector
{
public:
    BufOverflowDetector() {
        initExtAPIBufOverflowCheckRules();

    }
    ~BufOverflowDetector() = default;
    void updateGepObjOffsetFromBase(AddressValue gepAddrs,
                                    AddressValue objAddrs,
                                    IntervalValue offset);
    void detect(AbstractState& as, const SVFStmt*);

    void addToGepObjOffsetFromBase(const GepObjVar* obj, const IntervalValue& offset) {
        gepObjOffsetFromBase[obj] = offset;
    }

    bool hasGepObjOffsetFromBase(const GepObjVar* obj) const {
        return gepObjOffsetFromBase.find(obj) != gepObjOffsetFromBase.end();
    }

    IntervalValue getGepObjOffsetFromBase(const GepObjVar* obj) const {
        if (hasGepObjOffsetFromBase(obj))
            return gepObjOffsetFromBase.at(obj);
        else
            assert(false && "GepObjVar not found in gepObjOffsetFromBase");
    }

    IntervalValue getAccessOffset(AbstractState& as, NodeID objId, const GepStmt* gep);

    void addBugToReporter(const AEException& e, const ICFGNode* node) {
        const SVFInstruction* inst = nullptr;

        // Determine the instruction associated with the ICFG node
        if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node)) {
            inst = call->getCallSite(); // If the node is a call node, get the call site instruction
        }
        else {
            inst = node->getSVFStmts().back()->getInst(); // Otherwise, get the last instruction of the node's
                                                          // statements
        }

        GenericBug::EventStack eventStack;
        SVFBugEvent sourceInstEvent(SVFBugEvent::EventType::SourceInst, inst);
        eventStack.push_back(sourceInstEvent); // Add the source instruction event to the event stack

        if (eventStack.size() == 0) {
            return; // If the event stack is empty, return early
        }

        std::string loc = eventStack.back().getEventLoc(); // Get the location of the last event in the stack

        // Check if the bug at this location has already been reported
        if (_bugLoc.find(loc) != _bugLoc.end()) {
            return; // If the bug location is already reported, return early
        }
        else {
            _bugLoc.insert(loc); // Otherwise, mark this location as reported
        }

        // Add the bug to the recorder with details from the event stack
        _recoder.addAbsExecBug(GenericBug::FULLBUFOVERFLOW, eventStack, 0, 0, 0, 0);
        _nodeToBugInfo[node] = e.what(); // Record the exception information for the node
    }

    void reportBug() {
        if (_nodeToBugInfo.size() > 0) {
            std::cerr << "######################Buffer Overflow (" + std::to_string(_nodeToBugInfo.size())
                             + " found)######################\n";
            std::cerr << "---------------------------------------------\n";
            for (auto& it : _nodeToBugInfo) {
                std::cerr << it.second << "\n---------------------------------------------\n";
            }
        }
    }

    void initExtAPIBufOverflowCheckRules();

    void handleExtAPI(AbstractState& as, const CallICFGNode *call);


private:
    Map<const GepObjVar*, IntervalValue> gepObjOffsetFromBase;
    Map<std::string, std::vector<std::pair<u32_t, u32_t>>> _extAPIBufOverflowCheckRules;

    Set<std::string> _bugLoc;
    SVFBugReport _recoder;
    Map<const ICFGNode*, std::string> _nodeToBugInfo;
};
}