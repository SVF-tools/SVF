#include <AE/Svfexe/Detector.h>
using namespace SVF;
void BufOverflowDetector::detect(AbstractState& as, const SVFStmt* stmt) {
    if (!SVFUtil::isa<CallICFGNode>(stmt->getICFGNode())) {
        if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt)) {
            SVFIR* svfir = PAG::getPAG();
            NodeID lhs = gep->getLHSVarID();
            NodeID rhs = gep->getRHSVarID();
            updateGepObjOffsetFromBase(as[lhs].getAddrs(), as[rhs].getAddrs(), as.getByteOffset(gep));

            IntervalValue baseObjSize = IntervalValue::bottom();
            AddressValue objAddrs = as[gep->getRHSVarID()].getAddrs();
            for (const auto& addr : objAddrs) {
                NodeID objId = AbstractState::getInternalID(addr);
                u32_t size = svfir->getBaseObj(objId)->getByteSizeOfObj();
                IntervalValue accessOffset = getAccessOffset(as, objId, gep);
                if (accessOffset.ub().getIntNumeral() >= size) {
                    // Create an exception with the node's string representation
                    AEException bug(stmt->toString());
                    // Add the bug to the reporter using the helper
                    addBugToReporter(bug, stmt->getICFGNode());
                }
            }
        }
    }
    else
    {
        // callnode, check if it is external api call
        const CallICFGNode* callNode = SVFUtil::cast<CallICFGNode>(stmt->getICFGNode());
        const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
        if(SVFUtil::isExtCall(callfun)) {

        }
    }
}

void BufOverflowDetector::initExtAPIBufOverflowCheckRules()
{
    //void llvm_memcpy_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i64"] = {{0, 2}, {1,2}};
    //void llvm_memcpy_p0_p0_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0_p0_i64"] = {{0, 2}, {1,2}};
    //void llvm_memcpy_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i32"] = {{0, 2}, {1,2}};
    //void llvm_memcpy(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy"] = {{0, 2}, {1,2}};
    //void llvm_memmove(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i64"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0_p0_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0_p0_i64"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i32"] = {{0, 2}, {1,2}};
    //void __memcpy_chk(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["__memcpy_chk"] = {{0, 2}, {1,2}};
    //void *memmove(void *str1, const void *str2, unsigned long n)
    _extAPIBufOverflowCheckRules["memmove"] = {{0, 2}, {1,2}};
    //void bcopy(const void *s1, void *s2, unsigned long n){}
    _extAPIBufOverflowCheckRules["bcopy"] = {{0, 2}, {1,2}};
    //void *memccpy( void * restrict dest, const void * restrict src, int c, unsigned long count)
    _extAPIBufOverflowCheckRules["memccpy"] = {{0, 3}, {1,3}};
    //void __memmove_chk(char* dst, char* src, int sz){}
    _extAPIBufOverflowCheckRules["__memmove_chk"] = {{0, 2}, {1,2}};
    //void llvm_memset(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset"] = {{0, 2}};
    //void llvm_memset_p0i8_i32(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0i8_i32"] = {{0, 2}};
    //void llvm_memset_p0i8_i64(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0i8_i64"] = {{0, 2}};
    //void llvm_memset_p0_i64(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0_i64"] = {{0, 2}};
    //char *__memset_chk(char * dest, int c, unsigned long destlen, int flag)
    _extAPIBufOverflowCheckRules["__memset_chk"] = {{0, 2}};
    //char *wmemset(wchar_t * dst, wchar_t elem, int sz, int flag) {
    _extAPIBufOverflowCheckRules["wmemset"] = {{0, 2}};
    //char *strncpy(char *dest, const char *src, unsigned long n)
    _extAPIBufOverflowCheckRules["strncpy"] = {{0, 2}, {1,2}};
    //unsigned long iconv(void* cd, char **restrict inbuf, unsigned long *restrict inbytesleft, char **restrict outbuf, unsigned long *restrict outbytesleft)
    _extAPIBufOverflowCheckRules["iconv"] = {{1, 2}, {3, 4}};
}

void BufOverflowDetector::handleExtAPI(AbstractState& as, const SVF::CallICFGNode* call)
{
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    assert(fun && "SVFFunction* is nullptr");
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    // check the type of mem api,
    // MEMCPY: like memcpy, memcpy_chk, llvm.memcpy etc.
    // MEMSET: like memset, memset_chk, llvm.memset etc.
    // STRCPY: like strcpy, strcpy_chk, wcscpy etc.
    // STRCAT: like strcat, strcat_chk, wcscat etc.
    // for other ext api like printf, scanf, etc., they have their own handlers

    ExtAPIType extType = UNCLASSIFIED;
    // get type of mem api
    for (const std::string &annotation: fun->getAnnotations())
    {
        if (annotation.find("MEMCPY") != std::string::npos)
            extType =  MEMCPY;
        if (annotation.find("MEMSET") != std::string::npos)
            extType =  MEMSET;
        if (annotation.find("STRCPY") != std::string::npos)
            extType = STRCPY;
        if (annotation.find("STRCAT") != std::string::npos)
            extType =  STRCAT;
    }
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    if (extType == MEMCPY)
    {
        if (_extAPIBufOverflowCheckRules.count(fun->getName()) == 0)
        {
            // if it is not in the rules, we do not check it
            SVFUtil::errs() << "Warning: " << fun->getName() << " is not in the rules, please implement it\n";
            return;
        }
        // call parseMemcpyBufferCheckArgs to parse the BUF_CHECK annotation
        std::vector<std::pair<u32_t, u32_t>> args = _extAPIBufOverflowCheckRules.at(fun->getName());
        // loop the args and check the offset
        for (auto arg: args)
        {
            IntervalValue offset = as[_svfir->getValueNode(cs.getArgument(arg.second))].getInterval() - IntervalValue(1);
            canSafelyAccessMemory(cs.getArgument(arg.first), offset, call);
        }
    }
    // 2. memset functions like memset, memset_chk, annotate("MEMSET"), annotate("BUF_CHECK:Arg0, Arg2")
    else if (extType == MEMSET)
    {
        if (_extAPIBufOverflowCheckRules.count(fun->getName()) == 0)
        {
            // if it is not in the rules, we do not check it
            SVFUtil::errs() << "Warning: " << fun->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args = _extAPIBufOverflowCheckRules.at(fun->getName());
        // loop the args and check the offset
        for (auto arg: args)
        {
            IntervalValue offset = as[_svfir->getValueNode(cs.getArgument(arg.second))].getInterval() - IntervalValue(1);
            canSafelyAccessMemory(cs.getArgument(arg.first), offset, call);
        }
    }
    else if (extType == STRCPY)
    {
        detectStrcpy(call);
    }
    else if (extType == STRCAT)
    {
        detectStrcat(call);
    }
    else
    {

    }
    return;
}

IntervalValue BufOverflowDetector::getAccessOffset(SVF::AbstractState& as, SVF::NodeID objId, const SVF::GepStmt* gep)
{
    SVFIR* svfir = PAG::getPAG();
    auto obj = svfir->getGNode(objId);
    // Field-insensitive base object
    if (SVFUtil::isa<FIObjVar>(obj)) {
        // get base size
        IntervalValue accessOffset = as.getByteOffset(gep);
        return accessOffset;
    }
    // A sub object of an aggregate object
    else if (SVFUtil::isa<GepObjVar>(obj)) {
        IntervalValue accessOffset =
            getGepObjOffsetFromBase(SVFUtil::cast<GepObjVar>(obj)) + as.getByteOffset(gep);
        return accessOffset;
    }
    else{
        assert(SVFUtil::isa<DummyObjVar>(obj) && "What other types of object?");
        return IntervalValue::top();
    }
}

void BufOverflowDetector::updateGepObjOffsetFromBase(SVF::AddressValue gepAddrs, SVF::AddressValue objAddrs, SVF::IntervalValue offset)
{
    SVFIR* svfir = PAG::getPAG();
    for (const auto& objAddr : objAddrs) {
        NodeID objId = AbstractState::getInternalID(objAddr);
        auto obj = svfir->getGNode(objId);
        if (SVFUtil::isa<FIObjVar>(obj)) {
            for (const auto& gepAddr : gepAddrs) {
                NodeID gepObj = AbstractState::getInternalID(gepAddr);
                const GepObjVar* gepObjVar = SVFUtil::cast<GepObjVar>(svfir->getGNode(gepObj));
                addToGepObjOffsetFromBase(gepObjVar, offset);
            }
        }
        else if (SVFUtil::isa<GepObjVar>(obj)) {
            const GepObjVar* objVar = SVFUtil::cast<GepObjVar>(obj);
            for (const auto& gepAddr : gepAddrs) {
                NodeID gepObj = AbstractState::getInternalID(gepAddr);
                const GepObjVar* gepObjVar = SVFUtil::cast<GepObjVar>(svfir->getGNode(gepObj));
                if (hasGepObjOffsetFromBase(objVar)) {
                    IntervalValue objOffsetFromBase = getGepObjOffsetFromBase(objVar);
                    /// make sure gepObjVar has not been written before
                    if (!hasGepObjOffsetFromBase(gepObjVar))
                        addToGepObjOffsetFromBase(gepObjVar, objOffsetFromBase + offset);
                }
                else {
                    assert(false && "gepRhsObjVar has no gepObjOffsetFromBase");
                }
            }
        }
    }
}