/*
 * The contents of this file are subject to the AOLserver Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://aolserver.com/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is AOLserver Code and related documentation
 * distributed by AOL.
 * 
 * The Initial Developer of the Original Code is America Online,
 * Inc. Portions created by AOL are Copyright (C) 1999 America Online,
 * Inc. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under either the License or the GPL.
 */


/* 
 * reentrant.c --
 *
 *	Reentrant versions of common system utilities using per-thread
 *	data buffers.  See the corresponding manual page for details.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "thread.h"

/*
 * The following structure maintains state for the
 * reentrant wrappers.
 */

typedef struct Tls {
    char	    	naBuf[16];
#ifndef WIN32
    char	       *stBuf;
    struct tm   	gtBuf;
    struct tm   	ltBuf;
    char		ctBuf[27];
    char		asBuf[27];
    struct {
	struct dirent ent;
	char name[PATH_MAX+1];
    } rdBuf;
#endif
} Tls;

static Tls *GetTls(void);

#ifdef MACOSX
#define NO_REENTRANT	1
char *strtok_r(char *s, const char *delim, char **last);
#endif

struct dirent *
ns_readdir(DIR * dir)
{
    struct dirent *ent;

#if defined(WIN32) || defined(__FreeBSD__)
    ent = readdir(dir);

#else
    Tls *tlsPtr = GetTls();

#if defined(NO_REENTRANT)
    static Ns_Mutex lock;

    Ns_MutexLock(&lock);
    ent = readdir(dir);
    if (ent != NULL) {
	memcpy(&tlsPtr->rdBuf.ent, ent,
	       sizeof(*ent) - sizeof(ent->d_name) + ent->d_namlen + 1);
	ent = &tlsPtr->rdBuf.ent;
    }
    Ns_MutexUnlock(&lock);

#elif defined(__hp10)
    ent = &tlsPtr->rdBuf.ent;
    if (readdir_r(dir, ent) != 0) {
        ent = NULL;
    }

#else
    ent = &tlsPtr->rdBuf.ent; 
    if (readdir_r(dir, ent, &ent) != 0) {
	ent = NULL;
    }

#endif
#endif

    return ent;
}


struct tm *
ns_localtime(const time_t * clock)
{
    struct tm *ptm;

#ifdef WIN32
    ptm = localtime(clock);

#else
    Tls *tlsPtr = GetTls();

#if defined(NO_REENTRANT)
    static Ns_Mutex lock;

    Ns_MutexLock(&lock);
    ptm = localtime(clock);
    if (ptm != NULL) {
	tlsPtr->ltBuf = *ptm;
	ptm = &tlsPtr->ltBuf;
    }
    Ns_MutexUnlock(&lock);

#elif defined(__hp10)
    ptm = &tlsPtr->ltBuf;
    if (localtime_r(clock, ptm) != 0) {
	ptm = NULL;
    }

#else
    ptm = localtime_r(clock, &tlsPtr->ltBuf);

#endif
#endif

   return ptm;
}


struct tm *
ns_gmtime(const time_t * clock)
{
    struct tm *ptm;

#ifdef WIN32
    ptm = gmtime(clock);

#else
    Tls *tlsPtr = GetTls();

#if defined(NO_REENTRANT)
    static Ns_Mutex lock;

    Ns_MutexLock(&lock);
    ptm = gmtime(clock);
    if (ptm != NULL) {
	tlsPtr->gtBuf = *ptm;
	ptm = &tlsPtr->gtBuf;
    }
    Ns_MutexUnlock(&lock);

#elif defined(__hp10)
    ptm = &tlsPtr->gtBuf;
    if (gmtime_r(clock, &tlsPtr->gtBuf) != 0) {
	ptm = NULL;
    }

#else
    ptm = gmtime_r(clock, &tlsPtr->gtBuf);

#endif
#endif

    return ptm;
}


char *
ns_ctime(const time_t * clock)
{
    char *ct;

#ifdef WIN32
    ct = ctime(clock);

#else
    Tls *tlsPtr = GetTls();

#if defined(NO_REENTRANT)
    static Ns_Mutex lock;

    Ns_MutexLock(&lock);
    ct = ctime(clock);
    if (ct != NULL) {
	strcpy(tlsPtr->ctBuf, ct);
	ct = tlsPtr->ctBuf;
    }
    Ns_MutexUnlock(&lock);

#elif defined(__hp10)
    ct = tlsPtr->ctBuf;
    if (ctime_r(clock, ct, 27) != 0) {
	ct = NULL;
    }

#else
    ct = ctime_r(clock, tlsPtr->ctBuf);

#endif
#endif

    return ct;
}


char *
ns_asctime(const struct tm *tmPtr)
{
    char *at;

#ifdef WIN32
    at = asctime(tmPtr);

#else
    Tls *tlsPtr = GetTls();

#if defined(NO_REENTRANT)
    static Ns_Mutex lock;

    Ns_MutexLock(&lock);
    at = asctime(tmPtr);
    if (at != NULL) {
	strcpy(tlsPtr->asBuf, at);
	at = tlsPtr->asBuf;
    }
    Ns_MutexUnlock(&lock);

#elif defined(__hp10)
    at = tlsPtr->asBuf;
    if (asctime_r(tmPtr, at, 27) != 0) {
	at = NULL;
    }

#else
    at = asctime_r(tmPtr, tlsPtr->asBuf);

#endif
#endif
    return at;
}


char *
ns_strtok(char *s1, const char *s2)
{
#ifdef WIN32
    return strtok(s1, s2);
#else
    Tls *tlsPtr = GetTls();

    return strtok_r(s1, s2, &tlsPtr->stBuf);
#endif
}


char *
ns_inet_ntoa(struct in_addr addr)
{
    Tls *tlsPtr = GetTls();
    union {
    	unsigned long l;
    	unsigned char b[4];
    } u;
    
    u.l = (unsigned long) addr.s_addr;
    sprintf(tlsPtr->naBuf, "%u.%u.%u.%u", u.b[0], u.b[1], u.b[2], u.b[3]);
    return tlsPtr->naBuf;
}


static Tls *
GetTls(void)
{
    static Ns_Tls tls;
    Tls *tlsPtr;

    if (tls == NULL) {
	Ns_MasterLock();
	if (tls == NULL) {
	    Ns_TlsAlloc(&tls, ns_free);
	}
	Ns_MasterUnlock();
    }
    tlsPtr = Ns_TlsGet(&tls);
    if (tlsPtr == NULL) {
	tlsPtr = ns_malloc(sizeof(Tls));
	Ns_TlsSet(&tls, tlsPtr);
    }
    return tlsPtr;
}



#ifdef MACOSX

/*
 * Copyright (c) 1998 Softweyr LLC.  All rights reserved.
 *
 * strtok_r, from Berkeley strtok
 * Oct 13, 1998 by Wes Peters <wes@softweyr.com>
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Softweyr LLC, the
 *      University of California, Berkeley, and its contributors.
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SOFTWEYR LLC, THE REGENTS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL SOFTWEYR LLC, THE
 * REGENTS, OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <string.h>

char *
strtok_r(char *s, const char *delim, char **last)
{
    char *spanp;
    int c, sc;
    char *tok;

    if (s == NULL && (s = *last) == NULL)
    {
	return NULL;
    }

    /*
     * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
     */
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0; )
    {
	if (c == sc)
	{
	    goto cont;
	}
    }

    if (c == 0)		/* no non-delimiter characters */
    {
	*last = NULL;
	return NULL;
    }
    tok = s - 1;

    /*
     * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
     * Note that delim must have one NUL; we stop if we see that, too.
     */
    for (;;)
    {
	c = *s++;
	spanp = (char *)delim;
	do
	{
	    if ((sc = *spanp++) == c)
	    {
		if (c == 0)
		{
		    s = NULL;
		}
		else
		{
		    char *w = s - 1;
		    *w = '\0';
		}
		*last = s;
		return tok;
	    }
	}
	while (sc != 0);
    }
    /* NOTREACHED */
}

#endif
