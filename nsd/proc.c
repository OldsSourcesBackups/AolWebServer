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
 * proc.c --
 *
 *	Support for getting information on procs (thread routines,
 *	callbacks, scheduled procs, etc.).
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

/*
 * The following struct maintains callback and description for
 * Ns_GetProcInfo.
 */

typedef struct Info {
    Ns_ArgProc *proc;
    char *desc;
} Info;

/*
 * The following struct array defines common procs in nsd.
 */

struct proc {
	void *procAddr;
	char *desc;
	Ns_ArgProc *argProc;
} procs[] = {
	{(void *) NsTclThread, "ns:tclthread", NsTclThreadArgProc},
	{(void *) NsTclCallback, "ns:tclcallback", NsTclArgProc},
	{(void *) NsTclSchedProc, "ns:tclschedproc", NsTclArgProc},
	{(void *) NsTclSignalProc, "ns:tclsigproc", NsTclArgProc},
	{(void *) NsTclSockProc, "ns:tclsockcallback", NsTclSockArgProc},
	{(void *) NsCachePurge, "ns:cachepurge", NsCacheArgProc},
	{(void *) NsConnThread, "ns:connthread", NsConnArgProc},
	{NULL, NULL, NULL}
};

/*
 * Static functions defined in this file.
 */

static void AppendAddr(Tcl_DString *dsPtr, char *prefix, void *addr);
static Tcl_HashTable *GetTable(void);



/*
 *----------------------------------------------------------------------
 *
 * Ns_RegisterProcInfo --
 *
 *	Register a callback to describe the arguments to a proc,
 *	e.g., a thread start arg.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Given argProc will be invoked for given procAddr by
 *	Ns_GetProcInfo.
 *
 *----------------------------------------------------------------------
 */

void
Ns_RegisterProcInfo(void *procAddr, char *desc, Ns_ArgProc *argProc)
{
    Tcl_HashEntry *hPtr;
    Info *iPtr;
    int new;

    hPtr = Tcl_CreateHashEntry(GetTable(), (char *) procAddr, &new);
    if (!new) {
	iPtr = Tcl_GetHashValue(hPtr);
    } else {
    	iPtr = ns_malloc(sizeof(Info));
    	Tcl_SetHashValue(hPtr, iPtr);
    }
    iPtr->desc = desc;
    iPtr->proc = argProc;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_GetProcInfo --
 *
 *	Format a string of information for the given proc
 *	and arg, invoking the argProc callback if it exists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	String will be appended to given dsPtr.
 *
 *----------------------------------------------------------------------
 */

void
Ns_GetProcInfo(Tcl_DString *dsPtr, void *procAddr, void *arg)
{
    Tcl_HashEntry *hPtr;
    Info *iPtr;
    static Info nullInfo = {NULL, NULL};

    hPtr = Tcl_FindHashEntry(GetTable(), (char *) procAddr);
    if (hPtr != NULL) {
	iPtr = Tcl_GetHashValue(hPtr);
    } else {
	iPtr = &nullInfo;
    }
    if (iPtr->desc != NULL) {
    	Tcl_DStringAppendElement(dsPtr, iPtr->desc);
    } else {
	AppendAddr(dsPtr, "p", procAddr);
    }
    if (iPtr->proc != NULL) {
    	(*iPtr->proc)(dsPtr, arg);
    } else {
	AppendAddr(dsPtr, "a", arg);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * AppendAddr -- 
 *
 *	Format a simple string with the given address.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	String will be appended to given dsPtr.
 *
 *----------------------------------------------------------------------
 */

static void
AppendAddr(Tcl_DString *dsPtr, char *prefix, void *addr)
{
    char buf[30];

    if (addr == NULL) {
    	sprintf(buf, "%s:0x0", prefix);
    } else {
	sprintf(buf, "%s:%p", prefix, addr);
    }
    Tcl_DStringAppendElement(dsPtr, buf);
}


/*
 *----------------------------------------------------------------------
 *
 * GetTable --
 *
 *	Return the proc info table, initializing if needed.
 *
 * Results:
 *	Pointer to Tcl_HashTable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_HashTable *
GetTable(void)
{
    static Tcl_HashTable table;
    static int initialized;
    struct proc *procPtr;

    if (!initialized) {
    	Tcl_InitHashTable(&table, TCL_ONE_WORD_KEYS);
	initialized = 1;
    	procPtr = procs;
    	while (procPtr->procAddr != NULL) {
	    Ns_RegisterProcInfo(procPtr->procAddr, procPtr->desc,
		procPtr->argProc);
	    ++procPtr;
	}
    }
    return &table;
}
