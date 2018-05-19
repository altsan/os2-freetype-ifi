#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H


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
FT_Bool CopyBitmap( char *pSource, char *pTarget,
                    unsigned long rows, unsigned long s_pitch, unsigned long t_pitch )
{
    unsigned long row, col, ofs;

    if (( pSource && !pTarget ) || ( t_pitch < s_pitch ))
        return 0;
    ofs = 0;
    for ( row = 0; row < rows; row++ ) {
        for ( col = 0; col < s_pitch; col++ ) {
            pTarget[ ofs++ ] = pSource[ col + (row * s_pitch) ];
        }
        for ( ; col < t_pitch; col++ ) pTarget[ ofs++ ] = 0;
    }
    return 1;
}


/* ------------------------------------------------------------------------- */
int main ( int argc, char *argv[] )
{
    FT_Library    library;
    FT_Face       face;
    FT_Error      error = 0;
    int           rc;
    int           i, row, col;
    char          *pBitmap;
    long          width, rows, cols;
    unsigned long gindex;

    if ( argc < 2 ) {
        printf("Syntax: RASTEST <filename>\n\n");
        printf("\nNo font file specified.\n");
        return 0;
    }

    error = FT_Init_FreeType( &library );
    if ( error ) {
        printf("An error occurred during library initialization.\n");
        return (int) error;
    }
    printf("FreeType 2 library initialized successfully.\n");

    error = FT_New_Face( library, argv[1], 0, &face );
    if ( error ) {
        if ( error == FT_Err_Unknown_File_Format )
            printf("The file format is unsupported.\n");
        else
            printf("The file \"%s\" could not be loaded.\n", argv[1]);
        FT_Done_FreeType( library );
        return ( error );
    }
    printf("Font %s read successfully.\n", face->family_name );

    error = FT_Set_Char_Size( face, 0, 12*64, 120, 120 );
    if ( error ) goto done;
    gindex = FT_Get_Char_Index( face, (unsigned long)'a' );
    error = FT_Load_Glyph( face, gindex, FT_LOAD_NO_BITMAP );
    if ( error ) goto done;
    error = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_MONO );
    if ( error ) goto done;

    width = face->glyph->bitmap.width;
    rows  = face->glyph->bitmap.rows;

    cols  = face->glyph->bitmap.pitch;
    pBitmap = (char*)face->glyph->bitmap.buffer;

    // width rounded up to nearest multiple of 4 bytes
    cols  = ((width + 31) / 8) & -4;
    pBitmap = (char *) calloc( rows*cols, sizeof(char) );
    if ( !pBitmap || !CopyBitmap( face->glyph->bitmap.buffer, pBitmap,
                                  rows, face->glyph->bitmap.pitch, cols ))
        goto done;

    printf("Showing character %u\n", gindex );
    printf("Character bitmap has %u rows of %u bytes.\n", rows, cols);

    for (row=0; row < rows; row++) {
       for (col=0; col < cols; col++, pBitmap++) {
          printf(" ");
          for (i=7; i>=0; i--) {
             printf( ((*pBitmap >> i) & 1) ? "#" : ".");
          }
       }
       printf("\n");
    }
//  free( pBitmap );

done:
    error = FT_Done_Face( face );
    FT_Done_FreeType( library );
    return (int) error;
}

