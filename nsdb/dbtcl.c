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
 * dbtcl.c --
 *
 *	Tcl database access routines.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "db.h"

/*
 * The following structure maintains per-interp data.
 */

typedef struct InterpData {
    char *server;
    int   cleanup;
    Tcl_HashTable dbs;
} InterpData;

/*
 * Local functions defined in this file
 */

static int BadArgs(Tcl_Interp *interp, char **argv, char *args);
static int DbFail(Tcl_Interp *interp, Ns_DbHandle *handle, char *cmd);
static void EnterDbHandle(InterpData *idataPtr, Tcl_Interp *interp, Ns_DbHandle *handle);
static int DbGetHandle(InterpData *idataPtr, Tcl_Interp *interp, char *handleId,
		       Ns_DbHandle **handle, Tcl_HashEntry **phe);
static Tcl_InterpDeleteProc FreeData;
static Tcl_CmdProc DbCmd, QuoteListToListCmd, GetCsvCmd, DbErrorCodeCmd,
	DbErrorMsgCmd, GetCsvCmd, DbConfigPathCmd, PoolDescriptionCmd;
static Ns_TclDeferProc ReleaseDbs;
static char *datakey = "nsdb:data";


/*
 *----------------------------------------------------------------------
 * Ns_TclDbGetHandle --
 *
 *      Get database handle from its handle id.
 *
 * Results:
 *      See DbGetHandle().
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Ns_TclDbGetHandle(Tcl_Interp *interp, char *id, Ns_DbHandle **handlePtr)
{
    InterpData *idataPtr;

    idataPtr = Tcl_GetAssocData(interp, datakey, NULL);
    if (idataPtr == NULL) {
	return TCL_ERROR;
    }
    return DbGetHandle(idataPtr, interp, id, handlePtr, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * NsDbAddCmds --
 *
 *      Add the nsdb commands.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
NsDbAddCmds(Tcl_Interp *interp, void *arg)
{
    InterpData *idataPtr;

    /*
     * Initialize the per-interp data.
     */

    idataPtr = ns_malloc(sizeof(InterpData));
    idataPtr->server = arg;
    idataPtr->cleanup = 0;
    Tcl_InitHashTable(&idataPtr->dbs, TCL_STRING_KEYS);
    Tcl_SetAssocData(interp, datakey, FreeData, idataPtr);

    Tcl_CreateCommand(interp, "ns_db", DbCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_quotelisttolist", QuoteListToListCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_getcsv", GetCsvCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_dberrorcode", DbErrorCodeCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_dberrormsg", DbErrorMsgCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_getcsv", GetCsvCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_dbconfigpath", DbConfigPathCmd, idataPtr, NULL);
    Tcl_CreateCommand(interp, "ns_pooldescription", PoolDescriptionCmd, idataPtr, NULL);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * DbCmd --
 *
 *      Implement the AOLserver ns_db Tcl command.
 *
 * Results:
 *      Return TCL_OK upon success and TCL_ERROR otherwise.
 *
 * Side effects:
 *      Depends on the command.
 *
 *----------------------------------------------------------------------
 */

static int
DbCmd(ClientData arg, Tcl_Interp *interp, int argc, char **argv)
{
    InterpData	   *idataPtr = arg;
    Ns_DbHandle    *handlePtr;
    Ns_Set         *rowPtr;
    char           *cmd;
    char           *pool;

    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # of args: should be \"",
            argv[0], " command ?args ...?", NULL);
        return TCL_ERROR;
    }
    cmd = argv[1];
    if (STREQ(cmd, "open") || STREQ(cmd, "close")) {
    	Tcl_AppendResult(interp, "unsupported ns_db command: ", cmd, NULL);
    	return TCL_ERROR;

    } else if (STREQ(cmd, "pools")) {
        if (argc != 2) {
            return BadArgs(interp, argv, NULL);
        }
	pool = Ns_DbPoolList(idataPtr->server);
	if (pool != NULL) {
	    while (*pool != '\0') {
		Tcl_AppendElement(interp, pool);
		pool = pool + strlen(pool) + 1;
	    }
	}

    } else if (STREQ(cmd, "bouncepool")) {
	if (argc != 3) {
	    return BadArgs(interp, argv, "pool");
	}
	if (Ns_DbBouncePool(argv[2]) == NS_ERROR) {
	    Tcl_AppendResult(interp, "could not bounce: ", argv[2], NULL);
	    return TCL_ERROR;
	}

    } else if (STREQ(cmd, "gethandle")) {
	int timeout, nhandles, result;
	Ns_DbHandle **handlesPtrPtr;

	timeout = 0;
	if (argc >= 4) {
	    if (STREQ(argv[2], "-timeout")) {
		if (Tcl_GetInt(interp, argv[3], &timeout) != TCL_OK) {
		    return TCL_ERROR;
		}
		argv += 2;
		argc -= 2;
	    } else if (argc > 4) {
		return BadArgs(interp, argv,
		    "?-timeout timeout? ?pool? ?nhandles?");
	    }
	}
	argv += 2;
	argc -= 2;

	/*
	 * Determine the pool and requested number of handles
	 * from the remaining args.
	 */
       
	pool = argv[0];
	if (pool == NULL) {
	    pool = Ns_DbPoolDefault(idataPtr->server);
            if (pool == NULL) {
                Tcl_SetResult(interp, "no defaultpool configured", TCL_STATIC);
                return TCL_ERROR;
            }
        }
        if (Ns_DbPoolAllowable(idataPtr->server, pool) == NS_FALSE) {
            Tcl_AppendResult(interp, "no access to pool: \"", pool, "\"",
			     NULL);
            return TCL_ERROR;
        }
        if (argc < 2) {
	    nhandles = 1;
	} else {
            if (Tcl_GetInt(interp, argv[1], &nhandles) != TCL_OK) {
                return TCL_ERROR;
            }
	    if (nhandles <= 0) {
                Tcl_AppendResult(interp, "invalid nhandles \"", argv[1],
                    "\": should be greater than 0.", NULL);
                return TCL_ERROR;
            }
	}

    	/*
         * Allocate handles and enter them into Tcl.
         */

	if (nhandles == 1) {
    	    handlesPtrPtr = &handlePtr;
	} else {
	    handlesPtrPtr = ns_malloc(nhandles * sizeof(Ns_DbHandle *));
	}
	result = Ns_DbPoolTimedGetMultipleHandles(handlesPtrPtr, pool, 
    	    	                                  nhandles, timeout);
    	if (result == NS_OK) {
	    int i;
	    
	    for (i = 0; i < nhandles; ++i) {
                EnterDbHandle(idataPtr, interp, handlesPtrPtr[i]);
            }
	}
	if (handlesPtrPtr != &handlePtr) {
	    ns_free(handlesPtrPtr);
	}
	if (result != NS_TIMEOUT && result != NS_OK) {
            Tcl_AppendResult(interp, "could not allocate ",
	    	nhandles > 1 ? argv[1] : "1", " handle",
		nhandles > 1 ? "s" : "", " from pool \"",
		pool, "\"", NULL);
            return TCL_ERROR;
        }

    } else if (STREQ(cmd, "exception")) {
        if (argc != 3) {
            return BadArgs(interp, argv, "dbId");
        }
        if (DbGetHandle(idataPtr, interp, argv[2], &handlePtr, NULL) != TCL_OK) {
            return TCL_ERROR;
        }
        Tcl_AppendElement(interp, handlePtr->cExceptionCode);
        Tcl_AppendElement(interp, handlePtr->dsExceptionMsg.string);

    } else {
	Tcl_HashEntry  *hPtr;

    	/*
         * All remaining commands require a valid database
         * handle.  The exception message is cleared first.
         */
	
        if (argc < 3) {
            return BadArgs(interp, argv, "dbId ?args?");
        }
        if (DbGetHandle(idataPtr, interp, argv[2], &handlePtr, &hPtr) != TCL_OK) {
            return TCL_ERROR;
        }
        Ns_DStringFree(&handlePtr->dsExceptionMsg);
        handlePtr->cExceptionCode[0] = '\0';

    	/*
         * The following commands require just the handle.
         */

        if (STREQ(cmd, "poolname") ||
	    STREQ(cmd, "password") ||
	    STREQ(cmd, "user") ||
	    STREQ(cmd, "datasource") ||
	    STREQ(cmd, "disconnect") ||
	    STREQ(cmd, "dbtype") ||
	    STREQ(cmd, "driver") ||
	    STREQ(cmd, "cancel") ||
	    STREQ(cmd, "bindrow") ||
	    STREQ(cmd, "flush") ||
	    STREQ(cmd, "releasehandle") ||
	    STREQ(cmd, "resethandle") ||
	    STREQ(cmd, "connected") ||
	    STREQ(cmd, "sp_exec") ||
	    STREQ(cmd, "sp_getparams") ||
	    STREQ(cmd, "sp_returncode")) {
	    
            if (argc != 3) {
                return BadArgs(interp, argv, "dbId");
            }

	    if (STREQ(cmd, "poolname")) {
                Tcl_SetResult(interp, handlePtr->poolname, TCL_VOLATILE);

	    } else if (STREQ(cmd, "password")) {
                Tcl_SetResult(interp, handlePtr->password, TCL_VOLATILE);

	    } else if (STREQ(cmd, "user")) {		    
                Tcl_SetResult(interp, handlePtr->user, TCL_VOLATILE);

	    } else if (STREQ(cmd, "dbtype")) {
                Tcl_SetResult(interp, Ns_DbDriverDbType(handlePtr), 
			      TCL_STATIC);

	    } else if (STREQ(cmd, "driver")) {
                Tcl_SetResult(interp, Ns_DbDriverName(handlePtr), TCL_STATIC);

	    } else if (STREQ(cmd, "datasource")) {
		Tcl_SetResult(interp, handlePtr->datasource, TCL_STATIC);

	    } else if (STREQ(cmd, "disconnect")) {
		NsDbDisconnect(handlePtr);

	    } else if (STREQ(cmd, "flush")) {
                if (Ns_DbFlush(handlePtr) != NS_OK) {
                    return DbFail(interp, handlePtr, cmd);
                }

    	    } else if (STREQ(cmd, "bindrow")) {
                rowPtr = Ns_DbBindRow(handlePtr);
                if (rowPtr == NULL) {
                    return DbFail(interp, handlePtr, cmd);
                }
                Ns_TclEnterSet(interp, rowPtr, NS_TCL_SET_STATIC);

    	    } else if (STREQ(cmd, "releasehandle")) {
		Tcl_DeleteHashEntry(hPtr);
    		Ns_DbPoolPutHandle(handlePtr);

    	    } else if (STREQ(cmd, "resethandle")) {
		if (Ns_DbResetHandle(handlePtr) != NS_OK) {
		  return DbFail(interp, handlePtr, cmd);
		}
		Tcl_SetObjResult(interp, Tcl_NewIntObj(NS_OK));

    	    } else if (STREQ(cmd, "cancel")) {
                if (Ns_DbCancel(handlePtr) != NS_OK) {
                    return DbFail(interp, handlePtr, cmd);
                }

	    } else if (STREQ(cmd, "connected")) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(handlePtr->connected));

	    } else if (STREQ(cmd, "sp_exec")) {
		switch (Ns_DbSpExec(handlePtr)) {
		case NS_DML:
		    Tcl_SetResult(interp, "NS_DML", TCL_STATIC);
		    break;
		case NS_ROWS:
		    Tcl_SetResult(interp, "NS_ROWS", TCL_STATIC);
		    break;
		default:
		    return DbFail(interp, handlePtr, cmd);
		    break;
		}
	    } else if (STREQ(cmd, "sp_returncode")) {
		char *tmpbuf;

		tmpbuf = ns_malloc(32);
		if (Ns_DbSpReturnCode(handlePtr, tmpbuf, 32) != NS_OK) {
		    ns_free(tmpbuf);
		    return DbFail(interp, handlePtr, cmd);
		} else {
		    Tcl_SetResult(interp, tmpbuf, TCL_VOLATILE);
		    ns_free(tmpbuf);
		}
	    } else if (STREQ(cmd, "sp_getparams")) {
		rowPtr = Ns_DbSpGetParams(handlePtr);
		if (rowPtr == NULL) {
		    return DbFail(interp, handlePtr, cmd);
		}
		Ns_TclEnterSet(interp, rowPtr, NS_TCL_SET_DYNAMIC);
	    }
		

    	/*
         * The following commands require a 3rd argument.
      	 */

        } else if (STREQ(cmd, "getrow") ||
		   STREQ(cmd, "dml") ||
		   STREQ(cmd, "1row") ||
		   STREQ(cmd, "0or1row") ||
		   STREQ(cmd, "exec") ||
		   STREQ(cmd, "select") ||
		   STREQ(cmd, "sp_start") ||
		   STREQ(cmd, "interpretsqlfile")) {

            if (argc != 4) {
    	    	if (STREQ(cmd, "interpretsqlfile")) {
                    return BadArgs(interp, argv, "dbId sqlfile");

		} else if (STREQ(cmd, "getrow")) {
		    return BadArgs(interp, argv, "dbId row");

		} else {
		    return BadArgs(interp, argv, "dbId sql");
		}
	    }

    	    if (STREQ(cmd, "dml")) {
                if (Ns_DbDML(handlePtr, argv[3]) != NS_OK) {
                    return DbFail(interp, handlePtr, cmd);
                }

    	    } else if (STREQ(cmd, "1row")) {
                rowPtr = Ns_Db1Row(handlePtr, argv[3]);
                if (rowPtr == NULL) {
                    return DbFail(interp, handlePtr, cmd);
                }
                Ns_TclEnterSet(interp, rowPtr, NS_TCL_SET_DYNAMIC);

    	    } else if (STREQ(cmd, "0or1row")) {
    	    	int nrows;

                rowPtr = Ns_Db0or1Row(handlePtr, argv[3], &nrows);
                if (rowPtr == NULL) {
                    return DbFail(interp, handlePtr, cmd);
                }
                if (nrows == 0) {
                    Ns_SetFree(rowPtr);
                } else {
                    Ns_TclEnterSet(interp, rowPtr, NS_TCL_SET_DYNAMIC);
                }

    	    } else if (STREQ(cmd, "select")) {
                rowPtr = Ns_DbSelect(handlePtr, argv[3]);
                if (rowPtr == NULL) {
                    return DbFail(interp, handlePtr, cmd);
                }
                Ns_TclEnterSet(interp, rowPtr, NS_TCL_SET_STATIC);

    	    } else if (STREQ(cmd, "exec")) {
                switch (Ns_DbExec(handlePtr, argv[3])) {
                case NS_DML:
                    Tcl_SetResult(interp, "NS_DML", TCL_STATIC);
                    break;
                case NS_ROWS:
                    Tcl_SetResult(interp, "NS_ROWS", TCL_STATIC);
                    break;
                default:
                    return DbFail(interp, handlePtr, cmd);
                    break;
                }

    	    } else if (STREQ(cmd, "interpretsqlfile")) {
                if (Ns_DbInterpretSqlFile(handlePtr, argv[3]) != NS_OK) {
                    return DbFail(interp, handlePtr, cmd);
                }

	    } else if (STREQ(cmd, "sp_start")) {
		if (Ns_DbSpStart(handlePtr, argv[3]) != NS_OK) {
		    return DbFail(interp, handlePtr, cmd);
		}
		Tcl_SetResult(interp, "0", TCL_STATIC);
		
    	    } else { /* getrow */
                if (Ns_TclGetSet2(interp, argv[3], &rowPtr) != TCL_OK) {
                    return TCL_ERROR;
                }
                switch (Ns_DbGetRow(handlePtr, rowPtr)) {
                case NS_OK:
                    Tcl_SetResult(interp, "1", TCL_STATIC);
                    break;
                case NS_END_DATA:
                    Tcl_SetResult(interp, "0", TCL_STATIC);
                    break;
                default:
                    return DbFail(interp, handlePtr, cmd);
                    break;
                }
            }

        } else if (STREQ(cmd, "verbose")) {
	    int verbose;

            if (argc != 3 && argc != 4) {
                return BadArgs(interp, argv, "dbId ?on|off?");
            }
            if (argc == 4) {
                if (Tcl_GetBoolean(interp, argv[3], &verbose) != TCL_OK) {
                    return TCL_ERROR;
                }
                handlePtr->verbose = verbose;
            }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(handlePtr->verbose));

        } else if (STREQ(cmd, "setexception")) {
            if (argc != 5) {
                return BadArgs(interp, argv, "dbId code message");
            }
            if (strlen(argv[3]) > 5) {
                Tcl_AppendResult(interp, "code \"", argv[3],
		    "\" more than 5 characters", NULL);
                return TCL_ERROR;
            }
            Ns_DbSetException(handlePtr, argv[3], argv[4]);
	} else if (STREQ(cmd, "sp_setparam")) {
	    if (argc != 7) {
		return BadArgs(interp, argv,
			       "dbId paramname type in|out value");
	    }
	    if (!STREQ(argv[5], "in") && !STREQ(argv[5], "out")) {
		Tcl_SetResult(interp, "inout parameter of setparam must "
			      "be \"in\" or \"out\"", TCL_STATIC);
		return TCL_ERROR;
	    }
	    if (Ns_DbSpSetParam(handlePtr, argv[3], argv[4], argv[5],
				argv[6]) != NS_OK) {
		return DbFail(interp, handlePtr, cmd);
	    } else {
		Tcl_SetResult(interp, "1", TCL_STATIC);
	    }
        } else {
            Tcl_AppendResult(interp, argv[0], ":  Unknown command \"",
			     argv[1], "\":  should be "
			     "0or1row, "
			     "1row, "
			     "bindrow, "
			     "cancel, "
			     "connected, "
			     "datasource, "
			     "dbtype, "
			     "disconnect, "
			     "dml, "
			     "driver, "
			     "exception, "
			     "exec, "
			     "flush, "
			     "gethandle, "
			     "getrow, "
			     "interpretsqlfile, "
			     "password, "
			     "poolname, "
			     "pools, "
			     "releasehandle, "
			     "select, "
			     "setexception, "
			     "sp_start, "
			     "sp_setparam, "
			     "sp_exec, "
			     "sp_returncode, "
			     "sp_getparams, "
			     "user, "
			     "bouncepool"
			     " or verbose", NULL);
            return TCL_ERROR;
        }
    }

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * DbErrorCodeCmd --
 *
 *      Get database exception code for the database handle.
 *
 * Results:
 *      Returns TCL_OK and database exception code is set as Tcl result
 *	or TCL_ERROR if failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
ErrorCmd(ClientData arg, Tcl_Interp *interp, int argc, char **argv, int cmd)
{
    InterpData *idataPtr = arg;
    Ns_DbHandle *handle;

    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # args:  should be \"",
            argv[0], " dbId\"", NULL);
        return TCL_ERROR;
    }
    if (DbGetHandle(idataPtr, interp, argv[1], &handle, NULL) != TCL_OK) {
        return TCL_ERROR;
    }
    if (cmd == 'c') {
    	Tcl_SetResult(interp, handle->cExceptionCode, TCL_VOLATILE);
    } else {
    	Tcl_SetResult(interp, handle->dsExceptionMsg.string, TCL_VOLATILE);
    }
    return TCL_OK;
}

static int
DbErrorCodeCmd(ClientData arg, Tcl_Interp *interp, int argc,
		    char **argv)
{
    return ErrorCmd(arg, interp, argc, argv, 'c');
}

static int
DbErrorMsgCmd(ClientData arg, Tcl_Interp *interp, int argc, char **argv)
{
    return ErrorCmd(arg, interp, argc, argv, 'm');
}


/*
 *----------------------------------------------------------------------
 * DbConfigPathCmd --
 *
 *      Get the database section name from the configuration file.
 *
 * Results:
 *      TCL_OK and the database section name is set as the Tcl result
 *	or TCL_ERROR if failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
DbConfigPathCmd(ClientData arg, Tcl_Interp *interp, int argc,
		     char **argv)
{
    InterpData *idataPtr = arg;
    char *section;

    if (argc != 1) {
        Tcl_AppendResult(interp, "wrong # of args: should be \"", argv[0],
			 "\"", NULL);
        return TCL_ERROR;
    }
    section = Ns_ConfigGetPath(idataPtr->server, NULL, "db", NULL);
    Tcl_SetResult(interp, section, TCL_STATIC);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * PoolDescriptionCmd --
 *
 *      Get the pool's description string.
 *
 * Results:
 *      Return TCL_OK and the pool's description string is set as the 
 *	Tcl result string or TCL_ERROR if failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
PoolDescriptionCmd(ClientData arg, Tcl_Interp *interp, int argc,
			char **argv)
{
    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # of args: should be \"", argv[0],
			 " poolname\"", NULL);
        return TCL_ERROR;
    }
    Tcl_SetResult(interp, Ns_DbPoolDescription(argv[1]), TCL_STATIC);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * QuoteListToListCmd --
 *
 *      Remove space, \ and ' characters in a string.
 *
 * Results:
 *      TCL_OK and set the stripped string as the Tcl result or TCL_ERROR
 *	if failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
QuoteListToListCmd(ClientData arg, Tcl_Interp *interp, int argc,
			char **argv)
{
    char       *quotelist;
    int         inquotes;
    Ns_DString  ds;

    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # of args: should be \"",
	    argv[0], " quotelist\"", NULL);
        return TCL_ERROR;
    }
    quotelist = argv[1];
    inquotes = NS_FALSE;
    Ns_DStringInit(&ds);
    while (*quotelist != '\0') {
        if (isspace(UCHAR(*quotelist)) && inquotes == NS_FALSE) {
            if (ds.length != 0) {
                Tcl_AppendElement(interp, ds.string);
                Ns_DStringTrunc(&ds, 0);
            }
            while (isspace(UCHAR(*quotelist))) {
                quotelist++;
            }
        } else if (*quotelist == '\\' && (*(quotelist + 1) != '\0')) {
            Ns_DStringNAppend(&ds, quotelist + 1, 1);
            quotelist += 2;
        } else if (*quotelist == '\'') {
            if (inquotes) {
                /* Finish element */
                Tcl_AppendElement(interp, ds.string);
                Ns_DStringTrunc(&ds, 0);
                inquotes = NS_FALSE;
            } else {
                /* Start element */
                inquotes = NS_TRUE;
            }
            quotelist++;
        } else {
            Ns_DStringNAppend(&ds, quotelist, 1);
            quotelist++;
        }
    }
    if (ds.length != 0) {
        Tcl_AppendElement(interp, ds.string);
    }
    Ns_DStringFree(&ds);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * GetCsvCmd --
 *
 *	Implement the ns_getcvs command to read a line from a CSV file
 *	and parse the results into a Tcl list variable.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	One line is read for given open channel.
 *
 *----------------------------------------------------------------------
 */

static int
GetCsvCmd(ClientData arg, Tcl_Interp *interp, int argc, char **argv)
{
    int             ncols, inquote, quoted, blank;
    char            c, *p, buf[20];
    const char	   *result;
    Tcl_DString     line, cols, elem;
    Tcl_Channel	    chan;

    if (argc != 3) {
        Tcl_AppendResult(interp, "wrong # of args: should be \"",
	    argv[0], " fileId varName\"", NULL);
        return TCL_ERROR;
    }
    if (Ns_TclGetOpenChannel(interp, argv[1], 0, 0, &chan) == TCL_ERROR) {
        return TCL_ERROR;
    }
    
    Tcl_DStringInit(&line);
    if (Tcl_Gets(chan, &line) < 0) {
	Tcl_DStringFree(&line);
    	if (!Tcl_Eof(chan)) {
	    Tcl_AppendResult(interp, "could not read from ", argv[1],
	        ": ", Tcl_PosixError(interp), NULL);
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, "-1", TCL_STATIC);
	return TCL_OK;
    }

    Tcl_DStringInit(&cols);
    Tcl_DStringInit(&elem);
    ncols = 0;
    inquote = 0;
    quoted = 0;
    blank = 1;
    p = line.string;
    while (*p != '\0') {
        c = *p++;
loopstart:
        if (inquote) {
            if (c == '"') {
		c = *p++;
		if (c == '\0') {
		    break;
		}
                if (c == '"') {
                    Tcl_DStringAppend(&elem, &c, 1);
                } else {
                    inquote = 0;
                    goto loopstart;
                }
            } else {
                Tcl_DStringAppend(&elem, &c, 1);
            }
        } else {
            if ((c == '\n') || (c == '\r')) {
                while ((c = *p++) != '\0') {
                    if ((c != '\n') && (c != '\r')) {
			--p;
                        break;
                    }
                }
                break;
            }
            if (c == '"') {
                inquote = 1;
                quoted = 1;
                blank = 0;
            } else if ((c == '\r') || (elem.length == 0 && isspace(UCHAR(c)))) {
                continue;
            } else if (c == ',') {
                if (!quoted) {
                    Ns_StrTrimRight(elem.string);
                }
		Tcl_DStringAppendElement(&cols, elem.string);
                Tcl_DStringTrunc(&elem, 0);
                ncols++;
                quoted = 0;
            } else {
                blank = 0;
                Tcl_DStringAppend(&elem, &c, 1);
            }
        }
    }
    if (!quoted) {
        Ns_StrTrimRight(elem.string);
    }
    if (!blank) {
	Tcl_DStringAppendElement(&cols, elem.string);
        ncols++;
    }
    result = Tcl_SetVar(interp, argv[2], cols.string, TCL_LEAVE_ERR_MSG);
    Tcl_DStringFree(&line);
    Tcl_DStringFree(&cols);
    Tcl_DStringFree(&elem);
    if (result == NULL) {
	return TCL_ERROR;
    }
    sprintf(buf, "%d", ncols);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * DbGetHandle --
 *
 *      Get database handle from its handle id.
 *
 * Results:
 *      Return TCL_OK if handle is found or TCL_ERROR otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DbGetHandle(InterpData *idataPtr, Tcl_Interp *interp, char *id, Ns_DbHandle **handle,
	    Tcl_HashEntry **hPtrPtr)
{
    Tcl_HashEntry  *hPtr;

    hPtr = Tcl_FindHashEntry(&idataPtr->dbs, id);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "invalid database id:  \"", id, "\"",
	    NULL);
	return TCL_ERROR;
    }
    *handle = (Ns_DbHandle *) Tcl_GetHashValue(hPtr);
    if (hPtrPtr != NULL) {
	*hPtrPtr = hPtr;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * EnterDbHandle --
 *
 *      Enter a database handle and create its handle id.
 *
 * Results:
 *      The database handle id is returned as a Tcl result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
EnterDbHandle(InterpData *idataPtr, Tcl_Interp *interp, Ns_DbHandle *handle)
{
    Tcl_HashEntry *hPtr;
    int            new, next;
    char	   buf[100];

    if (!idataPtr->cleanup) {
	Ns_TclRegisterDeferred(interp, ReleaseDbs, idataPtr);
	idataPtr->cleanup = 1;
    }
    next = idataPtr->dbs.numEntries;
    do {
        sprintf(buf, "nsdb%x", next++);
        hPtr = Tcl_CreateHashEntry(&idataPtr->dbs, buf, &new);
    } while (!new);
    Tcl_AppendElement(interp, buf);
    Tcl_SetHashValue(hPtr, handle);
}


/*
 *----------------------------------------------------------------------
 * BadArgs --
 *
 *      Common routine that creates bad arguments message.
 *
 * Results:
 *      Return TCL_ERROR and set bad argument message as Tcl result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BadArgs(Tcl_Interp *interp, char **argv, char *args)
{
    Tcl_AppendResult(interp, "wrong # args: should be \"",
        argv[0], " ", argv[1], NULL);
    if (args != NULL) {
        Tcl_AppendResult(interp, " ", args, NULL);
    }
    Tcl_AppendResult(interp, "\"", NULL);

    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 * DbFail --
 *
 *      Common routine that creates database failure message.
 *
 * Results:
 *      Return TCL_ERROR and set database failure message as Tcl result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
DbFail(Tcl_Interp *interp, Ns_DbHandle *handle, char *cmd)
{
    Tcl_AppendResult(interp, "Database operation \"", cmd, "\" failed", NULL);
    if (handle->cExceptionCode[0] != '\0') {
        Tcl_AppendResult(interp, " (exception ", handle->cExceptionCode,
			 NULL);
        if (handle->dsExceptionMsg.length > 0) {
            Tcl_AppendResult(interp, ", \"", handle->dsExceptionMsg.string,
			     "\"", NULL);
        }
        Tcl_AppendResult(interp, ")", NULL);
    }
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 * FreeData --
 *
 *      Free per-interp data at interp delete time.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
FreeData(ClientData arg, Tcl_Interp *interp)
{
    InterpData *idataPtr = arg;

    Tcl_DeleteHashTable(&idataPtr->dbs);
    ns_free(idataPtr);
}


/*
 *----------------------------------------------------------------------
 * ReleaseDbs --
 *
 *      Release any database handles still held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
ReleaseDbs(Tcl_Interp *interp, void *arg)
{
    Ns_DbHandle *handlePtr;
    Tcl_HashEntry  *hPtr;
    Tcl_HashSearch  search;
    InterpData *idataPtr = arg;

    hPtr = Tcl_FirstHashEntry(&idataPtr->dbs, &search);
    while (hPtr != NULL) {
    	handlePtr = Tcl_GetHashValue(hPtr);
   	Ns_DbPoolPutHandle(handlePtr);
    	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&idataPtr->dbs);
    Tcl_InitHashTable(&idataPtr->dbs, TCL_STRING_KEYS);
    idataPtr->cleanup = 0;
}
