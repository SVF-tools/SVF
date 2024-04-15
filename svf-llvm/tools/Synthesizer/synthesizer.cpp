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
        if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            // To handle Call Edge
            std::string callstring = callPE->getValue()->toString();
            // std::string callinfo = callPE->getValue()->getSourceLoc();
            CallICFGNode* callNode =
                const_cast<CallICFGNode*>(callPE->getCallSite());
            std::string callinfo = callNode->toString();
            SVFInstruction* cs =
                const_cast<SVFInstruction*> (callNode->getCallSite());
            std::string m = cs->getSourceLoc();
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
