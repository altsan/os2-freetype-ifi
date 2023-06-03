/*                                                                         */
/*                               FreeType/2                                */
/*                                                                         */
/*             OS/2 Font Driver using the FreeType library                 */
/*                                                                         */
/*       Copyright (C) 2010--2012 Alex Taylor <alex@altsan.org>            */
/*       Copyright (C) 2002--2007 KO Myung-Hun <komh@chollian.net>         */
/*       Copyright (C) 2003--2004 Seo, Hyun-Tae <acrab001@hitel.net>       */
/*       Copyright (C) 1997--2000 Michal Necasek <mike@mendelu.cz>         */
/*       Copyright (C) 1997, 1998 David Turner <dturner@cybercable.fr>     */
/*       Copyright (C) 1997, 1999 International Business Machines          */
/*                                                                         */
/*       Version: 1.3.7                                                    */
/*                                                                         */
/* This source is to be compiled with IBM VisualAge C++ 3.0.               */
/* Other compilers may actually work too but don't forget this is NOT a    */
/* normal DLL but rather a subsystem DLL. That means it shouldn't use      */
/* the usual C library as it has to run without runtime environment.       */
/* VisualAge provides a special subsystem version of the run-time library. */
/* All this is of course very compiler-dependent. See makefiles for        */
/* discussion of switches used.                                            */
/*                                                                         */
/*  Implemantation Notes:                                                  */
/*                                                                         */
/* Note #1: As a consequence of this being a subsystem librarary, I had to */
/*   slightly modify the FreeType source, namely ttmemory.c and ttfile.c.  */
/*   FreeType/2 now allocates several chunks of memory and uses them as a  */
/*   heap. Note that memory allocation should use TTAlloc(), possibly      */
/*   directly SSAllocMem(). malloc() is unusable here and it doesn't work  */
/*   at all (runtime library isn't even initialized). See ttmemory.c for   */
/*   more info.                                                            */
/*    In ttfile.c I had to change all fopen(), fseek()... calls            */
/*   to OS/2 API calls (DosOpen, DosSetFilePtr...) because without proper  */
/*   runtime environment a subsystem DLL cannot use std C library calls.   */
/*                                                                         */
/* Note #2: On exit of each function reading from font file the API        */
/*   TT_Flush_Stream() must be called. This is because file handles opened */
/*   by this DLL actually belong to the calling process. As a consequence  */
/*    a) it's easy to run out of file handles, which results in really     */
/*       very nasty behavior and/or crashes. This could be solved by       */
/*       increased file handles limit, but cannot because                  */
/*    b) it is impossible to close files open by another process and       */
/*       therefore the fonts cannot be properly uninstalled (you can't     */
/*       delete them while the're open by other process)                   */
/*   The only solution I found is very simple - just close the file before */
/*   exiting a DLL function. This ensures files are not left open across   */
/*   processes and other problems.                                         */
/*                                                                         */
/* Note #3: The whole business with linked lists is aimed at lowering      */
/*   memory consumption dramatically. If you install 50 TT fonts, OS/2     */
/*   opens all of them at startup. Even if you never use them, they take   */
/*   up at least over 1M of memory. With certain fonts the consumption can */
/*   easily go over several megs. We limit such waste of memory by only    */
/*   actually keeping open several typefaces most recently used. Their     */
/*   number can be set via entry in OS2.INI.                               */
/*                                                                         */
/* For Intelligent Font Interface (IFI) specification please see IFI32.TXT */
/* or the updated version in Lotus WordPro format.                         */
#if 0
#define DEBUG
#endif

#ifndef  __IBMC__
   #error "Please use IBM VisualAge C++ to compile FreeType/2"
#endif

/* Defining the following uses UCONV.DLL instead of the built-in  */
/* translation tables. This code should work on any Warp 4 and    */
/* Warp 3 w/ FixPak 35(?) and above                               */
/* Note: this should be defined before FTIFI.H is #included       */
#define USE_UCONV

/* Defining the following causes FreeType/2 to use modified UGL   */
/* introduced in Warp Server for e-Business a.k.a. Aurora.        */
/* This removes the need to use separate Greek, Hebrew etc.       */
/* variants of UGL and simplifies the things a bit.               */
/* Note: USE_UCONV really should be defined if AURORA is defined! */
#define AURORA

/* Defining the following causes FreeType/2 to fall back to using */
/* the PostScript name of a font (for both the family and face    */
/* names) if no suitable name could be found in the proper places.*/
// #define USE_PSNAME_FALLBACK      // not implemented yet

#define INCL_WINSHELLDATA   /* for accessing OS2.INI */
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_DOSPROCESS
#define INCL_GRE_DCS
#define INCL_GRE_DEVSUPPORT
#define INCL_DDIMISC
#define INCL_IFI
#include <os2.h>
#include <pmddi.h>     /* SSAllocmem(), SSFreemem() and more */

#if defined AURORA && !defined USE_UCONV
#define USE_UCONV
#endif

#ifdef USE_UCONV       /* uconv.h isn't always available */
#include <uconv.h>
#endif  /* USE_UCONV */

#include <string.h>
#include <stdlib.h>         /* min and max macros */

#define  _syscall  _System  /* the IFI headers don't compile without it */

#include "32pmifi.h"        /* IFI header             */
#include "freetype.h"       /* FreeType header        */
#include "ftxkern.h"        /* kerning extension      */
#include "ftxsbit.h"        /* embedded bitmap extension */
#include "ftxwidth.h"       /* glyph width extension  */
#include "ftifi.h"          /* xlate table            */

/* to determine DBCS font face */
#define AT_LEAST_DBCS_GLYPH     3072

/* Font-name structure returned by QueryFullFaces().  The face name buffer
 * follows abFamilyName at the location indicated by cbFacenameOffset (counted
 * from the start of the structure).  The cbLength field indicates the total
 * length, including both the family and face name strings.
 */
typedef struct _FFDESCS2 {
    ULONG cbLength;             // length of this entry (WORD aligned)
    ULONG cbFacenameOffset;     // offset of face name (WORD aligned)
    UCHAR abFamilyName[1];      // family name string
} FFDESCS2, *PFFDESCS2;


/* (indirectly) exported functions */
LONG _System ConvertFontFile(PSZ pszSrc, PSZ pszDestDir, PSZ pszNewName);
HFF  _System LoadFontFile(PSZ pszFileName);
LONG _System UnloadFontFile(HFF hff);
LONG _System QueryFaces(HFF hff, PIFIMETRICS pifiMetrics, ULONG cMetricLen,
                        ULONG cFountCount, ULONG cStart);
HFC  _System OpenFontContext(HFF hff, ULONG ulFont);
LONG _System SetFontContext(HFC hfc, PCONTEXTINFO pci);
LONG _System CloseFontContext(HFC hfc);
LONG _System QueryFaceAttr(HFC hfc, ULONG iQuery, PBYTE pBuffer,
                           ULONG cb, PGLYPH pagi, GLYPH giStart);
LONG _System QueryCharAttr(HFC hfc, PCHARATTR pCharAttr,
                           PBITMAPMETRICS pbmm);
LONG _System QueryFullFaces(HFF hff, PVOID pBuffer, PULONG cBuflen,
                            PULONG cFontCount, ULONG cStart);

FDDISPATCH fdisp = {        /* Font driver dispatch table */
   LoadFontFile,
   QueryFaces,
   UnloadFontFile,
   OpenFontContext,
   SetFontContext,
   CloseFontContext,
   QueryFaceAttr,
   QueryCharAttr,
   NULL,     /* this one is no more used, only the spec fails to mention it */
   ConvertFontFile,
   QueryFullFaces
};



/****************************************************************************/
/* the single exported entry point; this way is faster than exporting every */
/* single function and a bit more flexible                                  */
/*                                                                          */
#pragma export (fdhdr, "FONT_DRIVER_DISPATCH_TABLE", 1)
FDHEADER   fdhdr =
{  /* Font driver Header */
   sizeof(FDHEADER),
   "OS/2 FONT DRIVER",                  /* do not change */
   "TrueType (Using FreeType Engine)",  /* description up to 40 chars */
   IFI_VERSION20,                       /* version */
   1,                                   /* 1 enables IFIMETRICS2 fields */
   &fdisp
};


/****************************************************************************/
/* some debug macros and functions. the debug version logs system requests  */
/* to the file \FTIFI.LOG                                                   */
/*                                                                          */
#ifdef DEBUG
  HFILE LogHandle = NULLHANDLE;
  ULONG Written   = 0;
  char  log[2048] = "";
  char  buf[2048] = "";


char*  itoa10( int i, char* buffer ) {
    char*  ptr  = buffer;
    char*  rptr = buffer;
    char   digit;

    if (i == 0) {
      buffer[0] = '0';
      buffer[1] =  0;
      return buffer;
    }

    if (i < 0) {
      *ptr = '-';
       ptr++; rptr++;
       i   = -i;
    }

    while (i != 0) {
      *ptr = (char) (i % 10 + '0');
       ptr++;
       i  /= 10;
    }

    *ptr = 0;  ptr--;

    while (ptr > rptr) {
      digit = *ptr;
      *ptr  = *rptr;
      *rptr = digit;
       ptr--;
       rptr++;
    }

    return buffer;
}

  #define  COPY(s)     strcpy(log, s)
  #define  CAT(s)      strcat(log, s)
  #define  CATI(v)     strcat(log, itoa10( (int)v, buf ))
  #define  WRITE       DosWrite(LogHandle, log, strlen(log), &Written)

  #define  ERET1(label) { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          goto label;        \
                       }

  #define  ERRRET(e)   { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          return(e);         \
                       }


#else

  #define  COPY(s)
  #define  CAT(s)
  #define  CATI(v)
  #define  WRITE

  #define  ERET1(label)  goto label;

  #define  ERRRET(e)  return(e);

#endif /* DEBUG */


/****************************************************************************/
/*                                                                          */
/* 'engine' :                                                               */
/*                                                                          */
/*  The FreeType engine instance. Although this is a DLL, it isn't          */
/*  supposed to be shared by apps, as it is only called by the OS/2 GRE.    */
/*  This means that there is no need to bother with reentrancy/thread       */
/*  safety, which aren't supported by FreeType 1.0 anyway.                  */
/*                                                                          */
TT_Engine engine;

/****************************************************************************/
/*                                                                          */
/* TList and TListElement :                                                 */
/*                                                                          */
/*  simple structures used to implement a doubly linked list. Lists are     */
/*  used to implement the HFF object lists, as well as the font size and    */
/*  outline caches.                                                         */
/*                                                                          */

typedef struct _TListElement  TListElement, *PListElement;

struct _TListElement
{
  PListElement  next;    /* next element in list - NULL if tail     */
  PListElement  prev;    /* previous element in list - NULL if head */
  long          key;     /* value used for searches                 */
  void*         data;    /* pointer to the listed/cached object     */
};

typedef struct _TList  TList, *PList;
struct _TList
{
  PListElement  head;    /* first element in list - NULL if empty */
  PListElement  tail;    /* last element in list - NULL if empty  */
  int           count;   /* number of elements in list            */
};

static  PListElement  free_elements = 0;

#if 0
/****************************************************************************/
/*                                                                          */
/* TGlyph_Image :                                                           */
/*                                                                          */
/*  structure used to store a glyph's attributes, i.e. outlines and metrics */
/*  Note that we do not cache bitmaps ourselves for the moment.             */
/*                                                                          */
typedef struct _TGlyph_Image  TGlyph_Image,  *PGlyph_Image;

struct _TGlyph_Image
{
  PListElement      element;  /* list element for this glyph image */
  TT_Glyph_Metrics  metrics;
  TT_Outline        outline;
};
#endif


/****************************************************************************/
/*                                                                          */
/* TFontFace :                                                              */
/*                                                                          */
/* a structure related to an open font face. It contains data for each of   */
/* possibly several faces in a .TTC file.                                   */

typedef struct _TFontFace  TFontFace, *PFontFace;

struct _TFontFace
{
   TT_Face       face;            /* handle to actual FreeType face object  */
   TT_Glyph      glyph;           /* handle to FreeType glyph container     */
   TT_CharMap    charMap;         /* handle to FreeType character map       */
   TT_Kerning    directory;       /* kerning directory                      */
   USHORT        *widths;         /* glyph width cache for large fonts      */
   USHORT        *kernIndices;    /* reverse translation cache for kerning  */
   LONG          em_size;         /* points per em square                   */
   ULONG         flags;           /* various FC_* flags (like FC_FLAG_FIXED)*/
#if 0   /* not now */
   TList         sizes;           /* list of live child font sizes          */
#endif
   LONG          charMode;        /* character translation mode :           */
                                  /*   0 = Unicode to UGL                   */
                                  /*   1 = Symbol (no translation)          */
                                  /*   2 = Unicode w/o translation          */
};


/****************************************************************************/
/*                                                                          */
/* TFontFile :                                                              */
/*                                                                          */
/* a structure related to an open font file handle. All TFontFiles are      */
/* kept in a simple linked list. There can be several faces in one font.    */
/* Face(s) information is stored in a variable-length array of TFontFaces.  */
/* A single TFontFile structure exactly corresponds to one HFF.             */

typedef struct _TFontFile  TFontFile, *PFontFile;

struct _TFontFile
{
   PListElement  element;         /* list element for this font face        */
   HFF           hff;             /* HFF handle used from outside           */
   CHAR          filename[260];   /* font file name                         */
   LONG          ref_count;       /* number of times this font file is open */
   ULONG         flags;           /* various FL_* flags                     */
   ULONG         numFaces;        /* number of faces in a file (normally 1) */
   TFontFace     *faces;          /* array of FontFace structures           */
};


/* Flag : The font face has a fixed pitch width */
#define FC_FLAG_FIXED_WIDTH      1

/* Flag : Effectively duplicated  FL_FLAG_DBCS_FILE. This info is    */
/*       kept twice for simplified access                            */
#define FC_FLAG_DBCS_FACE        2

/* Flag : This face is actually an alias, use the underlying face instead */
#define FL_FLAG_FACENAME_ALIAS   8

/* Flag : The font file has a live FreeType face object */
#define FL_FLAG_LIVE_FACE        16

/* Flag : A font file's face has a context open - DON'T CLOSE IT! */
#define FL_FLAG_CONTEXT_OPEN     32

/* Flag : This file has been already opened previously*/
#define FL_FLAG_ALREADY_USED     64

/* Flag : This is a font including DBCS characters; this also means  */
/*       the font driver presents to the system a second, vertically */
/*       rendered, version of this typeface with name prepended by   */
/*       an '@' (used in horizontal-only word processors)            */
/* Note : For TTCs, the whole collection is either DBCS or not. I've */
/*       no idea if there are any TTCs with both DBCS and non-DBCS   */
/*       faces. It's possible, but sounds unlikely.                  */
#define FL_FLAG_DBCS_FILE        128

/* Note, we'll only keep the first max_open_files files with opened */
/* FreeType objects/instances..                                     */
int  max_open_files = 10;

/* flag for fixing rendering bugs of Netscape */
static BOOL fNetscapeFix = FALSE;

/* flag for using fake TNR font */
static BOOL fUseFacenameAlias = FALSE;

/* flag for using fake Bold for DBCS fonts */
static BOOL fUseFakeBold = FALSE;

/* flag for unicode encoding */
static BOOL fUseUnicodeEncoding = TRUE;

/* flag for using MBCS/DBCS flag with Unicode fonts */
static BOOL fForceUnicodeMBCS = TRUE;

/* flag for disabling UNICODE glyphlist for the associate font */
static BOOL fExceptAssocFont = TRUE;

/* flag for disabling UNICODE glyphlist for "combined" aliased fonts */
static BOOL fExceptCombined = TRUE;

/* fixup level for broken style ID workaround */
static BOOL fStyleFix = 0;

/* instance dpi */
static int instance_dpi = 72;

/* default nominal point size */
static long lNominalPtSize = 12;

/* number of processes using the font driver; used by the init/term */
/* routine                                                          */
ULONG  ulProcessCount = 0;

/* current DBCS glyph-association font, if defined */
char szAssocFont[FACESIZE+3] = {0};

/* list of fonts currently aliased to DBCS combined fonts */
PSZ pszCmbAliases = NULL;

/* the list of live faces */
static TList  liveFiles  = { NULL, NULL, 0 };

/* the list of sleeping faces */
static TList  idleFiles = { NULL, NULL, 0 };

/****************************************************************************/
/*                                                                          */
/* TFontSize :                                                              */
/*                                                                          */
/*  a structure related to a opened font context (a.k.a. instance or        */
/*  transform/pointsize). It exactly corresponds to a HFC.                  */
/*                                                                          */

typedef struct _TFontSize  TFontSize, *PFontSize;

struct _TFontSize
{
   PListElement  element;       /* List element for this font size          */
   HFC           hfc;           /* HFC handle used from outside             */
   TT_Instance   instance;      /* handle to FreeType instance              */
   BOOL          transformed;   /* TRUE = rotation/shearing used (rare)     */
   BOOL          vertical;      /* TRUE = vertically rendered DBCS face     */
   BOOL          fakebold;      /* TRUE = fake bold DBCS face               */
   TT_Matrix     matrix;        /* transformation matrix                    */
   PFontFile     file;          /* HFF this context belongs to              */
   ULONG         faceIndex;     /* index of face in a font (for TTCs)       */
   ULONG         scaleFactor;   /* scaling factor used for this context     */
/* TList         outlines;*/    /* outlines cache list                      */
};

/****************************************************************************/
/* array of font context handles. Note that there isn't more than one font  */
/* context open at any time anyway, but we want to be safe..                */
/*                                                                          */
#define MAX_CONTEXTS  5

static TFontSize  contexts[MAX_CONTEXTS]; /* this is rather too much */


/****************************************************************************/
/* few globals used for NLS                                                 */
/*                                                                          */
/* Note: most of the internationalization (I18N) code was kindly provided   */
/*  by Ken Borgendale and Marc L Cohen from IBM (big thanks!). I also       */
/*  received help from Tetsuro Nishimura from IBM Japan.                    */
/*  I was also unable to test the I18N code on actual Japanese, Chinese...  */
/*  etc. systems. But it might work.                                        */
/*                                                                          */

static ULONG  ScriptTag = -1;
static ULONG  LangSysTag = -1;
static ULONG  iLangId = TT_MS_LANGID_ENGLISH_UNITED_STATES; /* language ID  */
static ULONG  uLastGlyph = 255;                  /* last glyph for language */
static PSZ    pGlyphlistName = "SYMBOL";         /* PM383, PMJPN, PMKOR.... */
static BOOL   isGBK = TRUE;                      /* only used for Chinese   */
static ULONG  ulCp[2] = {1};                     /* codepages used          */
static UCHAR  DBCSLead[12];                      /* DBCS lead byte table    */

/* rather simple-minded test to decide if given glyph index is a 'halfchar',*/
/* i.e. Latin character in a DBCS font which is _not_ to be rotated         */
#define is_HALFCHAR(_x)  ((_x) < 0x0400)


/****************************************************************************/
/*                                                                          */
/* interfaceSEId:                                                           */
/*                                                                          */
/* interfaceSEId (Interface-specific Encoding Id) determines what encoding  */
/* the font driver should use if a font includes a Unicode encoding.        */
/*                                                                          */
LONG    interfaceSEId(TT_Face face, BOOL UDCflag, LONG encoding);

/****************************************************************************/
/*                                                                          */
/* LookUpName :                                                             */
/*                                                                          */
/* this function tries to find M$ English name for a face                   */
/* length is limited to FACESIZE (defined by OS/2); returns NULL if         */
/* unsuccessful. warning: the string gets overwritten on the next           */
/* invocation                                                               */
/*                                                                          */
/* TODO: needs enhancing for I18N                                           */
static  char*  LookupName(TT_Face face,  int index );


/****************************************************************************/
/*                                                                          */
/* GetCharMap :                                                             */
/*                                                                          */
/* get suitable charmap from font                                           */
/*                                                                          */
static  ULONG GetCharmap(TT_Face face);


/****************************************************************************/
/*                                                                          */
/* GetOutlineLen :                                                          */
/*                                                                          */
/* get # of bytes needed for glyph outline                                  */
/*                                                                          */
static  int GetOutlineLen(TT_Outline *ol);


/****************************************************************************/
/*                                                                          */
/* GetOutline :                                                             */
/*                                                                          */
/* get glyph outline in PM format                                           */
/*                                                                          */
static  int GetOutline(TT_Outline *ol, PBYTE pb);


/****************************************************************************/
/*                                                                          */
/* ReallyUnicodeName :                                                      */
/*                                                                          */
/* check the font family name to see if it's on our blacklist of fonts      */
/* which lie about using Unicode names/cmaps.                               */
/*                                                                          */
static BOOL ReallyUnicodeName( PCH name, int len );


/****************************************************************************/
/*                                                                          */
/* IsDBCSChar :                                                             */
/*                                                                          */
/* Returns TRUE if character is first byte of a DBCS char, FALSE otherwise  */
/*                                                                          */
BOOL IsDBCSChar(UCHAR c)
{
    ULONG       i;

    for (i = 0; DBCSLead[i] && DBCSLead[i+1]; i += 2)
        if ((c >= DBCSLead[i]) && (c <= DBCSLead[i+1]))
            return TRUE;
    return FALSE;
}



/****************************************************************************/
/*                                                                          */
/* TT_Alloc & TT_Free :                                                     */
/*                                                                          */
/* The following two functions are declared here because including          */
/* the entire ttmemory.h creates more problems than it solves               */
/*                                                                          */
TT_Error  TT_Alloc( long  Size, void**  P );
TT_Error  TT_Free( void**  P );
TT_Error  TTMemory_Init(void);

static  TT_Error  error;

#define  ALLOC( p, size )  TT_Alloc( (size), (void**)&(p) )
#define  FREE( p )         TT_Free( (void**)&(p) )

/****************************************************************************/
/*                                                                          */
/* New_Element :                                                            */
/*                                                                          */
/*   return a fresh list element. Either new or recycled.                   */
/*   returns NULL if out of memory.                                         */
/*                                                                          */
static  PListElement   New_Element( void )
{
  PListElement e = free_elements;

  if (e)
    free_elements = e->next;
  else
  {
    if ( ALLOC( e, sizeof(TListElement) ) )
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
static  void  Done_Element( PListElement  element )
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
static  int  List_Insert( PList  list,  PListElement  element )
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
static  int  List_Remove( PList  list,  PListElement  element )
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
static  PListElement  List_Find( PList  list, long  key )
{
  static PListElement  cur;

  for ( cur=list->head; cur; cur = cur->next )
    if ( cur->key == key )
      return cur;

  /* not found */
  return NULL;
}

/****************************************************************************/
/*                                                                          */
/* Sleep_FontFile :                                                         */
/*                                                                          */
/*   closes a font file's FreeType objects to leave room in memory.         */
/*                                                                          */
static  int  Sleep_FontFile( PFontFile  cur_file )
{
  int  i;

  if (!(cur_file->flags & FL_FLAG_LIVE_FACE))
    ERRRET(-1);  /* already asleep */

  /* is this face in use?  */
  if (cur_file->flags & FL_FLAG_CONTEXT_OPEN) {
     /* move face to top of the list */
     if (List_Remove( &liveFiles, cur_file->element ))
        ERRRET(-1);
     if (List_Insert( &liveFiles, cur_file->element ))
        ERRRET(-1);

     cur_file = (PFontFile)(liveFiles.tail->data);
  }

  /* remove the face from the live list */
  if (List_Remove( &liveFiles, cur_file->element ))
     ERRRET(-1);

  /* add it to the sleep list */
  if (List_Insert( &idleFiles, cur_file->element ))
    ERRRET(-1);

  /* deactivate its objects - we ignore errors there */
  for (i = 0; i < cur_file->numFaces; i++) {
     TT_Done_Glyph( cur_file->faces[i].glyph );
     TT_Close_Face( cur_file->faces[i].face );
  }
  cur_file->flags  &= ~FL_FLAG_LIVE_FACE;

  return 0;
}

/****************************************************************************/
/*                                                                          */
/* Wake_FontFile :                                                          */
/*                                                                          */
/*   awakes a font file, and reloads important data from disk.              */
/*                                                                          */
static  int  Wake_FontFile( PFontFile  cur_file )
{
  static TT_Face              face;
  static TT_Glyph             glyph;
  static TT_CharMap           cmap;
  static TT_Face_Properties   props;
  static PFontFace            cur_face;
  ULONG                       encoding, i;

  if (cur_file->flags & FL_FLAG_LIVE_FACE)
    ERRRET(-1);  /* already awake !! */

  /* OK, try to activate the FreeType objects */
  error = TT_Open_Face(engine, cur_file->filename, &face);
  if (error)
  {
     COPY( "Error while opening " ); CAT( cur_file->filename );
     CAT( ", error code = " ); CATI( error ); CAT( "\r\n" ); WRITE;
     return -1; /* error, can't open file                 */
                /* XXX : should set error condition here! */
  }

  /* Create a glyph container for it */
  error = TT_New_Glyph( face, &glyph );
  if (error)
  {
     COPY( "Error while creating container for " ); CAT( cur_file->filename );
     CAT( ", error code = " ); CATI( error ); CAT( "\r\n" ); WRITE;
     goto Fail_Face;
  }

  /*  now get suitable charmap for this font */
  encoding = GetCharmap(face);
  COPY( "encoding = " ); CATI( encoding >> 16 ); CAT( " " ); CATI( encoding & 0xFFFF );
  CAT(  "\n" ); WRITE;

  error = TT_Get_CharMap(face, encoding & 0xFFFF, &cmap);
  if (error)
  {
     COPY( "Error: No char map in " ); CAT(  cur_file->filename );
     CAT(  "\r\n" ); WRITE;
     goto Fail_Glyph;
  }

  /* Get face properties. Necessary to find out number of fonts for TTCs */
  TT_Get_Face_Properties(face, &props);

  /* all right, now remove the face from the sleep list */
  if (List_Remove( &idleFiles, cur_file->element ))
    ERET1( Fail_Glyph );

  /* add it to the live list */
  if (List_Insert( &liveFiles, cur_file->element ))
    ERET1( Fail_Glyph );

  /* If the file is a TTC, the first face is now opened successfully.     */

  cur_file->numFaces = props.num_Faces;

  /* Now allocate memory for face data (one struct for each face in TTC). */
  if (cur_file->faces == NULL) {
     if (ALLOC(cur_face, sizeof(TFontFace) * cur_file->numFaces))
        ERET1( Fail_Glyph );

     cur_file->faces  = cur_face;
  }
  else
     cur_face = cur_file->faces;

  cur_face->face      = face;  /* possibly first face in a TTC */
  cur_face->glyph     = glyph;
  cur_face->charMap   = cmap;
  cur_file->flags    |= FL_FLAG_LIVE_FACE;


  if (!(cur_file->flags & FL_FLAG_ALREADY_USED)) {
     cur_face->charMode  = encoding >> 16;  /* Unicode, Symbol, ... */
     cur_face->em_size   = props.header->Units_Per_EM;

     /* if a face contains over 1024 glyphs, assume it's a DBCS font - */
     /* VERY probable                                                  */
     /* Note: the newer MS core fonts (Arial, Times New Roman) contain */
     /* over 1200 glyphs hence we raise the limit from 1024 to 3072.   */
     /* Real DBCS fonts probably have more glyphs than that.           */
     TT_Get_Face_Properties(cur_face->face, &props);

     if (props.num_Glyphs > AT_LEAST_DBCS_GLYPH) {
        cur_file->flags |= FL_FLAG_DBCS_FILE;
        cur_face->flags |= FC_FLAG_DBCS_FACE;
     }

     cur_face->widths    = NULL;
     cur_face->kernIndices = NULL;
  }
  /* load kerning directory, if any */
  error = TT_Get_Kerning_Directory(face, &(cur_face->directory));
  if (error)
     cur_face->directory.nTables = 0; /* indicates no kerning in font */

  TT_Flush_Face(face);    /* this is important !  */

  /* open remaining faces if this font is a TTC */
  for (i = 1; i < cur_file->numFaces; i++) {
     error = TT_Open_Collection(engine, cur_file->filename,
                                i, &face);
     if (error)
        return -1;   /* TODO: handle bad TTCs more tolerantly */

     error = TT_New_Glyph( face, &glyph );
     if (error)
        ERET1(Fail_Face);

     encoding = GetCharmap(face);
     COPY( "encoding = " ); CATI( encoding >> 16 ); CAT( " " ); CATI( encoding & 0xFFFF );
     CAT(  "\n" ); WRITE;

     error = TT_Get_CharMap(face, encoding & 0xFFFF, &cmap);
     if (error)
        ERET1(Fail_Glyph);

     cur_face = &(cur_file->faces[i]);

     cur_face->face     = face;
     cur_face->glyph    = glyph;
     cur_face->charMap  = cmap;

     if (!(cur_file->flags & FL_FLAG_ALREADY_USED)) {
        cur_face->em_size  = props.header->Units_Per_EM;
        cur_face->charMode = encoding >> 16;  /* 0 - Unicode; 1 - Symbol */

        if (cur_file->flags & FL_FLAG_DBCS_FILE)
           cur_face->flags |= FC_FLAG_DBCS_FACE;

        cur_face->widths    = NULL;
        cur_face->kernIndices = NULL;
     }

     /* load kerning directory, if any */
     error = TT_Get_Kerning_Directory(face, &(cur_face->directory));
     if (error)
        cur_face->directory.nTables = 0; /* indicates no kerning in font */
  }

  cur_file->flags    |= FL_FLAG_ALREADY_USED; /* indicates some fields need no re-init */

  error = TT_Flush_Face(face);    /* this is important !           */
  if (error) {
     COPY("Error flushing face\r\n"); WRITE;
  }

  return 0;    /* everything is in order, return 0 == success */

Fail_Glyph:
  /* This line isn't really necessary, because the glyph container */
  /* would be destroyed by the following TT_Close_Face anyway. We  */
  /* however use it for the sake of orthodoxy                      */
  TT_Done_Glyph( glyph );

Fail_Face:
  TT_Close_Face(face);

  /* note that in case of error (e.g. out of memory), the face stays */
  /* on the sleeping list                                            */
  return -1;
}

/****************************************************************************/
/*                                                                          */
/* Done_FontFile :                                                          */
/*                                                                          */
/*   destroys a given font file object. This will also destroy all of its   */
/*   live child font sizes (which in turn will destroy the glyph caches).   */
/*   This is done for all faces if the file is a collection.                */
/*                                                                          */
/* WARNING : The font face must be removed from its list by the caller      */
/*           before this function is called.                                */
/*                                                                          */
static  void  Done_FontFile( PFontFile  *file )
{
  static PListElement  element;
  static PListElement  next;
         ULONG         i;

#if 0    /* this part isn't really used and maybe it never will */
  /* destroy its font sizes */
  element = (*face)->sizes.head;
  while (element)
  {
    next = element->next;
    /* XXX : right now, we simply free the font size object, */
    /*       because the instance is destroyed automatically */
    /*       by FreeType.                                    */

    FREE( element->data );
    /* Done_FontSize( (PFontSize)element->data ); - later */

    Done_Element( element );
    element = next;
  }
#endif

  /* now discard the font face itself */
  if ((*file)->flags & FL_FLAG_LIVE_FACE)
  {
     for (i = 0; i < (*file)->numFaces; i++) {
        TT_Done_Glyph( (*file)->faces[i].glyph );
        TT_Close_Face( (*file)->faces[i].face );

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
/*   return the address of the TFontFile corresponding to a given           */
/*   HFF. Note that in our implementation, we could simply to a             */
/*   typecast like '(PFontFile)hff'. However, for safety reasons, we        */
/*   look up the handle in the list.                                        */
/*                                                                          */
static  PFontFile  New_FontFile( char*  file_name )
{
   static PListElement  element;
   static PFontFile     cur_file;
   static TT_CharMap    cmap;

   /* first, check if it's already open - in the live list */
   for ( element = liveFiles.head; element; element = element->next )
   {
     cur_file = (PFontFile)element->data;
     if (strcmp( cur_file->filename, file_name ) == 0)
       goto Exit_Same;
   }

   /* check in the idle list */
   for ( element = idleFiles.head; element; element = element->next )
   {
     cur_file = (PFontFile)element->data;
     if (strcmp( cur_file->filename, file_name ) == 0)
       goto Exit_Same;
   }

   /* OK, this file isn't opened yet. Create a new font face object     */
   /* then try to wake it up. This will fail if the file can't be found */
   /* or if we lack memory..                                            */

   element = New_Element();
   if (!element)
     ERRRET(NULL);

   if ( ALLOC( cur_file, sizeof(TFontFile) ) )
     ERET1( Fail_Element );

   element->data        = cur_file;
   element->key         = (long)cur_file;         /* use the HFF as cur key */

   cur_file->element    = element;
   cur_file->ref_count  = 1;
   cur_file->hff        = (HFF)cur_file;
   strcpy( cur_file->filename, file_name);
   cur_file->flags      = 0;
   cur_file->faces      = NULL;
#if 0  /* not used */
   cur_face->sizes.head = NULL;
   cur_face->sizes.tail = NULL;
   cur_face->sizes.count= 0;
#endif

   /* add new font face to sleep list */
   if (List_Insert( &idleFiles, element ))
     ERET1( Fail_File );

   /* Make enough room in the live list */
   if ( liveFiles.count >= max_open_files)
   {
     COPY( "rolling...\n" ); WRITE;
     if (Sleep_FontFile( (PFontFile)(liveFiles.tail->data) ))
       ERET1( Fail_File );
   }

   /* wake new font file */
   if ( Wake_FontFile( cur_file ) )
   {
     COPY( "could not open/wake " ); CAT( file_name ); CAT( "\r\n" ); WRITE;
     if (List_Remove( &idleFiles, element ))
       ERET1( Fail_File );

     ERET1( Fail_File );
   }

   return cur_file;      /* everything is in order */

Fail_File:
   FREE( cur_file );

Fail_Element:
   Done_Element( element );
   return  NULL;

Exit_Same:
   cur_file->ref_count++;  /* increment reference count */

   COPY( " -> (duplicate) hff = " ); CATI( cur_file->hff );
   CAT( "\r\n" ); WRITE;

   return cur_file;        /* no sense going on */
}

/****************************************************************************/
/*                                                                          */
/* getFontFile :                                                            */
/*                                                                          */
/*   return the address of the TFontFile corresponding to a given           */
/*   HFF. If asleep, the file and its face object(s) is awoken.             */
/*                                                                          */
PFontFile  getFontFile( HFF  hff )
{
  static PListElement  element;

  /* look in the live list first */
  element = List_Find( &liveFiles, (long)hff );
  if (element)
  {
    /* move it to the front of the live list - if it isn't already */
    if ( liveFiles.head != element )
    {
      if ( List_Remove( &liveFiles, element ) )
        ERRRET( NULL );

      if ( List_Insert( &liveFiles, element ) )
        ERRRET( NULL );
    }
    return (PFontFile)(element->data);
  }

  /* the file may be asleep, look in the second list */
  element = List_Find( &idleFiles, (long)hff );
  if (element)
  {
    /* we need to awake the font, but before that, we must be sure */
    /* that there is enough room in the live list                  */
    if ( liveFiles.count >= max_open_files )
      if (Sleep_FontFile( (PFontFile)(liveFiles.tail->data) ))
        ERRRET( NULL );

    if ( Wake_FontFile( (PFontFile)(element->data) ) )
      ERRRET( NULL );

    COPY ( "hff " ); CATI( hff ); CAT( " awoken\n" ); WRITE;
    return (PFontFile)(element->data);
  }

  COPY( "Could not find hff " ); CATI( hff ); CAT( " in lists\n" ); WRITE;

#ifdef DEBUG

  /* dump files lists */
  COPY( "Live files : " ); CATI( liveFiles.count ); CAT( "\r\n" ); WRITE;

  for (element = liveFiles.head; element; element = element->next)
  {
    COPY( ((PFontFile)(element->data))->filename ); CAT("\r\n");WRITE;
  }

  COPY( "Idle files : " ); CATI( idleFiles.count ); CAT( "\r\n" ); WRITE;
  for (element = idleFiles.head; element; element = element->next)
  {
    COPY( ((PFontFile)(element->data))->filename ); CAT("\r\n");WRITE;
  }
#endif

  /* could not find the HFF in the list */
  return NULL;
}


/****************************************************************************/
/*                                                                          */
/* getFontSize :                                                            */
/*                                                                          */
/*   return pointer to a TFontSize given a HFC handle, NULL if error        */
/*                                                                          */
static  PFontSize  getFontSize( HFC  hfc )
{
   int i;
   for ( i = 0; i < MAX_CONTEXTS; i++ )
      if ( contexts[i].hfc == hfc ) {
         return &contexts[i];
      }

   return NULL;
}

#ifdef USE_UCONV

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
/*   a function to cache UCONV objects based on current process. Now the    */
/*  _DLL_InitTerm function has been enhanced to destroy no longer needed    */
/*  UCONV object when a process terminates.                                 */
int getUconvObject(UniChar *name, UconvObject *ConvObj, ULONG UconvType) {
   PPIB                  ppib;   /* process/thread info blocks */
   PTIB                  ptib;
   PID                   curPid; /* current process ID */
   int                   i;

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
void CleanUCONVCache(void) {
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
#endif  /* USE_UCONV */

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
/*   a function to check whther Korean Hanja or not.                        */
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
/* PM2TT :                                                                  */
/*                                                                          */
/*   a function to convert PM codepoint to TT glyph index. This is the real */
/*   tricky part.                                                           */
/*    mode = TRANSLATE_UGL - translate UGL to Unicode                       */
/*    mode = TRANSLATE_SYMBOL - no translation - symbol font                */
/*    mode = TRANSLATE_UNICODE- no translation - Unicode                    */
static  int PM2TT( TT_CharMap  charMap,
                   ULONG       mode,
                   int         index)
{
#ifdef USE_UCONV
   /* Brand new version that uses UCONV.DLL. This should make FreeType/2 */
   /* smaller and at the same time more flexible as it now should use    */
   /* the Unicode translation tables supplied with base OS/2 Warp 4.     */
   /* Unfortunately there's a complication (again) since UCONV objects   */
   /* created in one process can't be used in another. Therefore we      */
   /* keep a small cache of recently used UCONV objects.                 */
   static UconvObject   UGLObj = NULL; /* UGL->Unicode conversion object */
   static UconvObject   DBCSObj = NULL; /*DBCS->Unicode conversion object*/
   static BOOL          UconvSet         = FALSE;
          char          char_data[4], *pin_char_str;
          size_t        in_bytes_left, uni_chars_left, num_subs;
          UniChar       *pout_uni_str, uni_buffer[4];
          int           rc;
   static UniChar       uglName[10]        = L"OS2UGL";
   static UniChar       uglNameBig5[10]    = L"IBM-950";
   static UniChar       uglNameSJIS[10]    = L"IBM-943";
   static UniChar       uglNameWansung[10] = L"IBM-949";
   static UniChar       uglNameGB2312[10]  = L"IBM-1381";    /* Probably */

   switch (mode) {
      case TRANSLATE_UGL:
         /* on Aurora there's only one UGL so there's no need to bother */
         #ifndef AURORA
         if (UconvSet == FALSE) {
            switch (iLangId) {   /* select proper conversion table */
               case TT_MS_LANGID_GREEK_GREECE:
                  strncpy((char*)uglName, (char*)L"OS2UGLG", 16);
                  break;
               case TT_MS_LANGID_HEBREW_ISRAEL:
                  strncpy((char*)uglName, (char*)L"OS2UGLH", 16);
                  break;
               case TT_MS_LANGID_ARABIC_SAUDI_ARABIA:
                  strncpy((char*)uglName, (char*)L"OS2UGLA", 16);
                  break;
            }
            UconvSet = TRUE;
         }
         #endif
         /* get Uconv object - either new or cached */
         if (getUconvObject(uglName, &UGLObj, UCONV_TYPE_UGL) != 0)
            return 0;

#if 0
         /* not really necessary as the conversion will take care of invalid values */
         if (index > MAX_GLYPH)
            return 0;
#endif

#if 0
         if( index > 31 )
         {
            rc = TT_Char_Index( charMap, index );

            COPY("UGL translation : PM2TT mode = "); CATI( mode ); CAT(" index = "); CATI( index );
            CAT( " result = "); CATI( rc );
            CAT("\r\n"); WRITE;

            if( rc )
                return rc;
         }
#endif

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

         COPY("UGL translation : PM2TT mode = "); CATI( mode ); CAT(" index = "); CATI( index );
         CAT(" unicode = "); CATI( uni_buffer[ 0 ]); CAT(" rc = "); CATI( rc );
         CAT( " result = "); CATI( TT_Char_Index( charMap, uni_buffer[ 0 ]));
         CAT("\r\n"); WRITE;

         if (rc != ULS_SUCCESS)
            return 0;
         else
            return TT_Char_Index(charMap, uni_buffer[0]);

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

         rc = TT_Char_Index( charMap, index );

         COPY("Passthrough : PM2TT mode = "); CATI( mode ); CAT(" index = "); CATI( index );
         CAT(" result = "); CATI( rc );
         CAT("\r\n"); WRITE;

         if( rc != 0 )
            return rc;

         /* If the character wasn't found we do some fallback logic here */

         if(( mode == TRANSLATE_WANSUNG ) && kr_IsHanja( index ))
         {
            /* For Korean only: if the missing character was a hanja ideograph,
             * substitute the equivalent hangul.
             */
            COPY("Convert Hanja to Hangul\r\n"); WRITE;

            index = kr_hj2hg( index );
         }
         else if(( mode > TRANSLATE_UNICODE ) && (( index == 190 ) || ( index == 314 ) || ( index == 947 )))
         {
            /* If the missing character (UGL index) was an Asian currency symbol
             * (yen/yuan/won), then substitute the backslash (which is mapped
             * to the currency sign under most CJK codepages anyway).
             */
            COPY("Convert Asian currency sign (UGL "); CATI( index );
            CAT(") to ASCII 92 (backslash)\r\n"); WRITE;

            index = 92;
         }
         else if(( mode == TRANSLATE_UNICODE ) && (( index == 0xA5 ) || ( index == 0x20A9 )))
         {
            /* Same as the above, except the incoming codepoint is UCS-2 */
            COPY("Convert Asian currency sign (UCS "); CATI( index );
            CAT(") to ASCII 92 (backslash)\r\n"); WRITE;

            index = 92;
         }
         else {
            COPY("Character not found, PM2TT mode = "); CATI( mode );
            CAT(" index = "); CATI( index );
            CAT("\r\n"); WRITE;
            return 0;
         }

         COPY("Passthrough : PM2TT mode = "); CATI( mode ); CAT(" index = "); CATI( index );
         CAT(" result = "); CATI( TT_Char_Index( charMap, index ));
         CAT("\r\n"); WRITE;

         return TT_Char_Index( charMap, index );


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

            COPY("translation : PM2TT mode = "); CATI( mode ); CAT(" index = "); CATI( index );
            CAT(" unicode = "); CATI( uni_buffer[ 0 ]); CAT(" rc = "); CATI( rc );
            CAT( " result = "); CATI( TT_Char_Index( charMap, uni_buffer[0]));
            CAT("\r\n"); WRITE;

            if (rc != ULS_SUCCESS)
                return 0;

            rc = TT_Char_Index( charMap, uni_buffer[0]);

            if(( rc == 0 ) && ( mode == TRANSLATE_UNI_WANSUNG ) && kr_IsHanja( index ))
            {
                /* For Korean only: if the missing character was a hanja ideograph,
                 * substitute the equivalent hangul.
                 */
                COPY("Convert Hanja to Hangul\r\n"); WRITE;

                index = kr_hj2hg( index );
            }
            else if(( rc == 0 ) && (( index == 190 ) || ( index == 314 ) || ( index == 947 )))
            {
                /* If the missing character was the Asian currency symbol
                 * (yen/yuan/won), then substitute the backslash (which is mapped
                 * to the currency sign under most CJK codepages anyway).
                 */
                COPY("Convert Asian currency sign (UGL "); CATI( index );
                CAT(") to ASCII 92 (backslash)\r\n"); WRITE;

                index = 92;
            }
            else
                break;
         }

         return rc;

      default:
         return 0;
   }
#else
   switch (mode)
   {
      /* convert from PM383 to Unicode */
      case TRANSLATE_UGL:
         /* TODO: Hebrew and Arabic UGL */
         if (iLangId == TT_MS_LANGID_GREEK_GREECE)  /* use Greek UGL */
            if ((index >= GREEK_START) && (index < GREEK_START + GREEK_GLYPHS))
               return TT_Char_Index(charMap, SubUGLGreek[index - GREEK_START]);

         if (index <= MAX_GLYPH)
            return TT_Char_Index(charMap, UGL2Uni[index]);
         else
            ERRRET(0);

      case TRANSLATE_SYMBOL :
      case TRANSLATE_UNICODE:
      case TRANSLATE_BIG5:
      case TRANSLATE_SJIS:
      case TRANSLATE_WANSUNG:
      case TRANSLATE_GB2312:
         return TT_Char_Index(charMap, index);

      default:
         return 0;
   }
#endif
}

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
int mystricmp(const char *s1, const char *s2) {
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
char *mystrrchr(char *s, char c) {
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
/*                                                                          */
/* getFakeBoldMetrics :                                                     */
/*                                                                          */
/* A simple function for fake bold metrics calculation                      */
/* returns added width in 26.6 format and bitmap metrics for fake bold face */
static
TT_Pos getFakeBoldMetrics( TT_Pos         Width266,
                           TT_Pos         Height266,
                           TT_Raster_Map* bitmapmetrics)
{
   TT_Pos addedWid266=0;

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
      bitmapmetrics->cols   = ((bitmapmetrics->width + 31) / 8) & -4;
      bitmapmetrics->flow   = TT_Flow_Down;
      bitmapmetrics->bitmap = NULL;
      bitmapmetrics->size   = bitmapmetrics->rows * bitmapmetrics->cols;
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
TT_Error buildFakeBoldBitmap( PVOID bitmap,
                              ULONG buflength,
                              int   oldwidth,
                              int   addwidth,
                              int   rows,
                              int   cols )
{
   unsigned char *pbyte, bits;
   int  i, j, row, col, newcols;

   if ( (addwidth < 1) || (addwidth > 7) )
      return TT_Err_Invalid_Argument;

   newcols = ((oldwidth + addwidth + 31) / 8) & -4;

   if ( (newcols*rows) > buflength )
      return TT_Err_Out_Of_Memory;

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

   return TT_Err_Ok;
}

/* -------------------------------------------------------------------------*/
/* here begin the exported functions                                        */
/* -------------------------------------------------------------------------*/

/****************************************************************************/
/*                                                                          */
/* ConvertFontFile :                                                        */
/*                                                                          */
/*  Install/delete font file                                                */
/*                                                                          */
LONG _System ConvertFontFile( PSZ  source,
                              PSZ  dest_dir,
                              PSZ  new_name )
{
   PSZ  source_name;

   COPY("ConvertFontFile: Src = "); CAT(source);
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
       ERRRET(-1);  /* XXX : we should probably set the error condition */

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
/* LoadFontFile :                                                           */
/*                                                                          */
/*  open a font file and return a handle for it                             */
/*                                                                          */
HFF _System LoadFontFile( PSZ file_name )
{
   PSZ           extension;
   PFontFile     cur_file;
   PListElement  element;

   COPY( "LoadFontFile " ); CAT( file_name ); CAT( "\r\n" ); WRITE;

   /* first check if the file extension is supported */
   extension = mystrrchr( file_name, '.' );  /* find the last dot */
   if ( extension == NULL ||
        (mystricmp(extension, ".TTF") &&
         mystricmp(extension, ".TTC")) )
     return ((HFF)-1);

   /* now actually open the file */
   cur_file = New_FontFile( file_name );
   if (cur_file)
     return  cur_file->hff;
   else
     return (HFF)-1;
}

/****************************************************************************/
/*                                                                          */
/* UnloadFontFile :                                                         */
/*                                                                          */
/*  destroy resources associated with a given HFF                           */
/*                                                                          */
LONG _System UnloadFontFile( HFF hff )
{
   PListElement  element;

   COPY("UnloadFontFile: hff = "); CATI((int) hff); CAT("\r\n"); WRITE;

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
/* QueryFaces :                                                             */
/*                                                                          */
/*   Return font metrics. This routine has to do a lot of not very          */
/*   hard work.                                                             */
/*                                                                          */
LONG _System QueryFaces( HFF          hff,
                         PIFIMETRICS  pifiMetrics,
                         ULONG        cMetricLen,
                         ULONG        cFontCount,
                         ULONG        cStart)
{
   static TT_Face_Properties   properties;
   static IFIMETRICS           ifi;   /* temporary structure */
          PFontFace            pface;
          TT_Header            *phead;
          TT_Horizontal_Header *phhea;
          TT_OS2               *pOS2;
          TT_Postscript        *ppost;
          PIFIMETRICS          pifi2;
          PIFIMETRICS          pifi3;

          PFontFile            file;
          LONG                 index, faceIndex, ifiCount = 0;
          LONG                 cyScreen;
          ULONG                range1, range2;
          USHORT               fsTrunc;
          SHORT                name_len;
          BOOL                 fCMB;
          BOOL                 fUni;
          char                 *name, *style;
          char                 *pszabr;
   static char                 *wrongName = "Wrong Name Font";


   COPY( "QueryFaces: hff = " ); CATI( hff );
   CAT(  ", cFontCount = " );    CATI( cFontCount );
   CAT(  ", cStart = " );        CATI( cStart );
   CAT(  ", cMetricLen = " );    CATI( cMetricLen );
   CAT( "\r\n");
   WRITE;

   file = getFontFile(hff);
   if (!file)
      ERRRET(-1) /* error, invalid handle */

   COPY( "Filename: " ); CAT( file->filename ); CAT("\r\n"); WRITE;
   if (cMetricLen == 0) {   /* only number of faces is requested */
      LONG rc = file->numFaces;

      if( fUseFacenameAlias )
      {
        /* create an alias for Times New Roman */
        pface = &(file->faces[0]);
        name = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
        if ( name && !strcmp(name, "Times New Roman")) {
            file->flags |= FL_FLAG_FACENAME_ALIAS;
            rc *= 2;
        }
      }
      if (file->flags & FL_FLAG_DBCS_FILE)
      {
         if( fUseFakeBold )
            rc *= 3;
         else
         rc *= 2;
      }

      return rc;
   }

   /* See if this font is aliased to a CMB file */
   fCMB = FALSE;
   if ( pszCmbAliases && fExceptCombined ) {
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

   for (faceIndex = 0; faceIndex < file->numFaces; faceIndex++) {
      /* get pointer to this face's data */
      pface = &(file->faces[faceIndex]);

      TT_Get_Face_Properties( pface->face, &properties );

      pOS2  = properties.os2;
      phead = properties.header;
      phhea = properties.horizontal;
      ppost = properties.postscript;

      /* get font name and check it's really found */
      name = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
      if (name == NULL)
#if 1
         name = wrongName;
#else
         ERET1(Fail);
#endif

      name_len = strlen( name );
      strncpy(ifi.szFamilyname, name, FACESIZE);
      ifi.szFamilyname[FACESIZE - 1] = '\0';
      fsTrunc = (name_len > FACESIZE) ? IFIMETRICS_FAMTRUNC : 0;

      ifi.fsSelection = 0;
      ifi.szFacename[0] = '\0';

#if 1
      if ( fStyleFix &&
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

      }
#endif

      // Get the full face name if we didn't set one up above
      if ( ifi.szFacename[0] == '\0') {
          name = LookupName(pface->face, TT_NAME_ID_FULL_NAME);
          if (name == NULL) {
#if 1
              name = wrongName;
#else
              ERET1(Fail);
#endif
          }
          strncpy(ifi.szFacename, name, FACESIZE);
          fsTrunc |= (strlen(name) > FACESIZE) ? IFIMETRICS_FACETRUNC : 0;
      }
      ifi.szFacename[FACESIZE - 1] = '\0';

      /* Now we set the glyphlist to use. This involves some work... */

      if ( !ReallyUnicodeName( ifi.szFacename, strlen( ifi.szFacename ))) {
          /* Font is on blacklist of non-Unicode fonts masquerading as one */
          switch (iLangId) {
             case TT_MS_LANGID_JAPANESE_JAPAN:
                pface->charMode = TRANSLATE_SJIS;
                break;
             case TT_MS_LANGID_CHINESE_PRC:
             case TT_MS_LANGID_CHINESE_SINGAPORE:
                pface->charMode = TRANSLATE_GB2312;
                break;
             case TT_MS_LANGID_CHINESE_TAIWAN:
             case TT_MS_LANGID_CHINESE_HONG_KONG:
                pface->charMode = TRANSLATE_BIG5;
                break;
             default:
             case TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA:
             case TT_MS_LANGID_KOREAN_JOHAB_KOREA:
                pface->charMode = TRANSLATE_WANSUNG;
                break;
         }
      }

      /* If Unicode cmap exists in font and it contains more than             */
      /* AT_LEAST_DBCS_GLYPH glyphs then do not translate between UGL and     */
      /* Unicode, but use straight Unicode.  But first check if it's a DBCS   */
      /* font and handle it properly.  (Note: fUseUnicodeEncoding==TRUE       */
      /* overrides the AT_LEAST_DBCS_GLYPH condition.)                        */
      else if ((pface->charMode == TRANSLATE_UGL) &&
          (fUseUnicodeEncoding || (properties.num_Glyphs > AT_LEAST_DBCS_GLYPH)))
      {
         LONG  specEnc = PSEID_UNICODE;
         BOOL  UDCflag = FALSE;   /* !!!!TODO: UDC support */

         /* fExceptAssocFont is a workaround for when a Unicode font is being
          * used as the DBCS association font (PM_AssociateFont).  This won't
          * work if the glyphlist is UNICODE, so we need to force use of the
          * correct Asian glyphlist instead.
          * fExceptCombined is a similar workaround for where the font file is
          * aliased to a CMB passthrough file containing associated bitmaps for
          * the glyphs.  This appears broken in at least some cases where the
          * glyphlist is UNICODE; so, again, we force an Asian glyphlist.
          */
         if (( fExceptCombined && fCMB ) ||
             ( fExceptAssocFont && !mystricmp(szAssocFont, ifi.szFacename)))
         {
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
            specEnc = interfaceSEId(pface->face, UDCflag, defEnc);
         }
         else if (fUseUnicodeEncoding)
            specEnc = PSEID_UNICODE;
         else if (( iLangId == TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA ) &&
                  ( TT_Char_Index( pface->charMap, 0xAC00 ) != 0 ))
            specEnc = PSEID_WANSUNG;
         else if (( iLangId == TT_MS_LANGID_JAPANESE_JAPAN ) &&
                  ( TT_Char_Index( pface->charMap, 0x3042 ) != 0 ) &&
                  ( TT_Char_Index( pface->charMap, 0x65E5 ) != 0 ))
            specEnc = PSEID_SHIFTJIS;
         else if (( iLangId == TT_MS_LANGID_CHINESE_TAIWAN ) &&
                  ( TT_Char_Index( pface->charMap, 0x3105 ) != 0 ))
            specEnc = PSEID_BIG5;
         else if (( iLangId == TT_MS_LANGID_CHINESE_PRC ) &&
                  ( TT_Char_Index( pface->charMap, 0x3105 ) != 0 ))
            specEnc = PSEID_PRC;
         else
            specEnc = interfaceSEId(pface->face, UDCflag, PSEID_UNICODE);

         COPY("specEnc = "); CATI( specEnc ); CAT("\r\n"); WRITE;
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

      COPY("szGlyphlistName = "); CAT( ifi.szGlyphlistName );
      CAT( " charMode = "); CATI( pface->charMode );
      CAT( " num_Glyphs = "); CATI( properties.num_Glyphs ); CAT("\n");
      WRITE;

      ifi.idRegistry         = 0;
      ifi.lCapEmHeight       = phead->Units_Per_EM; /* ??? probably correct  */
      ifi.lXHeight           = phead->yMax /2;      /* IBM TRUETYPE.DLL does */
      ifi.lMaxAscender       = pOS2->usWinAscent;

      if ((LONG)pOS2->usWinDescent >= 0)
         ifi.lMaxDescender   = pOS2->usWinDescent;
      else
         ifi.lMaxDescender   = -pOS2->usWinDescent;

      ifi.lLowerCaseAscent   = phhea->Ascender;
      ifi.lLowerCaseDescent  = -phhea->Descender;

      ifi.lInternalLeading   = ifi.lMaxAscender + ifi.lMaxDescender
                               - ifi.lCapEmHeight;

      ifi.lExternalLeading    = 0;
      ifi.lAveCharWidth       = pOS2->xAvgCharWidth;
      ifi.lMaxCharInc         = phhea->advance_Width_Max;
      ifi.lEmInc              = phead->Units_Per_EM;
      ifi.lMaxBaselineExt     = ifi.lMaxAscender + ifi.lMaxDescender;
      ifi.fxCharSlope         = -ppost->italicAngle;    /* is this correct ?  */
      ifi.fxInlineDir         = 0;
      ifi.fxCharRot           = 0;
      ifi.usWeightClass       = pOS2->usWeightClass;    /* hopefully OK       */
      ifi.usWidthClass        = pOS2->usWidthClass;
      ifi.lEmSquareSizeX      = phead->Units_Per_EM;
      ifi.lEmSquareSizeY      = phead->Units_Per_EM;    /* probably correct   */
      ifi.giFirstChar         = 0;            /* following values should work */
      ifi.giLastChar          = MAX_GLYPH;
      ifi.giDefaultChar       = 0;
      ifi.giBreakChar         = 32;

      /* Point size metrics are constant for all fonts */
      ifi.usNominalPointSize  = lNominalPtSize * 10;
      ifi.usMinimumPointSize  = 10;    /* minimum supported size (1pt)          */
      ifi.usMaximumPointSize  = 10000; /* limit to 1000 pt (like the ATM fonts) */

#if 0
      /* Set the LICENSED flag for Restricted, P&P or Editable licenses */
      ifi.fsType              = (pOS2->fsType & 0xE) ? IFIMETRICS_LICENSED : 0;
#else
      /* Imitate TRUETYPE.DLL behaviour */
      ifi.fsType              = (pOS2->fsType) ? IFIMETRICS_LICENSED : 0;
#endif
      ifi.fsDefn              = IFIMETRICS_OUTLINE;  /* always with TrueType  */
      ifi.fsCapabilities      = 0;  /* must be zero according to the IFI spec */
      ifi.lSubscriptXSize     = pOS2->ySubscriptXSize;
      ifi.lSubscriptYSize     = pOS2->ySubscriptYSize;
      ifi.lSubscriptXOffset   = pOS2->ySubscriptXOffset;
      ifi.lSubscriptYOffset   = pOS2->ySubscriptYOffset;
      ifi.lSuperscriptXSize   = pOS2->ySuperscriptXSize;
      ifi.lSuperscriptYSize   = pOS2->ySuperscriptYSize;
      ifi.lSuperscriptXOffset = pOS2->ySuperscriptXOffset;
      ifi.lSuperscriptYOffset = pOS2->ySuperscriptYOffset;
      ifi.lUnderscoreSize     = ppost->underlineThickness;
      if (ifi.lUnderscoreSize >= 100)
         ifi.lUnderscoreSize = 50;  /* little fix for Arial & others */
      ifi.lUnderscorePosition = -ppost->underlinePosition;
      if (ifi.lUnderscorePosition == 97)
         ifi.lUnderscorePosition = 150;  /* little fix for Gulim.ttc */
      ifi.lStrikeoutSize      = pOS2->yStrikeoutSize;
      ifi.lStrikeoutPosition  = pOS2->yStrikeoutPosition;

      /* flag the font name(s) as truncated if they were > 31 characters */
      ifi.fsType |= fsTrunc;

      /* A sneaky little trick gleaned from the PMATM driver: IFIMETRICS can
       * store charset flags in fsCapabilities; if set, GPI/GRE will move them
       * into the fsDefn and fsSelection fields of FONTMETRICS.
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

      ifi.ulMetricsLength = sizeof(IFIMETRICS);
      memcpy( ifi.panose, pOS2->panose, 10 );

      /* report support for kerning if font supports it */
      if (pface->directory.nTables != 0 &&
          pface->directory.tables[0].format == 0) { /* we support only format */
         ifi.cKerningPairs   = (pface->directory.tables[0].length - 8) / 6;
         ifi.fsType |= IFIMETRICS_KERNING;
      }
      else
         ifi.cKerningPairs   = 0;

      /* Note that the following field seems to be the only reliable method of */
      /* recognizing a TT font from an app!  Not that it should be done.       */
      ifi.ulFontClass        = 0x10D; /* just like TRUETYPE.DLL */

      /* the following adjustment are needed because the TT spec defines */
      /* usWeightClass and fsType differently                            */
      if (ifi.usWeightClass >= 100)
         ifi.usWeightClass /= 100;
      if (ifi.usWeightClass == 4)
         ifi.usWeightClass = 5;    /* does this help? */
      if (pOS2->panose[3] == 9) {
         ifi.fsType |= IFIMETRICS_FIXED;
         pface->flags |= FC_FLAG_FIXED_WIDTH; /* we'll need this later */
      }

      switch (pface->charMode) {      /* adjustments for var. encodings */
         case TRANSLATE_UNICODE:
            ifi.giLastChar = pOS2->usLastCharIndex;
            ifi.fsType |= IFIMETRICS_UNICODE;
            if ( fForceUnicodeMBCS || ( file->flags & FL_FLAG_DBCS_FILE ))
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
#if 1
      /* ifimetrics.h doesn't define a flag for bold, but the OS/2 toolkit
       * headers and the TT spec both specify 0x20 for this purpose.
       */
      if (pOS2->fsSelection & 0x20) {
          ifi.fsSelection |= TTFMETRICS_BOLD;
      }

      if (( ifi.fsSelection & TTFMETRICS_BOLD ) && ( ifi.usWeightClass < 6 ))
         ifi.usWeightClass = 7;
#endif

      index = faceIndex;
      /* copy the right amount of data to output buffer,         */
      /* also handle the 'fake' vertically rendered DBCS fonts   */
      if( file->flags & FL_FLAG_DBCS_FILE )
      {
        if( fUseFakeBold )
            index *= 3;
        else
            index *= 2;
      }
      else if( fUseFacenameAlias && ( file->flags & FL_FLAG_FACENAME_ALIAS ))
        index *= 2;

      if ((index >= cStart) && (index < (cStart + cFontCount))) {
         memcpy((((PBYTE) pifiMetrics) + ifiCount), &ifi,
            sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
         ifiCount += cMetricLen;
      }
      index++;

      if( file->flags & FL_FLAG_DBCS_FILE )
      {
          if ((index >= cStart) &&
              (index < (cStart + cFontCount))) {
             pifi2 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
             memcpy(pifi2, &ifi,
                    sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
             strncpy(pifi2->szFamilyname + 1, ifi.szFamilyname, FACESIZE - 1);
             pifi2->szFamilyname[ FACESIZE - 1 ] = '\0';
             pifi2->szFamilyname[0] = '@';
             strncpy(pifi2->szFacename + 1, ifi.szFacename, FACESIZE - 1);
             pifi2->szFacename[ FACESIZE - 1 ] = '\0';
             pifi2->szFacename[0] = '@';
             ifiCount += cMetricLen;
          }
          index++;

        if( fUseFakeBold )
        {
          /* handle the 'fake' bold faced DBCS fonts  */
          if ((index >= cStart) &&
              (index < (cStart + cFontCount))) {

             pifi3 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
             memcpy(pifi3, &ifi,
                    sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
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
      else if( fUseFacenameAlias && ( file->flags & FL_FLAG_FACENAME_ALIAS ))
      {
         if ((index >= cStart) &&
             (index < (cStart + cFontCount))) {
            pifi2 = (PIFIMETRICS) (((PBYTE) pifiMetrics) + ifiCount);
            memcpy(pifi2, &ifi,
                   sizeof(IFIMETRICS) > cMetricLen ? cMetricLen : sizeof(IFIMETRICS));
            strcpy(pifi2->szFamilyname, "Roman");
            switch (strlen(ifi.szFacename)) {  /* This looks weird but... works */
                case 15: /* Times New Roman */
                    strcpy(pifi2->szFacename, "Tms Rmn");
                    break;
                case 20: /* Times New Roman Bold*/
                    strcpy(pifi2->szFacename, "Tms Rmn Bold");
                    break;
                case 22: /* Times New Roman Italic*/
                    strcpy(pifi2->szFacename, "Tms Rmn Italic");
                    break;
                case 27: /* Times New Roman Bold Italic*/
                    strcpy(pifi2->szFacename, "Tms Rmn Bold Italic");
                    break;
            }
            ifiCount += cMetricLen;
         }
      }
   }

Exit:
   TT_Flush_Face(pface->face);
   return cFontCount;

Fail:
   TT_Flush_Face(pface->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* OpenFontContext :                                                        */
/*                                                                          */
/*  open new font context                                                   */
/*                                                                          */
HFC _System OpenFontContext( HFF    hff,
                             ULONG  ulFont)
{
          int          i = 0;
   static TT_Instance  instance;
   static PFontFile    file;
          ULONG        faceIndex;

   COPY("OpenFontContext: hff = "); CATI((int) hff); CAT("\r\n");
   COPY("              ulFont = "); CATI((int) ulFont); CAT("\r\n");
   WRITE;

   file = getFontFile(hff);
   if (!file)
      ERRRET((HFC)-1) /* error, invalid font handle */

   /* calculate real face index in font file */
   if( file->flags & FL_FLAG_DBCS_FILE )
   {
        if( fUseFakeBold )
            faceIndex = ulFont / 3;
        else
        faceIndex = ulFont / 2;
   }
   else
        faceIndex = ulFont;

   if ( fUseFacenameAlias && ( file->flags & FL_FLAG_FACENAME_ALIAS ))
      /* This font isn't real! */
      faceIndex = ulFont / 2;

   if (faceIndex >= file->numFaces)
      ERRRET((HFC)-1)

   /* OK, create new instance with defaults */
   error = TT_New_Instance( file->faces[faceIndex].face, &instance);
   if (error)
     ERET1( Fail );

   /* Instance resolution is set and is never changed     */
   error = TT_Set_Instance_Resolutions(instance, instance_dpi, instance_dpi);
   if (error)
      ERRRET((HFC)-1)

   /* find first unused index */
   i = 0;
   while ((contexts[i].hfc != 0) && (i < MAX_CONTEXTS))
      i++;

   if (i == MAX_CONTEXTS)
     ERET1( Fail );  /* no free slot in table */

   contexts[i].hfc          = (HFC)(i + 0x100); /* initialize table entries */
   contexts[i].instance     = instance;
   contexts[i].transformed  = FALSE;            /* no scaling/rotation assumed */
   contexts[i].file         = file;
   contexts[i].faceIndex    = faceIndex;

if( fUseFakeBold )
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
   /* for DBCS fonts/collections, odd indices are vertical versions*/
   if ((file->flags & FL_FLAG_DBCS_FILE) && (ulFont & 1))
      contexts[i].vertical  = TRUE;
   else
      contexts[i].vertical  = FALSE;

   file->flags |= FL_FLAG_CONTEXT_OPEN;         /* flag as in-use */

   COPY("-> hfc "); CATI((int) contexts[i].hfc); CAT("\r\n"); WRITE;

   TT_Flush_Face(file->faces[faceIndex].face);
   return contexts[i].hfc; /* everything OK */

Fail:
   TT_Flush_Face(file->faces[faceIndex].face);
   return (HFC)-1;
}

/****************************************************************************/
/*                                                                          */
/* SetFontContext :                                                         */
/*                                                                          */
/*  set font context parameters                                             */
/*                                                                          */
LONG _System SetFontContext( HFC           hfc,
                             PCONTEXTINFO  pci )
{
   LONG       ptsize, temp, emsize;
   PFontSize  size;

   COPY("SetFontContext: hfc = ");           CATI((int) hfc);
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

   emsize = size->file->faces[size->faceIndex].em_size;

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

         error = TT_Set_Instance_CharSizes(size->instance, ptsizex, ptsizey);
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
      error = TT_Set_Instance_CharSize(size->instance, emsize);
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

   error = TT_Set_Instance_CharSize(size->instance, ptsize);
   if (error)
      ERRRET(-1)  /* engine problem */

   return 0;      /* pretend everything is OK */
}

/****************************************************************************/
/*                                                                          */
/* CloseFontContext :                                                       */
/*                                                                          */
/*  destroy a font context                                                  */
/*                                                                          */
LONG _System CloseFontContext( HFC hfc)
{
   PFontSize  size;

   COPY("CloseFontContext: hfc = "); CATI((int)hfc); CAT("\r\n"); WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   /* mark table entry as free */
   size->hfc = 0;

   /* !!!!! set flag in TFontFile structure */
   size->file->flags &= ~FL_FLAG_CONTEXT_OPEN;   /* reset the in-use flag */

   if (size->file->flags & FL_FLAG_LIVE_FACE) {
      COPY("Closing instance: "); CATI((int)(size->instance.z)); CAT("\r\n"); WRITE;
      error = TT_Done_Instance(size->instance);
      if (error)
         ERRRET(-1)  /* engine error */
   }

   COPY("CloseFontContext successful\r\n"); WRITE;

   return 0; /* success */
}

/* TOODO: review for Aurora */
#define MAX_KERN_INDEX  504

GLYPH ReverseTranslate(PFontFace face, USHORT index) {
   ULONG  i;
   GLYPH  newidx = 0;

   /* TODO: enable larger fonts */
   for (i = 0; i < MAX_KERN_INDEX; i++) {
      COPY("PM2TT Called by 'ReverseTranslate'\r\n"); WRITE;
      newidx = PM2TT(face->charMap,
                     face->charMode,
                     i);
      if (newidx == index)
         break;
   }
   if (i < MAX_KERN_INDEX)
      return i;
   else
      return 0;
}

/****************************************************************************/
/*                                                                          */
/* QueryFaceAttr                                                            */
/*                                                                          */
/*  Return various info about font face                                     */
/*                                                                          */
LONG _System QueryFaceAttr( HFC     hfc,
                            ULONG   iQuery,
                            PBYTE   pBuffer,
                            ULONG   cb,
                            PGLYPH  pagi,
                            GLYPH   giStart )
{
   int                        count, i = 0;
   PFontSize                  size;
   PFontFace                  face;
   static TT_Face_Properties  properties;
   TT_OS2                     *pOS2;
   ABC_TRIPLETS*              pt;

   COPY("QueryFaceAttr: hfc = "); CATI((int) hfc); CAT("\r\n"); WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   face = &(size->file->faces[size->faceIndex]);

   if (iQuery == FD_QUERY_KERNINGPAIRS)
   {
      TT_Kern_0        kerntab;   /* actual kerning table */
      ULONG            used = 0;  /* # bytes used in output buffer */
      FD_KERNINGPAIRS  *kpair;
      USHORT           *kernIndices, idx;

      count = cb / sizeof(FD_KERNINGPAIRS);

      COPY("QUERY_KERNINGPAIRS, "); CATI((int) count);
      CAT("\r\n"); WRITE;
#if 1

      if (face->directory.tables == NULL)
         return 0;  /* no kerning info provided */
      /* !!!! could use better error checking */
      /* Only format 0 is supported (which is what M$ recommends) */
      if (face->directory.tables[0].format != 0) /* need only format 0 */
         ERRRET(-1);

      error = TT_Load_Kerning_Table(face->face, 0);
      if (error)
        ERET1( Fail );

      kerntab = face->directory.tables[0].t.kern0;
      kpair = (PVOID)pBuffer;

      if (face->kernIndices == NULL) {
         TT_Get_Face_Properties( face->face, &properties );
         error = ALLOC(face->kernIndices,
                       properties.num_Glyphs * sizeof (USHORT));
         if (error)
            ERET1( Fail );

         /* fill all entries with -1s */
         memset(face->kernIndices, 0xFF,
            properties.num_Glyphs * sizeof (USHORT));
      }

      kernIndices = face->kernIndices;

      while ((i < kerntab.nPairs) && (i < count))
      {
         idx = kerntab.pairs[i].left;
         if (kernIndices[idx] == (USHORT)-1)
            kernIndices[idx] = ReverseTranslate(face, idx);
         kpair->giFirst  = kernIndices[idx];
         idx = kerntab.pairs[i].right;
         if (kernIndices[idx] == (USHORT)-1)
            kernIndices[idx] = ReverseTranslate(face, idx);
         kpair->giSecond = kernIndices[idx];
         kpair->eKerningAmount = kerntab.pairs[i].value;
         kpair++;
         i++;
      }

      COPY("Returned kerning pairs: "); CATI(i); CAT("\r\n"); WRITE;
      return i; /* # items filled */
#else
      return 0;  /* no kerning support */

#endif
   }

   if (iQuery == FD_QUERY_ABC_WIDTHS)
   {
      count = cb / sizeof(ABC_TRIPLETS);

      COPY("QUERY_ABC_WIDTHS, "); CATI((int) count);
      CAT(" items, giStart = ");  CATI((int) giStart);
      if (pBuffer == NULL)
         CAT(" NULL buffer");
      CAT("\r\n"); WRITE;

      /* This call never fails - no error check needed */
      TT_Get_Face_Properties( face->face, &properties );

      pt = (ABC_TRIPLETS*)pBuffer;
      for (i = giStart; i < giStart + count; i++, pt++)
      {
         int                    index;
         unsigned short         wid;
         static unsigned short  adv_widths [2];
         static unsigned short  adv_heights[2];

         static unsigned short  widths[2], heights[2];
         static short           lefts [2], tops   [2];

         COPY("PM2TT Called by 'QueryFaceAttr'\r\n"); WRITE;

         index = PM2TT( face->charMap,
                        face->charMode,
                        i );

         /* get advances and bearings */
         if (size->vertical && properties.vertical && 0) /* TODO: enable */
           error = TT_Get_Face_Metrics( face->face, index, index,
                                        lefts, adv_widths, tops, adv_heights );
         else
           error = TT_Get_Face_Metrics( face->face, index, index,
                                        lefts, adv_widths, NULL, NULL );

         if (error)
           goto Broken_Glyph;

         /* skip complicated calculations for fixed fonts */
         if (face->flags & FC_FLAG_FIXED_WIDTH ) {
            wid = adv_widths[0] - lefts[0];
         }
         else {   /* proportianal font, it gets trickier */
            /* store glyph widths for DBCS fonts
               - needed for reasonable performance */
            if (face->flags & FC_FLAG_DBCS_FACE) {
               if (face->widths == NULL) {
                  error = ALLOC(face->widths,
                                properties.num_Glyphs * sizeof (USHORT));
                  if (error)
                     goto Broken_Glyph;   /* this error really shouldn't happen */

                  /* tag all entries as unused */
                  memset(face->widths, 0xFF,
                         properties.num_Glyphs * sizeof (USHORT));
               }
               if (face->widths[index] == 0xFFFF) { /* get from file if needed */
                  error = TT_Get_Face_Widths( face->face, index, index,
                                              widths, heights );
                  if (error)
                     goto Broken_Glyph;
                  /* save for later */
                  face->widths[index] = widths[0];
               }
               wid = face->widths[index];
            }
            /* 'small' font, no need to remember widths, OS/2 takes care of it */
            else {
               /* get width or height - use ftxwidth.c */
               error = TT_Get_Face_Widths( face->face, index, index,
                                           widths, heights );
               if (error)
                  goto Broken_Glyph;
               wid = widths[0];
            }
         }

      #if 1
         if ( fUseFakeBold && size->fakebold)
         {
            TT_Pos addwid;

            addwid = getFakeBoldMetrics( 0, heights[ 0 ], NULL );
            adv_widths[ 0 ] += addwid;
            wid += addwid;
         }
      #endif

         if (size->vertical && !is_HALFCHAR(i))
         {
            if (properties.vertical && 0) /* TODO: enable */
            {
              pt->lA = tops[0];
              pt->ulB = heights[0];
              pt->lC = adv_heights[0] - pt->lA - pt->ulB;
            }
            else
            {
              pt->lA  = pt->lC = 0;
              pt->ulB = properties.os2->usWinAscent +
                        properties.os2->usWinDescent;
            }

         }
         else
         {
           pt->lA = lefts[ 0 ];
           pt->ulB = wid;
           pt->lC = adv_widths[0] - pt->lA - pt->ulB;
         }

         if ( fNetscapeFix && face->charMode != TRANSLATE_SYMBOL &&
             !size->vertical) {
            if (face->flags & FC_FLAG_FIXED_WIDTH) {
               pt->ulB = pt->ulB + pt->lA + pt->lC;
               pt->lA  = 0;
               /* for better look with sbit fonts */
               pt->lC  = (is_HALFCHAR(i) ? 1 : 2) * 64;
            } else if (i == 32) {
               /* return nonzero B width for 'space' */
               pt->ulB = adv_widths[0] - 2 * lefts[0];
               pt->lC  = lefts[0];
            }
         }

         COPY("ABC info : index = "); CATI( index );
         CAT("\r\n");
         WRITE;

         COPY(" adv_width = "); CATI( adv_widths[ 0 ] );
         CAT(" lefts = "); CATI( lefts[ 0 ]);
         CAT(" width = "); CATI( wid );
         CAT("\r\n");
         WRITE;

         COPY(" A + B + C = "); CATI( pt->lA + pt->ulB + pt->lC );
         CAT("     A = "); CATI( pt->lA );
         CAT("     B = "); CATI( pt->ulB );
         CAT(" C = "); CATI( pt->lC );
         CAT("\r\n");
         WRITE;

         continue;

     Broken_Glyph:  /* handle broken glyphs gracefully */
           pt->lA = pt->lC = 0;

           if (size->vertical && !is_HALFCHAR(i))
             pt->ulB = properties.os2->usWinAscent +
                      properties.os2->usWinDescent;
           else
             pt->ulB = properties.horizontal->xMax_Extent;

      }
   }

   TT_Flush_Face(face->face);
   return count; /* number of entries filled in */

Fail:
   TT_Flush_Face(face->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* QueryCharAttr :                                                          */
/*                                                                          */
/*  Return glyph attributes, basically glyph's bit-map or outline           */
/*  some variables are declared static to conserve stack space.             */
/*                                                                          */
LONG _System QueryCharAttr( HFC             hfc,
                            PCHARATTR       pCharAttr,
                            PBITMAPMETRICS  pbmm )
{
   static   TT_Raster_Map       bitmap;
   static   TT_Outline          outline;
   static   TT_BBox             bbox;
   static   TT_SBit_Image       image;
   static   TT_Face_Properties  properties;

   PFontSize  size;
   PFontFace  face;
   LONG       mapsize, temp;
   PBYTE      pb;
   int        gindex, width, rows, cols, i, j;
   ULONG      cb;
   TT_Error   sbiterror;
   BOOL       sbitok;
   TT_Pos     ExtentX, ExtentY, addedwidth;

   static   TT_Raster_Map  fbbmm;

   #ifdef  DEBUG
   int        row, col;
   char *     pBitmap;
   #endif

   COPY( "QueryCharAttr: \n" ); WRITE;

   size = getFontSize(hfc);
   if (!size)
      ERRRET(-1) /* error, invalid context handle */

   face = &(size->file->faces[size->faceIndex]);

   COPY("PM2TT Called by 'QueryCharAttr'\r\n"); WRITE;
   gindex = PM2TT( face->charMap, face->charMode, pCharAttr->gi);

   error = TT_Load_Glyph( size->instance,
                          face->glyph,
                          gindex,
                          TTLOAD_DEFAULT);

   if (error)
   {
      if (i == 0)
         ERET1( Fail )  /* this font's no good, return error */
      else
      { /* try to recover quietly */
         error = TT_Load_Glyph( size->instance,
                                face->glyph,
                                0,
                                TTLOAD_DEFAULT);
         if (error) {
            COPY("  TT_Load_Glyph failed!!  Error code is "); CATI(error);
            CAT("\n"); WRITE;
            ERET1( Fail );
         }
      }
   }

   sbiterror = TT_Load_Glyph_Bitmap( face->face,
                                     size->instance,
                                     gindex,
                                     &image);
   if (sbiterror) {
      COPY("  TT_Load_Glyph_Bitmap Failed!!  Error code is " ); CATI(sbiterror);
      CAT("\n"); WRITE;
   } else {
      COPY("  TT_Load_Glyph_Bitmap Succeeded!!\n"); WRITE;

      COPY("  image.metrics.bbox.xMin = "); CATI (image.metrics.bbox.xMin);
      CAT( "  image.metrics.bbox.xMax = "); CATI (image.metrics.bbox.xMax); CAT("\n");
      CAT( "  image.metrics.bbox.yMin = "); CATI (image.metrics.bbox.yMin);
      CAT( "  image.metrics.bbox.yMax = "); CATI (image.metrics.bbox.yMax); CAT( "\n");
      WRITE;
   }

   sbitok = !sbiterror && !( size->vertical && !is_HALFCHAR(pCharAttr->gi)) && !size->transformed;

   TT_Flush_Face( face->face );

   error = TT_Get_Glyph_Outline( face->glyph, &outline );
   if (error)
      ERRRET(-1);

   /* --- Vertical fonts handling----------------------------------- */

   if (size->vertical && !is_HALFCHAR(pCharAttr->gi)) {
      TT_Matrix    vertMatrix;
      TT_OS2       *pOS2;

      vertMatrix.xx =  0x00000;
      vertMatrix.xy = -0x10000;
      vertMatrix.yx =  0x10000;
      vertMatrix.yy =  0x00000;

      /* rotate outline 90 degrees counterclockwise */
      TT_Transform_Outline(&outline, &vertMatrix);

      TT_Get_Face_Properties( face->face, &properties );
      pOS2 = properties.os2;

      if (size->transformed) {
         /* move outline to the right to adjust for rotation */
         TT_Translate_Outline(&outline, pOS2->usWinAscent, 0);
         /* move outline down a bit */
         TT_Translate_Outline(&outline, 0, -(pOS2->usWinDescent));
      }
      else {
         /* move outline to the right to adjust for rotation */
         TT_Translate_Outline(&outline,
                              (pOS2->usWinAscent * size->scaleFactor) >> 10, 0);
         /* move outline down a bit */
         TT_Translate_Outline(&outline, 0,
                              -((pOS2->usWinDescent * size->scaleFactor) >> 10));
      }
   }

   if (size->transformed)
     TT_Transform_Outline( &outline, &size->matrix );

   /* --- Outline processing --------------------------------------- */

   if ( pCharAttr->iQuery & FD_QUERY_OUTLINE )
   {
     if (pCharAttr->cbLen == 0)      /* send required outline size in bytes */
       return GetOutlineLen( &outline );

     return GetOutline( &outline, pCharAttr->pBuffer );
   }

   /* --- Bitmap processing ---------------------------------------- */
   TT_Get_Outline_BBox( &outline, &bbox );

   /* the following seems to be necessary for rotated glyphs */
   if (size->transformed) {
      bbox.xMax = bbox.xMin = 0;
      for (i = 0; i < outline.n_points; i++) {
         if (bbox.xMin  > outline.points[i].x)
            bbox.xMin = outline.points[i].x;
         if (bbox.xMax  < outline.points[i].x)
            bbox.xMax = outline.points[i].x;
      }
   }
   /* grid-fit the bbox */
   bbox.xMin &= -64;
   bbox.yMin &= -64;

   bbox.xMax = (bbox.xMax+63) & -64;
   bbox.yMax = (bbox.yMax+63) & -64;

   COPY("  bbox.xMin = "); CATI(bbox.xMin);
   CAT( "  bbox.xMax = "); CATI(bbox.xMax); CAT("\n");
   CAT( "  bbox.yMin = "); CATI(bbox.yMin);
   CAT( "  bbox.yMax = "); CATI(bbox.yMax);
   CAT( "\n"); WRITE;

   if (pCharAttr->iQuery & FD_QUERY_BITMAPMETRICS)
   {
      /* fill in bitmap metrics */
      /* metrics values are in 26.6 format ! */

      if ( sbitok ) {
         short lefts[ 2 ];

         ExtentX = image.metrics.bbox.xMax - image.metrics.bbox.xMin;
         ExtentY = image.metrics.bbox.yMax - image.metrics.bbox.yMin;

         TT_Get_Face_Metrics( face->face, gindex, gindex,
                              lefts, NULL, NULL, NULL );

         pbmm->pfxOrigin.x   = lefts[ 0 ] * size->scaleFactor;
         pbmm->pfxOrigin.y   = image.metrics.bbox.yMax << 10;
      } else {
         ExtentX = bbox.xMax - bbox.xMin;
         ExtentY = bbox.yMax - bbox.yMin;
         pbmm->pfxOrigin.x   = bbox.xMin << 10;
         pbmm->pfxOrigin.y   = bbox.yMax << 10;
      }

    if( fUseFakeBold )
    {
      addedwidth = (size->fakebold) ? getFakeBoldMetrics(ExtentX, ExtentY, &fbbmm) : 0;

      pbmm->sizlExtent.cx = (ExtentX + addedwidth) >> 6;
    }
    else
      pbmm->sizlExtent.cx = ExtentX >> 6;

      pbmm->sizlExtent.cy = ExtentY >> 6;
      pbmm->cyAscent      = 0;

      if (!(pCharAttr->iQuery & FD_QUERY_CHARIMAGE))
         return sizeof(*pbmm);
   }

   /* --- actual bitmap processing here --- */
   if (pCharAttr->iQuery & FD_QUERY_CHARIMAGE)
   {
      /* values in 26.6 format ?!? */
      bitmap.width  = (bbox.xMax - bbox.xMin) >> 6;
      bitmap.rows   = (bbox.yMax - bbox.yMin) >> 6;
      /* width rounded up to nearest multiple of 4 */
      bitmap.cols   = ((bitmap.width + 31) / 8) & -4;
      bitmap.flow   = TT_Flow_Down;
      bitmap.bitmap = pCharAttr->pBuffer;
      bitmap.size   = bitmap.rows * bitmap.cols;

      COPY ( "  pCharAttr->cbLen = " ); CATI( pCharAttr->cbLen ); CAT( "\n" ); WRITE;

      mapsize = (sbitok) ? image.map.size : bitmap.size;
      if ( fUseFakeBold && size->fakebold )
         mapsize = fbbmm.size;

      if (pCharAttr->cbLen == 0)
         return  mapsize;

      if (mapsize > pCharAttr->cbLen)
         ERRRET(-1)     /* otherwise we might overwrite something */

      /* clean provided buffer (unfortunately necessary) */
      memset(pCharAttr->pBuffer, 0, pCharAttr->cbLen);

      if ( sbitok ) {
         if ( !(memcpy( pCharAttr->pBuffer, image.map.bitmap, image.map.size )) )
            ERRRET(-1);

         COPY("  image.map.rows = " );  CATI( image.map.rows );
         CAT( "  image.map.cols = " );  CATI( image.map.cols ); CAT( "\n" );
         CAT( "  image.map.width = " ); CATI( image.map.width );
         CAT( "  image.map.size = " );  CATI( image.map.size );
         CAT( "\n" ); WRITE;

         COPY( "  SBit bitmap copied successfully!!\n" ); WRITE;

            width = image.map.width;
            rows  = image.map.rows;
            cols  = image.map.cols;
      } else {
         error = TT_Get_Glyph_Bitmap( face->glyph,
                                      &bitmap,
                                      -bbox.xMin,
                                      -bbox.yMin );
         if (error)
            ERRRET(-1); /* engine error */

         COPY("  bitmap.rows = " );  CATI( bitmap.rows );
         CAT( "  bitmap.cols = " );  CATI( bitmap.cols ); CAT( "\n" );
         CAT( "  bitmap.width = " ); CATI( bitmap.width );
         CAT( "  bitmap.size = " );  CATI( bitmap.size );
         CAT( "\n" ); WRITE;

         COPY( "  Glyph bitmap loaded successfully!!\n" ); WRITE;

            width = bitmap.width;
            rows  = bitmap.rows;
            cols  = bitmap.cols;
      }

      if ( fUseFakeBold && size->fakebold ) {
         error = buildFakeBoldBitmap( pCharAttr->pBuffer,
                                      pCharAttr->cbLen,
                                      width,
                                      addedwidth >> 6,
                                      rows, cols );

         if (error)
            ERRRET(-1); /* engine error */

            rows = fbbmm.rows;
            cols = fbbmm.cols;
      }

      #ifdef  DEBUG   /* print Character Image */
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

      return mapsize; /* return # of bytes */
   }
   ERRRET(-1) /* error */

Fail:
   TT_Flush_Face(face->face);
   return -1;
}

/****************************************************************************/
/*                                                                          */
/* QueryFullFaces :                                                         */
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
LONG _System QueryFullFaces( HFF     hff,          // font file handle
                             PVOID   pBuffer,      // buffer for output
                             PULONG  cBufLen,      // buffer length
                             PULONG  cFontCount,   // number of fonts
                             ULONG   cStart )      // starting index
{
#if 0
   COPY("!QueryFullFaces: hff = "); CATI((int) hff); CAT("\r\n"); WRITE;
   ERRRET(-1) /* error ? */
#else

   static TT_Face_Properties   properties;
          PFontFace            pface;
          PFontFile            file;
          PBYTE                pBuf;
          PFFDESCS2            pffd;
          LONG                 lCount, lTotal, faceIndex,
                               cbFamily, cbFace, lOff,
                               cbEntry, cbTotal  = 0,
                               ifiCount = 0;
          char                 *name1, *name2,
                               *pszFaceName;
   static char                 *wrongName = "Wrong Name Font";

   COPY( "QueryFullFaces: hff = " ); CATI( hff );
   CAT(  ", cFontCount = " );    CATI( *cFontCount );
   CAT(  ", cStart = " );        CATI( cStart );
   CAT(  ", cBufLen = " );       CATI( *cBufLen );
   CAT( "\r\n");
   WRITE;

   file = getFontFile(hff);
   if (!file)
      ERRRET(-1) // error, invalid handle

   if ( cStart > ( file->numFaces - 1 ))
      return 0;

   pBuf = pBuffer;
   pffd = (PFFDESCS2) pBuf;
   lCount = 0;     // # of font names being returned
   lTotal = 0;     // total # of font names in the file
   for (faceIndex = 0; faceIndex < file->numFaces; faceIndex++)
   {
      cbEntry = 0;  // size of the current buffer item

      // get pointer to this face's data
      pface = &(file->faces[faceIndex]);

      TT_Get_Face_Properties( pface->face, &properties );

      // get font name and check it's really found
      // 1. family name
      name1 = LookupName(pface->face, TT_NAME_ID_FONT_FAMILY);
      if (name1 == NULL)
         name1 = wrongName;
      cbFamily = strlen( name1 ) + 1;
      lOff = ( cbFamily + 3 ) & ( -4 );     // round up to nearest 4 bytes

      // 2. face name
      name2 = LookupName(pface->face, TT_NAME_ID_FULL_NAME);
      if (name2 == NULL)
         name2 = wrongName;
      cbFace = strlen( name2 ) + 1;

      cbEntry = sizeof( FFDESCS2 ) + lOff + cbFace;
      cbEntry = ( cbEntry + 3 ) & ( -4 );   // round up to nearest 4 bytes

      COPY( "QueryFullFaces: hff = " );  CATI( hff );
      CAT( " , FamilyName = " );         CAT( name1 );
      CAT( " , FullName = " );           CAT( name2 );
      CAT( " , Face # = " );             CATI( lTotal );
      CAT( " , Item # = " );             CATI( lCount );
      CAT( " , Item size = " );          CATI( cbEntry );
      CAT( " , Total size = " );         CATI( cbTotal );
      CAT( "\r\n");
      WRITE;

      if ( pffd && ( *cBufLen > 0 ) && ( lTotal >= cStart ) && ( lCount < *cFontCount )) {
          if (( cbTotal + cbEntry ) > *cBufLen ) goto Fail;
          pffd->cbLength = cbEntry;
          pszFaceName = pffd->abFamilyName + lOff;
          pffd->cbFacenameOffset = (ULONG) pszFaceName - (ULONG) pffd;
          strcpy( pffd->abFamilyName, name1 );
          pffd->abFamilyName[ cbFamily-1 ] = '\0';
          strcpy( pszFaceName, name2 );

          COPY( "QueryFullFaces: hff = " );  CATI( hff );
          CAT( " , FamilyName = " );         CAT( pffd->abFamilyName );
          CAT( " , FullName = " );           CAT( pszFaceName );
          CAT( "\r\n");
          WRITE;

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
              if (( cbTotal + cbEntry ) > *cBufLen ) goto Fail;
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
          if( fUseFakeBold )
          {
              cbFamily--;                   // remove what we added for '@' above
              lOff = ( cbFamily + 3 ) & ( -4 );     // round up to nearest 4 bytes
              cbFace   = strlen( name2 ) + 6;
              cbEntry = sizeof( FFDESCS2 ) + lOff + cbFace;
              cbEntry = ( cbEntry + 3 ) & ( -4 );   // round up to nearest 4 bytes

              if ( pffd && ( *cBufLen > 0 ) && ( lTotal >= cStart ) && ( lCount < *cFontCount )) {
                  if (( cbTotal + cbEntry ) > *cBufLen ) goto Fail;
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

      else if ( fUseFacenameAlias )
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
                  if (( cbTotal + cbEntry ) > *cBufLen ) goto Fail;
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

Exit:
   *cFontCount = lCount;
   TT_Flush_Face(pface->face);
   return ( lTotal - lCount );

Fail:
   *cFontCount = lCount;
   TT_Flush_Face(pface->face);
   return GPI_ALTERROR;

#endif

}

/*---------------------------------------------------------------------------*/
/* end of exported functions */
/*---------------------------------------------------------------------------*/


/****************************************************************************/
/* The (now rather inaccurately-named) LimitsInit function reads OS2.INI to */
/* get various settings used by the IFI driver.                             */
/*                                                                          */
static  void LimitsInit(void) {
   char *c;
   unsigned long cb;

   /* Get the max-open-files (font cache) limit */
   max_open_files = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                        "OpenFaces", 12 );
   if( max_open_files == 0 )
      max_open_files = 12;   /* reasonable default */

   if (max_open_files < 8)   /* ensure limit isn't too low */
      max_open_files = 8;

   /* Get various boolean settings which the user can modify */
   fNetscapeFix = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                      "Use_Netscape_Fix", FALSE );

   fUseFacenameAlias = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                           "Use_Facename_Alias", FALSE );

   fUseFakeBold = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                      "Use_Fake_Bold", FALSE );

   fUseUnicodeEncoding = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                             "Use_Unicode_Encoding", TRUE );

   fForceUnicodeMBCS = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                           "Set_Unicode_MBCS", TRUE );

   fExceptAssocFont = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                           "Force_DBCS_Association", TRUE );

   fExceptCombined = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                         "Force_DBCS_Combined", TRUE );

   fStyleFix = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                   "Style_Fixup", FALSE );

   /* Get the instance DPI */
   instance_dpi = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                      "DPI", 72 );
   if (( instance_dpi != 72 ) && ( instance_dpi != 96 ) && ( instance_dpi != 120 ))
      instance_dpi = 72;

   /* Get the nominal font size (0 means use built-in default of 12) */
   lNominalPtSize = PrfQueryProfileInt( HINI_USERPROFILE, "FreeType/2",
                                        "Nominal_Size", 12 );
   if (( lNominalPtSize < 6 ) || ( lNominalPtSize > 64 ))
       lNominalPtSize = 12;

   /* See if there's a DBCS association font in use */
   PrfQueryProfileString(HINI_USERPROFILE, "PM_SystemFonts", "PM_AssociateFont",
                         NULL, szAssocFont, sizeof(szAssocFont));
   if ((c = mystrrchr(szAssocFont, ';')) != NULL ) *c = '\0';

   /* See if there are any DBCS "combined font" aliases defined */
   cb = 0;
   if ( PrfQueryProfileSize(HINI_USERPROFILE, "PM_ABRFiles", NULL, &cb ) &&
        ! ALLOC( pszCmbAliases, ++cb ))
   {
      if ( ! PrfQueryProfileData(HINI_USERPROFILE, "PM_ABRFiles",
                                 NULL, pszCmbAliases, &cb ))
          FREE( pszCmbAliases );
   }
}


/****************************************************************************/
/* my_itoa is used only in the following function GetUdcInfo.               */
/* Works pretty much like expected.                                         */
/*                                                                          */
void    my_itoa(int num, char *cp) {
    char    temp[10];
    int     i = 0;

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
/* GetUdcInfo determines the UDC ranges used                                */
/*                                                                          */
VOID    GetUdcInfo(VOID) {
    ULONG   i;
#if 0
    ULONG   ulUdc, ulUdcInfo, i;
    PVOID   gPtr;
    HINI    hini;
    CHAR    szCpStr[10] = "CP";
    CHAR    szNlsFmProfile[] = " :\\OS2\\SYSDATA\\PMNLSFM.INI";
    CHAR    szUdcApp[] = "UserDefinedArea";
    typedef struct _USUDCRANGE {
        USHORT  usStart;
        USHORT  usEnd;
    } USUDCRANGE;
    USUDCRANGE *pUdcTemp;
#endif

    DosQueryCp(sizeof(ulCp), (ULONG*)&ulCp, &i);  /* find out default codepage */

#if 0
    my_itoa((INT) ulCp, szCpStr + 2);             /* convert to ASCII          */

    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &i, sizeof(ULONG));
    szNlsFmProfile[0] = 'A' + i - 1;

    if ((hini = PrfOpenProfile((HAB) 0L, szNlsFmProfile)) != NULLHANDLE) {
        if (PrfQueryProfileSize(hini, szUdcApp, szCpStr, &ulUdc) == TRUE) {

            pUdcTemp = (USUDCRANGE *) malloc(ulUdc);
            if (pUdcTemp && PrfQueryProfileData(hini, szUdcApp, szCpStr,
                                                pUdcTemp, &ulUdc) == TRUE) {

            }
            if (pUdcTemp) free(pUdcTemp);
        }
    }
#endif
}

/****************************************************************************/
/* LangInit determines language used at DLL startup, non-zero return value  */
/* means error.                                                             */
/* This code is crucial, because it determines behaviour of the font driver */
/* with regard to language encodings it will use.                           */
static  ULONG LangInit(void) {
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

      /* on Aurora there's no need for special handling */
      #ifndef AURORA
      case 30:            /* Greece  - for Alex! */
         iLangId = TT_MS_LANGID_GREEK_GREECE;
      #endif

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
ULONG  FirstInit(void) {
   LONG   lReqCount;
   ULONG  ulCurMaxFH;

   #ifdef DEBUG
      ULONG Action;
   #endif /* DEBUG */
   #ifdef DEBUG
      DosOpen("\\FTIFI.LOG", &LogHandle, &Action, 0, FILE_NORMAL,
              OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
              OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_WRITE_THROUGH |
              OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY,
              NULL);
      COPY("FreeType/2 loaded.\r\n");
      WRITE;
   #endif /* DEBUG */

   /* increase # of file handles by five to be on the safe side */
   lReqCount = 5;
   DosSetRelMaxFH(&lReqCount, &ulCurMaxFH);
   error = TT_Init_FreeType(&engine);   /* turn on the FT engine */
   if (error)
      return 0;     /* exit immediately */
   error = TT_Init_Kerning_Extension(engine); /* load kerning support */
   error = TT_Init_SBit_Extension(engine); /* load embedded bitmaps support */
   if (error)
      COPY("SBit Support Init Failed !!!\r\n");
   COPY("FreeType Init called\r\n");
   WRITE;

   if (LangInit())      /* initialize NLS */
      return 0;         /* exit on error  */
   COPY("NLS initialized.\r\n");
   WRITE;

   LimitsInit();  /* initialize max_open_files */
   COPY("Open faces limit set to "); CATI(max_open_files); CAT("\r\n");
   WRITE;

   if (error)
      return 0;     /* exit immediately */
   COPY("Initialization successful.\r\n");
   WRITE;
   return 1;
}


/****************************************************************************/
/*                                                                          */
/* FinalTerm :                                                              */
/*                                                                          */
/*  Called when font driver is unloaded for the last time time. Performs    */
/* final clean-up, shuts down engine etc.                                   */
ULONG  FinalTerm(void) {
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
   TT_Done_FreeType(engine);

   #ifdef DEBUG
      COPY("FreeType/2 terminated.\r\n");
      WRITE;
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
ULONG _System _DLL_InitTerm(ULONG hModule, ULONG ulFlag) {
   switch (ulFlag) {
      case 0:             /* initializing */
         if (++ulProcessCount == 1)
            return FirstInit();  /* loaded for the first time */
         else
            return 1;

      case 1:  {          /* terminating */
         int   i;
         /* clean UCONV cache */
         #ifdef USE_UCONV
            CleanUCONVCache();
         #endif
         if(--ulProcessCount == 0)
            return FinalTerm();
         else
            return 1;
      }
   }
   return 0;
}

/****************************************************************************/
/*                                                                          */
/* interfaceSEId (Interface-specific Encoding Id) determines what encoding  */
/* the font driver should use if a font includes a Unicode encoding.        */
/* Note: the 'encoding' parameter is the default to fall back to (usually   */
/* Unicode) if nothing better can be found.                                 */
/*                                                                          */
LONG    interfaceSEId(TT_Face face, BOOL UDCflag, LONG encoding) {
    ULONG   range1 = 0;
    ULONG   bits = 0,
            mask;
    TT_OS2  *pOS2;
    static  TT_Face_Properties  props;

    TT_Get_Face_Properties(face, &props);
    pOS2 = props.os2;

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
/* some Japanese fonts included with OS/2 which commit just that crime.     */
/*                                                                          */
/*   This check is far from foolproof, it's just a quick-and-dirty sanity   */
/* check.  It's mainly only effective for Japanese strings, and even then   */
/* only if they contain fullwidth-halfwidth forms, or plain ASCII.)         */
/*                                                                          */
/*   Returns FALSE if the name is determined not to be DBCS-encoded.        */

static BOOL CheckDBCSEnc( PCH name, int len )
{
    int i;
    for ( i = 0; i < len; i++ ) {
        if ( name[i] == 0xFF ) // || (i < len-1 && name[i] == 0 ))
            return FALSE;
    }
    return TRUE;
}


/****************************************************************************/
/*                                                                          */
/* ReallyUnicodeName:                                                       */
/*                                                                          */
/*   Make sure a font's name and cmap are really Unicode (and not an OS/2   */
/* codepage/glyphlist mis-identifying itself as Unicode).  This is mainly a */
/* workaround for some badly-behaved system fonts bundled with Korean OS/2. */
/*                                                                          */
/*   Since there's no real way to make this determination heuristically, we */
/* just compare the font's name against a blacklist of known offenders.     */
/*                                                                          */
/* Returns FALSE if the name appears on the blacklist (and should therefore */
/* be treated as a codepage, not Unicode, string).                          */

PSZ apszBrokenFontNames[] = {
    "\xB1\xBD\xC0\xBA\xB0\xED\xB5\xF1",     /* BGOTHICH.TTF - really PMKOR */
    "\xB1\xBD\xC0\xBA\xB8\xED\xC1\xB6",     /* BMYGJO.TTF   - really PMKOR */
    "\xB0\xED\xB5\xF1",                     /* GOTHICH.TTF  - really PMKOR */
    "\xB8\xED\xC1\xB6",                     /* MYGJO.TTF    - really PMKOR */
    "\xB5\xD5\xB1\xD9\xB0\xED\xB5\xF1",     /* RGOTHICH.TTF - really PMKOR */
};

#define NUM_BROKEN_NAMES   5

static BOOL ReallyUnicodeName( PCH name, int len )
{
    int i;
    for ( i = 0; i < NUM_BROKEN_NAMES; i++ ) {
        if ( len && !strcmp( (PSZ) name, apszBrokenFontNames[ i ] ))
            return FALSE;
    }
    return TRUE;
}


/****************************************************************************/
/*                                                                          */
/* LookupName :                                                             */
/*                                                                          */
/*   Look for a TrueType name by index, prefer current language             */
/*                                                                          */

/* Hopefully enough to handle "long" fontnames... not really ideal but it's
 * faster than alloc-ing everything.  This allows us to both return the
 * actual fontname in QueryFullNames, and know when the name has been
 * truncated in IFIMETRICS/FONTMETRICS.  The caller will truncate this name
 * to FACESIZE where needed.
 */
#define LONGFACESIZE 128

static  char*  LookupName(TT_Face face,  int index )
{
   static char name_buffer[LONGFACESIZE + 2];
   int    name_len = 0;

   int i, j, n;

   USHORT platform, encoding, language, id;
   char*  string;
   USHORT string_len;

   int    found;
   int    best;

#ifdef USE_UCONV
   static UniChar     *cpNameWansung = L"IBM-949";
   static UniChar     *cpNameBig5    = L"IBM-950";
   static UniChar     *cpNameSJIS    = L"IBM-943";
   static UniChar     *cpNameGB2312  = L"IBM-1381";
   static UniChar     *cpNameSystem  = L"";

   UconvObject uconvObject = NULL;
   UniChar    *cpName = NULL;
   ULONG       uconvType;
#endif

   n = TT_Get_Name_Count( face );
   if ( n < 0 )
      return NULL;

#ifdef USE_UCONV
   found = -1;
   best  = -1;

   /* Pass 1: Search for a Unicode-encoded name (Microsoft platform).
    * If a CJK language name that matches the system codepage is found, select
    * it.  If not, but US-English is found, select that.  Otherwise, fall back
    * to the first encoding found.
    */
   for ( i = 0; found == -1 && i < n; i++ )
   {
      TT_Get_Name_ID( face, i, &platform, &encoding, &language, &id );

      if ( id == index )
      {
         /* Try to find an appropriate language */
         if ( platform == TT_PLATFORM_MICROSOFT && encoding == TT_MS_ID_UNICODE_CS )
         {
            if (best == -1 || language == TT_MS_LANGID_ENGLISH_UNITED_STATES)
               best = i;

            switch( language )
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

   if (found == -1)
      found = best;

   /* Now convert the Unicode name to the appropriate codepage: for CJK
    * languages, use the predetermined DBCS codepage; for all other
    * languages, use the active process codepage.
    */
   if (found != -1)
   {
      TT_Get_Name_ID( face, found, &platform, &encoding, &language, &id );
      TT_Get_Name_String( face, found, &string, &string_len );

      if ( !ReallyUnicodeName( string, string_len )) {
          /* Oops, this isn't really a Unicode string.  Not much we can do
           * in this case except interpret it using the current codepage.
           */
          for (i=0, j=0; ( i<string_len ) && ( j < LONGFACESIZE - 1 ); i++)
             if (string[i] != '\0')
                name_buffer[j++] = string[i];
          name_buffer[j] = '\0';
          RemoveLastDBCSLead( name_buffer );
          return name_buffer;
      }

      switch( language )
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
      }

      if( getUconvObject( cpName, &uconvObject, uconvType ) == 0 )
      {
         char  uniName[( LONGFACESIZE + 1 ) * 2 ] = { 0, };
         int   len;
         ULONG rc;

         len = min( LONGFACESIZE * 2, string_len );

         for( j = 0; j * 2 < len; j++ )
         {
            uniName[ j * 2 ] = string[ j * 2 + 1 ];
            uniName[ j * 2 + 1 ] = string[ j * 2 ];
         }

         rc = UniStrFromUcs( uconvObject, name_buffer, ( UniChar * )uniName, sizeof( name_buffer ));
         if( rc == ULS_SUCCESS )
            return name_buffer;
      }
   }
#endif

   found = -1;
   best  = -1;

   /* Pass 2: Unicode strings not available!!! Try to find NLS strings.
    * Search for a name under other encodings (Microsoft platform).  If a DBCS
    * encoding that matches the system codepage is found, OR a symbol encoding
    * is found, select it and return a copy of the string (verbatim).
    */
   COPY("Unicode name not found!  Looking for native-language name.");
   CAT("\r\n"); WRITE;

   for ( i = 0; found == -1 && i < n; i++ )
   {
      TT_Get_Name_ID( face, i, &platform, &encoding, &language, &id );

      if ( id == index )
      {
         /* Try to find an appropriate encoding */
         if ( platform == TT_PLATFORM_MICROSOFT )
         {
            if (best == -1)
               best = i;

            switch (encoding)
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
      // We found a suitable encoding; now get the name and copy it
      TT_Get_Name_String( face, found, &string, &string_len );

#ifdef USE_UCONV
      /* Quick check to make sure it's not actually a mis-identified Unicode
       * string (workaround for RF Gothic etc.)
       */
      if ( !CheckDBCSEnc( string, string_len ))
      {
          TT_Get_Name_ID( face, found, &platform, &encoding, &language, &id );

          COPY(" --> \"");
          CAT( string );
          CAT("\" actually seems to be a Unicode name."); CAT("\r\n"); WRITE;

          switch ( language ) {
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
          }
          if( getUconvObject( cpName, &uconvObject, uconvType ) == 0 )
          {
             char  uniName[( LONGFACESIZE + 1 ) * 2 ] = { 0, };
             int   len;
             ULONG rc;

             len = min( LONGFACESIZE * 2, string_len );
             for( j = 0; j * 2 < len; j++ ) {
                uniName[ j * 2 ] = string[ j * 2 + 1 ];
                uniName[ j * 2 + 1 ] = string[ j * 2 ];
             }
             rc = UniStrFromUcs( uconvObject, name_buffer, ( UniChar * )uniName, sizeof( name_buffer ));
             if( rc == ULS_SUCCESS )
                return name_buffer;
          }
      }
#endif

      COPY(" --> Using name \"");
      CAT( string );
      CAT("\"\r\n"); WRITE;

      /* Some of these fonts put extra 0s between each byte value, so we
       * allow for that.
       */
      for (i=0, j=0; ( i<string_len ) && ( j < LONGFACESIZE - 1 ); i++)
         if (string[i] != '\0')
            name_buffer[j++] = string[i];
      name_buffer[j] = '\0';

      RemoveLastDBCSLead( name_buffer );

      return name_buffer;
   }

   COPY("No usable names found!"); CAT("\r\n");

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
static  ULONG GetCharmap(TT_Face face)
{
   int    n;      /* # of encodings (charmaps) available */
   USHORT platform, encoding;
   int    i, best, bestVal, val;

   n = TT_Get_CharMap_Count(face);

   if (n < 0)      /* no encodings at all; don't yet know what the best course of action would be */
      ERRRET(-1)   /* such font should probably be rejected */

   bestVal = 16;
   best    = -1;

   for (i = 0; i < n; i++)
   {
      TT_Get_CharMap_ID( face, i, &platform, &encoding );

      /* Windows Unicode is the highest encoding, return immediately */
      /* if we find it..                                             */
      if ( platform == TT_PLATFORM_MICROSOFT && encoding == TT_MS_ID_UNICODE_CS)
         return i;

      /* otherwise, compare it to the best encoding found */
      val = -1;
      switch (platform) {
         case TT_PLATFORM_MICROSOFT:
            switch (encoding) {
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
            if (encoding == TT_MAC_ID_ROMAN)
               val = 8;
            break;
      }

      if (val > 0 && val <= bestVal)
      {
         bestVal = val;
         best    = i;
      }
   }

   if (val < 0)
      return 0;           /* we didn't find any suitable encoding !! */

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
/*                                                                          */
/* GetOutlineLen :                                                          */
/*                                                                          */
/*   Used to compute the size of an outline once it is converted to         */
/*   OS/2's specific format. The translation is performed by the later      */
/*   function called simply "GetOutline".                                   */
/*                                                                          */
static  int GetOutlineLen(TT_Outline *ol)
{
   int    index;     /* current point's index */
   BOOL   on_curve;  /* current point's state */
   int    i, start = 0;
   int    first, last;
   ULONG  cb = 0;

   /* loop thru all contours in a glyph */
   for ( i = 0; i < ol->n_contours; i++ ) {

      cb += sizeof(POLYGONHEADER);

      first = start;
      last  = ol->contours[i];

      on_curve = (ol->flags[first] & 1);
      index    = first;

      /* process each contour point individually */
      while ( index < last ) {
         index++;

         if ( on_curve ) {
            /* the previous point was on the curve */
            on_curve = ( ol->flags[index] & 1 );
            if ( on_curve ) {
               /* two successive on points => emit segment */
               cb += sizeof(PRIMLINE);
            }
         }
         else {
            /* the previous point was off the curve */
            on_curve = ( ol->flags[index] & 1 );
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
      if ( ol->flags[first] & 1 )
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
/*   add a line segment to the PM outline that GetOultine is currently      */
/*   building.                                                              */
/*                                                                          */
static  void Line_From(LONG x, LONG y) {
   line.pte.x = x << 10;
   line.pte.y = y << 10;
   /* store to output buffer */
   memcpy(&(pb[cb]), &line, sizeof(line));
   cb     += sizeof(PRIMLINE);
   polycb += sizeof(PRIMLINE);
}


/****************************************************************************/
/*                                                                          */
/* BezierFrom :                                                             */
/*                                                                          */
/*   add a bezier arc to the PM outline that GetOutline is currently        */
/*   buidling. The second-order Bezier is trivially converted to its        */
/*   equivalent third-order form.                                           */
/*                                                                          */
static  void Bezier_From( LONG x0, LONG y0, LONG x2, LONG y2, LONG x1, LONG y1 ) {
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
/* GetOutline :                                                             */
/*                                                                          */
/*   Translate a FreeType glyph outline into PM format. The buffer is       */
/*   expected to be of the size returned by a previous call to the          */
/*   function GetOutlineLen().                                              */
/*                                                                          */
/*   This code is taken right from the FreeType ttraster.c source, and      */
/*   subsequently modified to emit PM segments and arcs.                    */
/*                                                                          */
static  int GetOutline(TT_Outline *ol, PBYTE pbuf) {
   LONG   x,  y;   /* current point                */
   LONG   cx, cy;  /* current Bezier control point */
   LONG   mx, my;  /* current middle point         */
   LONG   x_first, y_first;  /* first point's coordinates */
   LONG   x_last,  y_last;   /* last point's coordinates  */

   int    index;     /* current point's index */
   BOOL   on_curve;  /* current point's state */
   int    i, start = 0;
   int    first, last;
   ULONG  polystart;

   pb = pbuf;
   cb = 0;

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

      on_curve = (ol->flags[first] & 1);
      index    = first;

      /* check first point to determine origin */
      if ( !on_curve ) {
         /* first point is off the curve.  Yes, this happens... */
         if ( ol->flags[last] & 1 ) {
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
            on_curve = ( ol->flags[index] & 1 );
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
            on_curve = ( ol->flags[index] & 1 );
            if ( on_curve ) {
               /* reaching an `on' point */
               Bezier_From(lastX, lastY, x, y, cx, cy );
               lastX = x;
               lastY = y;
            }
            else {
               /* two successive `off' points => create middle point */
               mx = (cx + x) / 2;
               my = (cy + y)/2;

               Bezier_From( lastX, lastY, mx, my, cx, cy );
               lastX = mx;
               lastY = my;

               cx = x;
               cy = y;
            }
         }
      }

      /* end of contour, close curve cleanly */
      if ( ol->flags[first] & 1 ) {
         if ( on_curve )
            Line_From( lastX, lastY); /* x_first, y_first );*/
         else
            Bezier_From( lastX, lastY, x_first, y_first, cx, cy );
      }
      else
        if (!on_curve)
          Bezier_From( lastX, lastY, x_last, y_last, cx, cy );

      start = ol->contours[i] + 1;

      hdr.cb = polycb;
      memcpy(&(pb[polystart]), &hdr, sizeof(hdr));

   }
   return cb; /* return # bytes used */
}

