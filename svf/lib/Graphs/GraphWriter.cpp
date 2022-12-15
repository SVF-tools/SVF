//===- GraphWriter.cpp - Implements GraphWriter support routines ----------===//
//
// From the LLVM Project with some modifications, under the Apache License v2.0
// with LLVM Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include "Graphs/GraphWriter.h"

#include <string>

std::string SVF::DOT::EscapeStr(const std::string &Label)
{
    std::string Str(Label);
    for (unsigned i = 0; i != Str.length(); ++i)
        switch (Str[i])
        {
        case '\n':
            Str.insert(Str.begin()+i, '\\');  // Escape character...
            ++i;
            Str[i] = 'n';
            break;
        case '\t':
            Str.insert(Str.begin()+i, ' ');  // Convert to two spaces
            ++i;
            Str[i] = ' ';
            break;
        case '\\':
            if (i+1 != Str.length())
                switch (Str[i+1])
                {
                case 'l':
                    continue; // don't disturb \l
                case '|':
                case '{':
                case '}':
                    Str.erase(Str.begin()+i);
                    continue;
                default:
                    break;
                }
        case '{':
        case '}':
        case '<':
        case '>':
        case '|':
        case '"':
            Str.insert(Str.begin()+i, '\\');  // Escape character...
            ++i;  // don't infinite loop
            break;
        }
    return Str;
}
