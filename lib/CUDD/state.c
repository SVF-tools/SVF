#ifdef lint
util_restart_save_state()
{
    return 0;
}


util_restart_restore_state()
{
}

#else

static char rcsid[] = "$Id: state.c,v 1.1 1997/11/04 22:38:50 fabio Exp $";

#ifdef vax
int util_restart_state[32];

util_restart_save_state()
{
    asm("movl	sp,_util_save_sp");
    asm("movl	r1,_util_restart_state");
    asm("movl	r2,_util_restart_state+4");
    asm("movl	r3,_util_restart_state+8");
    asm("movl	r4,_util_restart_state+12");
    asm("movl	r5,_util_restart_state+16");
    asm("movl	r6,_util_restart_state+20");
    asm("movl	r7,_util_restart_state+24");
    asm("movl	r8,_util_restart_state+28");
    asm("movl	r9,_util_restart_state+32");
    asm("movl	r10,_util_restart_state+36");
    asm("movl	r11,_util_restart_state+40");
    asm("movl	8(fp),_util_restart_state+44");
    asm("movl	12(fp),_util_restart_state+48");
    asm("movl	16(fp),_util_restart_state+52");
    asm("movl	$0,r0");
}

util_restart_restore_state()
{
    asm("movl	_util_restart_state,r1");
    asm("movl	_util_restart_state+4,r2");
    asm("movl	_util_restart_state+8,r3");
    asm("movl	_util_restart_state+12,r4");
    asm("movl	_util_restart_state+16,r5");
    asm("movl	_util_restart_state+20,r6");
    asm("movl	_util_restart_state+24,r7");
    asm("movl	_util_restart_state+28,r8");
    asm("movl	_util_restart_state+32,r9");
    asm("movl	_util_restart_state+36,r10");
    asm("movl	_util_restart_state+40,r11");
    asm("movl	_util_restart_state+44,ap");
    asm("movl	_util_restart_state+48,fp");
    asm("addl3	fp,$4,sp");
    asm("movl	_util_restart_state+52,r0");
    asm("jmp	(r0)");
}
#endif


#if defined(sun) && ! defined(sparc)
int util_restart_state[32];

util_restart_save_state()
{
    asm("movel	sp,_util_save_sp");
    asm("movel	sp@,_util_restart_state");
    asm("movel	sp@(0x4),_util_restart_state+4");
    asm("moveml	#0xFFFF,_util_restart_state+8");
    return 0;
}

util_restart_restore_state()
{
    asm("moveml	_util_restart_state+8,#0xFFFF");
    asm("movel	_util_restart_state+4,sp@(0x4)");
    asm("movel	_util_restart_state,sp@");
    return 1;
}
#endif

#endif
