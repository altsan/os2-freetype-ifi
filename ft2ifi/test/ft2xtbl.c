/*******************************************************************
 *
 *  ft2table.c
 *
 *  Simple parsing routines for several OpenType tables.
 *
 *  NOTE: These routines are very inefficient for our purposes, as
 *        they basically allocate buffers and copy the stream data
 *        into them.  It would probably be much better if I could
 *        figure out a way to parse out the stream data directly.
 *
 *  This file may only be used, modified and distributed under the
 *  terms of the FreeType project license, LICENSE.TXT.  By
 *  continuing to use, modify, or distribute this file you indicate
 *  that you have read the license and understand and accept it
 *  fully.
 *
 ******************************************************************/

#include <os2.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#include FT_SYSTEM_H

#include "ft2xtbl.h"
#include "ft2mem.h"

#define SWAP_SHORT(value)   ((FT_UShort)((value << 8) | (value >> 8)))
#define SWAP_LONG(value)    ((FT_ULong)((value << 24) | ((value << 8)|0xff0000) | ((value >> 8)|0xff00) | ((value >> 24)|0xff)))


/*******************************************************************
 *
 *  Function    :  FX_Get_Kerning_Pairs
 *
 *  Description :  Loads the kerning table from a TrueType/OpenType
 *                 font and locates a format 0 subtable.
 *
 *  Input  :  face   Face handle.
 *            kern   Pointer to a buffer into which the kerning
 *                   table will be copied.
 *            found  Index of the first format 0 subtable found.
 *
 *  Output :  error code
 *
 *  Notes  :  On successful return, the memory for the kerning
 *            table buffer will be allocated by this function.
 *            It should be freed when no longer needed.
 *
 ******************************************************************/

FT_Error FX_Get_Kerning_Pairs( FT_Face    face,
                               FX_Kerning **kern,
                               FT_UShort  *found )
{
    FT_Error   error;
    FT_ULong   length;
    FT_UShort  sub_index,
               pair;
    FX_Kerning *pKern;


    /* Try to load the KERN table from the font.
     */
    length = 0;
    error = FT_Load_Sfnt_Table( face, TTAG_kern, 0, NULL, &length );
    if ( error ) return error;
    if ( length < sizeof( FX_Kerning ))
        return FX_Err_Invalid_Kerning_Table;

    error = (FT_Error) safe_alloc( (PPVOID) &pKern, length );
    if ( error )
        return FT_Err_Out_Of_Memory;

    error = FT_Load_Sfnt_Table( face, TTAG_kern, 0, (PVOID) pKern, &length );
    if ( error ) return error;

    /* Numbers in the font are big-endian but they'll have been treated as
     * native (little-endian) when the buffer was populated.  We need to
     * convert them back to big-endian.
     */
    pKern->version = SWAP_SHORT( pKern->version );
    pKern->nTables = SWAP_SHORT( pKern->nTables );

    if ( pKern->nTables == 0 ) {
        safe_free( pKern );
        return FT_Err_Table_Missing;
    }

    /* Make sure that a format 0 subtable exists.  (Fortunately, the format
     * field is one byte so we don't need any fixups before checking it.)
     */
    for ( sub_index = 0; (sub_index < pKern->nTables) &&
                         (pKern->subtable[sub_index].format != 0); sub_index++ );
    if ( sub_index == pKern->nTables ) {
        safe_free( pKern );
        return FX_Err_Invalid_Kerning_Table_Format;
    }

    /* We've found the kerning subtable, now fix its values.
     */
    pKern->subtable[sub_index].version = SWAP_SHORT( pKern->subtable[sub_index].version );
    pKern->subtable[sub_index].length  = SWAP_SHORT( pKern->subtable[sub_index].length );
    pKern->subtable[sub_index].data.kern0.nPairs = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.nPairs );
    pKern->subtable[sub_index].data.kern0.searchRange = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.searchRange );
    pKern->subtable[sub_index].data.kern0.entrySelector = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.entrySelector );
    pKern->subtable[sub_index].data.kern0.rangeShift = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.rangeShift );
    for ( pair = 0; pair < pKern->subtable[sub_index].data.kern0.nPairs; pair++ ) {
        pKern->subtable[sub_index].data.kern0.pairs[pair].left = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.pairs[pair].left );
        pKern->subtable[sub_index].data.kern0.pairs[pair].right = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.pairs[pair].right );
        pKern->subtable[sub_index].data.kern0.pairs[pair].value = SWAP_SHORT( pKern->subtable[sub_index].data.kern0.pairs[pair].value );
    }

    /* All done.
     */
    *found = sub_index;
    *kern  = pKern;

    return FT_Ok;
}


/*******************************************************************
 *
 *  Function    :  FX_Get_OS2_Table
 *
 *  Description :  Loads the OS/2 table from a TrueType/OpenType
 *                 font.
 *
 *  Input  :  face  Face handle.
 *            OS2   Pointer to a buffer into which the OS/2 table
 *                  will be copied.
 *
 *  Output :  error code
 *
 *  Notes  :  On successful return, the memory for the input
 *            buffer will be allocated by this function.  It
 *            should be freed by the caller when no longer needed.
 *
 ******************************************************************/

FT_Error FX_Get_OS2_Table( FT_Face face,
                           TT_OS2  **OS2 )
{
    FT_Error   error;
    FT_ULong   length;
    TT_OS2    *pOS2;


    /* Try to load the OS/2 table from the font.
     */
    length = 0;
    error = FT_Load_Sfnt_Table( face, TTAG_OS2, 0, NULL, &length );
    if ( error ) return error;
    if ( length < sizeof( TT_OS2 ))
        return FT_Err_Table_Missing;

    error = (FT_Error) safe_alloc( (PPVOID) &pOS2, length );
    if ( error )
        return FT_Err_Out_Of_Memory;

    error = FT_Load_Sfnt_Table( face, TTAG_OS2, 0, (PVOID) pOS2, &length );
    if ( error ) return error;

    /* Numbers in the font are big-endian but they'll have been treated as
     * native (little-endian) when the buffer was populated.  We need to
     * convert them back to big-endian.
     */
    pOS2->version = SWAP_SHORT( pOS2->version );
    if ( pOS2->version == 0xFFFF ) {
        safe_free( pOS2 );
        return FT_Err_Table_Missing;
    }

    pOS2->xAvgCharWidth       = SWAP_SHORT( pOS2->xAvgCharWidth );
    pOS2->usWeightClass       = SWAP_SHORT( pOS2->usWeightClass );
    pOS2->usWidthClass        = SWAP_SHORT( pOS2->usWidthClass );
    pOS2->fsType              = SWAP_SHORT( pOS2->fsType );
    pOS2->ySubscriptXSize     = SWAP_SHORT( pOS2->ySubscriptXSize );
    pOS2->ySubscriptYSize     = SWAP_SHORT( pOS2->ySubscriptYSize );
    pOS2->ySubscriptXOffset   = SWAP_SHORT( pOS2->ySubscriptXOffset );
    pOS2->ySubscriptYOffset   = SWAP_SHORT( pOS2->ySubscriptYOffset );
    pOS2->ySuperscriptXSize   = SWAP_SHORT( pOS2->ySuperscriptXSize );
    pOS2->ySuperscriptYSize   = SWAP_SHORT( pOS2->ySuperscriptYSize );
    pOS2->ySuperscriptXOffset = SWAP_SHORT( pOS2->ySuperscriptXOffset );
    pOS2->ySuperscriptYOffset = SWAP_SHORT( pOS2->ySuperscriptYOffset );
    pOS2->yStrikeoutSize      = SWAP_SHORT( pOS2->yStrikeoutSize );
    pOS2->yStrikeoutPosition  = SWAP_SHORT( pOS2->yStrikeoutPosition );
    pOS2->sFamilyClass        = SWAP_SHORT( pOS2->sFamilyClass );

    pOS2->ulUnicodeRange1 = SWAP_LONG( pOS2->ulUnicodeRange1 );
    pOS2->ulUnicodeRange2 = SWAP_LONG( pOS2->ulUnicodeRange2 );
    pOS2->ulUnicodeRange3 = SWAP_LONG( pOS2->ulUnicodeRange3 );
    pOS2->ulUnicodeRange4 = SWAP_LONG( pOS2->ulUnicodeRange4 );

    pOS2->fsSelection      = SWAP_SHORT( pOS2->fsSelection );
    pOS2->usFirstCharIndex = SWAP_SHORT( pOS2->usFirstCharIndex );
    pOS2->usLastCharIndex  = SWAP_SHORT( pOS2->usLastCharIndex );
    pOS2->sTypoAscender    = SWAP_SHORT( pOS2->sTypoAscender );
    pOS2->sTypoDescender   = SWAP_SHORT( pOS2->sTypoDescender );
    pOS2->sTypoLineGap     = SWAP_SHORT( pOS2->sTypoLineGap );
    pOS2->usWinAscent      = SWAP_SHORT( pOS2->usWinAscent );
    pOS2->usWinDescent     = SWAP_SHORT( pOS2->usWinDescent );

    if ( pOS2->version == 1 ) {
        pOS2->ulCodePageRange1 = SWAP_LONG( pOS2->ulCodePageRange1 );
        pOS2->ulCodePageRange2 = SWAP_LONG( pOS2->ulCodePageRange2 );
    }
    if ( pOS2->version > 1 ) {
        pOS2->sxHeight      = SWAP_SHORT( pOS2->sxHeight );
        pOS2->sCapHeight    = SWAP_SHORT( pOS2->sCapHeight );
        pOS2->usDefaultChar = SWAP_SHORT( pOS2->usDefaultChar );
        pOS2->usBreakChar   = SWAP_SHORT( pOS2->usBreakChar );
        pOS2->usMaxContext  = SWAP_SHORT( pOS2->usMaxContext );
    }

    /* All done.
     */
    *OS2  = pOS2;
    return FT_Ok;
}


/*******************************************************************
 *
 *  Function    :  FX_Get_PostScript_Table
 *
 *  Description :  Loads the PostScript (post) table from a
 *                 TrueType/OpenType font.
 *
 *  Input  :  face  Face handle.
 *            post  Pointer to a buffer into which the PostScript
 *                  table will be copied.
 *
 *  Output :  error code
 *
 *  Notes  :  On successful return, the memory for the input
 *            buffer will be allocated by this function.  It
 *            should be freed by the caller when no longer needed.
 *
 ******************************************************************/

FT_Error FX_Get_PostScript_Table( FT_Face       face,
                                  TT_Postscript **post )
{
    FT_Error      error;
    FT_ULong      length;
    TT_Postscript *pPost;


    /* Try to load the PostScript table from the font.
     */
    length = 0;
    error = FT_Load_Sfnt_Table( face, TTAG_post, sizeof(long), NULL, &length );
    if ( error ) return error;
    if ( length < sizeof( TT_Postscript ))
        return FT_Err_Table_Missing;

    error = (FT_Error) safe_alloc( (PPVOID) &pPost, length );
    if ( error )
        return FT_Err_Out_Of_Memory;

    error = FT_Load_Sfnt_Table( face, TTAG_post, sizeof(long), (PVOID) pPost, &length );
    if ( error ) return error;

    /* Numbers in the font are big-endian but they'll have been treated as
     * native (little-endian) when the buffer was populated.  We need to
     * convert them back to big-endian.
     */
    pPost->FormatType  = SWAP_LONG( pPost->FormatType );
    pPost->italicAngle = SWAP_LONG( pPost->italicAngle );

    pPost->underlinePosition  = SWAP_SHORT( pPost->underlinePosition );
    pPost->underlineThickness = SWAP_SHORT( pPost->underlineThickness );

    pPost->isFixedPitch = SWAP_LONG( pPost->isFixedPitch );
    pPost->minMemType42 = SWAP_LONG( pPost->minMemType42 );
    pPost->maxMemType42 = SWAP_LONG( pPost->maxMemType42 );
    pPost->minMemType1  = SWAP_LONG( pPost->minMemType1 );
    pPost->maxMemType1  = SWAP_LONG( pPost->maxMemType1 );

    /* All done.
     */
    *post  = pPost;
    return FT_Ok;
}

