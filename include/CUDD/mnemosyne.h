/************************************************************************
 *									*
 *			Copyright (c) 1985 by				*
 *		Digital Equipment Corporation, Maynard, MA		*
 *			All rights reserved.				*
 *									*
 *   The information in this software is subject to change  without	*
 *   notice  and should not be construed as a commitment by Digital	*
 *   Equipment Corporation.						*
 *									*
 *   Digital assumes no responsibility for the use  or  reliability	*
 *   of its software on equipment which is not supplied by Digital.	*
 *									*
 *   Redistribution and use in source and binary forms are permitted	*
 *   provided that the above copyright notice and this paragraph are	*
 *   duplicated in all such forms and that any documentation,		*
 *   advertising materials, and other materials related to such		*
 *   distribution and use acknowledge that the software was developed	*
 *   by Digital Equipment Corporation. The name of Digital Equipment	*
 *   Corporation may not be used to endorse or promote products derived	*
 *   from this software without specific prior written permission.	*
 *   THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR	*
 *   IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED	*
 *   WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.*
 *   Do not take internally. In case of accidental ingestion, contact	*
 *   your physician immediately.					*
 *									*
 ************************************************************************/

#ifndef	_INCL_MNEMOSYNE_H

/*
/fats/tools/hsv/mnemosyne/mnemosyne.h,v 1.1.1.1 1995/06/06 18:18:28 fabio Exp
*/


/*
main include file for the mnemosyne memory allocation tracker. this file
provides some pre-processor fakes for malloc(), realloc() and family,
as well as forward declarations for the mnemosyne functions.

	Marcus J. Ranum, 1990. (mjr@decuac.dec.com)
*/


/* these disguise mnemosyne calls as calls to malloc and family */
#ifndef	NOFAKEMALLOC
#define malloc(siz)		mnem_malloc(siz,__FILE__,__LINE__)
#define calloc(siz,cnt)		mnem_calloc(siz,cnt,__FILE__,__LINE__)
#define realloc(ptr,siz)	mnem_realloc(ptr,siz,__FILE__,__LINE__)
#define free(ptr)		mnem_free(ptr,__FILE__,__LINE__)
#endif


#ifdef	MALLOC_IS_VOIDSTAR
typedef	void	*mall_t;
#else
typedef	char	*mall_t;
#endif

extern	mall_t	mnem_malloc();
extern	mall_t	mnem_calloc();
extern	mall_t	mnem_realloc();
extern	void	mnem_free();

/* some internal functions and oddimentia */
extern	int	mnem_recording();
extern	int	mnem_setrecording();
extern	void	mnem_setlog();
extern	int	mnem_writestats();

#define	_INCL_MNEMOSYNE_H
#endif
