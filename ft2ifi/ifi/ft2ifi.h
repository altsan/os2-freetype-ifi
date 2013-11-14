#ifndef _FT2IFI_H_INCLUDED_
#define _FT2IFI_H_INCLUDED_


/****************************************************************************
 * USEFUL MACROS                                                            *
 ****************************************************************************/

/* These are used for grid-fitting 26.6 (1/64th) pixel coordinates */

#define PIXGRID_ROUND( x )    (( x + 32 ) & -64 )   // nearest
#define PIXGRID_FLOOR( x )    ( x & -64 )           // round down
#define PIXGRID_CEILING( x )  (( x + 63 ) & -64 )   // round up


/* These are shortcuts for checking spline point tags */

#define POINT_ON_CURVE( x )   (( x ) & 1 )
#define POINT_IS_CUBIC( x )   (( x ) & 2 )


/****************************************************************************
 * FUNCTIONS                                                                *
 ****************************************************************************/

/* Instead of directly exporting the functions, an IFI driver is supposed to
 * export a single entrypoint which provides a table of function pointers.
 */
LONG _System FdConvertFontFile(PSZ pszSrc, PSZ pszDestDir, PSZ pszNewName);
HFF  _System FdLoadFontFile(PSZ pszFileName);
LONG _System FdUnloadFontFile(HFF hff);
LONG _System FdQueryFaces(HFF hff, PIFIMETRICS pifiMetrics, ULONG cMetricLen,
                          ULONG cFountCount, ULONG cStart);
HFC  _System FdOpenFontContext(HFF hff, ULONG ulFont);
LONG _System FdSetFontContext(HFC hfc, PCONTEXTINFO pci);
LONG _System FdCloseFontContext(HFC hfc);
LONG _System FdQueryFaceAttr(HFC hfc, ULONG iQuery, PBYTE pBuffer,
                             ULONG cb, PGLYPH pagi, GLYPH giStart);
LONG _System FdQueryCharAttr(HFC hfc, PCHARATTR pCharAttr,
                             PBITMAPMETRICS pbmm);
LONG _System FdQueryFullFaces(HFF hff, PVOID pBuffer, PULONG cBuflen,
                              PULONG cFontCount, ULONG cStart);


/****************************************************************************
 * CONSTANTS                                                                *
 ****************************************************************************/

/* Internal 'instance' resolution to use in FreeType library calls.  Using
 * 72 here means that 1 point equals exactly 1 dot or pixel, which simplifies
 * various calculations.  Note that this only applies internally to the
 * driver; OS/2 itself uses its own resolutions for whatever device (screen,
 * printer, whatever) it's currently drawing on.  FdSetFontContext sets up
 * (among other things) the conversion factor between them as needed.
 */
#define INSTANCE_DPI          72

/* These are defined by the TTF spec but aren't in the FT headers */
#define TTFMETRICS_ITALIC     0x1
#define TTFMETRICS_BOLD       0x20
#define TTFMETRICS_REGULAR    0x40

/* Platform-specific Encoding IDs for MS platform */
#define PSEID_SYMBOL          0
#define PSEID_UNICODE         1
#define PSEID_SHIFTJIS        2
#define PSEID_PRC             3
#define PSEID_BIG5            4
#define PSEID_WANSUNG         5
#define PSEID_JOHAB           6
/* We define this one ourselves */
#define PSEID_PM383           100

/* bit masks for determining supported codepages */
#define OS2_CP1_ANSI_OEM_LATIN1                0x1
#define OS2_CP1_ANSI_OEM_LATIN2                0x2
#define OS2_CP1_ANSI_OEM_CYRILLIC              0x4
#define OS2_CP1_ANSI_OEM_GREEK                 0x8
#define OS2_CP1_ANSI_OEM_HEBREW                0x10
#define OS2_CP1_ANSI_OEM_ARABIC                0x20
#define OS2_CP1_ANSI_OEM_BALTIC                0x40
#define OS2_CP1_ANSI_OEM_VIETNAMESE            0x80
#define OS2_CP1_ANSI_OEM_THAI_TIS              (1 << 16)
#define OS2_CP1_ANSI_OEM_JAPANESE_JIS          (1 << 17)
#define OS2_CP1_ANSI_OEM_CHINESE_SIMPLIFIED    (1 << 18)
#define OS2_CP1_ANSI_OEM_CHINESE_TRADITIONAL   (1 << 19)
#define OS2_CP1_ANSI_OEM_KOREAN_WANSUNG        (1 << 20)
#define OS2_CP1_ANSI_OEM_KOREAN_JOHAB          (1 << 21)
#define OS2_CP2_IBM_GREEK_869                  (1 << 48)
#define OS2_CP2_IBM_RUSSIAN_866                (1 << 49)
#define OS2_CP2_IBM_NORDIC_865                 (1 << 50)
#define OS2_CP2_IBM_ARABIC_864                 (1 << 51)
#define OS2_CP2_IBM_HEBREW_862                 (1 << 53)
#define OS2_CP2_IBM_ICELANDIC_861              (1 << 54)
#define OS2_CP2_IBM_PORTUGUESE_860             (1 << 55)
#define OS2_CP2_IBM_CYRILLIC_855               (1 << 57)
#define OS2_CP2_IBM_LATIN2_852                 (1 << 58)
#define OS2_CP2_IBM_LATIN1_850                 (1 << 62)
#define OS2_CP2_IBM_US_437                     (1 << 63)

/* bit masks indicating charset support in fsCapabilities */
#define IFIMETRICS_DBCS_JAPAN                   0x0001
#define IFIMETRICS_DBCS_TAIWAN                  0x0002
#define IFIMETRICS_DBCS_CHINA                   0x0004
#define IFIMETRICS_DBCS_KOREA                   0x0008
#define IFIMETRICS_CHARSET_LATIN1               0x0010
#define IFIMETRICS_CHARSET_PC                   0x0020
#define IFIMETRICS_CHARSET_LATIN2               0x0040
#define IFIMETRICS_CHARSET_CYRILLIC             0x0080
#define IFIMETRICS_CHARSET_HEBREW               0x0100
#define IFIMETRICS_CHARSET_GREEK                0x0200
#define IFIMETRICS_CHARSET_ARABIC               0x0400
#define IFIMETRICS_CHARSET_UGLEXT               0x0800
#define IFIMETRICS_CHARSET_KANA                 0x1000
#define IFIMETRICS_CHARSET_THAI                 0x2000

/* IMPORTANT: If you change any of these, double-check the code because it
 *            makes some assumptions about certain values being >= others.
 */
/* defines for character translation */
/* from Unicode to UGL          */
#define TRANSLATE_UGL         0
/* Symbol - no translation      */
#define TRANSLATE_SYMBOL      1
/* Unicode - no translation     */
#define TRANSLATE_UNICODE     2
/* Big5 - no translation        */
#define TRANSLATE_BIG5        4
/* ShiftJIS - no translation    */
#define TRANSLATE_SJIS        5
/* GB2312 - no translation      */
#define TRANSLATE_GB2312      6
/* Wansung - no translation     */
#define TRANSLATE_WANSUNG     7
/* Johab - no translation       */
#define TRANSLATE_JOHAB       8
/* from Unicode to Big5         */
#define TRANSLATE_UNI_BIG5    16
/* from Unicode to ShiftJIS     */
#define TRANSLATE_UNI_SJIS    17
/* from Unicode to GB2312       */
#define TRANSLATE_UNI_GB2312  18
/* from Unicode to Wansung      */
#define TRANSLATE_UNI_WANSUNG 19
/* from Unicode to Johab        */
#define TRANSLATE_UNI_JOHAB   20

/* This may not work on Warp 4 (maybe FP 13+ is OK) but do we really care? */
#define MAX_GLYPH 1105

/* To determine DBCS font face */
#define AT_LEAST_DBCS_GLYPH     3072


/**** Font status flags ****/
/* (these will be kept in the low-order word of the flags field) */

/* Flag : The font face has a fixed pitch width */
#define FC_FLAG_FIXED_WIDTH      0x1

/* Flag : Effectively duplicated  FL_FLAG_DBCS_FILE. This info is    */
/*       kept twice for simplified access                            */
#define FC_FLAG_DBCS_FACE        0x2

/* Flag : This face is actually an alias, use the underlying face instead */
#define FL_FLAG_FACENAME_ALIAS   0x8

/* Flag : The font file has a live FreeType face object */
#define FL_FLAG_LIVE_FACE        0x10

/* Flag : A font file's face has a context open - DON'T CLOSE IT! */
#define FL_FLAG_CONTEXT_OPEN     0x20

/* Flag : This file has been already opened previously*/
#define FL_FLAG_ALREADY_USED     0x40

/* Flag : This is a font including DBCS characters; this also means  */
/*       the font driver presents to the system a second, vertically */
/*       rendered, version of this typeface with name prepended by   */
/*       an '@' (used in horizontal-only word processors)            */
/* Note : For TTCs, the whole collection is either DBCS or not. I've */
/*       no idea if there are any TTCs with both DBCS and non-DBCS   */
/*       faces. It's possible, but sounds unlikely.                  */
#define FL_FLAG_DBCS_FILE        0x80


/**** User-settable configuration flags ****/
/* (these are kept in the high-order word of the flags field) */

/* Option flags that are only used per font-file */
#define FL_OPT_FAKE_BOLD        0x010000    // create a fake bold face
#define FL_OPT_FAKE_ITALIC      0x020000    // create a fake italic face (not implemented)

/* Option flags that can be used for a font-file, or globally */
#define FL_OPT_FORCE_UNICODE    0x100000    // force use of Unicode over UGL-translation
#define FL_OPT_FORCE_UNI_MBCS   0x200000    // always set MBCS flag when Unicode used
#define FL_OPT_ALT_FACENAME     0x400000    // use alternate facename logic

/* Option flags that are only used globally */
#define FL_OPT_ALIAS_TMS_RMN    0x1000000   // create a special 'TmsRmn' alias for 'Times New Roman'
#define FL_OPT_STYLE_FIX        0x2000000   // enable the OpenOffice face/style name workaround


#define IFI_PROFILE_APP         "FreeType IFI v2"
#define IFI_PROFILE_FONTFLAGS   "FreeType IFI v2:FontFlags"


/****************************************************************************
 * TYPEDEFS                                                                 *
 ****************************************************************************/


/* --------------------------------------------------------------------------
 * Font-name structure returned by QueryFullFaces().  The face name buffer
 * follows abFamilyName at the location indicated by cbFacenameOffset (counted
 * from the start of the structure).  The cbLength field indicates the total
 * length, including both the family and face name strings.
 */
#pragma pack(1)

typedef struct _FFDESCS2 {
    ULONG cbLength;             // length of this entry (WORD aligned)
    ULONG cbFacenameOffset;     // offset of face name (WORD aligned)
    UCHAR abFamilyName[1];      // family name string
} FFDESCS2, *PFFDESCS2;


/* --------------------------------------------------------------------------
 * TList and TListElement
 *
 *  simple structures used to implement a doubly linked list. Lists are
 *  used to implement the HFF object lists, as well as the font size and
 *  outline caches.
 */

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



/* --------------------------------------------------------------------------
 * TFontFace
 *
 *  a structure related to an open font face. It contains data for each of
 *  possibly several faces in a .TTC file.
 */

typedef struct _TFontFace  TFontFace, *PFontFace;
struct _TFontFace
{
   FT_Face       face;            /* handle to actual FreeType face object  */
   FX_Kern_0     *directory;      /* kerning directory                      */
   USHORT        *widths;         /* glyph width cache for large fonts      */
   USHORT        *kernIndices;    /* reverse translation cache for kerning  */
   ULONG         flags;           /* various FC_* flags (like FC_FLAG_FIXED)*/
   LONG          charMode;        /* character translation mode             */
};


/* --------------------------------------------------------------------------
 * TFontFile
 *
 *  a structure related to an open font file handle. All TFontFiles are
 *  kept in a simple linked list. There can be several faces in one font.
 *  Face(s) information is stored in a variable-length array of TFontFaces.
 *  A single TFontFile structure exactly corresponds to one HFF.
 */

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


/* --------------------------------------------------------------------------
 * TFontSize
 *
 *  a structure related to a opened font context (a.k.a. instance or
 *  transform/pointsize). It exactly corresponds to a HFC.
 */

typedef struct _TFontSize  TFontSize, *PFontSize;
struct _TFontSize
{
   PListElement  element;       /* List element for this font size          */
   HFC           hfc;           /* HFC handle used from outside             */
// FT_Size       instance;      /* handle to FreeType instance              */
   BOOL          transformed;   /* TRUE = rotation/shearing used (rare)     */
   BOOL          vertical;      /* TRUE = vertically rendered DBCS face     */
   BOOL          fakebold;      /* TRUE = fake bold DBCS face               */
   FT_Matrix     matrix;        /* transformation matrix                    */
   PFontFile     file;          /* HFF this context belongs to              */
   ULONG         faceIndex;     /* index of face in a font (for TTCs)       */
   ULONG         scaleFactor;   /* scaling factor used for this context     */
};

#pragma pack()

#endif /* _FT2IFI_H_INCLUDED_ */
