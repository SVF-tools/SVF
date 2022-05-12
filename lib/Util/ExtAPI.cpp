/*  [ExtAPI.cpp] The actual database of external functions
 *  v. 005, 2008-08-08
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
*/

#include "Util/ExtAPI.h"
#include <stdio.h>
#include <dirent.h>
#include <assert.h>
#include <stdlib.h>
<<<<<<< HEAD
#include <unistd.h> //for chdir
=======
#include <unistd.h>
>>>>>>> 9900fdf02845305b4bbe28d50f5f459d2194576a


using namespace std;
using namespace SVF;

ExtAPI* ExtAPI::extAPI = nullptr;

namespace
{

struct ei_pair
{
    const char *n;
    ExtAPI::extf_t t;
};

} // End anonymous namespace

//Each (name, type) pair will be inserted into the map.
//All entries of the same type must occur together (for error detection).
static const ei_pair ei_pairs[]=
{
    //The current llvm-gcc puts in the \01.
    {"\01creat64", ExtAPI::EFT_NOOP},
    {"\01fseeko64", ExtAPI::EFT_NOOP},
    {"\01fstat64", ExtAPI::EFT_NOOP},
    {"\01fstatvfs64", ExtAPI::EFT_NOOP},
    {"\01ftello64", ExtAPI::EFT_NOOP},
    {"\01getrlimit64", ExtAPI::EFT_NOOP},
    {"\01lstat64", ExtAPI::EFT_NOOP},
    {"\01open64", ExtAPI::EFT_NOOP},
    {"\01stat64", ExtAPI::EFT_NOOP},
    {"\01statvfs64", ExtAPI::EFT_NOOP},
    {"Gpm_GetEvent", ExtAPI::EFT_NOOP},
    {"Gpm_Open", ExtAPI::EFT_NOOP},
    {"RAND_seed", ExtAPI::EFT_NOOP},
    {"SSL_CTX_set_default_verify_paths", ExtAPI::EFT_NOOP},
    {"SSL_get_error", ExtAPI::EFT_NOOP},
    {"SSL_get_fd", ExtAPI::EFT_NOOP},
    {"SSL_pending", ExtAPI::EFT_NOOP},
    {"SSL_read", ExtAPI::EFT_NOOP},
    {"SSL_set_bio", ExtAPI::EFT_NOOP},
    {"SSL_set_connect_state", ExtAPI::EFT_NOOP},
    {"SSL_shutdown", ExtAPI::EFT_NOOP},
    {"SSL_state", ExtAPI::EFT_NOOP},
    {"SSL_write", ExtAPI::EFT_NOOP},
    {"Void_FreeCore", ExtAPI::EFT_NOOP},
    {"X509_STORE_CTX_get_error", ExtAPI::EFT_NOOP},
    {"XAllocColor", ExtAPI::EFT_NOOP},
    {"XCloseDisplay", ExtAPI::EFT_NOOP},
    {"XCopyArea", ExtAPI::EFT_NOOP},
    {"XCreateColormap", ExtAPI::EFT_NOOP},
    {"XCreatePixmap", ExtAPI::EFT_NOOP},
    {"XCreateWindow", ExtAPI::EFT_NOOP},
    {"XDrawPoint", ExtAPI::EFT_NOOP},
    {"XDrawString", ExtAPI::EFT_NOOP},
    {"XDrawText", ExtAPI::EFT_NOOP},
    {"XFillRectangle", ExtAPI::EFT_NOOP},
    {"XFillRectangles", ExtAPI::EFT_NOOP},
    {"XGetGCValues", ExtAPI::EFT_NOOP},
    {"XGetGeometry", ExtAPI::EFT_NOOP},
    {"XInternAtom", ExtAPI::EFT_NOOP},
    {"XMapWindow", ExtAPI::EFT_NOOP},
    {"XNextEvent", ExtAPI::EFT_NOOP},
    {"XPutImage", ExtAPI::EFT_NOOP},
    {"XQueryColor", ExtAPI::EFT_NOOP},
    {"XResizeWindow", ExtAPI::EFT_NOOP},
    {"XSelectInput", ExtAPI::EFT_NOOP},
    {"XSendEvent", ExtAPI::EFT_NOOP},
    {"XSetBackground", ExtAPI::EFT_NOOP},
    {"XSetClipMask", ExtAPI::EFT_NOOP},
    {"XSetClipOrigin", ExtAPI::EFT_NOOP},
    {"XSetFillStyle", ExtAPI::EFT_NOOP},
    {"XSetFont", ExtAPI::EFT_NOOP},
    {"XSetForeground", ExtAPI::EFT_NOOP},
    {"XSetFunction", ExtAPI::EFT_NOOP},
    {"XSetGraphicsExposures", ExtAPI::EFT_NOOP},
    {"XSetLineAttributes", ExtAPI::EFT_NOOP},
    {"XSetTile", ExtAPI::EFT_NOOP},
    {"XSetWMHints", ExtAPI::EFT_NOOP},
    {"XSetWMNormalHints", ExtAPI::EFT_NOOP},
    {"XSetWindowBackgroundPixmap", ExtAPI::EFT_NOOP},
    {"XStoreName", ExtAPI::EFT_NOOP},
    {"XSync", ExtAPI::EFT_NOOP},
    {"XVisualIDFromVisual", ExtAPI::EFT_NOOP},
    {"XWMGeometry", ExtAPI::EFT_NOOP},
    {"XtAppSetFallbackResources", ExtAPI::EFT_NOOP},
    {"XtCloseDisplay", ExtAPI::EFT_NOOP},
    {"XtDestroyApplicationContext", ExtAPI::EFT_NOOP},
    {"XtDestroyWidget", ExtAPI::EFT_NOOP},
    {"_IO_getc", ExtAPI::EFT_NOOP},
    {"_IO_putc", ExtAPI::EFT_NOOP},
    {"__assert_fail", ExtAPI::EFT_NOOP},
    {"__dn_expand", ExtAPI::EFT_NOOP},
    {"__dn_skipname", ExtAPI::EFT_NOOP},
    {"__res_nclose", ExtAPI::EFT_NOOP},
    {"__res_ninit", ExtAPI::EFT_NOOP},
    {"__res_nmkquery", ExtAPI::EFT_NOOP},
    {"__res_nsend", ExtAPI::EFT_NOOP},
    {"__res_query", ExtAPI::EFT_NOOP},
    {"__res_querydomain", ExtAPI::EFT_NOOP},
    {"__res_search", ExtAPI::EFT_NOOP},
    {"__sigsetjmp", ExtAPI::EFT_NOOP},
    {"_obstack_begin", ExtAPI::EFT_NOOP},
    {"_obstack_memory_used", ExtAPI::EFT_NOOP},
    {"_obstack_newchunk", ExtAPI::EFT_NOOP},
    {"_setjmp", ExtAPI::EFT_NOOP},
    {"accept", ExtAPI::EFT_NOOP},
    {"access", ExtAPI::EFT_NOOP},
    {"asprintf", ExtAPI::EFT_NOOP},
    {"atexit", ExtAPI::EFT_NOOP},
    {"atof", ExtAPI::EFT_NOOP},
    {"atoi", ExtAPI::EFT_NOOP},
    {"atol", ExtAPI::EFT_NOOP},
    {"bind", ExtAPI::EFT_NOOP},
    {"cfgetospeed", ExtAPI::EFT_NOOP},
    {"cfsetispeed", ExtAPI::EFT_NOOP},
    {"cfsetospeed", ExtAPI::EFT_NOOP},
    {"chdir", ExtAPI::EFT_NOOP},
    {"chmod", ExtAPI::EFT_NOOP},
    {"chown", ExtAPI::EFT_NOOP},
    {"chroot", ExtAPI::EFT_NOOP},
    {"clearerr", ExtAPI::EFT_NOOP},
    {"clearok", ExtAPI::EFT_NOOP},
    {"closedir", ExtAPI::EFT_NOOP},
    {"compress2", ExtAPI::EFT_NOOP},
    {"confstr", ExtAPI::EFT_NOOP},
    {"connect", ExtAPI::EFT_NOOP},
    {"crc32", ExtAPI::EFT_NOOP},
    {"creat", ExtAPI::EFT_NOOP},
    {"creat64", ExtAPI::EFT_NOOP},
    {"deflate", ExtAPI::EFT_NOOP},
    {"deflateEnd", ExtAPI::EFT_NOOP},
    {"deflateInit2_", ExtAPI::EFT_NOOP},
    {"deflateReset", ExtAPI::EFT_NOOP},
    {"delwin", ExtAPI::EFT_NOOP},
    {"dladdr", ExtAPI::EFT_NOOP},
    {"dlclose", ExtAPI::EFT_NOOP},
    {"execl", ExtAPI::EFT_NOOP},
    {"execle", ExtAPI::EFT_NOOP},
    {"execlp", ExtAPI::EFT_NOOP},
    {"execv", ExtAPI::EFT_NOOP},
    {"execve", ExtAPI::EFT_NOOP},
    {"execvp", ExtAPI::EFT_NOOP},
    {"feof", ExtAPI::EFT_NOOP},
    {"ferror", ExtAPI::EFT_NOOP},
    {"fflush", ExtAPI::EFT_NOOP},
    {"fgetc", ExtAPI::EFT_NOOP},
    {"fgetpos", ExtAPI::EFT_NOOP},
    {"fileno", ExtAPI::EFT_NOOP},
    {"flockfile", ExtAPI::EFT_NOOP},
    {"fnmatch", ExtAPI::EFT_NOOP},
    {"forkpty", ExtAPI::EFT_NOOP},
    {"fprintf", ExtAPI::EFT_NOOP},
    {"fputc", ExtAPI::EFT_NOOP},
    {"fputs", ExtAPI::EFT_NOOP},
    {"fread", ExtAPI::EFT_NOOP},
    {"frexp", ExtAPI::EFT_NOOP},
    {"fscanf", ExtAPI::EFT_NOOP},
    {"fseek", ExtAPI::EFT_NOOP},
    {"fseeko", ExtAPI::EFT_NOOP},
    {"fseeko64", ExtAPI::EFT_NOOP},
    {"fsetpos", ExtAPI::EFT_NOOP},
    {"fstat", ExtAPI::EFT_NOOP},
    {"fstat64", ExtAPI::EFT_NOOP},
    {"fstatfs", ExtAPI::EFT_NOOP},
    {"fstatvfs64", ExtAPI::EFT_NOOP},
    {"ftell", ExtAPI::EFT_NOOP},
    {"ftello", ExtAPI::EFT_NOOP},
    {"ftello64", ExtAPI::EFT_NOOP},
    {"ftok", ExtAPI::EFT_NOOP},
    {"funlockfile", ExtAPI::EFT_NOOP},
    {"fwrite", ExtAPI::EFT_NOOP},
    {"g_scanner_destroy", ExtAPI::EFT_NOOP},
    {"g_scanner_eof", ExtAPI::EFT_NOOP},
    {"g_scanner_get_next_token", ExtAPI::EFT_NOOP},
    {"g_scanner_input_file", ExtAPI::EFT_NOOP},
    {"g_scanner_scope_add_symbol", ExtAPI::EFT_NOOP},
    {"gcry_cipher_close", ExtAPI::EFT_NOOP},
    {"gcry_cipher_ctl", ExtAPI::EFT_NOOP},
    {"gcry_cipher_decrypt", ExtAPI::EFT_NOOP},
    {"gcry_cipher_map_name", ExtAPI::EFT_NOOP},
    {"gcry_cipher_open", ExtAPI::EFT_NOOP},
    {"gcry_md_close", ExtAPI::EFT_NOOP},
    {"gcry_md_ctl", ExtAPI::EFT_NOOP},
    {"gcry_md_get_algo", ExtAPI::EFT_NOOP},
    {"gcry_md_hash_buffer", ExtAPI::EFT_NOOP},
    {"gcry_md_map_name", ExtAPI::EFT_NOOP},
    {"gcry_md_open", ExtAPI::EFT_NOOP},
    {"gcry_md_setkey", ExtAPI::EFT_NOOP},
    {"gcry_md_write", ExtAPI::EFT_NOOP},
    {"gcry_mpi_add", ExtAPI::EFT_NOOP},
    {"gcry_mpi_add_ui", ExtAPI::EFT_NOOP},
    {"gcry_mpi_clear_highbit", ExtAPI::EFT_NOOP},
    {"gcry_mpi_print", ExtAPI::EFT_NOOP},
    {"getaddrinfo", ExtAPI::EFT_NOOP},
    {"getc_unlocked", ExtAPI::EFT_NOOP},
    {"getgroups", ExtAPI::EFT_NOOP},
    {"gethostname", ExtAPI::EFT_NOOP},
    {"getloadavg", ExtAPI::EFT_NOOP},
    {"getopt", ExtAPI::EFT_NOOP},
    {"getopt_long", ExtAPI::EFT_NOOP},
    {"getopt_long_only", ExtAPI::EFT_NOOP},
    {"getpeername", ExtAPI::EFT_NOOP},
    {"getresgid", ExtAPI::EFT_NOOP},
    {"getresuid", ExtAPI::EFT_NOOP},
    {"getrlimit", ExtAPI::EFT_NOOP},
    {"getrlimit64", ExtAPI::EFT_NOOP},
    {"getrusage", ExtAPI::EFT_NOOP},
    {"getsockname", ExtAPI::EFT_NOOP},
    {"getsockopt", ExtAPI::EFT_NOOP},
    {"gettimeofday", ExtAPI::EFT_NOOP},
    {"gnutls_pkcs12_bag_decrypt", ExtAPI::EFT_NOOP},
    {"gnutls_pkcs12_bag_deinit", ExtAPI::EFT_NOOP},
    {"gnutls_pkcs12_bag_get_count", ExtAPI::EFT_NOOP},
    {"gnutls_pkcs12_bag_get_type", ExtAPI::EFT_NOOP},
    {"gnutls_x509_crt_deinit", ExtAPI::EFT_NOOP},
    {"gnutls_x509_crt_get_dn_by_oid", ExtAPI::EFT_NOOP},
    {"gnutls_x509_crt_get_key_id", ExtAPI::EFT_NOOP},
    {"gnutls_x509_privkey_deinit", ExtAPI::EFT_NOOP},
    {"gnutls_x509_privkey_get_key_id", ExtAPI::EFT_NOOP},
    {"gzclose", ExtAPI::EFT_NOOP},
    {"gzeof", ExtAPI::EFT_NOOP},
    {"gzgetc", ExtAPI::EFT_NOOP},
    {"gzread", ExtAPI::EFT_NOOP},
    {"gzseek", ExtAPI::EFT_NOOP},
    {"gztell", ExtAPI::EFT_NOOP},
    {"gzwrite", ExtAPI::EFT_NOOP},
    {"hstrerror", ExtAPI::EFT_NOOP},
    {"iconv_close", ExtAPI::EFT_NOOP},
    {"inet_addr", ExtAPI::EFT_NOOP},
    {"inet_aton", ExtAPI::EFT_NOOP},
    {"inet_pton", ExtAPI::EFT_NOOP},
    {"inflate", ExtAPI::EFT_NOOP},
    {"inflateEnd", ExtAPI::EFT_NOOP},
    {"inflateInit2_", ExtAPI::EFT_NOOP},
    {"inflateInit_", ExtAPI::EFT_NOOP},
    {"inflateReset", ExtAPI::EFT_NOOP},
    {"initgroups", ExtAPI::EFT_NOOP},
    {"jpeg_CreateCompress", ExtAPI::EFT_NOOP},
    {"jpeg_CreateDecompress", ExtAPI::EFT_NOOP},
    {"jpeg_destroy", ExtAPI::EFT_NOOP},
    {"jpeg_finish_compress", ExtAPI::EFT_NOOP},
    {"jpeg_finish_decompress", ExtAPI::EFT_NOOP},
    {"jpeg_read_header", ExtAPI::EFT_NOOP},
    {"jpeg_read_scanlines", ExtAPI::EFT_NOOP},
    {"jpeg_resync_to_restart", ExtAPI::EFT_NOOP},
    {"jpeg_set_colorspace", ExtAPI::EFT_NOOP},
    {"jpeg_set_defaults", ExtAPI::EFT_NOOP},
    {"jpeg_set_linear_quality", ExtAPI::EFT_NOOP},
    {"jpeg_set_quality", ExtAPI::EFT_NOOP},
    {"jpeg_start_compress", ExtAPI::EFT_NOOP},
    {"jpeg_start_decompress", ExtAPI::EFT_NOOP},
    {"jpeg_write_scanlines", ExtAPI::EFT_NOOP},
    {"keypad", ExtAPI::EFT_NOOP},
    {"lchown", ExtAPI::EFT_NOOP},
    {"link", ExtAPI::EFT_NOOP},
    {"llvm.dbg", ExtAPI::EFT_NOOP},
    {"llvm.stackrestore", ExtAPI::EFT_NOOP},
    {"llvm.va_copy", ExtAPI::EFT_NOOP},
    {"llvm.va_end", ExtAPI::EFT_NOOP},
    {"llvm.va_start", ExtAPI::EFT_NOOP},
    {"longjmp", ExtAPI::EFT_NOOP},
    {"lstat", ExtAPI::EFT_NOOP},
    {"lstat64", ExtAPI::EFT_NOOP},
    {"mblen", ExtAPI::EFT_NOOP},
    {"mbrlen", ExtAPI::EFT_NOOP},
    {"mbrtowc", ExtAPI::EFT_NOOP},
    {"mbtowc", ExtAPI::EFT_NOOP},
    {"memcmp", ExtAPI::EFT_NOOP},
    {"mkdir", ExtAPI::EFT_NOOP},
    {"mknod", ExtAPI::EFT_NOOP},
    {"mkfifo", ExtAPI::EFT_NOOP},
    {"mkstemp", ExtAPI::EFT_NOOP},
    {"mkstemp64", ExtAPI::EFT_NOOP},
    {"mktime", ExtAPI::EFT_NOOP},
    {"modf", ExtAPI::EFT_NOOP},
    {"mprotect", ExtAPI::EFT_NOOP},
    {"munmap", ExtAPI::EFT_NOOP},
    {"nanosleep", ExtAPI::EFT_NOOP},
    {"nodelay", ExtAPI::EFT_NOOP},
    {"open", ExtAPI::EFT_NOOP},
    {"open64", ExtAPI::EFT_NOOP},
    {"openlog", ExtAPI::EFT_NOOP},
    {"openpty", ExtAPI::EFT_NOOP},
    {"pathconf", ExtAPI::EFT_NOOP},
    {"pclose", ExtAPI::EFT_NOOP},
    {"perror", ExtAPI::EFT_NOOP},
    {"pipe", ExtAPI::EFT_NOOP},
    {"png_destroy_write_struct", ExtAPI::EFT_NOOP},
    {"png_init_io", ExtAPI::EFT_NOOP},
    {"png_set_bKGD", ExtAPI::EFT_NOOP},
    {"png_set_invert_alpha", ExtAPI::EFT_NOOP},
    {"png_set_invert_mono", ExtAPI::EFT_NOOP},
    {"png_write_end", ExtAPI::EFT_NOOP},
    {"png_write_info", ExtAPI::EFT_NOOP},
    {"png_write_rows", ExtAPI::EFT_NOOP},
    {"poll", ExtAPI::EFT_NOOP},
    {"pread64", ExtAPI::EFT_NOOP},
    {"printf", ExtAPI::EFT_NOOP},
    {"pthread_attr_destroy", ExtAPI::EFT_NOOP},
    {"pthread_attr_init", ExtAPI::EFT_NOOP},
    {"pthread_attr_setscope", ExtAPI::EFT_NOOP},
    {"pthread_attr_setstacksize", ExtAPI::EFT_NOOP},
    {"pthread_create", ExtAPI::EFT_NOOP},
    {"pthread_mutex_destroy", ExtAPI::EFT_NOOP},
    {"pthread_mutex_init", ExtAPI::EFT_NOOP},
    {"pthread_mutex_lock", ExtAPI::EFT_NOOP},
    {"pthread_mutex_unlock", ExtAPI::EFT_NOOP},
    {"pthread_mutexattr_destroy", ExtAPI::EFT_NOOP},
    {"pthread_mutexattr_init", ExtAPI::EFT_NOOP},
    {"pthread_mutexattr_settype", ExtAPI::EFT_NOOP},
    {"ptsname", ExtAPI::EFT_NOOP},
    {"putenv", ExtAPI::EFT_NOOP},
    {"puts", ExtAPI::EFT_NOOP},
    {"qsort", ExtAPI::EFT_NOOP},
    {"re_compile_fastmap", ExtAPI::EFT_NOOP},
    {"re_exec", ExtAPI::EFT_NOOP},
    {"re_search", ExtAPI::EFT_NOOP},
    {"read", ExtAPI::EFT_NOOP},
    {"readlink", ExtAPI::EFT_NOOP},
    {"recv", ExtAPI::EFT_NOOP},
    {"recvfrom", ExtAPI::EFT_NOOP},
    {"regcomp", ExtAPI::EFT_NOOP},
    {"regerror", ExtAPI::EFT_NOOP},
    {"remove", ExtAPI::EFT_NOOP},
    {"rename", ExtAPI::EFT_NOOP},
    {"rewind", ExtAPI::EFT_NOOP},
    {"rewinddir", ExtAPI::EFT_NOOP},
    {"rmdir", ExtAPI::EFT_NOOP},
    {"rresvport", ExtAPI::EFT_NOOP},
    {"scrollok", ExtAPI::EFT_NOOP},
    {"select", ExtAPI::EFT_NOOP},
    {"sem_destroy", ExtAPI::EFT_NOOP},
    {"sem_init", ExtAPI::EFT_NOOP},
    {"sem_post", ExtAPI::EFT_NOOP},
    {"sem_trywait", ExtAPI::EFT_NOOP},
    {"sem_wait", ExtAPI::EFT_NOOP},
    {"send", ExtAPI::EFT_NOOP},
    {"sendto", ExtAPI::EFT_NOOP},
    {"setbuf", ExtAPI::EFT_NOOP},
    {"setenv", ExtAPI::EFT_NOOP},
    {"setgroups", ExtAPI::EFT_NOOP},
    {"setitimer", ExtAPI::EFT_NOOP},
    {"setrlimit", ExtAPI::EFT_NOOP},
    {"setsockopt", ExtAPI::EFT_NOOP},
    {"setvbuf", ExtAPI::EFT_NOOP},
    {"sigaction", ExtAPI::EFT_NOOP},
    {"sigaddset", ExtAPI::EFT_NOOP},
    {"sigaltstack", ExtAPI::EFT_NOOP},
    {"sigdelset", ExtAPI::EFT_NOOP},
    {"sigemptyset", ExtAPI::EFT_NOOP},
    {"sigfillset", ExtAPI::EFT_NOOP},
    {"sigisemptyset", ExtAPI::EFT_NOOP},
    {"sigismember", ExtAPI::EFT_NOOP},
    {"siglongjmp", ExtAPI::EFT_NOOP},
    {"sigprocmask", ExtAPI::EFT_NOOP},
    {"sigsuspend", ExtAPI::EFT_NOOP},
    {"snprintf", ExtAPI::EFT_NOOP},
    {"socketpair", ExtAPI::EFT_NOOP},
    {"sprintf", ExtAPI::EFT_NOOP},
    {"sscanf", ExtAPI::EFT_NOOP},
    {"stat", ExtAPI::EFT_NOOP},
    {"stat64", ExtAPI::EFT_NOOP},
    {"statfs", ExtAPI::EFT_NOOP},
    {"statvfs", ExtAPI::EFT_NOOP},
    {"statvfs64", ExtAPI::EFT_NOOP},
    {"strcasecmp", ExtAPI::EFT_NOOP},
    {"strcmp", ExtAPI::EFT_NOOP},
    {"strcoll", ExtAPI::EFT_NOOP},
    {"strcspn", ExtAPI::EFT_NOOP},
    {"strfmon", ExtAPI::EFT_NOOP},
    {"strftime", ExtAPI::EFT_NOOP},
    {"strlen", ExtAPI::EFT_NOOP},
    {"strncasecmp", ExtAPI::EFT_NOOP},
    {"strncmp", ExtAPI::EFT_NOOP},
    {"strspn", ExtAPI::EFT_NOOP},
    {"symlink", ExtAPI::EFT_NOOP},
    {"sysinfo", ExtAPI::EFT_NOOP},
    {"syslog", ExtAPI::EFT_NOOP},
    {"system", ExtAPI::EFT_NOOP},
    {"tcgetattr", ExtAPI::EFT_NOOP},
    {"tcsetattr", ExtAPI::EFT_NOOP},
    {"tgetent", ExtAPI::EFT_NOOP},
    {"tgetflag", ExtAPI::EFT_NOOP},
    {"tgetnum", ExtAPI::EFT_NOOP},
    {"time", ExtAPI::EFT_NOOP},
    {"timegm", ExtAPI::EFT_NOOP},
    {"times", ExtAPI::EFT_NOOP},
    {"tputs", ExtAPI::EFT_NOOP},
    {"truncate", ExtAPI::EFT_NOOP},
    {"uname", ExtAPI::EFT_NOOP},
    {"uncompress", ExtAPI::EFT_NOOP},
    {"ungetc", ExtAPI::EFT_NOOP},
    {"unlink", ExtAPI::EFT_NOOP},
    {"unsetenv", ExtAPI::EFT_NOOP},
    {"utime", ExtAPI::EFT_NOOP},
    {"utimes", ExtAPI::EFT_NOOP},
    {"vasprintf", ExtAPI::EFT_NOOP},
    {"vfprintf", ExtAPI::EFT_NOOP},
    {"vprintf", ExtAPI::EFT_NOOP},
    {"vsnprintf", ExtAPI::EFT_NOOP},
    {"vsprintf", ExtAPI::EFT_NOOP},
    {"waddch", ExtAPI::EFT_NOOP},
    {"waddnstr", ExtAPI::EFT_NOOP},
    {"wait", ExtAPI::EFT_NOOP},
    {"wait3", ExtAPI::EFT_NOOP},
    {"wait4", ExtAPI::EFT_NOOP},
    {"waitpid", ExtAPI::EFT_NOOP},
    {"wattr_off", ExtAPI::EFT_NOOP},
    {"wattr_on", ExtAPI::EFT_NOOP},
    {"wborder", ExtAPI::EFT_NOOP},
    {"wclrtobot", ExtAPI::EFT_NOOP},
    {"wclrtoeol", ExtAPI::EFT_NOOP},
    {"wcrtomb", ExtAPI::EFT_NOOP},
    {"wctomb", ExtAPI::EFT_NOOP},
    {"wctype", ExtAPI::EFT_NOOP},
    {"werase", ExtAPI::EFT_NOOP},
    {"wgetch", ExtAPI::EFT_NOOP},
    {"wmove", ExtAPI::EFT_NOOP},
    {"wrefresh", ExtAPI::EFT_NOOP},
    {"write", ExtAPI::EFT_NOOP},
    {"wtouchln", ExtAPI::EFT_NOOP},

    {"\01_fopen", ExtAPI::EFT_ALLOC},
    {"\01fopen64", ExtAPI::EFT_ALLOC},
    {"\01readdir64", ExtAPI::EFT_ALLOC},
    {"\01tmpfile64", ExtAPI::EFT_ALLOC},
    {"BIO_new_socket", ExtAPI::EFT_ALLOC},
    {"FT_Get_Sfnt_Table", ExtAPI::EFT_ALLOC},
    {"FcFontList", ExtAPI::EFT_ALLOC},
    {"FcFontMatch", ExtAPI::EFT_ALLOC},
    {"FcFontRenderPrepare", ExtAPI::EFT_ALLOC},
    {"FcFontSetCreate", ExtAPI::EFT_ALLOC},
    {"FcFontSort", ExtAPI::EFT_ALLOC},
    {"FcInitLoadConfig", ExtAPI::EFT_ALLOC},
    {"FcObjectSetBuild", ExtAPI::EFT_ALLOC},
    {"FcObjectSetCreate", ExtAPI::EFT_ALLOC},
    {"FcPatternBuild", ExtAPI::EFT_ALLOC},
    {"FcPatternCreate", ExtAPI::EFT_ALLOC},
    {"FcPatternDuplicate", ExtAPI::EFT_ALLOC},
    {"SSL_CTX_new", ExtAPI::EFT_ALLOC},
    {"SSL_get_peer_certificate", ExtAPI::EFT_ALLOC},
    {"SSL_new", ExtAPI::EFT_ALLOC},
    {"SSLv23_client_method", ExtAPI::EFT_ALLOC},
    {"SyGetmem", ExtAPI::EFT_ALLOC},
    {"TLSv1_client_method", ExtAPI::EFT_ALLOC},
    {"XAddExtension", ExtAPI::EFT_ALLOC},
    {"XAllocClassHint", ExtAPI::EFT_ALLOC},
    {"XAllocSizeHints", ExtAPI::EFT_ALLOC},
    {"XAllocStandardColormap", ExtAPI::EFT_ALLOC},
    {"XCreateFontSet", ExtAPI::EFT_ALLOC},
    {"XCreateImage", ExtAPI::EFT_ALLOC},
    {"XCreateGC", ExtAPI::EFT_ALLOC},
    //Returns the prev. defined handler.
    {"XESetCloseDisplay", ExtAPI::EFT_ALLOC},
    {"XGetImage", ExtAPI::EFT_ALLOC},
    {"XGetModifierMapping", ExtAPI::EFT_ALLOC},
    {"XGetMotionEvents", ExtAPI::EFT_ALLOC},
    {"XGetVisualInfo", ExtAPI::EFT_ALLOC},
    {"XLoadQueryFont", ExtAPI::EFT_ALLOC},
    {"XListPixmapFormats", ExtAPI::EFT_ALLOC},
    {"XRenderFindFormat", ExtAPI::EFT_ALLOC},
    {"XRenderFindStandardFormat", ExtAPI::EFT_ALLOC},
    {"XRenderFindVisualFormat", ExtAPI::EFT_ALLOC},
    {"XOpenDisplay", ExtAPI::EFT_ALLOC},
    //These 2 return the previous handler.
    {"XSetErrorHandler", ExtAPI::EFT_ALLOC},
    {"XSetIOErrorHandler", ExtAPI::EFT_ALLOC},
    {"XShapeGetRectangles", ExtAPI::EFT_ALLOC},
    {"XShmCreateImage", ExtAPI::EFT_ALLOC},
    //This returns the handler last passed to XSetAfterFunction().
    {"XSynchronize", ExtAPI::EFT_ALLOC},
    {"XcursorImageCreate", ExtAPI::EFT_ALLOC},
    {"XcursorLibraryLoadImages", ExtAPI::EFT_ALLOC},
    {"XcursorShapeLoadImages", ExtAPI::EFT_ALLOC},
    {"XineramaQueryScreens", ExtAPI::EFT_ALLOC},
    {"XkbGetMap", ExtAPI::EFT_ALLOC},
    {"XtAppCreateShell", ExtAPI::EFT_ALLOC},
    {"XtCreateApplicationContext", ExtAPI::EFT_ALLOC},
    {"XtOpenDisplay", ExtAPI::EFT_ALLOC},
    {"alloc", ExtAPI::EFT_ALLOC},
    {"alloc_check", ExtAPI::EFT_ALLOC},
    {"alloc_clear", ExtAPI::EFT_ALLOC},
    {"art_svp_from_vpath", ExtAPI::EFT_ALLOC},
    {"art_svp_vpath_stroke", ExtAPI::EFT_ALLOC},
    {"art_svp_writer_rewind_new", ExtAPI::EFT_ALLOC},
    //FIXME: returns arg0->svp
    {"art_svp_writer_rewind_reap", ExtAPI::EFT_ALLOC},
    {"art_vpath_dash", ExtAPI::EFT_ALLOC},
    {"cairo_create", ExtAPI::EFT_ALLOC},
    {"cairo_image_surface_create_for_data", ExtAPI::EFT_ALLOC},
    {"cairo_pattern_create_for_surface", ExtAPI::EFT_ALLOC},
    {"cairo_surface_create_similar", ExtAPI::EFT_ALLOC},
    {"calloc", ExtAPI::EFT_ALLOC},
    {"zmalloc", ExtAPI::EFT_ALLOC},
    {"fopen", ExtAPI::EFT_ALLOC},
    {"fopen64", ExtAPI::EFT_ALLOC},
    {"fopencookie", ExtAPI::EFT_ALLOC},
    {"g_scanner_new", ExtAPI::EFT_ALLOC},
    {"gcry_sexp_nth_mpi", ExtAPI::EFT_ALLOC},
    {"gzdopen", ExtAPI::EFT_ALLOC},
    {"iconv_open", ExtAPI::EFT_ALLOC},
    {"jpeg_alloc_huff_table", ExtAPI::EFT_ALLOC},
    {"jpeg_alloc_quant_table", ExtAPI::EFT_ALLOC},
    {"lalloc", ExtAPI::EFT_ALLOC},
    {"lalloc_clear", ExtAPI::EFT_ALLOC},
    {"malloc", ExtAPI::EFT_ALLOC},
    {"nhalloc", ExtAPI::EFT_ALLOC},
    {"oballoc", ExtAPI::EFT_ALLOC},
    {"pango_cairo_font_map_create_context", ExtAPI::EFT_ALLOC},
    //This may also point *arg2 to a new string.
    {"pcre_compile", ExtAPI::EFT_ALLOC},
    {"pcre_study", ExtAPI::EFT_ALLOC},
    {"permalloc", ExtAPI::EFT_ALLOC},
    {"png_create_info_struct", ExtAPI::EFT_ALLOC},
    {"png_create_write_struct", ExtAPI::EFT_ALLOC},
    {"popen", ExtAPI::EFT_ALLOC},
    {"pthread_getspecific", ExtAPI::EFT_ALLOC},
    {"readdir", ExtAPI::EFT_ALLOC},
    {"readdir64", ExtAPI::EFT_ALLOC},
    {"safe_calloc", ExtAPI::EFT_ALLOC},
    {"safe_malloc", ExtAPI::EFT_ALLOC},
    {"safecalloc", ExtAPI::EFT_ALLOC},
    {"safemalloc", ExtAPI::EFT_ALLOC},
    {"safexcalloc", ExtAPI::EFT_ALLOC},
    {"safexmalloc", ExtAPI::EFT_ALLOC},
    {"savealloc", ExtAPI::EFT_ALLOC},
    {"setmntent", ExtAPI::EFT_ALLOC},
    {"shmat", ExtAPI::EFT_ALLOC},
    //These 2 return the previous handler.
    {"__sysv_signal", ExtAPI::EFT_ALLOC},
    {"signal", ExtAPI::EFT_ALLOC},
    {"sigset", ExtAPI::EFT_ALLOC},
    {"tempnam", ExtAPI::EFT_ALLOC},
    {"tmpfile", ExtAPI::EFT_ALLOC},
    {"tmpfile64", ExtAPI::EFT_ALLOC},
    {"xalloc", ExtAPI::EFT_ALLOC},
    {"xcalloc", ExtAPI::EFT_ALLOC},
    {"xmalloc", ExtAPI::EFT_ALLOC},
    //C++ functions
    {"_Znwm", ExtAPI::EFT_ALLOC},	// new
    {"_Znam", ExtAPI::EFT_ALLOC},	// new []
    {"_Znaj", ExtAPI::EFT_ALLOC},	// new
    {"_Znwj", ExtAPI::EFT_ALLOC},	// new []
    {"__cxa_allocate_exception", ExtAPI::EFT_ALLOC},	// allocate an exception
    {"aligned_alloc", ExtAPI::EFT_ALLOC},
    {"memalign", ExtAPI::EFT_ALLOC},
    {"valloc", ExtAPI::EFT_ALLOC},
    {"SRE_LockCreate", ExtAPI::EFT_ALLOC},
    {"VOS_MemAlloc", ExtAPI::EFT_ALLOC},

    {"\01mmap64", ExtAPI::EFT_NOSTRUCT_ALLOC},
    //FIXME: this is like realloc but with arg1.
    {"X509_NAME_oneline", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"X509_verify_cert_error_string", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XBaseFontNameListOfFontSet", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XGetAtomName", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XGetDefault", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XGetKeyboardMapping", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XListDepths", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XListFonts", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XSetLocaleModifiers", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"XcursorGetTheme", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"__strdup", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"crypt", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"ctime", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"dlerror", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"dlopen", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gai_strerror", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gcry_cipher_algo_name", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gcry_md_algo_name", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gcry_md_read", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"getenv", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"getlogin", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"getpass", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gnutls_strerror", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gpg_strerror", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"gzerror", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"inet_ntoa", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"initscr", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"llvm.stacksave", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"mmap", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"mmap64", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"newwin", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"nl_langinfo", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"opendir", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"sbrk", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"strdup", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"strerror", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"strsignal", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"textdomain", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"tgetstr", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"tigetstr", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"tmpnam", ExtAPI::EFT_NOSTRUCT_ALLOC},
    {"ttyname", ExtAPI::EFT_NOSTRUCT_ALLOC},

    {"__ctype_b_loc", ExtAPI::EFT_STAT2},
    {"__ctype_tolower_loc", ExtAPI::EFT_STAT2},
    {"__ctype_toupper_loc", ExtAPI::EFT_STAT2},

    {"XKeysymToString", ExtAPI::EFT_STAT},
    {"__errno_location", ExtAPI::EFT_STAT},
    {"__h_errno_location", ExtAPI::EFT_STAT},
    {"__res_state", ExtAPI::EFT_STAT},
    {"asctime", ExtAPI::EFT_STAT},
    {"bindtextdomain", ExtAPI::EFT_STAT},
    {"bind_textdomain_codeset", ExtAPI::EFT_STAT},
    //This is L_A0 when arg0 is not null.
    {"ctermid", ExtAPI::EFT_STAT},
    {"dcgettext", ExtAPI::EFT_STAT},
    {"dgettext", ExtAPI::EFT_STAT},
    {"dngettext", ExtAPI::EFT_STAT},
    {"fdopen", ExtAPI::EFT_STAT},
    {"gcry_strerror", ExtAPI::EFT_STAT},
    {"gcry_strsource", ExtAPI::EFT_STAT},
    {"getgrgid", ExtAPI::EFT_STAT},
    {"getgrnam", ExtAPI::EFT_STAT},
    {"gethostbyaddr", ExtAPI::EFT_STAT},
    {"gethostbyname", ExtAPI::EFT_STAT},
    {"gethostbyname2", ExtAPI::EFT_STAT},
    {"getmntent", ExtAPI::EFT_STAT},
    {"getprotobyname", ExtAPI::EFT_STAT},
    {"getprotobynumber", ExtAPI::EFT_STAT},
    {"getpwent", ExtAPI::EFT_STAT},
    {"getpwnam", ExtAPI::EFT_STAT},
    {"getpwuid", ExtAPI::EFT_STAT},
    {"getservbyname", ExtAPI::EFT_STAT},
    {"getservbyport", ExtAPI::EFT_STAT},
    {"getspnam", ExtAPI::EFT_STAT},
    {"gettext", ExtAPI::EFT_STAT},
    {"gmtime", ExtAPI::EFT_STAT},
    {"gnu_get_libc_version", ExtAPI::EFT_STAT},
    {"gnutls_check_version", ExtAPI::EFT_STAT},
    {"localeconv", ExtAPI::EFT_STAT},
    {"localtime", ExtAPI::EFT_STAT},
    {"ngettext", ExtAPI::EFT_STAT},
    {"pango_cairo_font_map_get_default", ExtAPI::EFT_STAT},
    {"re_comp", ExtAPI::EFT_STAT},
    {"setlocale", ExtAPI::EFT_STAT},
    {"tgoto", ExtAPI::EFT_STAT},
    {"tparm", ExtAPI::EFT_STAT},
    {"zError", ExtAPI::EFT_STAT},

    {"getcwd", ExtAPI::EFT_REALLOC},
    {"mem_realloc", ExtAPI::EFT_REALLOC},
    {"realloc", ExtAPI::EFT_REALLOC},
    {"realloc_obj", ExtAPI::EFT_REALLOC},
    {"safe_realloc", ExtAPI::EFT_REALLOC},
    {"saferealloc", ExtAPI::EFT_REALLOC},
    {"safexrealloc", ExtAPI::EFT_REALLOC},
    //FIXME: when arg0 is null, the return points into the string that was
    //  last passed in arg0 (rather than a new string, as for realloc).
    {"strtok", ExtAPI::EFT_REALLOC},
    //As above, but also stores the last string into *arg2.
    {"strtok_r", ExtAPI::EFT_REALLOC},
    {"xrealloc", ExtAPI::EFT_REALLOC},

    {"SSL_CTX_free", ExtAPI::EFT_FREE},
    {"SSL_free", ExtAPI::EFT_FREE},
    {"cfree", ExtAPI::EFT_FREE},
    {"free", ExtAPI::EFT_FREE},
    {"free_all_mem", ExtAPI::EFT_FREE},
    {"freeaddrinfo", ExtAPI::EFT_FREE},
    {"gcry_mpi_release", ExtAPI::EFT_FREE},
    {"gcry_sexp_release", ExtAPI::EFT_FREE},
    {"globfree", ExtAPI::EFT_FREE},
    {"nhfree", ExtAPI::EFT_FREE},
    {"obstack_free", ExtAPI::EFT_FREE},
    {"safe_cfree", ExtAPI::EFT_FREE},
    {"safe_free", ExtAPI::EFT_FREE},
    {"safefree", ExtAPI::EFT_FREE},
    {"safexfree", ExtAPI::EFT_FREE},
    {"sm_free", ExtAPI::EFT_FREE},
    {"vim_free", ExtAPI::EFT_FREE},
    {"xfree", ExtAPI::EFT_FREE},
    {"fclose", ExtAPI::EFT_FREE},
    {"XFreeColormap", ExtAPI::EFT_FREE},
    {"XFreeColors", ExtAPI::EFT_FREE},
    {"XFreeFont", ExtAPI::EFT_FREE},
    {"XFreeFontNames", ExtAPI::EFT_FREE},
    {"XFreeGC", ExtAPI::EFT_FREE},
    {"XFreePixmap", ExtAPI::EFT_FREE},
    {"XFree", ExtAPI::EFT_FREE},
    {"VOS_MemFree", ExtAPI::EFT_FREE},
    //C++ functions
    {"_ZdaPv", ExtAPI::EFT_FREE},	// delete
    {"_ZdlPv", ExtAPI::EFT_FREE},	// delete []

    {"__rawmemchr", ExtAPI::EFT_L_A0},
    {"cairo_surface_reference", ExtAPI::EFT_L_A0},
    {"fgets", ExtAPI::EFT_L_A0},
    {"jpeg_std_error", ExtAPI::EFT_L_A0},
    {"memchr", ExtAPI::EFT_L_A0},
    //This may return a new ptr if the region was moved.
    {"mremap", ExtAPI::EFT_L_A0},
    {"strchr", ExtAPI::EFT_L_A0},
    {"strerror_r", ExtAPI::EFT_L_A0},
    {"strpbrk", ExtAPI::EFT_L_A0},
    {"strptime", ExtAPI::EFT_L_A0},
    {"strrchr", ExtAPI::EFT_L_A0},
    {"strstr", ExtAPI::EFT_L_A0},
    {"tmpnam_r", ExtAPI::EFT_L_A0},
    //cctype
    {"isalnum", ExtAPI::EFT_L_A0},
    {"isalpha", ExtAPI::EFT_L_A0},
    {"isblank", ExtAPI::EFT_L_A0},
    {"iscntrl", ExtAPI::EFT_L_A0},
    {"isdigit", ExtAPI::EFT_L_A0},
    {"isgraph", ExtAPI::EFT_L_A0},
    {"islower", ExtAPI::EFT_L_A0},
    {"isprint", ExtAPI::EFT_L_A0},
    {"ispunct", ExtAPI::EFT_L_A0},
    {"isspace", ExtAPI::EFT_L_A0},
    {"isupper", ExtAPI::EFT_L_A0},
    {"isxdigit", ExtAPI::EFT_L_A0},
    {"asctime_r", ExtAPI::EFT_L_A1},
    {"bsearch", ExtAPI::EFT_L_A1},
    {"getmntent_r", ExtAPI::EFT_L_A1},
    {"gmtime_r", ExtAPI::EFT_L_A1},
    {"gzgets", ExtAPI::EFT_L_A1},
    {"localtime_r", ExtAPI::EFT_L_A1},
    {"realpath", ExtAPI::EFT_L_A1},
    {"\01freopen64", ExtAPI::EFT_L_A2},
    //FIXME: may do L_A3 if arg5 > 0.
    {"_XGetAsyncReply", ExtAPI::EFT_L_A2},
    {"freopen", ExtAPI::EFT_L_A2},
    {"freopen64", ExtAPI::EFT_L_A2},
    {"inet_ntop", ExtAPI::EFT_L_A2},
    {"XGetSubImage", ExtAPI::EFT_L_A8},

    {"memset", ExtAPI::EFT_L_A0__A0R_A1},
    {"llvm.memset", ExtAPI::EFT_L_A0__A0R_A1},
    {"llvm.memset.p0i8.i32", ExtAPI::EFT_L_A0__A0R_A1},
    {"llvm.memset.p0i8.i64", ExtAPI::EFT_L_A0__A0R_A1},
    {"__memset_chk", ExtAPI::EFT_L_A0__A0R_A1},
    {"llvm.memcpy", ExtAPI::EFT_L_A0__A0R_A1R},
    {"llvm.memcpy.p0i8.p0i8.i32", ExtAPI::EFT_L_A0__A0R_A1R},
    {"llvm.memcpy.p0i8.p0i8.i64", ExtAPI::EFT_L_A0__A0R_A1R},
    {"llvm.memmove", ExtAPI::EFT_L_A0__A0R_A1R},
    {"llvm.memmove.p0i8.p0i8.i32", ExtAPI::EFT_L_A0__A0R_A1R},
    {"llvm.memmove.p0i8.p0i8.i64", ExtAPI::EFT_L_A0__A0R_A1R},
    {"memccpy", ExtAPI::EFT_L_A0__A0R_A1R},
    {"memcpy", ExtAPI::EFT_L_A0__A0R_A1R},
    {"memmove", ExtAPI::EFT_L_A0__A0R_A1R},
    {"dlsym", ExtAPI::EFT_L_A1__FunPtr},
    {"bcopy", ExtAPI::EFT_A1R_A0R},
    {"iconv", ExtAPI::EFT_A3R_A1R_NS},
    {"strtod", ExtAPI::EFT_A1R_A0},
    {"strtof", ExtAPI::EFT_A1R_A0},
    {"strtol", ExtAPI::EFT_A1R_A0},
    {"strtold", ExtAPI::EFT_A1R_A0},
    {"strtoll", ExtAPI::EFT_A1R_A0},
    {"strtoul", ExtAPI::EFT_A1R_A0},
    {"readdir_r", ExtAPI::EFT_A2R_A1},

    {"__strcpy_chk", ExtAPI::EFT_L_A0__A1_A0},
    {"__strcat_chk", ExtAPI::EFT_L_A0__A1_A0},
    {"stpcpy", ExtAPI::EFT_L_A0__A1_A0},
    {"strcat", ExtAPI::EFT_L_A0__A1_A0},
    {"strcpy", ExtAPI::EFT_L_A0__A1_A0},
    {"strncat", ExtAPI::EFT_L_A0__A1_A0},
    {"strncpy", ExtAPI::EFT_L_A0__A1_A0},

    //These also set arg1->pw_name etc. to new strings.
    {"getpwnam_r", ExtAPI::EFT_A4R_A1},
    {"getpwuid_r", ExtAPI::EFT_A4R_A1},

    {"db_create", ExtAPI::EFT_A0R_NEW},
    {"gcry_mpi_scan", ExtAPI::EFT_A0R_NEW},
    {"gcry_pk_decrypt", ExtAPI::EFT_A0R_NEW},
    {"gcry_sexp_build", ExtAPI::EFT_A0R_NEW},
    {"gnutls_pkcs12_bag_init", ExtAPI::EFT_A0R_NEW},
    {"gnutls_pkcs12_init", ExtAPI::EFT_A0R_NEW},
    {"gnutls_x509_crt_init", ExtAPI::EFT_A0R_NEW},
    {"gnutls_x509_privkey_init", ExtAPI::EFT_A0R_NEW},
    {"posix_memalign", ExtAPI::EFT_A0R_NEW},
    {"scandir", ExtAPI::EFT_A1R_NEW},
    {"XGetRGBColormaps", ExtAPI::EFT_A2R_NEW},
    {"XmbTextPropertyToTextList", ExtAPI::EFT_A2R_NEW},
    {"SRE_SplSpecCreate", ExtAPI::EFT_A2R_NEW},
    {"XQueryTree", ExtAPI::EFT_A4R_NEW},
    {"XGetWindowProperty", ExtAPI::EFT_A11R_NEW},

    // C++ STL functions
    // std::_Rb_tree_insert_and_rebalance(bool, std::_Rb_tree_node_base*, std::_Rb_tree_node_base*, std::_Rb_tree_node_base&)
    {"_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_", ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE},

    // std::_Rb_tree_increment   and   std::_Rb_tree_decrement
    // TODO: the following side effects seem not to be necessary
//    {"_ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base", ExtAPI::EFT_STD_RB_TREE_INCREMENT},
//    {"_ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base", ExtAPI::EFT_STD_RB_TREE_INCREMENT},

    {"_ZNSt8__detail15_List_node_base7_M_hookEPS0_", ExtAPI::EFT_STD_LIST_HOOK},


    /// string constructor: string (const char *s)
    {"_ZNSsC1EPKcRKSaIcE", ExtAPI::CPP_EFT_A0R_A1}, // c++98
    {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1EPKcRKS3_", ExtAPI::CPP_EFT_A0R_A1}, // c++11

    /// string constructor: string (const char *s, size_t n)
    {"_ZNSsC1EPKcmRKSaIcE", ExtAPI::CPP_EFT_A0R_A1}, // c++98
    {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1EPKcmRKS3_", ExtAPI::CPP_EFT_A0R_A1}, // c++11

    /// string operator=: operator= (const char *s)
    {"_ZNSsaSEPKc", ExtAPI::CPP_EFT_A0R_A1}, // c++98
    {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEaSEPKc", ExtAPI::CPP_EFT_A0R_A1}, // c++11

    /// string constructor: string (const string &str)
    {"_ZNSsC1ERKSs", ExtAPI::CPP_EFT_A0R_A1R}, // c++98
    {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1ERKS4_", ExtAPI::CPP_EFT_A0R_A1R}, // c++11

    /// string constructor: string (const string &str, size_t pos, size_t len)
    {"_ZNSsC1ERKSsmm", ExtAPI::CPP_EFT_A0R_A1R}, // c++98
    {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1ERKS4_mm", ExtAPI::CPP_EFT_A0R_A1R}, // c++11

    /// string operator=: operator= (const string &str)
    {"_ZNSsaSERKSs", ExtAPI::CPP_EFT_A0R_A1R}, // c++98
    {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEaSERKS4_", ExtAPI::CPP_EFT_A0R_A1R}, // c++11

    /// std::operator<<: operator<< (const string &str)
    {"_ZStlsIcSt11char_traitsIcESaIcEERSt13basic_ostreamIT_T0_ES7_RKSbIS4_S5_T1_E", ExtAPI::CPP_EFT_A1R}, // c++98
    {"_ZStlsIcSt11char_traitsIcESaIcEERSt13basic_ostreamIT_T0_ES7_RKNSt7__cxx1112basic_stringIS4_S5_T1_EE", ExtAPI::CPP_EFT_A1R}, // c++11

    //This must be the last entry.
    {"__dynamic_cast", ExtAPI::CPP_EFT_DYNAMIC_CAST},
    {0, ExtAPI::EFT_NOOP}
};

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

extern char **environ;
void ExtAPI::init()
{
    set<extf_t> t_seen;
    extf_t prev_t= EFT_NOOP;
    t_seen.insert(EFT_NOOP);
    const char* env = std::getenv("SVF_DIR");
    string env_str(env);
    assert(env != nullptr && "SVF_DIR is not set");
    cout << "env is set as " << env << endl;
    if (chdir(env) != 0){
        perror("chdir() to /usr failed");
    } 
    
    // DIR *pdir = nullptr;
    // if((pdir  = opendir(env_str.c_str())) == NULL) {
    //     cout << "Error(" << errno << ") opening " << pdir << endl;
    // }
    // string env_str(env);
    // string command = "ls ";
    // command.append(env_str);
    // std::system(command.c_str());
    
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
        if(info.count(p->n))
        {
            fputs(p->n, stderr);
            putc('\n', stderr);
            assert(!"duplicate name in ei_pairs");
        }
        info[p->n]= p->t;
    }
}