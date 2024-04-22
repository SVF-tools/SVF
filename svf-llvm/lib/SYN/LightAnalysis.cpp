//
// Created by LiShangyu on 2024/3/5.
//

#include "SYN/LightAnalysis.h"

using namespace SVF;

LightAnalysis::LightAnalysis(const std::string& _srcPath)
{
    srcPath = _srcPath;
}

LightAnalysis::~LightAnalysis() {}

void LightAnalysis::runOnSrc()
{
    //     test: srcPath temporarily represents the whole file path.
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, srcPath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None);
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(cursor, &cursorVisitor, nullptr);
}
struct VisitorData
{
    int order_number;
    unsigned int target_line;
    std::string functionName;
    std::vector<std::string> parameters;
};

void LightAnalysis::findNodeOnTree(unsigned int target_line, int order_number,
                                   const std::string& functionName,
                                   const std::vector<std::string>& parameters)
{
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, srcPath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None);
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    if (order_number == 0)
    {
        VisitorData data{order_number, target_line, functionName, parameters};
        clang_visitChildren(cursor, &astVisitor, &data);
    }
    else if (order_number == 1)
    {
        VisitorData data{order_number, target_line, functionName};
        clang_visitChildren(cursor, &astVisitor, &data);
    }
}

enum CXChildVisitResult LightAnalysis::astVisitor(CXCursor curCursor,
                                                  CXCursor parent,
                                                  CXClientData client_data)
{
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned int line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column, nullptr);
    VisitorData* data = static_cast<VisitorData*>(client_data);
    unsigned int target_line = data->target_line;
    if (line == target_line)
    {
        int order_number = data->order_number;
        switch (order_number)
        {
        case 0: {

            if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
                CXCursor_CallExpr)
            {
                std::string functionName = data->functionName;
                std::cout << "Function name: " << functionName << "\n";
                std::vector<std::string> parameters = data->parameters;
                std::cout << "Parameters: ";
                for (auto& parameter : parameters)
                {
                    std::cout << parameter << " ";
                }
                CXString cursor_name = clang_getCursorSpelling(curCursor);
                std::string current_function_name =
                    clang_getCString(cursor_name);
                if (current_function_name == functionName)
                {
                    std::cout << "Function name matches with the target "
                                 "function name.\n";
                    // 找到函数返回值赋值的变量，判断这个变量是不是第一次定义，找到这个
                    // call site 所处的 scope 信息（大括号）
                    CXString var_name = clang_getCursorSpelling(parent);

                    if (clang_getCursorKind(parent) == CXCursor_VarDecl)
                    {
                        std::cout << "Variable " << clang_getCString(var_name)
                                  << " is defined here.\n";
                    }
                    auto temp_parent = parent;
                    while (static_cast<CXCursorKind>(clang_getCursorKind(
                               temp_parent)) != CXCursor_CompoundStmt &&
                           static_cast<CXCursorKind>(clang_getCursorKind(
                               temp_parent)) != CXCursor_FunctionDecl)
                    {
                        temp_parent = clang_getCursorSemanticParent(temp_parent);
                    }
                  

                    CXSourceRange scope_range = clang_getCursorExtent(temp_parent);
                    CXSourceLocation startLoc =
                        clang_getRangeStart(scope_range);
                    CXSourceLocation endLoc = clang_getRangeEnd(scope_range);
                    unsigned startLine, startColumn, endLine, endColumn;
                    clang_getSpellingLocation(startLoc, NULL, &startLine,
                                              &startColumn, NULL);
                    clang_getSpellingLocation(endLoc, NULL, &endLine,
                                              &endColumn, NULL);
                    std::cout << "The scope starts from line " << startLine
                              << ", column " << startColumn << "\n";
                    std::cout << "The scope ends at line " << endLine
                              << ", column " << endColumn << "\n";
                }
                clang_disposeString(cursor_name);
            }
            break;
        }
        case 1: {
            std::string operation = data->functionName;
            if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
                CXCursor_BinaryOperator)
            {
                CXSourceRange range = clang_getCursorExtent(curCursor);
                CXToken* tokens = 0;
                unsigned int numTokens = 0;
                CXTranslationUnit TU =
                    clang_Cursor_getTranslationUnit(curCursor);
                clang_tokenize(TU, range, &tokens, &numTokens);
                int flag = 0;
                if (numTokens > 1)
                {
                    CXString op_name = clang_getTokenSpelling(TU, tokens[1]);

                    const char* op_string = clang_getCString(op_name);
                    if (strcmp(op_string, "<") == 0 && operation == "slt")
                    {
                        std::cout << "find <" << std::endl;
                        flag = 1;
                    }
                    if (strcmp(op_string, "<=") == 0 && operation == "sle")
                    {
                        std::cout << "find <=" << std::endl;
                        flag = 1;
                    }
                    if (strcmp(op_string, ">") == 0 && operation == "sgt")
                    {
                        std::cout << "find >" << std::endl;
                        flag = 1;
                    }
                    if (strcmp(op_string, ">=") == 0 && operation == "sge")
                    {
                        std::cout << "find >=" << std::endl;
                        flag = 1;
                    }
                    if (flag == 1 &&
                        static_cast<CXCursorKind>(
                            clang_getCursorKind(parent)) == CXCursor_IfStmt)
                    {
                        // 找到这个 condition 所在的 scope 信息，以及它所
                        // dominate 的 scope 信息（对于 if，可以进一步找 else 的
                        // scope 信息）

                        CXSourceRange range = clang_getCursorExtent(parent);
                        CXSourceLocation startLoc = clang_getRangeStart(range);
                        CXSourceLocation endLoc = clang_getRangeEnd(range);
                        unsigned startLine, startColumn, endLine, endColumn;
                        clang_getSpellingLocation(startLoc, NULL, &startLine,
                                                  &startColumn, NULL);
                        clang_getSpellingLocation(endLoc, NULL, &endLine,
                                                  &endColumn, NULL);
                        std::cout << "The if scope starts from line "
                                  << startLine << ", column " << startColumn
                                  << "\n";
                        std::cout << "The if scope ends at line " << endLine
                                  << ", column " << endColumn << "\n";
                    }
                    else if (flag == 1 && static_cast<CXCursorKind>(
                                              clang_getCursorKind(parent)) ==
                                              CXCursor_WhileStmt)
                    {
                        // 找到这个 condition 所在的 scope 信息，以及它所
                        // dominate 的 scope 信息
                        CXSourceRange range = clang_getCursorExtent(parent);
                        CXSourceLocation startLoc = clang_getRangeStart(range);
                        CXSourceLocation endLoc = clang_getRangeEnd(range);
                        unsigned startLine, startColumn, endLine, endColumn;
                        clang_getSpellingLocation(startLoc, NULL, &startLine,
                                                  &startColumn, NULL);
                        clang_getSpellingLocation(endLoc, NULL, &endLine,
                                                  &endColumn, NULL);
                        std::cout << "The while scope starts from line "
                                  << startLine << ", column " << startColumn
                                  << "\n";
                        std::cout << "The while scope ends at line " << endLine
                                  << ", column " << endColumn << "\n";
                    }
                    clang_disposeString(op_name);
                }
                clang_disposeTokens(TU, tokens, numTokens);
            }
            break;
        }
        default: {
            break;
        }
        }
    }
    return CXChildVisit_Recurse;
}

enum CXChildVisitResult LightAnalysis::cursorVisitor(CXCursor curCursor,
                                                     CXCursor parent,
                                                     CXClientData client_data)
{

    CXString current_display_name = clang_getCursorDisplayName(curCursor);
    // Allocate a CXString representing the name of the current cursor

    auto str = clang_getCString(current_display_name);
    std::cout << "Visiting element " << str << "\n";
    // Print the char* value of current_display_name
    // 获取源代码中的位置
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned int line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column,
                              nullptr); // 将位置转换为行号、列号和文件名

    // 打印行号、列号和元素名称
    std::cout << "Visiting element " << str << " at line " << line
              << ", column " << column << "\n";

    clang_disposeString(current_display_name);
    // Since clang_getCursorDisplayName allocates a new CXString, it must be
    // freed. This applies to all functions returning a CXString

    return CXChildVisit_Recurse;
}