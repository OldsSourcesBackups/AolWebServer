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
 * adprequest.c --
 *
 *	ADP connection request support.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

static int AdpFlush(NsInterp *itPtr, int stream);


/*
 *----------------------------------------------------------------------
 *
 * NsAdpProc --
 *
 *	Check for a normal file and call Ns_AdpRequest.
 *
 * Results:
 *	A standard AOLserver request result.
 *
 * Side effects:
 *	Depends on code embedded within page.
 *
 *----------------------------------------------------------------------
 */

int
NsAdpProc(void *arg, Ns_Conn *conn)
{
    Ns_DString file;
    int status;

    Ns_DStringInit(&file);
    Ns_UrlToFile(&file, Ns_ConnServer(conn), conn->request->url);
    if (access(file.string, R_OK) != 0) {
	status = Ns_ConnReturnNotFound(conn);
    } else {
	status = Ns_AdpRequest(conn, file.string);
    }
    Ns_DStringFree(&file);
    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_AdpRequest -
 *
 *  	Invoke a file for an ADP request.
 *
 * Results:
 *	A standard AOLserver request result.
 *
 * Side effects:
 *	Depends on code embedded within page.
 *
 *----------------------------------------------------------------------
 */

int
Ns_AdpRequest(Ns_Conn *conn, char *file)
{
    Conn	     *connPtr = (Conn *) conn;
    Tcl_Interp       *interp;
    NsInterp          *itPtr;
    int               status;
    char             *type, *start, *argv[1];
    Ns_Set           *setPtr;
    NsServer	     *servPtr;
    
    /*
     * Get the current connection's interp.
     */

    interp = Ns_GetConnInterp(conn);
    itPtr = NsGetInterp(interp);
    servPtr = itPtr->servPtr;

    /*
     * Set the old conn variable for backwards compatibility.
     */

    Tcl_SetVar2(interp, "conn", NULL, connPtr->idstr, TCL_GLOBAL_ONLY);
    Tcl_ResetResult(interp);

    itPtr->adp.stream = 0;
    if (servPtr->adp.enabledebug &&
	STREQ(conn->request->method, "GET") &&
	(setPtr = Ns_ConnGetQuery(conn)) != NULL) {
	itPtr->adp.debugFile = Ns_SetIGet(setPtr, "debug");
    }
    type = Ns_GetMimeType(file);
    if (type == NULL || (strcmp(type, "*/*") == 0)) {
        type = "text/html; charset=iso-8859-1";
    }
    NsAdpSetMimeType(itPtr, type);
    if (servPtr->adp.enableexpire) {
	Ns_ConnCondSetHeaders(conn, "Expires", "now");
    }

    /*
     * Include the ADP with the special start page and null args.
     */

    start = servPtr->adp.startpage ? servPtr->adp.startpage : file;
    argv[0] = NULL;
    if (NsAdpInclude(itPtr, start, 0, argv) != TCL_OK) {
	Ns_TclLogError(interp);
    }

    /*
     * If a response was not generated by the ADP code,
     * generate one now.
     */

    status = NS_OK;
    if (!(conn->flags & NS_CONN_SENTHDRS)) {
	if (itPtr->adp.exception == ADP_OVERFLOW) {
	    Ns_Log(Error, "adp: stack overflow: '%s'", file);
	    status = Ns_ConnReturnInternalError(conn);
	} else if (itPtr->adp.exception != ADP_ABORT) {
	    status = AdpFlush(itPtr, 0);
	}
    }

    /*
     * Cleanup the per-thead ADP context.
     */

    itPtr->adp.outputPtr = NULL;
    itPtr->adp.exception = ADP_OK;
    itPtr->adp.depth = 0;
    itPtr->adp.argc = 0;
    itPtr->adp.argv = NULL;
    itPtr->adp.cwd = NULL;
    itPtr->adp.file = NULL;
    itPtr->adp.debugLevel = 0;
    itPtr->adp.debugInit = 0;
    itPtr->adp.debugFile = NULL;
    NsAdpSetMimeType(itPtr, NULL);
    NsAdpSetCharSet(itPtr, NULL);
    Tcl_DStringFree(&itPtr->adp.response);
    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpFlush --
 *
 *	Flush current response output to connection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None unless streaming is enabled in which case AdpFlush
 *	is called.
 *
 *----------------------------------------------------------------------
 */

void
NsAdpFlush(NsInterp *itPtr)
{
    if (itPtr->adp.stream
	&& itPtr->conn != NULL
	&& itPtr->adp.response.length > 0) {
	if (AdpFlush(itPtr, 1) != NS_OK) {
	    itPtr->adp.stream = 0;
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpStream --
 *
 *	Turn streaming mode on.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Headers and current data, if any, are flushed.
 *
 *----------------------------------------------------------------------
 */

void
NsAdpStream(NsInterp *itPtr)
{
    if (!itPtr->adp.stream && itPtr->conn != NULL) {
    	itPtr->adp.stream = 1;
	NsAdpFlush(itPtr);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpSetMimeType, NsAdpSetCharSet --
 *
 *	Sets the mime type (charset) for this adp.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *  	New mime type (charset) will be used on output.
 *
 *----------------------------------------------------------------------
 */

void
NsAdpSetMimeType(NsInterp *itPtr, char *mimetype)
{
    if (itPtr->adp.mimetype != NULL) {
	ns_free(itPtr->adp.mimetype);
    }
    itPtr->adp.mimetype = ns_strcopy(mimetype);
}

void
NsAdpSetCharSet(NsInterp *itPtr, char *charset)
{
    if (itPtr->adp.charset != NULL) {
	ns_free(itPtr->adp.charset);
    }
    itPtr->adp.charset = ns_strcopy(charset);
}


/*
 *----------------------------------------------------------------------
 *
 * NsFreeAdp --
 *
 *	Interp delete callback to free ADP resources.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *  	None.
 *
 *----------------------------------------------------------------------
 */

void
NsFreeAdp(NsInterp *itPtr)
{
    if (itPtr->adp.cache != NULL) {
	Ns_CacheDestroy(itPtr->adp.cache);
    }
    if (itPtr->adp.mimetype != NULL) {
        ns_free(itPtr->adp.mimetype);
    }
    if (itPtr->adp.charset != NULL) {
        ns_free(itPtr->adp.charset);
    }
}


static int
AdpFlush(NsInterp *itPtr, int stream)
{
    Ns_Conn *conn;
    Tcl_DString  ds, *dsPtr;
    int result, len, senthdrs;
    char *buf, *charset, *type;

    Tcl_DStringInit(&ds);
    conn = itPtr->conn;
    dsPtr = &itPtr->adp.response;
    buf = dsPtr->string;
    len = dsPtr->length;
    type = itPtr->adp.mimetype;

    /*
     * On the first flush request, determine the output
     * encoding to which the UTF buffer should be converted
     * if any.
     */

    senthdrs = (conn->flags & NS_CONN_SENTHDRS);
    if (!senthdrs) {
	charset = itPtr->adp.charset;
	if (charset == NULL && type != NULL) {
	    charset = strstr(type, "charset=");
	    if (charset != NULL) {
		charset += 8;
		itPtr->adp.encoding = Ns_GetEncoding(charset);
	    }
	}
    }

    /*
     * If necessary, encode the output.
     */

    if (itPtr->adp.encoding != NULL) {
	Tcl_UtfToExternalDString(itPtr->adp.encoding, buf, len, &ds);
	buf = ds.string;
	len = ds.length;
    }

    /*
     * Flush out the headers now that the encoded output length
     * is known for non-streaming output.
     */

    result = NS_OK;
    if (!senthdrs) {
	Ns_ConnSetRequiredHeaders(conn, type, stream ? 0 : len);
	result = Ns_ConnFlushHeaders(conn, 200);
    }

    /*
     * Write the output buffer and if not streaming, close the
     * connection.
     */

    if (result == NS_OK) {
	result = Ns_WriteConn(conn, buf, len);
	if (result == NS_OK && !stream) {
	    result = Ns_ConnClose(conn);
	}
    }

    Tcl_DStringFree(&ds);
    Tcl_DStringTrunc(dsPtr, 0);
    return result;
}
