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
 * limits.c --
 *
 *  Routines to manage resource limits.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

/*
 * Static functions defined in this file.
 */

static void InitLimits(Limits *limitsPtr);
static int LimitsResult(Tcl_Interp *interp, Limits *limitsPtr);
static int AppendLimit(Tcl_Interp *interp, char *limit, int val);
static int GetLimits(Tcl_Interp *interp, Tcl_Obj *objPtr,
             Limits **limitsPtrPtr, int create);

/*
 * Static variables defined in this file.
 */

static int            limid;
static Limits         deflimits;
static Tcl_HashTable  limtable;


/*
 *----------------------------------------------------------------------
 *
 * NsInitLimits --
 *
 *  Initialize the limits API.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

void
NsInitLimits(void)
{
    limid    = Ns_UrlSpecificAlloc();
    InitLimits(&deflimits);
    Tcl_InitHashTable(&limtable, TCL_STRING_KEYS);
}

Limits *
NsGetLimits(char *server, char *method, char *url)
{
    Limits *limitsPtr;

    limitsPtr = Ns_UrlSpecificGet(server, method, url, limid);
    if (limitsPtr == NULL) {
    limitsPtr = &deflimits;
    }
    return limitsPtr;
}

static void
InitLimits(Limits *limitsPtr)
{
    Ns_MutexInit(&limitsPtr->lock);
    limitsPtr->nrunning = limitsPtr->nwaiting = 0;
    limitsPtr->maxrun = limitsPtr->maxwait = INT_MAX;
    limitsPtr->maxupload = limitsPtr->timeout = INT_MAX;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclLimitsObjCmd --
 *
 *  Implements ns_limits command. 
 *
 * Results:
 *  Tcl result. 
 *
 * Side effects:
 *  See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclLimitsObjCmd(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj **objv)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Limits *limitsPtr, saveLimits;
    char *limits, *pattern;
    int i, val;
    static CONST char *opts[] = {
    "get", "set", "list", "register", NULL
    };
    enum {
    LGetIdx, LSetIdx, LListIdx, LRegisterIdx
    } opt;
    static CONST char *cfgs[] = {
    "-maxrun", "-maxwait", "-maxupload", NULL
    };
    enum {
    LCRunIdx, LCWaitIdx, LCUploadIdx
    } cfg;

    if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
    return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], opts, "option", 1,
                (int *) &opt) != TCL_OK) {
    return TCL_ERROR;
    }

    switch (opt) {
    case LListIdx:
    if (objc != 2 && objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?pattern?");
        return TCL_ERROR;
    }
    if (objc == 2) {
        pattern = NULL;
    } else {
        pattern = Tcl_GetString(objv[2]);
    }
    hPtr = Tcl_FirstHashEntry(&limtable, &search);
    while (hPtr != NULL) {
        limits = Tcl_GetHashKey(&limtable, hPtr);
        if (pattern == NULL || Tcl_StringMatch(limits, pattern)) {
            Tcl_AppendElement(interp, limits);
        }
        hPtr = Tcl_NextHashEntry(&search);
    }
    break;

    case LGetIdx:
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "limit");
        return TCL_ERROR;
    }
    if (GetLimits(interp, objv[2], &limitsPtr, 0) != TCL_OK ||
        LimitsResult(interp, limitsPtr) != TCL_OK) {
        return TCL_ERROR;
    }
    break;

    case LSetIdx:
    if (objc < 3 || (((objc - 3) % 2) != 0)) {
        Tcl_WrongNumArgs(interp, 2, objv, "limit ?opt val opt val...?");
        return TCL_ERROR;
    }
    (void) GetLimits(interp, objv[2], &limitsPtr, 1);
    saveLimits = *limitsPtr;
    for (i = 3; i < objc; i += 2) {
        if (Tcl_GetIndexFromObj(interp, objv[i], cfgs, "cfg", 0,
                (int *) &cfg) != TCL_OK || 
            Tcl_GetIntFromObj(interp, objv[i+1], &val) != TCL_OK) {
        *limitsPtr = saveLimits;
            return TCL_ERROR;
        }
        switch (cfg) {
        case LCRunIdx:
        limitsPtr->maxrun = val;
        break;

        case LCWaitIdx:
        limitsPtr->maxwait = val;
        break;

        case LCUploadIdx:
        limitsPtr->maxupload = val;
        break;

        }
    }
    if (LimitsResult(interp, limitsPtr) != TCL_OK) {
        return TCL_ERROR;
    }
    break;

    case LRegisterIdx:
    if (objc != 6) {
        Tcl_WrongNumArgs(interp, 2, objv, "limit server method url");
        return TCL_ERROR;
    }
    if (GetLimits(interp, objv[2], &limitsPtr, 0) != TCL_OK) {
        return TCL_ERROR;
    }
    Ns_UrlSpecificSet(Tcl_GetString(objv[3]),
              Tcl_GetString(objv[4]),
              Tcl_GetString(objv[5]), limid, limitsPtr, 0, NULL);
    break;
    }
    return TCL_OK;
}


static int
AppendLimit(Tcl_Interp *interp, char *limit, int val)
{
    Tcl_Obj *result = Tcl_GetObjResult(interp);

    if (Tcl_ListObjAppendElement(interp, result, Tcl_NewStringObj(limit, -1))
                != TCL_OK ||
        Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(val))
                != TCL_OK) {
    return 0;
    }
    return 1;
}


static int
LimitsResult(Tcl_Interp *interp, Limits *limitsPtr)
{
    if (!AppendLimit(interp, "nrunning", limitsPtr->nrunning) ||
    !AppendLimit(interp, "nwaiting", limitsPtr->nwaiting) ||
        !AppendLimit(interp, "maxwait", limitsPtr->maxwait) ||
        !AppendLimit(interp, "maxupload", limitsPtr->maxupload) ||
        !AppendLimit(interp, "maxrun", limitsPtr->maxrun)) {
    return TCL_ERROR;
    }
    return TCL_OK;
}


static int
GetLimits(Tcl_Interp *interp, Tcl_Obj *objPtr, Limits **limitsPtrPtr,
      int create)
{
    Limits *limitsPtr;
    char *limits = Tcl_GetString(objPtr);
    int new;
    Tcl_HashEntry *hPtr;

    if (!create) {
    hPtr = Tcl_FindHashEntry(&limtable, limits);
    } else {
    hPtr = Tcl_CreateHashEntry(&limtable, limits, &new);
    if (new) {
        limitsPtr = ns_malloc(sizeof(Limits));
        InitLimits(limitsPtr);
        Tcl_SetHashValue(hPtr, limitsPtr);
    }
    }
    if (hPtr == NULL) {
    Tcl_AppendResult(interp, "no such limits: ", limits, NULL);
    return TCL_ERROR;
    }
    *limitsPtrPtr = Tcl_GetHashValue(hPtr);
    return TCL_OK;
}
