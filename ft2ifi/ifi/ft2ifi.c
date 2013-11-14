/*                                                                         */
/*                        FreeType/2 IFI version 2                         */
/*                                                                         */
/*            OS/2 Font Driver using the FreeType 2 library                */
/*                                                                         */
/*       Copyright (C) 2013 Alex Taylor <alex@altsan.org>                  */
/*       Based on previous work for FreeType/2 version 1.x by:             */
/*       Copyright (C) 2010--2013 Alex Taylor <alex@altsan.org>            */
/*       Copyright (C) 2002--2007 KO Myung-Hun <komh@chollian.net>         */
/*       Copyright (C) 2003--2004 Seo, Hyun-Tae <acrab001@hitel.net>       */
/*       Copyright (C) 1997--2000 Michal Necasek <mike@mendelu.cz>         */
/*       Copyright (C) 1997, 1998 David Turner <dturner@cybercable.fr>     */
/*       Copyright (C) 1997, 1999 International Business Machines          */
/*                                                                         */
/*       Version: 2.0.0                                                    */
/*                                                                         */
/* This source is to be compiled with IBM VisualAge C++ 3.0.               */
/* Other compilers may actually work too but don't forget this is NOT a    */
/* normal DLL but rather a subsystem DLL. That means it shouldn't use      */
/* the usual C library as it has to run without runtime environment.       */
/* VisualAge provides a special subsystem version of the run-time library. */
/* All this is of course very compiler-dependent. See makefiles for        */
/* discussion of switches used.                                            */
/*                                                                         */
/* Implementation Notes:                                                   */
/*                                                                         */
/* Note #1: As a consequence of this being a subsystem library, a build    */
/*   target "os2-ifi" has been added to the FreeType v2 source (with       */
/*   dependent files in the builds/os2 directory).  In particular,         */
/*   ifisystem.c contains the necessary memory management and stream       */
/*   (file) i/o routines.                                                  */
/*                                                                         */
/*   If it is necessary to do explicit memory or stream management         */
/*   (outside the normal FT APIs), use the functions provided here (like   */
/*   safe_alloc and safe_free) - don't try to use std C library calls      */
/*   like malloc(), fopen(), fseek() etc. because they won't work.         */
/*                                                                         */
/* Note #2: On exit of each entrypoint function that reads from font file, */
/*   the underlying stream must be closed. This is because file handles    */
/*   opened by this DLL actually belong to the calling process. As a       */
/*   consequence                                                           */
/*    a) it's easy to run out of file handles, which results in really     */
/*       very nasty behavior and/or crashes. This could be solved by       */
/*       increased file handles limit, but cannot because                  */
/*    b) it is impossible to close files open by another process and       */
/*       therefore the fonts cannot be properly uninstalled (you can't     */
/*       delete them while they're open by another process)                */
/*   The only solution is very simple - just close the file using          */
/*   FX_Flush_Stream() before exiting an entrypoint function. This ensures */
/*   files are not left open and other problems.                           */
/*                                                                         */
/*   Similarly, at the start of every entrypoint function that reads from  */
/*   a font file (via a face object), the stream must be tested and        */
/*   reopened if necessary.  FreeType v2 apparently no longer does this    */
/*   automatically.  FX_Activate_Stream() has been provided for this.      */
/*                                                                         */
/* Note #3: The whole business with linked lists is aimed at lowering      */
/*   memory consumption dramatically. If you install 50 TT fonts, OS/2     */
/*   opens all of them at startup. Even if you never use them, they take   */
/*   up at least over 1M of memory. With certain fonts the consumption can */
/*   easily go over several megs. We limit such waste of memory by only    */
/*   actually keeping open several typefaces most recently used. Their     */
/*   number can be set via entry in OS2.INI.                               */
/*                                                                         */
/* For Intelligent Font Interface (IFI) specification please see IFI32.LWP */
/* or the PDF version IFI32.PDF.  Note that this does have a few           */
/* inaccuracies or at least gaps in the information; the source code below */
/* generally notes in the comments when it's taking these into account.    */
/*                                                                         */

#ifndef  __IBMC__
   #error "Please use IBM VisualAge C++ to compile FreeType/2"
#endif

#define IFI_DRIVER          /* checked for by some of our extensions */
//#define KERN_TABLE_SUPPORT  /* support for our KERN table parser */

#define INCL_WINSHELLDATA   /* for accessing OS2.INI */
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_DOSPROCESS
#define INCL_GRE_DCS
#define INCL_GRE_DEVSUPPORT
#define INCL_DDIMISC
#define INCL_IFI
#include <os2.h>
#include <pmddi.h>          /* SSAllocMem(), SSFreeMem() and more */
#include <uconv.h>
#include <string.h>
#include <stdlib.h>         /* min and max macros */

#define  _syscall  _System  /* the IFI headers don't compile without it */

#include "32pmifi.h"        /* IFI header & includes        */

/* FreeType includes */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_BBOX_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H

/* Some private extensions */
#include "ft2mem.h"         /* safe_alloc() and safe_free() */
#include "ft2kern.h"        /* KERN table parsing routines  */

#include "ft2ifi.h"         /* various private definitions  */
#include "ft2debug.h"       /* debugging routines           */

#define  ALLOC( p, size )  safe_alloc( (void**)&(p), (size) )
#define  FREE( p )         safe_free( (void**)&(p) )

extern FT_BASE_DEF( FT_Error ) FT_Stream_Open( FT_Stream stream, const char*  filepathname );


/****************************************************************************
 * GLOBALS                                                                  *
 ****************************************************************************/

/* --------------------------------------------------------------------------
 * The following defines the single exported entry point mandated by the IFI
 * spec.  Doing it this way is faster than exporting every single function,
 * and a bit more flexible.
 */

/* This is the exported function dispatch table.
 */
FDDISPATCH fdisp = {
    FdLoadFontFile,
    FdQueryFaces,
    FdUnloadFontFile,
    FdOpenFontContext,
    FdSetFontContext,
    FdCloseFontContext,
    FdQueryFaceAttr,
    FdQueryCharAttr,
    NULL,               /* no longer used, only the spec fails to mention it */
    FdConvertFontFile,
    FdQueryFullFaces
};

#pragma export (fdhdr, "FONT_DRIVER_DISPATCH_TABLE", 1)

FDHEADER fdhdr = {
    sizeof(FDHEADER),                    /* Font driver Header           */
    "OS/2 FONT DRIVER",                  /* IFI signature, do not change */
    "OpenType (using FreeType 2 engine)",/* Description up to 40 chars   */
    IFI_VERSION20,                       /* IFI version                  */
    IFI_CAPS_PANOSE,                     /* Capabilities flag            */
    &fdisp                               /* Table of function pointers   */
};


/* --------------------------------------------------------------------------
 * library
 *
 * The FreeType library instance.  Although this is a DLL, it isn't supposed
 * to be shared by apps, as it is only called by the OS/2 GRE.  Therefore,
 * there is no need to bother with reentrancy/thread safety.
 */

FT_Library library;



/* --------------------------------------------------------------------------
 * free_elements
 */

static PListElement free_elements = 0;



/* --------------------------------------------------------------------------
 * liveFiles
 *
 * The list of live faces.
 */

static TList liveFiles  = { NULL, NULL, 0 };



/* --------------------------------------------------------------------------
 * idleFiles
 *
 * The list of sleeping faces
 */

static TList idleFiles = { NULL, NULL, 0 };



/* --------------------------------------------------------------------------
 * number of processes using the font driver; used by the init/term
 * routine
 */

ULONG ulProcessCount = 0;



/* --------------------------------------------------------------------------
 * current DBCS glyph-association font, if defined
 */

char szAssocFont[FACESIZE+3] = {0};



/* --------------------------------------------------------------------------
 * list of fonts currently aliased to DBCS combined fonts
 */

PSZ pszCmbAliases = NULL;


/* --------------------------------------------------------------------------
 * list of font files with specific flags set
 */

PSZ pszCfgFontFiles = NULL;


/* --------------------------------------------------------------------------
 * array of flag values for the above
PULONG pulCfgFileFlags = 0;
 */


/* --------------------------------------------------------------------------
 * Global variables corresponding to user-configurable settings
 */

/* Note, we'll only keep the first max_open_files files with opened
 * FreeType objects/instances..
 */
int  max_open_files = 12;

/* Bitmask of global user options (replaces old mess of booleans) */
ULONG fGlobalOptions = 0;

// DEPRECATE
/* flag for using fake TNR font */
//static BOOL fUseFacenameAlias = FALSE;

/* flag for using fake Bold for DBCS fonts */
//static BOOL fUseFakeBold = FALSE;

/* flag for unicode encoding */
//static BOOL fUseUnicodeEncoding = TRUE;

/* flag for using MBCS/DBCS flag with Unicode fonts */
//static BOOL fForceUnicodeMBCS = TRUE;

/* flag for disabling UNICODE glyphlist for the associate font */
//static BOOL fExceptAssocFont = TRUE;

/* flag for disabling UNICODE glyphlist for "combined" aliased fonts */
//static BOOL fExceptCombined = TRUE;

/* fixup level for broken style ID workaround */
//static BOOL fStyleFix = FALSE;
// DEPRECATE


/* --------------------------------------------------------------------------
 * array of font context handles. Note that there isn't more than one font
 * context open at any time anyway, but we want to be safe..
 */
#define MAX_CONTEXTS  5

static TFontSize contexts[ MAX_CONTEXTS ]; /* this is rather too much */


/* --------------------------------------------------------------------------
 * A few globals used for NLS
 *
 * Note: much of the internationalization (I18N) code was kindly provided
 *   by Ken Borgendale and Marc L Cohen from IBM (big thanks!).  Help was
 *   also received from Tetsuro Nishimura from IBM Japan.
 */

static ULONG  ScriptTag = -1;
static ULONG  LangSysTag = -1;
static ULONG  iLangId = TT_MS_LANGID_ENGLISH_UNITED_STATES; /* language ID  */
static ULONG  uLastGlyph = 255;                  /* last glyph for language */
static PSZ    pGlyphlistName = "SYMBOL";         /* PM383, PMJPN, PMKOR.... */
static BOOL   isGBK = TRUE;                      /* only used for Chinese   */
static ULONG  ulCp[2] = {1};                     /* codepages used          */
static UCHAR  DBCSLead[ 12 ];                    /* DBCS lead byte table    */

/* rather simple-minded test to decide if given glyph index is a 'halfchar',*/
/* i.e. Latin character in a DBCS font which is _not_ to be rotated         */
#define is_HALFCHAR(_x)  ((_x) < 0x0400)


/****************************************************************************/
/* PRIVATE FUNCTION DECLARATIONS                                            */
/****************************************************************************/

FT_Error FX_Flush_Stream( FT_Face face );
FT_Error FX_Activate_Stream( FT_Face face );

static PListElement New_Element( void );
static void         Done_Element( PListElement  element );
static int          List_Insert( PList  list,  PListElement  element );
static int          List_Remove( PList  list,  PListElement  element );
static PListElement List_Find( PList  list, long  key );

       int    kr_IsHanja( USHORT hj );
       USHORT kr_hj2hg( USHORT hj );
       BOOL   IsDBCSChar(UCHAR c);
static void   RemoveLastDBCSLead( char *str );
static BOOL   CheckDBCSEnc( PCH name, int len );
static int    mystricmp(const char *s1, const char *s2);
static char   *mystrrchr(char *s, char c);
       void   my_itoa(int num, char *cp);
       VOID   GetUdcInfo(VOID);

static int        Sleep_FontFile( PFontFile  cur_file );
static int        Wake_FontFile( PFontFile  cur_file );
static void       Done_FontFile( PFontFile  *file );
static PFontFile  New_FontFile( char*  file_name );
       PFontFile  getFontFile( HFF  hff );
static PFontSize  getFontSize( HFC  hfc );
       int        getUconvObject( UniChar *name, UconvObject *ConvObj, ULONG UconvType );
       void       CleanUCONVCache(void);
static int        PM2TT( FT_Face face, ULONG   mode, int     index);
static GLYPH      ReverseTranslate( PFontFace face, USHORT index );
static FT_Pos     getFakeBoldMetrics( FT_Pos     Width266, FT_Pos     Height266, FT_Bitmap* bitmapmetrics);
static FT_Error   buildFakeBoldBitmap( PVOID bitmap, ULONG buflength, int   oldwidth, int   addwidth, int   rows, int   cols );
static BOOL       CopyBitmap( CHAR *pSource, CHAR *pTarget, ULONG rows, ULONG s_pitch, ULONG t_pitch );

static void       OptionsInit(void);
static ULONG      LangInit(void);
       ULONG      FirstInit(void);
       ULONG      FinalTerm(void);

ULONG _System _DLL_InitTerm(ULONG hModule, ULONG ulFlag);

        LONG   interfaceSEId( TT_OS2 *pOS2, BOOL UDCflag, LONG encoding );
        BOOL   ConvertNameFromUnicode( USHORT langid, PSZ string, ULONG string_len, PSZ buffer, ULONG buffer_len );
static  char*  LookupName(FT_Face face,  int index );
static  ULONG  GetCharmap( FT_Face face );
static  int    GetOutlineLen(FT_Outline *ol);
static  void   Line_From(LONG x, LONG y);
static  void   Conic_From( LONG x0, LONG y0, LONG x2, LONG y2, LONG x1, LONG y1 );
static  void   Cubic_From( LONG x0, LONG y0, LONG cx1, LONG cy1, LONG cx2, LONG cy2 );
static  int    GetOutline(FT_Outline *ol, PBYTE pbuf);


/****************************************************************************/
/* FREETYPE INTERFACE WRAPPERS                                              */
/****************************************************************************/

#define STREAM_FILE( stream )  ( (HFILE)stream->descriptor.pointer )

/* Version 2 of FreeType removed direct access to the file stream from the  */
/* public API.  However, we need to be able to flush the stream on demand.  */
/* These routines are a cheat which allow us to do that, by accessing the   */
/* face object's own private stream management routines.  This is kind of   */
/* an evil thing to do, but we should be able to get away with it as we     */
/* don't have to worry about portability.                                   */


/* Close the underlying face stream without destroying the face object.
 */
FT_Error FX_Flush_Stream( FT_Face face )
{
    if ( STREAM_FILE( face->stream )) {
        DosSetFilePtr( STREAM_FILE( face->stream ),
                       0, FILE_CURRENT, &(face->stream->pos));
        face->stream->close( face->stream );
        COPY( "[FX_Flush_Stream] Closed stream with saved position: ");
        CATI( face->stream->pos ); CAT("\r\n"); WRITE;
    }
    return FT_Err_Ok;
}


/* Re-activate a previously flushed face: make sure the stream is re-opened
 * if necessary, and restore the saved file position.
 */
FT_Error FX_Activate_Stream( FT_Face face )
{
    FT_ULong ulPos;
    FT_Error error;

    ulPos = face->stream->pos;
    if ( ! STREAM_FILE( face->stream )) {
        error = FT_Stream_Open( face->stream, face->stream->pathname.pointer );
        if ( !error ) {
            COPY( "[FX_Activate_Stream] FT_Stream_Open successful. Restored position: ");
            DosSetFilePtr( STREAM_FILE( face->stream ),
                           ulPos, FILE_BEGIN, &(face->stream->pos));
            CATI( face->stream->pos ); CAT("\r\n"); WRITE;
        }
        else {
            COPY( "[FX_Activate_Stream] FT_Stream_Open failed: " );
            CATI( error ); CAT("\r\n"); WRITE;
        }
    }
    else
        error = FT_Err_Ok;
    return error;
}


/****************************************************************************/
/* LINKED LIST UTILITIES                                                    */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* New_Element :                                                            */
/*                                                                          */
/*   return a fresh list element. Either new or recycled.                   */
/*   returns NULL if out of memory.                                         */
/*                                                                          */
static
PListElement New_Element( void )
{
    PListElement e = free_elements;

    if ( e )
        free_elements = e->next;
    else {
        if ( ALLOC( e, sizeof( TListElement )))
            return NULL;
    }
    e->next = e->prev = e->data = NULL;
    e->key  = 0;

    return e;
}


/****************************************************************************/
/*                                                                          */
/*  Done_Element :                                                          */
/*                                                                          */
/*    recycles an old list element                                          */
/*                                                                          */
static
void Done_Element( PListElement element )
{
  element->next = free_elements;
  free_elements = element;
}


/****************************************************************************/
/*                                                                          */
/*  List_Insert :                                                           */
/*                                                                          */
/*    inserts a new object at the head of a given list                      */
/*    returns 0 in case of success, -1 otherwise.                           */
/*                                                                          */
static
int List_Insert( PList list, PListElement element )
{
    if (!list || !element)
        return -1;

    element->next = list->head;

    if (list->head)
        list->head->prev = element;

    element->prev = NULL;
    list->head    = element;

    if (!list->tail)
        list->tail = element;

    list->count++;
    return 0;
}


/****************************************************************************/
/*                                                                          */
/*  List_Remove :                                                           */
/*                                                                          */
/*    removes an element from its list. Returns 0 in case of success,       */
/*    -1 otherwise. WARNING : this function doesn't check that the          */
/*    element is part of the list.                                          */
/*                                                                          */
static
int List_Remove( PList list, PListElement element )
{
    if (!element)
        return -1;

    if (element->prev)
        element->prev->next = element->next;
    else
        list->head = element->next;

    if (element->next)
        element->next->prev = element->prev;
    else
        list->tail = element->prev;

    element->next = element->prev = NULL;
    list->count --;
    return 0;
}


/****************************************************************************/
/*                                                                          */
/*  List_Find :                                                             */
/*                                                                          */
/*    Look for a given object with a specified key. Returns NULL if the     */
/*    list is empty, or the object wasn't found.                            */
/*                                                                          */
static
PListElement List_Find( PList list, long key )
{
    static PListElement  cur;

    for ( cur=list->head; cur; cur = cur->next )
        if ( cur->key == key )
            return cur;

    // not found
    return NULL;
}


/****************************************************************************/
/* DBCS UTILITIES                                                           */
/****************************************************************************/

/* To support Korean font without Hanja */

#define HANGULCODE      0       /* Hangul Code */
#define NOOFHANJA       1       /* Number of Hanja with same sound of Hangul */
#define NOOFACCHANJA    2       /* Number of accumulated Hanaj */

/* Hangul and Hanaja converting table */
static USHORT kr_hanjaTbl[474][3] = {
    { 45217U,  29,     0 }, { 45218U,  11,    29 }, { 45219U,  24,    40 },
    { 45221U,  10,    64 }, { 45224U,  20,    74 }, { 45225U,   6,    94 },
    { 45229U,  24,   100 }, { 45235U,  20,   124 }, { 45236U,   2,   144 },
    { 45243U,   4,   146 }, { 45245U,   1,   150 }, { 45253U,  17,   151 },
    { 45255U,  12,   168 }, { 45257U,   4,   180 }, { 45259U,   7,   184 },
    { 45260U,   3,   191 }, { 45268U,   3,   194 }, { 45277U,   7,   197 },
    { 45279U,  11,   204 }, { 45281U,   6,   215 }, { 45282U,   6,   221 },
    { 45286U,  45,   227 }, { 45288U,  24,   272 }, { 45293U,  39,   296 },
    { 45294U,   7,   335 }, { 45295U,  10,   342 }, { 45297U,   3,   352 },
    { 45304U,  16,   355 }, { 45305U,   1,   371 }, { 45306U,  12,   372 },
    { 45307U,   4,   384 }, { 45308U,  17,   388 }, { 45309U,   4,   405 },
    { 45476U,  13,   409 }, { 45477U,   3,   422 }, { 45483U,   9,   425 },
    { 45490U,   4,   434 }, { 45491U,  25,   438 }, { 45496U,  54,   463 },
    { 45497U,   6,   517 }, { 45498U,   6,   523 }, { 45500U,   4,   529 },
    { 45507U,   6,   533 }, { 45511U,  10,   539 }, { 45512U,   5,   549 },
    { 45515U,   6,   554 }, { 45517U,   6,   560 }, { 45524U,  15,   566 },
    { 45525U,   7,   581 }, { 45526U,   1,   588 }, { 45528U,   7,   589 },
    { 45529U,  15,   596 }, { 45531U,   1,   611 }, { 45533U,  14,   612 },
    { 45534U,   7,   626 }, { 45536U,   4,   633 }, { 45538U,  64,   637 },
    { 45540U,   1,   701 }, { 45542U,   4,   702 }, { 45544U,   1,   706 },
    { 45987U,   1,   707 }, { 45994U,  15,   708 }, { 45995U,   8,   723 },
    { 45997U,   9,   731 }, { 45999U,   2,   740 }, { 46002U,   9,   742 },
    { 46003U,   5,   751 }, { 46006U,   7,   756 }, { 46011U,   6,   763 },
    { 46019U,   1,   769 }, { 46048U,   1,   770 }, { 46050U,   3,   771 },
    { 46052U,   4,   774 }, { 46055U,   2,   778 }, { 46059U,  18,   780 },
    { 46060U,   6,   798 }, { 46061U,   1,   804 }, { 46067U,   7,   805 },
    { 46074U,   6,   812 }, { 46242U,   1,   818 }, { 46249U,   8,   819 },
    { 46251U,   1,   827 }, { 46253U,   1,   828 }, { 46266U,   2,   829 },
    { 46273U,   2,   831 }, { 46278U,   1,   833 }, { 46281U,   6,   834 },
    { 46287U,   2,   840 }, { 46288U,   2,   842 }, { 46297U,   2,   844 },
    { 46300U,  20,   846 }, { 46302U,   5,   866 }, { 46307U,  17,   871 },
    { 46308U,   5,   888 }, { 46311U,  11,   893 }, { 46315U,  16,   904 },
    { 46316U,   1,   920 }, { 46326U,   2,   921 }, { 46517U,  40,   923 },
    { 46518U,  10,   963 }, { 46519U,  10,   973 }, { 46521U,   2,   983 },
    { 46527U,  17,   985 }, { 46542U,  11,  1002 }, { 46544U,   6,  1013 },
    { 46566U,   1,  1019 }, { 46574U,   9,  1020 }, { 46835U,   9,  1029 },
    { 46836U,   9,  1038 }, { 46837U,   9,  1047 }, { 46838U,   2,  1056 },
    { 46839U,  10,  1058 }, { 46840U,   3,  1068 }, { 46843U,   8,  1071 },
    { 47009U,   4,  1079 }, { 47017U,   2,  1083 }, { 47019U,   1,  1085 },
    { 47022U,  13,  1086 }, { 47041U,  18,  1099 }, { 47042U,   7,  1117 },
    { 47043U,  12,  1124 }, { 47044U,   6,  1136 }, { 47045U,   5,  1142 },
    { 47046U,   1,  1147 }, { 47049U,  18,  1148 }, { 47050U,   5,  1166 },
    { 47054U,  18,  1171 }, { 47055U,   7,  1189 }, { 47056U,   1,  1196 },
    { 47061U,   7,  1197 }, { 47066U,   8,  1204 }, { 47073U,  12,  1212 },
    { 47078U,   1,  1224 }, { 47079U,  13,  1225 }, { 47097U,  14,  1238 },
    { 47098U,   3,  1252 }, { 47099U,   6,  1255 }, { 47100U,   4,  1261 },
    { 47266U,   1,  1265 }, { 47268U,   2,  1266 }, { 47271U,   1,  1268 },
    { 47274U,   6,  1269 }, { 47278U,  26,  1275 }, { 47280U,   9,  1301 },
    { 47282U,   5,  1310 }, { 47283U,   4,  1315 }, { 47286U,   8,  1319 },
    { 47287U,   6,  1327 }, { 47288U,  19,  1333 }, { 47291U,   7,  1352 },
    { 47297U,  12,  1359 }, { 47301U,  14,  1371 }, { 47302U,   5,  1385 },
    { 47309U,   6,  1390 }, { 47336U,   2,  1396 }, { 47337U,  11,  1398 },
    { 47338U,   2,  1409 }, { 47341U,  15,  1411 }, { 47343U,   1,  1426 },
    { 47344U,  24,  1427 }, { 47345U,   7,  1451 }, { 47348U,   2,  1458 },
    { 47353U,   3,  1460 }, { 47526U,  12,  1463 }, { 47531U,  22,  1475 },
    { 47532U,   2,  1497 }, { 47534U,  12,  1499 }, { 47536U,   3,  1511 },
    { 47564U,  19,  1514 }, { 47566U,  13,  1533 }, { 47568U,   3,  1546 },
    { 47578U,  19,  1549 }, { 47581U,  25,  1568 }, { 47583U,  11,  1593 },
    { 47590U,  28,  1604 }, { 47592U,  20,  1632 }, { 47593U,   8,  1652 },
    { 47608U,  10,  1660 }, { 47610U,   4,  1670 }, { 47612U,   9,  1674 },
    { 47613U,   2,  1683 }, { 47790U,  11,  1685 }, { 47791U,   7,  1696 },
    { 47792U,   4,  1703 }, { 47796U,  17,  1707 }, { 47800U,  16,  1724 },
    { 47801U,  17,  1740 }, { 47803U,   1,  1757 }, { 47804U,   1,  1758 },
    { 47808U,  16,  1759 }, { 47822U,  43,  1775 }, { 47823U,   1,  1818 },
    { 47824U,  19,  1819 }, { 47826U,   5,  1838 }, { 47832U,   6,  1843 },
    { 47857U,  43,  1849 }, { 47859U,  14,  1892 }, { 47865U,   4,  1906 },
    { 48103U,  60,  1910 }, { 48104U,   4,  1970 }, { 48106U,  12,  1974 },
    { 48108U,   5,  1986 }, { 48111U,   8,  1991 }, { 48112U,   4,  1999 },
    { 48115U,  31,  2003 }, { 48117U,   3,  2034 }, { 48118U,   5,  2037 },
    { 48125U,   5,  2042 }, { 48301U,  30,  2047 }, { 48302U,  15,  2077 },
    { 48305U,  32,  2092 }, { 48307U,  13,  2124 }, { 48310U,   8,  2137 },
    { 48311U,   4,  2145 }, { 48314U,  18,  2149 }, { 48316U,   9,  2167 },
    { 48338U,  37,  2176 }, { 48339U,   9,  2213 }, { 48341U,   6,  2222 },
    { 48342U,   1,  2228 }, { 48347U,   8,  2229 }, { 48354U,   5,  2237 },
    { 48360U,   2,  2242 }, { 48374U,  61,  2244 }, { 48375U,  12,  2305 },
    { 48376U,  27,  2317 }, { 48378U,   4,  2344 }, { 48382U,   3,  2348 },
    { 48573U,   3,  2351 }, { 48576U,   5,  2354 }, { 48578U,  10,  2359 },
    { 48579U,  28,  2369 }, { 48580U,  15,  2397 }, { 48581U,  24,  2412 },
    { 48583U,   4,  2436 }, { 48585U,  10,  2440 }, { 48586U,   3,  2450 },
    { 48598U,   1,  2453 }, { 48830U,   1,  2454 }, { 48838U,  18,  2455 },
    { 48839U,  14,  2473 }, { 48840U,  10,  2487 }, { 48843U,   4,  2497 },
    { 48847U,   8,  2501 }, { 48848U,   4,  2509 }, { 48851U,   7,  2513 },
    { 48854U,  11,  2520 }, { 48855U,   7,  2531 }, { 48862U,   4,  2538 },
    { 48863U,  11,  2542 }, { 48864U,   9,  2553 }, { 48871U,  31,  2562 },
    { 48878U,  10,  2593 }, { 48879U,   5,  2603 }, { 48880U,   6,  2608 },
    { 48883U,   2,  2614 }, { 48886U,   6,  2616 }, { 48887U,   2,  2622 },
    { 49059U,   1,  2624 }, { 49065U,  24,  2625 }, { 49066U,  13,  2649 },
    { 49068U,  43,  2662 }, { 49069U,  10,  2705 }, { 49072U,  15,  2715 },
    { 49073U,   4,  2730 }, { 49077U,  40,  2734 }, { 49081U,  24,  2774 },
    { 49088U,  30,  2798 }, { 49089U,   5,  2828 }, { 49090U,   6,  2833 },
    { 49091U,   1,  2839 }, { 49099U,   9,  2840 }, { 49101U,   8,  2849 },
    { 49103U,  18,  2857 }, { 49104U,   1,  2875 }, { 49109U,   5,  2876 },
    { 49110U,   4,  2881 }, { 49116U,   5,  2885 }, { 49124U,  38,  2890 },
    { 49125U,   6,  2928 }, { 49131U,  24,  2934 }, { 49132U,  32,  2958 },
    { 49133U,   9,  2990 }, { 49134U,  13,  2999 }, { 49135U,   3,  3012 },
    { 49141U,   2,  3015 }, { 49144U,  27,  3017 }, { 49145U,   3,  3044 },
    { 49319U,  25,  3047 }, { 49327U,  56,  3072 }, { 49328U,   7,  3128 },
    { 49329U,  13,  3135 }, { 49330U,   5,  3148 }, { 49334U,   5,  3153 },
    { 49338U,   7,  3158 }, { 49339U,   1,  3165 }, { 49341U,   6,  3166 },
    { 49342U,   3,  3172 }, { 49344U,   4,  3175 }, { 49351U,  19,  3179 },
    { 49356U,  38,  3198 }, { 49357U,   8,  3236 }, { 49358U,  24,  3244 },
    { 49359U,   9,  3268 }, { 49363U,  11,  3277 }, { 49364U,   5,  3288 },
    { 49367U,   4,  3293 }, { 49370U,  26,  3297 }, { 49371U,  13,  3323 },
    { 49372U,   5,  3336 }, { 49377U,   6,  3341 }, { 49378U,   1,  3347 },
    { 49381U,  37,  3348 }, { 49383U,  17,  3385 }, { 49391U,   4,  3402 },
    { 49402U,  28,  3406 }, { 49403U,  25,  3434 }, { 49404U,  41,  3459 },
    { 49405U,   8,  3500 }, { 49569U,   9,  3508 }, { 49570U,   3,  3517 },
    { 49572U,  55,  3520 }, { 49574U,  23,  3575 }, { 49590U,  46,  3598 },
    { 49591U,   4,  3644 }, { 49592U,   2,  3648 }, { 49593U,   3,  3650 },
    { 49598U,  17,  3653 }, { 49602U,   5,  3670 }, { 49611U,   1,  3675 },
    { 49622U,  40,  3676 }, { 49623U,   2,  3716 }, { 49624U,  19,  3718 },
    { 49625U,   1,  3737 }, { 49631U,   4,  3738 }, { 49647U,   1,  3742 },
    { 49649U,   1,  3743 }, { 49651U,   3,  3744 }, { 49653U,  11,  3747 },
    { 49654U,  34,  3758 }, { 49655U,   5,  3792 }, { 49656U,  35,  3797 },
    { 49658U,  15,  3832 }, { 49660U,   2,  3847 }, { 49661U,   7,  3849 },
    { 49825U,   3,  3856 }, { 49911U,  15,  3859 }, { 49912U,   7,  3874 },
    { 49913U,  15,  3881 }, { 49915U,   5,  3896 }, { 49916U,  10,  3901 },
    { 50082U,  22,  3911 }, { 50084U,  12,  3933 }, { 50085U,   4,  3945 },
    { 50099U,   4,  3949 }, { 50100U,  15,  3953 }, { 50101U,  19,  3968 },
    { 50102U,  10,  3987 }, { 50103U,  10,  3997 }, { 50104U,  10,  4007 },
    { 50107U,   8,  4017 }, { 50108U,  10,  4025 }, { 50122U,  27,  4035 },
    { 50123U,   6,  4062 }, { 50124U,   3,  4068 }, { 50129U,  11,  4071 },
    { 50132U,   1,  4082 }, { 50134U,   3,  4083 }, { 50143U,  23,  4086 },
    { 50144U,  12,  4109 }, { 50145U,   3,  4121 }, { 50146U,   3,  4124 },
    { 50150U,   6,  4127 }, { 50153U,   3,  4133 }, { 50155U,  15,  4136 },
    { 50168U,   5,  4151 }, { 50174U,   1,  4156 }, { 50337U,  24,  4157 },
    { 50338U,   3,  4181 }, { 50339U,   1,  4184 }, { 50341U,   3,  4185 },
    { 50343U,   9,  4188 }, { 50344U,   1,  4197 }, { 50346U,   2,  4198 },
    { 50408U,   1,  4200 }, { 50616U,  14,  4201 }, { 50617U,  16,  4215 },
    { 50618U,  10,  4231 }, { 50619U,   2,  4241 }, { 50621U,   4,  4243 },
    { 50622U,   3,  4247 }, { 50625U,   5,  4250 }, { 50626U,  14,  4255 },
    { 50627U,   3,  4269 }, { 50634U,   1,  4272 }, { 50637U,   1,  4273 },
    { 50660U,   4,  4274 }, { 50667U,   7,  4278 }, { 50672U,   6,  4285 },
    { 50677U,   6,  4291 }, { 50863U,   2,  4297 }, { 50868U,   1,  4299 },
    { 50884U,  16,  4300 }, { 50887U,   9,  4316 }, { 50888U,   3,  4325 },
    { 50896U,  11,  4328 }, { 50904U,   4,  4339 }, { 50906U,   1,  4343 },
    { 50925U,  10,  4344 }, { 50927U,   1,  4354 }, { 50930U,   5,  4355 },
    { 50931U,  10,  4360 }, { 50935U,  28,  4370 }, { 50936U,   6,  4398 },
    { 51109U,  14,  4404 }, { 51120U,   2,  4418 }, { 51123U,   5,  4420 },
    { 51143U,   7,  4425 }, { 51146U,  10,  4432 }, { 51148U,   2,  4442 },
    { 51151U,  14,  4444 }, { 51152U,   5,  4458 }, { 51153U,  14,  4463 },
    { 51154U,   2,  4477 }, { 51156U,  12,  4479 }, { 51157U,   7,  4491 },
    { 51159U,  17,  4498 }, { 51160U,  18,  4515 }, { 51161U,   2,  4533 },
    { 51168U,   5,  4535 }, { 51170U,   9,  4540 }, { 51171U,   4,  4549 },
    { 51173U,   4,  4553 }, { 51174U,   1,  4557 }, { 51176U,   2,  4558 },
    { 51189U,   4,  4560 }, { 51190U,  21,  4564 }, { 51191U,   4,  4585 },
    { 51192U,   1,  4589 }, { 51193U,  12,  4590 }, { 51196U,  20,  4602 },
    { 51197U,   9,  4622 }, { 51363U,  41,  4631 }, { 51364U,   3,  4672 },
    { 51365U,   6,  4675 }, { 51366U,   3,  4681 }, { 51371U,  10,  4684 },
    { 51373U,  14,  4694 }, { 51374U,   6,  4708 }, { 51375U,  17,  4714 },
    { 51376U,   5,  4731 }, { 51378U,  24,  4736 }, { 51384U,  20,  4760 },
    { 51385U,   2,  4780 }, { 51390U,   3,  4782 }, { 51391U,  13,  4785 },
    { 51396U,  13,  4798 }, { 51398U,  10,  4811 }, { 51403U,   1,  4821 },
    { 51405U,   4,  4822 }, { 51409U,   3,  4826 }, { 51414U,   8,  4829 },
    { 51422U,   5,  4837 }, { 51425U,   3,  4842 }, { 51428U,   5,  4845 },
    { 51430U,   1,  4850 }, { 51431U,   4,  4851 }, { 51434U,   4,  4855 },
    { 51436U,   3,  4859 }, { 51437U,   4,  4862 }, { 51439U,   1,  4866 },
    { 51441U,  20,  4867 }, { 51450U,   1,  4887 }, {     0U,   0,  4888 },
};


/****************************************************************************/
/*                                                                          */
/* kr_IsHanja :                                                             */
/*                                                                          */
/*   a function to check whether Korean Hanja or not.                        */
int kr_IsHanja( USHORT hj )
{
    UCHAR first = hj >> 8, second = hj & 0xFF;

    return (( first >= 0xCA ) && ( first <= 0xFD ) &&
            ( second >= 0xA1 ) && ( second <= 0xFE ));
}


/****************************************************************************/
/*                                                                          */
/* kr_hj2hg :                                                               */
/*                                                                          */
/*   a function to convert Korean Hanja to phonetic Hangul.                 */
USHORT kr_hj2hg( USHORT hj )
{
    int hi, lo;
    int pos;
    int i;

    hi = (( hj >> 8 ) - 0xCA ) * 94;
    lo = ( hj & 0x00FF ) - 0xA1;
    pos = hi + lo;

    for (i = 0; i < 473; i++)
        if ( pos < kr_hanjaTbl[i+1][NOOFACCHANJA]) break;

    return kr_hanjaTbl[i][HANGULCODE];
}


/****************************************************************************/
/*                                                                          */
/* IsDBCSChar :                                                             */
/*                                                                          */
/* Returns TRUE if character is first byte of a DBCS char, FALSE otherwise  */
/*                                                                          */
BOOL IsDBCSChar(UCHAR c)
{
    ULONG i;

    for (i = 0; DBCSLead[i] && DBCSLead[i+1]; i += 2)
        if ((c >= DBCSLead[i]) && (c <= DBCSLead[i+1]))
            return TRUE;
    return FALSE;
}


/****************************************************************************/
/*                                                                          */
/* RemoveLastDBCSLead :                                                     */
/*                                                                          */
/*   Remove last truncated DBCS lead character                              */
/*                                                                          */
static void RemoveLastDBCSLead( char *str )
{
    for( ; *str; str++ )
    {
        if( IsDBCSChar( *str ))
        {
            if( *( str + 1 ))
                str++;
            else
                *str = 0;
        }
    }
}


/****************************************************************************/
/*                                                                          */
/* CheckDBCSEnc :                                                           */
/*                                                                          */
/*   Make sure a string is really DBCS-encoded (and not a Unicode string    */
/* that's been mis-identified as one).  This is mainly a workaround for     */
/* some Japanese system fonts included with OS/2-J which commit just that   */
/* particular crime.                                                        */
/*                                                                          */
/*   Note: this check is far from foolproof, it's just a quick-and-dirty    */
/* sanity check.  It's mainly only effective for Japanese strings, and even */
/* then only if they contain fullwidth-halfwidth forms, or plain ASCII.     */
/*                                                                          */
static BOOL CheckDBCSEnc( PCH name, int len )
{
    int i;
    for ( i = 0; i < len; i++ )
        if ( name[i] == 0xFF || (i < len-1 && name[i] == 0 ))
            return FALSE;
    return TRUE;
}


/****************************************************************************/
/* OTHER UTILITY FUNCTIONS                                                  */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* mystricmp :                                                              */
/*                                                                          */
/* A simple function for comparing strings without case sensitivity. Just   */
/* returns zero if strings match, one otherwise. I wrote this because       */
/* stricmp is not available in the subsystem run-time library (probably     */
/* because it uses locales). toupper() is unfortunately unavailable too.    */
/*                                                                          */

#define toupper( c ) ( ((c) >= 'a') && ((c) <= 'z') ? (c) - 'a' + 'A' : (c) )

static
int mystricmp(const char *s1, const char *s2)
{
   int i = 0;
   int match = 0;
   int len = strlen(s1);

   if (len != strlen(s2))
      return 1;   /* no match */

   while (i < len) {
      if (toupper(s1[i]) != toupper(s2[i])) {
         match = 1;
         break;
      }
      i++;
   }
   return match;
}


/* DBCS enabled strrchr (only looks for SBCS chars though) */
static
char *mystrrchr(char *s, char c)
{
    int i = 0;
    int lastfound = -1;
    int len = strlen(s);

    while (i <= len) {
        if (IsDBCSChar(s[i])) {
            i += 2;
            continue;
        }
        if (s[i] == c)
            lastfound = i;
        i++;
    }
    if (lastfound == -1)
        return NULL;
    else
        return s + lastfound;
}


/****************************************************************************/
/* my_itoa is used only in the following function GetUdcInfo.               */
/* Works pretty much like expected.                                         */
/*                                                                          */
void my_itoa(int num, char *cp)
{
    char temp[10];
    int  i = 0;

    do {
        temp[i++] = (num % 10) + '0';
        num /= 10;
    } while (num);                  /* enddo */

    while (i--) {
        *cp++ = temp[i];
    }                               /* endwhile */
    *cp = '\0';
}


/****************************************************************************/
/* CONFIGURATION MANAGEMENT ROUTINES                                        */
/****************************************************************************/


/****************************************************************************/
/* GetUdcInfo determines the UDC ranges used                                */
/*                                                                          */
/* NOTE: UDC == User Defined Characters, a feature of DBCS systems which    */
/*       allows users to define their own characters in custom bitmap fonts */
/*       (basically if they need a rarely-used kanji/hanzi/hanja that isn't */
/*       defined in any codepage/character set).                            */
/*                                                                          */
/*       Not supported yet.                                                 */
/*                                                                          */
static
VOID GetUdcInfo( VOID )
{
#if 0
    static CHAR szCpStr[10] = "CP";
    static CHAR szNlsFmProfile[] = " :\\OS2\\SYSDATA\\PMNLSFM.INI";
    static CHAR szUdcApp[] = "UserDefinedArea";

    ULONG   ulUdc, ulUdcInfo, i;
    PVOID   gPtr;
    HINI    hini;
    typedef struct _USUDCRANGE {
        USHORT  usStart;
        USHORT  usEnd;
    } USUDCRANGE;
    USUDCRANGE *pUdcTemp;

    DosQueryCp(sizeof(ulCp), (ULONG*)&ulCp, &i);  /* find out default codepage */
    my_itoa((INT) ulCp, szCpStr + 2);             /* convert to ASCII          */

    DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &i, sizeof( ULONG ));
    szNlsFmProfile[0] = 'A' + i - 1;

    if (( hini = PrfOpenProfile((HAB) 0L, szNlsFmProfile)) != NULLHANDLE )
    {
        if (PrfQueryProfileSize(hini, szUdcApp, szCpStr, &ulUdc) == TRUE) {
            if ( !ALLOC( pUdcTemp, ulUdc) && pUdcTemp )
            {
                if ( PrfQueryProfileData( hini, szUdcApp, szCpStr,
                                          pUdcTemp, &ulUdc) == TRUE )
                    ;   // TODO
                if (pUdcTemp) free(pUdcTemp);
            }
        }
    }
#endif
}

/****************************************************************************/
/* The OptionsInit function reads OS2.INI to get various user-configurable  */
/* settings used by the IFI driver.                                         */
/*                                                                          */
static  void OptionsInit( void )
{
    char          *c;
    int           iVal;
    unsigned long cb;

    // Get the max-open-files (font cache) limit
    max_open_files = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                                         "OpenFaces", 12 );
    if ( max_open_files == 0 )
        max_open_files = 12;        // reasonable default

    if ( max_open_files < 8 )       // ensure limit isn't too low
        max_open_files = 8;


    iVal = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                               "Use_Facename_Alias", FALSE );
    if ( iVal ) fGlobalOptions |= FL_OPT_ALIAS_TMS_RMN;

    iVal = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                               "Use_Unicode_Encoding", TRUE );
    if ( iVal ) fGlobalOptions |= FL_OPT_FORCE_UNICODE;

    iVal = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                               "Set_Unicode_MBCS", TRUE );
    if ( iVal ) fGlobalOptions |= FL_OPT_FORCE_UNI_MBCS;

    iVal = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                               "Use_Generated_Facename", TRUE );
    if ( iVal ) fGlobalOptions |= FL_OPT_ALT_FACENAME;

    iVal = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                               "Style_Fixup", FALSE );
    if ( iVal ) fGlobalOptions |= FL_OPT_STYLE_FIX;

/*
   fExceptAssocFont = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                                           "Force_DBCS_Association", TRUE );
   fExceptCombined = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                                         "Force_DBCS_Combined", TRUE );
   fStyleFix = PrfQueryProfileInt( HINI_USERPROFILE, IFI_PROFILE_APP,
                                   "Style_Fixup", FALSE );
   if ( fStyleFix > 1 )
      fAlwaysFix = TRUE;
*/

    // Get a list of all fonts which have specific flags set
    cb = 0;
    if ( PrfQueryProfileSize( HINI_USERPROFILE,
                              IFI_PROFILE_FONTFLAGS, NULL, &cb ) &&
         ! ALLOC( pszCfgFontFiles, ++cb ))
    {
        if ( ! PrfQueryProfileData( HINI_USERPROFILE, IFI_PROFILE_APP,
                                    NULL, pszCfgFontFiles, &cb ))
            FREE( pszCfgFontFiles );
    }

    // See if there's a DBCS association font in use
    PrfQueryProfileString( HINI_USERPROFILE, "PM_SystemFonts", "PM_AssociateFont",
                           NULL, szAssocFont, sizeof(szAssocFont));
    if (( c = mystrrchr(szAssocFont, ';')) != NULL ) *c = '\0';

    // See if there are any DBCS "combined font" aliases defined
    cb = 0;
    if ( PrfQueryProfileSize(HINI_USERPROFILE, "PM_ABRFiles", NULL, &cb ) &&
         ! ALLOC( pszCmbAliases, ++cb ))
    {
        if ( ! PrfQueryProfileData( HINI_USERPROFILE, "PM_ABRFiles",
                                    NULL, pszCmbAliases, &cb ))
            FREE( pszCmbAliases );
    }
}


/****************************************************************************/
/* FONT/FACE MANAGEMENT ROUTINES                                            */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* Sleep_FontFile :                                                         */
/*                                                                          */
/*   Closes a font file's FreeType objects to free room in memory.          */
/*                                                                          */
static
int Sleep_FontFile( PFontFile cur_file )
{
    int i;

    if ( !( cur_file->flags & FL_FLAG_LIVE_FACE ))
        ERRRET(-1);                                 // already asleep!

    // Is this font in use?
    if ( cur_file->flags & FL_FLAG_CONTEXT_OPEN ) {
        // Move this face to top of the live list
        if ( List_Remove( &liveFiles, cur_file->element ))
            ERRRET(-1);
        if ( List_Insert( &liveFiles, cur_file->element ))
            ERRRET(-1);
        // Now choose another file to drop (from the new end of the list)
        cur_file = (PFontFile)(liveFiles.tail->data);
    }

    // Remove the chosen face from the live list
    if ( List_Remove( &liveFiles, cur_file->element ))
        ERRRET(-1);

    // Add it to the sleep list
    if ( List_Insert( &idleFiles, cur_file->element ))
        ERRRET(-1);

    // Deactivate its objects - we ignore errors here
    for ( i = 0; i < cur_file->numFaces; i++ ) {

        // FT_Done_Face may expect the stream to be open, so make sure it is...
        FX_Activate_Stream( cur_file->faces[i].face );

        // Now tell FreeType to close down the face
        FT_Done_Face( cur_file->faces[i].face );

        // Also free the kerning directory, which may be quite large
        if ( cur_file->faces[i].directory != NULL )
            FREE( cur_file->faces[i].directory );
    }

    // Update the state flag and return
    cur_file->flags &= ~FL_FLAG_LIVE_FACE;
    return 0;
}


/****************************************************************************/
/*                                                                          */
/* Wake_FontFile :                                                          */
/*                                                                          */
/*   Awakens a font file, and reloads important face data.                  */
/*                                                                          */
static
int Wake_FontFile( PFontFile cur_file )
{
    static FT_Face   face;
    static PFontFace cur_face;
    FT_Error         error;
    FT_ULong         cb;
    FT_Int           cmap_idx;
    ULONG            encoding, i;


    if ( cur_file->flags & FL_FLAG_LIVE_FACE )
        ERRRET(-1);                         // already awake!

    COPY ("[Wake_FontFile] Calling FT_New_Face on ");
    CAT( cur_file->filename ); CAT("\r\n"); WRITE;

    // Try to activate the FreeType objects
    error = FT_New_Face( library, cur_file->filename, 0, &face );
    if ( error ) {
        COPY("[Wake_FontFile] Error while opening ");
        CAT( cur_file->filename );  CAT(", error code = "); CATI( error );
        CAT("\r\n");    WRITE;

        ERRRET(-1); // error, can't open file
                    // TODO: should set error condition here?
    }
    COPY ("[Wake_FontFile]  - face 1 of "); CATI( face->num_faces );
    CAT(" opened successfully.\r\n");   WRITE;

    // Now determine the best charmap for this font
    encoding = GetCharmap( face );
    cmap_idx = encoding & 0xFFFF;       // charmap index within the font
    COPY("[Wake_FontFile]  - got encoding = " );
    CATI( encoding >> 16 ); CAT(" / charmap "); CATI( cmap_idx );
    CAT("\r\n");    WRITE;

    // Tell FreeType to select this charmap
    error = FT_Set_Charmap( face, face->charmaps[cmap_idx] );
    if ( error ) {
        COPY("[Wake_FontFile]  - error: No charmap in ");
        CAT( cur_file->filename ); CAT("\r\n" );   WRITE;
        ERET1( Fail_Face );
    }

    // All right, now remove the face from the sleep list
    if ( List_Remove( &idleFiles, cur_file->element ))
        ERET1( Fail_Face );

    // Add it to the live list
    if ( List_Insert( &liveFiles, cur_file->element ))
        ERET1( Fail_Face );

    // If the file is a TTC, the first face is now opened successfully.

    cur_file->numFaces = face->num_faces;

    // Now allocate memory for face data (one struct for each face in a TTC).
    if ( cur_file->faces == NULL ) {
       if ( ALLOC(cur_face, sizeof(TFontFace) * cur_file->numFaces ))
            ERET1( Fail_Face );

       cur_file->faces  = cur_face;
    }
    else
       cur_face = cur_file->faces;

    cur_face->face   = face;  /* possibly first face in a TTC */
    cur_file->flags |= FL_FLAG_LIVE_FACE;

    if (!(cur_file->flags & FL_FLAG_ALREADY_USED)) {
        cur_face->charMode  = encoding >> 16;     // TRANSLATE_xxx

        /* If a face contains more than this number of glyphs, it's likely
         * a 'DBCS' (i.e. CJK) font - VERY probable.
         * Note: The newer MS core fonts (Arial, Times New Roman) contain
         * over 1200 glyphs hence the limit is 3072.  Real DBCS fonts
         * probably have more glyphs than that.
         *
         * Have also added a check to see if the font contains at least one
         * of the following basic CJK glyphs. If it doesn't have at least one
         * of these then it almost certainly can't be a CJK font.
         */

        if (( face->num_glyphs > AT_LEAST_DBCS_GLYPH ) &&
            (( FT_Get_Char_Index( face, 0x3105 ) != 0 ) ||      // pinyin
             ( FT_Get_Char_Index( face, 0xAC00 ) != 0 ) ||      // hangul
             ( FT_Get_Char_Index( face, 0x3042 ) != 0 ) ||      // hiragana
             ( FT_Get_Char_Index( face, 0x65E5 ) != 0 )))       // kanji/hanzi/hanja
        {
            cur_file->flags |= FL_FLAG_DBCS_FILE;
            cur_face->flags |= FC_FLAG_DBCS_FACE;
        }

        cur_face->widths      = NULL;
        cur_face->kernIndices = NULL;
    }

#ifdef KERN_TABLE_SUPPORT
    /* Load the kerning table, if there is one.  We use our own custom
     * routine for this, since the FreeType API no longer exposes the table.
     */
    error = FX_Get_Kerning_Pairs( cur_face->face, &(cur_face->directory) );
    if ( !error )
    {
        COPY("[Wake_FontFile]  - successfully queried kerning pairs: ");
        CATI(cur_face->directory->nPairs); CAT(" pairs found.\r\n");
        WRITE;
    }
    else
#endif
       cur_face->directory = NULL;

    // Close the font file as FT_New_Face won't expect it to be open already
    FX_Flush_Stream(face);

    // Open remaining faces if this font is a TrueType Collection (TTC)
    for (i = 1; i < cur_file->numFaces; i++) {
        error = FT_New_Face( library, cur_file->filename, i, &face );
        if ( error )
            return -1;   /* TODO: handle bad TTCs more tolerantly */

        COPY ("[Wake_FontFile]  - face ");  CATI( i+1 );  CAT(" of ");
        CATI( face->num_faces );  CAT(" opened successfully.\r\n");
        WRITE;

        // Now repeat the earlier steps with the additional faces
        encoding = GetCharmap( face );
        cmap_idx = encoding & 0xFFFF;
        COPY( "[Wake_FontFile]  - got encoding = " ); CATI( encoding >> 16 );
        CAT( " / charmap " ); CATI( cmap_idx ); CAT(  "\r\n" ); WRITE;

        error = FT_Set_Charmap(face, face->charmaps[cmap_idx]);
        if ( error )
            ERET1( Fail_Face );

        cur_face = &(cur_file->faces[i]);
        cur_face->face = face;

        if ( !( cur_file->flags & FL_FLAG_ALREADY_USED )) {
            cur_face->charMode = encoding >> 16;

            if (cur_file->flags & FL_FLAG_DBCS_FILE)
                cur_face->flags |= FC_FLAG_DBCS_FACE;

            cur_face->widths      = NULL;
            cur_face->kernIndices = NULL;
        }
        cb = 0;

#ifdef KERN_TABLE_SUPPORT
        error = FX_Get_Kerning_Pairs( cur_face->face, &(cur_face->directory) );
        if ( !error )
        {
            COPY("[Wake_FontFile]  - successfully queried kerning pairs: ");
            CATI(cur_face->directory->nPairs); CAT(" pairs found.\r\n");
            WRITE;
        }
        else
#endif
            cur_face->directory = NULL;

        FX_Flush_Stream( face );
    }

    // This indicates that some fields won't need re-init next time
    cur_file->flags |= FL_FLAG_ALREADY_USED;

    return 0;    // everything is in order, return 0 == success


Fail_Face:
    FT_Done_Face(face);

    /* Note that in case of error (e.g. out of memory), the face stays
     * on the sleeping list.
     */
    return -1;
}


/****************************************************************************/
/*                                                                          */
/* Done_FontFile :                                                          */
/*                                                                          */
/*   Destroys a given font file object. This will also destroy all of its   */
/*   live child font sizes (which in turn will destroy the glyph caches).   */
/*   This is done for all faces if the file is a collection.                */
/*                                                                          */
/* WARNING : The font face must be removed from its list by the caller      */
/*           before this function is called.                                */
/*                                                                          */
static
void Done_FontFile( PFontFile *file )
{
    static PListElement  element;
    static PListElement  next;
           ULONG         i;

    if ( (*file)->flags & FL_FLAG_LIVE_FACE )
    {
        for (i = 0; i < (*file)->numFaces; i++) {
            FT_Done_Face( (*file)->faces[i].face );

            if ((*file)->faces[i].directory)
                FREE((*file)->faces[i].directory);
            if ((*file)->faces[i].widths)
                FREE((*file)->faces[i].widths);
            if ((*file)->faces[i].kernIndices)
                FREE((*file)->faces[i].kernIndices);
        }
    }
    FREE( (*file)->faces );
    FREE( *file );
}


/****************************************************************************/
/*                                                                          */
/* New_FontFile :                                                           */
/*                                                                          */
/*   Return the address of the TFontFile corresponding to a given           */
/*   HFF.  Note that in our implementation, we could simply do a            */
/*   typecast like '(PFontFile)hff'.  However, for safety reasons,          */
/*   we look up the handle in the list.                                     */
/*                                                                          */
static
PFontFile New_FontFile( PSZ file_name )
{
    static PListElement  element;
    static PFontFile     cur_file;

    // First, check if it's already open - in the live list
    for ( element = liveFiles.head; element; element = element->next ) {
        cur_file = (PFontFile)element->data;
        if ( strcmp( cur_file->filename, file_name ) == 0 )
            goto Exit_Same;
    }

    // Check in the idle list
    for ( element = idleFiles.head; element; element = element->next ) {
        cur_file = (PFontFile)element->data;
        if ( strcmp( cur_file->filename, file_name ) == 0 )
            goto Exit_Same;
    }

    COPY("[New_FontFile] "); CAT ( file_name );
    CAT(" not in live or idle list. Creating new font.\r\n");
    WRITE;

    /* OK, this file hasn't been opened before.  Create a new font face object
     * then try to wake it up.  This will fail if the file can't be found, or
     * if we lack memory.
     */

    element = New_Element();
    if ( !element )
        ERRRET(NULL);

    if ( ALLOC( cur_file, sizeof(TFontFile) ) )
        ERET1( Fail_Element );

    element->data        = cur_file;
    element->key         = (long)cur_file;         // use the HFF as cur key

    cur_file->element    = element;
    cur_file->ref_count  = 1;
    cur_file->hff        = (HFF)cur_file;
    strcpy( cur_file->filename, file_name );
    cur_file->flags      = fGlobalOptions & 0xFF0000;
    cur_file->faces      = NULL;

    // See if this font file has any user-configured flags defined
    if ( pszCfgFontFiles ) {
       PSZ pszCur,
           pszName;
       int iVal;

       pszCur = pszCfgFontFiles;
       pszName = strrchr( file_name, '\\') + 1;
       while ( pszCur && *pszCur && pszName && *pszName ) {
          if ( !mystricmp( pszCur, pszName )) {
             iVal = PrfQueryProfileInt( HINI_USERPROFILE,
                                        IFI_PROFILE_FONTFLAGS, pszCur, 0L );
             // per-font settings override the global ones, if defined
             cur_file->flags = iVal & 0xFF0000;
             break;
          }
          pszCur += strlen( pszCur ) + 1;
       }
    }

    // Add new font face element to idle list
    if ( List_Insert( &idleFiles, element ))
        ERET1( Fail_File );

    // Make enough room in the live list
    if ( liveFiles.count >= max_open_files ) {
        COPY( "[New_FontFile]  - rolling live list...\n" ); WRITE;
        if ( Sleep_FontFile( (PFontFile)(liveFiles.tail->data) ))
            ERET1( Fail_File );
    }

    // Now wake the new font file
    if ( Wake_FontFile( cur_file )) {
        COPY( "[New_FontFile] Could not open/wake " ); CAT( file_name );
        CAT( "\r\n" ); WRITE;
        if ( List_Remove( &idleFiles, element ))
            ERET1( Fail_File );
        ERET1( Fail_File );
   }

   return cur_file;      // everything is in order

Fail_File:
   FREE( cur_file );

Fail_Element:
   Done_Element( element );
   return  NULL;

Exit_Same:
   cur_file->ref_count++;  /* increment reference count */

   COPY( "[New_FontFile]  -> (duplicate) hff = " ); CATI( cur_file->hff );
   CAT( "\r\n" ); WRITE;

   return cur_file;        /* no sense going on */
}


/****************************************************************************/
/*                                                                          */
/* getFontFile :                                                            */
/*                                                                          */
/*   Return the address of the TFontFile corresponding to a given           */
/*   HFF.  If asleep, the file and its face object(s) is awoken.            */
/*                                                                          */
PFontFile getFontFile( HFF hff )
{
    static PListElement  element;

    // Look in the live list first
    element = List_Find( &liveFiles, (long)hff );
    if ( element ) {
        // Move it to the front of the live list - if it isn't already
        if ( liveFiles.head != element ) {
            if ( List_Remove( &liveFiles, element ) )
                ERRRET( NULL );
            if ( List_Insert( &liveFiles, element ) )
                ERRRET( NULL );
        }
        return (PFontFile)(element->data);
    }

    // The file may be asleep, look in the second list
    element = List_Find( &idleFiles, (long)hff );
    if ( element ) {
        /* We need to awake the font, but first we must make sure there is
        /* enough room in the live list
        */
        if ( liveFiles.count >= max_open_files )
            if ( Sleep_FontFile( (PFontFile)(liveFiles.tail->data) ))
                ERRRET( NULL );

        if ( Wake_FontFile( (PFontFile)(element->data) ))
            ERRRET( NULL );

        COPY ("hff "); CATI( hff ); CAT(" awoken\r\n"); WRITE;
        return (PFontFile)(element->data);
    }

    COPY( "Could not find hff " ); CATI( hff ); CAT( " in lists\n" ); WRITE;

#ifdef DEBUG
    /* dump files lists */
    COPY( "Live files : " ); CATI( liveFiles.count ); CAT( "\r\n" ); WRITE;
    for ( element = liveFiles.head; element; element = element->next ) {
        COPY( ((PFontFile)(element->data))->filename ); CAT("\r\n"); WRITE;
    }
    COPY( "Idle files : " ); CATI( idleFiles.count ); CAT( "\r\n" ); WRITE;
    for ( element = idleFiles.head; element; element = element->next ) {
        COPY( ((PFontFile)(element->data))->filename ); CAT("\r\n"); WRITE;
    }
#endif

    // Could not find the HFF
    return NULL;
}


/****************************************************************************/
/*                                                                          */
/* getFontSize :                                                            */
/*                                                                          */
/*   Return pointer to a TFontSize given a HFC handle, NULL if error        */
/*                                                                          */
static
PFontSize getFontSize( HFC  hfc )
{
    int i;
    for ( i = 0; i < MAX_CONTEXTS; i++ )
        if ( contexts[i].hfc == hfc ) {
            return ( &contexts[i] );
        }

    return NULL;
}


/* maximum number of cached UCONV objects */
#define MAX_UCONV_CACHE   10

/* UCONV object used for conversion from UGL to Unicode */
#define UCONV_TYPE_UGL    1

/* UCONV objects used for conversion from local DBCS codepage to Unicode */
#define UCONV_TYPE_BIG5     2
#define UCONV_TYPE_SJIS     4
#define UCONV_TYPE_WANSUNG  8
#define UCONV_TYPE_GB2312   16
#define UCONV_TYPE_SYSTEM   32

/* UCONV objects cache entry */
typedef struct  _UCACHEENTRY {
    UconvObject   object;      /* actual UCONV object */
    PID           pid;         /* process ID the object is valid for */
    ULONG         type;        /* type of UCONV object (UGL or DBCS) */
} UCACHEENTRY, *PUCACHEENTRY;

/* UCONV globals */
static UCACHEENTRY    UconvCache[MAX_UCONV_CACHE];  /* 10 should do it */
static int            slotsUsed = 0;     /* number of cache slots used */


/****************************************************************************/
/*                                                                          */
/* getUconvObject :                                                         */
/*                                                                          */
/*   A function to cache UCONV objects based on current process. Now the    */
/*   _DLL_InitTerm function has been enhanced to destroy no longer needed   */
/*   UCONV object when a process terminates.                                */
/*                                                                          */
int getUconvObject(UniChar *name, UconvObject *ConvObj, ULONG UconvType)
{
   PPIB ppib;   /* process/thread info blocks */
   PTIB ptib;
   PID  curPid; /* current process ID */
   int  i;

   /* query current process ID */
   if (DosGetInfoBlocks(&ptib, &ppib))
      return -1;

   curPid = ppib->pib_ulpid;

   if (slotsUsed == 0) {     /* initialize cache */
      if (UniCreateUconvObject(name, ConvObj) != ULS_SUCCESS)
         return -1;
      UconvCache[0].object = *ConvObj;
      UconvCache[0].pid    = curPid;
      UconvCache[0].type   = UconvType;

      for (i = 1; i < MAX_UCONV_CACHE; i++) {
         UconvCache[i].object = NULL;
         UconvCache[i].pid    = 0;
      }
      slotsUsed = 1;
      return 0;
   }

   /* search cache for available conversion object */
   i = 0;
   while ((UconvCache[i].pid != curPid || UconvCache[i].type != UconvType)
          && i < slotsUsed)
      i++;

   if (i < slotsUsed) {  /* entry found in cache */
      *ConvObj = UconvCache[i].object;
      return 0;
   }

   /* if cache is full, remove first entry and shift the others 'down' */
   if (slotsUsed == MAX_UCONV_CACHE) {
#if 0    /* this code cause a crash in UCONV.DLL */
      UniFreeUconvObject(UconvCache[0].object);
#endif
      for (i = 1; i < MAX_UCONV_CACHE; i++) {
         UconvCache[i - 1].object = UconvCache[i].object;
         UconvCache[i - 1].pid    = UconvCache[i].pid;
         UconvCache[i - 1].type   = UconvCache[i].type;
      }
   }

   if (UniCreateUconvObject(name, ConvObj) != ULS_SUCCESS)
      return -1;

   if (slotsUsed < MAX_UCONV_CACHE)
      slotsUsed++;

   UconvCache[slotsUsed - 1].object = *ConvObj;
   UconvCache[slotsUsed - 1].pid    = curPid;
   UconvCache[slotsUsed - 1].type   = UconvType;

   return 0;
}


/****************************************************************************/
/*                                                                          */
/* CleanUCONVCache :                                                        */
/*                                                                          */
/*  When process is terminated, removes this process' entries in the UCONV  */
/* object cache. Errors are disregarded at this point.                      */
/*                                                                          */
void CleanUCONVCache(void)
{
   PPIB                  ppib;   /* process/thread info blocks */
   PTIB                  ptib;
   PID                   curPid; /* current process ID */
   int                   i = 0, j;

   /* query current process ID */
   if (DosGetInfoBlocks(&ptib, &ppib))
      return;

   curPid = ppib->pib_ulpid;

   while (i < slotsUsed) {
      /* if PID matches, remove the entry and shift the others 'down' (or up?) */
      if (UconvCache[i].pid == curPid) {
         UniFreeUconvObject(UconvCache[i].object);
         for (j = i + 1; j < slotsUsed; j++) {
            UconvCache[j - 1].object = UconvCache[j].object;
            UconvCache[j - 1].pid    = UconvCache[j].pid;
            UconvCache[j - 1].type   = UconvCache[j].type;
         }
         slotsUsed--;
      }
      i++;
   }
}


/****************************************************************************/
/*                                                                          */
/* PM2TT :                                                                  */
/*                                                                          */
/*   a function to convert PM codepoint to TT glyph index. This is the real */
/*   tricky part.                                                           */
/*    mode = TRANSLATE_UGL - translate UGL to Unicode                       */
/*    mode = TRANSLATE_SYMBOL - no translation - symbol font                */
/*    mode = TRANSLATE_UNICODE- no translation - Unicode                    */
static  int PM2TT( FT_Face face,
                   ULONG   mode,
                   int     index)
{
   /* UCONV objects created in one process can't be used in another.     */
   /* Therefore we keep a small cache of recently used UCONV objects.    */
   static UconvObject   UGLObj = NULL; /* UGL->Unicode conversion object */
   static UconvObject   DBCSObj = NULL; /*DBCS->Unicode conversion object*/
   static BOOL          UconvSet         = FALSE;
          char          char_data[4], *pin_char_str;
          size_t        in_bytes_left, uni_chars_left, num_subs;
          UniChar       *pout_uni_str, uni_buffer[4];
          int           rc;
   static UniChar       uglName[]        = L"OS2UGL@endian=big:system";
   static UniChar       uglNameBig5[]    = L"IBM-950@endian=big:system";
   static UniChar       uglNameSJIS[]    = L"IBM-943@endian=big:system";
   static UniChar       uglNameWansung[] = L"IBM-949@endian=big:system";
   static UniChar       uglNameGB2312[]  = L"IBM-1381@endian=big:system";    /* Probably */

   switch (mode) {
      case TRANSLATE_UGL:
         /* get Uconv object - either new or cached */
         if (getUconvObject(uglName, &UGLObj, UCONV_TYPE_UGL) != 0)
            return 0;

         /* perform the UGL -> Unicode conversion */
         char_data[0] = index & 0xFF;
         char_data[1] = index >> 8;
         char_data[2] = 0;

         pout_uni_str = uni_buffer;
         pin_char_str = char_data;
         in_bytes_left = 2;
         uni_chars_left = 2;

         rc = UniUconvToUcs(UGLObj, (void**)&pin_char_str, &in_bytes_left,
                            &pout_uni_str, &uni_chars_left,
                            &num_subs);

         COPY("[PM2TT] UGL translation : PM2TT mode = "); CATI( mode ); CAT(" index = "); CATI( index );
         CAT(" unicode = "); CATI( uni_buffer[ 0 ]); CAT(" rc = "); CATI( rc );
         CAT( " result = "); CATI( FT_Get_Char_Index( face, uni_buffer[ 0 ]));
         CAT("\r\n"); WRITE;

         if (rc != ULS_SUCCESS)
            return 0;
         else
            return FT_Get_Char_Index(face, uni_buffer[0]);

      case TRANSLATE_BIG5:
      case TRANSLATE_SJIS:
      case TRANSLATE_WANSUNG:
      case TRANSLATE_GB2312:
         /* These DBCS glyphlists require a bit of translation: while any
          * double-byte values (above 0x8000) are the same as the corresponding
          * DBCS encoding (Shift-JIS, Big-5, Wansung or GB2312), all lesser
          * values are actually UGL indices.
          */
         if ((index > 31) && (index < 128))
             ;  // no need to adjust ASCII characters; just fall through
         else if ((mode == TRANSLATE_SJIS) && (index > 769) && (index < 833))
             // remap UGL katakana to halfwidth ShiftJIS set
             index -= 609;
         else if (index < 950) {
             /* for UGL values, we need to convert UGL -> UCS, then UCS -> DBCS
              * encoding.  First we get the required UconvObjects...
              */
             if (getUconvObject(uglName, &UGLObj, UCONV_TYPE_UGL) != 0)
                return 0;
             switch (mode) {
                case TRANSLATE_BIG5:
                    COPY("TRANSLATE_BIG5 (PMCHT) ");
                    rc = getUconvObject(uglNameBig5, &DBCSObj, UCONV_TYPE_BIG5 );
                    break;
                case TRANSLATE_SJIS:
                    COPY("TRANSLATE_SJIS (PMJPN) ");
                    rc = getUconvObject(uglNameSJIS, &DBCSObj, UCONV_TYPE_SJIS );
                    break;
                case TRANSLATE_WANSUNG:
                    COPY("TRANSLATE_WANSUNG (PMKOR): ");
                    rc = getUconvObject(uglNameWansung, &DBCSObj, UCONV_TYPE_WANSUNG );
                    break;
                case TRANSLATE_GB2312:
                    COPY("TRANSLATE_BIG5 (PMPRC) ");
                    rc = getUconvObject(uglNameGB2312, &DBCSObj, UCONV_TYPE_GB2312 );
                    break;
             }
             CAT("index = "); CATI( index ); CAT(": "); WRITE;
             if (rc != 0)
                return 0;

             /* perform the UGL -> Unicode conversion */
             char_data[0] = index & 0xFF;
             char_data[1] = index >> 8;
             char_data[2] = 0;

             pout_uni_str = uni_buffer;
             pin_char_str = char_data;
             in_bytes_left = 2;
             uni_chars_left = 2;

             rc = UniUconvToUcs(UGLObj, (void**)&pin_char_str, &in_bytes_left,
                               &pout_uni_str, &uni_chars_left,
                               &num_subs);
             COPY("UniUconvToUcs="); CATI( rc ); CAT(", "); WRITE;

             /* now perform the UCS -> SJIS/Wansung/Big-5/GB2312 conversion */
             if (rc == ULS_SUCCESS) {
                pout_uni_str = uni_buffer; // input UCS string
                pin_char_str = char_data;  // actually the output string here
                in_bytes_left  = 3;        // actually no. of output characters
                uni_chars_left = 1;        // no. of input UniChars
                num_subs = 0;
                rc = UniUconvFromUcs(DBCSObj, &pout_uni_str, &uni_chars_left,
                                     (void**)&pin_char_str, &in_bytes_left, &num_subs);
                COPY("UniUconvFromUcs="); CATI( rc ); CAT("\r\n"); WRITE;
                if (rc == ULS_SUCCESS)
                    index = char_data[1] ?
                            ((short)char_data[0] << 8) | char_data[1] :
                            char_data[0];

             }
         }
         // now continue as below (no break)

      case TRANSLATE_SYMBOL:
      case TRANSLATE_UNICODE:

         rc = FT_Get_Char_Index( face, index );

         COPY("[PM2TT] Passthrough mode = "); CATI( mode ); CAT(" index = "); CATI( index );
         CAT(" result = "); CATI( rc );
         CAT("\r\n"); WRITE;

         if( rc != 0 )
            return rc;

         /* If the character wasn't found we do some fallback logic here */

#if 0
         if(( mode == TRANSLATE_WANSUNG ) && kr_IsHanja( index ))
         {
            /* For Korean only: if the missing character was a hanja ideograph,
             * substitute the equivalent hangul.
             */
            COPY("[PM2TT]  - convert Hanja to Hangul\r\n"); WRITE;

            index = kr_hj2hg( index );
         }
         else if(( mode > TRANSLATE_UNICODE ) && (( index == 190 ) || ( index == 314 ) || ( index == 947 )))
         {
            /* If the missing character (UGL index) was an Asian currency symbol
             * (yen/yuan/won), then substitute the backslash (which is mapped
             * to the currency sign under most CJK codepages anyway).
             */
            COPY("[PM2TT]  - convert Asian currency sign (UGL "); CATI( index );
            CAT(") to ASCII 92 (backslash)\r\n"); WRITE;

            index = 92;
         }
         else if(( mode == TRANSLATE_UNICODE ) && (( index == 0xA5 ) || ( index == 0x20A9 )))
         {
            /* Same as the above, except the incoming codepoint is UCS-2 */
            COPY("[PM2TT]  - convert Asian currency sign (UCS "); CATI( index );
            CAT(") to ASCII 92 (backslash)\r\n"); WRITE;

            index = 92;
         }
         else
#endif
         {
            COPY("[PM2TT]  - character not found, PM2TT mode = "); CATI( mode );
            CAT(" index = "); CATI( index );
            CAT("\r\n"); WRITE;
            return 0;
         }

         COPY("[PM2TT] Passthrough mode = "); CATI( mode ); CAT(" index = "); CATI( index );
         CAT(" result = "); CATI( FT_Get_Char_Index( face, index ));
         CAT("\r\n"); WRITE;

         return FT_Get_Char_Index( face, index );


      case TRANSLATE_UNI_SJIS:
      case TRANSLATE_UNI_BIG5:
      case TRANSLATE_UNI_WANSUNG:
      case TRANSLATE_UNI_GB2312:
         if( index <= 949 /*2070*/ ) /* for Universal Glyph List 2070 */
         {
             if ((mode == TRANSLATE_UNI_SJIS) && (index > 769) && (index < 833)) {
                 /* for UGL katakana, remap to halfwidth character values and
                  * translate as ShiftJIS
                  */
                 index -= 609;
                 if (getUconvObject(uglNameSJIS, &UGLObj, UCONV_TYPE_SJIS) != 0)
                     return 0;
             }
             else
                if (getUconvObject( uglName, &UGLObj, UCONV_TYPE_UGL) != 0)
                   return 0;
         }
         else
         {
            /* get Uconv object - either new or cached */
            switch (mode) {
                /* get proper conversion object */
                case TRANSLATE_UNI_BIG5:
                    if (getUconvObject(uglNameBig5, &UGLObj, UCONV_TYPE_BIG5) != 0)
                        return 0;
                    break;

                case TRANSLATE_UNI_SJIS:
                    if (getUconvObject(uglNameSJIS, &UGLObj, UCONV_TYPE_SJIS) != 0)
                        return 0;
                    break;

                case TRANSLATE_UNI_WANSUNG:
                    if (getUconvObject(uglNameWansung, &UGLObj, UCONV_TYPE_WANSUNG) != 0)
                        return 0;
                break;

                case TRANSLATE_UNI_GB2312:
                    if (getUconvObject(uglNameGB2312, &UGLObj, UCONV_TYPE_GB2312) != 0)
                        return 0;
                break;
            }
         }

         for(;;)
         {
            /* Note the bytes are swapped here for double byte chars! */
            if (( index & 0xFF00 ) && ( index > 949 /*2070*/ )) {
               char_data[0] = (index & 0xFF00) >> 8;
               char_data[1] = index & 0x00FF;
            }
            else {
                   char_data[0] = index & 0xFF;
                   char_data[1] = index >> 8;
            }

            pout_uni_str = uni_buffer;
            pin_char_str = char_data;
            in_bytes_left = 2;
            uni_chars_left = 2;
            rc = UniUconvToUcs(UGLObj, (void**)&pin_char_str, &in_bytes_left,
                               &pout_uni_str, &uni_chars_left,
                               &num_subs);

            COPY("[PM2TT] Translation mode = "); CATI( mode ); CAT(" index = "); CATI( index );
            CAT(" unicode = "); CATI( uni_buffer[ 0 ]); CAT(" rc = "); CATI( rc );
            CAT( " result = "); CATI( FT_Get_Char_Index( face, uni_buffer[0]));
            CAT("\r\n"); WRITE;

            if (rc != ULS_SUCCESS)
                return 0;

            rc = FT_Get_Char_Index( face, uni_buffer[0]);
#if 0
            if(( rc == 0 ) && ( mode == TRANSLATE_UNI_WANSUNG ) && kr_IsHanja( index ))
            {
                /* For Korean only: if the missing character was a hanja ideograph,
                 * substitute the equivalent hangul.
                 */
                COPY("[PM2TT]  - convert Hanja to Hangul\r\n"); WRITE;

                index = kr_hj2hg( index );
            }
            else if(( rc == 0 ) && (( index == 190 ) || ( index == 314 ) || ( index == 947 )))
            {
                /* If the missing character was the Asian currency symbol
                 * (yen/yuan/won), then substitute the backslash (which is mapped
                 * to the currency sign under most CJK codepages anyway).
                 */
                COPY("[PM2TT]  - convert Asian currency sign (UGL "); CATI( index );
                CAT(") to ASCII 92 (backslash)\r\n"); WRITE;

                index = 92;
            }
            else
#endif
                break;
         }

         return rc;

      default:
         return 0;
   }
}


/* TOODO: review for Aurora */
/*#define MAX_KERN_INDEX  504*/

/****************************************************************************/
/*                                                                          */
/* ReverseTranslate :                                                       */
/*                                                                          */
/* Translate a physical glyph index within the font file into a glyphlist   */
/* index usable by PM.  This is basically a brute-force approach: we loop   */
/* through the entire glyphlist until we find the corresponding character   */
/* in the font file.  (This information will be cached by the caller so we  */
/* should only have to do this once for any given face/glyph value.)        */
/*                                                                          */
static
GLYPH ReverseTranslate( PFontFace face, USHORT index )
{
    ULONG  i;
    GLYPH  newidx = 0;

    /* TODO: enable larger fonts.  MAX_GLYPH here assumes use of the UGL
     * glyphlist, which won't always be the case. We should presumably set
     * a variable maximum depending on the current charMode.
     */
    for ( i = 0; i < MAX_GLYPH; i++ ) {
        COPY("PM2TT Called by 'ReverseTranslate'\r\n"); WRITE;
        newidx = PM2TT( face->face,
                        face->charMode,
                        i );
        if ( newidx == index )
            break;
    }
    if ( i < MAX_GLYPH )
        return i;
    else
        return 0;
}


/****************************************************************************/
/*                                                                          */
/* getFakeBoldMetrics :                                                     */
/*                                                                          */
/* A simple function for fake bold metrics calculation                      */
/* returns added width in 26.6 format and bitmap metrics for fake bold face */
static
FT_Pos getFakeBoldMetrics( FT_Pos     Width266,
                           FT_Pos     Height266,
                           FT_Bitmap* bitmapmetrics)
{
   FT_Pos addedWid266=0;

   if ( Height266 )
      addedWid266 = (Height266 * 9) / 100;
   else if ( Width266 )
      addedWid266 = (Width266 * 9) / 100;

   if ( addedWid266 < (1 << 6) )
      addedWid266 = 1 << 6;
   if ( addedWid266 > (7 << 6) )
      addedWid266 = 7 << 6;

   if ( bitmapmetrics ) {
      bitmapmetrics->width  = ( Width266 + addedWid266 ) >> 6;
      bitmapmetrics->rows   = Height266 >> 6;
      bitmapmetrics->pitch  = ((bitmapmetrics->width + 31) / 8) & -4;
      bitmapmetrics->buffer = NULL;
   }

   return addedWid266;
}

/****************************************************************************/
/*                                                                          */
/* buildFakeBoldBitmap :                                                    */
/*                                                                          */
/* convert existing normal bitmap image to bold faced bitmap image          */
/* this function assumes that unused buffer space was set to zero           */
static
FT_Error buildFakeBoldBitmap( PVOID bitmap,
                              ULONG buflength,
                              int   oldwidth,
                              int   addwidth,
                              int   rows,
                              int   cols )
{
   unsigned char *pbyte, bits;
   int  i, j, row, col, newcols;

   if ( (addwidth < 1) || (addwidth > 7) )
      return FT_Err_Invalid_Argument;

   newcols = ((oldwidth + addwidth + 31) / 8) & -4;

   if ( (newcols*rows) > buflength )
      return FT_Err_Out_Of_Memory;

   if ( newcols > cols ) {
      for (row=rows-1; row >= 0 ; row--) {
         for (col=cols-1; col >= 0; col--) {
            pbyte = (unsigned char*)bitmap + ((row*cols)+col);
            bits = *pbyte;
            *pbyte = 0;
            pbyte = (unsigned char*)bitmap + ((row*newcols)+col);
            *pbyte = bits;
         }
      }
   }

   for (i=(rows*newcols)-1; i >= 0; i--) {
      pbyte = (unsigned char*)bitmap + i;
      bits = *pbyte;
      for (j=1; j <= addwidth; j++) {
         *pbyte |= bits >> j;
         if ( ((i+1) % newcols) != 0 )
            *(pbyte+1) |= (~(~0<<j) & bits) << (8-j);
      }
   }

   return FT_Err_Ok;
}


/****************************************************************************/
/*                                                                          */
/* CopyBitmap                                                               */
/*                                                                          */
/* Copy a glyph bitmap from source to target buffer, converting it to the   */
/* specified number of bytes per row (a.k.a. 'pitch') if necessary.  The    */
/* target buffer's pitch (t_pitch) must be equal to or greater than that of */
/* the source buffer (s_pitch).                                             */
/*                                                                          */
/* The target buffer must have been allocated previously.  It must have an  */
/* allocated length of at least (rows * t_pitch) bytes.                     */
/*                                                                          */
static
BOOL CopyBitmap( CHAR *pSource, CHAR *pTarget,
                 ULONG rows, ULONG s_pitch, ULONG t_pitch )
{
    ULONG row, col, ofs;

    if (( pSource && !pTarget ) || ( t_pitch < s_pitch ))     // sanity check
    {
        COPY("[CopyBitmap] Invalid parameter:  pSource = ");
        CATI( (int)pSource );
        CAT(", pTarget = ");    CATI( (int)pTarget );
        CAT(", rows = ");       CATI( rows );
        CAT(", s_pitch = ");    CATI( s_pitch );
        CAT(", t_pitch = ");    CATI( t_pitch );
        CAT("\r\n");            WRITE;
        return FALSE;
    }
    ofs = 0;
    for ( row = 0; row < rows; row++ ) {
        for ( col = 0; col < s_pitch; col++ ) {
            pTarget[ ofs++ ] = pSource[ col + (row * s_pitch) ];
        }
        for ( ; col < t_pitch; col++ ) pTarget[ ofs++ ] = 0;
    }
    return TRUE;
}


/* -------------------------------------------------------------------------*/
/* here begins the exported functions                                       */
/* -------------------------------------------------------------------------*/


/****************************************************************************/
/*                                                                          */
/* FdConvertFontFile :                                                      */
/*                                                                          */
/*  Install/delete font file.  We handle TT/OT fonts as they are, so there  */
/*  isn't any actual 'conversion' necessary - just copy the file to its     */
/*  destination.                                                            */
/*                                                                          */
LONG _System FdConvertFontFile( PSZ  source,
                                PSZ  dest_dir,
                                PSZ  new_name )
{
   PSZ  source_name;

   COPY("FdConvertFontFile: Src = "); CAT(source);
   if (dest_dir) {
      CAT(", DestDir = "); CAT(dest_dir);
   }
   CAT("\r\n"); WRITE;

   if (dest_dir && new_name)
   {
     /* install the font file */
     source_name = mystrrchr( source, '\\' );  /* find the last backslash */
     if (!source_name)
       ERRRET(-1);

     source_name++;
     strcpy( new_name, source_name );

     /* check if file is to be copied onto itself */
     if (strncmp(source, dest_dir, strlen(dest_dir)) == 0)
        return OK;  /* do nothing */

     if ( DosCopy( source, dest_dir, DCPY_EXISTING) )  /* overwrite file */
       ERRRET(-1);

      COPY(" -> Name: "); CAT(new_name); CAT("\r\n"); WRITE;
   }
   else
   {
      COPY("Delete file "); CAT(source); CAT("\r\n"); WRITE;
      DosDelete(source);  /* fail quietly */
   }

   return OK;
}


/****************************************************************************/
/*                                                                          */
/* FdLoadFontFile :                                                         */
/*                                                                          */
/*  open a font file and return a handle for it                             */
/*                                                                          */
HFF _System FdLoadFontFile( PSZ file_name )
{
   PSZ           extension;
   PFontFile     cur_file;
   PListElement  element;
   ULONG         i;

   COPY( "[FdLoadFontFile] " ); CAT( file_name ); CAT( "\r\n" ); WRITE;

   /* first check if the file extension is supported */
   extension = mystrrchr( file_name, '.' );  /* find the last dot */
   if ( extension == NULL ||
        (mystricmp(extension, ".TTF") &&
         mystricmp(extension, ".TTC") &&
         mystricmp(extension, ".OTF")))
   {
     COPY( "[FdLoadFontFile]  - Unsupported file extension.\r\n" ); WRITE;
     return ((HFF)-1);
   }

   /* now actually open the file */
   cur_file = New_FontFile( file_name );

   if (cur_file)
      return  cur_file->hff;
   else
     return (HFF)-1;
}


/****************************************************************************/
/*                                                                          */
/* FdUnloadFontFile :                                                       */
/*                                                                          */
/*  destroy resources associated with a given HFF                           */
/*                                                                          */
LONG _System FdUnloadFontFile( HFF hff )
{
   PListElement  element;

   COPY("[FdUnloadFontFile] hff = "); CATI((int) hff); CAT("\r\n"); WRITE;

   /* look in the live list first */
   for (element = liveFiles.head; element; element = element->next)
   {
     if (element->key == (long)hff)
     {
       PFontFile  file = (PFontFile)element->data;

       if (--file->ref_count > 0)  /* don't really close, return OK */
         return 0;

       List_Remove( &liveFiles, element );
       Done_Element( element );
       Done_FontFile( &file );
       return 0;
     }
   }

   /* now look in sleep list */
   for (element = idleFiles.head; element; element = element->next)
   {
     if (element->key == (long)hff)
     {
       PFontFile  file = (PFontFile)element->data;

       if (--file->ref_count > 0)  /* don't really close, return OK */
         return 0;

       List_Remove( &idleFiles, element );
       Done_Element( element );
       Done_FontFile( &file );
       return 0;
     }
   }

   /* didn't find the file */
   return -1;
}


/****************************************************************************/
/*                                                                          */
/* FdQueryFaces :                                                           */
/*                                                                          */
/*   Return font metrics. This routine has to do a lot of not very          */
/*   hard work.                                                             */
/*                                                                          */
LONG _System FdQueryFaces( HFF          hff,
                           PIFIMETRICS  pifiMetrics,
                           ULONG        cMetricLen,
                           ULONG        cFontCount,
                           ULONG        cStart)
{
   static IFIMETRICS           ifi;         /* temporary structure */
          PIFIMETRICS          pifi2;
          PIFIMETRICS          pifi3;
          PFontFile            file;
          PFontFace            pface;

          TT_OS2               *pOS2;
          TT_Postscript        *ppost;
          FT_Error             error;
          FT_ULong             cbTable;

          LONG                 index, faceIndex, ifiCount = 0;
          ULONG                range1, range2;
          USHORT               fsTrunc;
          SHORT                name_len;
          SHORT                ifimet_len;
          BOOL                 fCMB;
          BOOL                 fUni;
          char                 *name, *style;
          char                 *pszabr;
#if 0
   static char                 *wrongName = "Wrong Name Font";
#endif

   COPY( "[FdQueryFaces] hff = " ); CATI( hff );
   CAT(  ", cFontCount = " );       CATI( cFontCount );
   CAT(  ", cStart = " );           CATI( cStart );
   CAT(  ", cMetricLen = " );       CATI( cMetricLen );
   WRITE;

   file = getFontFile(hff);
   if (!file) {
      /* error, invalid handle */
      COPY( " - Invalid handle!\r\n"); WRITE;
      ERRRET(-1)
   }

   COPY( " Filename: " ); CAT( file->filename ); CAT("\r\n"); WRITE;

   pface = &(file->faces[0]);
   if (cMetricLen == 0) {   /* only number of faces is requested */
      LONG rc = file->numFaces;

      if ( fGlobalOptions & FL_OPT_ALIAS_TMS_RMN ) {
        /* create an alias for Times New Roman */
        name = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
        if ( name && !strcmp(name, "Times New Roman")) {
            file->flags |= FL_FLAG_FACENAME_ALIAS;
            rc *= 2;
        }
      }

      if (file->flags & FL_FLAG_DBCS_FILE)
      {
         COPY( "[FdQueryFaces]  - file is DBCS\r\n" ); WRITE;
         if ( file->flags & FL_OPT_FAKE_BOLD )
            rc *= 3;
         else
            rc *= 2;
      }

      return rc;
   }

   /* See if this font is aliased to a CMB file */
   fCMB = FALSE;
   if ( pszCmbAliases ) {
      pszabr = pszCmbAliases;
      name = strrchr( file->filename, '\\') + 1;
      while ( pszabr && *pszabr && name && *name ) {
         if ( !mystricmp( pszabr, name )) {
            fCMB = TRUE;
            break;
         }
         pszabr += strlen( pszabr ) + 1;
      }
   }

   // we'll need this repeatedly later on, so just calculate it now
   ifimet_len = sizeof( IFIMETRICS );

   for (faceIndex = 0; faceIndex < file->numFaces; faceIndex++) {
      /* get pointer to this face's data */
      pface = &(file->faces[faceIndex]);

      COPY( "[FdQueryFaces]  - getting information on face " ); CATI( faceIndex ); CAT("\r\n"); WRITE;

      /* make sure the stream is active */
      error = FX_Activate_Stream( pface->face );
      if ( error )
      {
         COPY( "[FdQueryFaces]  - failed to activate stream!\r\n" ); WRITE;
         continue;
      }

      /* get the OS/2 and PostScript tables */
#if 0
      error = FX_Get_OS2_Table( pface->face, &pOS2 );
      if ( error || !pOS2 )
#else
      pOS2 = (TT_OS2 *) FT_Get_Sfnt_Table( pface->face, ft_sfnt_os2 );
      if ( !pOS2 )
#endif
      {
         COPY( "[FdQueryFaces]  - error getting OS/2 table: " ); CATI( error ); CAT("\r\n"); WRITE;
         continue;
      }

#if 0
      error = FX_Get_PostScript_Table( pface->face, &ppost );
      if ( error || !ppost )
#else
      ppost = (TT_Postscript *) FT_Get_Sfnt_Table( pface->face, ft_sfnt_post );
      if ( !ppost )
#endif
      {
         // this is not fatal; we only use POST to get the italic angle
         COPY( "[FdQueryFaces]  - error getting POST table: " ); CATI( error ); CAT("\r\n"); WRITE;
      }

      /* get font name and check it's really found */
      name = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
      if (name == NULL)
#if 0
         name = wrongName;
#else
         name = pface->face->family_name;
#endif
      COPY( "[FdQueryFaces]  - font family name is: " ); CAT( name ); CAT("\r\n"); WRITE;

      name_len = strlen( name );
      strncpy(ifi.szFamilyname, name, FACESIZE);
      ifi.szFamilyname[FACESIZE-1] = '\0';
      fsTrunc = (name_len > FACESIZE) ? IFIMETRICS_FAMTRUNC : 0;

      ifi.fsSelection = 0;
      ifi.szFacename[0] = '\0';

      if (( fGlobalOptions & FL_OPT_STYLE_FIX ) &&
          (( style = LookupName(pface->face, TT_NAME_ID_FONT_SUBFAMILY)) != NULL ))
      {
          LONG style_len = strlen( style );

          /* The following fixup, if enabled, changes the face name to always
           * take the format <familyname> <style> if the style is one of "Bold",
           * "Italic", "Bold Italic", "Medium", or "Medium Italic"; or to just
           * <familyname> if the style is "Regular", "Reg" or "Roman".  (It is
           * left unchanged if none of these patterns apply.)
           *
           * This is a workaround for OpenOffice, which (for whatever reason)
           * cannot print properly when the font styles in use don't follow
           * rigid naming conventions.
           */

          if (( !strcmp( style, "Bold Italic") &&
                !strstr( ifi.szFamilyname, " Bold") &&
                !strstr( ifi.szFamilyname, " Italic")) ||
              ( !strcmp( style, "Bold Oblique") &&
                !strstr( ifi.szFamilyname, " Bold") &&
                !strstr( ifi.szFamilyname, " Oblique")) ||
              ( !strcmp( style, "Bold") &&
                !strstr( ifi.szFamilyname, " Bold")) ||
              ( !strcmp( style, "Italic") &&
                !strstr( ifi.szFamilyname, " Italic")) ||
              ( !strcmp( style, "Oblique") &&
                !strstr( ifi.szFamilyname, " Oblique")))
          {
              strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
              strncat( ifi.szFacename, " ", FACESIZE );
              strncat( ifi.szFacename, style, FACESIZE );
              fsTrunc |= ((name_len + style_len + 1) > FACESIZE ) ?
                         IFIMETRICS_FACETRUNC : 0;
          }
          /* For a style name of "Medium" we check the weight class to find
           * out what the best equivalent is: regular if < 500, bold if > 600,
           * or just keep it as "Medium" otherwise (but apply the family name
           * fixup regardless).
           */
          else if ( !strcmp( style, "Medium") &&
                    !strstr( ifi.szFamilyname, " Medium"))
          {
              if ( pOS2->usWeightClass > 600 ) {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Bold", FACESIZE );
                  fsTrunc |= ((name_len + 5) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
              else if ( pOS2->usWeightClass < 500 ) {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  fsTrunc |= (name_len > FACESIZE ) ? IFIMETRICS_FACETRUNC : 0;
              }
              else {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Medium", FACESIZE );
                  fsTrunc |= ((name_len + 7) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
          }
          else if ( !strcmp( style, "Medium Italic") &&
                    !strstr( ifi.szFamilyname, " Medium Italic"))
          {
              if ( pOS2->usWeightClass > 600 ) {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Bold Italic", FACESIZE );
                  fsTrunc |= ((name_len + 12) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
              else if ( pOS2->usWeightClass < 500 ) {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Italic", FACESIZE );
                  fsTrunc |= ((name_len + 7) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
              else {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Medium Italic", FACESIZE );
                  fsTrunc |= ((name_len + 14) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
          }
          else if ( !strcmp( style, "Medium Oblique") &&
                    !strstr( ifi.szFamilyname, " Medium Oblique"))
          {
              if ( pOS2->usWeightClass > 600 ) {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Bold Oblique", FACESIZE );
                  fsTrunc |= ((name_len + 13) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
              else if ( pOS2->usWeightClass < 500 ) {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Oblique", FACESIZE );
                  fsTrunc |= ((name_len + 8) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
              else {
                  strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
                  strncat( ifi.szFacename, " Medium Oblique", FACESIZE );
                  fsTrunc |= ((name_len + 15) > FACESIZE ) ?
                             IFIMETRICS_FACETRUNC : 0;
              }
          }
          else if ( !strcmp( style, "Regular") ||
                    !strcmp( style, "Normal") ||
                    !strcmp( style, "Roman") ||
                    !strcmp( style, "Reg"))
          {
              strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
              fsTrunc |= (name_len > FACESIZE ) ? IFIMETRICS_FACETRUNC : 0;
          }
/*
          else if ( fAlwaysFix )
          {
              strncpy( ifi.szFacename, ifi.szFamilyname, FACESIZE );
              strncat( ifi.szFacename, " ", FACESIZE );
              strncat( ifi.szFacename, style, FACESIZE );
              fsTrunc |= ((name_len + style_len + 1) > FACESIZE ) ?
                         IFIMETRICS_FACETRUNC : 0;
          }
*/
      }

      // Get the full face name if we didn't set one up above
      if ( ifi.szFacename[0] == '\0') {
          name = LookupName(pface->face, TT_NAME_ID_FULL_NAME);
          if (name == NULL) {
              strncpy(ifi.szFacename, pface->face->family_name, FACESIZE );
              strncat(ifi.szFacename, " ", FACESIZE );
              strncat(ifi.szFacename, pface->face->style_name, FACESIZE );
              if (( strlen(pface->face->family_name) + 1 +
                    strlen(pface->face->style_name)) > FACESIZE )
                 fsTrunc |= IFIMETRICS_FACETRUNC;
          }
          else {
              strncpy(ifi.szFacename, name, FACESIZE );
              fsTrunc |= (strlen(name) > FACESIZE) ? IFIMETRICS_FACETRUNC : 0;
          }
      }
      ifi.szFacename[FACESIZE - 1] = '\0';
      if (ifi.szFacename[strlen(ifi.szFacename)-1] == ' ')
         ifi.szFacename[strlen(ifi.szFacename)-1] = '\0';
      COPY( "[FdQueryFaces]  - font face name is: " ); CAT( ifi.szFacename ); CAT("\r\n"); WRITE;

      /* If a Unicode cmap exists in font and EITHER                          */
      /*   the FL_OPT_FORCE_UNICODE flag is defined                           */
      /* OR                                                                   */
      /*   the font contains more than AT_LEAST_DBCS_GLYPH glyphs             */
      /* then do not translate between UGL and Unicode, but just use straight */
      /* Unicode.  But first check if it's a genuine DBCS font and handle it  */
      /* properly.                                                            */
      if (( pface->charMode == TRANSLATE_UGL ) &&
          (( file->flags & FL_OPT_FORCE_UNICODE ) ||
           ( pface->face->num_glyphs > AT_LEAST_DBCS_GLYPH ))
         )
      {
         LONG  specEnc = PSEID_UNICODE;
         BOOL  UDCflag = FALSE;   /* !!!!TODO: UDC support */

         /* If a Unicode font is being used as the DBCS association font
          * (PM_SystemFonts->PM_AssociateFont in OS2.INI), using UNICODE will
          * break this functionality, so we need to force use of an appropriate
          * Asian glyphlist instead.
          *
          * We also need to force an Asian glyphlist in cases where the font
          * file is aliased to a CMB passthrough file (which contains external
          * CJK bitmaps for the glyphs), as this also appears broken in at
          * least some cases where the glyphlist is UNICODE.
          */
         if ( fCMB || !mystricmp(szAssocFont, ifi.szFacename)) {
             LONG defEnc;
             /* Use the system country settings as a fallback; hopefully,
              * interfaceSEId() will find a more intelligent match.
              */
             switch (iLangId) {
                default:
                case TT_MS_LANGID_JAPANESE_JAPAN:
                   defEnc = PSEID_SHIFTJIS;
                   break;
                case TT_MS_LANGID_CHINESE_PRC:
                case TT_MS_LANGID_CHINESE_SINGAPORE:
                   defEnc = PSEID_PRC;
                   break;
                case TT_MS_LANGID_CHINESE_TAIWAN:
                case TT_MS_LANGID_CHINESE_HONG_KONG:
                   defEnc = PSEID_BIG5;
                   break;
                case TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA:
                   defEnc = PSEID_WANSUNG;
                   break;
                case TT_MS_LANGID_KOREAN_JOHAB_KOREA:
                   defEnc = PSEID_JOHAB;
                   break;
            }
            specEnc = interfaceSEId(pOS2, UDCflag, defEnc);
         }
         else if ( file->flags & FL_OPT_FORCE_UNICODE )
            specEnc = PSEID_UNICODE;
         else if (( iLangId == TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA ) &&
                  ( FT_Get_Char_Index( pface->face, 0xAC00 ) != 0 ))
            specEnc = PSEID_WANSUNG;
         else if (( iLangId == TT_MS_LANGID_JAPANESE_JAPAN ) &&
                  ( FT_Get_Char_Index( pface->face, 0x3042 ) != 0 ) &&
                  ( FT_Get_Char_Index( pface->face, 0x65E5 ) != 0 ))
            specEnc = PSEID_SHIFTJIS;
         else if (( iLangId == TT_MS_LANGID_CHINESE_TAIWAN ) &&
                  ( FT_Get_Char_Index( pface->face, 0x3105 ) != 0 ))
            specEnc = PSEID_BIG5;
         else if (( iLangId == TT_MS_LANGID_CHINESE_PRC ) &&
                  ( FT_Get_Char_Index( pface->face, 0x3105 ) != 0 ))
            specEnc = PSEID_PRC;
         else
            specEnc = interfaceSEId(pOS2, UDCflag, PSEID_UNICODE);

         COPY("[FdQueryFaces]  - specEnc = "); CATI( specEnc ); CAT("\r\n"); WRITE;
         switch (specEnc) {
            case PSEID_SHIFTJIS:
               pface->charMode = TRANSLATE_UNI_SJIS;
               break;

            case PSEID_BIG5:
               pface->charMode = TRANSLATE_UNI_BIG5;
               break;

            case PSEID_WANSUNG:
               pface->charMode = TRANSLATE_UNI_WANSUNG;
               break;

            case PSEID_PRC:
               pface->charMode = TRANSLATE_UNI_GB2312;
               break;

            default:  /* do use straight Unicode */
               pface->charMode = TRANSLATE_UNICODE; /* straight Unicode */
         }
      }

      switch( pface->charMode )
      {
         case TRANSLATE_SYMBOL :
            /* symbol encoding    */
            strcpy(ifi.szGlyphlistName, "SYMBOL");
            break;

         case TRANSLATE_BIG5 :
         case TRANSLATE_UNI_BIG5 :
            /* Big5 encoding      */
            strcpy(ifi.szGlyphlistName, "PMCHT");
            break;

         case TRANSLATE_SJIS :
         case TRANSLATE_UNI_SJIS :
            /* ShiftJIS encoding  */
            strcpy(ifi.szGlyphlistName, "PMJPN");
            break;

         case TRANSLATE_WANSUNG :
         case TRANSLATE_UNI_WANSUNG :
            /* Wansung encoding */
            strcpy(ifi.szGlyphlistName, "PMKOR");
            break;

         case TRANSLATE_GB2312 :
         case TRANSLATE_UNI_GB2312 :
            /* GB2312 encoding */
            strcpy( ifi.szGlyphlistName, "PMPRC" );
            break;

         case TRANSLATE_UNICODE :
            /* straight Unicode */
            strcpy( ifi.szGlyphlistName, "UNICODE" );
            break;

         default :
             strcpy(ifi.szGlyphlistName, "PMUGL");
      }

      COPY("[FdQueryFaces]  - szGlyphlistName = "); CAT( ifi.szGlyphlistName );
      CAT( ", charMode = "); CATI( pface->charMode );
      CAT( ", num_Glyphs = "); CATI( pface->face->num_glyphs ); CAT("\r\n");
      WRITE;

      ifi.idRegistry          = 0;
      ifi.lCapEmHeight        = pface->face->units_per_EM;
#if 0
      ifi.lXHeight            = pface->face->bbox->yMax /2;      /* IBM TRUETYPE.DLL does */
#else
      ifi.lXHeight            = ((pOS2->version > 1) && pOS2->sxHeight) ?
                                    pOS2->sxHeight :
                                    (pface->face->bbox.yMax /2);
#endif
      ifi.lMaxAscender        = pOS2->usWinAscent;
      ifi.lMaxDescender       = abs( pOS2->usWinDescent );
      ifi.lLowerCaseAscent    = pface->face->ascender;
      ifi.lLowerCaseDescent   = -pface->face->descender;

      ifi.lMaxBaselineExt     = ifi.lMaxAscender + ifi.lMaxDescender;
      ifi.lInternalLeading    = ifi.lMaxBaselineExt - pface->face->units_per_EM;

      /* There's really no well-defined way to determine the external leading.
       * In theory it should probably be the difference between lInternalLeading
       * and pOS2->sTypoLineGap, but the OpenType standard is ambiguous about
       * how sTypoLineGap should be set, and some fonts have it set to weird
       * values.  On the other hand, standard OS/2 practice is apparently just
       * to make lExternalLeading always be 0.  What we have here is an attempt
       * at a compromise: we'll use the difference between lInternalLeading and
       * sTypoLineGap IF AND ONLY IF it is greater than 0 and less than 0.2 em.
       * Otherwise just use 0.
       */
      ifi.lExternalLeading    = pOS2->sTypoLineGap - ifi.lInternalLeading;
      if (( ifi.lExternalLeading < 1 ) ||
          (ifi.lExternalLeading > ( pface->face->units_per_EM / 5 )))
         ifi.lExternalLeading = 0;

      ifi.lAveCharWidth       = (SHORT) pOS2->xAvgCharWidth;
      ifi.lMaxCharInc         = pface->face->max_advance_width;
      ifi.lEmInc              = -1;        /* per IFI spec (value is ignored) */
      if (ppost != NULL)
         ifi.fxCharSlope      = -ppost->italicAngle;
      else                        /* workaround in case of missing POST table */
         ifi.fxCharSlope      = (pOS2->fsSelection & 0x01) ? 10 : 0;
      ifi.fxInlineDir         = 0;
      ifi.fxCharRot           = 0;
      ifi.usWeightClass       = pOS2->usWeightClass;    /* hopefully OK       */
      ifi.usWidthClass        = pOS2->usWidthClass;
      ifi.lEmSquareSizeX      = pface->face->units_per_EM;
      ifi.lEmSquareSizeY      = pface->face->units_per_EM;
      ifi.giFirstChar         = 0;            /* following values should work */
      ifi.giLastChar          = MAX_GLYPH;
      ifi.giDefaultChar       = 0;
      ifi.giBreakChar         = 32;
      ifi.usNominalPointSize  = 120;   /*    these are simply constants       */
      ifi.usMinimumPointSize  = 10;
      ifi.usMaximumPointSize  = 10000;   /* limit to 1000 pt (like ATM fonts) */
      ifi.fsType              = (pOS2->fsType) ? IFIMETRICS_LICENSED : 0;
      ifi.fsDefn              = IFIMETRICS_OUTLINE;  /* always with OpenType  */
      ifi.fsCapabilities      = 0;            /* per IFI spec (but see below) */
      ifi.lSubscriptXSize     = pOS2->ySubscriptXSize;
      ifi.lSubscriptYSize     = pOS2->ySubscriptYSize;
      ifi.lSubscriptXOffset   = pOS2->ySubscriptXOffset;
      ifi.lSubscriptYOffset   = pOS2->ySubscriptYOffset;
      ifi.lSuperscriptXSize   = pOS2->ySuperscriptXSize;
      ifi.lSuperscriptYSize   = pOS2->ySuperscriptYSize;
      ifi.lSuperscriptXOffset = pOS2->ySuperscriptXOffset;
      ifi.lSuperscriptYOffset = pOS2->ySuperscriptYOffset;
      ifi.lUnderscoreSize     = pface->face->underline_thickness;
      if (ifi.lUnderscoreSize >= 100)
         ifi.lUnderscoreSize = 50;  /* little fix for Arial & others */
      ifi.lUnderscorePosition = -pface->face->underline_position;
      if (ifi.lUnderscorePosition == 97)
         ifi.lUnderscorePosition = 150;  /* little fix for Gulim.ttc */
      ifi.lStrikeoutSize      = pOS2->yStrikeoutSize;
      ifi.lStrikeoutPosition  = pOS2->yStrikeoutPosition;

      /* flag the font name(s) as truncated if they were > 31 characters */
      ifi.fsType |= fsTrunc;

      /* A sneaky little trick gleaned from some ATM driver source in the DDK:
       * IFIMETRICS can store charset flags in fsCapabilities; if set, GPI/GRE
       * will move them into the fsDefn and fsSelection fields of FONTMETRICS.
       */
      if ( pOS2->version >= 1 ) {
         range1 = pOS2->ulCodePageRange1;
         range2 = pOS2->ulCodePageRange2;
         // ulUnicodeRange* is only valid if font has a Unicode cmap, so check
         fUni = ((pface->charMode == TRANSLATE_UGL) ||
                 (pface->charMode == TRANSLATE_UNICODE) ||
                 (pface->charMode >= TRANSLATE_UNI_BIG5)) ? TRUE : FALSE;

         /* For each charset, we check the codepage flags first, as these have
          * a more-or-less direct correspondance to the OS/2 charset flags.  We
          * fall back to checking the closest-corresponding Unicode range flag
          * only for certain charsets, and only if the codepage flag isn't set.
          */

         if ((range1 & OS2_CP1_ANSI_OEM_LATIN1) || (range2 & OS2_CP2_IBM_LATIN1_850))
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_LATIN1;
         else if ( fUni && pOS2->ulUnicodeRange1 & 0x2 )
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_LATIN1;

         if (range2 & OS2_CP2_IBM_US_437)
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_PC;

         if ((range1 & OS2_CP1_ANSI_OEM_LATIN2) || (range2 & OS2_CP2_IBM_LATIN2_852))
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_LATIN2;
         else if ( fUni && pOS2->ulUnicodeRange1 & 0x4 )
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_LATIN2;

         if ((range1 & OS2_CP1_ANSI_OEM_GREEK) || (range2 & OS2_CP2_IBM_GREEK_869))
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_GREEK;
         else if ( fUni && pOS2->ulUnicodeRange1 & 0x80 )
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_GREEK;

         if ((range1 & OS2_CP1_ANSI_OEM_CYRILLIC) ||
             (range2 & OS2_CP2_IBM_CYRILLIC_855)  || (range2 & OS2_CP2_IBM_RUSSIAN_866))
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_CYRILLIC;
         else if ( fUni && pOS2->ulUnicodeRange1 & 0x200 )
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_CYRILLIC;

         if ((range1 & OS2_CP1_ANSI_OEM_HEBREW) || (range2 & OS2_CP2_IBM_HEBREW_862))
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_HEBREW;
         else if ( fUni && pOS2->ulUnicodeRange1 & 0x800 )
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_HEBREW;

         if ((range1 & OS2_CP1_ANSI_OEM_ARABIC) || (range2 & OS2_CP2_IBM_ARABIC_864))
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_ARABIC;

         if (range1 & OS2_CP1_ANSI_OEM_THAI_TIS)
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_THAI;

         if (range1 & OS2_CP1_ANSI_OEM_JAPANESE_JIS)
            ifi.fsCapabilities |= (IFIMETRICS_CHARSET_KANA | IFIMETRICS_DBCS_JAPAN);
         else if ( fUni && pOS2->ulUnicodeRange2 & 0x30 )
            ifi.fsCapabilities |= IFIMETRICS_CHARSET_KANA;

         if (range1 & OS2_CP1_ANSI_OEM_CHINESE_TRADITIONAL)
            ifi.fsCapabilities |= IFIMETRICS_DBCS_TAIWAN;

         if (range1 & OS2_CP1_ANSI_OEM_CHINESE_SIMPLIFIED)
            ifi.fsCapabilities |= IFIMETRICS_DBCS_CHINA;

         if ((range1 & OS2_CP1_ANSI_OEM_KOREAN_WANSUNG) ||
             (range1 & OS2_CP1_ANSI_OEM_KOREAN_JOHAB))
            ifi.fsCapabilities |= IFIMETRICS_DBCS_KOREA;
         else if ( fUni && pOS2->ulUnicodeRange2 & 0x100000 )
            ifi.fsCapabilities |= IFIMETRICS_DBCS_KOREA;
      }

      ifi.ulMetricsLength = ifimet_len;
      memcpy( &(ifi.panose), &(pOS2->panose), 10 );

#ifdef KERN_TABLE_SUPPORT
      /* report support for kerning if font supports it */
      if (pface->directory && pface->directory->nPairs != 0) {
         ifi.cKerningPairs = pface->directory->nPairs;
         ifi.fsType |= IFIMETRICS_KERNING;
      }
      else
#endif
         ifi.cKerningPairs = 0;

      /* Note that the following field seems to be the only reliable method of */
      /* recognizing a TT/OT font from an app!  Not that it should be done.    */
      ifi.ulFontClass        = 0x10D; /* just like TRUETYPE.DLL */

      /* the following adjustment are needed because the OT spec defines */
      /* usWeightClass and fsType differently                            */
      if (ifi.usWeightClass >= 100)
         ifi.usWeightClass /= 100;
      if (ifi.usWeightClass == 4)
         ifi.usWeightClass = 5;    /* does this help? */
      if (pOS2->panose[3] == 9) {
         ifi.fsType |= IFIMETRICS_FIXED;
         pface->flags |= FC_FLAG_FIXED_WIDTH; /* we'll need this later */
      }

      switch ( pface->charMode ) {      /* adjustments for var. encodings */
         case TRANSLATE_UNICODE:
            ifi.giLastChar = pOS2->usLastCharIndex;
            ifi.fsType |= IFIMETRICS_UNICODE;
            if (( file->flags & FL_OPT_FORCE_UNI_MBCS ) ||
                ( file->flags & FL_FLAG_DBCS_FILE )
               )
                ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

         case TRANSLATE_SYMBOL:
            ifi.giLastChar = 255;
            break;

         case TRANSLATE_BIG5:
         case TRANSLATE_UNI_BIG5:
            ifi.giLastChar = 383;
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

         case TRANSLATE_SJIS:
         case TRANSLATE_UNI_SJIS:
            ifi.giLastChar = 890;
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

         case TRANSLATE_WANSUNG:
         case TRANSLATE_UNI_WANSUNG:
            ifi.giLastChar = 949;
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;

         case TRANSLATE_GB2312:
         case TRANSLATE_UNI_GB2312:
            ifi.fsType |= IFIMETRICS_MBCS | IFIMETRICS_DBCS;
            break;
      }

      /* adjust fsSelection (TT defines this differently) */
      /* Note: Interestingly, the IBM font driver seems to use the values
       *  defined in TT spec, at least for italic. Strange. Better leave it.
       *  (The OS/2 toolkit headers seem to bear this out.)
       */
      if (pOS2->fsSelection & 0x01) {
          ifi.fsSelection |= TTFMETRICS_ITALIC;
      }
      if (pOS2->fsSelection & 0x02) {
          ifi.fsSelection |= IFIMETRICS_UNDERSCORE;
      }
      if (pOS2->fsSelection & 0x04) {
          ifi.fsSelection |= IFIMETRICS_OVERSTRUCK;
      }

      /* ifimetrics.h doesn't define a flag for bold, but the OS/2 toolkit
       * headers and the TT spec both specify 0x20 for this purpose.
       */
      if (pOS2->fsSelection & 0x20) {
          ifi.fsSelection |= TTFMETRICS_BOLD;
      }

      if (( ifi.fsSelection & TTFMETRICS_BOLD ) && ( ifi.usWeightClass < 6 ))
         ifi.usWeightClass = 7;

      index = faceIndex;
      /* copy the right amount of data to output buffer,         */
      /* also handle the 'fake' vertically rendered DBCS fonts   */

      if ( file->flags & FL_FLAG_DBCS_FILE )
      {
         if ( file->flags & FL_OPT_FAKE_BOLD )
            index *= 3;
         else
            index *= 2;
      }
      else if (( fGlobalOptions & FL_OPT_ALIAS_TMS_RMN ) && ( file->flags & FL_FLAG_FACENAME_ALIAS ))
        index *= 2;

      if ((index >= cStart) && (index < (cStart + cFontCount))) {
         memcpy( (((PBYTE)pifiMetrics) + ifiCount), &ifi,
                 ifimet_len > cMetricLen ? cMetricLen : ifimet_len );
         ifiCount += cMetricLen;
      }
      index++;

      if( file->flags & FL_FLAG_DBCS_FILE )
      {
          if ((index >= cStart) &&
              (index < (cStart + cFontCount)))
          {
             pifi2 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
             memcpy( pifi2, &ifi,
                     ifimet_len > cMetricLen ? cMetricLen : ifimet_len );
             strncpy(pifi2->szFamilyname + 1, ifi.szFamilyname, FACESIZE);
             pifi2->szFamilyname[ FACESIZE - 1 ] = '\0';
             pifi2->szFamilyname[0] = '@';
             strncpy(pifi2->szFacename + 1, ifi.szFacename, FACESIZE);
             pifi2->szFacename[ FACESIZE - 1 ] = '\0';
             pifi2->szFacename[0] = '@';
             ifiCount += cMetricLen;
          }
          index++;

        if ( file->flags & FL_OPT_FAKE_BOLD )
        {
          // handle the 'fake' bold faced DBCS fonts
          if ((index >= cStart) &&
              (index < (cStart + cFontCount))) {

             pifi3 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
             memcpy(pifi3, &ifi,
                    ifimet_len > cMetricLen ? cMetricLen : ifimet_len);
             strcpy(pifi3->szFamilyname, ifi.szFamilyname);
             strcpy(pifi3->szFacename, ifi.szFacename);
             strncat(pifi3->szFacename, " Bold", FACESIZE - strlen( pifi3->szFacename ) - 1);
             pifi3->usWeightClass = 7;
#if 1
             if( ifi.fsType & IFIMETRICS_FIXED )
                pifi3->lAveCharWidth += getFakeBoldMetrics( 0, pifi3->lMaxBaselineExt, NULL );
#endif
             ifiCount += cMetricLen;
          }
        }
      }
      else if (( fGlobalOptions & FL_OPT_ALIAS_TMS_RMN ) && ( file->flags & FL_FLAG_FACENAME_ALIAS ))
      {
         if ((index >= cStart) &&
             (index < (cStart + cFontCount))) {
            pifi2 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
            memcpy(pifi2, &ifi,
                   ifimet_len > cMetricLen ? cMetricLen : ifimet_len);
            strcpy(pifi2->szFamilyname, "Roman");
            switch (strlen(ifi.szFacename)) {  // This looks weird but... works
                case 15: // Times New Roman
                    strcpy(pifi2->szFacename, "Tms Rmn");
                    break;
                case 20: // Times New Roman Bold
                    strcpy(pifi2->szFacename, "Tms Rmn Bold");
                    break;
                case 22: // Times New Roman Italic
                    strcpy(pifi2->szFacename, "Tms Rmn Italic");
                    break;
                case 27: // Times New Roman Bold Italic
                    strcpy(pifi2->szFacename, "Tms Rmn Bold Italic");
                    break;
            }
            ifiCount += cMetricLen;
         }
      }

      COPY( "[FdQueryFaces]  - metrics populated successfully.\r\n"); WRITE;
#if 0
      FREE( pOS2 );
      if (ppost != NULL)
         FREE( ppost );
#endif
      FX_Flush_Stream(pface->face);
   }

Exit:
   return cFontCount;

Fail:
   FX_Flush_Stream(pface->face);
   return -1;
}


/****************************************************************************/
/*                                                                          */
/* FdOpenFontContext :                                                      */
/*                                                                          */
/*  open new font context                                                   */
/*                                                                          */
HFC _System FdOpenFontContext( HFF    hff,
                               ULONG  ulFont)
{
          int          i = 0;
//   static FT_Size      instance;
   static PFontFile    file;
          ULONG        faceIndex;

   COPY("[FdOpenFontContext] hff = "); CATI((int) hff);
   CAT(",  ulFont = "); CATI((int) ulFont); CAT("\r\n");
   WRITE;

   file = getFontFile(hff);
   if (!file)
      ERRRET((HFC)-1) /* error, invalid font handle */

   /* ?Don't need to activate the stream since it isn't used here? */
   //FX_Activate_Stream(file->faces[faceIndex].face);

   /* calculate real face index in font file */
   if( file->flags & FL_FLAG_DBCS_FILE )
   {
        if( file->flags & FL_OPT_FAKE_BOLD )
            faceIndex = ulFont / 3;
        else
            faceIndex = ulFont / 2;
   }
   else
        faceIndex = ulFont;

   if (( fGlobalOptions & FL_OPT_ALIAS_TMS_RMN ) && ( file->flags & FL_FLAG_FACENAME_ALIAS ))
      /* This font isn't real! */
      faceIndex = ulFont / 2;

   if (faceIndex >= file->numFaces)
      ERRRET((HFC)-1)

   /* OK, create new instance with defaults (the resolution, point size
    * etc. will be set in SetFontContext).
    * NOT NECESSARY? FT_Face now includes a default size object.
       error = FT_New_Size( file->faces[faceIndex].face, &instance);
       if (error)
         ERET1( Fail );
    */

   /* find first unused index */
   i = 0;
   while ((contexts[i].hfc != 0) && (i < MAX_CONTEXTS))
      i++;

   if (i == MAX_CONTEXTS)
     ERET1( Fail );  /* no free slot in table */

   contexts[i].hfc          = (HFC)(i + 0x100); /* initialize table entries */
//   contexts[i].instance     = instance;
   contexts[i].transformed  = FALSE;            /* no scaling/rotation assumed */
   contexts[i].file         = file;
   contexts[i].faceIndex    = faceIndex;

   if( file->flags & FL_OPT_FAKE_BOLD )
   {
      /* for DBCS fonts/collections, odd indices are vertical versions*/
      if ((file->flags & FL_FLAG_DBCS_FILE) && ((ulFont % 3 ) == 1))
         contexts[i].vertical  = TRUE;
      else
         contexts[i].vertical  = FALSE;

      /* for DBCS fonts/collections, deal with fake bold versions */
      if ( (file->flags & FL_FLAG_DBCS_FILE) && ((ulFont % 3) == 2))
         contexts[i].fakebold  = TRUE;
      else
         contexts[i].fakebold  = FALSE;
   }
   else
   {
      /* for DBCS fonts/collections, odd indices are vertical versions*/
      if ((file->flags & FL_FLAG_DBCS_FILE) && (ulFont & 1))
         contexts[i].vertical  = TRUE;
      else
         contexts[i].vertical  = FALSE;
   }

   file->flags |= FL_FLAG_CONTEXT_OPEN;         /* flag as in-use */

   COPY("-> hfc "); CATI((int) contexts[i].hfc); CAT("\r\n"); WRITE;

   /* Do a flush in case getFontFile had to call WakeFontFile... */
   // FX_Flush_Stream(file->faces[faceIndex].face);
   return contexts[i].hfc; /* everything OK */

Fail:
   FX_Flush_Stream(file->faces[faceIndex].face);
   return (HFC)-1;
}


/****************************************************************************/
/*                                                                          */
/* FdSetFontContext :                                                       */
/*                                                                          */
/*  set font context parameters                                             */
/*                                                                          */
LONG _System FdSetFontContext( HFC           hfc,
                               PCONTEXTINFO  pci )
{
   LONG       ptsize, temp, emsize;
   FT_Error   error;
   PFontSize  size;

   COPY("FdSetFontContext: hfc = ");         CATI((int) hfc);
   CAT(", sizlPPM.cx = ");                   CATI((int) pci->sizlPPM.cx);
   CAT(", sizlPPM.cy = ");                   CATI((int) pci->sizlPPM.cy);
   CAT("\r\n                pfxSpot.x = ");  CATI((int) pci->pfxSpot.x);
   CAT(", pfxSpot.y = ");                    CATI((int) pci->pfxSpot.y);
   CAT("\r\n                eM11 = ");       CATI((int) pci->matXform.eM11);
   CAT(", eM12 = ");                         CATI((int) pci->matXform.eM12);
   CAT(", eM21 = ");                         CATI((int) pci->matXform.eM21);
   CAT(", eM22 = ");                         CATI((int) pci->matXform.eM22);
   CAT("\r\n");
   WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   emsize = size->file->faces[size->faceIndex].face->units_per_EM;

   /* Look at matrix and see if a transform is asked for */
   /* Actually when rotating by 90 degrees hinting could be used */

   size->transformed =
     ( pci->matXform.eM11 != pci->matXform.eM22       ||
      (pci->matXform.eM12 | pci->matXform.eM21) != 0  ||
       pci->matXform.eM11 <= 0 );

   if ( size->transformed )
   {
      /* check for simple stretch in one direction */
      if ((pci->matXform.eM11 > 0 && pci->matXform.eM22 > 0) &&
          (pci->matXform.eM12 | pci->matXform.eM21) == 0) {

         LONG    ptsizex, ptsizey;

         size->transformed = FALSE; /* will be handled like nontransformed font */
         size->scaleFactor = pci->matXform.eM11 + pci->matXform.eM21; /* TODO: correct! */

         ptsizex = (emsize * pci->matXform.eM11) >> 10;
         ptsizey = (emsize * pci->matXform.eM22) >> 10;

         error = FT_Set_Char_Size(size->file->faces[size->faceIndex].face,
                                  ptsizex, ptsizey, INSTANCE_DPI, INSTANCE_DPI);
         if (error)
            ERRRET(-1)  /* engine problem */

         return 0;
      }

      /* note that eM21 and eM12 are swapped; I have no idea why, but */
      /* it seems to be correct */
      size->matrix.xx = pci->matXform.eM11 * 64;
      size->matrix.xy = pci->matXform.eM21 * 64;
      size->matrix.yx = pci->matXform.eM12 * 64;
      size->matrix.yy = pci->matXform.eM22 * 64;

      /* set pointsize to Em size; this effectively disables scaling */
      /* but enables use of hinting */
      error = FT_Set_Char_Size(size->file->faces[size->faceIndex].face,
                               emsize, emsize, INSTANCE_DPI, INSTANCE_DPI);
      if (error)
         ERRRET(-1)  /* engine problem */

      return 0;
   }

   /* calculate & set  point size  */
   ptsize = (emsize * (pci->matXform.eM11 + pci->matXform.eM21)) >> 10;

   if (ptsize <= 0)                /* must not allow zero point size ! */
      ptsize = 1;                  /* !!!  should be handled better     */

   /* save scaling factor for later use (in 16.16 format) */
   size->scaleFactor = pci->matXform.eM11 + pci->matXform.eM21;

   error = FT_Set_Char_Size(size->file->faces[size->faceIndex].face,
                            ptsize, ptsize, INSTANCE_DPI, INSTANCE_DPI);
   if (error)
      ERRRET(-1)  /* engine problem */

   /* This doesn't seem to be necessary here... is it? */
   // FX_Flush_Stream(size->file->faces[faceIndex].face);

   return 0;      /* pretend everything is OK */

}


/****************************************************************************/
/*                                                                          */
/* FdCloseFontContext :                                                     */
/*                                                                          */
/*  destroy a font context                                                  */
/*                                                                          */
LONG _System FdCloseFontContext( HFC hfc)
{
   PFontSize  size;

   COPY("[FdCloseFontContext] hfc = "); CATI((int)hfc); CAT("\r\n"); WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   /* mark table entry as free */
   size->hfc = 0;

   /* !!!!! set flag in TFontFile structure */
   size->file->flags &= ~FL_FLAG_CONTEXT_OPEN;   /* reset the in-use flag */

/*** Deprecated: FT_Face now manages the size object automatically
   if (size->file->flags & FL_FLAG_LIVE_FACE) {
      COPY("Closing instance: "); CATI((int)(size->instance.z)); CAT("\r\n"); WRITE;
      error = TT_Done_Instance(size->instance);
      if (error)
         ERRRET(-1)  // engine error
   }
*/
   COPY("[FdCloseFontContext] Done\r\n"); WRITE;
   return 0; /* success */
}


/****************************************************************************/
/*                                                                          */
/* FdQueryFaceAttr                                                          */
/*                                                                          */
/*  Return various info about font face                                     */
/*                                                                          */
LONG _System FdQueryFaceAttr( HFC     hfc,
                              ULONG   iQuery,
                              PBYTE   pBuffer,
                              ULONG   cb,
                              PGLYPH  pagi,
                              GLYPH   giStart )
{
    int           count, i = 0;
    PFontSize     size;
    PFontFace     face;
    FT_Error      error;
    TT_OS2       *pOS2;
    ABC_TRIPLETS *pt;

    COPY("[FdQueryFaceAttr] hfc = "); CATI((int) hfc); CAT("\r\n"); WRITE;

    size = getFontSize(hfc);
    if (!size)
        ERRRET(-1) /* error, invalid context handle */

    face = &(size->file->faces[size->faceIndex]);

    COPY( "[FdQueryFaceAttr] Face = "); CAT( face->face->family_name );
    CAT( " ");                          CAT( face->face->style_name ); CAT("\r\n");
    WRITE;

    if ( FX_Activate_Stream( face->face )) ERRRET(-1);


    /* Kerning pairs table was requested.
     */
   if (iQuery == FD_QUERY_KERNINGPAIRS)
   {
#ifdef KERN_TABLE_SUPPORT
        FX_Kern_0       *pKerning;    /* contents of format 0 kerning subtable */
        ULONG            used = 0;    /* # bytes used in output buffer */
        FD_KERNINGPAIRS *kpair;
        USHORT          *kernIndices, idx;

        count = cb / sizeof( FD_KERNINGPAIRS );

        COPY("[FdQueryFaceAttr] QUERY_KERNINGPAIRS, "); CATI((int) count);
        CAT("\r\n"); WRITE;

        if ( !face->directory || face->directory->nPairs )
            return 0;   /* no kerning info provided */
        pKerning = face->directory;

        kpair = (PVOID)pBuffer;
        if ( !kpair )
            return 0;   /* this should not really be possible */

        /* Create a reverse-lookup table if this face doesn't have one yet */
        if ( face->kernIndices == NULL ) {
            error = ALLOC( face->kernIndices,
                           face->face->num_glyphs * sizeof( USHORT ));
            if ( error )
                ERET1( Fail );

            /* fill all entries with -1s */
            memset( face->kernIndices, 0xFF,
                    face->face->num_glyphs * sizeof( USHORT ));
        }

        kernIndices = face->kernIndices;

        /* Copy all kerning pairs to the buffer, converting the indices to PM */
        while (( i < pKerning->nPairs ) && ( i < count ))
        {
            /* The 'left' and 'right' values in the kerning table are physical
             * indices in the font file.  We have to convert them to glyphlist
             * indices that PM can use, hence the rather awkward logic here.
             */
            idx = pKerning->pairs[i].left;
            if ( kernIndices[idx] == (USHORT)-1 )
                kernIndices[idx] = ReverseTranslate( face, idx );
            kpair->giFirst  = kernIndices[idx];
            idx = pKerning->pairs[i].right;
            if ( kernIndices[idx] == (USHORT)-1 )
                kernIndices[idx] = ReverseTranslate(face, idx);
            kpair->giSecond = kernIndices[idx];
            kpair->eKerningAmount = pKerning->pairs[i].value;
            kpair++;
            i++;
        }

        COPY("Returned kerning pairs: "); CATI(i); CAT("\r\n"); WRITE;
        return i;   /* # items filled */
#else
        return 0;   /* no kerning support */
#endif
   }


    /* ABC widths requested
     */
   if (iQuery == FD_QUERY_ABC_WIDTHS)
   {
      count = cb / sizeof(ABC_TRIPLETS);

      COPY("[FdQueryFaceAttr] QUERY_ABC_WIDTHS, "); CATI((int) count);
      CAT(" items, giStart = ");  CATI((int) giStart);
      if (pBuffer == NULL)
         CAT(" NULL buffer");
      CAT("\r\n"); WRITE;

      pt = (ABC_TRIPLETS*)pBuffer;
      for (i = giStart; i < giStart + count; i++, pt++)
      {
         int            index;
         unsigned short wid;

         COPY("[FdQueryFaceAttr] Entering PM2TT\r\n"); WRITE;
         index = PM2TT( face->face, face->charMode, i );

         /* get advances and bearings */
         error = FT_Load_Glyph( face->face, index, FT_LOAD_NO_SCALE );
         if (error)
           goto Broken_Glyph;

         /* skip complicated calculations for fixed fonts */
         if (face->flags & FC_FLAG_FIXED_WIDTH )
         {
            wid = face->face->glyph->metrics.width;
         }
         else
         {  /* proportional font, it gets trickier */
            if (face->flags & FC_FLAG_DBCS_FACE) {
                /* store glyph widths for DBCS fonts
                    - needed for reasonable performance */
               if (face->widths == NULL) {
                  error = ALLOC(face->widths,
                                face->face->num_glyphs * sizeof (USHORT));
                  if (error)
                     goto Broken_Glyph;   /* this error really shouldn't happen */

                  /* tag all entries as unused */
                  memset(face->widths, 0xFF,
                         face->face->num_glyphs * sizeof (USHORT));
               }
               if (face->widths[index] == 0xFFFF) { /* not yet cached */
                  /* save for later */
                  face->widths[index] = face->face->glyph->metrics.width;
               }
               wid = face->widths[index];
            }
            /* 'small' font, no need to remember widths, OS/2 takes care of it */
            else
            {
               wid = face->face->glyph->metrics.width;
            }
         }

         if (( size->file->flags & FL_OPT_FAKE_BOLD ) && size->fakebold )
         {
            FT_Pos addwid;

            addwid = getFakeBoldMetrics( 0, face->face->glyph->metrics.height, NULL );
            face->face->glyph->metrics.horiAdvance += addwid;
            wid += addwid;
         }

         if (size->vertical && !is_HALFCHAR(i))
         {
            if ((face->face->face_flags & FT_FACE_FLAG_VERTICAL) && 0) /* TODO: enable */
            {
              pt->lA  = face->face->glyph->metrics.vertBearingY;
              pt->ulB = face->face->glyph->metrics.height;
              pt->lC  = face->face->glyph->metrics.vertAdvance - pt->lA - pt->ulB;
            }
            else
            {
              pt->lA  = pt->lC = 0;
              pt->ulB = face->face->max_advance_height;
            }

         }
         else
         {
           pt->lA = face->face->glyph->metrics.horiBearingX;
           pt->ulB = wid;
           pt->lC = face->face->glyph->metrics.horiAdvance - pt->lA - pt->ulB;
         }

         COPY("[FdQueryFaceAttr] ABC info : index = "); CATI( index );
         CAT("\r\n");
         WRITE;

         COPY("[FdQueryFaceAttr]  - adv_width = "); CATI( face->face->glyph->metrics.horiAdvance );
         CAT(" lefts = "); CATI( face->face->glyph->metrics.horiBearingX );
         CAT(" width = "); CATI( wid );
         CAT("\r\n");
         WRITE;

         COPY("[FdQueryFaceAttr]  - A + B + C = "); CATI( pt->lA + pt->ulB + pt->lC );
         CAT("     A = "); CATI( pt->lA );
         CAT("     B = "); CATI( pt->ulB );
         CAT("     C = "); CATI( pt->lC );         CAT("\r\n");
         WRITE;

         continue;

    Broken_Glyph:  /* handle broken glyphs gracefully */
         pt->lA = pt->lC = 0;

         if (size->vertical && !is_HALFCHAR(i))
            pt->ulB = face->face->max_advance_height;
         else
            pt->ulB = face->face->max_advance_width;

      }
   }

   FX_Flush_Stream(face->face);
   return count; /* number of entries filled in */

   if (pagi);   // keep the compiler happy

Fail:
   FX_Flush_Stream(face->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* FdQueryCharAttr :                                                        */
/*                                                                          */
/*  Return glyph attributes, basically glyph's bit-map or outline           */
/*  some variables are declared static to conserve stack space.             */
/*                                                                          */
LONG _System FdQueryCharAttr( HFC             hfc,
                              PCHARATTR       pCharAttr,
                              PBITMAPMETRICS  pbmm )
{
// static     FT_Bitmap bitmap;   // converted glyph bitmap
   static     FT_Bitmap fbbmm;    // fake bold bitmap, if applicable
   static     FT_Glyph  glyph;    // format-agnostic handle to glyph
   static     FT_BBox   bbox;     // glyph bounding box

   PFontSize  size;
   PFontFace  face;
   LONG       mapsize;
   int        gindex, width, rows, cols, i, j;
   ULONG      cb;
   ULONG      load_format;
   FT_Pos     ExtentX, ExtentY, addedwidth;
   FT_Error   error;

   #ifdef  DEBUG
   int        row, col;
   char *     pBitmap;
   #endif

   COPY( "[FdQueryCharAttr] hfc = " ); CATI( hfc );
   CAT( "; cb = ");                    CATI( pCharAttr->cb );
   CAT( "; type = ");                  CATI( pCharAttr->iQuery );
   CAT( "; index = ");                 CATI( pCharAttr->gi );
   CAT("\r\n");                        WRITE;

   size = getFontSize(hfc);
   if (!size) {
      COPY( "[FdQueryCharAttr] Invalid font context handle.\r\n" );
      ERRRET(-1) /* error, invalid context handle */
   }

   face = &(size->file->faces[size->faceIndex]);
   COPY( "[FdQueryCharAttr] Face = "); CAT( face->face->family_name );
   CAT( " ");                          CAT( face->face->style_name ); CAT("\r\n");
   WRITE;

   if ( FX_Activate_Stream( face->face ))
      ERRRET(-1);

   COPY("[FdQueryCharAttr] Entering PM2TT\r\n"); WRITE;
   gindex = PM2TT( face->face, face->charMode, pCharAttr->gi);

   if ((pCharAttr->iQuery & FD_QUERY_OUTLINE) ||
       (size->vertical && !is_HALFCHAR(pCharAttr->gi)) || size->transformed)
      load_format = FT_LOAD_NO_BITMAP;
   else
      load_format = FT_LOAD_DEFAULT;

   error = FT_Load_Glyph( face->face, gindex, load_format );
   if (error)
   {
       /* try to recover quietly */
       error = FT_Load_Glyph( face->face, 0, load_format );
       if (error) {
            COPY("[FdQueryCharAttr] FT_Load_Glyph failed!!  Error code is "); CATI(error);
            CAT("\n"); WRITE;
            ERET1( Fail );
       }
   }

   if (face->face->glyph->format == FT_GLYPH_FORMAT_BITMAP)
   {
      COPY("[FdQueryCharAttr] Native glyph bitmap was loaded.\r\n"); WRITE;
      FT_Get_Glyph( face->face->glyph, &glyph );
      FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_GRIDFIT, &bbox );
      FT_Done_Glyph( glyph );
   }
   else
   {

      /* --- Vertical fonts handling----------------------------------- */
      if (size->vertical && !is_HALFCHAR(pCharAttr->gi)) {
         FT_Matrix  vertMatrix;

         /* rotate outline 90 degrees counterclockwise */
         vertMatrix.xx =  0x00000;
         vertMatrix.xy = -0x10000;
         vertMatrix.yx =  0x10000;
         vertMatrix.yy =  0x00000;
         FT_Outline_Transform(&(face->face->glyph->outline), &vertMatrix);

         if (size->transformed) {
            /* move outline to the right to adjust for rotation */
            FT_Outline_Translate(&(face->face->glyph->outline), face->face->size->metrics.ascender, 0);
            /* move outline down a bit */
            FT_Outline_Translate(&(face->face->glyph->outline), 0, -(face->face->size->metrics.descender));
         }
         else {
            /* move outline to the right to adjust for rotation */
            FT_Outline_Translate(&(face->face->glyph->outline),
                                 (face->face->size->metrics.ascender * size->scaleFactor) >> 10, 0);
            /* move outline down a bit */
            FT_Outline_Translate(&(face->face->glyph->outline), 0,
                                 -((face->face->size->metrics.descender * size->scaleFactor) >> 10));
         }
      }

      /* --- Other transformations ------------------------------------ */
      if (size->transformed) {
         COPY("[FdQueryCharAttr] Applying transformation matrix: [ ");
         CATI( size->matrix.xx ); CAT(" ");
         CATI( size->matrix.xy ); CAT(" ");
         CATI( size->matrix.yx ); CAT(" ");
         CATI( size->matrix.yy ); CAT("]\r\n "); WRITE;

         FT_Outline_Transform( &(face->face->glyph->outline), &size->matrix );
      }

      /* --- Outline processing --------------------------------------- */
      if ( pCharAttr->iQuery & FD_QUERY_OUTLINE )
      {
         FX_Flush_Stream(face->face);
         if (pCharAttr->cbLen == 0)      /* send required outline size in bytes */
            return GetOutlineLen( &(face->face->glyph->outline) );
         return GetOutline( &(face->face->glyph->outline), pCharAttr->pBuffer );
      }

      // TODO better error checking here
#if 0
      FT_Get_Glyph( face->face->glyph, &glyph );
      FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_GRIDFIT, &bbox );
      /* the following seems to be necessary for rotated glyphs */
      if (size->transformed) {
         bbox.xMax = bbox.xMin = 0;
         for (i = 0; i < face->face->glyph->outline.n_points; i++) {
            if (bbox.xMin  > face->face->glyph->outline.points[i].x)
               bbox.xMin = face->face->glyph->outline.points[i].x;
            if (bbox.xMax  < face->face->glyph->outline.points[i].x)
               bbox.xMax = face->face->glyph->outline.points[i].x;
         }
      }
      FT_Done_Glyph( glyph );
#else
      FT_Outline_Get_BBox( &(face->face->glyph->outline), &bbox );
      // grid-fit the bbox
      bbox.xMin = PIXGRID_ROUND( bbox.xMin );
      bbox.yMin = PIXGRID_ROUND( bbox.yMin );
      bbox.xMax = PIXGRID_ROUND( bbox.xMax );
      bbox.yMax = PIXGRID_ROUND( bbox.yMax );
#endif

   }

   /* --- Bitmap (real or rasterized) processing --------------------- */

   /* --- first the bitmap metrics --- */

   COPY("[FdQueryCharAttr]   bbox.xMin = "); CATI(bbox.xMin);
   CAT( "  bbox.xMax = ");                   CATI(bbox.xMax); CAT("\r\n");
   CAT( "[FdQueryCharAttr]   bbox.yMin = "); CATI(bbox.yMin);
   CAT( "  bbox.yMax = ");                   CATI(bbox.yMax); CAT( "\r\n");
   WRITE;

   if (pCharAttr->iQuery & FD_QUERY_BITMAPMETRICS)
   {
      /* Calculate the glyph metrics from the bounding box.  Note that these
       * values are in 26.6 format or 1/64th pixels. We will convert them to
       * pixels a bit later.
       */
      ExtentX = bbox.xMax - bbox.xMin;
      ExtentY = bbox.yMax - bbox.yMin;
      /* Origin coordinates must be converted to FIXED 16:16 format, which
       * means multiplying 26.6 values by 2^10.
       */
      //pbmm->pfxOrigin.x   = face->face->glyph->metrics.horiBearingX * size->scaleFactor;
      //pbmm->pfxOrigin.y   = bbox.yMax << 10;
      pbmm->pfxOrigin.x   = bbox.xMin << 10;
      pbmm->pfxOrigin.y   = bbox.yMax << 10;

      if ( size->file->flags & FL_OPT_FAKE_BOLD ) {
         addedwidth = (size->fakebold) ? getFakeBoldMetrics(ExtentX, ExtentY, &fbbmm) : 0;
         pbmm->sizlExtent.cx = (ExtentX + addedwidth) >> 6;
      }
      else
        pbmm->sizlExtent.cx = ExtentX >> 6;

      pbmm->sizlExtent.cy = ExtentY >> 6;
      pbmm->cyAscent      = 0;

      COPY("[FdQueryCharAttr]   pbmm->sizlExtent.cy = "); CATI(pbmm->sizlExtent.cy);
      CAT( "  pbmm->sizlExtent.cx = ");                   CATI(pbmm->sizlExtent.cx); CAT("\r\n");
      CAT( "[FdQueryCharAttr]   pbmm->pfxOrigin.x = ");   CATI(pbmm->pfxOrigin.x);
      CAT( "  pbmm->pfxOrigin.y = ");                     CATI(pbmm->pfxOrigin.y);   CAT( "\r\n");
      WRITE;

      if (!(pCharAttr->iQuery & FD_QUERY_CHARIMAGE)) {
         FX_Flush_Stream(face->face);
         return sizeof(*pbmm);
      }
   }

   /* --- actual bitmap processing here --- */
   if (pCharAttr->iQuery & FD_QUERY_CHARIMAGE)
   {
      /* create a rasterized bitmap if we don't have a real one */
      if ( face->face->glyph->format != FT_GLYPH_FORMAT_BITMAP )
      {
         error = FT_Render_Glyph( face->face->glyph, FT_RENDER_MODE_MONO );
         if (error)
            ERET1(Fail); /* engine error */
         COPY( "[FdQueryCharAttr] Glyph bitmap generated successfully.\r\n" ); WRITE;
      }

      width = face->face->glyph->bitmap.width;
      rows  = face->face->glyph->bitmap.rows;
      // width rounded up to nearest multiple of 4 bytes
      cols  = ((width + 31) / 8) & -4;

      mapsize = (( size->file->flags & FL_OPT_FAKE_BOLD ) && size->fakebold ) ?
                    fbbmm.rows * abs(fbbmm.pitch)  :
                    rows * cols;

      COPY ( "[FdQueryCharAttr] pCharAttr->cbLen = " ); CATI( pCharAttr->cbLen ); CAT( "\n" ); WRITE;

      if (pCharAttr->cbLen == 0) {
         FX_Flush_Stream(face->face);
         return mapsize;
      }

      /* --- Return the bitmap data in the buffer provided --- */

      if (mapsize > pCharAttr->cbLen)
         ERET1(Fail);     /* otherwise we might overwrite something */

      /* clean provided buffer (unfortunately necessary) */
      memset(pCharAttr->pBuffer, 0, pCharAttr->cbLen);

      if ( ! CopyBitmap( face->face->glyph->bitmap.buffer, pCharAttr->pBuffer,
                         rows, face->face->glyph->bitmap.pitch, cols ))
         ERET1(Fail);

      COPY("[FdQueryCharAttr]  bitmap.rows  = " ); CATI( face->face->glyph->bitmap.rows );
      CAT( "  bitmap.pitch = " );                  CATI( face->face->glyph->bitmap.pitch );
      CAT( " -> ");                                CATI( cols );         CAT( "\r\n" );
      CAT( "[FdQueryCharAttr]  bitmap.width = " ); CATI( face->face->glyph->bitmap.width );
      CAT( "  bitmap size  = " );                  CATI( mapsize );      CAT( "\r\n" );
      WRITE;

      if (( size->file->flags & FL_OPT_FAKE_BOLD ) && size->fakebold ) {
         error = buildFakeBoldBitmap( pCharAttr->pBuffer, pCharAttr->cbLen,
                                      width, addedwidth >> 6, rows, cols );
         if (error)
            ERET1(Fail);

         rows = fbbmm.rows;
         cols = abs(fbbmm.pitch);
         width += addedwidth;
      }

      pbmm->sizlExtent.cx = width;
      pbmm->sizlExtent.cy = rows;
      COPY("[FdQueryCharAttr]   pbmm->sizlExtent.cy = "); CATI(pbmm->sizlExtent.cy);
      CAT( "  pbmm->sizlExtent.cx = ");                   CATI(pbmm->sizlExtent.cx);
      CAT("\r\n");      WRITE;

#ifdef  DEBUG
      /* print character image */
      pBitmap = (char*)pCharAttr->pBuffer;
      for (row=0; row < rows; row++) {
         for (col=0; col < cols; col++, pBitmap++) {
            COPY( " " );
            for (i=7; i>=0; i--) {
               CAT( ((*pBitmap >> i) & 1) ? "#" : "." );
            }
            WRITE;
         }
         COPY( "\n" ); WRITE;
      }
#endif  /* DEBUG */

      FX_Flush_Stream(face->face);
      return mapsize; /* return # of bytes */
   }

Fail:
   FX_Flush_Stream(face->face);
   return -1;
}


/****************************************************************************/
/*                                                                          */
/* FdQueryFullFaces :                                                       */
/*                                                                          */
/*  Query names of all faces in this file.  Input/output behaviour depends  */
/*  on how the function is called.                                          */
/*                                                                          */
/*  When input *cBufLen is 0 and/or pBuffer is NULL, the output is:         */
/*     *pBuffer: unchanged                                                  */
/*     *cBufLen: the required buffer size for all faces                     */
/*     *cFontCount: 0                                                       */
/*     Return value: the number of fonts in the file                        */
/*                                                                          */
/*  Otherwise, the output is:                                               */
/*     *pBuffer: contains the names of *cFontCount fonts (from cStart)      */
/*     *cBufLen: the required buffer size for all faces                     */
/*     *cFontCount: the number of fonts returned                            */
/*     Return value: the number of fonts NOT returned                       */
/*                                                                          */
LONG _System FdQueryFullFaces( HFF     hff,          // font file handle
                               PVOID   pBuffer,      // buffer for output
                               PULONG  cBufLen,      // buffer length
                               PULONG  cFontCount,   // number of fonts
                               ULONG   cStart )      // starting index
{
    static PFontFace    pface;
           PFontFile    file;
           FT_Error     error;
           PBYTE        pBuf;
           PFFDESCS2    pffd;
           LONG         lCount, lTotal, faceIndex,
                        cbFamily, cbFace, lOff,
                        cbEntry, cbTotal  = 0,
                        ifiCount = 0;
           char         *name1, *name2,
                        *pszFaceName;


    COPY( "[FdQueryFullFaces] hff = " ); CATI( hff );
    CAT(  ", cFontCount = " );           CATI( *cFontCount );
    CAT(  ", cStart = " );               CATI( cStart );
    CAT(  ", cBufLen = " );              CATI( *cBufLen );
    CAT( "\r\n");
    WRITE;

    file = getFontFile(hff);
    if ( !file )
        ERRRET(-1)          // error, invalid handle

    COPY( "[FdQueryFullFaces] Filename: " ); CAT( file->filename ); CAT("\r\n"); WRITE;

    if ( cStart > ( file->numFaces - 1 ))
        return 0;

    pBuf = pBuffer;
    pffd = (PFFDESCS2) pBuf;
    lCount = 0;         // # of font names being returned
    lTotal = 0;         // total # of font names in the file
    for (faceIndex = 0; faceIndex < file->numFaces; faceIndex++)
    {
        cbEntry = 0;    // size of the current buffer item

        // get pointer to this face's data
        pface = &(file->faces[faceIndex]);

        // make sure the file is open
        error = FX_Activate_Stream( pface->face );
        if ( error ) continue;

        // get font name and check it's really found
        // 1. family name
        name1 = LookupName( pface->face, TT_NAME_ID_FONT_FAMILY );
        if (name1 == NULL)
            name1 = pface->face->family_name;
        cbFamily = strlen( name1 ) + 1;
        lOff = ( cbFamily + 3 ) & ( -4 );     // round up to nearest 4 bytes

        // 2. face name
        name2 = LookupName( pface->face, TT_NAME_ID_FULL_NAME );
        if ( name2 == NULL )
            cbFace = strlen( name1 ) + strlen( pface->face->style_name ) + 2;
        else
            cbFace = strlen( name2 ) + 1;

        cbEntry = sizeof( FFDESCS2 ) + lOff + cbFace;
        cbEntry = ( cbEntry + 3 ) & ( -4 );   // round up to nearest 4 bytes

        COPY( "[FdQueryFullFaces]  -face # = " );   CATI( lTotal );
        CAT( " , Item # = " );                      CATI( lCount );
        CAT( " , Item size = " );                   CATI( cbEntry );
        CAT( " , Total size = " );                  CATI( cbTotal );
        CAT( "\r\n");
        WRITE;

        if ( pffd && ( *cBufLen > 0 ) && ( lTotal >= cStart ) && ( lCount < *cFontCount )) {
            if (( cbTotal + cbEntry ) > *cBufLen )
                goto Fail;
            pffd->cbLength = cbEntry;
            pszFaceName = pffd->abFamilyName + lOff;
            pffd->cbFacenameOffset = (ULONG) pszFaceName - (ULONG) pffd;
            strcpy( pffd->abFamilyName, name1 );
            pffd->abFamilyName[ cbFamily-1 ] = '\0';
            if ( name2 == NULL )
                sprintf( pszFaceName, "%s %s", name1, pface->face->style_name );
            else
                strcpy( pszFaceName, name2 );

            COPY("[FdQueryFullFaces]  - family name = ");  CAT( pffd->abFamilyName ); WRITE;
            COPY(" , full name = ");   CAT( pszFaceName ); CAT("\r\n");               WRITE;

            pBuf += (ULONG) cbEntry;
            pffd = (PFFDESCS2) pBuf;
            lCount++;
        }
        cbTotal += cbEntry;
        lTotal++;

        if( file->flags & FL_FLAG_DBCS_FILE )
        {
            // DBCS fonts will have an extra '@' name...
            cbFamily++;
            lOff = ( cbFamily + 3 ) & ( -4 );     // round up to nearest 4 bytes
            name2 = pszFaceName;
            cbFace  = strlen( name2 ) + 2;
            cbEntry = sizeof( FFDESCS2 ) + lOff + cbFace;
            cbEntry = ( cbEntry + 3 ) & ( -4 );   // round up to nearest 4 bytes

            COPY( " QueryFullFaces: hff = " ); CATI( hff );
            CAT( " , DBCS, Face # = " );       CATI( lTotal );
            CAT( " , Item # = " );             CATI( lCount );
            CAT( " , Item size = " );          CATI( cbEntry );
            CAT( " , Total size = " );         CATI( cbTotal );
            CAT( "\r\n");
            WRITE;

            if ( pffd && ( *cBufLen > 0 ) && ( lTotal >= cStart ) && ( lCount < *cFontCount )) {
                if (( cbTotal + cbEntry ) > *cBufLen )
                    goto Fail;
                pffd->cbLength = cbEntry;
                pszFaceName = pffd->abFamilyName + lOff;
                pffd->cbFacenameOffset = (ULONG) pszFaceName - (ULONG) pffd;
                pffd->abFamilyName[0] = '@';
                strncat( pffd->abFamilyName, name1, lOff-1 );
                pffd->abFamilyName[ cbFamily-1 ] = '\0';
                *pszFaceName = '@';
                strncat( pszFaceName, name2, cbFace-1 );

                COPY( "QueryFullFaces: hff = " );  CATI( hff );
                CAT( " , FamilyName = " );         CAT( pffd->abFamilyName );
                CAT( " ("); CATI( cbFamily ); CAT("->"); CATI( lOff ); CAT( ") ");
                CAT( " , FullName = " );           CAT( pszFaceName );
                CAT( " , FacenameOffset = " );     CATI( pffd->cbFacenameOffset );
                CAT( "\r\n");
                WRITE;

                pBuf += (ULONG) cbEntry;
                pffd = (PFFDESCS2) pBuf;
                lCount++;
            }
            cbTotal += cbEntry;
            lTotal++;

            // ...and possibly an extra 'fake bold' name as well
            if ( file->flags & FL_OPT_FAKE_BOLD )
            {
                cbFamily--;                   // remove what we added for '@' above
                lOff = ( cbFamily + 3 ) & ( -4 );     // round up to nearest 4 bytes
                cbFace   = strlen( name2 ) + 6;
                cbEntry = sizeof( FFDESCS2 ) + lOff + cbFace;
                cbEntry = ( cbEntry + 3 ) & ( -4 );   // round up to nearest 4 bytes

                if ( pffd && ( *cBufLen > 0 ) && ( lTotal >= cStart ) && ( lCount < *cFontCount )) {
                    if (( cbTotal + cbEntry ) > *cBufLen )
                        goto Fail;
                    pffd->cbLength = cbEntry;
                    pszFaceName = pffd->abFamilyName + lOff;
                    pffd->cbFacenameOffset = (ULONG) pszFaceName - (ULONG) pffd;
                    strcpy( pffd->abFamilyName, name1 );
                    pffd->abFamilyName[ cbFamily-1 ] = '\0';
                    strcpy( pszFaceName, name2 );
                    strcat( pszFaceName, " Bold");
                    pBuf += (ULONG) cbEntry;
                    pffd = (PFFDESCS2) pBuf;
                    lCount++;
                }
                cbTotal += cbEntry;
                lTotal++;
            }
        }

        else if (( fGlobalOptions & FL_OPT_ALIAS_TMS_RMN ))
        {
            // If enabled, Times New Roman has an extra aliased name for each face
            pface = &(file->faces[0]);
            name1 = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
            if ( name1 && !strcmp(name1, "Times New Roman"))
            {
                lOff = 8;             // "Roman" (word-aligned)
                name2 = LookupName(pface->face, TT_NAME_ID_FULL_NAME);
                name2 += 15;              // we only want the style portion
                cbFace = strlen( name2 ) + 8;
                cbEntry = sizeof( FFDESCS2 ) + lOff + cbFace;
                cbEntry = ( cbEntry + 3 ) & ( -4 );

                if ( pffd && ( *cBufLen > 0 ) && ( lTotal >= cStart ) && ( lCount < *cFontCount )) {
                    if (( cbTotal + cbEntry ) > *cBufLen )
                        goto Fail;
                    pffd->cbLength = cbEntry;
                    pszFaceName = pffd->abFamilyName + lOff;
                    pffd->cbFacenameOffset = (ULONG) pszFaceName - (ULONG) pffd;
                    strcpy( pffd->abFamilyName, "Roman");
                    pffd->abFamilyName[ 5 ] = '\0';
                    strcpy( pszFaceName, "Tms Rmn");
                    strcat( pszFaceName, name2 );
                    pBuf += (ULONG) cbEntry;
                    pffd = (PFFDESCS2) pBuf;
                    lCount++;
                }
                cbTotal += cbEntry;
                lTotal++;
            }
        }
    }
    *cBufLen = cbTotal;

    *cFontCount = lCount;
    FX_Flush_Stream(pface->face);
    return ( lTotal - lCount );

Fail:
    *cFontCount = lCount;
    FX_Flush_Stream(pface->face);
    return GPI_ALTERROR;
}

/*---------------------------------------------------------------------------*/
/* end of exported functions                                                 */
/*---------------------------------------------------------------------------*/


/****************************************************************************/
/* LangInit determines language used at DLL startup, non-zero return value  */
/* means error.                                                             */
/* This code is crucial, because it determines behaviour of the font driver */
/* with regard to language encodings it will use.                           */
static  ULONG LangInit(void)
{
   COUNTRYCODE cc = {0, 0};
   COUNTRYINFO ci;
   ULONG       cilen;

   isGBK = FALSE;

   GetUdcInfo();           /* get User Defined Character info */

   /* get country info; ci.country then contains country code */
   if (DosQueryCtryInfo(sizeof(ci), &cc, &ci, &cilen))
      return -1;
   /* get DBCS lead byte values for later use */
   DosQueryDBCSEnv(sizeof(DBCSLead), &cc, DBCSLead);

   uLastGlyph = 383;
   switch (ci.country) {
      case 81:            /* Japan */
         iLangId = TT_MS_LANGID_JAPANESE_JAPAN;
         ScriptTag = *(ULONG *) "kana";
         LangSysTag = *(ULONG *) "JAN ";
         pGlyphlistName = "PMJPN";
         uLastGlyph = 890;
         break;

      case 88:            /* Taiwan */
         iLangId = TT_MS_LANGID_CHINESE_TAIWAN;
         ScriptTag = *(ULONG *) "kana";
         LangSysTag = *(ULONG *) "CHT ";
         pGlyphlistName = "PMCHT";
         break;

      case 86:            /* People's Republic of China */
         if (ci.codepage == 1386 || ulCp[0] == 1386 || ulCp[1] == 1386) {
            isGBK = TRUE;
         }               /* endif */
         iLangId = TT_MS_LANGID_CHINESE_PRC;
         ScriptTag = *(ULONG *) "kana";
         LangSysTag = *(ULONG *) "CHS ";
         pGlyphlistName = "PMPRC";
         break;

      case 82:            /* Korea */
         iLangId = TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA;
         ScriptTag = *(ULONG *) "hang";
         LangSysTag = *(ULONG *) "KOR ";
         pGlyphlistName = "PMKOR";
         uLastGlyph = 949;
         break;

      default:            /* none of the above countries */
         ScriptTag = *(ULONG *) "";
         LangSysTag = *(ULONG *) "";
         break;
   }                      /* endswitch */

   return 0;
}

/****************************************************************************/
/*                                                                          */
/* FirstInit :                                                              */
/*                                                                          */
/*  Called when font driver is loaded for the first time. Performs the      */
/* necessary one-time initialization.                                       */
/*                                                                          */
ULONG  FirstInit(void)
{
   LONG   lReqCount;
   ULONG  ulCurMaxFH;
   FT_Error error;

   #ifdef DEBUG
      ULONG Action;
   #endif /* DEBUG */
   #ifdef DEBUG
      DosOpen("\\FTIFI.LOG", &LogHandle, &Action, 0, FILE_NORMAL,
              OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
              OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_WRITE_THROUGH |
              OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY,
              NULL);
      COPY("[FirstInit] FreeType/2 loaded.\r\n");  WRITE;
   #endif /* DEBUG */

   /* increase # of file handles by five to be on the safe side */
   lReqCount = 5;
   DosSetRelMaxFH(&lReqCount, &ulCurMaxFH);

   /* turn on the FT engine */
   error = FT_Init_FreeType(&library);
   if (error)
      return 0;     /* exit immediately */
   COPY("[FirstInit] FreeType engine initialized.\r\n");  WRITE;

   if (LangInit())      /* initialize NLS */
      return 0;         /* exit on error  */
   COPY("[FirstInit] NLS initialized.\r\n");  WRITE;

   OptionsInit();  /* initialize max_open_files */
   COPY("[FirstInit] Open faces limit set to "); CATI(max_open_files); CAT("\r\n");
   WRITE;

   COPY("[FirstInit] Initialization successful.\r\n");  WRITE;

   return 1;
}

/****************************************************************************/
/*                                                                          */
/* FinalTerm :                                                              */
/*                                                                          */
/*  Called when font driver is unloaded for the last time time. Performs    */
/* final clean-up, shuts down engine etc.                                   */
ULONG  FinalTerm(void)
{
   PListElement  cur;
   PListElement  tmp;

   /* throw away elements from 'free elements' list */
   cur = free_elements;
   while (cur != NULL) {
      tmp = cur;
      cur = cur->next;
      FREE(tmp);
   }

   /* clean up the combined-fonts alias list if necessary */
   if ( pszCmbAliases ) FREE( pszCmbAliases );

   /* turn off engine */
   FT_Done_FreeType(library);

   #ifdef DEBUG
      COPY("FreeType/2 terminated.\r\n");  WRITE;
      DosClose(LogHandle);
   #endif

   return 1;
}

/****************************************************************************/
/*                                                                          */
/* _DLL_InitTerm :                                                          */
/*                                                                          */
/*  This is the DLL Initialization/termination function. It initializes     */
/*  the FreeType engine and some internal structures at startup. It cleans  */
/*  up the UCONV cache at process termination.                              */
/*                                                                          */
ULONG _System _DLL_InitTerm( ULONG hModule, ULONG ulFlag )
{
    switch (ulFlag) {
        case 0:             /* initializing */
           if (++ulProcessCount == 1)
              return FirstInit();  /* loaded for the first time */
           else
              return 1;

        case 1:  {          /* terminating */
           int  i;

           CleanUCONVCache();     /* clean UCONV cache */

           if(--ulProcessCount == 0)
              return FinalTerm();
           else
              return 1;
        }
    }

    if ( hModule );      // keep the compiler happy
    return 0;
}


/****************************************************************************/
/*                                                                          */
/* interfaceSEId (Interface-specific Encoding Id) determines what encoding  */
/* the font driver should use if a font includes a Unicode encoding.        */
/* Note: the 'encoding' parameter is the default to fall back to (usually   */
/* Unicode) if nothing better can be found.                                 */
/*                                                                          */
LONG    interfaceSEId(TT_OS2 *pOS2, BOOL UDCflag, LONG encoding)
{
    ULONG   range1 = 0;
    ULONG   bits = 0,
            mask;

    if (pOS2->version >= 1) {
       /*
        * OS/2 table version 1 and later contains codepage *
        * bitfield to support multiple codepages.
        */
       range1 = pOS2->ulCodePageRange1;

       if (range1 & OS2_CP1_ANSI_OEM_JAPANESE_JIS)
          bits++;
       if (range1 & OS2_CP1_ANSI_OEM_CHINESE_SIMPLIFIED)
          bits++;
       if (range1 & OS2_CP1_ANSI_OEM_CHINESE_TRADITIONAL)
          bits++;
       if (range1 & OS2_CP1_ANSI_OEM_KOREAN_WANSUNG)
          bits++;
       if (range1 & OS2_CP1_ANSI_OEM_KOREAN_JOHAB)
          bits++;

       if (bits == 0) {
          /* No codepage-specific encodings found!  In this case we'd better
           * fall back to PSEID_UNICODE, regardless of the specified default.
           */
          encoding = PSEID_UNICODE;

       /* Note: if font supports more than one of the following codepages
        * (bits > 1 ), encoding will be left at the specified default.
        */
       } else if (bits == 1) {
          switch (range1) {
             case OS2_CP1_ANSI_OEM_JAPANESE_JIS:
                encoding = PSEID_SHIFTJIS;
                break;
             case OS2_CP1_ANSI_OEM_CHINESE_SIMPLIFIED:
                encoding = PSEID_PRC;
                break;
             case OS2_CP1_ANSI_OEM_CHINESE_TRADITIONAL:
                encoding = PSEID_BIG5;
                break;
             case OS2_CP1_ANSI_OEM_KOREAN_WANSUNG:
                encoding = PSEID_WANSUNG;
                break;
             case OS2_CP1_ANSI_OEM_KOREAN_JOHAB:
                encoding = PSEID_JOHAB;
                break;
             default:
                break;
          }                   /* endswitch */
       }                      /* endif */
    } else {
       /*
        * The codepage range bitfield is not available.
        * Codepage must be assumed from the COUNTRY setting.
        * This means the user is on his own.
        */
       switch (iLangId) {
          case TT_MS_LANGID_JAPANESE_JAPAN:
             encoding = PSEID_SHIFTJIS;
             break;
          case TT_MS_LANGID_CHINESE_PRC:
          case TT_MS_LANGID_CHINESE_SINGAPORE:
             encoding = PSEID_PRC;
             break;
          case TT_MS_LANGID_CHINESE_TAIWAN:
          case TT_MS_LANGID_CHINESE_HONG_KONG:
             encoding = PSEID_BIG5;
             break;
          case TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA:
             encoding = PSEID_WANSUNG;
             break;
          case TT_MS_LANGID_KOREAN_JOHAB_KOREA:
             encoding = PSEID_JOHAB;
             break;
       }
    }

    return encoding;
    if ( UDCflag );     // keep compiler happy
}


/****************************************************************************/
/*                                                                          */
/* ConvertNameFromUnicode                                                   */
/*                                                                          */
/*   Converts a UCS-2 encoded SFNT name into the appropriate codepage.      */
/*                                                                          */

BOOL ConvertNameFromUnicode(USHORT langid, PSZ string, ULONG string_len,
                            PSZ buffer, ULONG buffer_len)
{
   static UniChar *cpNameWansung = L"IBM-949@endian=big:system";
   static UniChar *cpNameBig5    = L"IBM-950@endian=big:system";
   static UniChar *cpNameSJIS    = L"IBM-943@endian=big:system";
   static UniChar *cpNameGB2312  = L"IBM-1381@endian=big:system";
   static UniChar *cpNameSystem  = L"@endian=big:system";

   UconvObject uconvObject = NULL;
   UniChar    *cpName = NULL;
   ULONG       uconvType;
   ULONG       rc;

   switch( langid )
   {
      case TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA :
         cpName    = cpNameWansung;
         uconvType = UCONV_TYPE_WANSUNG;
         break;

      case TT_MS_LANGID_JAPANESE_JAPAN :
         cpName    = cpNameSJIS;
         uconvType = UCONV_TYPE_SJIS;
         break;

      case TT_MS_LANGID_CHINESE_TAIWAN :
      case TT_MS_LANGID_CHINESE_HONG_KONG :
         cpName    = cpNameBig5;
         uconvType = UCONV_TYPE_BIG5;
         break;

      case TT_MS_LANGID_CHINESE_PRC :
         cpName    = cpNameGB2312;
         uconvType = UCONV_TYPE_GB2312;
         break;

      case TT_MS_LANGID_ENGLISH_UNITED_STATES :
      default :
         cpName    = cpNameSystem;
         uconvType = UCONV_TYPE_SYSTEM;
         break;
   }

   if ( getUconvObject( cpName, &uconvObject, uconvType ) == 0 )
   {
#if 0
      rc = UniStrFromUcs( uconvObject, buffer, (UniChar *)string, buffer_len );
#else
      size_t input_len = string_len;
      size_t output_len = buffer_len - 1;
      size_t subs = 0;
      memset( buffer, 0, buffer_len );
      rc = UniUconvFromUcs( uconvObject,
                            (UniChar **)&string, &input_len,
                            (void **) &buffer, &output_len, &subs );
#endif
      if ( rc == ULS_SUCCESS )
         return TRUE;
   }

   return FALSE;
}


/****************************************************************************/
/*                                                                          */
/* LookupName :                                                             */
/*                                                                          */
/*   Look for a TrueType name by index, prefer current language             */
/*                                                                          */

/* Hopefully enough to handle "long" fontnames... not really ideal but it's
 * faster than alloc-ing everything.  This allows us to both return the
 * actual fontname in QueryFullNames, as well as know when the name has been
 * truncated in IFIMETRICS/FONTMETRICS.  The caller will truncate this name
 * to FACESIZE where needed.
 */
#define LONGFACESIZE 128

static  char*  LookupName(FT_Face face,  int index )
{
   static char name_buffer[ LONGFACESIZE + 2 ];
   FT_SfntName name;

   int i, j, n;

   char   *string;
   USHORT string_len;

   int    found;
   int    best;


   n = FT_Get_Sfnt_Name_Count( face );
   if ( n < 0 )
      return NULL;

   found = -1;
   best  = -1;

   /* Pass 1: Search for a Unicode-encoded name (Microsoft platform).
    * If a CJK language name that matches the system codepage is found, select
    * it.  If not, but US-English is found, select that.  Otherwise, fall back
    * to the first encoding found.
    */
   for ( i = 0; found == -1 && i < n; i++ )
   {
      FT_Get_Sfnt_Name( face, i, &name );

      if ( name.name_id == index )
      {
         /* Try to find an appropriate language */
         if ( name.platform_id == TT_PLATFORM_MICROSOFT && name.encoding_id == TT_MS_ID_UNICODE_CS )
         {
            if (best == -1 || name.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES)
               best = i;

            switch( name.language_id )
            {
               case TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA :
                  if( ulCp[ 1 ] == 949 )
                     found = i;
                   break;

               case TT_MS_LANGID_JAPANESE_JAPAN :
                  if( ulCp[ 1 ] == 932 || ulCp[ 1 ] == 942 || ulCp[ 1 ] == 943 )
                     found = i;
                  break;

               case TT_MS_LANGID_CHINESE_TAIWAN :
               case TT_MS_LANGID_CHINESE_HONG_KONG :
                  if( ulCp [ 1 ] == 950 )
                     found = i;
                  break;

               case TT_MS_LANGID_CHINESE_PRC :
                  if( ulCp[ 1 ] == 1381 || ulCp[ 1 ] == 1386 )
                     found = i;
                  break;
            }
         }
      }
   }

   if ( found == -1 )
      found = best;

   /* Now convert the Unicode name to the appropriate codepage: for CJK
    * languages, we use the predetermined DBCS codepage; for all other
    * languages, use the active process codepage.
    */
   if ( found != -1 )
   {
      FT_Get_Sfnt_Name( face, found, &name );
#if 0
      COPY("[LookupName]  - using name index "); CATI( found );
      CAT(" = platform/encoding: ");             CATI( name.platform_id );
      CAT("/");                                  CATI( name.encoding_id );
      CAT("; language ID: ");                    CATI( name.language_id );
      CAT("\r\n");                               WRITE;
#endif
      if ( ConvertNameFromUnicode( name.language_id, name.string,
                                   (name.string_len / 2),
                                   name_buffer, sizeof( name_buffer )))
         return name_buffer;
   }

   found = -1;
   best  = -1;

   COPY("[LookupName]  - Unicode name not found, trying native encodings.\r\n"); WRITE;

   /* Pass 2: Unicode strings not available!!! Try to find NLS strings.
    * Search for a name under other encodings (Microsoft platform).  If a DBCS
    * encoding that matches the system codepage is found, OR a symbol encoding
    * is found, select it and return a copy of the string (verbatim).
    */
   for ( i = 0; found == -1 && i < n; i++ )
   {
      FT_Get_Sfnt_Name( face, i, &name );

      if ( name.name_id == index )
      {
         /* Try to find an appropriate encoding */
         if ( name.platform_id == TT_PLATFORM_MICROSOFT )
         {
            if (best == -1)
               best = i;

            switch (name.encoding_id)
            {
               case TT_MS_ID_WANSUNG :
                  if( ulCp[ 1 ] == 949 )
                     found = i;
                  break;

               case TT_MS_ID_SJIS :
                  if( ulCp[ 1 ] == 932 || ulCp[ 1 ] == 942 || ulCp[ 1 ] == 943 )
                     found = i;
                  break;

               case TT_MS_ID_BIG_5 :
                  if( ulCp[ 1 ] == 950 )
                     found = i;
                  break;

               case TT_MS_ID_GB2312 :
                  if( ulCp[ 1 ] == 1381 || ulCp[ 1 ] == 1386 )
                     found = i;
                  break;

               case TT_MS_ID_SYMBOL_CS :
                  found = i;
                  break;
            }
         }
      }
   }

   if ( found == -1 )
      found = best;

   if ( found != -1 )
   {

      FT_Get_Sfnt_Name( face, found, &name );
      COPY("[LookupName]  - using name index "); CATI( found );
      CAT(" (encoding "); CATI( name.encoding_id ); CAT(")\r\n");
      WRITE;

      // We found a suitable encoding; now copy the name
#if 1
      /* Quick check to make sure it's not actually a mis-identified Unicode
       * string (workaround for RF Gothic etc.)
       */
      if ( !CheckDBCSEnc( name.string, name.string_len ))
      {
         if ( ConvertNameFromUnicode( name.language_id,
                                      name.string, name.string_len,
                                      name_buffer, sizeof( name_buffer )))
            return name_buffer;
      }
#endif

      for ( i=0, j=0; ( i < name.string_len ) && ( j < LONGFACESIZE - 1 ); i++)
         if (name.string[i] != '\0')
            name_buffer[j++] = name.string[i];
      name_buffer[j] = '\0';
      RemoveLastDBCSLead( name_buffer );

      return name_buffer;
   }

   /* Not found */
   return NULL;
}


/****************************************************************************/
/*                                                                          */
/* GetCharMap :                                                             */
/*                                                                          */
/*  A function to find a suitable charmap, searching in the following       */
/*  order of importance :                                                   */
/*                                                                          */
/*   1) Windows Unicode                                                     */
/*   2) Apple Unicode                                                       */
/*   3) ROC (Taiwan)                                                        */
/*   4) ShiftJIS (Japan)                                                    */
/*   5) PRC (PR China)                                                      */
/*   6) Wansung (Korea)                                                     */
/*   7) Johab (Korea)                                                       */
/*   8) Apple Roman                                                         */
/*   9) Windows Symbol - not really supported                               */
/*                                                                          */
/* High word of returned ULONG contains type of encoding; low word contains */
/* one of the values above.                                                 */
/*                                                                          */
static  ULONG GetCharmap( FT_Face face )
{
   FT_CharMap cmap;
   int  i, best, bestVal, val;

   if ( face->num_charmaps < 1 )
      ERRRET(-1)

   bestVal = 16;
   best    = -1;

   for (i = 0; i < face->num_charmaps; i++)
   {
      cmap = face->charmaps[i];

      /* Windows Unicode is the highest encoding, return immediately */
      /* if we find it..                                             */
      if (( cmap->platform_id == TT_PLATFORM_MICROSOFT ) &&
          ( cmap->encoding_id == TT_MS_ID_UNICODE_CS ))
         return i;

      /* otherwise, compare it to the best encoding found */
      val = -1;
      switch ( cmap->platform_id )
      {
         case TT_PLATFORM_MICROSOFT:
            switch ( cmap->encoding_id )
            {
               case TT_MS_ID_BIG_5:
                  val = 3;
                  break;
               case TT_MS_ID_SJIS:
                  val = 4;
                  break;
               case TT_MS_ID_GB2312:
                  val = 5;
                  break;
               case TT_MS_ID_WANSUNG:
                  val = 6;
                  break;
               case TT_MS_ID_JOHAB:
                  val = 7;
                  break;
               case TT_MS_ID_SYMBOL_CS:
                  val = 9;
                  break;
            }
            break;

         case TT_PLATFORM_APPLE_UNICODE:
            val = 2;
            break;

         case TT_PLATFORM_MACINTOSH:
            /* used for symbol fonts */
            if (cmap->encoding_id == TT_MAC_ID_ROMAN)
               val = 8;
            break;
      }

      if (val > 0 && val <= bestVal)
      {
         bestVal = val;
         best    = i;
      }
   }

   if (val < 0) {
      /* we didn't find any suitable encoding !! */
      COPY ("[GetCharmap]  - no supported encoding found!\r\n"); WRITE;
      return 0;
   }

   COPY ("[GetCharmap]  - encoding translation will be used.\r\n"); WRITE;

   switch (bestVal) {
     case 3:        /* Traditional Chinese font */
        best |= ( TRANSLATE_BIG5 << 16 );
        break;
     case 4:        /* Japanese font */
        best |= ( TRANSLATE_SJIS << 16 );
        break;
     case 5:        /* Simplified Chinese font */
        best |= ( TRANSLATE_GB2312  << 16 );
        break;
     case 6:        /* Korean font */
        best |= ( TRANSLATE_WANSUNG  << 16 );
        break;
     case 7:        /* Korean font */
        best |= ( TRANSLATE_JOHAB  << 16 );
        break;
     case 8:        /* for Apple Roman encoding only, this      */
        best |= ( TRANSLATE_SYMBOL << 16 );   /* means no translation should be performed */
        break;
   }
   return best;
}


/****************************************************************************/
/* FREETYPE-TO-GPI OUTLINE TRANSLATION                                      */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* GetOutlineLen :                                                          */
/*                                                                          */
/*   Used to compute the size of an outline once it is converted to         */
/*   OS/2's specific format. The translation is performed by the later      */
/*   function called simply "GetOutline".                                   */
/*                                                                          */
static  int GetOutlineLen(FT_Outline *ol)
{
   int    index;     /* current point's index */
   BOOL   on_curve;  /* current point's state */
   int    i, start = 0;
   int    first, last;
   ULONG  cb = 0;


   /* cubic splines cannot start with a control point */
   if ( POINT_IS_CUBIC( ol->tags[0] ))
      return 0;

   /* loop thru all contours in a glyph */
   for ( i = 0; i < ol->n_contours; i++ ) {

      cb += sizeof(POLYGONHEADER);

      first = start;
      last  = ol->contours[i];

      on_curve = (ol->tags[first] & 1);
      index    = first;

      /* process each contour point individually */
      while ( index < last ) {
         index++;

         if ( on_curve ) {
            /* the previous point was on the curve */
            on_curve = ( ol->tags[index] & 1 );
            if ( on_curve ) {
               /* two successive on points => emit segment */
               cb += sizeof(PRIMLINE);
            }
         }
         else {
            /* the previous point was off the curve */
            on_curve = ( ol->tags[index] & 1 );
            if ( on_curve ) {
               /* reaching an `on' point */
               cb += sizeof(PRIMSPLINE);
            }
            else {
               /* two successive `off' points => create middle point */
               cb += sizeof(PRIMSPLINE);
            }
         }
      }

      /* end of contour, close curve cleanly */
      if ( ol->tags[first] & 1 )
      {
        if ( on_curve )
           cb += sizeof(PRIMLINE);
        else
           cb += sizeof(PRIMSPLINE);
      }
      else
        if (!on_curve)
           cb += sizeof(PRIMSPLINE);

      start = ol->contours[i] + 1;

   }
   return cb; /* return # bytes used */
}

/****************************************************************************/
/*                                                                          */
/*  a few global variables used in the following functions                  */
/*                                                                          */
static ULONG          cb = 0, polycb;
static LONG           lastX, lastY;
static PBYTE          pb;
static POINTFX        Q, R;
static POLYGONHEADER  hdr = {0, FD_POLYGON_TYPE};
static PRIMLINE       line = {FD_PRIM_LINE};
static PRIMSPLINE     spline = {FD_PRIM_SPLINE};

/****************************************************************************/
/*                                                                          */
/* LineFrom :                                                               */
/*                                                                          */
/*   add a line segment to the PM outline that GetOutline is currently      */
/*   building.                                                              */
/*                                                                          */
static  void Line_From(LONG x, LONG y)
{
   line.pte.x = x << 10;
   line.pte.y = y << 10;
   /* store to output buffer */
   memcpy(&(pb[cb]), &line, sizeof(line));
   cb     += sizeof(PRIMLINE);
   polycb += sizeof(PRIMLINE);
}

/****************************************************************************/
/*                                                                          */
/* ConicFrom :                                                              */
/*                                                                          */
/*   Add a conic bezier arc to the PM outline that GetOutline is currently  */
/*   buidling. The second-order Bezier is trivially converted to its        */
/*   equivalent third-order form.                                           */
/*                                                                          */
static  void Conic_From( LONG x0, LONG y0, LONG x2, LONG y2, LONG x1, LONG y1 )
{
   spline.pte[0].x = x0 << 10;
   spline.pte[0].y = y0 << 10;
   /* convert from second-order to cubic Bezier spline */
   Q.x = (x0 + 2 * x1) / 3;
   Q.y = (y0 + 2 * y1) / 3;
   R.x = (x2 + 2 * x1) / 3;
   R.y = (y2 + 2 * y1) / 3;
   spline.pte[1].x = Q.x << 10;
   spline.pte[1].y = Q.y << 10;
   spline.pte[2].x = R.x << 10;
   spline.pte[2].y = R.y << 10;
   /* store to output buffer */
   memcpy(&(pb[cb]), &spline, sizeof(spline));
   cb     += sizeof(PRIMSPLINE);
   polycb += sizeof(PRIMSPLINE);
}

/****************************************************************************/
/*                                                                          */
/* CubicFrom :                                                              */
/*                                                                          */
/*   Add a cubic bezier arc to the PM outline that GetOutline is currently  */
/*   buidling. The Bezier is already in third-order form.                   */
/*                                                                          */
static  void Cubic_From( LONG x0, LONG y0, LONG cx1, LONG cy1, LONG cx2, LONG cy2 )
{
   spline.pte[0].x = x0 << 10;
   spline.pte[0].y = y0 << 10;
   spline.pte[1].x = cx1 << 10;
   spline.pte[1].y = cy1 << 10;
   spline.pte[2].x = cx2 << 10;
   spline.pte[2].y = cy2 << 10;
   /* store to output buffer */
   memcpy(&(pb[cb]), &spline, sizeof(spline));
   cb     += sizeof(PRIMSPLINE);
   polycb += sizeof(PRIMSPLINE);
}

/****************************************************************************/
/*                                                                          */
/* GetOutline :                                                             */
/*                                                                          */
/*   Translate a FreeType glyph outline into PM format. The buffer is       */
/*   expected to be of the size returned by a previous call to the          */
/*   function GetOutlineLen().                                              */
/*                                                                          */
/*   This code is taken largely from the FreeType v1 ttraster.c source,     */
/*   and subsequently modified to emit PM segments and arcs.  We've also    */
/*   had to modify it to deal with both conic (quadratic, i.e. TrueType)    */
/*   curves and cubic (i.e. PostScript/CFF) curves.                         */
/*                                                                          */
static  int GetOutline(FT_Outline *ol, PBYTE pbuf)
{
   LONG   x,  y;   /* current point                */
   LONG   cx, cy;  /* current Bezier control point */
   LONG   mx, my;  /* current middle point         */
   LONG   x_first, y_first;  /* first point's coordinates */
   LONG   x_last,  y_last;   /* last point's coordinates  */

   int    index;     /* current point's index */
   BOOL   on_curve;  /* current point's state */
   BOOL   is_cubic;  /* current point is a cubic control point */
   int    i, start = 0;
   int    first, last;
   ULONG  polystart;

   pb = pbuf;
   cb = 0;

   is_cubic = FALSE;

   /* loop thru all contours in a glyph */
   for ( i = 0; i < ol->n_contours; i++ ) {

      polystart = cb;  /* save this polygon's start offset */
      polycb = sizeof(POLYGONHEADER); /* size of this polygon */
      cb += sizeof(POLYGONHEADER);

      first = start;
      last = ol->contours[i];

      x_first = ol->points[first].x;
      y_first = ol->points[first].y;

      x_last  = ol->points[last].x;
      y_last  = ol->points[last].y;

      lastX = cx = x_first;
      lastY = cy = y_first;

      on_curve = POINT_ON_CURVE(ol->tags[first]);
      index    = first;

      /* check first point to determine origin */
      if ( !on_curve ) {
         if ( POINT_IS_CUBIC(ol->tags[last]))
            return 0;                       /* invalid outline! */

         /* first point is off the curve.  Yes, this happens... */
         if ( POINT_ON_CURVE(ol->tags[last])) {
            lastX = x_last;  /* start at last point if it */
            lastY = y_last;  /* is on the curve           */
         }
         else {
            /* if both first and last points are off the curve, */
            /* start at their middle and record its position    */
            /* for closure                                      */
            lastX = (lastX + x_last)/2;
            lastY = (lastY + y_last)/2;

            x_last = lastX;
            y_last = lastY;
         }
      }

      /* now process each contour point individually */
      while ( index < last ) {
         index++;
         x = ( ol->points[index].x );
         y = ( ol->points[index].y );

         if ( on_curve ) {
            /* the previous point was on the curve */
            on_curve = POINT_ON_CURVE( ol->tags[index] );
            if ( on_curve ) {
               /* two successive on points => emit segment */
               Line_From( lastX, lastY ); /*x, y*/
               lastX = x;
               lastY = y;
            }
            else {
               /* else, keep current control point for next bezier */
               cx = x;
               cy = y;
            }
         }
         else {
            /* the previous point was off the curve */
            on_curve = POINT_ON_CURVE( ol->tags[index] );
            if ( on_curve ) {
               /* reaching an `on' point */

               if ( is_cubic && index )
                  /* the previous point was a cubic c.p. */
                  Cubic_From( lastX, lastY, cx, cy,
                              ol->points[index-1].x, ol->points[index-1].y );
               else
                  Conic_From(lastX, lastY, x, y, cx, cy );

               lastX = x;
               lastY = y;
            }
            else if ( !POINT_IS_CUBIC( ol->tags[index] )) {
               /* two successive conic `off' points => create middle point */
               mx = (cx + x) / 2;
               my = (cy + y)/2;

               Conic_From( lastX, lastY, mx, my, cx, cy );
               lastX = mx;
               lastY = my;

               cx = x;
               cy = y;
            }
         }

         if ( !on_curve )
            is_cubic = POINT_IS_CUBIC( ol->tags[index] );
      }

      /* end of contour, close curve cleanly */
      if ( POINT_ON_CURVE( ol->tags[first] )) {
         if ( on_curve )
            Line_From( lastX, lastY); /* x_first, y_first );*/
         else {
            if ( is_cubic )
               Cubic_From( lastX, lastY, cx, cy,
                           ol->points[index].x, ol->points[index].y );
            else
               Conic_From( lastX, lastY, x_first, y_first, cx, cy );
         }
      }
      else {
         if ( !on_curve ) {
           if ( is_cubic )
              Cubic_From( lastX, lastY, cx, cy,
                          ol->points[index].x, ol->points[index].y );
           else
              Conic_From( lastX, lastY, x_last, y_last, cx, cy );
         }
      }
      start = ol->contours[i] + 1;

      hdr.cb = polycb;
      memcpy(&(pb[polystart]), &hdr, sizeof(hdr));

   }
   return cb; /* return # bytes used */
}

