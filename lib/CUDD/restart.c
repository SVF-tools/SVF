#include <stdio.h>
#include "CUDD/util.h"

#if (defined(sun) && ! defined(sparc)) || defined(vax)

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

static char *save_stack_base;
static char *stack_lo_addr;
static char *stack_hi_addr;
static int stack_size;

static int restart_global_flag;
static char *old_file_name;
static char *new_file_name;

char *util_save_sp;		/* set by util_restart_save_state() */
extern char *sbrk();

static void
grow_stack() 
{
    int i, space[256];

    for(i = 0; i < 256; i++) {
	space[i] = 0;
    }
    if ((char *) &i > stack_lo_addr) {
	grow_stack();
    }
}


/* ARGSUSED */
static int
handle_sigquit(int sig, int code, struct sigcontext *scp)
{
    if (util_restart_save_state()) {
	/* we are restarting ! -- return from signal */

    } else {
	/* copy stack to user data space */
	stack_lo_addr = util_save_sp;
	stack_size = stack_hi_addr - stack_lo_addr + 1;
	save_stack_base = sbrk(stack_size);
	(void) memcpy(save_stack_base, stack_lo_addr, stack_size);

	/* write a new executable */
	(void) fprintf(stderr, "Writing executable %s ...\n", new_file_name);
	(void) util_save_image(old_file_name, new_file_name);

	/* terminate if signal was a QUIT */
	if (sig == SIGQUIT) {
	    (void) _exit(1);
	}
    }
}


static void
restart_program()
{
    (void) fprintf(stderr, "Continuing execution ...\n");

    /* create the stack */
    grow_stack();

#ifdef vax
    asm("movl _util_save_sp,sp");
#endif
#ifdef sun
    asm("movl _util_save_sp,sp");
#endif

    /* copy the stack back from user space */
    (void) memcpy(stack_lo_addr, save_stack_base, stack_size);

    /* remove the sbrk for the stack */
    if (sbrk(-stack_size) < 0) {
	perror("sbrk");
    }

    util_restart_restore_state();	/* jump back into handle_sigquit() */
}

void
util_restart(char const *old, char const *neW, int interval)
{
    struct itimerval itimer;

#ifdef vax
#ifdef ultrix
    stack_hi_addr = (char *) 0x7fffe3ff;	/* ultrix */
#else
    stack_hi_addr = (char *) 0x7fffebff;	/* bsd 4.3 */
#endif
#endif
#ifdef sun
    stack_hi_addr = (char *) 0x0effffff;	/* Sun OS 3.2, 3.4 */ 
#endif

    old_file_name = old;
    new_file_name = neW;

    (void) signal(SIGQUIT, handle_sigquit);

    if (interval > 0) {
	(void) signal(SIGVTALRM, handle_sigquit);
	itimer.it_interval.tv_sec = interval;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_sec = interval;
	itimer.it_value.tv_usec = 0;
	if (setitimer(ITIMER_VIRTUAL, &itimer, (struct itimerval *) 0) < 0) {
	    perror("setitimer");
	    exit(1);
	}
    }

    if (restart_global_flag) {
	restart_program();
    }
    restart_global_flag = 1;
}

#else

/* ARGSUSED */
void
util_restart(char const *old, char const *neW, int interval)
{
    (void) fprintf(stderr, 
	"util_restart: not supported on your operating system/hardware\n");
}

#endif
