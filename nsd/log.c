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
 * log.c --
 *
 *	Manage the server log file.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

/*
 * The following struct maintains per-thread
 * cached formatted time strings and log buffers.
 */

typedef struct Cache {
    int		hold;
    int		count;
    time_t	gtime;
    time_t	ltime;
    char	gbuf[100];
    char	lbuf[100];
    Ns_DString  buffer;
} Cache;

/*
 * Local functions defined in this file
 */

static int    LogReOpen(void);
static void   Log(Ns_LogSeverity severity, char *fmt, va_list ap);
static void   LogWrite(void);
static Ns_Callback LogFlush;

static Cache *LogGetCache(void);
static Ns_TlsCleanup LogFreeCache;
static void   LogFlushCache(Cache *cachePtr);
static char  *LogTime(Cache *cachePtr, int gmtoff, long *usecPtr);
static int    LogStart(Cache *cachePtr, Ns_LogSeverity severity);
static void   LogEnd(Cache *cachePtr);

/*
 * Static variables defined in this file
 */

static Ns_Mutex lock;
static Ns_Cond cond;
static Ns_DString buffer;
static int buffered = 0;
static int flushing = 0;
static int nbuffered = 0;
static Ns_Tls tls;


/*
 *----------------------------------------------------------------------
 *
 * NsInitLog --
 *
 *	Initialize the log API and TLS slot.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

void
NsInitLog(void)
{
    Ns_MutexInit(&lock);
    Ns_CondInit(&cond);
    Ns_MutexSetName(&lock, "ns:log");
    Ns_DStringInit(&buffer);
    Ns_TlsAlloc(&tls, LogFreeCache);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_InfoErrorLog --
 *
 *	Returns the filename of the log file. 
 *
 * Results:
 *	Log file name or NULL if none. 
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

char *
Ns_InfoErrorLog(void)
{
    return nsconf.log.file;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_LogRoll --
 *
 *	Signal handler for SIG_HUP which will roll the files. Also a 
 *	tasty snack from Stuckey's. 
 *
 * Results:
 *	NS_OK/NS_ERROR 
 *
 * Side effects:
 *	Will rename the log file and reopen it. 
 *
 *----------------------------------------------------------------------
 */

int
Ns_LogRoll(void)
{
    if (nsconf.log.file != NULL) {
        if (access(nsconf.log.file, F_OK) == 0) {
            Ns_RollFile(nsconf.log.file, nsconf.log.maxback);
        }
        Ns_Log(Notice, "log: re-opening log file '%s'", nsconf.log.file);
        if (LogReOpen() != NS_OK) {
	    return NS_ERROR;
	}
    }
    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_Log --
 *
 *	Spit a message out to the server log. 
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

void
Ns_Log(Ns_LogSeverity severity, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    Log(severity, fmt, ap);
    va_end(ap);
}

/* NB: For binary compatibility with previous releases. */

void
ns_serverLog(Ns_LogSeverity severity, char *fmt, va_list *vaPtr)
{
    Log(severity, fmt, *vaPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_Fatal --
 *
 *	Spit a message out to the server log with severity level of 
 *	Fatal, and then terminate the nsd process. 
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	WILL CAUSE AOLSERVER TO EXIT! 
 *
 *----------------------------------------------------------------------
 */

void
Ns_Fatal(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    Log(Fatal, fmt, ap);
    va_end(ap);
    _exit(1);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_LogTime --
 *
 *	Copy a GMT date and time string useful for common log
 *	format enties (e.g., nslog).
 *
 * Results:
 *	Pointer to given buffer.
 *
 * Side effects:
 *	Will put data into timeBuf, which must be at least 41 bytes 
 *	long. 
 *
 *----------------------------------------------------------------------
 */

char *
Ns_LogTime(char *timeBuf)
{
    strcpy(timeBuf, LogTime(LogGetCache(), 1, NULL));
    return timeBuf;
}


/*
 *----------------------------------------------------------------------
 *
 * NsLogOpen --
 *
 *	Open the log file. Adjust configurable parameters, too. 
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	Configures this module to use the newly opened log file.
 *	If LogRoll is turned on in the config file, then it registers
 *	a signal callback.
 *
 *----------------------------------------------------------------------
 */

void
NsLogOpen(void)
{
    /*
     * Initialize log buffering.
     */

    if (nsconf.log.flags & LOG_BUFFER) {
	buffered = 1;
	Ns_RegisterAtShutdown(LogFlush, (void *) 1);
	if (nsconf.log.flushint > 0) {
	    Ns_ScheduleProc(LogFlush, (void *) 0, 0, nsconf.log.flushint);
	}
    }

    /*
     * Open the log and schedule the signal roll.
     */

    if (LogReOpen() != NS_OK) {
	Ns_Fatal("log: failed to open server log '%s': '%s'", 
		 nsconf.log.file, strerror(errno));
    }
    if (nsconf.log.flags & LOG_ROLL) {
	Ns_RegisterAtSignal((Ns_Callback *) Ns_LogRoll, NULL);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclLogRollCmd --
 *
 *	Implements ns_logroll. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclLogRollCmd(ClientData dummy, Tcl_Interp *interp, int argc, char **argv)
{
    if (Ns_LogRoll() != NS_OK) {
	Tcl_SetResult(interp, "could not roll server log", TCL_STATIC);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclLogRollObjCmd --
 *
 *	Implements ns_logroll as obj command. 
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclLogRollObjCmd(ClientData arg, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    if (Ns_LogRoll() != NS_OK) {
	Tcl_SetResult(interp, "could not roll server log", TCL_STATIC);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclLogCtlObjCmd --
 *
 *	Implements ns_logctl command to manage log buffering
 *	and release.
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclLogCtlObjCmd(ClientData arg, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
    int len;
    Cache *cachePtr;
    static CONST char *opts[] = {
	"hold",
	"count",
	"get",
	"peek",
	"flush",
	"release",
	"truncate",
	NULL
    };
    enum {
	CHoldIdx,
	CCountIdx,
	CGetIdx,
	CPeekIdx,
	CFlushIdx,
	CReleaseIdx,
	CTruncIdx
    } opt;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg?");
    	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], opts, "option", 0,
			    (int *) &opt) != TCL_OK) {
	return TCL_ERROR;
    }
    cachePtr = LogGetCache();
    switch (opt) {
    case CHoldIdx:
	cachePtr->hold = 1;
	break;

    case CPeekIdx:
	Tcl_SetResult(interp, cachePtr->buffer.string, TCL_VOLATILE);
	break;

    case CGetIdx:
	Tcl_SetResult(interp, cachePtr->buffer.string, TCL_VOLATILE);
	Ns_DStringFree(&cachePtr->buffer);
	break;

    case CReleaseIdx:
	cachePtr->hold = 0;
	/* FALLTHROUGH */

    case CFlushIdx:
	LogFlushCache(cachePtr);
	break;

    case CCountIdx:
	Tcl_SetIntObj(Tcl_GetObjResult(interp), cachePtr->count);
	break;

    case CTruncIdx:
	len = 0;
	if (objc > 2 && Tcl_GetIntFromObj(interp, objv[2], &len) != TCL_OK) {
	    return TCL_ERROR;
	}
	Ns_DStringTrunc(&cachePtr->buffer, len);
	break;
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclLogObjCmd --
 *
 *	Implements ns_log as obj command.
 *
 * Results:
 *	Tcl result. 
 *
 * Side effects:
 *	See docs. 
 *
 *----------------------------------------------------------------------
 */

int
NsTclLogObjCmd(ClientData arg, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
    Ns_LogSeverity severity;
    Cache *cachePtr;
    char *severitystr;
    int i;

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "severity string ?string ...?");
    	return TCL_ERROR;
    }
    severitystr = Tcl_GetString(objv[1]);
    cachePtr = LogGetCache();
    if (STRIEQ(severitystr, "notice")) {
	severity = Notice;
    } else if (STRIEQ(severitystr, "warning")) {
	severity = Warning;
    } else if (STRIEQ(severitystr, "error")) {
	severity = Error;
    } else if (STRIEQ(severitystr, "fatal")) {
	severity = Fatal;
    } else if (STRIEQ(severitystr, "bug")) {
	severity = Bug;
    } else if (STRIEQ(severitystr, "debug")) {
	severity = Debug;
    } else if (Tcl_GetIntFromObj(NULL, objv[1], &i) == TCL_OK) {
	severity = i;
    } else {
	Tcl_AppendResult(interp, "unknown severity: \"", severitystr,
	    "\": should be notice, warning, error, "
	    "fatal, bug, debug or integer value", NULL);
	return TCL_ERROR;
    }
    if (LogStart(cachePtr, severity)) {
	for (i = 2; i < objc; ++i) {
	    Ns_DStringVarAppend(&cachePtr->buffer,
		Tcl_GetString(objv[i]), i > 2 ? " " : NULL, NULL);
	}
	LogEnd(cachePtr);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Log --
 *
 *	Add an entry to the log file if the severity is not surpressed.
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	May write immediately or later through buffer.
 *
 *----------------------------------------------------------------------
 */

static void
Log(Ns_LogSeverity severity, char *fmt, va_list ap)
{
    Cache *cachePtr;

    cachePtr = LogGetCache();
    if (LogStart(cachePtr, severity)) {
	Ns_DStringVPrintf(&cachePtr->buffer, fmt, ap);
	LogEnd(cachePtr);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * LogStart --
 *
 *	Start a log entry.
 *
 * Results:
 *	1 if log started and should be written, 0 if given severity
 *	is surpressed.
 *
 * Side effects:
 *	May append log header to given dstring.
 *
 *----------------------------------------------------------------------
 */

static int
LogStart(Cache *cachePtr, Ns_LogSeverity severity)
{
    char *severityStr, buf[10];
    long usec;

    switch (severity) {
	case Notice:
	    if (nsconf.log.flags & LOG_NONOTICE) {
		return 0;
	    }
	    severityStr = "Notice";
	    break;
        case Warning:
	    severityStr = "Warning";
	    break;
	case Error:
	    severityStr = "Error";
	    break;
	case Fatal:
	    severityStr = "Fatal";
	    break;
	case Bug:
	    severityStr = "Bug";
	    break;
	case Debug:
	    if (!(nsconf.log.flags & LOG_DEBUG)) {
		return 0;
	    }
	    severityStr = "Debug";
	    break;
	case Dev:
	    if (!(nsconf.log.flags & LOG_DEV)) {
		return 0;
	    }
	    severityStr = "Dev";
	    break;
	default:
	    if (severity > nsconf.log.maxlevel) {
		return 0;
	    }
	    sprintf(buf, "Level%d", severity);
	    severityStr = buf;
	    break;
    }
    Ns_DStringAppend(&cachePtr->buffer, LogTime(cachePtr, 0, &usec));
    if (nsconf.log.flags & LOG_USEC) {
    	Ns_DStringTrunc(&cachePtr->buffer, cachePtr->buffer.length-1);
	Ns_DStringPrintf(&cachePtr->buffer, ".%ld]", usec);
    }
    Ns_DStringPrintf(&cachePtr->buffer, "[%d.%d][%s] %s: ",
	Ns_InfoPid(), Ns_ThreadId(), Ns_ThreadGetName(), severityStr);
    if (nsconf.log.flags & LOG_EXPAND) {
	Ns_DStringAppend(&cachePtr->buffer, "\n    ");
    }
    return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * LogEnd --
 *
 *	Complete a log entry and either append to buffer or write
 *	immediately.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May flush log.
 *
 *----------------------------------------------------------------------
 */

static void
LogEnd(Cache *cachePtr)
{
    Ns_DStringNAppend(&cachePtr->buffer, "\n", 1);
    if (nsconf.log.flags & LOG_EXPAND) {
	Ns_DStringNAppend(&cachePtr->buffer, "\n", 1);
    }
    ++cachePtr->count;
    if (!cachePtr->hold) {
    	LogFlushCache(cachePtr);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * LogFlushCache --
 *
 *	Flush per-thread log entries to buffer or open file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May write to log file.
 *
 *----------------------------------------------------------------------
 */

static void
LogFlushCache(Cache *cachePtr)
{
    Ns_DString *dsPtr = &cachePtr->buffer;

    Ns_MutexLock(&lock);
    if (!buffered) {
	Ns_MutexUnlock(&lock);
	(void) write(2, dsPtr->string, dsPtr->length);
    } else {
	while (flushing) {
	    Ns_CondWait(&cond, &lock);
	}
	Ns_DStringNAppend(&buffer, dsPtr->string, dsPtr->length);
	if (nbuffered++ > nsconf.log.maxbuffer) {
	    LogWrite();
	}
	Ns_MutexUnlock(&lock);
    }
    cachePtr->count = 0;
    Ns_DStringFree(dsPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * LogFlush --
 *
 *	Flush the buffered log, either from the scheduled proc timeout
 *	or at shutdown.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May write to log and disables buffering at shutdown.
 *
 *----------------------------------------------------------------------
 */

static void
LogFlush(void *arg)
{
    int exit = (int) arg;

    Ns_MutexLock(&lock);

    /*
     * Wait for current flushing, if any, to complete.
     */

    while (flushing) {
	Ns_CondWait(&cond, &lock);
    }
    if (nbuffered > 0) {
	LogWrite();
    }

    /*
     * Disable further buffering at exit time.
     */

    if (exit) {
	buffered = 0;
    }
    Ns_MutexUnlock(&lock);
}


/*
 *----------------------------------------------------------------------
 *
 * LogWrite --
 *
 *	Write current buffer to log file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Will write data.
 *
 *----------------------------------------------------------------------
 */

static void
LogWrite(void)
{
    flushing = 1;
    Ns_MutexUnlock(&lock);
    (void) write(2, buffer.string, buffer.length);
    Ns_DStringSetLength(&buffer, 0);
    Ns_MutexLock(&lock);
    flushing = 0;
    nbuffered = 0;
    Ns_CondBroadcast(&cond);
}


/*
 *----------------------------------------------------------------------
 *
 * LogReOpen --
 *
 *	Open the log file name specified in the 'logFile' global. If 
 *	it's successfully opened, make that file the sink for stdout 
 *	and stderr too. 
 *
 * Results:
 *	NS_OK/NS_ERROR 
 *
 * Side effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

static int
LogReOpen(void)
{
    int fd; 
    int status;

    status = NS_OK;
    fd = open(nsconf.log.file, O_WRONLY|O_APPEND|O_CREAT, 0644);
    if (fd < 0) {
        Ns_Log(Error, "log: failed to re-open log file '%s': '%s'",
	       nsconf.log.file, strerror(errno));
        status = NS_ERROR;
    } else {
	/*
	 * Route stderr to the file
	 */
	
        if (fd != STDERR_FILENO && dup2(fd, STDERR_FILENO) == -1) {
            fprintf(stdout, "dup2(%s, STDERR_FILENO) failed: %s\n",
		nsconf.log.file, strerror(errno));
            status = NS_ERROR;
        }
	
	/*
	 * Route stdout to the file
	 */
	
        if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
            Ns_Log(Error, "log: failed to route stdout to file: '%s'",
		   strerror(errno));
            status = NS_ERROR;
        }
	
	/*
	 * Clean up dangling 'open' reference to the fd
	 */
	
        if (fd != STDERR_FILENO && fd != STDOUT_FILENO) {
            close(fd);
        }
    }
    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * LogTime --
 *
 *	Get formatted local or gmt log time.
 *
 * Results:
 *	Pointer to per-thread buffer.
 *
 * Side effects:
 *	Buffer is updated if time has changed since check.
 *
 *----------------------------------------------------------------------
 */

static char *
LogTime(Cache *cachePtr, int gmtoff, long *usecPtr)
{
    time_t   *tp;
    struct tm *ptm;
    int gmtoffset, n, sign;
    char *bp;
    Ns_Time now;

    if (gmtoff) {
	tp = &cachePtr->gtime;
	bp = cachePtr->gbuf;
    } else {
	tp = &cachePtr->ltime;
	bp = cachePtr->lbuf;
    }
    Ns_GetTime(&now);
    if (*tp != now.sec) {
	*tp = now.sec;
	ptm = ns_localtime(&now.sec);
	n = strftime(bp, 32, "[%d/%b/%Y:%H:%M:%S", ptm);
	if (!gmtoff) {
	    bp[n++] = ']';
	    bp[n] = '\0';
	} else {
#ifdef NO_TIMEZONE
	    gmtoffset = ptm->tm_gmtoff / 60;
#else
	    gmtoffset = -timezone / 60;
	    if (daylight && ptm->tm_isdst) {
		gmtoffset += 60;
	    }
#endif
	    if (gmtoffset < 0) {
		sign = '-';
		gmtoffset *= -1;
	    } else {
		sign = '+';
	    }
	    sprintf(bp + n, 
		    " %c%02d%02d]", sign, gmtoffset / 60, gmtoffset % 60);
	}
    }
    if (usecPtr != NULL) {
	*usecPtr = now.usec;
    }
    return bp;
}


/*
 *----------------------------------------------------------------------
 *
 * LogGetCache --
 *
 *	Get the per-thread Cache struct.
 *
 * Results:
 *	Pointer to per-thread struct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Cache *
LogGetCache(void)
{
    Cache *cachePtr;

    cachePtr = Ns_TlsGet(&tls);
    if (cachePtr == NULL) {
	cachePtr = ns_calloc(1, sizeof(Cache));
	Ns_DStringInit(&cachePtr->buffer);
	Ns_TlsSet(&tls, cachePtr);
    }
    return cachePtr;
}


/*
 *----------------------------------------------------------------------
 *
 * LogFreeCache --
 *
 *	TLS cleanup callback to destory per-thread Cache struct.
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
LogFreeCache(void *arg)
{
    Cache *cachePtr = arg;

    Ns_DStringFree(&cachePtr->buffer);
    ns_free(cachePtr);
}
