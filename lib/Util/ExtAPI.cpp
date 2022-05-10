/*  [ExtAPI.cpp] The actual database of external functions
 *  v. 005, 2008-08-08
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
*/

#include "Util/ExtAPI.h"
#include "Util/SVFUtil.h"   // for debugging
#include <dirent.h>         // to interact with directories
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>          // for getenv
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <string>
#include <fstream>          // to read extAPI.txt
#include <sys/stat.h>       // for chmod
#include <limits.h>
#include <sys/types.h>

using namespace std;
using namespace SVF;

ExtAPI* ExtAPI::extAPI = nullptr;

namespace {

struct ei_pair
{
    const char *n;
    ExtAPI::extf_t t;
    ei_pair(const char* ID, ExtAPI::extf_t in): n(ID), t(in) {}
};

} // End anonymous namespace

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
    // extf_t prev_t= EFT_NOOP;
    // t_seen.insert(EFT_NOOP);

    vector<ei_pair> ei_pairs;           
 
    const char* env = std::getenv("SVF_DIR");
    // const char* lib = "/lib";

    std::string env_str(env);
    // std::string lib_str(lib);

    // env_str.append(lib_str);

    const char* file_name = env_str.c_str();        // convert string to const char*
    // const char* full_path;
    
    DIR *pdir = nullptr;                    // declare a pointer to a directory
    // struct dirent *pent = nullptr;          // a struct used when reading a directory
    pdir = opendir(file_name);
    SVFUtil::outs() << "Opening " << file_name << "\n";
    assert(pdir != nullptr && "directory pointer could not be initialised correctly!");
    // char buffer[PATH_MAX];
    
    // std::vector<std::pair<std::string,std::string>> data;       // to preprocess extAPI.txt

    // while((pent = readdir(pdir))){
    //     assert(pent != nullptr && "Reading file in a directory unsucessful");
    //     // std::cout << pent->d_name << "\n";
    //     // find extAPI.txt in lib and open it if found
    //     if(strcmp(pent->d_name,"extAPI.txt")==0){
    //         full_path = realpath(file_name,buffer);
    //         std::string full_path_str(full_path);
    //         full_path_str.append("/");
    //         std::string extapi_file(pent->d_name);
    //         full_path_str.append(extapi_file);
    //         SVFUtil::outs() << "Full path to extAPI.txt " << full_path_str << "\n";

    //         std::ifstream file;
    //         file.open(full_path_str.c_str());

    //         assert(file.is_open() && "File cannot be opened");
    //         if(file.is_open()){
    //             SVFUtil::outs() << "Sucessfully opened " << full_path_str.c_str() << "\n";
    //             std::string ID;
    //             std::string effect;
    //             while(getline(file,ID,',') && getline(file,effect)){
    //                 data.push_back(make_pair(ID,effect));
    //             }
    //         }
    //         file.close();
    //     }
    // }
    // closedir(pdir);
    // assert(!data.empty() && "Migrating extAPI.txt unss");
    // check if SVF_DIR environment variable is set
    // const char* env = std::getenv("SVF_DIR"); // root folder of SVF
    // // const char* lib = "/lib";
    // assert(env != nullptr && "SVF_DIR not set");

    // string file_path(env);
    // string lib_str(lib);
    // file_path.append(lib_str);              // path to SVF "lib" folder

    // const char* dir = file_path.c_str();    // convert string path to const char*
    // assert(dir != nullptr && "SVF_DIR/lib pointer was not initialised correctly");
    // SVFUtil::outs() << "path to directory is set to " << dir << "\n";

    // // const char* full_path = nullptr;       // store full path to extAPI.txt to open it
    // // char buffer[PATH_MAX];                 // helper for getting full path

    // DIR *pdir = NULL;                               // declare a pointer to a directory
    // // struct dirent *pent = NULL;                     // a struct used when reading a directory
    // pdir = opendir(".");                            // open the directory
    // if(pdir == NULL){
    //     SVFUtil::outs() << "pdir is NULL" << "\n";
    // }
    // assert(pdir != NULL && errno);
    // if(pdir == NULL){
    //     int res = chmod(file_path.c_str(),S_IRWXU|S_IRWXG);
    //     assert(res == 0 && errno);
    // }
    // pdir = opendir(file_path.c_str());                  // attempt again after setting permissions
    


    // while((pent = readdir(pdir))){
    //     assert(pent != nullptr && "Reading file in a directory unsucessful");
    //     if(strcmp(pent->d_name,"extAPI.txt") == 0){
    //         full_path = realpath(dir,buffer);         // store directory path (does not contain extAPI.txt yet)
    //         string full_path_str(full_path);                // convert to string for append capabilities
    //         full_path_str.append("/");
    //         string extapi_file(pent->d_name);               // this should be extAPI.txt according to assertion
    //         full_path_str.append(extapi_file);

    //         std::ifstream file;
    //         file.open(full_path_str.c_str());               // convert to char*
    //         assert(file.is_open() && "File cannot be opened");
            
    //         if(file.is_open()){
    //             string ID;                      // the external API function name
    //             string effect;                  // the corresponding extf enum
    //             while(getline(file,ID,',') && getline(file,effect)){
    //                 data.push_back(make_pair(ID,effect));                 // store the pair data from the .txt file
    //             }
    //         }
    //         file.close();
    //     }
    // }
    // closedir(pdir);

    // assert(!data.empty() && "Migrating extAPI.txt file data unsuccesful");

    // transform line from txt file to its corresponding extf_t type
    map<string, ExtAPI::extf_t> extf_map = {
        {"ExtAPI::EFT_NOOP", ExtAPI::EFT_NOOP},
        {"ExtAPI::EFT_ALLOC", ExtAPI::EFT_ALLOC},
        {"ExtAPI::EFT_REALLOC", ExtAPI::EFT_REALLOC},
        {"ExtAPI::EFT_FREE", ExtAPI::EFT_FREE},
        {"ExtAPI::EFT_NOSTRUCT_ALLOC", ExtAPI::EFT_NOSTRUCT_ALLOC},
        {"ExtAPI::EFT_STAT", ExtAPI::EFT_STAT},
        {"ExtAPI::EFT_STAT2", ExtAPI::EFT_STAT2},
        {"ExtAPI::EFT_L_A0", ExtAPI::EFT_L_A0},
        {"ExtAPI::EFT_L_A1", ExtAPI::EFT_L_A1},
        {"ExtAPI::EFT_L_A2", ExtAPI::EFT_L_A2},
        {"ExtAPI::EFT_L_A8", ExtAPI::EFT_L_A8},
        {"ExtAPI::EFT_L_A0__A0R_A1", ExtAPI::EFT_L_A0__A0R_A1},
        {"ExtAPI::EFT_L_A0__A0R_A1R", ExtAPI::EFT_L_A0__A0R_A1R},
        {"ExtAPI::EFT_L_A1__FunPtr", ExtAPI::EFT_L_A1__FunPtr},
        {"ExtAPI::EFT_A1R_A0R", ExtAPI::EFT_A1R_A0R},
        {"ExtAPI::EFT_A3R_A1R_NS", ExtAPI::EFT_A3R_A1R_NS},
        {"ExtAPI::EFT_A1R_A0", ExtAPI::EFT_A1R_A0},
        {"ExtAPI::EFT_A2R_A1", ExtAPI::EFT_A2R_A1},
        {"ExtAPI::EFT_A4R_A1", ExtAPI::EFT_A4R_A1},
        {"ExtAPI::EFT_L_A0__A2R_A0", ExtAPI::EFT_L_A0__A2R_A0},
        {"ExtAPI::EFT_L_A0__A1_A0", ExtAPI::EFT_L_A0__A1_A0},
        {"ExtAPI::EFT_A0R_NEW", ExtAPI::EFT_A0R_NEW},
        {"ExtAPI::EFT_A1R_NEW", ExtAPI::EFT_A1R_NEW},
        {"ExtAPI::EFT_A2R_NEW", ExtAPI::EFT_A2R_NEW},
        {"ExtAPI::EFT_A4R_NEW", ExtAPI::EFT_A4R_NEW},
        {"ExtAPI::EFT_A11R_NEW", ExtAPI::EFT_A11R_NEW},
        {"ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE", ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE},
        {"ExtAPI::EFT_STD_RB_TREE_INCREMENT", ExtAPI::EFT_STD_RB_TREE_INCREMENT},
        {"ExtAPI::EFT_STD_LIST_HOOK", ExtAPI::EFT_STD_LIST_HOOK},
        {"ExtAPI::CPP_EFT_A0R_A1", ExtAPI::CPP_EFT_A0R_A1},
        {"ExtAPI::CPP_EFT_A0R_A1R", ExtAPI::CPP_EFT_A0R_A1R},
        {"ExtAPI::CPP_EFT_A1R", ExtAPI::CPP_EFT_A1R},
        {"ExtAPI::EFT_CXA_BEGIN_CATCH", ExtAPI::EFT_CXA_BEGIN_CATCH},
        {"ExtAPI::CPP_EFT_DYNAMIC_CAST", ExtAPI::CPP_EFT_DYNAMIC_CAST},
        {"ExtAPI::EFT_OTHER", ExtAPI::EFT_OTHER}
    };

    // construct ei_pairs and store ei_pairs to ei_pairs vector
    // for(auto data_iter = data.begin(); data_iter != data.end(); data_iter++){
    //     // check for duplicate side effects on the same external function name
    //     const char* c = data_iter->first.c_str();                               // convert from std::string to const char* as per struct member
    //     ei_pairs.push_back(ei_pair(c,extf_map[data_iter->second]));             // construct ei_pair with external function name and extf_t type
    // }
    // assert(!ei_pairs.empty() && "ei_pairs vector is empty");

    // for(auto p= ei_pairs.begin(); p != ei_pairs.end(); ++p)
    // {
    //     if(p->t != prev_t)
    //     {
    //         //This will detect if you move an entry to another block
    //         //  but forget to change the type.
    //         if(t_seen.count(p->t))
    //         {
    //             fputs(p->n, stderr);
    //             putc('\n', stderr);
    //             assert(!"ei_pairs not grouped by type");
    //         }
    //         t_seen.insert(p->t);
    //         prev_t= p->t;
    //     }
    //     if(info.count(p->n))
    //     {
    //         fputs(p->n, stderr);
    //         putc('\n', stderr);
    //         assert(!"duplicate name in ei_pairs");
    //     }
    //     info[p->n]= p->t;
    // }
}   // end init() function