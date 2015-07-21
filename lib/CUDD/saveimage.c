/* LINTLIBRARY */


/*
 * saveimage.c --
 *
 * Function to save an executable copy of the current process's
 * image in a file.
 *
 */

#include <stdio.h>
#include "CUDD/util.h"

#ifdef BSD
#include <sys/types.h>
#include <sys/stat.h>
#include <a.out.h>
#include <errno.h>

extern int errno;

#define	BUFSIZE		8192

extern long lseek();	/* For lint */
extern int getpagesize();
extern char *sbrk();

static int copy_file();
static int pad_file();


int
util_save_image(char const *orig_file_name, char const *save_file_name)
{
    int origFd = -1, saveFd = -1;
    char *start_data, *end_data, *start_text, *end_round;
    struct exec old_hdr, new_hdr;
    struct stat old_stat;
    int n, page_size, length_text, length_data;

    if ((origFd = open(orig_file_name, 0)) < 0) {
	perror(orig_file_name);
	(void) fprintf(stderr, "Cannot open original a.out file\n");
	goto bad;
    }

    if (fstat(origFd, &old_stat) < 0) {
	perror(orig_file_name);
	(void) fprintf(stderr, "Cannot stat original a.out file\n");
	goto bad;
    }

    /*
     * Read the a.out header from the original file.
     */
    if (read(origFd, (char *) &old_hdr, sizeof(old_hdr)) != sizeof(old_hdr)) {
	perror(orig_file_name);
	(void) fprintf(stderr, "Cannot read original a.out header\n");
	goto bad;
    }
    if (N_BADMAG(old_hdr)) {
	(void) fprintf(stderr, "File %s has a bad magic number (%o)\n",
			orig_file_name, old_hdr.a_magic);
	goto bad;
    }
    if (old_hdr.a_magic != ZMAGIC) {
	(void) fprintf(stderr, "File %s is not demand-paged\n", orig_file_name);
	goto bad;
    }

    /*
     * Open the output file.
     */
    if (access(save_file_name, /* F_OK */ 0) == 0) {
	(void) unlink(save_file_name);
    }
    if ((saveFd = creat(save_file_name, 0777)) < 0) {
	if (errno == ETXTBSY) {
	    (void) unlink(save_file_name);
	    saveFd = creat(save_file_name, 0777);
	}
	if (saveFd < 0) {
	    perror(save_file_name);
	    (void) fprintf(stderr, "Cannot create save file.\n");
	    goto bad;
	}
    }

    /*
     * Find out how far the data segment extends.
     */
    new_hdr = old_hdr;
    end_data = sbrk(0);
    page_size = getpagesize();
    n = ((((int) end_data) + page_size - 1) / page_size) * page_size;
    end_round = (char *) n;
    if (end_round > end_data) {
	end_data = sbrk(end_round - end_data);
    }

#ifdef vax
    start_text = 0;
    length_text = new_hdr.a_text;
    start_data = (char *) old_hdr.a_text;
    length_data = end_data - start_data;
#endif vax
#ifdef	sun
    start_text = (char *) N_TXTADDR(old_hdr) + sizeof(old_hdr);
    length_text = old_hdr.a_text - sizeof(old_hdr);
    start_data = (char *) N_DATADDR(old_hdr);
    length_data = end_data - start_data;
#endif	sun
    new_hdr.a_data = end_data - start_data;
    new_hdr.a_bss = 0;

    /*
     * First, the header plus enough pad to extend up to N_TXTOFF.
     */
    if (write(saveFd, (char *) &new_hdr, (int) sizeof(new_hdr)) !=
				sizeof(new_hdr)) {
	perror("write");
	(void) fprintf(stderr, "Error while copying header.\n");
	goto bad;
    }
    if (! pad_file(saveFd, N_TXTOFF(old_hdr) - sizeof(new_hdr))) {
	(void) fprintf(stderr, "Error while padding.\n");
	goto bad;
    }


    /*
     *  Copy our text segment
     */
    if (write(saveFd, start_text, length_text) != length_text) {
	perror("write");
	(void) fprintf(stderr, "Error while copying text segment.\n");
	goto bad;
    }


    /*
     *  Copy our data segment
     */
    if (write(saveFd, start_data, length_data) != length_data) {
	perror("write");
	(void) fprintf(stderr, "Error while copying data segment.\n");
	goto bad;
    }

    /*
     * Copy the symbol table and everything else.
     * This takes us to the end of the original file.
     */
    (void) lseek(origFd, (long) N_SYMOFF(old_hdr), 0);
    if (! copy_file(origFd, saveFd, old_stat.st_size - N_SYMOFF(old_hdr))) {
	(void) fprintf(stderr, "Error while copying symbol table.\n");
	goto bad;
    }
    (void) close(origFd);
    (void) close(saveFd);
    return 1;

bad:
    if (origFd >= 0) (void) close(origFd);
    if (saveFd >= 0) (void) close(saveFd);
    return 0;
}


static int
copy_file(inFd, outFd, nbytes)
int inFd, outFd;
unsigned long nbytes;
{
    char buf[BUFSIZE];
    int nread, ntoread;

    while (nbytes > 0) {
	ntoread = nbytes;
	if (ntoread > sizeof buf) ntoread = sizeof buf;
	if ((nread = read(inFd, buf, ntoread)) != ntoread) {
	    perror("read");
	    return (0);
	}
	if (write(outFd, buf, nread) != nread) {
	    perror("write");
	    return (0);
	}
	nbytes -= nread;
    }

    return (1);
}


static int
pad_file(outFd, nbytes)
int outFd;
int nbytes;
{
    char buf[BUFSIZE];
    int nzero;

    nzero = (nbytes > sizeof(buf)) ? sizeof(buf) : nbytes;
    bzero(buf, nzero);
    while (nbytes > 0) {
	nzero = (nbytes > sizeof(buf)) ? sizeof(buf) : nbytes;
	if (write(outFd, buf, nzero) != nzero) {
	    perror("write");
	    return (0);
	}
	nbytes -= nzero;
    }

    return (1);
}
#else

/* ARGSUSED */
int
util_save_image(char const *orig_file_name, char const *save_file_name)
{
    (void) fprintf(stderr, 
	"util_save_image: not implemented on your operating system\n");
    return 0;
}

#endif
