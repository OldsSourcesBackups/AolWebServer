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
 * nsexample.c --
 *
 *      A simple AOLserver module example.
 *
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "ns.h"


/*
 * The Ns_ModuleVersion variable is required.
 */
int Ns_ModuleVersion = 1;


/*
 * Private functions
 */
int
Ns_ModuleInit(char *hServer, char *hModule);

static int
ExampleInterpInit(Tcl_Interp *interp, void *context);

static int
HelloCmd(ClientData context, Tcl_Interp *interp, int argc, char **argv);



/*
 *----------------------------------------------------------------------
 *
 * Ns_ModuleInit --
 *
 *      This is the nsexample module's entry point.  AOLserver runs
 *      this function right after the module is loaded.  It is used to
 *      read configuration data, initialize data structures, kick off
 *      the Tcl initialization function (if any), and do other things
 *      at startup.
 *
 * Results:
 *	NS_OK or NS_ERROR
 *
 * Side effects:
 *	Module loads and initializes itself.
 *
 *----------------------------------------------------------------------
 */
 
int
Ns_ModuleInit(char *hServer, char *hModule)
{

    return (Ns_TclInitInterps(hServer, ExampleInterpInit, NULL));

}



/*
 *----------------------------------------------------------------------
 *
 * ExampleInterpInit --
 *
 *      Register new commands with the Tcl interpreter.
 *
 * Results:
 *	NS_OK or NS_ERROR
 *
 * Side effects:
 *	A C function is registered with the Tcl interpreter.
 *
 *----------------------------------------------------------------------
 */
 
static int
ExampleInterpInit(Tcl_Interp *interp, void *context)
{

    Tcl_CreateCommand(interp, "ns_hello", HelloCmd, NULL, NULL);

    return NS_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * HelloCmd --
 *
 *      A Tcl command that prints a friendly string with the name
 *      passed in on the first argument.
 *
 * Results:
 *	NS_OK
 *
 * Side effects:
 *	Tcl result is set to a string value.
 *
 *----------------------------------------------------------------------
 */
 
static int
HelloCmd(ClientData context, Tcl_Interp *interp, int argc, char **argv)
{

    Tcl_AppendResult(interp, "Hello, there, ", argv[1], ".  Welcome!", NULL);

    return NS_OK;
}

