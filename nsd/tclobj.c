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
 * tclobj.c --
 *
 *	Implement specialized Tcl_Obj's types for AOLserver.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static void		SetTimeInternalRep(Tcl_Obj *objPtr, Ns_Time *timePtr);
static int		SetTimeFromAny (Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		UpdateStringOfTime(Tcl_Obj *objPtr);

/*
 * The following type defines an Ns_Time used for high resolution
 * timing and waits.
 */

static Tcl_ObjType timeType = {
    "ns_time",
    (Tcl_FreeInternalRepProc *) NULL,
    (Tcl_DupInternalRepProc *) NULL,
    UpdateStringOfTime,
    SetTimeFromAny
};

static Tcl_ObjType *intTypePtr;


/*
 *----------------------------------------------------------------------
 *
 * NsTclInitObjs --
 *
 *	Initialize AOLserver Tcl_Obj types.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New Tcl types are registered.
 *
 *----------------------------------------------------------------------
 */

void
NsTclInitObjs()
{
    Tcl_Obj obj;

    if (sizeof(obj.internalRep) < sizeof(Ns_Time)) {
    	Tcl_Panic("NsTclInitObjs: sizeof(obj.internalRep) < sizeof(Ns_Time)");
    }
    if (sizeof(int) < sizeof(long)) {
    	Tcl_Panic("NsTclInitObjs: sizeof(int) < sizeof(long)");
    }
    intTypePtr = Tcl_GetObjType("int");
    if (intTypePtr == NULL) {
    	Tcl_Panic("NsTclInitObjs: no int type");
    }
    /* NB: Just Ns_Time type for now. */
    Tcl_RegisterObjType(&timeType);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_TclSetTimeObj --
 *
 *	Set a Tcl_Obj to an Ns_Time object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	String rep is invalidated and internal rep is set.
 *
 *----------------------------------------------------------------------
 */

void
Ns_TclSetTimeObj(Tcl_Obj *objPtr, Ns_Time *timePtr)
{
    Tcl_InvalidateStringRep(objPtr);
    SetTimeInternalRep(objPtr, timePtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_TclGetTimeFromObj --
 *
 *	Return the internal value of an Ns_Time Tcl_Obj.
 *
 * Results:
 *	TCL_OK or TCL_ERROR if not a valid Ns_Time.
 *
 * Side effects:
 *	Object is set to Ns_Time type if necessary.
 *
 *----------------------------------------------------------------------
 */

int
Ns_TclGetTimeFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, Ns_Time *timePtr)
{
    if (objPtr->typePtr == intTypePtr) {
	if (Tcl_GetLongFromObj(interp, objPtr, &timePtr->sec) != TCL_OK) {
	    return TCL_ERROR;
	}
	timePtr->usec = 0;
    } else {
    	if (objPtr->typePtr != &timeType
		&& SetTimeFromAny(interp, objPtr) != TCL_OK) {
	    return TCL_ERROR;
        }
    	*timePtr = *((Ns_Time *) &objPtr->internalRep);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfTime --
 *
 *	Update the string representation for an Ns_Time object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the Ns_Time-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfTime(objPtr)
    register Tcl_Obj *objPtr;	/* Int object whose string rep to update. */
{
    Ns_Time *timePtr = (Ns_Time *) &objPtr->internalRep;
    char buf[100];

    Ns_AdjTime(timePtr);
    if (timePtr->usec == 0) {
    	objPtr->length = sprintf(buf, "%ld", timePtr->sec);
    } else {
    	objPtr->length = sprintf(buf, "%ld:%ld", timePtr->sec, timePtr->usec);
    }
    objPtr->bytes = Tcl_Alloc(objPtr->length + 1);
    memcpy(objPtr->bytes, buf, objPtr->length+1);
}


/*
 *----------------------------------------------------------------------
 *
 * SetTimeFromAny --
 *
 *	Attempt to generate an Ns_Time internal form for the Tcl object.
 *
 * Results:
 *	The return value is a standard object Tcl result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an int is stored as "objPtr"s internal
 *	representation. 
 *
 *----------------------------------------------------------------------
 */

static int
SetTimeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    char *str, *sep;
    Ns_Time time;
    int result;

    str = Tcl_GetString(objPtr);
    sep = strchr(str, ':');
    if (objPtr->typePtr == intTypePtr || sep == NULL) {
	if (Tcl_GetLongFromObj(interp, objPtr, &time.sec) != TCL_OK) {
	    return TCL_ERROR;
	}
	time.usec = 0;
    } else {
	*sep = '\0';
	result = Tcl_GetInt(interp, str, (int *) &time.sec);
	*sep = ':';
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, sep+1, (int *) &time.usec) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    Ns_AdjTime(&time);
    SetTimeInternalRep(objPtr, &time);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SetTimeInternalRep --
 *
 *	Set the internal Ns_Time, freeing a previous internal rep if
 *	necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Object will be an Ns_Time type.
 *
 *----------------------------------------------------------------------
 */

static void
SetTimeInternalRep(Tcl_Obj *objPtr, Ns_Time *timePtr)
{
    Tcl_ObjType *typePtr = objPtr->typePtr;

    if (typePtr != NULL && typePtr->freeIntRepProc != NULL) {
	(*typePtr->freeIntRepProc)(objPtr);
    }
    objPtr->typePtr = &timeType;
    *((Ns_Time *) &objPtr->internalRep) = *timePtr;
}
