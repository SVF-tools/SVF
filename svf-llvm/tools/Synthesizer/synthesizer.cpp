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

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

using namespace llvm;
using namespace SVF;

static Option<std::string> SOURCEPATH("srcpath",
                                      "Path for source code to transform", "");

static Option<std::string> NEWSPECPATH("newspec",
                                       "Path for new specification file", "");

void traverseOnSVFStmt(const ICFGNode* node)
{
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        std::string stmtstring = stmt->getValue()->toString();
        if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            std::string addrstring = addr->getValue()->toString();
        }
        else if (const BinaryOPStmt* binary =
                     SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
        {
            std::string bistring = binary->getValue()->toString();
        }
        else if (const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
        {
            std::string cmpstring = cmp->getValue()->toString();
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            std::string loadstring = load->getValue()->toString();
        }
        else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            std ::string storestring = store->getValue()->toString();
        }
        else if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
        {
            std::string copystring = copy->getValue()->toString();
        }
        else if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            std ::string gepstring = gep->getValue()->toString();
        }
        else if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        {
            std ::string selectstring = select->getValue()->toString();
        }
        else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
        {
            std::string phistring = phi->getValue()->toString();
        }
        else if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            // To handle Call Edge
            std::string callstring = callPE->getValue()->toString();
            // std::string callinfo = callPE->getValue()->getSourceLoc();
            CallICFGNode* callNode =
                const_cast<CallICFGNode*>(callPE->getCallSite());

            // 希望知道是不是第一次定义 也就是有没有type
            std::string callinfo = callNode->toString();
            SVFInstruction* cs =
                const_cast<SVFInstruction*>(callNode->getCallSite());
            std::string m = cs->getSourceLoc();
            //"{ \"ln\": 15, \"cl\": 12, \"fl\": \"test1.c\" }"
             //把ln后面的数字提取出来
            
            //提取15
            std::string::size_type pos = m.find("\"ln\":");
            //后面就是15
            int num = std::stoi(m.substr(pos + 5, m.find(",") - pos - 5));
            printf("num = %d\n", num);
            auto str = SOURCEPATH();
            auto lightAnalysis = new LightAnalysis(str);

            lightAnalysis->findNodeOnTree();
        }
        else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            std::string retstring = retPE->getValue()->toString();
        }
        else if (const BranchStmt* branch = SVFUtil::dyn_cast<BranchStmt>(stmt))
        {
            std::string brstring = branch->getValue()->toString();
        }
    }
}
int main(int argc, char** argv)
{

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
        argc, argv, "Tool to transform your code automatically",
        "[options] <input-bitcode...>");

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
