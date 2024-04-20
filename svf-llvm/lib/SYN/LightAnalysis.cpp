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
    unsigned int target_line;
    std::string functionName;
    std::vector<std::string> parameters;
};

void LightAnalysis::findNodeOnTree(unsigned int target_line,
                                   const std::string& functionName,
                                   const std::vector<std::string>& parameters)
{

    CXIndex index = clang_createIndex(0, 0);

    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, srcPath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None);

    assert(unit && "unit cannot be nullptr!");

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    VisitorData data{target_line, functionName, parameters};
    clang_visitChildren(cursor, &astVisitor, &data);
}

enum CXChildVisitResult LightAnalysis::astVisitor(CXCursor curCursor,
                                                  CXCursor parent,
                                                  CXClientData client_data)
{

    CXString current_display_name = clang_getCursorDisplayName(curCursor);
    auto str = clang_getCString(current_display_name);
    std::cout << "Visiting element " << str << "\n";
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned int line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column,
                              nullptr); // 将位置转换为行号、列号和文件名
    std::cout << "Visiting element " << str << " at line " << line
              << ", column " << column << "\n";
    VisitorData* data = static_cast<VisitorData*>(client_data);
    unsigned int target_line = data->target_line;
    std::string functionName = data->functionName;
    std::cout << "Function name: " << functionName << "\n";
    std::vector<std::string> parameters = data->parameters;
    std::cout << "Parameters: ";
    for (auto& parameter : parameters)
    {
        std::cout << parameter << " ";
    }
    if (line == target_line)
    {
        if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
            CXCursor_CallExpr)
        {
            CXString cursor_name = clang_getCursorSpelling(curCursor);
            std::string current_function_name = clang_getCString(cursor_name);
            if (current_function_name == functionName)
            {
                std::cout
                    << "Function name matches with the target function name.\n";

                // 找到函数返回值赋值的变量，判断这个变量是不是第一次定义，找到这个
                // call site 所处的 scope 信息（大括号）
                CXString var_name = clang_getCursorSpelling(parent);

                if (clang_getCursorKind(parent) == CXCursor_VarDecl)
                {
                    std::cout << "Variable " << clang_getCString(var_name)
                              << " is defined here.\n";
                }
                // 打印 scope 信息
                CXCursor scope = clang_getCursorSemanticParent(parent);
                CXString scope_name = clang_getCursorSpelling(scope);
                std::cout << "The call site is in the scope of "
                          << clang_getCString(scope_name) << ".\n";
            }

            clang_disposeString(cursor_name);
        }
    }
    clang_disposeString(current_display_name);

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