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
 * adpparse.c --
 *
 *	ADP parser.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

#define SERV_STREAM	1
#define SERV_RUNAT	2
#define SERV_NOTTCL	4

#define APPEND		"ns_adp_append "
#define APPEND_LEN	(sizeof(APPEND)-1)

/*
 * The following structure maintains proc and adp registered tags.
 */

typedef struct {
    char          *tag;     /* The name of the tag (e.g., "netscape") */
    char          *endtag;  /* The closing tag or null (e.g., "/netscape")*/
    char          *string;  /* Proc (e.g., "ns_adp_netscape") or ADP string. */
    int		   isproc;  /* Arg is a proc, not ADP string. */
} Tag;

/*
 * Local functions defined in this file
 */

static void AppendBlock(AdpParse *parsePtr, char *s, char *e, int type);
static void Parse(AdpParse *parsePtr, NsServer *servPtr, char *utf);
static Tag   *GetRegTag(NsServer *servPtr, char *tag);
static int	 RegisterCmd(ClientData arg, Tcl_Interp *interp,
		    int argc, char **argv, int isproc);


/*
 *----------------------------------------------------------------------
 *
 * NsTclRegisterTagCmd, NsTclRegisterAdpCmd --
 *
 *	Register an ADP proc or string tag.
 *	
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	An ADP tag may be added to the hashtable.
 *
 *----------------------------------------------------------------------
 */

int
NsTclRegisterTagCmd(ClientData arg, Tcl_Interp *interp, int argc,
		    char **argv)
{
    return RegisterCmd(arg, interp, argc, argv, 1);
}

int
NsTclRegisterAdpTagCmd(ClientData arg, Tcl_Interp *interp, int argc,
		    char **argv)
{
    return RegisterCmd(arg, interp, argc, argv, 0);
}

static int
RegisterCmd(ClientData arg, Tcl_Interp *interp, int argc,
		    char **argv, int isproc)
{
    NsInterp       *itPtr = arg;
    NsServer	   *servPtr = itPtr->servPtr;
    char           *tag, *endtag, *string;
    Tcl_HashEntry  *hPtr;
    int             new;
    Tag         *tagPtr;
    
    if (argc != 4 && argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 argv[0], " tag ?endtag? proc\"", NULL);
	return TCL_ERROR;
    }
    tag = argv[1];
    string = argv[argc-1];
    endtag = (argc == 3 ? NULL : argv[2]);

    Ns_MutexLock(&servPtr->adp.taglock);
    hPtr = Tcl_CreateHashEntry(&servPtr->adp.tags, tag, &new);
    if (new) {
    	tagPtr = ns_malloc(sizeof(Tag));
    	tagPtr->tag = ns_strdup(tag);
    	tagPtr->endtag = endtag ? ns_strdup(endtag) : NULL;
    	tagPtr->string = ns_strdup(string);
    	tagPtr->isproc = isproc;
    	Tcl_SetHashValue(hPtr, (void *) tagPtr);
    }
    Ns_MutexUnlock(&servPtr->adp.taglock);
    if (!new) {
	Tcl_AppendResult(interp, "ADP tag \"", tag, "\" already registered.",
			 NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsAdpParse --
 *
 *	Parse a string of ADP.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Given Parse structure filled in with copy of parsed ADP.
 *
 *----------------------------------------------------------------------
 */

void
NsAdpParse(AdpParse *parsePtr, NsServer *servPtr, char *utf)
{
    char *s, *e;

    /*
     * Initialize the parse structure.
     */

    Tcl_DStringInit(&parsePtr->hdr);
    Tcl_DStringInit(&parsePtr->text);
    parsePtr->code.nscripts = parsePtr->code.nblocks = 0;

    /*
     * Scan for <% ... %> sequences which take precedence over
     * other tags.
     */

    while ((s = strstr(utf, "<%")) && (e = strstr(s, "%>"))) {
	/*
	 * Parse text preceeding the script.
	 */

	*s = '\0';
	Parse(parsePtr, servPtr, utf);
	*s = '<';
	if (s[2] != '=') {
	    AppendBlock(parsePtr, s + 2, e, 's');
	} else {
	    AppendBlock(parsePtr, s + 3, e, 'S');
	}
	utf = e + 2;
    }

    /*
     * Parse the remaining text.
     */

    Parse(parsePtr, servPtr, utf);;

    /*
     * Complete the parse code structure.
     */

    parsePtr->code.len = (int *) parsePtr->hdr.string;
    parsePtr->code.base = parsePtr->text.string;
}


/*
 *----------------------------------------------------------------------
 *
 * FindRegTag --
 *
 *	Return the Tag structure for the given tag.
 *
 * Results:
 *	Either a pointer to the requested regtag or null if none exists.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tag *
FindRegTag(NsServer *servPtr, char *tag)
{
    Tcl_HashEntry *hPtr;
    
    Ns_MutexLock(&servPtr->adp.taglock);
    hPtr = Tcl_FindHashEntry(&servPtr->adp.tags, tag);
    Ns_MutexUnlock(&servPtr->adp.taglock);
    if (hPtr == NULL) {
	return NULL;
    }
    return Tcl_GetHashValue(hPtr) ;
}


/*
 *----------------------------------------------------------------------
 *
 * AppendBlock --
 *
 *	Add a text or script block to the output buffer.
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
AppendBlock(AdpParse *parsePtr, char *s, char *e, int type)
{
    int len;

    if (s < e) {
	++parsePtr->code.nblocks;
	len = e - s;
	if (type == 'S') {
	    len += APPEND_LEN;
	    Tcl_DStringAppend(&parsePtr->text, APPEND, APPEND_LEN);
	}
	Tcl_DStringAppend(&parsePtr->text, s, e - s);
	if (type != 't') {
	    ++parsePtr->code.nscripts;
	    len = -len;
	}
	Tcl_DStringAppend(&parsePtr->hdr, (char *) &len, sizeof(len));
    }
}


/*
 *----------------------------------------------------------------------
 *
 * GetTag --
 *
 *	Copy tag name in lowercase to given dstring.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Start of att=val pairs, if any, are set is aPtr if not null.
 *
 *----------------------------------------------------------------------
 */

static void
GetTag(Tcl_DString *dsPtr, char *s, char *e, char **aPtr)
{
    char *t;

    ++s;
    while (s < e && isspace(UCHAR(*s))) {
	++s;
    }
    t = s;
    while (s < e  && !isspace(UCHAR(*s))) {
	++s;
    }
    Tcl_DStringTrunc(dsPtr, 0);
    Tcl_DStringAppend(dsPtr, t, s - t);
    if (aPtr != NULL) {
	while (s < e && isspace(UCHAR(*s))) {
	    ++s;
	}
	*aPtr = s;
    }
    dsPtr->length = Tcl_UtfToLower(dsPtr->string);
}


/*
 *----------------------------------------------------------------------
 *
 * ParseAtts --
 *
 *	Parse tag attributes, either looking for known <script>
 *	pairs or copying cleaned up pairs to given dstring.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Flags in given servPtr are updated and/or data copied to given
 *	dstring.
 *
 *----------------------------------------------------------------------
 */

static void
ParseAtts(char *s, char *e, int *servPtr, Tcl_DString *attsPtr)
{
    char    *vs, *ve, *as, *ae, end, vsave, asave;
    
    if (servPtr != NULL) {
	*servPtr = 0;
    }
    while (s < e) {
	/*
	 * Trim attribute name.
	 */

	while (s < e && isspace(UCHAR(*s))) {
	    ++s;
	}
	as = s;
	while (s < e && !isspace(UCHAR(*s)) && *s != '=') {
	    ++s;
	}
	ae = s;
	while (s < e && isspace(UCHAR(*s))) {
	    ++s;
	}

	if (*s++ != '=') {
	    /*
	     * Use attribute name as value.
	     */

	    vs = as;
	} else {
	    /*
	     * Trim spaces and/or quotes from value.
	     */

	    while (s < e && isspace(UCHAR(*s))) {
		++s;
	    }
	    vs = s;
	    while (s < e && !isspace(UCHAR(*s))) {
		++s;
	    }
	    ve = s;
	    end = *vs;
	    if (end != '=' && end != '\'' && end != '"') {
		end = 0;
	    }
	    if (end && ve > vs && ve[-1] == end) {
		++vs;
		--ve;
	    }
	    vsave = *ve;
	    *ve = '\0';
	}
	asave = *ae;
	*ae = '\0';

	/*
	 * Append attributes or scan for special <script> pairs.
	 */

	if (attsPtr != NULL) {
	    Tcl_DStringAppendElement(attsPtr, as);
	    Tcl_DStringAppendElement(attsPtr, vs);
	}
	if (servPtr != NULL && vs != as) {
	    if (STRIEQ(as, "runat") && STRIEQ(vs, "server")) {
		*servPtr |= SERV_RUNAT;
	    } else if (STRIEQ(as, "language") && !STRIEQ(vs, "tcl")) {
		*servPtr |= SERV_NOTTCL;
	    } else if (STRIEQ(as, "stream") && STRIEQ(vs, "on")) {
		*servPtr |= SERV_STREAM;
	    }
	}

	/*
	 * Restore strings.
	 */

	*ae = asave;
	if (vs != as) {
	    *ve = vsave;
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * IsServer --
 *
 *	Parse attributes for known <script> attributes.
 *
 * Results:
 *	1 if attributes indicate valid server-side script, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
IsServer(char *tag, char *as, char *ae, int *streamPtr)
{
    int serv;

    if (as < ae && STREQ(tag, "script")) {
	ParseAtts(as, ae, &serv, NULL);
	if ((serv & SERV_RUNAT) && !(serv & SERV_NOTTCL)) {
	    *streamPtr = (serv & SERV_STREAM);
	    return 1;
	}
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * AppendTag --
 *
 *	Append tag script block.
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
AppendTag(AdpParse *parsePtr, Tag *tagPtr, char *as, char *ae, char *se)
{
    Tcl_DString script;
    char save;

    Tcl_DStringInit(&script);
    Tcl_DStringAppend(&script, "ns_adp_append [", -1);
    if (!tagPtr->isproc) {
	Tcl_DStringAppend(&script, "ns_adp_eval", -1);
    }
    Tcl_DStringAppendElement(&script, tagPtr->string);
    if (se > ae) {
	save = *se;
	*se = '\0';
	Tcl_DStringAppendElement(&script, ae + 1);
	*se = save;
    }
    Tcl_DStringAppend(&script, " [ns_set create", -1);
    Tcl_DStringAppendElement(&script, tagPtr->tag);
    ParseAtts(as, ae, NULL, &script);
    Tcl_DStringAppend(&script, "]]", 2);
    AppendBlock(parsePtr, script.string, script.string+script.length, 's');
    Tcl_DStringFree(&script);
}


/*
 *----------------------------------------------------------------------
 *
 * Parse --
 *
 *	Parse UTF text for <script> and/or registered tags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Blocks will be appended to the given parsePtr.
 *
 *----------------------------------------------------------------------
 */

static void
Parse(AdpParse *parsePtr, NsServer *servPtr, char *utf)
{
    Tag            *tagPtr;
    char           *ss, *se, *s, *e, *a, *as, *ae, *t;
    int             level, state, stream, streamdone;
    Tcl_DString     tag;

    Tcl_DStringInit(&tag);
    t = utf;
    streamdone = 0;
    state = 0;
    while ((s = strchr(utf, '<')) && (e = strchr(s, '>'))) {
	/*
	 * Process the tag depending on the current state.
	 */

	switch (state) {
	case 0:
	    /*
	     * Look for possible <script> or <tag>.
	     */

	    GetTag(&tag, s, e, &a);
	    if (IsServer(tag.string, a, e, &stream)) {
		/*
		 * Record start of script.
		 */

		ss = s;
		se = e + 1;
		state = 1;
	    } else {
		tagPtr = FindRegTag(servPtr, tag.string);
		if (tagPtr) {
		    if (tagPtr->endtag == NULL) {
			/*
			 * Output simple no-end registered tag.
			 */

			AppendBlock(parsePtr, t, s, 't');
			t = e + 1;
			AppendTag(parsePtr, tagPtr, a, e, NULL);
		    } else {
			/*
			 * Record start of registered tag.
			 */

			ss = s;
			as = a;
			ae = e;
			level = 1;
			state = 2;
		    }
		}
	    }
	    break;

	case 1:
	    GetTag(&tag, s, e, NULL);
	    if (STREQ(tag.string, "/script")) {
		/*
		 * Output end of script.
		 */

		AppendBlock(parsePtr, t, ss, 't');
		t = e + 1;
		if (stream && !streamdone) {
		    AppendBlock(parsePtr, "ns_adp_stream", NULL, 's');
		    streamdone = 1;
		}
		AppendBlock(parsePtr, se, s, 's');
		state = 0;
	    }
	    break;

	case 2:
	    GetTag(&tag, s, e, NULL);
	    if (STRIEQ(tag.string, tagPtr->tag)) {
		/*
		 * Increment register tag nesting level.
		 */

		++level;
	    } else if (STRIEQ(tag.string, tagPtr->endtag)) {
		--level;
		if (level == 0) {
		    /*
		     * Dump out registered tag.
		     */

		    AppendBlock(parsePtr, t, ss, 't');
		    t = e + 1;
		    AppendTag(parsePtr, tagPtr, as, ae, s);
		    state = 0;
		}
	    }
	    break;
	}
	utf = e + 1;
    }

    /*
     * Append the remaining text block.
     */

    AppendBlock(parsePtr, t, t + strlen(t), 't');
    Tcl_DStringFree(&tag);
}
