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
 * tclimg.c --
 *
 *	Commands for image files.
 */

static const char *RCSID = "@(#) $Header$, compiled: " __DATE__ " " __TIME__;

#include "nsd.h"

/*
 * Local functions defined in this file
 */

static int ChanGetc(Tcl_Channel chan);
static unsigned int JpegRead2Bytes(Tcl_Channel chan);
static int JpegNextMarker(Tcl_Channel chan);
static int JpegSize(Tcl_Channel chan, int *wPtr, int *hPtr);
static unsigned int JpegRead2Bytes(Tcl_Channel chan);
static void AppendDims(Tcl_Interp *interp, int w, int h);


/*
 *----------------------------------------------------------------------
 *
 * NsTclGifSizeCmd --
 *
 *	Implements ns_gifsize, returning a list of width and height.
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
NsTclGifSizeCmd(ClientData dummy, Tcl_Interp *interp, int argc, char **argv) 
{
    int fd;
    unsigned char  buf[0x300];
    int depth, colormap, dx, dy, status;

    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
                         argv[0], " gif\"", NULL);
        return TCL_ERROR;
    }
    fd = open(argv[1], O_RDONLY|O_BINARY);
    if (fd == -1) {
        Tcl_AppendResult(interp, "could not open \"", argv[1],
	    "\": ", Tcl_PosixError(interp), NULL);
        return TCL_ERROR;
    }
    status = TCL_ERROR;

    /*
     * Read the GIF version number
     */
    
    if (read(fd, buf, 6) == -1) {
readfail:
        Tcl_AppendResult(interp, "could not read \"", argv[1],
	    "\": ", Tcl_PosixError(interp), NULL);
	goto done;
    }

    if (strncmp((char *) buf, "GIF87a", 6) && 
	strncmp((char *) buf, "GIF89a", 6)) {
badfile:
        Tcl_AppendResult(interp, "invalid gif file: ", argv[1], NULL);
        goto done;
    }

    if (read(fd, buf, 7) == -1) {
	goto readfail;
    }

    depth = 1 << ((buf[4] & 0x7) + 1);
    colormap = (buf[4] & 0x80 ? 1 : 0);

    if (colormap) {
        if (read(fd,buf,3*depth) == -1) {
            goto readfail;
        }
    }

  outerloop:
    if (read(fd, buf, 1) == -1) {
        goto readfail;
    }

    if (buf[0] == '!') {
        unsigned char count;
	
        if (read(fd, buf, 1) == -1) {
            goto readfail;
        }
      innerloop:
        if (read(fd, (char *) &count, 1) == -1) {
            goto readfail;
        }
        if (count == 0) {
            goto outerloop;
        }
        if (read(fd, buf, count) == -1) {
            goto readfail;
        }
        goto innerloop;
    } else if (buf[0] != ',') {
        goto badfile;
    }

    if (read(fd,buf,9) == -1) {
        goto readfail;
    }

    dx = 0x100 * buf[5] + buf[4];
    dy = 0x100 * buf[7] + buf[6];
    AppendDims(interp, dx, dy);
    status = TCL_OK;

done:
    close(fd);
    return status;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclJpegSizeCmd --
 *
 *	Implements ns_jpegsize. 
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
NsTclJpegSizeCmd(ClientData dummy, Tcl_Interp *interp, int argc, char **argv)
{
    int   code, w, h;
    Tcl_Channel chan;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " file\"", NULL);
	return TCL_ERROR;
    }

    chan = Tcl_OpenFileChannel(interp, argv[1], "r", 0);
    if (chan == NULL) {
	Tcl_AppendResult(interp, "could not open \"",
	    argv[1], "\": ", Tcl_PosixError(interp), NULL);
	return TCL_ERROR;
    }
    code = JpegSize(chan, &w, &h);
    Tcl_Close(interp, chan);
    if (code != TCL_OK) {
    	Tcl_AppendResult(interp, "invalid jpeg file: ", argv[1], NULL);
	return TCL_ERROR;
    }
    AppendDims(interp, w, h);
    return TCL_OK;
}

#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */

static int
JpegSize(Tcl_Channel chan, int *wPtr, int *hPtr)
{
    unsigned int i, w, h;

    if (ChanGetc(chan) == 0xFF && ChanGetc(chan) == M_SOI) {
	while (1) {
	    i = JpegNextMarker(chan);
	    if (i == EOF || i == M_SOS || i == M_EOI) {
	    	break;
	    }
	    if ((i >> 4) == 0xC) {
		if (JpegRead2Bytes(chan) != EOF && ChanGetc(chan) != EOF
		    && (h = JpegRead2Bytes(chan)) != EOF
		    && (w = JpegRead2Bytes(chan)) != EOF) {
		    *wPtr = w;
		    *hPtr = h;
		    return TCL_OK;
		}
		break;
	    }
	    i = JpegRead2Bytes(chan);
	    if (i < 2 || Tcl_Seek(chan, i-2, SEEK_CUR) == -1) {
	    	break;
	    }
	}
    }
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * JpegRead2Bytes --
 *
 *	Read 2 bytes, convert to unsigned int. All 2-byte quantities 
 *	in JPEG markers are MSB first. 
 *
 * Results:
 *	The two byte value, or -1 on error. 
 *
 * Side effects:
 *	Advances file pointer. 
 *
 *----------------------------------------------------------------------
 */

static unsigned int
JpegRead2Bytes(Tcl_Channel chan)
{
    int c1, c2;
    
    c1 = ChanGetc(chan);
    c2 = ChanGetc(chan);
    if (c1 == EOF || c2 == EOF) {
	return -1;
    }
    return (((unsigned int) c1) << 8) + ((unsigned int) c2);
}


/*
 *----------------------------------------------------------------------
 *
 * JpegNextMarker --
 *
 *	Find the next JPEG marker and return its marker code. We 
 *	expect at least one FF byte, possibly more if the compressor 
 *	used FFs to pad the file. There could also be non-FF garbage 
 *	between markers. The treatment of such garbage is 
 *	unspecified; we choose to skip over it but emit a warning 
 *	msg. This routine must not be used after seeing SOS marker, 
 *	since it will not deal correctly with FF/00 sequences in the 
 *	compressed image data... 
 *
 * Results:
 *	The next marker code.
 *
 * Side effects:
 *	Will eat up any duplicate FF bytes.
 *
 *----------------------------------------------------------------------
 */

static int
JpegNextMarker(Tcl_Channel chan)
{
    int c;

    /*
     * Find 0xFF byte; count and skip any non-FFs.
     */
    
    c = ChanGetc(chan);
    while (c != EOF && c != 0xFF) {
	c = ChanGetc(chan);
    }
    if (c != EOF) {
	/*
	 * Get marker code byte, swallowing any duplicate FF bytes.
	 */
	
	do {
	    c = ChanGetc(chan);
	} while (c == 0xFF);
    }

    return c;
}


/*
 *----------------------------------------------------------------------
 *
 * ChanGetc --
 *
 *	Read a single unsigned char from a channel.
 *
 * Results:
 *	Character or EOF. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ChanGetc(Tcl_Channel chan)
{
    unsigned char buf[1];

    if (Tcl_Read(chan, (char *) buf, 1) != 1) {
	return EOF;
    }
    return (int) buf[0];
}


/*
 *----------------------------------------------------------------------
 *
 * AppendDims --
 *
 *	Format and append width and height dimensions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	List elements appended to interp result.
 *
 *----------------------------------------------------------------------
 */

static void
AppendDims(Tcl_Interp *interp, int w, int h)
{
    char buf[20];

    sprintf(buf, "%d", w);
    Tcl_AppendElement(interp, buf);
    sprintf(buf, "%d", h);
    Tcl_AppendElement(interp, buf);
}