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
 * ns.h --
 *
 *      All the public types and function declarations for the core
 *	AOLserver.
 *
 *	$Header$
 */

#ifndef NS_H
#define NS_H

#ifndef _CLIENTDATA
typedef void *ClientData;
#define _CLIENTDATA
#endif

#include "nsthread.h"
#include "tcl.h"

#ifdef NSD_EXPORTS
#undef NS_EXTERN
#ifdef __cplusplus
#define NS_EXTERN		extern "C" NS_EXPORT
#else
#define NS_EXTERN		extern NS_EXPORT
#endif
#endif

/*
 * Various constants.
 */

#define NS_CONN_CLOSED		  1
#define NS_CONN_SKIPHDRS	  2
#define NS_CONN_SKIPBODY	  4
#define NS_CONN_READHDRS	  8
#define NS_CONN_SENTHDRS	 16

#define NS_CONN_MAXCLS		  16

#define NS_AOLSERVER_3_PLUS
#define NS_UNAUTHORIZED		(-2)
#define NS_FORBIDDEN		(-3)
#define NS_FILTER_BREAK		(-4)
#define NS_FILTER_RETURN	(-5)
#define NS_SHUTDOWN		(-4)
#define NS_TRUE			  1
#define NS_FALSE		  0
#define NS_FILTER_PRE_AUTH        1
#define NS_FILTER_POST_AUTH       2
#define NS_FILTER_TRACE           4
#define NS_FILTER_VOID_TRACE      8
#define NS_REGISTER_SERVER_TRACE 16
#define NS_OP_NOINHERIT		  2
#define NS_OP_NODELETE		  4
#define NS_OP_RECURSE		  8
#define NS_SCHED_THREAD		  1
#define NS_SCHED_ONCE		  2
#define NS_SCHED_DAILY		  4
#define NS_SCHED_WEEKLY		  8
#define NS_SCHED_PAUSED		 16 
#define NS_SCHED_RUNNING	 32 
#define NS_SOCK_READ		  1
#define NS_SOCK_WRITE		  2
#define NS_SOCK_EXCEPTION	  4
#define NS_SOCK_EXIT		  8
#define NS_NO_DATA 		  8
#define NS_END_DATA 		  4
#define NS_ROWS 		  2
#define NS_DML  		  1
#define NS_ENCRYPT_BUFSIZE 	 16
#ifndef WIN32
#ifdef __alpha
typedef long INT64;
#define NS_INT_64_FORMAT_STRING "%ld"
#else
typedef long long INT64;
#define NS_INT_64_FORMAT_STRING "%lld"
#endif
#else
typedef __int64 INT64;
#define NS_INT_64_FORMAT_STRING "%lld"
#endif

/*
 * The following flags define how Ns_Set's
 * are managed by Tcl:
 *
 * NS_TCL_SET_STATIC:	Underlying Ns_Set managed
 * 			elsewhere, only maintain a
 * 			Tcl reference.
 *
 * NS_TCL_SET_DYNAMIC:	Ownership of Ns_Set is given
 * 			entirely to Tcl, free the set
 * 			when no longer referenced in Tcl.
 *
 * In addition, the NS_TCL_SET_SHARED flag can be set
 * to make the set available to all interps.
 */

#define NS_TCL_SET_STATIC      	  0
#define NS_TCL_SET_DYNAMIC        1
#define NS_TCL_SET_SHARED	  2

/*
 * Backwards compatible (and confusing) names.
 */

#define NS_TCL_SET_PERSISTENT     NS_TCL_SET_SHARED
#define NS_TCL_SET_TEMPORARY      NS_TCL_SET_STATIC

#define NS_DSTRING_STATIC_SIZE    512
#define NS_DSTRING_PRINTF_MAX	  2048

#define NS_CACHE_FREE		((Ns_Callback *) (-1))

#ifdef WIN32
NS_EXTERN char *		NsWin32ErrMsg(int err);
NS_EXTERN SOCKET		ns_sockdup(SOCKET sock);
NS_EXTERN int			ns_socknbclose(SOCKET sock);
NS_EXTERN int			truncate(char *file, off_t size);
NS_EXTERN int			link(char *from, char *to);
NS_EXTERN int			symlink(char *from, char *to);
NS_EXTERN int			kill(int pid, int sig);
#define ns_sockclose		closesocket
#define ns_sockioctl		ioctlsocket
#define ns_sockerrno		GetLastError()
#define ns_sockstrerror		NsWin32ErrMsg
#define strcasecmp		_stricmp
#define strncasecmp		_strnicmp
#define vsnprintf		_vsnprintf
#define mkdir(d,m)		mkdir((d))
#define ftruncate(f,s)		chsize((f),(s))
#define EINPROGRESS		WSAEINPROGRESS
#define EWOULDBLOCK		WSAEWOULDBLOCK
#define F_OK			0
#define W_OK			2
#define R_OK			4
#define X_OK			R_OK
#else
#define SOCKET                  int
#define INVALID_SOCKET	        (-1)
#define SOCKET_ERROR	        (-1)
#define ns_sockclose		close
#define ns_socknbclose		close
#define ns_sockioctl		ioctl
#define ns_sockerrno		errno
#define ns_sockstrerror		strerror
#define ns_sockdup		dup
#define closesocket		close
#endif

/*
 * C API macros.
 */

#define STREQ(a,b) 		(((*a) == (*b)) && (strcmp((a),(b)) == 0))
#define STRIEQ(a,b)     	(strcasecmp((a),(b)) == 0)
#define Ns_IndexCount(X) 	((X)->n)
#define Ns_ListPush(elem,list)  ((list)=Ns_ListCons((elem),(list)))
#define Ns_ListFirst(list)      ((list)->first)
#define Ns_ListRest(list)       ((list)->rest)
#define Ns_SetSize(s)           ((s)->size)
#define Ns_SetName(s)           ((s)->name)
#define Ns_SetKey(s,i)          ((s)->fields[(i)].name)
#define Ns_SetValue(s,i)        ((s)->fields[(i)].value)
#define Ns_SetLast(s)           (((s)->size)-1)
#define Ns_DStringLength(dsPtr)        ((dsPtr)->length)
#define Ns_DStringValue(dsPtr)         ((dsPtr)->string)
#define Ns_DStringAppend(dsPtr,string) Ns_DStringNAppend(dsPtr, string, -1)

/*
 * This is used for logging messages.
 */

typedef enum {
    Notice,
    Warning,
    Error,
    Fatal,
    Bug,
    Debug
} Ns_LogSeverity;

/*
 * The following enum lists the possible HTTP headers
 * conversion options (default: Preserve).
 */

typedef enum {
    Preserve, ToLower, ToUpper
} Ns_HeaderCaseDisposition;

/*
 * This is used with the Ns_DbProc structure.
 */

typedef enum {
    DbFn_Name,
    DbFn_DbType,
    DbFn_ServerInit,
    DbFn_OpenDb,
    DbFn_CloseDb,
    DbFn_DML,
    DbFn_Select,
    DbFn_GetRow,
    DbFn_Flush,
    DbFn_Cancel,
    DbFn_GetTableInfo,
    DbFn_TableList,
    DbFn_BestRowId,
    DbFn_Exec,
    DbFn_BindRow,
    DbFn_ResetHandle,
    DbFn_SpStart,
    DbFn_SpSetParam,
    DbFn_SpExec,
    DbFn_SpReturnCode,
    DbFn_SpGetParams,
    DbFn_End
} Ns_DbProcId;

/*
 * This is used with the Ns_DrvProc structure.
 */

typedef enum {
    Ns_DrvIdName,
    Ns_DrvIdStart,
    Ns_DrvIdAccept,
    Ns_DrvIdStop,
    Ns_DrvIdInit,
    Ns_DrvIdRead,
    Ns_DrvIdWrite,
    Ns_DrvIdClose,
    Ns_DrvIdFree,
    Ns_DrvIdPeer,
    Ns_DrvIdLocation,
    Ns_DrvIdHost,
    Ns_DrvIdPort,
    Ns_DrvIdSendFd,
    Ns_DrvIdSendFile,
    Ns_DrvIdDetach,
    Ns_DrvIdConnectionFd,
    Ns_DrvIdMoveContext,
    Ns_DrvIdPeerPort
} Ns_DrvId;

/*
 * Typedefs of functions
 */

typedef int   (Ns_IndexCmpProc) (const void *, const void *);
typedef int   (Ns_SortProc) (void *, void *);
typedef int   (Ns_EqualProc) (void *, void *);
typedef void  (Ns_ElemVoidProc) (void *);
typedef void *(Ns_ElemValProc) (void *);
typedef int   (Ns_ElemTestProc) (void *);
typedef void  (Ns_Callback) (void *arg);
typedef int   (Ns_TclInterpInitProc) (Tcl_Interp *interp, void *arg);
typedef int   (Ns_TclTraceProc) (Tcl_Interp *interp, void *arg);
typedef void  (Ns_TclDeferProc) (Tcl_Interp *interp, void *arg);
typedef int   (Ns_SockProc) (SOCKET sock, void *arg, int why);
typedef void  (Ns_SchedProc) (void *arg, int id);
typedef int   (Ns_ModuleInitProc) (char *server, char *module);
typedef int   (Ns_ServerInitProc) (char *server);
typedef int   (Ns_RequestAuthorizeProc) (char *server, char *method,
			char *url, char *user, char *pass, char *peer);

/*
 * The string data type.
 */

typedef struct Ns_DString {
    char              *string;
    int                length;
    int                spaceAvl;
    char               staticSpace[NS_DSTRING_STATIC_SIZE];
    struct Ns_DString *addr;
} Ns_DString;

/*
 * The field of a key-value data structure.
 */

typedef struct Ns_SetField {
    char *name;
    char *value;
} Ns_SetField;

/*
 * The key-value data structure.
 */

typedef struct Ns_Set {
    char        *name;
    int          size;
    int          maxSize;
    Ns_SetField *fields;
} Ns_Set;

/*
 * The request structure.
 */

typedef struct Ns_Request {
    char           *line;
    char           *method;
    char           *protocol;
    char           *host;
    unsigned short  port;
    char           *url;
    char           *query;
    int             urlc;
    char          **urlv;
    double          version;
} Ns_Request;

/*
 * The connection structure.
 */

typedef struct Ns_Conn {
    Ns_Request *request;
    Ns_Set     *headers;
    Ns_Set     *outputheaders;
    char       *authUser;
    char       *authPasswd;
    int         contentLength;
    int         flags;		/* Currently, only NS_CONN_CLOSED. */
} Ns_Conn;

/*
 * The index data structure.  This is a linear array of values.
 */

typedef struct Ns_Index {
    void            **el;
    Ns_IndexCmpProc  *CmpEls;
    Ns_IndexCmpProc  *CmpKeyWithEl;
    int               n;
    int               max;
    int               inc;
} Ns_Index;

/*
 * A linked list data structure.
 */

typedef struct Ns_List {
    void           *first;
    float           weight;   /* Between 0 and 1 */
    struct Ns_List *rest;
} Ns_List;

/*
 * Database handle structure.
 */

typedef struct Ns_DbHandle {
    char       *driver;
    char       *datasource;
    char       *user;
    char       *password;
    void       *connection;
    char       *poolname;
    int         connected;
    int         verbose;
    Ns_Set     *row;
    char        cExceptionCode[6];
    Ns_DString  dsExceptionMsg;
    void       *context;
    void       *statement;
    int         fetchingRows;
} Ns_DbHandle;

/*
 * Database procedure structure.
 */

typedef struct Ns_DbProc {
    Ns_DbProcId id;
    void       *func;
} Ns_DbProc;

/*
 * The following structure is no longer supported and only provided to
 * allow existing database modules to compile.  All of the TableInfo
 * routines now log an unsupported use error and return an error result.
 */

typedef struct {
    Ns_Set  *table;
    int      size;
    int      ncolumns;
    Ns_Set **columns;
} Ns_DbTableInfo;

/*
 * More typedefs of functions 
 */

typedef void  (Ns_ArgProc) (Tcl_DString *dsPtr, void *arg);
typedef int   (Ns_OpProc) (void *arg, Ns_Conn *conn);
typedef void  (Ns_TraceProc) (void *arg, Ns_Conn *conn);
typedef int   (Ns_FilterProc) (void *arg, Ns_Conn *conn, int why);
typedef int   (Ns_UrlToFileProc) (Ns_DString *dsPtr, char *server, char *url);
typedef char *(Ns_LocationProc) (Ns_Conn *conn);
typedef void  (Ns_AdpParserProc)(Ns_DString *, char *);

/*
 * Typedefs of variables
 */

typedef struct _Ns_Cache	*Ns_Cache;
typedef struct _Ns_Entry	*Ns_Entry;
typedef Tcl_HashSearch 		 Ns_CacheSearch;

typedef struct _Ns_Cls 		*Ns_Cls;
typedef void 	      		*Ns_OpContext;

/*
 * Typedefs for a modular socket driver.
 */

typedef int     (Ns_DriverStartProc) (char *server, char *driver, void **dargPtr);
typedef int     (Ns_DriverAcceptProc) (void *darg, void **cargPtr);
typedef void    (Ns_DriverStopProc) (void *carg);
typedef int     (Ns_ConnInitProc) (void *carg);
typedef int     (Ns_ConnReadProc) (void *carg, void *buf, int nread);
typedef int     (Ns_ConnWriteProc) (void *carg, void *buf, int nwrite);
typedef int     (Ns_ConnCloseProc) (void *carg);
typedef void   *(Ns_ConnDetachProc) (void *carg);
typedef void    (Ns_ConnFreeProc) (void *carg);
typedef char   *(Ns_ConnPeerProc) (void *carg);
typedef int     (Ns_ConnPeerPortProc) (void *carg);
typedef char   *(Ns_ConnLocationProc) (void *carg);
typedef char   *(Ns_ConnHostProc) (void *carg);
typedef int     (Ns_ConnPortProc) (void *carg);
typedef char   *(Ns_ConnDriverNameProc) (void *carg);
typedef int     (Ns_ConnSendFdProc) (void *carg, int fd, int nsend);
typedef int     (Ns_ConnSendFileProc) (void *carg, char *file);
typedef int	(Ns_ConnConnectionFdProc) (void *carg);

typedef struct Ns_DrvProc {
    Ns_DrvId        id;
    void           *proc;
} Ns_DrvProc;

typedef struct Ns_Driver_ *Ns_Driver;

/*
 * adp.c:
 */

NS_EXTERN int Ns_AdpRegisterParser(char *map, Ns_AdpParserProc *proc);
NS_EXTERN int Ns_AdpRequest(Ns_Conn *conn, char *file);

/*
 * auth.c:
 */

NS_EXTERN void Ns_GenSeeds(unsigned long *seedsPtr, int nseeds);
NS_EXTERN double Ns_DRand(void);
NS_EXTERN int Ns_AuthorizeRequest(char *server, char *method, char *url,
			       char *user, char *passwd, char *peer);
NS_EXTERN void Ns_SetRequestAuthorizeProc(char *server,
    				       Ns_RequestAuthorizeProc *procPtr);

/*
 * cache.c:
 */

NS_EXTERN Ns_Cache *Ns_CacheCreate(char *name, int keys, time_t timeout,
				Ns_Callback *freeProc);
NS_EXTERN Ns_Cache *Ns_CacheCreateSz(char *name, int keys, size_t maxSize,
				  Ns_Callback *freeProc);
NS_EXTERN void Ns_CacheDestroy(Ns_Cache *cache);
NS_EXTERN Ns_Cache *Ns_CacheFind(char *name);
NS_EXTERN void *Ns_CacheMalloc(Ns_Cache *cache, size_t len);
NS_EXTERN void Ns_CacheFree(Ns_Cache *cache, void *bytes);
NS_EXTERN Ns_Entry *Ns_CacheFindEntry(Ns_Cache *cache, char *key);
NS_EXTERN Ns_Entry *Ns_CacheCreateEntry(Ns_Cache *cache, char *key, int *newPtr);
NS_EXTERN char *Ns_CacheName(Ns_Entry *entry);
NS_EXTERN char *Ns_CacheKey(Ns_Entry *entry);
NS_EXTERN void *Ns_CacheGetValue(Ns_Entry *entry);
NS_EXTERN void Ns_CacheSetValue(Ns_Entry *entry, void *value);
NS_EXTERN void Ns_CacheSetValueSz(Ns_Entry *entry, void *value, size_t size);
NS_EXTERN void Ns_CacheUnsetValue(Ns_Entry *entry);
NS_EXTERN void Ns_CacheDeleteEntry(Ns_Entry *entry);
NS_EXTERN void Ns_CacheFlushEntry(Ns_Entry *entry);
NS_EXTERN Ns_Entry *Ns_CacheFirstEntry(Ns_Cache *cache, Ns_CacheSearch *search);
NS_EXTERN Ns_Entry *Ns_CacheNextEntry(Ns_CacheSearch *search);
NS_EXTERN void Ns_CacheFlush(Ns_Cache *cache);
NS_EXTERN void Ns_CacheLock(Ns_Cache *cache);
NS_EXTERN void Ns_CacheUnlock(Ns_Cache *cache);
NS_EXTERN int Ns_CacheTimedWait(Ns_Cache *cache, Ns_Time *timePtr);
NS_EXTERN void Ns_CacheWait(Ns_Cache *cache);
NS_EXTERN void Ns_CacheSignal(Ns_Cache *cache);
NS_EXTERN void Ns_CacheBroadcast(Ns_Cache *cache);

/*
 * callbacks.c:
 */

NS_EXTERN void *Ns_RegisterAtStartup(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterAtPreStartup(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterAtSignal(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterServerShutdown(char *server, Ns_Callback *proc,
				       void *arg);
NS_EXTERN void *Ns_RegisterShutdown(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterAtServerShutdown(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterAtShutdown(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterAtReady(Ns_Callback *proc, void *arg);
NS_EXTERN void *Ns_RegisterAtExit(Ns_Callback *proc, void *arg);

/*
 * cls.c:
 */

NS_EXTERN void Ns_ClsAlloc(Ns_Cls *clsPtr, Ns_Callback *proc);
NS_EXTERN void *Ns_ClsGet(Ns_Cls *clsPtr, Ns_Conn *conn);
NS_EXTERN void Ns_ClsSet(Ns_Cls *clsPtr, Ns_Conn *conn, void *data);

/*
 * config.c:
 */

NS_EXTERN char *Ns_ConfigGetValue(char *section, char *key);
NS_EXTERN char *Ns_ConfigGetValueExact(char *section, char *key);
NS_EXTERN int Ns_ConfigGetInt(char *section, char *key, int *valuePtr);
NS_EXTERN int Ns_ConfigGetInt64(char *section, char *key, INT64 *valuePtr);
NS_EXTERN int Ns_ConfigGetBool(char *section, char *key, int *valuePtr);
NS_EXTERN char *Ns_ConfigGetPath(char *server, char *module, ...);
NS_EXTERN Ns_Set **Ns_ConfigGetSections(void);
NS_EXTERN Ns_Set *Ns_ConfigGetSection(char *section);

/*
 * conn.c:
 */

NS_EXTERN int Ns_ConnClose(Ns_Conn *conn);
NS_EXTERN int Ns_ConnInit(Ns_Conn *connPtr);
NS_EXTERN int Ns_ConnRead(Ns_Conn *conn, void *vbuf, int toread);
NS_EXTERN int Ns_ConnWrite(Ns_Conn *conn, void *buf, int towrite);
NS_EXTERN int Ns_ConnReadLine(Ns_Conn *conn, Ns_DString *dsPtr, int *nreadPtr);
NS_EXTERN int Ns_WriteConn(Ns_Conn *conn, char *buf, int len);
NS_EXTERN int Ns_ConnPuts(Ns_Conn *conn, char *string);
NS_EXTERN int Ns_ConnSendDString(Ns_Conn *conn, Ns_DString *dsPtr);
NS_EXTERN int Ns_ConnSendChannel(Ns_Conn *conn, Tcl_Channel chan, int nsend);
NS_EXTERN int Ns_ConnSendFp(Ns_Conn *conn, FILE *fp, int nsend);
NS_EXTERN int Ns_ConnSendFd(Ns_Conn *conn, int fd, int nsend);
NS_EXTERN int Ns_ConnCopyToDString(Ns_Conn *conn, size_t ncopy,
                                Ns_DString *dsPtr);
NS_EXTERN int Ns_ConnCopyToChannel(Ns_Conn *conn, size_t ncopy, Tcl_Channel chan);
NS_EXTERN int Ns_ConnCopyToFile(Ns_Conn *conn, size_t ncopy, FILE *fp);
NS_EXTERN int Ns_ConnCopyToFd(Ns_Conn *conn, size_t ncopy, int fd);
NS_EXTERN int Ns_ConnFlushContent(Ns_Conn *conn);
NS_EXTERN int Ns_ConnModifiedSince(Ns_Conn *conn, time_t inTime);
NS_EXTERN char *Ns_ConnGets(char *outBuffer, size_t inSize, Ns_Conn *conn);
NS_EXTERN int Ns_ConnReadHeaders(Ns_Conn *conn, Ns_Set *set, int *nreadPtr);
NS_EXTERN int Ns_ParseHeader(Ns_Set *set, char *header, Ns_HeaderCaseDisposition disp);
NS_EXTERN Ns_Set  *Ns_ConnGetQuery(Ns_Conn *conn);
NS_EXTERN int Ns_QueryToSet(char *query, Ns_Set *qset);
NS_EXTERN Ns_Set *Ns_ConnHeaders(Ns_Conn *conn);
NS_EXTERN Ns_Set *Ns_ConnOutputHeaders(Ns_Conn *conn);
NS_EXTERN char *Ns_ConnAuthUser(Ns_Conn *conn);
NS_EXTERN char *Ns_ConnAuthPasswd(Ns_Conn *conn);
NS_EXTERN int Ns_ConnContentLength(Ns_Conn *conn);
NS_EXTERN char *Ns_ConnServer(Ns_Conn *conn);
NS_EXTERN int Ns_ConnResponseStatus(Ns_Conn *conn);
NS_EXTERN int Ns_ConnContentSent(Ns_Conn *conn);
NS_EXTERN int Ns_ConnResponseLength(Ns_Conn *conn);
NS_EXTERN char *Ns_ConnPeer(Ns_Conn *conn);
NS_EXTERN int Ns_ConnPeerPort(Ns_Conn *conn);
NS_EXTERN void Ns_SetConnLocationProc(Ns_LocationProc *procPtr);
NS_EXTERN char *Ns_ConnLocation(Ns_Conn *conn);
NS_EXTERN char *Ns_ConnHost(Ns_Conn *conn);
NS_EXTERN int Ns_ConnPort(Ns_Conn *conn);
NS_EXTERN int Ns_ConnSock(Ns_Conn *conn);
NS_EXTERN char *Ns_ConnDriverName(Ns_Conn *conn);
NS_EXTERN void *Ns_ConnDriverContext(Ns_Conn *conn);

/*
 * crypt.c:
 */

NS_EXTERN char *Ns_Encrypt(char *pw, char *salt, char iobuf[ ]);

/*
 * dbdrv.c:
 */

NS_EXTERN int Ns_DbRegisterDriver(char *driver, Ns_DbProc *procs);
NS_EXTERN char *Ns_DbDriverName(Ns_DbHandle *handle);
NS_EXTERN char *Ns_DbDriverDbType(Ns_DbHandle *handle);
NS_EXTERN int Ns_DbDML(Ns_DbHandle *handle, char *sql);
NS_EXTERN Ns_Set *Ns_DbSelect(Ns_DbHandle *handle, char *sql);
NS_EXTERN int Ns_DbExec(Ns_DbHandle *handle, char *sql);
NS_EXTERN Ns_Set *Ns_DbBindRow(Ns_DbHandle *handle);
NS_EXTERN int Ns_DbGetRow(Ns_DbHandle *handle, Ns_Set *row);
NS_EXTERN int Ns_DbFlush(Ns_DbHandle *handle);
NS_EXTERN int Ns_DbCancel(Ns_DbHandle *handle);
NS_EXTERN int Ns_DbResetHandle(Ns_DbHandle *handle);
NS_EXTERN int Ns_DbSpStart(Ns_DbHandle *handle, char *procname);
NS_EXTERN int Ns_DbSpSetParam(Ns_DbHandle *handle, char *paramname,
			   char *paramtype, char *inout, char *value);
NS_EXTERN int Ns_DbSpExec(Ns_DbHandle *handle);
NS_EXTERN int Ns_DbSpReturnCode(Ns_DbHandle *handle, char *returnCode,
			     int bufsize);
NS_EXTERN Ns_Set *Ns_DbSpGetParams(Ns_DbHandle *handle);

/*
 * dbinit.c:
 */

NS_EXTERN char *Ns_DbPoolDescription(char *pool);
NS_EXTERN char *Ns_DbPoolDefault(char *server);
NS_EXTERN char *Ns_DbPoolList(char *server);
NS_EXTERN int Ns_DbPoolAllowable(char *server, char *pool);
NS_EXTERN void Ns_DbPoolPutHandle(Ns_DbHandle *handle);
NS_EXTERN Ns_DbHandle *Ns_DbPoolTimedGetHandle(char *pool, int wait);
NS_EXTERN Ns_DbHandle *Ns_DbPoolGetHandle(char *pool);
NS_EXTERN int Ns_DbPoolGetMultipleHandles(Ns_DbHandle **handles, char *pool,
				       int nwant);
NS_EXTERN int Ns_DbPoolTimedGetMultipleHandles(Ns_DbHandle **handles, char *pool,
					    int nwant, int wait);
NS_EXTERN int Ns_DbBouncePool(char *pool);

/*
 * dbtcl.c:
 */

NS_EXTERN int Ns_TclDbGetHandle(Tcl_Interp *interp, char *handleId,
			     Ns_DbHandle **handle);

/*
 * dbutil.c:
 */
    
NS_EXTERN void Ns_DbQuoteValue(Ns_DString *pds, char *string);
NS_EXTERN Ns_Set *Ns_Db0or1Row(Ns_DbHandle *handle, char *sql, int *nrows);
NS_EXTERN Ns_Set *Ns_Db1Row(Ns_DbHandle *handle, char *sql);
NS_EXTERN int Ns_DbInterpretSqlFile(Ns_DbHandle *handle, char *filename);
NS_EXTERN void Ns_DbSetException(Ns_DbHandle *handle, char *code, char *msg);

/*
 * dns.c:
 */

NS_EXTERN int Ns_GetHostByAddr(Ns_DString *dsPtr, char *addr);
NS_EXTERN int Ns_GetAddrByHost(Ns_DString *dsPtr, char *host);

/*
 * driver.c:
 */

NS_EXTERN Ns_Driver Ns_RegisterDriver(char *server, char *name,
				   Ns_DrvProc *procs, void *arg);
NS_EXTERN void *Ns_GetDriverContext(Ns_Driver driver);
NS_EXTERN char *Ns_GetDriverServer(Ns_Driver driver);

/*
 * dstring.c:
 */

NS_EXTERN void Ns_DStringInit(Ns_DString *dsPtr);
NS_EXTERN char *Ns_DStringVarAppend(Ns_DString *dsPtr, ...);
NS_EXTERN char *Ns_DStringExport(Ns_DString *dsPtr);
NS_EXTERN char *Ns_DStringPrintf(Ns_DString *dsPtr, char *fmt,...);
NS_EXTERN void Ns_DStringFree(Ns_DString *dsPtr);
NS_EXTERN void Ns_DStringTrunc(Ns_DString *dsPtr, int length);
NS_EXTERN void Ns_DStringSetLength(Ns_DString *dsPtr, int length);
NS_EXTERN char *Ns_DStringNAppend(Ns_DString *dsPtr, char *string, int length);
NS_EXTERN char *Ns_DStringAppendArg(Ns_DString *dsPtr, char *string);
NS_EXTERN char *Ns_DStringAppendElement(Ns_DString *dsPtr, char *string);
NS_EXTERN Ns_DString *Ns_DStringPop(void);
NS_EXTERN void Ns_DStringPush(Ns_DString *dsPtr);

/*
 * see macros above for:
 *
 * extern char *Ns_DStringAppend(Ns_DString *dsPtr, char *string);
 * extern int Ns_DStringLength(Ns_DString *dsPtr);
 * extern char *Ns_DStringValue(Ns_DString *dsPtr);
 */

/*
 * exec.c:
 */

NS_EXTERN int Ns_ExecProcess(char *exec, char *dir, int fdin, int fdout,
			  char *args, Ns_Set *env);
NS_EXTERN int Ns_ExecProc(char *exec, char **argv);
NS_EXTERN int Ns_ExecArgblk(char *exec, char *dir, int fdin, int fdout,
			 char *args, Ns_Set *env);
NS_EXTERN int Ns_ExecArgv(char *exec, char *dir, int fdin, int fdout, char **argv,
		       Ns_Set *env);
NS_EXTERN int Ns_WaitProcess(int pid);
NS_EXTERN int Ns_WaitForProcess(int pid, int *statusPtr);

/*
 * fastpath.c:
 */

NS_EXTERN char *Ns_PageRoot(char *server);
NS_EXTERN void Ns_SetUrlToFileProc(char *server, Ns_UrlToFileProc *procPtr);
NS_EXTERN int Ns_UrlToFile(Ns_DString *dsPtr, char *server, char *url);
NS_EXTERN int Ns_UrlIsFile(char *server, char *url);
NS_EXTERN int Ns_UrlIsDir(char *server, char *url);

/*
 * filter.c:
 */

NS_EXTERN void *Ns_RegisterFilter(char *server, char *method, char *URL,
			       Ns_FilterProc *proc, int when, void *args);
NS_EXTERN void *Ns_RegisterServerTrace(char *server, Ns_TraceProc *proc, void *arg);
NS_EXTERN void *Ns_RegisterCleanup(char *server, Ns_TraceProc *proc, void *arg);

/*
 * htuu.c
 */

NS_EXTERN int Ns_HtuuEncode(unsigned char *string, unsigned int bufsize,
			 char *buf);
NS_EXTERN int Ns_HtuuDecode(char *string, unsigned char *buf, int bufsize);

/*
 * index.c:
 */

NS_EXTERN void Ns_IndexInit(Ns_Index *indexPtr, int inc,
			 int (*CmpEls) (const void *, const void *),
			 int (*CmpKeyWithEl) (const void *, const void *));
NS_EXTERN void Ns_IndexTrunc(Ns_Index*indexPtr);
NS_EXTERN void Ns_IndexDestroy(Ns_Index *indexPtr);
NS_EXTERN Ns_Index *Ns_IndexDup(Ns_Index *indexPtr);
NS_EXTERN void *Ns_IndexFind(Ns_Index *indexPtr, void *key);
NS_EXTERN void *Ns_IndexFindInf(Ns_Index *indexPtr, void *key);
NS_EXTERN void **Ns_IndexFindMultiple(Ns_Index *indexPtr, void *key);
NS_EXTERN void Ns_IndexAdd(Ns_Index *indexPtr, void *el);
NS_EXTERN void Ns_IndexDel(Ns_Index *indexPtr, void *el);
NS_EXTERN void *Ns_IndexEl(Ns_Index *indexPtr, int i);
NS_EXTERN void Ns_IndexStringInit(Ns_Index *indexPtr, int inc);
NS_EXTERN Ns_Index *Ns_IndexStringDup(Ns_Index *indexPtr);
NS_EXTERN void Ns_IndexStringAppend(Ns_Index *addtoPtr, Ns_Index *addfromPtr);
NS_EXTERN void Ns_IndexStringDestroy(Ns_Index *indexPtr);
NS_EXTERN void Ns_IndexStringTrunc(Ns_Index *indexPtr);
NS_EXTERN void Ns_IndexIntInit(Ns_Index *indexPtr, int inc);
/*
 * see macros above for:
 *
 * Ns_IndexCount(X) 
 */

/*
 * lisp.c:
 */

NS_EXTERN Ns_List *Ns_ListNconc(Ns_List *l1Ptr, Ns_List *l2Ptr);
NS_EXTERN Ns_List *Ns_ListCons(void *elem, Ns_List *lPtr);
NS_EXTERN Ns_List *Ns_ListNreverse(Ns_List *lPtr);
NS_EXTERN Ns_List *Ns_ListLast(Ns_List *lPtr);
NS_EXTERN void Ns_ListFree(Ns_List *lPtr, Ns_ElemVoidProc *freeProc);
NS_EXTERN void Ns_IntPrint(int d);
NS_EXTERN void Ns_StringPrint(char *s);
NS_EXTERN void Ns_ListPrint(Ns_List *lPtr, Ns_ElemVoidProc *printProc);
NS_EXTERN Ns_List *Ns_ListCopy(Ns_List *lPtr);
NS_EXTERN int Ns_ListLength(Ns_List *lPtr);
NS_EXTERN Ns_List *Ns_ListWeightSort(Ns_List *wPtr);
NS_EXTERN Ns_List *Ns_ListSort(Ns_List *wPtr, Ns_SortProc *sortProc);
NS_EXTERN Ns_List *Ns_ListDeleteLowElements(Ns_List *mPtr, float minweight);
NS_EXTERN Ns_List *Ns_ListDeleteWithTest(void *elem, Ns_List *lPtr,
				      Ns_EqualProc *equalProc);
NS_EXTERN Ns_List *Ns_ListDeleteIf(Ns_List *lPtr, Ns_ElemTestProc *testProc);
NS_EXTERN Ns_List *Ns_ListDeleteDuplicates(Ns_List *lPtr,
				        Ns_EqualProc *equalProc);
NS_EXTERN Ns_List *Ns_ListNmapcar(Ns_List *lPtr, Ns_ElemValProc *valProc);
NS_EXTERN Ns_List *Ns_ListMapcar(Ns_List *lPtr, Ns_ElemValProc *valProc);
/*
 * see macros above for:
 *
 * Ns_ListPush(elem,list)
 * Ns_ListFirst(list)
 * Ns_ListRest(list)
 */

/*
 * tclxkeylist.c:
 */

NS_EXTERN char *Tcl_DeleteKeyedListField (Tcl_Interp  *interp, const char *fieldName,
        const char *keyedList);
NS_EXTERN int Tcl_GetKeyedListField (Tcl_Interp  *interp, const char *fieldName,
        const char *keyedList, char **fieldValuePtr);
NS_EXTERN int Tcl_GetKeyedListKeys (Tcl_Interp  *interp, char const *subFieldName,
        const char *keyedList, int *keysArgcPtr, char ***keysArgvPtr);
NS_EXTERN char *Tcl_SetKeyedListField (Tcl_Interp  *interp, const char *fieldName,
        const char *fieldvalue, const char *keyedList);

/*
 * listen.c:
 */

NS_EXTERN int Ns_SockListenCallback(char *addr, int port, Ns_SockProc *proc,
				 void *arg);
NS_EXTERN int Ns_SockPortBound(int port);

/*
 * log.c:
 */

NS_EXTERN char *Ns_InfoErrorLog(void);
NS_EXTERN int   Ns_LogRoll(void);
NS_EXTERN void  Ns_Log(Ns_LogSeverity severity, char *fmt, ...);
NS_EXTERN void  Ns_Fatal(char *fmt, ...);
NS_EXTERN char *Ns_LogTime(char *timeBuf);
NS_EXTERN int   Ns_RollFile(char *file, int max);
NS_EXTERN int   Ns_PurgeFiles(char *file, int max);
NS_EXTERN int   Ns_RollFileByDate(char *file, int max);

/*
 * nsmain.c:
 */

NS_EXTERN int Ns_Main(int argc, char **argv, Ns_ServerInitProc *initProc);
NS_EXTERN int Ns_WaitForStartup(void);

/*
 * info.c:
 */

NS_EXTERN char *Ns_InfoHomePath(void);
NS_EXTERN char *Ns_InfoServerName(void);
NS_EXTERN char *Ns_InfoServerVersion(void);
NS_EXTERN char *Ns_InfoConfigFile(void);
NS_EXTERN int Ns_InfoPid(void);
NS_EXTERN char *Ns_InfoNameOfExecutable(void);
NS_EXTERN char *Ns_InfoPlatform(void);
NS_EXTERN int Ns_InfoUptime(void);
NS_EXTERN int Ns_InfoBootTime(void);
NS_EXTERN char *Ns_InfoHostname(void);
NS_EXTERN char *Ns_InfoAddress(void);
NS_EXTERN char *Ns_InfoBuildDate(void);
NS_EXTERN int Ns_InfoShutdownPending(void);
NS_EXTERN int Ns_InfoStarted(void);
NS_EXTERN int Ns_InfoServersStarted(void);
NS_EXTERN char *Ns_InfoLabel(void);
NS_EXTERN char *Ns_InfoTag(void);

/*
 * mimetypes.c:
 */

NS_EXTERN char *Ns_GetMimeType(char *file);

/*
 * encoding.c:
 */

NS_EXTERN Tcl_Encoding Ns_GetEncoding(char *charset);

/*
 * modload.c:
 */

NS_EXTERN int Ns_ModuleLoad(char *server, char *module, char *file, char *init);
NS_EXTERN void *Ns_ModuleSymbol(char *file, char *name);
NS_EXTERN void *Ns_ModuleGetSymbol(char *name);

/*
 * nsthread.c:
 */

NS_EXTERN void Ns_SetThreadServer(char *server);
NS_EXTERN char *Ns_GetThreadServer(void);

/*
 * op.c:
 */

NS_EXTERN void Ns_RegisterRequest(char *server, char *method, char *url,
			       Ns_OpProc *procPtr, Ns_Callback *deleteProcPtr,
			       void *arg, int flags);
NS_EXTERN void Ns_GetRequest(char *server, char *method, char *url,
    			  Ns_OpProc **procPtrPtr,
			  Ns_Callback **deleteProcPtrPtr, void **argPtr,
			  int *flagsPtr);
NS_EXTERN void Ns_UnRegisterRequest(char *server, char *method, char *url,
				 int inherit);
NS_EXTERN int Ns_ConnRunRequest(Ns_Conn *conn);
NS_EXTERN int Ns_ConnRedirect(Ns_Conn *conn, char *url);

/*
 * pathname.c:
 */

NS_EXTERN int Ns_PathIsAbsolute(char *path);
NS_EXTERN char *Ns_NormalizePath(Ns_DString *dsPtr, char *path);
NS_EXTERN char *Ns_MakePath(Ns_DString *dsPtr, ...);
NS_EXTERN char *Ns_LibPath(Ns_DString *dsPtr, ...);
NS_EXTERN char *Ns_HomePath(Ns_DString *dsPtr, ...);
NS_EXTERN char *Ns_ModulePath(Ns_DString *dsPtr, char *server, char *module, ...);

/*
 * proc.c:
 */

NS_EXTERN void Ns_RegisterProcDesc(void *procAddr, char *desc);
NS_EXTERN void Ns_RegisterProcInfo(void *procAddr, char *desc, Ns_ArgProc *argProc);
NS_EXTERN void Ns_GetProcInfo(Tcl_DString *dsPtr, void *procAddr, void *arg);

/*
 * quotehtml.c:
 */

NS_EXTERN void Ns_QuoteHtml(Ns_DString *pds, char *string);

/*
 * request.c:
 */

NS_EXTERN void Ns_FreeRequest(Ns_Request *request);
NS_EXTERN Ns_Request *Ns_ParseRequest(char *line);
NS_EXTERN char *Ns_SkipUrl(Ns_Request *request, int n);
NS_EXTERN void Ns_SetRequestUrl(Ns_Request *request, char *url);

/*
 * return.c:
 */

NS_EXTERN void Ns_RegisterReturn(int status, char *url);
NS_EXTERN void Ns_ConnConstructHeaders(Ns_Conn *conn, Ns_DString *dsPtr);
NS_EXTERN int Ns_ConnFlushHeaders(Ns_Conn *conn, int status);
NS_EXTERN void Ns_ConnSetHeaders(Ns_Conn *conn, char *field, char *value);
NS_EXTERN void Ns_ConnCondSetHeaders(Ns_Conn *conn, char *field, char *value);
NS_EXTERN void Ns_ConnReplaceHeaders(Ns_Conn *conn, Ns_Set *newheaders);
NS_EXTERN void Ns_ConnSetRequiredHeaders(Ns_Conn *conn, char *type, int length);
NS_EXTERN void Ns_ConnSetTypeHeader(Ns_Conn *conn, char *type);
NS_EXTERN void Ns_ConnSetLengthHeader(Ns_Conn *conn, int length);
NS_EXTERN void Ns_ConnSetLastModifiedHeader(Ns_Conn *conn, time_t *mtime);
NS_EXTERN void Ns_ConnSetExpiresHeader(Ns_Conn *conn, char *expires);
NS_EXTERN int Ns_ConnPrintfHeader(Ns_Conn *conn, char *fmt,...);
NS_EXTERN int Ns_ConnResetReturn(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnAdminNotice(Ns_Conn *conn, int status, char *title,
				    char *notice);
NS_EXTERN int Ns_ConnReturnNotice(Ns_Conn *conn, int status, char *title,
			       char *notice);
NS_EXTERN int Ns_ConnReturnData(Ns_Conn *conn, int status, char *data, int len,
			     char *type);
NS_EXTERN int Ns_ConnReturnHtml(Ns_Conn *conn, int status, char *html, int len);
NS_EXTERN int Ns_ConnReturnOk(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnNoResponse(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnRedirect(Ns_Conn *conn, char *url);
NS_EXTERN int Ns_ConnReturnBadRequest(Ns_Conn *conn, char *reason);
NS_EXTERN int Ns_ConnReturnUnauthorized(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnForbidden(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnNotFound(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnNotModified(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnNotImplemented(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnInternalError(Ns_Conn *conn);
NS_EXTERN int Ns_ConnReturnStatus(Ns_Conn *conn, int status);
NS_EXTERN int Ns_ConnReturnOpenChannel(Ns_Conn *conn, int status, char *type,
				    Tcl_Channel chan, int len);
NS_EXTERN int Ns_ConnReturnOpenFile(Ns_Conn *conn, int status, char *type,
				 FILE *fp, int len);
NS_EXTERN int Ns_ConnReturnOpenFd(Ns_Conn *conn, int status, char *type, int fd,
			       int len);
NS_EXTERN int Ns_ConnReturnFile(Ns_Conn *conn, int status, char *type,
			     char *filename);

/*
 * sched.c:
 */

NS_EXTERN int Ns_After(int seconds, Ns_Callback *proc, void *arg,
		    Ns_Callback *deleteProc);
NS_EXTERN int Ns_Cancel(int id);
NS_EXTERN int Ns_Pause(int id);
NS_EXTERN int Ns_Resume(int id);
NS_EXTERN int Ns_ScheduleProc(Ns_Callback *proc, void *arg, int thread,
			   int interval);
NS_EXTERN int Ns_ScheduleDaily(Ns_SchedProc *proc, void *arg, int flags,
			    int hour, int minute, Ns_SchedProc *cleanupProc);
NS_EXTERN int Ns_ScheduleWeekly(Ns_SchedProc *proc, void *arg, int flags,
			     int day, int hour, int minute,
			     Ns_SchedProc *cleanupProc);
NS_EXTERN int Ns_ScheduleProcEx(Ns_SchedProc *proc, void *arg, int flags,
			     int interval, Ns_SchedProc *cleanupProc);
NS_EXTERN void Ns_UnscheduleProc(int id);

/*
 * queue.c:
 */

NS_EXTERN int Ns_QueueConn(Ns_Driver driver, void *arg);

/*
 * set.c:
 */

NS_EXTERN void Ns_SetUpdate(Ns_Set *set, char *key, char *value);
NS_EXTERN Ns_Set *Ns_SetCreate(char *name);
NS_EXTERN void Ns_SetFree(Ns_Set *set);
NS_EXTERN int Ns_SetPut(Ns_Set *set, char *key, char *value);
NS_EXTERN int Ns_SetUniqueCmp(Ns_Set *set, char *key, int (*cmp) (char *s1,
			   char *s2));
NS_EXTERN int Ns_SetFindCmp(Ns_Set *set, char *key, int (*cmp) (char *s1,
			 char *s2));
NS_EXTERN char *Ns_SetGetCmp(Ns_Set *set, char *key, int (*cmp) (char *s1,
			  char *s2));
NS_EXTERN int Ns_SetUnique(Ns_Set *set, char *key);
NS_EXTERN int Ns_SetIUnique(Ns_Set *set, char *key);
NS_EXTERN int Ns_SetFind(Ns_Set *set, char *key);
NS_EXTERN int Ns_SetIFind(Ns_Set *set, char *key);
NS_EXTERN char *Ns_SetGet(Ns_Set *set, char *key);
NS_EXTERN char *Ns_SetIGet(Ns_Set *set, char *key);
NS_EXTERN void Ns_SetTrunc(Ns_Set *set, int size);
NS_EXTERN void Ns_SetDelete(Ns_Set *set, int index);
NS_EXTERN void Ns_SetPutValue(Ns_Set *set, int index, char *value);
NS_EXTERN void Ns_SetDeleteKey(Ns_Set *set, char *key);
NS_EXTERN void Ns_SetIDeleteKey(Ns_Set *set, char *key);
NS_EXTERN Ns_Set *Ns_SetListFind(Ns_Set **sets, char *name);
NS_EXTERN Ns_Set **Ns_SetSplit(Ns_Set *set, char sep);
NS_EXTERN void Ns_SetListFree(Ns_Set **sets);
NS_EXTERN void Ns_SetMerge(Ns_Set *high, Ns_Set *low);
NS_EXTERN Ns_Set *Ns_SetCopy(Ns_Set *old);
NS_EXTERN void Ns_SetMove(Ns_Set *to, Ns_Set *from);
NS_EXTERN void Ns_SetPrint(Ns_Set *set);
/*
 * see macros above for:
 *
 * Ns_SetSize(s)
 * Ns_SetName(s)
 * Ns_SetKey(s,i)
 * Ns_SetValue(s,i)
 * Ns_SetLast(s)
 */

/*
 * sock.c:
 */

NS_EXTERN int Ns_SockRecv(SOCKET sock, void *vbuf, int nrecv, int timeout);
NS_EXTERN int Ns_SockSend(SOCKET sock, void *vbuf, int nsend, int timeout);
NS_EXTERN int Ns_SockWait(SOCKET sock, int what, int timeout);
NS_EXTERN SOCKET Ns_BindSock(struct sockaddr_in *psa);
NS_EXTERN SOCKET Ns_SockBind(struct sockaddr_in *psa);
NS_EXTERN SOCKET Ns_SockListen(char *address, int port);
NS_EXTERN SOCKET Ns_SockListenEx(char *address, int port, int backlog);
NS_EXTERN SOCKET Ns_SockAccept(SOCKET sock, struct sockaddr *psa, int *lenPtr);
NS_EXTERN SOCKET Ns_SockConnect(char *host, int port);
NS_EXTERN SOCKET Ns_SockAsyncConnect(char *host, int port);
NS_EXTERN SOCKET Ns_SockTimedConnect(char *host, int port, int timeout);
NS_EXTERN int Ns_SockSetNonBlocking(SOCKET sock);
NS_EXTERN int Ns_SockSetBlocking(SOCKET sock);
NS_EXTERN int Ns_SockCallback(SOCKET sock, Ns_SockProc *proc, void *arg, int when);
NS_EXTERN void Ns_SockCancelCallback(SOCKET sock);
NS_EXTERN int Ns_GetSockAddr(struct sockaddr_in *psa, char *host, int port);
NS_EXTERN void Ns_ClearSockErrno(void);
NS_EXTERN int Ns_GetSockErrno(void);
NS_EXTERN void Ns_SetSockErrno(int err);
NS_EXTERN char *Ns_SockStrError(int err);
NS_EXTERN int Ns_SockCloseLater(SOCKET sock);

/*
 * str.c:
 */

NS_EXTERN char *Ns_StrTrim(char *string);
NS_EXTERN char *Ns_StrTrimLeft(char *string);
NS_EXTERN char *Ns_StrTrimRight(char *string);
NS_EXTERN char *Ns_StrToLower(char *string);
NS_EXTERN char *Ns_StrToUpper(char *string);
NS_EXTERN char *Ns_StrCaseFind(char *s1, char *s2);
NS_EXTERN char *Ns_Match(char *a, char *b);
NS_EXTERN char *Ns_NextWord(char *line);
NS_EXTERN char *Ns_StrNStr(char *pattern, char *expression);

/*
 * tclfile.c:
 */

NS_EXTERN int Ns_TclGetOpenChannel(Tcl_Interp *interp, char *chanId, int write,
				int check, Tcl_Channel *chanPtr);
NS_EXTERN int Ns_TclGetOpenFd(Tcl_Interp *interp, char *chanId, int write,
			   int *fdPtr);

/*
 * tclinit.c:
 */

NS_EXTERN int Ns_TclInitInterps(char *server, Ns_TclInterpInitProc *proc, void *arg);
NS_EXTERN void Ns_TclRegisterDeferred(Tcl_Interp *interp, Ns_TclDeferProc *proc, void *arg);
NS_EXTERN void Ns_TclMarkForDelete(Tcl_Interp *interp);
NS_EXTERN Tcl_Interp *Ns_TclAllocateInterp(char *server);
NS_EXTERN void Ns_TclDeAllocateInterp(Tcl_Interp *interp);
NS_EXTERN char *Ns_TclLibrary(char *server);
NS_EXTERN char *Ns_TclInterpServer(Tcl_Interp *interp);
NS_EXTERN Ns_Conn *Ns_TclGetConn(Tcl_Interp *interp);

/*
 * tclop.c:
 */

NS_EXTERN int Ns_TclRequest(Ns_Conn *conn, char *proc);
NS_EXTERN int Ns_TclEval(Ns_DString *pds, char *server, char *script);
NS_EXTERN char *Ns_TclLogError(Tcl_Interp *interp);
NS_EXTERN char *Ns_TclLogErrorRequest(Tcl_Interp *interp, Ns_Conn *conn);
NS_EXTERN Tcl_Interp *Ns_GetConnInterp(Ns_Conn *conn);

/*
 * tclsched.c:
 */

NS_EXTERN int Ns_TclThread(Tcl_Interp *interp, char *script, Ns_Thread *tid);
NS_EXTERN int Ns_TclDetachedThread(Tcl_Interp *interp, char *script);

/*
 * tclset.c:
 */

NS_EXTERN int Ns_TclEnterSet(Tcl_Interp *interp, Ns_Set *set, int flags);
NS_EXTERN Ns_Set *Ns_TclGetSet(Tcl_Interp *interp, char *setId);
NS_EXTERN int Ns_TclGetSet2(Tcl_Interp *interp, char *setId, Ns_Set **setPtrPtr);
NS_EXTERN int Ns_TclFreeSet(Tcl_Interp *interp, char *setId);

/*
 * time.c:
 */

NS_EXTERN char *Ns_HttpTime(Ns_DString *pds, time_t *when);
NS_EXTERN time_t Ns_ParseHttpTime(char *str);

/*
 * url.c:
 */

NS_EXTERN char *Ns_RelativeUrl(char *url, char *location);
NS_EXTERN int Ns_ParseUrl(char *url, char **pprotocol, char **phost, char **pport,
		       char **ppath, char **ptail);
NS_EXTERN int Ns_AbsoluteUrl(Ns_DString *pds, char *url, char *baseurl);

/*
 * urlencode.c:
 */

NS_EXTERN char *Ns_EncodeUrl(Ns_DString *pds, char *string);
NS_EXTERN char *Ns_DecodeUrl(Ns_DString *pds, char *string);

/*
 * urlopen.c:
 */

NS_EXTERN int Ns_FetchPage(Ns_DString *pds, char *url, char *server);
NS_EXTERN int Ns_FetchURL(Ns_DString *pds, char *url, Ns_Set *headers);

/*
 * urlspace.c:
 */

NS_EXTERN int Ns_UrlSpecificAlloc(void);
NS_EXTERN void Ns_UrlSpecificSet(char *handle, char *method, char *url, int id,
			      void *data, int flags,
			      void (*deletefunc) (void *));
NS_EXTERN void *Ns_UrlSpecificGet(char *handle, char *method, char *url, int id);
NS_EXTERN void *Ns_UrlSpecificGetFast(char *handle, char *method, char *url,
			      int id);
NS_EXTERN void *Ns_UrlSpecificGetExact(char *handle, char *method, char *url,
			      int id, int flags);
NS_EXTERN void *Ns_UrlSpecificDestroy(char *handle, char *method, char *url,
			      int id, int flags);
NS_EXTERN int Ns_ServerSpecificAlloc(void);
NS_EXTERN void Ns_ServerSpecificSet(char *handle, int id, void *data, int flags,
				 void (*deletefunc) (void *));
NS_EXTERN void *Ns_ServerSpecificGet(char *handle, int id);
NS_EXTERN void *Ns_ServerSpecificDestroy(char *handle, int id, int flags);

/*
 * fd.c:
 */

NS_EXTERN int Ns_CloseOnExec(int fd);
NS_EXTERN int Ns_NoCloseOnExec(int fd);
NS_EXTERN int Ns_DupHigh(int *fdPtr);

/*
 * unix.c, win32.c:
 */

NS_EXTERN int ns_sockpair(SOCKET *socks);
NS_EXTERN int ns_pipe(int *fds);
NS_EXTERN int Ns_GetUserHome(Ns_DString *pds, char *user);
NS_EXTERN int Ns_GetGid(char *group);
NS_EXTERN int Ns_GetUserGid(char *user);
NS_EXTERN int Ns_GetUid(char *user);


/*
 * Compatibility macros.
 */

#define DllExport		NS_EXPORT
#define Ns_Select	        select
#define Ns_InfoHome()		Ns_InfoHomePath()
#define Ns_InfoServer()		Ns_InfoServerName()
#define Ns_InfoVersion()	Ns_InfoServerVersion()
#define Ns_RequestAuthorize(s,m,u,au,ap,p) Ns_AuthorizeRequest(s,m,u,au,ap,p)
#define Ns_RequestFree(r)	Ns_FreeRequest(r)
#define Ns_ResetReturn(c)	Ns_ConnResetReturn(c)
#define Ns_PutsConn(c,s)	Ns_ConnPuts(c,s)
#define Ns_PrintfHeader		Ns_ConnPrintfHeader
#define Ns_ExpiresHeader(c,h)	Ns_ConnSetExpiresHeader(c,h)
#define Ns_ReturnHtml(c,s,h,l)	Ns_ConnReturnHtml(c,s,h,l)
#define Ns_ReturnOk(c)		Ns_ConnReturnOk(c)
#define Ns_ReturnNoResponse(c)	Ns_ConnReturnNoResponse(c)
#define Ns_ReturnRedirect(c,l)	Ns_ConnReturnRedirect(c,l)
#define Ns_ReturnNotModified(c)	Ns_ConnReturnNotModified(c)
#define Ns_ReturnBadRequest(c,r) Ns_ConnReturnBadRequest(c,r)
#define Ns_ReturnUnauthorized(c) Ns_ConnReturnUnauthorized(c)
#define Ns_ReturnForbidden(c)	Ns_ConnReturnForbidden(c)
#define Ns_ReturnNotFound(c)	Ns_ConnReturnNotFound(c)
#define Ns_ReturnInternalError(c) Ns_ConnReturnInternalError(c)
#define Ns_ReturnNotImplemented(c) Ns_ConnReturnNotImplemented(c)
#define Ns_ReturnStatus(c,s)	Ns_ConnReturnStatus(c,s)
#define Ns_TypeHeader(c,t)      Ns_ConnSetTypeHeader(c,t)
#define Ns_LengthHeader(c,l)    Ns_ConnSetLengthHeader(c,l)
#define Ns_LastModifiedHeader(c,w) Ns_ConnSetLastModifiedHeader(c,w)
#define Ns_ReturnFile(c,s,t,f)  Ns_ConnReturnFile(c,s,t,f)
#define Ns_ReturnOpenFile(c,s,t,f,l) Ns_ConnReturnOpenFile(c,s,t,f,l)
#define Ns_ReturnNotice(c,s,t,m) Ns_ConnReturnNotice(c,s,t,m)
#define Ns_ReturnAdminNotice(c,s,t,m) Ns_ConnReturnAdminNotice(c,s,t,m)
#define Ns_HeadersFlush(c,s)    Ns_ConnFlushHeaders(c,s)
#define Ns_HeadersPut(Nc,f,v)   Ns_ConnSetHeaders(Nc,f,v)
#define Ns_HeadersCondPut(c,f,v) Ns_ConnCondSetHeaders(c,f,v)
#define Ns_HeadersReplace(c,n)  Ns_ConnReplaceHeaders(c,n)
#define Ns_HeadersRequired(c,t,l) Ns_ConnSetRequiredHeaders(c,t,l)
#define Ns_ConfigPath           Ns_ConfigGetPath
#define Ns_ConfigSection(s)     Ns_ConfigGetSection(s)
#define Ns_ConfigSections()     Ns_ConfigGetSections()
#define Ns_ConfigGet(s,k)       Ns_ConfigGetValue(s,k)
#define Ns_ConfigGetExact(s,k)  Ns_ConfigGetValueExact(s,k)
#define Ns_UrlEncode(p,u)       Ns_EncodeUrl(p,u)
#define Ns_UrlDecode(p,u)       Ns_DecodeUrl(p,u)

#endif /* NS_H */
