#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uconv.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H

#include "ft2table.h"
#include "ft2mem.h"


#if 0
extern FT_BASE_DEF( FT_Error ) FT_Stream_Open( FT_Stream stream, const char*  filepathname );
#define STREAM_FILE( stream )  ( (HFILE)stream->descriptor.pointer )

FT_Error FX_Flush_Stream( FT_Face face ) {
    DosSetFilePtr( STREAM_FILE( face->stream ),
                   0, FILE_CURRENT, &(face->stream->pos));
    face->stream->close( face->stream );
    return FT_Err_Ok;
}


FT_Error FX_Activate_Stream( FT_Face face ) {
    FT_ULong ulPos;
    FT_Error error;

    ulPos = face->stream->pos;
    if ( ! STREAM_FILE( face->stream )) {
        error = FT_Stream_Open( face->stream, face->stream->pathname.pointer );
        if ( !error )
            DosSetFilePtr( STREAM_FILE( face->stream ),
                           ulPos, FILE_BEGIN, &(face->stream->pos));
    }
    else
        error = FT_Err_Ok;
    return error;
}
#endif


static UconvObject uconv;

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Dump a byte sequence as a list of hex byte values                         */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void DumpUnicodeString( PCH pchBytes, ULONG length )
{
    ULONG i;
    for ( i = 0; i < length; i++ ) {
        printf("%02X", pchBytes[ i ] );
        if ( i+1 < length ) printf(" ");
    }
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Print a Unicode (UCS-2) name string as a normal codepage string           */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void PrintUnicodeString( PCH pchBytes, ULONG length )
{
    PSZ pszOutput,
        psz;
    UniChar *puInput,
            *pu;
    size_t stIn, stOut, stSub;

    if (( puInput = (UniChar *) calloc( length+sizeof(UniChar), 1 )) == NULL ) return;
    UniStrncpy( puInput, (UniChar *) pchBytes, length / sizeof(UniChar) );
    stIn  = length / sizeof(UniChar);
    stOut = stIn * 4;
    stSub = 0;
    pszOutput = (PSZ) calloc( stOut+1, 1 );
    pu  = puInput;
    psz = pszOutput;
    if ( psz && ( UniUconvFromUcs( uconv, &pu, &stIn, (PPVOID) &psz,
                                   &stOut, &stSub ) == ULS_SUCCESS ))
        printf("%s", pszOutput );
    if ( puInput ) free( puInput );
    if ( pszOutput ) free( pszOutput );
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Print a name string                                                       */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void PrintNameString( PCH pchBytes, ULONG length )
{
    PSZ pszOutput;

    pszOutput = (PSZ) calloc( length+1, 1 );
    if ( pszOutput ) {
        strncpy( pszOutput, (PSZ) pchBytes, length );
        printf("%s", pszOutput );
        free( pszOutput );
    }
}


/* ------------------------------------------------------------------------- */
int main ( int argc, char *argv[] )
{
    FT_Library  library;
    FT_Face     face;
    FT_FaceRec  props;
    FT_Error    error = 0;
    FT_UInt     names;
    FT_CharMap  charmap;
    FX_Kern_0   *kstable;
    TT_OS2      *pOS2;
    TT_Postscript *ppost;
    ULONG       rc;
    BOOL        bDumpUCS = FALSE;
    BOOL        bShowKST = FALSE;
    BOOL        bShowOS2 = FALSE;
    BOOL        bShowPS  = FALSE;
    char        szPID[10];
    char        szEID[32];
    int         i;


    char     *buf;

    if ( argc < 2 ) {
        printf("FONTNAME - Show summary of OpenType font names and other optional information.\n");
        printf("Syntax: FONTNAME <filename> [ options ]\n\n");
        printf("  /D    Dump Unicode and DBCS strings as hex values\n");
        printf("  /K    Show kerning summary (KERN format 0 only)\n");
        printf("  /O    Show OS/2 table\n");
        printf("  /P    Show POST table\n\n");
        printf("NAME and CMAP table information is always shown.\n\n");
        printf("\nNo font file specified.\n");
        return 0;
    }
    for ( i = 2; i < argc; i++ ) {
        if ( strlen( argv[i] ) > 1 ) {
            CHAR cOption = 0;
            if ( argv[i][0] == '/' || argv[i][0] == '-') cOption = argv[i][1];
            switch ( cOption ) {
                case 'd':
                case 'D':
                    bDumpUCS = TRUE;
                    break;
                case 'k':
                case 'K':
                    bShowKST = TRUE;
                    break;
                case 'o':
                case 'O':
                    bShowOS2 = TRUE;
                    break;
                case 'p':
                case 'P':
                    bShowPS = TRUE;
                    break;
                default : break;
            }
        }
    }

    if (( rc = UniCreateUconvObject( L"@endian=big", &uconv )) != ULS_SUCCESS ) {
        printf("Failed to create Unicode conversion object (rc=0x%X).\n");
        printf("Unicode text values will not be shown.\n");
        uconv = NULL;
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
        goto done;
    }
    printf("Font %s read successfully.\n", face->family_name );

    names = FT_Get_Sfnt_Name_Count( face );
    if ( names ) {

        printf("Names table contains %d entries:\n", names );
        for ( i = 0; i < names; i++ ) {
            FT_SfntName name;
            error = FT_Get_Sfnt_Name( face, i, &name );
            if ( error ) continue;
            printf("%2d: Plat %d, Enc %d, Lang 0x%X, ID %2d ",
                   i, name.platform_id, name.encoding_id, name.language_id, name.name_id );
            if ( name.platform_id == 1 && name.encoding_id == 0 && name.string_len < 100 ) {
                printf(" \"");
                PrintNameString( name.string, name.string_len );
                printf("\"\n");
            }
            else if ( name.platform_id == 3 && name.encoding_id == 1 && uconv && name.string_len < 200 ) {
                printf(" (U)\"");
                if (bDumpUCS)
                    DumpUnicodeString( name.string, name.string_len );
                else
                    PrintUnicodeString( name.string, name.string_len );
                printf("\"\n");
            }
            else if ( name.string_len < 200 && bDumpUCS ) {
                printf(" \"");
                DumpUnicodeString( name.string, name.string_len );
                printf("\"\n");
            }
            else
                printf(" [%d bytes]\n", name.string_len );
        }
    }

    printf("\nINCLUDED CMAPS");
    printf("\n--------------\n");
    for ( i = 0; i < face->num_charmaps; i++ ) {
        charmap = face->charmaps[i];
        switch ( charmap->platform_id ) {
            case 0:
                strcpy( szPID, "Unicode");
                switch ( charmap->encoding_id ) {
                    case 0:  strcpy( szEID, "default");   break;
                    case 1:  strcpy( szEID, "1.1");       break;
                    case 3:  strcpy( szEID, "2+");        break;
                    default: strcpy( szEID, "unknown");   break;
                }
                break;

            case 1:
                strcpy( szPID, "Macintosh");
                switch ( charmap->encoding_id ) {
                    case 0:  strcpy( szEID, "Roman");         break;
                    case 1:  strcpy( szEID, "Japanese");      break;
                    case 2:  strcpy( szEID, "Chinese-T");     break;
                    case 3:  strcpy( szEID, "Korean");        break;
                    case 8:  strcpy( szEID, "symbols");       break;
                    case 25: strcpy( szEID, "Chinese-S");     break;
                    default: strcpy( szEID, "other language");break;
                }
                break;

            case 3:
                strcpy( szPID, "Windows");
                switch ( charmap->encoding_id ) {
                    case 0:  strcpy( szEID, "symbols");           break;
                    case 1:  strcpy( szEID, "Unicode");           break;
                    case 2:  strcpy( szEID, "Shift-JIS (Japan)"); break;
                    case 3:  strcpy( szEID, "GB2312 (China)");    break;
                    case 4:  strcpy( szEID, "Big5 (Taiwan)");     break;
                    case 5:  strcpy( szEID, "Wansung (Korea)");   break;
                    case 6:  strcpy( szEID, "Johab (Korea)");     break;
                    case 10: strcpy( szEID, "UCS-4");             break;
                    default: strcpy( szEID, "unknown");           break;
                }
                break;

            default:
                strcpy( szPID, "unknown");
                strcpy( szEID, "unknown");
                break;
        }
        printf("Platform: %d (%s), Encoding: %d (%s)\n", charmap->platform_id, szPID,
                                                         charmap->encoding_id, szEID );
    }
#if 0
    error = FX_Flush_Stream( face );
    if ( error ) {
        printf("Failed to close stream: 0x%X\n", error );
        FT_Done_Face( face );
        goto done;
    }
    error = FX_Activate_Stream( face );
    if ( error ) {
        printf("Failed to reopen stream: 0x%X\n", error );
        FT_Done_Face( face );
        goto done;
    }
#endif

    if ( bShowKST ) {
        printf("\nKERNING INFORMATION");
        printf("\n-------------------\n");
        error = FX_Get_Kerning_Pairs( face, &kstable );
        switch ( error ) {
            case 0:
                printf("%u kerning pairs defined:\n", kstable->nPairs );
                if ( kstable->nPairs )
                {
                    for ( i = 0; i < kstable->nPairs; i++ ) {
                        printf("   %X / %X (%d)\n",
                               kstable->pairs[i].left,
                               kstable->pairs[i].right,
                               kstable->pairs[i].value );
                    }
                }
                safe_free( kstable );
                break;
            case FT_Err_Table_Missing:
                printf("No kerning table defined.\n");
                break;
            case FX_Err_Invalid_Kerning_Table:
                printf("The kerning table is invalid.\n");
                break;
            case FT_Err_Out_Of_Memory:
                printf("Memory allocation error.\n");
                break;
            case FX_Err_Invalid_Kerning_Table_Format:
                printf("No supported kerning table format found.\n");
                break;
            default: printf("An unknown error (0x%X) was encountered.\n", error );
                     break;
        }
    }

    if ( bShowOS2 ) {
        printf("\nOS/2 TABLE");
        printf("\n----------\n");

        pOS2 = (TT_OS2 *) FT_Get_Sfnt_Table( face, ft_sfnt_os2 );
        if ( pOS2 ) {
            printf("version:             %u\n", pOS2->version );
            printf("xAvgCharWidth:       %d\n", pOS2->xAvgCharWidth );
            printf("usWeightClass:       %u\n", pOS2->usWeightClass );
            printf("usWidthClass:        %u\n", pOS2->usWidthClass );
            printf("fsType:              0x%X\n", pOS2->fsType );
            printf("ySubscriptXSize:     %d\n", pOS2->ySubscriptXSize );
            printf("ySubscriptYSize:     %d\n", pOS2->ySubscriptYSize );
            printf("ySubscriptXOffset:   %d\n", pOS2->ySubscriptXOffset );
            printf("ySubscriptYOffset:   %d\n", pOS2->ySubscriptYOffset );
            printf("ySuperscriptXSize:   %d\n", pOS2->ySuperscriptXSize );
            printf("ySuperscriptYSize:   %d\n", pOS2->ySuperscriptYSize );
            printf("ySuperscriptXOffset: %d\n", pOS2->ySuperscriptXOffset );
            printf("ySuperscriptYOffset: %d\n", pOS2->ySuperscriptYOffset );
            printf("yStrikeoutSize:      %d\n", pOS2->yStrikeoutSize );
            printf("yStrikeoutPosition:  %d\n", pOS2->yStrikeoutPosition );
            printf("sFamilyClass:        %d\n", pOS2->sFamilyClass );
            printf("panose:              []\n");
            printf("ulUnicodeRange1:     0x%X\n", pOS2->ulUnicodeRange1 );
            printf("ulUnicodeRange2:     0x%X\n", pOS2->ulUnicodeRange2 );
            printf("ulUnicodeRange3:     0x%X\n", pOS2->ulUnicodeRange3 );
            printf("ulUnicodeRange4:     0x%X\n", pOS2->ulUnicodeRange4 );
            printf("achVendID:           %c%c%c%c\n", pOS2->achVendID[0], pOS2->achVendID[1], pOS2->achVendID[2], pOS2->achVendID[3] );
            printf("fsSelection:         0x%X\n", pOS2->fsSelection );
            printf("usFirstCharIndex:    %u\n", pOS2->usFirstCharIndex );
            printf("usLastCharIndex:     %u\n", pOS2->usLastCharIndex );
            printf("sTypoAscender:       %d\n", pOS2->sTypoAscender );
            printf("sTypoDescender:      %d\n", pOS2->sTypoDescender );
            printf("sTypoLineGap:        %d\n", pOS2->sTypoLineGap );
            printf("usWinAscent:         %u\n", pOS2->usWinAscent );
            printf("usWinDescent:        %u\n", pOS2->usWinDescent );
        }
        else printf("OS/2 table could not be located.\n");
    }

    if ( bShowPS) {
        printf("\nPOST TABLE");
        printf("\n----------\n");

        ppost = (TT_Postscript *) FT_Get_Sfnt_Table( face, ft_sfnt_post );
        if ( ppost ) {
            printf("FormatType:         0x%X\n", ppost->FormatType );
            printf("italicAngle:        %d\n", ppost->italicAngle );
            printf("underlinePosition:  %d\n", ppost->underlinePosition );
            printf("underlineThickness: %d\n", ppost->underlineThickness );
            printf("isFixedPitch:       %u\n", ppost->isFixedPitch );
            printf("minMemType42:       %u\n", ppost->minMemType42 );
            printf("maxMemType42:       %u\n", ppost->maxMemType42 );
            printf("minMemType1:        %u\n", ppost->minMemType1 );
            printf("maxMemType1:        %u\n", ppost->maxMemType1 );
        }
        else printf("POST table could not be loaded.\n");
    }

    error = FT_Done_Face( face );
done:
    FT_Done_FreeType( library );
    if ( uconv ) UniFreeUconvObject( uconv );
    return (int) error;
}

