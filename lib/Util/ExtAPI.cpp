/*  [ExtAPI.cpp] The actual database of external functions
 *  v. 005, 2008-08-08
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
*/

#include "Util/ExtAPI.h"
#include <fstream>
//#include <iostream>
#include <stdio.h>
#include <string>

using namespace std;
using namespace SVF;

ExtAPI *ExtAPI::extAPI = nullptr;

namespace
{

    struct ei_pair
    {
        const char *n;
        ExtAPI::extf_t t;
    };

    struct ei_pair_map
    {
        std::string string_ref;
        ExtAPI::extf_t t_ref;
    };

} // End anonymous namespace

static const ei_pair_map ei_pair_maps[] =
    {
        {"ExtAPI::EFT_NOOP", ExtAPI::EFT_NOOP},
        {"ExtAPI::EFT_ALLOC", ExtAPI::EFT_ALLOC},
        {"ExtAPI::EFT_NOSTRUCT_ALLOC", ExtAPI::EFT_NOSTRUCT_ALLOC},
        {"ExtAPI::EFT_STAT2", ExtAPI::EFT_STAT2},
        {"ExtAPI::EFT_STAT", ExtAPI::EFT_STAT},
        {"ExtAPI::EFT_REALLOC", ExtAPI::EFT_REALLOC},
        {"ExtAPI::EFT_FREE", ExtAPI::EFT_FREE},
        {"ExtAPI::EFT_L_A0", ExtAPI::EFT_L_A0},
        {"ExtAPI::EFT_L_A1", ExtAPI::EFT_L_A1},
        {"ExtAPI::EFT_L_A2", ExtAPI::EFT_L_A2},
        {"ExtAPI::EFT_L_A8", ExtAPI::EFT_L_A8},
        {"ExtAPI::EFT_L_A0__A0R_A1", ExtAPI::EFT_L_A0__A0R_A1},
        {"ExtAPI::EFT_L_A1__FunPtr", ExtAPI::EFT_L_A1__FunPtr},
        {"ExtAPI::EFT_A1R_A0R", ExtAPI::EFT_A1R_A0R},
        {"ExtAPI::EFT_A3R_A1R_NS", ExtAPI::EFT_A3R_A1R_NS},
        {"ExtAPI::EFT_A1R_A0", ExtAPI::EFT_A1R_A0},
        {"ExtAPI::EFT_A2R_A1", ExtAPI::EFT_A2R_A1},
        {"ExtAPI::EFT_L_A0__A1_A0", ExtAPI::EFT_L_A0__A1_A0},
        {"ExtAPI::EFT_A4R_A1", ExtAPI::EFT_A4R_A1},
        {"ExtAPI::EFT_A0R_NEW", ExtAPI::EFT_A0R_NEW},
        {"ExtAPI::EFT_A1R_NEW", ExtAPI::EFT_A1R_NEW},
        {"ExtAPI::EFT_A2R_NEW", ExtAPI::EFT_A2R_NEW},
        {"ExtAPI::EFT_A4R_NEW", ExtAPI::EFT_A4R_NEW},
        {"ExtAPI::EFT_A11R_NEW", ExtAPI::EFT_A11R_NEW},
        {"ExtAPI::EFT_STD_LIST_HOOK", ExtAPI::EFT_STD_LIST_HOOK},
        {"ExtAPI::CPP_EFT_A0R_A1", ExtAPI::CPP_EFT_A0R_A1},
        {"ExtAPI::CPP_EFT_A0R_A1R", ExtAPI::CPP_EFT_A0R_A1R},
        {"ExtAPI::CPP_EFT_A1R", ExtAPI::CPP_EFT_A1R},
        {"ExtAPI::CPP_EFT_DYNAMIC_CAST", ExtAPI::CPP_EFT_DYNAMIC_CAST}};

/*  FIXME:
 *  SSL_CTX_ctrl, SSL_ctrl - may set the ptr field arg0->x
 *  SSL_CTX_set_verify - sets the function ptr field arg0->x
 *  X509_STORE_CTX_get_current_cert - returns arg0->x
 *  X509_get_subject_name - returns arg0->x->y
 *  XStringListToTextProperty, XGetWindowAttributes - sets arg2->x
 *  XInitImage - sets function ptrs arg0->x->y
 *  XMatchVisualInfo - sets arg4->x
 *  XtGetApplicationResources - ???
 *  glob - sets arg3->gl_pathv
 *  gnutls_pkcs12_bag_get_data - copies arg0->element[arg1].data to *arg2
 *  gnutls_pkcs12_get_bag - finds the arg1'th bag in the ASN1 tree structure
 *    rooted at arg0->pkcs12 and copies it to *arg2
 *  gnutls_pkcs12_import - builds an ASN1 tree rooted at arg0->pkcs12,
 *    based on decrypted data
 *  gnutls_x509_crt_import - builds an ASN1 tree rooted at arg0->cert
 *  gnutls_x509_privkey_export_rsa_raw - points arg1->data thru arg6->data
 *    to new strings
 *  gnutls_x509_privkey_import, gnutls_x509_privkey_import_pkcs8 -
 *    builds an ASN1 tree rooted at arg0->key from decrypted data
 *  cairo_get_target - returns arg0->gstate->original_target
 *  hasmntopt - returns arg0->mnt_opts
 */

void ExtAPI::init()
{
    set<extf_t> t_seen;
    extf_t prev_t = EFT_NOOP;
    t_seen.insert(EFT_NOOP);
    std::string get_line, get_str, temp_str;
    char get_char;
    const char *ei_pair_n;
    ExtAPI::extf_t ei_pair_t;
    bool getEIPairs = false;
    std::size_t pos_start, pos_end;
    std::ifstream getEiPairs("lib/Util/summary.txt");
    while (std::getline(getEiPairs, get_line))
    {
        // Remove spaces
        for (char c : get_line)
        {
            if (c != ' ')
            {
                get_char = c;
                break;
            }
        }
        get_str = get_line.substr(get_line.find(get_char));
        if (get_str.find("ei_pair ei_pairs[]") == 0)
        {
            getEIPairs = true;
        }
        else if (get_str.find("};") == 0)
        {
            getEIPairs = false;
        }
        if (getEIPairs)
        {
            if (get_str.find("{") == 0)
            {
                pos_start = 1;
                pos_end = get_str.find(",") - 1;
                std::string n_str;
                for (char c : get_str.substr(pos_start, pos_end))
                {
                    // Remove " "
                    if (c != '"')
                    {
                        n_str += c;
                    }
                }
                // Get const *char ei_pair_n
                if (n_str.find("\\01") == 0)
                {
                    temp_str = "\01" + n_str.substr(3);
                    ei_pair_n = temp_str.c_str();
                }
                else if (n_str.find("0") == 0)
                {
                    ei_pair_n = 0;
                }
                else
                {
                    ei_pair_n = n_str.c_str();
                }
                // Get ExtAPI::extf_t ei_pair_t
                pos_start = get_str.find(",");
                std::string t_str;
                for (char c : get_str.substr(pos_start + 1))
                {
                    if (c == '}')
                    {
                        break;
                    }
                    if (c != ' ')
                    {
                        t_str += c;
                    }
                }
                for (ei_pair_map map : ei_pair_maps)
                {
                    if (t_str.compare(map.string_ref) == 0)
                    {
                        ei_pair_t = map.t_ref;
                        break;
                    }
                }
                if (ei_pair_t != prev_t)
                {
                    //This will detect if you move an entry to another block
                    //  but forget to change the type.
                    if (t_seen.count(ei_pair_t) && ei_pair_n != 0)
                    {
                        fputs(ei_pair_n, stderr);
                        putc('\n', stderr);
                        assert(!"ei_pairs not grouped by type");
                    }
                    t_seen.insert(ei_pair_t);
                    prev_t = ei_pair_t;
                }
                if (info.count(ei_pair_n))
                {
                    fputs(ei_pair_n, stderr);
                    putc('\n', stderr);
                    assert(!"duplicate name in ei_pairs");
                }
                info[ei_pair_n] = ei_pair_t;
            }
        }
    }
}
