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
 * form.c --
 *
 *      Routines for dealing with HTML FORM's.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

static void ParseQuery(char *form, Ns_Set *set, Tcl_Encoding encoding);
static void ParseMultiInput(Conn *connPtr, Tcl_Encoding encoding,
			    char *start, char *end);
static char *Ext2Utf(Tcl_DString *dsPtr, char *s, int len, Tcl_Encoding encoding);
static int GetBoundary(Tcl_DString *dsPtr, Ns_Conn *conn);
static char *NextBoundry(Tcl_DString *dsPtr, char *s, char *e);
static int GetValue(char *hdr, char *att, char **vsPtr, char **vePtr);


/*
 *----------------------------------------------------------------------
 *
 * Ns_ConnGetQuery --
 *
 *	Get the connection query data, either by reading the content 
 *	of a POST request or get it from the query string 
 *
 * Results:
 *	Query data or NULL if error 
 *
 * Side effects:
 *	
 *
 *----------------------------------------------------------------------
 */

Ns_Set  *
Ns_ConnGetQuery(Ns_Conn *conn)
{
    Conn           *connPtr = (Conn *) conn;
    Tcl_Encoding    encoding;
    Tcl_DString	    bound;
    char	   *s, *e, *form, *formend;
    
    if (!NsCheckQuery(conn)) {
	Ns_ConnClearQuery(conn);
    }
    if (connPtr->query == NULL) {
	encoding = connPtr->queryEncoding = Ns_ConnGetUrlEncoding(conn);
	connPtr->query = Ns_SetCreate(NULL);
	if (!STREQ(connPtr->request->method, "POST")) {
	    form = connPtr->request->query;
	    if (form != NULL) {
		ParseQuery(form, connPtr->query, encoding);
	    }
	} else if ((form = connPtr->content) != NULL) {
	    Tcl_DStringInit(&bound);
	    if (!GetBoundary(&bound, conn)) {
		ParseQuery(form, connPtr->query, encoding);
	    } else {
	    	formend = form + connPtr->contentLength;
		s = NextBoundry(&bound, form, formend);
		while (s != NULL) {
		    s += bound.length;
		    if (*s == '\r') ++s;
		    if (*s == '\n') ++s;
		    e = NextBoundry(&bound, s, formend);
		    if (e != NULL) {
			ParseMultiInput(connPtr, encoding, s, e);
		    }
		    s = e;
		}
	    }
	    Tcl_DStringFree(&bound);
	}
    }
    return connPtr->query;
}



/*
 *----------------------------------------------------------------------
 *
 * Ns_ConnClearQuery --
 *
 *	Release the any query set cached up from a previous call
 *      to Ns_ConnGetQuery.  Useful if the query data requires
 *      reparsing, as when the encoding changes.
 *
 * Results:
 *	Query data or NULL if error 
 *
 * Side effects:
 *	
 *
 *----------------------------------------------------------------------
 */

void
Ns_ConnClearQuery(Ns_Conn *conn)
{
    Conn           *connPtr = (Conn *) conn;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    FormFile	  *filePtr;

    if (conn == NULL || connPtr->query == NULL) {
        return;
    }
    Ns_SetFree(connPtr->query);
    connPtr->query = NULL;
    connPtr->queryEncoding = NULL;
    hPtr = Tcl_FirstHashEntry(&connPtr->files, &search);
    while (hPtr != NULL) {
	filePtr = Tcl_GetHashValue(hPtr);
	Ns_SetFree(filePtr->hdrs);
	ns_free(filePtr);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&connPtr->files);
    Tcl_InitHashTable(&connPtr->files, TCL_STRING_KEYS);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_QueryToSet --
 *
 *	Parse query data into an Ns_Set 
 *
 * Results:
 *	NS_OK. 
 *
 * Side effects:
 *	Will add data to set without any UTF conversion.
 *
 *----------------------------------------------------------------------
 */

int
Ns_QueryToSet(char *query, Ns_Set *set)
{
    ParseQuery(query, set, NULL);
    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclParseQueryObjCmd --
 *
 *	This procedure implements the AOLserver Tcl
 *
 *	    ns_parsequery querystring
 *
 *	command.
 *
 * Results:
 *	The Tcl result is a Tcl set with the parsed name-value pairs from
 *	the querystring argument
 *
 * Side effects:
 *	None external.
 *
 *----------------------------------------------------------------------
 */

int
NsTclParseQueryObjCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj **objv)
{
    Ns_Set *set;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "querystring");
	return TCL_ERROR;
    }
    set = Ns_SetCreate(NULL);
    if (Ns_QueryToSet(Tcl_GetString(objv[1]), set) != NS_OK) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"could not parse: \"", Tcl_GetString(objv[1]), "\"", NULL);
	Ns_SetFree(set);
	return TCL_ERROR;
    }
    return Ns_TclEnterSet(interp, set, NS_TCL_SET_DYNAMIC);
}


/*
 *----------------------------------------------------------------------
 *
 * NsCheckQuery --
 *
 *	Validate the connection query was decoded with the current
 *	URL encoding.
 *
 * Results:
 *	1 if query is valid, 0 otherwise.
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

int
NsCheckQuery(Ns_Conn *conn)
{
    Conn *connPtr = (Conn *) conn;

    if (connPtr->queryEncoding != Ns_ConnGetUrlEncoding(conn)) {
	return 0;
    }
    return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * ParseQuery --
 *
 *	Parse the given form string for URL encoded key=value pairs,
 *	converting to UTF if given encoding is not NULL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

static void
ParseQuery(char *form, Ns_Set *set, Tcl_Encoding encoding)
{
    char *p, *k, *v;
    Tcl_DString      kds, vds;

    Tcl_DStringInit(&kds);
    Tcl_DStringInit(&vds);
    p = form;
    while (p != NULL) {
	k = p;
	p = strchr(p, '&');
	if (p != NULL) {
	    *p = '\0';
	}
	v = strchr(k, '=');
	if (v != NULL) {
	    *v = '\0';
	}
        Ns_DStringTrunc(&kds, 0);
	k = Ns_DecodeUrlWithEncoding(&kds, k, encoding);
	if (v != NULL) {
            Ns_DStringTrunc(&vds, 0);
	    Ns_DecodeUrlWithEncoding(&vds, v+1, encoding);
	    *v = '=';
	    v = vds.string;
	}
	Ns_SetPut(set, k, v);
	if (p != NULL) {
	    *p++ = '&';
	}
    }
    Tcl_DStringFree(&kds);
    Tcl_DStringFree(&vds);
}


/*
 *----------------------------------------------------------------------
 *
 * ParseMulitInput --
 *
 *	Parse the a multipart form input.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Records offset, lengths for files.
 *
 *----------------------------------------------------------------------
 */

static void
ParseMultiInput(Conn *connPtr, Tcl_Encoding encoding, char *start, char *end)
{
    Tcl_DString kds, vds;
    Tcl_HashEntry *hPtr;
    FormFile	  *filePtr;
    char *s, *e, *ks, *ke, *fs, *fe, save, saveend;
    char *key, *value, *disp;
    Ns_Set *set;
    int new;

    Tcl_DStringInit(&kds);
    Tcl_DStringInit(&vds);
    set = Ns_SetCreate(NULL);

    /*
     * Trim off the trailing \r\n and null terminate the input.
     */

    if (end > start && end[-1] == '\n') --end;
    if (end > start && end[-1] == '\r') --end;
    saveend = *end;
    *end = '\0';

    /*
     * Parse header lines
     */

    ks = fs = NULL;
    while ((e = strchr(start, '\n')) != NULL) {
	s = start;
	start = e + 1;
	if (e > s && e[-1] == '\r') {
	    --e;
	}
	if (s == e) {
	    break;
	}
	save = *e;
	*e = '\0';
	Ns_ParseHeader(set, s, ToLower);
	*e = save;
    }

    /*
     * Look for valid disposition header.
     */

    disp = Ns_SetGet(set, "content-disposition");
    if (disp != NULL && GetValue(disp, "name=", &ks, &ke)) {
	key = Ext2Utf(&kds, ks, ke-ks, encoding);
	if (!GetValue(disp, "filename=", &fs, &fe)) {
	    value = Ext2Utf(&vds, start, end-start, encoding);
	} else {
	    value = Ext2Utf(&vds, fs, fe-fs, encoding);
	    hPtr = Tcl_CreateHashEntry(&connPtr->files, key, &new);
	    if (new) {
		filePtr = ns_malloc(sizeof(FormFile));
		filePtr->hdrs = set;
	    	filePtr->off = start - connPtr->content;
		filePtr->len = end - start;
		Tcl_SetHashValue(hPtr, filePtr);
	    	set = NULL;
	    }
	}
	Ns_SetPut(connPtr->query, key, value);
    }

    /*
     * Restore the end marker.
     */

    *end = saveend;
    Tcl_DStringFree(&kds);
    Tcl_DStringFree(&vds);
    if (set != NULL) {
	Ns_SetFree(set);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * GetBoundary --
 *
 *	Copy multipart/form-data boundy string, if any.
 *
 * Results:
 *	1 if boundy copied, 0 otherwise.
 *
 * Side effects:
 *	Copies boundry string to given dstring.
 *
 *----------------------------------------------------------------------
 */

static int
GetBoundary(Tcl_DString *dsPtr, Ns_Conn *conn)
{
    char *type, *bs, *be;

    type = Ns_SetIGet(conn->headers, "content-type");
    if (type != NULL
	&& Ns_StrCaseFind(type, "multipart/form-data") != NULL
	&& (bs = Ns_StrCaseFind(type, "boundary=")) != NULL) {
	bs += 9;
	be = bs;
	while (*be && !isspace(UCHAR(*be))) {
	    ++be;
	}
	Tcl_DStringAppend(dsPtr, "--", 2);
	Tcl_DStringAppend(dsPtr, bs, be-bs);
	return 1;
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * NextBoundary --
 *
 *	Locate the next form boundry.
 *
 * Results:
 *	Pointer to start of next input field or NULL on end of fields.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
NextBoundry(Tcl_DString *dsPtr, char *s, char *e)
{
    char c, sc, *find;
    size_t len;

    find = dsPtr->string;
    c = *find++;
    len = dsPtr->length-1;
    e -= len;
    do {
	do {
	    sc = *s++;
	    if (s > e) {
		return NULL;
	    }
	} while (sc != c);
    } while (strncmp(s, find, len) != 0);
    s--;
    return s;
}


/*
 *----------------------------------------------------------------------
 *
 * GetValue --
 *
 *	Determine start and end of a multipart form input value.
 *
 * Results:
 *	1 if attribute found and value parsed, 0 otherwise.
 *
 * Side effects:
 *	Start and end are stored in given pointers.
 *
 *----------------------------------------------------------------------
 */

static int
GetValue(char *hdr, char *att, char **vsPtr, char **vePtr)
{
    char *s, *e;

    s = Ns_StrCaseFind(hdr, att);
    if (s == NULL) {
	return 0;
    }
    s += strlen(att);
    e = s;
    if (*s != '"' && *s != '\'') {
	/* NB: End of unquoted att=value is next space. */
	while (*e && !isspace(UCHAR(*e))) {
	    ++e;
	}
    } else {
	/* NB: End of quoted att="value" is next quote. */
	++e;
	while (*e && *e != *s) {
	    ++e;
	}
	++s;
    }
    *vsPtr = s;
    *vePtr = e;
    return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * Ext2Utf --
 *
 *	Convert input string to UTF.
 *
 * Results:
 *	Pointer to converted string.
 *
 * Side effects:
 *	Converted string is copied to given dstring, overwriting
 *	any previous content.
 *
 *----------------------------------------------------------------------
 */

static char *
Ext2Utf(Tcl_DString *dsPtr, char *start, int len, Tcl_Encoding encoding)
{
    if (encoding == NULL) {
	Tcl_DStringTrunc(dsPtr, 0);
	Tcl_DStringAppend(dsPtr, start, len);
    } else {
	/* NB: ExternalToUtfDString will re-init dstring. */
	Tcl_DStringFree(dsPtr);
	Tcl_ExternalToUtfDString(encoding, start, len, dsPtr);
    }
    return dsPtr->string;
}
