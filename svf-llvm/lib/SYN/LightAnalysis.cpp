//
// Created by LiShangyu on 2024/3/5.
//

#include "SYN/LightAnalysis.h"

using namespace SVF;

LightAnalysis::LightAnalysis(const std::string& _srcPath) {
    srcPath = _srcPath;
}

LightAnalysis::~LightAnalysis() {}

void LightAnalysis::runOnSrc() {

//     test: srcPath temporarily represents the whole file path.

    CXIndex index = clang_createIndex(0, 0);

    CXTranslationUnit unit = clang_parseTranslationUnit(index, srcPath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None);

    assert(unit && "unit cannot be nullptr!");

    CXCursor cursor = clang_getTranslationUnitCursor(unit);

    clang_visitChildren(cursor, &cursorVisitor, nullptr);

}

enum CXChildVisitResult LightAnalysis::cursorVisitor(CXCursor curCursor,
                                                   CXCursor parent,
                                                   CXClientData client_data) {


        CXString current_display_name = clang_getCursorDisplayName(curCursor);
        //Allocate a CXString representing the name of the current cursor

        auto str = clang_getCString(current_display_name);
        std::cout << "Visiting element " << str << "\n";
        //Print the char* value of current_display_name

        clang_disposeString(current_display_name);
        //Since clang_getCursorDisplayName allocates a new CXString, it must be freed. This applies
        //to all functions returning a CXString

        return CXChildVisit_Recurse;

}
