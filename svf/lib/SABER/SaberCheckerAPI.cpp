//===- SaberCheckerAPI.cpp -- API for checkers-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SaberCheckerAPI.cpp
 *
 *  Created on: Apr 23, 2014
 *      Author: Yulei Sui
 */
#include "SABER/SaberCheckerAPI.h"
#include <stdio.h>

using namespace std;
using namespace SVF;

SaberCheckerAPI* SaberCheckerAPI::ckAPI = nullptr;

namespace
{

/// string and type pair
struct ei_pair
{
    const char *n;
    SaberCheckerAPI::CHECKER_TYPE t;
};

} // End anonymous namespace

//Each (name, type) pair will be inserted into the map.
//All entries of the same type must occur together (for error detection).
static const ei_pair ei_pairs[]=
{
    {"alloc", SaberCheckerAPI::CK_ALLOC},
    {"alloc_check", SaberCheckerAPI::CK_ALLOC},
    {"alloc_clear", SaberCheckerAPI::CK_ALLOC},
    {"calloc", SaberCheckerAPI::CK_ALLOC},
    {"jpeg_alloc_huff_table", SaberCheckerAPI::CK_ALLOC},
    {"jpeg_alloc_quant_table", SaberCheckerAPI::CK_ALLOC},
    {"lalloc", SaberCheckerAPI::CK_ALLOC},
    {"lalloc_clear", SaberCheckerAPI::CK_ALLOC},
    {"malloc", SaberCheckerAPI::CK_ALLOC},
    {"nhalloc", SaberCheckerAPI::CK_ALLOC},
    {"oballoc", SaberCheckerAPI::CK_ALLOC},
    {"permalloc", SaberCheckerAPI::CK_ALLOC},
    {"png_create_info_struct", SaberCheckerAPI::CK_ALLOC},
    {"png_create_write_struct", SaberCheckerAPI::CK_ALLOC},
    {"safe_calloc", SaberCheckerAPI::CK_ALLOC},
    {"safe_malloc", SaberCheckerAPI::CK_ALLOC},
    {"safecalloc", SaberCheckerAPI::CK_ALLOC},
    {"safemalloc", SaberCheckerAPI::CK_ALLOC},
    {"safexcalloc", SaberCheckerAPI::CK_ALLOC},
    {"safexmalloc", SaberCheckerAPI::CK_ALLOC},
    {"savealloc", SaberCheckerAPI::CK_ALLOC},
    {"xalloc", SaberCheckerAPI::CK_ALLOC},
    {"xcalloc", SaberCheckerAPI::CK_ALLOC},
    {"xmalloc", SaberCheckerAPI::CK_ALLOC},
    {"SSL_CTX_new", SaberCheckerAPI::CK_ALLOC},
    {"SSL_new", SaberCheckerAPI::CK_ALLOC},
    {"VOS_MemAlloc", SaberCheckerAPI::CK_ALLOC},

    {"VOS_MemFree", SaberCheckerAPI::CK_FREE},
    {"cfree", SaberCheckerAPI::CK_FREE},
    {"free", SaberCheckerAPI::CK_FREE},
    {"free_all_mem", SaberCheckerAPI::CK_FREE},
    {"freeaddrinfo", SaberCheckerAPI::CK_FREE},
    {"gcry_mpi_release", SaberCheckerAPI::CK_FREE},
    {"gcry_sexp_release", SaberCheckerAPI::CK_FREE},
    {"globfree", SaberCheckerAPI::CK_FREE},
    {"nhfree", SaberCheckerAPI::CK_FREE},
    {"obstack_free", SaberCheckerAPI::CK_FREE},
    {"safe_cfree", SaberCheckerAPI::CK_FREE},
    {"safe_free", SaberCheckerAPI::CK_FREE},
    {"safefree", SaberCheckerAPI::CK_FREE},
    {"safexfree", SaberCheckerAPI::CK_FREE},
    {"sm_free", SaberCheckerAPI::CK_FREE},
    {"vim_free", SaberCheckerAPI::CK_FREE},
    {"xfree", SaberCheckerAPI::CK_FREE},
    {"SSL_CTX_free", SaberCheckerAPI::CK_FREE},
    {"SSL_free", SaberCheckerAPI::CK_FREE},
    {"XFree", SaberCheckerAPI::CK_FREE},

    {"fopen", SaberCheckerAPI::CK_FOPEN},
    {"\01_fopen", SaberCheckerAPI::CK_FOPEN},
    {"\01fopen64", SaberCheckerAPI::CK_FOPEN},
    {"\01readdir64", SaberCheckerAPI::CK_FOPEN},
    {"\01tmpfile64", SaberCheckerAPI::CK_FOPEN},
    {"fopen64", SaberCheckerAPI::CK_FOPEN},
    {"XOpenDisplay", SaberCheckerAPI::CK_FOPEN},
    {"XtOpenDisplay", SaberCheckerAPI::CK_FOPEN},
    {"fopencookie", SaberCheckerAPI::CK_FOPEN},
    {"popen", SaberCheckerAPI::CK_FOPEN},
    {"readdir", SaberCheckerAPI::CK_FOPEN},
    {"readdir64", SaberCheckerAPI::CK_FOPEN},
    {"gzdopen", SaberCheckerAPI::CK_FOPEN},
    {"iconv_open", SaberCheckerAPI::CK_FOPEN},
    {"tmpfile", SaberCheckerAPI::CK_FOPEN},
    {"tmpfile64", SaberCheckerAPI::CK_FOPEN},
    {"BIO_new_socket", SaberCheckerAPI::CK_FOPEN},
    {"gcry_md_open", SaberCheckerAPI::CK_FOPEN},
    {"gcry_cipher_open", SaberCheckerAPI::CK_FOPEN},


    {"fclose", SaberCheckerAPI::CK_FCLOSE},
    {"XCloseDisplay", SaberCheckerAPI::CK_FCLOSE},
    {"XtCloseDisplay", SaberCheckerAPI::CK_FCLOSE},
    {"__res_nclose", SaberCheckerAPI::CK_FCLOSE},
    {"pclose", SaberCheckerAPI::CK_FCLOSE},
    {"closedir", SaberCheckerAPI::CK_FCLOSE},
    {"dlclose", SaberCheckerAPI::CK_FCLOSE},
    {"gzclose", SaberCheckerAPI::CK_FCLOSE},
    {"iconv_close", SaberCheckerAPI::CK_FCLOSE},
    {"gcry_md_close", SaberCheckerAPI::CK_FCLOSE},
    {"gcry_cipher_close", SaberCheckerAPI::CK_FCLOSE},

    //This must be the last entry.
    {0, SaberCheckerAPI::CK_DUMMY}

};


/*!
 * initialize the map
 */
void SaberCheckerAPI::init()
{
    set<CHECKER_TYPE> t_seen;
    CHECKER_TYPE prev_t= CK_DUMMY;
    t_seen.insert(CK_DUMMY);
    for(const ei_pair *p= ei_pairs; p->n; ++p)
    {
        if(p->t != prev_t)
        {
            //This will detect if you move an entry to another block
            //  but forget to change the type.
            if(t_seen.count(p->t))
            {
                fputs(p->n, stderr);
                putc('\n', stderr);
                assert(!"ei_pairs not grouped by type");
            }
            t_seen.insert(p->t);
            prev_t= p->t;
        }
        if(tdAPIMap.count(p->n))
        {
            fputs(p->n, stderr);
            putc('\n', stderr);
            assert(!"duplicate name in ei_pairs");
        }
        tdAPIMap[p->n]= p->t;
    }
}




