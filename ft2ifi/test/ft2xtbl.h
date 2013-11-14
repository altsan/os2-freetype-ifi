/*********************************************************************
 *
 *  ft2table.h
 *
 *  Simple parser for various TT/OT tables that we need to access
 *  directly.
 *
 *  Borrows somewhat from the old ftxkern extension, which is
 *  copyright (c) 1996-1999 by David Turner, Robert Wilhelm, and
 *  Werner Lemberg.
 *
 *  This file may only be used, modified and distributed under the
 *  terms of the FreeType project license.  By continuing to use,
 *  modify, or distribute this file you indicate that you have read
 *  the license and understand and accept it fully.
 *
 ********************************************************************/

#ifndef FT2TABLE_H
#define FT2TABLE_H


/* ---------------------------------------------------------------------------
 * Kerning error codes
 */

#define FX_Err_Invalid_Kerning_Table_Format  0x0A00
#define FX_Err_Invalid_Kerning_Table         0x0A01


/* ---------------------------------------------------------------------------
 * Exported structures
 */

#pragma pack(1)

/* A format 0 kerning subtable.
 */

typedef struct  TT_Kern_0_Pair_
{
    FT_UShort  left;            /* index of left  glyph in pair */
    FT_UShort  right;           /* index of right glyph in pair */
    FT_FWord   value;           /* kerning value                */
} FX_Kern_0_Pair;

typedef struct  TT_Kern_0_
{
    FT_UShort  nPairs;          /* number of kerning pairs */
    FT_UShort  searchRange;     /* these values are defined by the TT spec */
    FT_UShort  entrySelector;   /*   for table searchs.                    */
    FT_UShort  rangeShift;      /*                                         */
    FX_Kern_0_Pair pairs[1];    /* table of nPairs `pairs'                 */
} FX_Kern_0;


/* A format 2 kerning subtable (we don't actually use this).
 */

typedef struct  TT_Kern_2_Class_
{
    FT_UShort   firstGlyph;     /* first glyph in range                    */
    FT_UShort   nGlyphs;        /* number of glyphs in range               */
    FT_UShort*  classes;        /* a table giving for each ranged glyph    */
                                /* its class offset in the subtable pairs  */
                                /* two-dimensional array                   */
} FX_Kern_2_Class;

typedef struct TT_Kern_2_
{
    FT_UShort        rowWidth;       /* length of one row in bytes         */
    FX_Kern_2_Class  leftClass;      /* left class table                   */
    FX_Kern_2_Class  rightClass;     /* right class table                  */
    FT_FWord*        array;          /* 2-dimensional kerning values array */
} FX_Kern_2;


/* A kerning subtable header.
 */
typedef struct  _Kerning_Subtable {
    FT_UShort  version;        /* table version number                     */
    FT_UShort  length;         /* length of table, including this header   */
    FT_Byte    format;         /* the subtable format, as found in the     */
                               /*   high-order byte of the coverage entry  */
    FT_Byte    coverage;       /* low-order byte of the coverage entry     */
    union {
      FX_Kern_0  kern0;
      FX_Kern_2  kern2;
    } data;
} FX_Kerning_Subtable;


/* The top-level kerning directory header.
 */
typedef struct _Kerning_Table {
    FT_UShort           version;     /* kern table version number (from 0) */
    FT_UShort           nTables;     /* number of subtables                */
    FX_Kerning_Subtable subtable[1]; /* first subtable (others follow)     */
} FX_Kerning;

#pragma pack()


/* ---------------------------------------------------------------------------
 * Function prototypes
 */

FT_Error FX_Get_Kerning_Pairs( FT_Face face, FX_Kerning **kern, FT_UShort *found );
FT_Error FX_Get_OS2_Table( FT_Face face, TT_OS2  **OS2 );
FT_Error FX_Get_PostScript_Table( FT_Face face, TT_Postscript **post );

#define FT_Ok   0

#endif /* FT2TABLE_H */


