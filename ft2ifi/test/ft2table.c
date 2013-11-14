/*******************************************************************
 *
 *  ft2table.c
 *
 *  Simple parsing routines for several OpenType tables.
 *
 *  NOTE: These routines are rather inefficient for our purposes, in
 *        that they allocate buffers and copy the stream data into
 *        them.  It would probably be better if I could find a way
 *        to parse out the stream data directly.
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

#include "ft2table.h"

extern unsigned long safe_alloc( void **object, unsigned long length );
extern unsigned long safe_free( void *object );

#define SWAP_SHORT(value)   (FT_UShort)((value << 8) | ((FT_UShort)value >> 8))
#define SWAP_LONG(value)    (FT_ULong)(((value << 24) | ((value << 8)&0xff0000) | (((FT_ULong)value >> 8)&0xff00) | ((FT_ULong)value >> 24)))


/*******************************************************************
 *
 *  Function    :  FX_Get_Kerning_Pairs
 *
 *  Description :  Loads the kerning table from a TrueType/OpenType
 *                 font and locates a format 0 subtable.
 *
 *  Input  :  face   Face handle.
 *            kern0  Pointer to a buffer into which the format 0
 *                   kerning subtable data will be copied.
 *
 *  Output :  error code
 *
 *  Notes  :  On successful return, the memory for the kerning
 *            data buffer will be allocated by this function.
 *            It should be freed when no longer needed.
 *
 ******************************************************************/

FT_Error FX_Get_Kerning_Pairs( FT_Face     face,
                               FX_Kern_0 **kern0 )
{
    FX_Kerning          *pKern;
    FX_Kerning_Subtable *pKST;
    FX_Kern_0           *pKD;

    FT_Error   error;
    FT_ULong   length;
    FT_UShort  sub_index,
               pair;
    ULONG      cb;

    /* Try to load the KERN table from the font.
     */
    length = 0;
    error = FT_Load_Sfnt_Table( face, TTAG_kern, 0, NULL, &length );
    if ( error )
        return error;
    if ( length < sizeof( FX_Kerning ))
        return FX_Err_Invalid_Kerning_Table;

    error = (FT_Error) safe_alloc( (PPVOID) &pKern, length );
    if ( error )
        return FT_Err_Out_Of_Memory;

    error = FT_Load_Sfnt_Table( face, TTAG_kern, 0, (PVOID) pKern, &length );
    if ( error )
        goto Finish;

    /* Numbers in the font are big-endian but they'll have been treated as
     * native (little-endian) when the buffer was populated.  We need to
     * convert them back to big-endian.
     */
    error = FT_Err_Table_Missing;
    pKern->version = SWAP_SHORT( pKern->version );
    pKern->nTables = SWAP_SHORT( pKern->nTables );
    if ( pKern->nTables == 0 )
        goto Finish;

    /* Make sure that a format 0 subtable exists. */
    error = FX_Err_Invalid_Kerning_Table_Format;
    sub_index = 0;
    pKST = (FX_Kerning_Subtable *)(pKern->tables);
    pKST->version = SWAP_SHORT( pKST->version );
    pKST->length  = SWAP_SHORT( pKST->length );
#if 1
    /* Disable this block to prevent checking subtables other than the 1st */
    while (( pKST->format != 0 ) && ( sub_index < pKern->nTables )) {
        pKST = (PVOID)(pKST + pKST->length);
        pKST->version = SWAP_SHORT( pKST->version );
        pKST->length  = SWAP_SHORT( pKST->length );
        sub_index++;
    }
#endif
    if ( pKST->format != 0 )
        goto Finish;

    /* Simple sanity check in case of bogus or mis-read length value */
    cb = ( SWAP_SHORT( pKST->data.kern0.nPairs ) * sizeof( FX_Kern_0_Pair )) + 8;
    if (( (ULONG)pKST + cb ) > ( (ULONG)pKern + length ))
        goto Finish;

    error = safe_alloc( (PPVOID) &pKD, cb );
    if ( error ) {
        error = FT_Err_Out_Of_Memory;
        goto Finish;
    }
    memcpy( (PVOID) pKD, (PVOID) &(pKST->data.kern0), cb );

    /* We've found the kerning subtable, now fix its values.
     */
    pKD->nPairs = SWAP_SHORT( pKD->nPairs );
    pKD->searchRange = SWAP_SHORT( pKD->searchRange );
    pKD->entrySelector = SWAP_SHORT( pKD->entrySelector );
    pKD->rangeShift = SWAP_SHORT( pKD->rangeShift );
    for ( pair = 0; pair < pKD->nPairs; pair++ ) {
        pKD->pairs[pair].left = SWAP_SHORT( pKD->pairs[pair].left );
        pKD->pairs[pair].right = SWAP_SHORT( pKD->pairs[pair].right );
        pKD->pairs[pair].value = SWAP_SHORT( pKD->pairs[pair].value );
    }

    /* All done.
     */
    *kern0 = pKD;
    error  = 0;

Finish:
    safe_free( pKern );
    return error;
}


#if 0

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
    if ( error )
        return error;

    error = (FT_Error) safe_alloc( (PPVOID) &pOS2, length );
    if ( error )
        return FT_Err_Out_Of_Memory;

    error = FT_Load_Sfnt_Table( face, TTAG_OS2, 0, (PVOID) pOS2, &length );
    if ( error ) {
        safe_free( pOS2 );
        return error;
    }

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

    if ( pOS2->version > 0 ) {
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
    return 0;
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
    if ( error ) {
        safe_free( pPost );
        return error;
    }

    /* Numbers in the font are big-endian but they'll have been treated as
     * native (little-endian) when the buffer was populated.  We need to
     * convert them back to big-endian.
     */
    pPost->FormatType  = (FT_Long) SWAP_LONG( pPost->FormatType );
    pPost->italicAngle = (FT_Long) SWAP_LONG( pPost->italicAngle );

    pPost->underlinePosition  = SWAP_SHORT( pPost->underlinePosition );
    pPost->underlineThickness = SWAP_SHORT( pPost->underlineThickness );

    pPost->isFixedPitch = SWAP_LONG( pPost->isFixedPitch );
    pPost->minMemType42 = SWAP_LONG( pPost->minMemType42 );
    pPost->maxMemType42 = SWAP_LONG( pPost->maxMemType42 );
    pPost->minMemType1  = SWAP_LONG( pPost->minMemType1 );
    pPost->maxMemType1  = SWAP_LONG( pPost->maxMemType1 );

    /* All done.
     */
    *post = pPost;
    return 0;
}

#endif
