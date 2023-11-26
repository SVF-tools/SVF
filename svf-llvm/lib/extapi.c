#define NULL ((void *)0)
#define STATIC_OBJECT malloc(10)

/*
    Functions with __attribute__((annotate("XXX"))) will be handle by SVF specifcially.
    Their logical behaviours can't be described by C/C++ language. For example, "malloc()" functionality is to alloc an object to it's return value.

    The description of methodProperties is as follows:

        ALLOC_RET,    // returns a ptr to a newly allocated object
        ALLOC_ARGi    // stores a pointer to an allocated object in *argi
        REALLOC_RET,  
        MEMSET,       // memcpy() operations
        MEMCPY,       // memset() operations
        OVERWRITE,    // svf function overwrite app function
*/
__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *malloc(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *fopen(const char *voidname, const char *mode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *fopen64(const char *voidname, const char *mode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *fdopen(int fd, const char *mode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
struct dirent64 *readdir64(void *dirp)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *tmpvoid64(void)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0*Arg1")))
void *calloc(unsigned long nitems, unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *zmalloc(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *gzdopen(int fd, const char *mode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *iconv_open(const char *tocode, const char *fromcode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *lalloc(unsigned long size, int a)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *lalloc_clear(unsigned long size, int a)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
long *nhalloc(unsigned int a, const char *b, int c)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *oballoc(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *popen(const char *command, const char *type)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *pthread_getspecific(const char *a, const char *b)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
struct dirent *readdir(void *dirp)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0*Arg1")))
void* safe_calloc(unsigned nelem, unsigned elsize)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void* safe_malloc(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0*Arg1")))
char* safecalloc(int a, int b)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
char* safemalloc(int a, int b)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *setmntent(const char *voidname, const char *type)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *shmat(int shmid, const void *shmaddr, int shmflg)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void* __sysv_signal(int a, void *b)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void (*signal(int sig, void (*func)(int)))(int)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *tempnam(const char *dir, const char *pfx)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *tmpvoid(void)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void* xcalloc(unsigned long size1, unsigned long size2)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void* xmalloc(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *_Znam(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *_Znaj(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *_Znwj(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *__cxa_allocate_exception(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg1")))
void* aligned_alloc(unsigned long size1, unsigned long size2)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg1")))
void* memalign(unsigned long size1, unsigned long size2)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *valloc(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg1")))
void *mmap64(void *addr, unsigned long len, int prot, int flags, int fildes, long off)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *XSetLocaleModifiers(char *a)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char * __strdup(const char * string)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *crypt(const char *key, const char *salt)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *ctime(const void *timer)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *dlerror(void)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *dlopen(const char *voidname, int flags)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
const char *gai_strerror(int errcode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
const char *gcry_cipher_algo_name(int errcode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
const char *svfgcry_md_algo_name_(int errcode)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *getenv(const char *name)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *getlogin(void)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *getpass(const char *prompt)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
const char * gnutls_strerror(int error)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
const char *gpg_strerror(unsigned int a)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
const char * gzerror(void* file, int * errnum)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *inet_ntoa(unsigned int in)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *initscr(void)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void* llvm_stacksave()
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg1")))
void *mmap(void *addr, unsigned long len, int prot, int flags, int fildes, long off)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *newwin(int nlines, int ncols, int begin_y, int begin_x)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *nl_langinfo(int item)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *opendir(const char *name)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void *sbrk(long increment)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *strdup(const char *s)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *strerror(int errnum)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *strsignal(int errnum)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *textdomain(const char * domainname)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *tgetstr(char *id, char **area)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *tigetstr(char *capname)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *tmpnam(char *s)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *ttyname(int fd)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *getcwd(char *buf, unsigned long size)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:Arg1")))
char *mem_realloc(void *ptr, unsigned long size)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:Arg1")))
char *realloc(void *ptr, unsigned long size)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:Arg1")))
void* safe_realloc(void *p, unsigned long n)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:Arg1*Arg2")))
void* saferealloc(void *p, unsigned long n1, unsigned long n2)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:UNKNOWN")))
void* safexrealloc()
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *strtok(char *str, const char *delim)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:UNKNOWN")))
char *strtok_r(char *str, const char *delim, char **saveptr)
{
    return NULL;
}

__attribute__((annotate("REALLOC_RET"), annotate("AllocSize:Arg1")))
void *xrealloc(void *ptr, unsigned long bytes)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *_Znwm(unsigned long size)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *_ZnwmRKSt9nothrow_t(unsigned long size, void *)
{
    return NULL;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
void *_ZnamRKSt9nothrow_t(unsigned long size, void *)
{
    return NULL;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int asprintf(char **restrict strp, const char *restrict fmt, ...)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int vasprintf(char **strp, const char *fmt, void* ap)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int db_create(void **dbp, void *dbenv, unsigned int flags)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int gnutls_pkcs12_bag_init(void *a)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int gnutls_pkcs12_init(void *a)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int gnutls_x509_crt_init(void *a)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:UNKNOWN")))
int gnutls_x509_privkey_init(void *a)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG0"), annotate("AllocSize:Arg2")))
int posix_memalign(void **a, unsigned long b, unsigned long c)
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG1"), annotate("AllocSize:UNKNOWN")))
int scandir(const char *restrict dirp, struct dirent ***restrict namelist, int (*filter)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **))
{
    return 0;
}

__attribute__((annotate("ALLOC_ARG2"), annotate("AllocSize:UNKNOWN")))
int XmbTextPropertyToTextList(void *a, void *b, char ***c, int *d)
{
    return 0;
}

__attribute__((annotate("MEMCPY")))
void llvm_memcpy_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void llvm_memcpy_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void llvm_memcpy(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void llvm_memmove(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void llvm_memmove_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void llvm_memmove_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void __memcpy_chk(char* dst, char* src, int sz, int flag){}

__attribute__((annotate("MEMCPY")))
void *memmove(void *str1, const void *str2, unsigned long n)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
void bcopy(const void *s1, void *s2, unsigned long n){}

__attribute__((annotate("MEMCPY")))
void *memccpy( void * restrict dest, const void * restrict src, int c, unsigned long count)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
void __memmove_chk(char* dst, char* src, int sz){}

__attribute__((annotate("MEMSET")))
void llvm_memset(char* dst, char elem, int sz, int flag){}

__attribute__((annotate("MEMSET")))
void llvm_memset_p0i8_i32(char* dst, char elem, int sz, int flag){}

__attribute__((annotate("MEMSET")))
void llvm_memset_p0i8_i64(char* dst, char elem, int sz, int flag){}

__attribute__((annotate("MEMSET")))
char *__memset_chk(char * dest, int c, unsigned long destlen, int flag)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char * __strcpy_chk(char * dest, const char * src, unsigned long destlen)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char *__strcat_chk(char * dest, const char * src, unsigned long destlen)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char *stpcpy(char *restrict dst, const char *restrict src)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char *strcat(char *dest, const char *src)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char *strcpy(char *dest, const char *src)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char *strncat(char *dest, const char *src, unsigned long n)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
char *strncpy(char *dest, const char *src, unsigned long n)
{
    return NULL;
}

__attribute__((annotate("MEMCPY")))
unsigned long iconv(void* cd, char **restrict inbuf, unsigned long *restrict inbytesleft, char **restrict outbuf, unsigned long *restrict outbytesleft)
{
    return 0;
}

__attribute__((annotate("OVERWRITE")))
void* _ZNSt5arrayIPK1ALm2EE4backEv(void *arg)
{
    void* ptr1 = (char*)arg + 0;
    void* ptr2 = (char*)ptr1 + 0;
    return ptr2;
}

__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
__attribute__((annotate("OVERWRITE")))
void *SyGetmem(unsigned long size)
{
    return NULL;
}

void * __rawmemchr(const void * s, int c)
{
    return (void *)s;
}

struct jpeg_error_mgr *jpeg_std_error(struct jpeg_error_mgr * a)
{
    return a;
}

char *fgets(char *str, int n, void *stream)
{
    return str;
}

void *memchr(const void *str, int c, unsigned long n)
{
    return (void *)str;
}

void * mremap(void * old_address, unsigned long old_size, unsigned long new_size, int flags)
{
    return old_address;
}

char *strchr(const char *str, int c)
{
    return (char *)str;
}

char *strerror_r(int errnum, char *buf, unsigned long buflen)
{
    return buf;
}

char *strpbrk(const char *str1, const char *str2)
{
    return (char *)str1;
}

char *strptime(const void* s, const void* format, void* tm)
{
    return (char *)s;
}

char *strrchr(const char *str, int c)
{
    return (char *)str;
}

char *strstr(const char *haystack, const char *needle)
{
    return (char *)haystack;
}

char *tmpnam_r(char *s)
{
    return s;
}

int isalnum(int character)
{
    return character;
}

int isalpha(int character)
{
    return character;
}

int isblank(int character)
{
    return character;
}

int iscntrl(int c)
{
    return c;
}

int isdigit(int c)
{
    return c;
}

int isgraph(int c)
{
    return c;
}

int islower( int arg )
{
    return arg;
}

int isprint(int c)
{
    return c;
}

int ispunct(int argument)
{
    return argument;
}

int isspace(char c)
{
    return c;
}

int isupper(int c)
{
    return c;
}

int isxdigit(int c)
{
    return c;
}

char *asctime_r(const void *tm, char *buf)
{
    return buf;
}

void *bsearch(const void *key, const void *base, unsigned long nitems, unsigned long size, int (*compar)(const void *, const void *))
{
    return (void *)base;
}

struct mntent *getmntent_r(void *fp, struct mntent *mntbuf, char *buf, int buflen)
{
    return mntbuf;
}

struct tm *gmtime_r(const void *timer, struct tm *buf)
{
    return buf;
}

char * gzgets(void* file, char * buf, int len)
{
    return buf;
}

struct tm *localtime_r(const void *timep, struct tm *result)
{
    return result;
}

char *realpath(const char *restrict path, char *restrict resolved_path)
{
    return resolved_path;
}

void* freopen64( const char* voidname, const char* mode, void* fp )
{
    return fp;
}

void* freopen(const char* voidname, const char* mode, void* fp)
{
    return fp;
}

const char *inet_ntop(int af, const void *restrict src,  char *restrict dst, unsigned int size)
{
    return dst;
}

double strtod(const char *str, char **endptr)
{
    *endptr = (char *)str;
    return 0.0;
}

float strtof(const char *nptr, char **endptr)
{
    *endptr = (char *)nptr;
    return 0.0;
}

long int strtol(const char *str, char **endptr, int base)
{
    *endptr = (char *)str;
    return 0;
}

long double strtold(const char* str, char** endptr)
{
    *endptr = (char *)str;
    return 0.0;
}

unsigned long int strtoul(const char *str, char **endptr, int base)
{
    *endptr = (char *)str;
    return 0;
}

int readdir_r(void *__restrict__dir, void *__restrict__entry, void **__restrict__result)
{
    __restrict__entry = *__restrict__result;   
    return 0;
}

int getpwnam_r(const char *name, void *pwd, char *buf, unsigned long buflen, void **result)
{
    *result = pwd;
    return 0;
}

int getpwuid_r(unsigned int uid, void *pwd, char *buf, unsigned long buflen, void **result)
{
    *result = pwd;
    return 0;
}

void _ZNSt8__detail15_List_node_base7_M_hookEPS0_(void *arg0, void **arg1)
{
    *arg1 = arg0;
}

void* __dynamic_cast(void* source, const void* sourceTypeInfo, const void* targetTypeInfo, unsigned long castType)
{
    return source;
}

void _ZNSsC1EPKcRKSaIcE(void **arg0, void *arg1)
{
    *arg0 = arg1;
}

void _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1EPKcRKS3_(void **arg0, void *arg1)
{
    *arg0 = arg1;
}

const unsigned short **__ctype_b_loc(void)
{
    return STATIC_OBJECT;
}

int **__ctype_tolower_loc(void)
{
    return STATIC_OBJECT;
}

int **__ctype_toupper_loc(void)
{
    return STATIC_OBJECT;
}

int *__errno_location(void)
{
    return STATIC_OBJECT;
}

int * __h_errno_location(void)
{
    return STATIC_OBJECT;
}

void* __res_state(void)
{
    return STATIC_OBJECT;
}

char *asctime(const void *timeptr)
{
    return STATIC_OBJECT;
}

char * bindtextdomain(const char * domainname, const char * dirname)
{
    return STATIC_OBJECT;
}

char * bind_textdomain_codeset(const char * domainname, const char * codeset)
{
    return STATIC_OBJECT;
}

char *ctermid(char *s)
{
    return STATIC_OBJECT;
}

char * dcgettext(const char * domainname, const char * msgid, int category)
{
    return STATIC_OBJECT;
}

char * dgettext(const char * domainname, const char * msgid)
{
    return STATIC_OBJECT;
}

char * dngettext(const char * domainname, const char * msgid, const char * msgid_plural, unsigned long int n)
{
    return STATIC_OBJECT;
}

struct group *getgrgid(unsigned int gid)
{
    return STATIC_OBJECT;
}

struct group *getgrnam(const char *name)
{
    return STATIC_OBJECT;
}

struct hostent *gethostbyaddr(const void *addr, unsigned int len, int type)
{
    return STATIC_OBJECT;
}

struct hostent *gethostbyname(const char *name)
{
    return STATIC_OBJECT;
}

struct hostent *gethostbyname2(const char *name, int af)
{
    return STATIC_OBJECT;
}

struct mntent *getmntent(void *stream)
{
    return STATIC_OBJECT;
}

struct protoent *getprotobyname(const char *name)
{
    return STATIC_OBJECT;
}

struct protoent *getprotobynumber(int proto)
{
    return STATIC_OBJECT;
}

struct passwd *getpwent(void)
{
    return STATIC_OBJECT;
}

struct passwd *getpwnam(const char *name)
{
    return STATIC_OBJECT;
}

struct passwd *getpwuid(unsigned int uid)
{
    return STATIC_OBJECT;
}

struct servent *getservbyname(const char *name, const char *proto)
{
    return STATIC_OBJECT;
}

struct servent *getservbyport(int port, const char *proto)
{
    return STATIC_OBJECT;
}

struct spwd *getspnam(const char *name)
{
    return STATIC_OBJECT;
}

char * gettext(const char * msgid)
{
    return STATIC_OBJECT;
}

struct tm *gmtime(const void *timer)
{
    return STATIC_OBJECT;
}

const char *gnu_get_libc_version(void)
{
    return STATIC_OBJECT;
}

const char * gnutls_check_version(const char * req_version)
{
    return STATIC_OBJECT;
}

struct lconv *localeconv(void)
{
    return STATIC_OBJECT;
}

struct tm *localtime(const void *timer)
{
    return STATIC_OBJECT;
}

char * ngettext(const char * msgid, const char * msgid_plural, unsigned long int n)
{
    return STATIC_OBJECT;
}

void *pango_cairo_font_map_get_default(void)
{
    return STATIC_OBJECT;
}

char *re_comp(const char *regex)
{
    return STATIC_OBJECT;
}

char *setlocale(int category, const char *locale)
{
    return STATIC_OBJECT;
}

char *tgoto(const char *cap, int col, int row)
{
    return STATIC_OBJECT;
}

char *tparm(char *str, ...)
{
    return STATIC_OBJECT;
}

const char *zError(int a)
{
    return STATIC_OBJECT;
}