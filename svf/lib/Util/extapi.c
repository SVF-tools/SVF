#define NULL ((void *)0)

/*
   The functions in extapi.bc are named using two patterns:
    1: "svf_methodName";
    2: "svf_methodName_methodProperties";

    "svf_methodName": which is used to replace the functionality of methodName with the 'definition' of svf_methodName.

    "svf_methodName_methodProperties": The second pattern, svf_methodName_methodProperties, 
        is used when certain external functions cannot be implemented using C/C++ code. For example, 
        functions like malloc() require object allocation, which SVF needs to analyze at a logical level 
        while the actual malloc() is executed at a physical level. In such cases, additional properties
        are added to the signature of the external method. svf_methodName_methodProperties is used as a 
        'declaration', and when SVF encounters a function named svf_methodName_methodProperties(), 
        it processes the method based on its name and properties.


    However, when Clang compiles extapi.c to LLVM IR, it treats unused function declarations in svf_methodName_methodProperties as dead code
    and removes them. To preserve the function declarations of svf_methodName_methodProperties, they need to be called in svf_preserveExtFuncDeclarations().

    The description of methodProperties is as follows:

        ALLOC_RET,    // returns a ptr to a newly allocated object
        ALLOC_ARGi    // stores a pointer to an allocated object in *argi
        REALLOC_RET,  
        STATIC,       // retval points to an unknown static var X
        MEMSET,       // memcpy() operations
        MEMCPY,       // memset() operations
        OVERWRITE,    // svf extern function overwrite app function
*/

extern const unsigned short **svf___ctype_b_loc_STATIC_ALLOC_RET(void);

extern int **svf___ctype_tolower_loc_STATIC_ALLOC_RET(void);

extern int **svf___ctype_toupper_loc_STATIC_ALLOC_RET(void);

extern int *svf___errno_location_STATIC_ALLOC_RET(void);

extern int * svf___h_errno_location_STATIC_ALLOC_RET(void);

extern void* svf___res_state_STATIC_ALLOC_RET(void);

extern char *svf_asctime_STATIC_ALLOC_RET(const void *timeptr);

extern char * svf_bindtextdomain_STATIC_ALLOC_RET(const char * domainname, const char * dirname);

extern char * svf_bind_textdomain_codeset_STATIC_ALLOC_RET(const char * domainname, const char * codeset);

extern char *svf_ctermid_STATIC_ALLOC_RET(char *s);

extern char * svf_dcgettext_STATIC_ALLOC_RET(const char * domainname, const char * msgid, int category);

extern char * svf_dgettext_STATIC_ALLOC_RET(const char * domainname, const char * msgid);

extern char * svf_dngettext_STATIC_ALLOC_RET(const char * domainname, const char * msgid, const char * msgid_plural, unsigned long int n);

extern void *svf_fdopen_STATIC_ALLOC_RET(int fd, const char *mode);

extern struct group *svf_getgrgid_STATIC_ALLOC_RET(unsigned int gid);

extern struct group *svf_getgrnam_STATIC_ALLOC_RET(const char *name);

extern struct hostent *svf_gethostbyaddr_STATIC_ALLOC_RET(const void *addr, unsigned int len, int type);

extern struct hostent *svf_gethostbyname_STATIC_ALLOC_RET(const char *name);

extern struct hostent *svf_gethostbyname2_STATIC_ALLOC_RET(const char *name, int af);

extern struct mntent *svf_getmntent_STATIC_ALLOC_RET(void *stream);

extern struct protoent *svf_getprotobyname_STATIC_ALLOC_RET(const char *name);

extern struct protoent *svf_getprotobynumber_STATIC_ALLOC_RET(int proto);

extern struct passwd *svf_getpwent_STATIC_ALLOC_RET(void);

extern struct passwd *svf_getpwnam_STATIC_ALLOC_RET(const char *name);

extern struct passwd *svf_getpwuid_STATIC_ALLOC_RET(unsigned int uid);

extern struct servent *svf_getservbyname_STATIC_ALLOC_RET(const char *name, const char *proto);

extern struct servent *svf_getservbyport_STATIC_ALLOC_RET(int port, const char *proto);

extern struct spwd *svf_getspnam_STATIC_ALLOC_RET(const char *name);

extern char * svf_gettext_STATIC_ALLOC_RET(const char * msgid);

extern struct tm *svf_gmtime_STATIC_ALLOC_RET(const void *timer);

extern const char *svf_gnu_get_libc_version_STATIC_ALLOC_RET(void);

extern const char * svf_gnutls_check_version_STATIC_ALLOC_RET(const char * req_version);

extern struct lconv *svf_localeconv_STATIC_ALLOC_RET(void);

extern struct tm *svf_localtime_STATIC_ALLOC_RET(const void *timer);

extern char * svf_ngettext_STATIC_ALLOC_RET(const char * msgid, const char * msgid_plural, unsigned long int n);

extern void *svf_pango_cairo_font_map_get_default_STATIC_ALLOC_RET(void);

extern char *svf_re_comp_STATIC_ALLOC_RET(const char *regex);

extern char *svf_setlocale_STATIC_ALLOC_RET(int category, const char *locale);

extern char *svf_tgoto_STATIC_ALLOC_RET(const char *cap, int col, int row);

extern char *svf_tparm_STATIC_ALLOC_RET(char *str, ...);

extern const char *svf_zError_STATIC_ALLOC_RET(int a);

extern void *svf_fopen_ALLOC_RET(const char *voidname, const char *mode);

extern void *svf_fopen64_ALLOC_RET(const char *voidname, const char *mode);

extern struct dirent64 *svf_readdir64_ALLOC_RET(void *dirp);

extern void *svf_tmpvoid64_ALLOC_RET(void);

extern void *svf_calloc_ALLOC_RET(unsigned long nitems, unsigned long size);

extern void *svf_zmalloc_ALLOC_RET(unsigned long size);

extern void *svf_gzdopen_ALLOC_RET(int fd, const char *mode);

extern void *svf_iconv_open_ALLOC_RET(const char *tocode, const char *fromcode);

extern void *svf_lalloc_ALLOC_RET(unsigned long size, int a);

extern void *svf_lalloc_clear_ALLOC_RET(unsigned long size, int a);

extern long *svf_nhalloc_ALLOC_RET(unsigned int a, const char *b, int c);

extern void *svf_oballoc_ALLOC_RET(unsigned long size);

extern void *svf_popen_ALLOC_RET(const char *command, const char *type);

extern void *svf_pthread_getspecific_ALLOC_RET(const char *a, const char *b);

extern struct dirent *svf_readdir_ALLOC_RET(void *dirp);

extern void* svf_safe_calloc_ALLOC_RET(unsigned nelem, unsigned elsize);

extern void* svf_safe_malloc_ALLOC_RET(unsigned long);

extern char* svf_safecalloc_ALLOC_RET(int a, int b);

extern char* svf_safemalloc_ALLOC_RET(int a, int b);

extern void *svf_setmntent_ALLOC_RET(const char *voidname, const char *type);

extern void *svf_shmat_ALLOC_RET(int shmid, const void *shmaddr, int shmflg);

extern void* svf___sysv_signal_ALLOC_RET(int a, void *b);

extern void (*svf_signal_ALLOC_RET(int sig, void (*func)(int)))(int);

extern char *svf_tempnam_ALLOC_RET(const char *dir, const char *pfx);

extern void *svf_tmpvoid_ALLOC_RET(void);

extern void* svf_xcalloc_ALLOC_RET(unsigned long size1, unsigned long size2);

extern void* svf_xmalloc_ALLOC_RET(unsigned long size);

extern void *svf__Znam_ALLOC_RET(unsigned long size);

extern void *svf__Znaj_ALLOC_RET(unsigned long size);

extern void *svf__Znwj_ALLOC_RET(unsigned long size);

extern void *svf___cxa_allocate_exception_ALLOC_RET(unsigned long size);

extern void* svf_aligned_alloc_ALLOC_RET(unsigned long size1, unsigned long size2);

extern void* svf_memalign_ALLOC_RET(unsigned long size1, unsigned long size2);

extern void *svf_valloc_ALLOC_RET(unsigned long size);

extern void *svf_mmap64_ALLOC_RET(void *addr, unsigned long len, int prot, int flags, int fildes, long off);

extern char *svf_XSetLocaleModifiers_ALLOC_RET(char *a);

extern char * svf___strdup_ALLOC_RET(const char * string);

extern char *svf_crypt_ALLOC_RET(const char *key, const char *salt);

extern char *svf_ctime_ALLOC_RET(const void *timer);

extern char *svf_dlerror_ALLOC_RET(void);

extern void *svf_dlopen_ALLOC_RET(const char *voidname, int flags);

extern const char *svf_gai_strerror_ALLOC_RET(int errcode);

extern const char *svf_gcry_cipher_algo_name_ALLOC_RET(int errcode);

extern const char *svfgcry_md_algo_name_ALLOC_RET_(int errcode);

extern char *svf_getenv_ALLOC_RET(const char *name);

extern char *svf_getlogin_ALLOC_RET(void);

extern char *svf_getpass_ALLOC_RET(const char *prompt);

extern const char * svf_gnutls_strerror_ALLOC_RET(int error);

extern const char *svf_gpg_strerror_ALLOC_RET(unsigned int a);

extern const char * svf_gzerror_ALLOC_RET(void* file, int * errnum);

extern char *svf_inet_ntoa_ALLOC_RET(unsigned int in);

extern void *svf_initscr_ALLOC_RET(void);

extern void* svf_llvm_stacksave_ALLOC_RET();

extern void *svf_mmap_ALLOC_RET(void *addr, unsigned long len, int prot, int flags, int fildes, long off);

extern void *svf_newwin_ALLOC_RET(int nlines, int ncols, int begin_y, int begin_x);

extern char *svf_nl_langinfo_ALLOC_RET(int item);

extern void *svf_opendir_ALLOC_RET(const char *name);

extern void *svf_sbrk_ALLOC_RET(long increment);

extern char *svf_strdup_ALLOC_RET(const char *s);

extern char *svf_strerror_ALLOC_RET(int errnum);

extern char *svf_strsignal_ALLOC_RET(int errnum);

extern char * svf_textdomain_ALLOC_RET(const char * domainname);

extern char *svf_tgetstr_ALLOC_RET(char *id, char **area);

extern char *svf_tigetstr_ALLOC_RET(char *capname);

extern char *svf_tmpnam_ALLOC_RET(char *s);

extern char *svf_ttyname_ALLOC_RET(int fd);

extern void *svf_malloc_ALLOC_RET(unsigned long size);

extern char *svf_getcwd_REALLOC_RET(char *buf, unsigned long size);

extern char *svf_mem_realloc_REALLOC_RET(void *ptr, unsigned long size);

extern char *svf_realloc_REALLOC_RET(void *ptr, unsigned long size);

extern void* svf_safe_realloc_REALLOC_RET(void *p, unsigned long n);

extern void* svf_saferealloc_REALLOC_RET(void *p, unsigned long n1, unsigned long n2);

extern void* svf_safexrealloc_REALLOC_RET();

extern char *svf_strtok_REALLOC_RET(char *str, const char *delim);

extern char *svf_strtok_r_REALLOC_RET(char *str, const char *delim, char **saveptr);

extern void *svf_xrealloc_REALLOC_RET(void *ptr, unsigned long bytes);

extern void *svf__Znwm_ALLOC_RET(unsigned long size);

extern void *svf_SyGetmem_ALLOC_RET_OVERWRITE(unsigned long size);

extern int svf_asprintf_ALLOC_ARG0(char **restrict strp, const char *restrict fmt, ...);

extern int svf_vasprintf_ALLOC_ARG0(char **strp, const char *fmt, void* ap);

extern int svf_db_create_ALLOC_ARG0(void **dbp, void *dbenv, unsigned int flags);

extern int svf_gnutls_pkcs12_bag_init_ALLOC_ARG0(void *a);

extern int svf_gnutls_pkcs12_init_ALLOC_ARG0(void *a);

extern int svf_gnutls_x509_crt_init_ALLOC_ARG0(void *a);

extern int svf_gnutls_x509_privkey_init_ALLOC_ARG0(void *a);

extern int svf_posix_memalign_ALLOC_ARG0(void **a, unsigned long b, unsigned long c);

extern int svf_scandir_ALLOC_ARG1(const char *restrict dirp, struct dirent ***restrict namelist, int (*filter)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **));

extern int svf_XmbTextPropertyToTextList_ALLOC_ARG2(void *a, void *b, char ***c, int *d);

//llvm.memcpy.p0i8.p0i8.i64
extern void svf_llvm_memcpy_p0i8_p0i8_i64_MEMCPY(char* dst, char* src, int sz, int flag);

extern void svf_llvm_memcpy_p0i8_p0i8_i32_MEMCPY(char* dst, char* src, int sz, int flag);

extern void svf_llvm_memcpy_MEMCPY(char* dst, char* src, int sz, int flag);

extern void svf_llvm_memmove_MEMCPY(char* dst, char* src, int sz, int flag);

extern void svf_llvm_memmove_p0i8_p0i8_i64_MEMCPY(char* dst, char* src, int sz, int flag);

extern void svf_llvm_memmove_p0i8_p0i8_i32_MEMCPY(char* dst, char* src, int sz, int flag);

extern void svf___memcpy_chk_MEMCPY(char* dst, char* src, int sz, int flag);

extern void *svf_memmove_MEMCPY(void *str1, const void *str2, unsigned long n);

extern void svf_bcopy_MEMCPY(const void *s1, void *s2, unsigned long n);

extern void *svf_memccpy_MEMCPY( void * restrict dest, const void * restrict src, int c, unsigned long count);

extern void svf___memmove_chk_MEMCPY(char* dst, char* src, int sz);

extern void svf_llvm_memset_MEMSET(char* dst, char elem, int sz, int flag);

extern void svf_llvm_memset_p0i8_i32_MEMSET(char* dst, char elem, int sz, int flag);

extern void svf_llvm_memset_p0i8_i64_MEMSET(char* dst, char elem, int sz, int flag);

extern void svf_MEMSET(char* dst, char elem, int sz, int flag);

extern char * svf___memset_chk_MEMSET(char * dest, int c, unsigned long destlen, int flag);

char * svf___strcpy_chk_MEMCPY(char * dest, const char * src, unsigned long destlen);

extern char * svf___strcat_chk_MEMCPY(char * dest, const char * src, unsigned long destlen);

extern char *svf_stpcpy_MEMCPY(char *restrict dst, const char *restrict src);

extern char *svf_strcat_MEMCPY(char *dest, const char *src);

extern char *svf_strcpy_MEMCPY(char *dest, const char *src);

extern char *svf_strncat_MEMCPY(char *dest, const char *src, unsigned long n);

extern char *svf_strncpy_MEMCPY(char *dest, const char *src, unsigned long n);

extern unsigned long svf_iconv_MEMCPY(void* cd, char **restrict inbuf, unsigned long *restrict inbytesleft, char **restrict outbuf, unsigned long *restrict outbytesleft);

extern void* svf__ZNSt5arrayIPK1ALm2EE4backEv_OVERWRITE(void *arg);

extern void * svf___rawmemchr(const void * s, int c)
{
    return (void *)s;
}

struct jpeg_error_mgr *svf_jpeg_std_error(struct jpeg_error_mgr * a)
{
    return a;
}

char *svf_fgets(char *str, int n, void *stream)
{
    return str;
}

void *svf_memchr(const void *str, int c, unsigned long n)
{
    return (void *)str;
}

void * svf_mremap(void * old_address, unsigned long old_size, unsigned long new_size, int flags)
{
    return old_address;
}

char *svf_strchr(const char *str, int c)
{
    return (char *)str;
}

char *svf_strerror_r(int errnum, char *buf, unsigned long buflen)
{
    return buf;
}

char *svf_strpbrk(const char *str1, const char *str2)
{
    return (char *)str1;
}

char *svf_strptime(const char *restrict s, const char *restrict format, struct tm *restrict tm)
{
    return (char *)s;
}

char *svf_strrchr(const char *str, int c)
{
    return (char *)str;
}

char *svf_strstr(const char *haystack, const char *needle)
{
    return (char *)haystack;
}

char *svf_tmpnam_r(char *s)
{
    return s;
}

int svf_isalnum(int character)
{
    return character;
}

int svf_isalpha(int character)
{
    return character;
}

int svf_isblank(int character)
{
    return character;
}

int svf_iscntrl(int c)
{
    return c;
}

int svf_isdigit(int c)
{
    return c;
}

int svf_isgraph(int c)
{
    return c;
}

int svf_islower( int arg )
{
    return arg;
}

int svf_isprint(int c)
{
    return c;
}

int svf_ispunct(int argument)
{
    return argument;
}

int svf_isspace(char c)
{
    return c;
}

int svf_isupper(int c)
{
    return c;
}

int svf_isxdigit(int c)
{
    return c;
}

char *svf_asctime_r(const struct tm *tm, char *buf)
{
    return buf;
}

void *svf_bsearch(const void *key, const void *base, unsigned long nitems, unsigned long size, int (*compar)(const void *, const void *))
{
    return (void *)base;
}

struct mntent *svf_getmntent_r(void *fp, struct mntent *mntbuf, char *buf, int buflen)
{
    return mntbuf;
}

struct tm *svf_gmtime_r(const void *timer, struct tm *buf)
{
    return buf;
}

char * svf_gzgets(void* file, char * buf, int len)
{
    return buf;
}

struct tm *svf_localtime_r(const void *timep, struct tm *result)
{
    return result;
}

char *svf_realpath(const char *restrict path, char *restrict resolved_path)
{
    return resolved_path;
}

void* svf_freopen64( const char* voidname, const char* mode, void* fp )
{
    return fp;
}

void* svf_freopen(const char* voidname, const char* mode, void* fp)
{
    return fp;
}

const char *svf_inet_ntop(int af, const void *restrict src,  char *restrict dst, unsigned int size)
{
    return dst;
}

double svf_strtod(const char *str, char **endptr)
{
    *endptr = (char *)str;
    return 0.0;
}

float svf_strtof(const char *nptr, char **endptr)
{
    *endptr = (char *)nptr;
    return 0.0;
}

long int svf_strtol(const char *str, char **endptr, int base)
{
    *endptr = (char *)str;
    return 0;
}

long double svf_strtold(const char* str, char** endptr)
{
    *endptr = (char *)str;
    return 0.0;
}

unsigned long int svf_strtoul(const char *str, char **endptr, int base)
{
    *endptr = (char *)str;
    return 0;
}

int svf_readdir_r(void *__restrict__dir, struct dirent *__restrict__entry, struct dirent **__restrict__result)
{
    __restrict__entry = *__restrict__result;   
    return 0;
}

int svf_getpwnam_r(const char *name, struct passwd *pwd, char *buf, unsigned long buflen, struct passwd **result)
{
    *result = pwd;
    return 0;
}

int svf_getpwuid_r(unsigned int uid, struct passwd *pwd, char *buf, unsigned long buflen, struct passwd **result)
{
    *result = pwd;
    return 0;
}


void svf__ZNSt8__detail15_List_node_base7_M_hookEPS0_(void *arg0, void **arg1)
{
    *arg1 = arg0;
}

void* svf___dynamic_cast(void* source, const void* sourceTypeInfo, const void* targetTypeInfo, unsigned long castType)
{
    return source;
}

void svf__ZNSsC1EPKcRKSaIcE(void **arg0, void *arg1)
{
    *arg0 = arg1;
}

void svf__ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1EPKcRKS3_(void **arg0, void *arg1)
{
    *arg0 = arg1;
}

void svf_preserveExtFuncDeclarations()
{
    svf_asprintf_ALLOC_ARG0(NULL, NULL);
    svf_vasprintf_ALLOC_ARG0(NULL, NULL, NULL);
    svf_db_create_ALLOC_ARG0(NULL, NULL, 0);
    svf_gnutls_pkcs12_bag_init_ALLOC_ARG0(NULL);
    svf_gnutls_pkcs12_init_ALLOC_ARG0(NULL);
    svf_gnutls_x509_crt_init_ALLOC_ARG0(NULL);
    svf_gnutls_x509_privkey_init_ALLOC_ARG0(NULL);
    svf_posix_memalign_ALLOC_ARG0(NULL, 0, 0);
    svf_scandir_ALLOC_ARG1(NULL, NULL, NULL, NULL);
    svf_XmbTextPropertyToTextList_ALLOC_ARG2(NULL, NULL, NULL, NULL);
    svf_llvm_memcpy_p0i8_p0i8_i64_MEMCPY(NULL, NULL, 0, 0);
    svf_llvm_memcpy_p0i8_p0i8_i32_MEMCPY(NULL, NULL, 0, 0);
    svf_llvm_memcpy_MEMCPY(NULL, NULL, 0, 0);
    svf_llvm_memmove_MEMCPY(NULL, NULL, 0, 0);
    svf_llvm_memmove_p0i8_p0i8_i64_MEMCPY(NULL, NULL, 0, 0);
    svf_llvm_memmove_p0i8_p0i8_i32_MEMCPY(NULL, NULL, 0, 0);
    svf___memcpy_chk_MEMCPY(NULL, NULL, 0, 0);
    svf_memmove_MEMCPY(NULL, NULL, 0);
    svf_bcopy_MEMCPY(NULL, NULL, 0);
    svf_memccpy_MEMCPY(NULL, NULL, 0, 0);
    svf___memmove_chk_MEMCPY(NULL, NULL, 0);
    svf_llvm_memset_MEMSET(NULL, 'a', 0, 0);
    svf_llvm_memset_p0i8_i32_MEMSET(NULL, 'a', 0, 0);
    svf_llvm_memset_p0i8_i64_MEMSET(NULL, 'a', 0, 0);
    svf_MEMSET(NULL, 'a', 0, 0);
    svf___memset_chk_MEMSET(NULL, 'a', 0, 0);
    svf___strcpy_chk_MEMCPY(NULL, NULL, 0);
    svf___strcat_chk_MEMCPY(NULL, NULL, 0);
    svf_stpcpy_MEMCPY(NULL, NULL);
    svf_strcat_MEMCPY(NULL, NULL);
    svf_strcpy_MEMCPY(NULL, NULL);
    svf_strncat_MEMCPY(NULL, NULL, 0);
    svf_strncpy_MEMCPY(NULL, NULL, 0);
    svf_iconv_MEMCPY(NULL, NULL, NULL, NULL, NULL);
    svf___ctype_b_loc_STATIC_ALLOC_RET();
    svf___ctype_tolower_loc_STATIC_ALLOC_RET();
    svf___ctype_toupper_loc_STATIC_ALLOC_RET();
    svf___errno_location_STATIC_ALLOC_RET();
    svf___h_errno_location_STATIC_ALLOC_RET();
    svf___res_state_STATIC_ALLOC_RET();
    svf_asctime_STATIC_ALLOC_RET(NULL);
    svf_bindtextdomain_STATIC_ALLOC_RET(NULL, NULL);
    svf_bind_textdomain_codeset_STATIC_ALLOC_RET(NULL, NULL);
    svf_ctermid_STATIC_ALLOC_RET(NULL);
    svf_dcgettext_STATIC_ALLOC_RET(NULL, NULL, 0);
    svf_dgettext_STATIC_ALLOC_RET(NULL, NULL);
    svf_dngettext_STATIC_ALLOC_RET(NULL, NULL, NULL, 0);
    svf_fdopen_STATIC_ALLOC_RET(0, NULL);
    svf_getgrgid_STATIC_ALLOC_RET(0);
    svf_getgrnam_STATIC_ALLOC_RET(NULL);
    svf_gethostbyaddr_STATIC_ALLOC_RET(NULL, 0, 0);
    svf_gethostbyname_STATIC_ALLOC_RET(NULL);
    svf_gethostbyname2_STATIC_ALLOC_RET(NULL, 0);
    svf_getmntent_STATIC_ALLOC_RET(NULL);
    svf_getprotobyname_STATIC_ALLOC_RET(NULL);
    svf_getprotobynumber_STATIC_ALLOC_RET(0);
    svf_getpwent_STATIC_ALLOC_RET();
    svf_getpwnam_STATIC_ALLOC_RET(NULL);
    svf_getpwuid_STATIC_ALLOC_RET(0);
    svf_getservbyname_STATIC_ALLOC_RET(NULL, NULL);
    svf_getservbyport_STATIC_ALLOC_RET(0, NULL);
    svf_getspnam_STATIC_ALLOC_RET(NULL);
    svf_gettext_STATIC_ALLOC_RET(NULL);
    svf_gmtime_STATIC_ALLOC_RET(NULL);
    svf_gnu_get_libc_version_STATIC_ALLOC_RET();
    svf_gnutls_check_version_STATIC_ALLOC_RET(NULL);
    svf_localeconv_STATIC_ALLOC_RET();
    svf_localtime_STATIC_ALLOC_RET(NULL);
    svf_ngettext_STATIC_ALLOC_RET(NULL, NULL, 0);
    svf_pango_cairo_font_map_get_default_STATIC_ALLOC_RET();
    svf_re_comp_STATIC_ALLOC_RET(NULL);
    svf_setlocale_STATIC_ALLOC_RET(0, NULL);
    svf_tgoto_STATIC_ALLOC_RET(NULL, 0, 0);
    svf_tparm_STATIC_ALLOC_RET(NULL);
    svf_zError_STATIC_ALLOC_RET(0);
    svf_fopen_ALLOC_RET(NULL, NULL);
    svf_fopen64_ALLOC_RET(NULL, NULL);
    svf_readdir64_ALLOC_RET(NULL);
    svf_tmpvoid64_ALLOC_RET();
    svf_calloc_ALLOC_RET(0, 0);
    svf_zmalloc_ALLOC_RET(0);
    svf_gzdopen_ALLOC_RET(0, NULL);
    svf_iconv_open_ALLOC_RET(NULL, NULL);
    svf_lalloc_ALLOC_RET(0, 0);
    svf_lalloc_clear_ALLOC_RET(0, 0);
    svf_nhalloc_ALLOC_RET(0, NULL, 0);
    svf_oballoc_ALLOC_RET(0);
    svf_popen_ALLOC_RET(NULL, NULL);
    svf_pthread_getspecific_ALLOC_RET(NULL, NULL);
    svf_readdir_ALLOC_RET(NULL);
    svf_safe_calloc_ALLOC_RET(0, 0);
    svf_safe_malloc_ALLOC_RET(0);
    svf_safecalloc_ALLOC_RET(0, 0);
    svf_safemalloc_ALLOC_RET(0, 0);
    svf_setmntent_ALLOC_RET(NULL, NULL);
    svf_shmat_ALLOC_RET(0, NULL, 0);
    svf___sysv_signal_ALLOC_RET(0, NULL);
    svf_signal_ALLOC_RET(0, NULL);
    svf_tempnam_ALLOC_RET(NULL, NULL);
    svf_tmpvoid_ALLOC_RET();
    svf_xcalloc_ALLOC_RET(0, 0);
    svf_xmalloc_ALLOC_RET(0);
    svf__Znam_ALLOC_RET(0);
    svf__Znaj_ALLOC_RET(0);
    svf__Znwj_ALLOC_RET(0);
    svf___cxa_allocate_exception_ALLOC_RET(0);
    svf_aligned_alloc_ALLOC_RET(0, 0);
    svf_memalign_ALLOC_RET(0, 0);
    svf_valloc_ALLOC_RET(0);
    svf_mmap64_ALLOC_RET(NULL, 0, 0, 0, 0, 0);
    svf_XSetLocaleModifiers_ALLOC_RET(NULL);
    svf___strdup_ALLOC_RET(NULL);
    svf_crypt_ALLOC_RET(NULL, NULL);
    svf_dlerror_ALLOC_RET();
    svf_dlopen_ALLOC_RET(NULL, 0);
    svf_gai_strerror_ALLOC_RET(0);
    svf_gcry_cipher_algo_name_ALLOC_RET(0);
    svf_getenv_ALLOC_RET(NULL);
    svf_getlogin_ALLOC_RET();
    svf_getpass_ALLOC_RET(NULL);
    svf_gnutls_strerror_ALLOC_RET(0);
    svf_gpg_strerror_ALLOC_RET(0);
    svf_gzerror_ALLOC_RET(NULL, NULL);
    svf_inet_ntoa_ALLOC_RET(0);
    svf_initscr_ALLOC_RET();
    svf_llvm_stacksave_ALLOC_RET();
    svf_mmap_ALLOC_RET(NULL, 0, 0, 0, 0, 0);
    svf_newwin_ALLOC_RET(0, 0, 0, 0);
    svf_nl_langinfo_ALLOC_RET(0);
    svf_opendir_ALLOC_RET(NULL);
    svf_sbrk_ALLOC_RET(0);
    svf_strdup_ALLOC_RET(NULL);
    svf_strerror_ALLOC_RET(0);
    svf_strsignal_ALLOC_RET(0);
    svf_textdomain_ALLOC_RET(NULL);
    svf_tgetstr_ALLOC_RET(NULL, NULL);
    svf_tigetstr_ALLOC_RET(NULL);
    svf_tmpnam_ALLOC_RET(NULL);
    svf_ttyname_ALLOC_RET(0);
    svf_malloc_ALLOC_RET(0);
    svf_getcwd_REALLOC_RET(NULL, 0);
    svf_mem_realloc_REALLOC_RET(NULL, 0);
    svf_realloc_REALLOC_RET(NULL, 0);
    svf_safe_realloc_REALLOC_RET(NULL, 0);
    svf_saferealloc_REALLOC_RET(NULL, 0, 0);
    svf_safexrealloc_REALLOC_RET();
    svf_strtok_REALLOC_RET(NULL, NULL);
    svf_strtok_r_REALLOC_RET(NULL, NULL, NULL);
    svf_xrealloc_REALLOC_RET(NULL, 0);
    svf__Znwm_ALLOC_RET(0);
    svf_SyGetmem_ALLOC_RET_OVERWRITE(0);
    svf__ZNSt5arrayIPK1ALm2EE4backEv_OVERWRITE(NULL);
}