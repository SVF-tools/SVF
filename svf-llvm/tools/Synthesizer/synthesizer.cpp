//
// Created by LiShangyu on 2024/3/1.
//

#include "AbstractExecution/SVFIR2ItvExeState.h"
#include "Graphs/SVFG.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SYN/LightAnalysis.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "Util/Z3Expr.h"
#include <string>

using namespace llvm;
using namespace SVF;

static Option<std::string> SOURCEPATH("srcpath",
                                      "Path for source code to transform", "");
//
// static Option<std::string> IRPATH(
//    "irpath",
//    "Path for ir file",
//    ""
//);

static Option<std::string> NEWSPECPATH("newspec",
                                       "Path for new specification file", "");

void traverseOnSVFStmt(const ICFGNode* node)
{
    SVFIR2ItvExeState* svfir2ExeState = new SVFIR2ItvExeState(SVFIR::getPAG());
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        std::string stmtstring = stmt->getValue()->toString();
        if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            svfir2ExeState->translateAddr(addr);
        }
        else if (const BinaryOPStmt* binary =
                     SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
        {
            svfir2ExeState->translateBinary(binary);
        }
        else if (const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
        {
            svfir2ExeState->translateCmp(cmp);
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            svfir2ExeState->translateLoad(load);
        }
        else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            std ::string storestring = store->getValue()->toString();
            svfir2ExeState->translateStore(store);
        }
        else if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
        {
            std::string copystring = copy->getValue()->toString();
            svfir2ExeState->translateCopy(copy);
        }
        else if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            std ::string gepstring = gep->getValue()->toString();
            if (gep->isConstantOffset())
            {
                gep->accumulateConstantByteOffset();
                gep->accumulateConstantOffset();
            }
            svfir2ExeState->translateGep(gep);
        }
        else if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        {
            std ::string selectstring = select->getValue()->toString();
            svfir2ExeState->translateSelect(select);
        }
        else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
        {
            std::string phistring = phi->getValue()->toString();
            svfir2ExeState->translatePhi(phi);
        }
        else if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            // To handle Call Edge
            std::string callstring = callPE->getValue()->toString();
            std::string callinfo = callPE->getValue()->getSourceLoc();
            CallICFGNode* callNode =
                const_cast<CallICFGNode*>(callPE->getCallSite());
              callinfo =  callNode->toString();
            svfir2ExeState->translateCall(callPE);
        }
        else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            std::string retstring = retPE->getValue()->toString();
            svfir2ExeState->translateRet(retPE);
        }
        //   else
        //  assert(false && "implement this part");
    }
}

int main(int argc, char** argv)
{

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
        argc, argv, "Tool to transform your code automatically",
        "[options] <input-bitcode...>");

    // unused.
    std::string irPath = "";
    std::string srcPath = "";
    std::string specPath = "";

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    if (SOURCEPATH().empty())
    {
        std::cerr << "You should specify the path of source code!" << std::endl;
        std::exit(1);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVF::SVFIRBuilder builder(svfModule);
    auto pag = builder.build();
    assert(pag && "pag cannot be nullptr!");

    auto str = SOURCEPATH();
    auto lightAnalysis = new LightAnalysis(str);

    lightAnalysis->runOnSrc();
    ICFG* icfg = pag->getICFG();

    for (const auto& it : *icfg)
    {
        const ICFGNode* node = it.second;
        traverseOnSVFStmt(node);
    }

    return 0;
}
