//===- Options.cpp -- Command line options ------------------------//

#include <llvm/Support/CommandLine.h>

#include "Util/Options.h"

namespace SVF
{
    const Options *Options::get(void)
    {
        if (options == nullptr)
        {
            options = new Options();
        }

        return options;
    }

    Options::Options(void) { }

};  // namespace SVF.
