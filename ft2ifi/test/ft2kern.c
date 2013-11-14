/*******************************************************************
 *
 *  ft2kern.c
 *
 *  Simple kerning table parser.
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

#include "ft2mem.h"
#include "ft2kern.h"


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

    return error;
}


