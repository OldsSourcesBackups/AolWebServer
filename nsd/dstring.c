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
 * dstring.c --
 *
 *	Ns_DString routines.  Ns_DString's are now compatible 
 *	with Tcl_DString's.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"


/*
 *----------------------------------------------------------------------
 *
 * Ns_DStringVarAppend --
 *
 *	Append a variable number of string arguments to a dstring.
 *
 * Results:
 *	Pointer to current dstring value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Ns_DStringVarAppend(Ns_DString *dsPtr, ...)
{
    register char   *s;
    va_list         ap;

    va_start(ap, dsPtr);
    while ((s = va_arg(ap, char *)) != NULL) {
	Ns_DStringAppend(dsPtr, s);
    }
    va_end(ap);
    return dsPtr->string;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_DStringExport --
 *
 *	Return a copy of the string value on the heap.
 *	Ns_DString is left in an initialized state.
 *
 * Results:
 *	Pointer to ns_malloc'ed string which must be eventually freed.
 *
 * Side effects:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */

char *
Ns_DStringExport(Ns_DString *dsPtr)
{
    char *s;

    if (dsPtr->string != dsPtr->staticSpace) {
	s = dsPtr->string;
	dsPtr->string = dsPtr->staticSpace;
    } else {
	s = ns_malloc(dsPtr->length+1);
	memcpy(s, dsPtr->string, dsPtr->length+1);  
    }
    Ns_DStringFree(dsPtr);
    return s;
}


/*
 *----------------------------------------------------------------------
 * Ns_DStringAppendArg --
 *
 *      Append a string including its terminating null byte.
 *
 * Results:
 *	Pointer to the current string value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
Ns_DStringAppendArg(Ns_DString *dsPtr, char *string)
{
    return Ns_DStringNAppend(dsPtr, string, strlen(string) + 1);
}


/*
 *----------------------------------------------------------------------
 * Ns_DStringPrintf --
 *
 *      Append a sequence of values using a format string
 *
 * Results:
 *	Pointer to the current string value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
Ns_DStringPrintf(Ns_DString *dsPtr, char *fmt,...)
{
    char            buf[NS_DSTRING_PRINTF_MAX+1];
    va_list         ap;

    va_start(ap, fmt);
#if NO_VSNPRINTF
    /* NB: Technically unsafe. */
    vsprintf(buf, fmt, ap);
#else
    vsnprintf(buf, sizeof(buf)-1, fmt, ap);
#endif
    va_end(ap);
    return Ns_DStringAppend(dsPtr, buf);
}


/*
 *----------------------------------------------------------------------
 * Ns_DStringPop --
 *
 *	Pop a DString from the per-thread cache, allocating one
 *  	if necessary.
 *
 * Results:
 *	Pointer to initialized DString.
 *
 * Side effects:
 *      Will initialize TLS and config parameters on first use.
 *
 *----------------------------------------------------------------------
 */

Ns_DString *
Ns_DStringPop(void)
{
    NsTls *tlsPtr = NsGetTls();
    Ns_DString *dsPtr;

    if (tlsPtr->dstring.firstPtr == NULL) {
	dsPtr = ns_malloc(sizeof(Ns_DString));
	Ns_DStringInit(dsPtr);
    } else {
	dsPtr = tlsPtr->dstring.firstPtr;
	tlsPtr->dstring.firstPtr = *((Ns_DString **) dsPtr->staticSpace);
    	dsPtr->staticSpace[0] = 0;
	--tlsPtr->dstring.ncached;
    }
    return dsPtr;
}


/*
 *----------------------------------------------------------------------
 * Ns_DStringPush --
 *
 *	Push a DString back on the per-thread cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *      DString may be free'ed if the max entries and/or size has been
 *  	exceeded.
 *
 *----------------------------------------------------------------------
 */

void
Ns_DStringPush(Ns_DString *dsPtr)
{
    NsTls *tlsPtr = NsGetTls();

    if (tlsPtr->dstring.ncached >= nsconf.dstring.maxentries) {
	Ns_DStringFree(dsPtr);
	ns_free(dsPtr);
    } else {
	if (dsPtr->spaceAvl > nsconf.dstring.maxsize) {
	    Ns_DStringFree(dsPtr);
	} else {
    	    Ns_DStringTrunc(dsPtr, 0);
	}
	*((Ns_DString **) dsPtr->staticSpace) = tlsPtr->dstring.firstPtr;
	tlsPtr->dstring.firstPtr = dsPtr;
	++tlsPtr->dstring.ncached;
    }
}


/*
 *----------------------------------------------------------------------
 * Compatibility routines --
 *
 *	Wrappers for old Ns_DString functions.
 *	memory.
 *
 * Results:
 *      See Tcl_DString routine.
 *
 * Side effects:
 *      See Tcl_DString routine.
 *
 *----------------------------------------------------------------------
 */

#undef Ns_DStringInit

void
Ns_DStringInit(Ns_DString *dsPtr)
{
    Tcl_DStringInit(dsPtr);
}

#undef Ns_DStringFree

void
Ns_DStringFree(Ns_DString *dsPtr)
{
    Tcl_DStringFree(dsPtr);
}

#undef Ns_DStringSetLength

void
Ns_DStringSetLength(Ns_DString *dsPtr, int length)
{
    Tcl_DStringSetLength(dsPtr, length);
}

#undef Ns_DStringTrunc

void
Ns_DStringTrunc(Ns_DString *dsPtr, int length)
{
    Tcl_DStringTrunc(dsPtr, length);
}

#undef Ns_DStringNAppend

char *
Ns_DStringNAppend(Ns_DString *dsPtr, char *string, int length)
{
    return Tcl_DStringAppend(dsPtr, string, length);
}

#undef Ns_DStringAppend

char *
Ns_DStringAppend(Ns_DString *dsPtr, char *string)
{
    return Tcl_DStringAppend(dsPtr, string, -1);
}

#undef Ns_DStringAppendElement

char *
Ns_DStringAppendElement(Ns_DString *dsPtr, char *string)
{
    return Tcl_DStringAppendElement(dsPtr, string);
}

#undef Ns_DStringLength

int
Ns_DStringLength(Ns_DString *dsPtr)
{
    return dsPtr->length;
}

#undef Ns_DStringValue

char *
Ns_DStringValue(Ns_DString *dsPtr)
{
    return dsPtr->string;
}
