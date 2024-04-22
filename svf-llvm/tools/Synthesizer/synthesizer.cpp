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
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace llvm;
using namespace SVF;

static Option<std::string> SOURCEPATH("srcpath",
                                      "Path for source code to transform", "");

static Option<std::string> NEWSPECPATH("newspec",
                                       "Path for new specification file", "");
#define call_order 0
#define branch_order 1
void traverseOnSVFStmt(const ICFGNode* node)
{
    auto str = SOURCEPATH();
    auto lightAnalysis = new LightAnalysis(str);
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
        else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            std::string retstring = retPE->getValue()->toString();
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
            std::string::size_type pos = m.find("\"ln\":");
            unsigned int num =
                std::stoi(m.substr(pos + 5, m.find(",") - pos - 5));
       //     printf("num = %d\n", num);
            std::regex re("@(\\w+)\\((.+)\\)");
            std::smatch match;
            std::string functionName;
            std::vector<std::string> parameters;
            if (std::regex_search(callstring, match, re) && match.size() > 2)
            {
                functionName = match.str(1);
                std::string parametersStr = match.str(2);
                std::stringstream ss(parametersStr);
                std::string parameter;
                while (std::getline(ss, parameter, ','))
                {
                    parameter.erase(0, parameter.find_first_not_of(' '));
                    parameter.erase(parameter.find_last_not_of(' ') + 1);
                    parameters.push_back(parameter);
                }
            }
            lightAnalysis->findNodeOnTree(num, call_order, functionName,
                                          parameters);
        }

        else if (const BranchStmt* branch = SVFUtil::dyn_cast<BranchStmt>(stmt))
        {
            std::string brstring = branch->getValue()->toString();
            //"   br i1 %5, label %6, label %7 "
            SVFVar* branchVar = const_cast<SVFVar*>(branch->getBranchInst());

            SVFValue* branchValue =
                const_cast<SVFValue*>(branchVar->getValue());
            std::string location = branchValue->getSourceLoc();
         //   std::cout << location << std::endl;
            std::string::size_type pos = location.find("\"ln\":");
            unsigned int num = std::stoi(
                location.substr(pos + 5, location.find(",") - pos - 5));
       //     printf("num = %d\n", num);
            SVFVar* conditionVar = const_cast<SVFVar*>(branch->getCondition());
            std::string conditionstring = conditionVar->getValue()->toString();
      //      std::cout << conditionstring << std::endl;
            //   %5 = icmp slt i32 %4, 0, !dbg !17 { "ln": 7, "cl": 11, "fl":
            //   "test1.c" }
            std::size_t icmpPos = conditionstring.find("icmp");
            if (icmpPos != std::string::npos)
            {
                std::size_t spacePos = conditionstring.find(" ", icmpPos);
                std::size_t nextSpacePos =
                    conditionstring.find(" ", spacePos + 1);
                std::string operation = conditionstring.substr(
                    spacePos + 1, nextSpacePos - spacePos - 1);
         //       std::cout << operation << std::endl;
                // 创建空的vector
                std::vector<std::string> parameters = {};
                lightAnalysis->findNodeOnTree(num, branch_order, operation,
                                              parameters);
            }
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
  //  auto lightAnalysis = new LightAnalysis(str);

  //  lightAnalysis->runOnSrc();
    ICFG* icfg = pag->getICFG();

    for (const auto& it : *icfg)
    {
        const ICFGNode* node = it.second;
        traverseOnSVFStmt(node);
    }

    return 0;
}
