/*  [ExtAPI.cpp] The actual database of external functions
 *  v. 005, 2008-08-08
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
*/

#include "Util/ExtAPI.h"
#include <stdio.h>
#include <fstream>

#include <llvm/Support/CommandLine.h> // for tool output file

using namespace std;
using namespace llvm;

static cl::opt<string> ReadApiSum("read-apisum",  cl::init(""),
                                 cl::desc("Read API summaries (names and side effects) from a file"));

const std::string ExtAPI::DB_NAME("externalAPIDB.txt");
const char *ExtAPI::SVFHOME = "SVFHOME";

ExtAPI* ExtAPI::extAPI = NULL;

struct ei_pair {
    const char *n;
    ExtAPI::extf_t t;
};

llvm::StringMap<ExtAPI::extf_t> stringToExtAPI;

static void fillExtAPIStringMap() {
    stringToExtAPI["EFT_NOOP"]             = ExtAPI::EFT_NOOP;
    stringToExtAPI["EFT_ALLOC"]            = ExtAPI::EFT_ALLOC;
    stringToExtAPI["EFT_REALLOC"]          = ExtAPI::EFT_REALLOC;
    stringToExtAPI["EFT_FREE"]             = ExtAPI::EFT_FREE;
    stringToExtAPI["EFT_NOSTRUCT_ALLOC"]   = ExtAPI::EFT_NOSTRUCT_ALLOC;
    stringToExtAPI["EFT_STAT"]             = ExtAPI::EFT_STAT;
    stringToExtAPI["EFT_STAT2"]            = ExtAPI::EFT_STAT2;
    stringToExtAPI["EFT_L_A0"]             = ExtAPI::EFT_L_A0;
    stringToExtAPI["EFT_L_A1"]             = ExtAPI::EFT_L_A1;
    stringToExtAPI["EFT_L_A2"]             = ExtAPI::EFT_L_A2;
    stringToExtAPI["EFT_L_A8"]             = ExtAPI::EFT_L_A8;
    stringToExtAPI["EFT_L_A0__A0R_A1R"]    = ExtAPI::EFT_L_A0__A0R_A1R;
    stringToExtAPI["EFT_A1R_A0R"]          = ExtAPI::EFT_A1R_A0R;
    stringToExtAPI["EFT_A3R_A1R_NS"]       = ExtAPI::EFT_A3R_A1R_NS;
    stringToExtAPI["EFT_A1R_A0"]           = ExtAPI::EFT_A1R_A0;
    stringToExtAPI["EFT_A2R_A1"]           = ExtAPI::EFT_A2R_A1;
    stringToExtAPI["EFT_A4R_A1"]           = ExtAPI::EFT_A4R_A1;
    stringToExtAPI["EFT_L_A0__A2R_A0"]     = ExtAPI::EFT_L_A0__A2R_A0;
    stringToExtAPI["EFT_A0R_NEW"]          = ExtAPI::EFT_A0R_NEW;
    stringToExtAPI["EFT_A1R_NEW"]          = ExtAPI::EFT_A1R_NEW;
    stringToExtAPI["EFT_A2R_NEW"]          = ExtAPI::EFT_A2R_NEW;
    stringToExtAPI["EFT_A4R_NEW"]          = ExtAPI::EFT_A4R_NEW;
    stringToExtAPI["EFT_A11R_NEW"]         = ExtAPI::EFT_A11R_NEW;
    stringToExtAPI["EFT_STD_RB_TREE_INSERT_AND_REBALANCE"]
        = ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE;
    stringToExtAPI["EFT_STD_RB_TREE_INCREMENT"]
        = ExtAPI::EFT_STD_RB_TREE_INCREMENT;
    stringToExtAPI["EFT_STD_LIST_HOOK"]    = ExtAPI::EFT_STD_LIST_HOOK;
    stringToExtAPI["CPP_EFT_A0R_A1"]       = ExtAPI::CPP_EFT_A0R_A1;
    stringToExtAPI["CPP_EFT_A0R_A1R"]      = ExtAPI::CPP_EFT_A0R_A1R;
    stringToExtAPI["CPP_EFT_A1R"]          = ExtAPI::CPP_EFT_A1R;
    stringToExtAPI["EFT_CXA_BEGIN_CATCH"]  = ExtAPI::EFT_CXA_BEGIN_CATCH;
    stringToExtAPI["CPP_EFT_DYNAMIC_CAST"] = ExtAPI::CPP_EFT_DYNAMIC_CAST;
    stringToExtAPI["EFT_OTHER"]            = ExtAPI::EFT_OTHER;
}


void ExtAPI::init() {
    fillExtAPIStringMap();

    const char *svfhome = std::getenv(SVFHOME);

    if (ReadApiSum.empty() && svfhome != NULL) {
        ReadApiSum = std::string(svfhome) + "/" + DB_NAME;
    }

    ifstream db(ReadApiSum);
    if (!db.is_open()) {
        if (svfhome == NULL && ReadApiSum.empty()) {
            cout << "Neither $" << SVFHOME
                 << " nor option -read-apisum is defined.\n";
        } else {
            cout << "cannot read file: '" << ReadApiSum << "'\n";
        }

        assert(!"Could not open external API database.");

        return;
    }

    set<extf_t> t_seen;
    extf_t prev_t= EFT_NOOP;
    t_seen.insert(EFT_NOOP);

    string name;
    string typeString;

    while (db >> name) {
        // Is it a comment?
        if (name[0] == '/') {
            if (name.length() > 1 && name[1] == '/') {
                db.ignore(numeric_limits<std::streamsize>::max(), '\n');
                continue;
            } else {
                assert(!"came across single '/'");
            }
        }

        // It's some data.
        db >> typeString;
        llvm::StringMap<ExtAPI::extf_t>::const_iterator it
            = stringToExtAPI.find(typeString);
        if (it == stringToExtAPI.end()) {
            cerr << typeString << "\n";
            assert(!"^ unknown type");
        }

        ExtAPI::extf_t type = it->second;

        if (type != prev_t) {
            // This will detect if an entry is moved to another block
            // but did not have its type changed.
            if (t_seen.count(type)) {
                cerr << name << "\n";
                assert(!"names not grouped by type");
            }

            t_seen.insert(type);
            prev_t = type;
        }

        if (info.count(name)) {
            cerr << name << "\n";
            assert(!"duplicate name");
        }

        info[name] = type;
    }

    db.close();
}

