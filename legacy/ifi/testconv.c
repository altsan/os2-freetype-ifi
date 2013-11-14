#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uconv.h>

int main( int argc, char *argv[] )
{
    UconvObject UGLObj, DBCSObj;
    ULONG index, rc;
    char char_data[ 4 ],
         *pin_char_str;
    UniChar uni_buffer[ 4 ],
            *pout_uni_str;
    size_t in_bytes_left, uni_chars_left, num_subs;


    if ( argc < 2 ) index = 161;
    else if ( sscanf( argv[1], "%d", &index ) == 0 ) index = 161;

     /* These DBCS glyphlists require a bit of translation: while double-
      * byte values are the same as the corresponding font encoding, the
      * single-byte values are UGL indices.
      */
    if (index == 0x5C)
       index = 0xFE;
    else if (index == 0x7E)
       index = 0xFF;
    else if ((index > 31) && (index < 128))
       ;   /* no need to adjust ASCII characters; just fall through */
    else if (index < 950) {

        printf("Input character index = %d (0x%X)\n", index, index );

        if ( UniCreateUconvObject( (UniChar *) L"OS2UGL", &UGLObj ) != ULS_SUCCESS )
            return 0;
        if ( UniCreateUconvObject( (UniChar *) L"IBM-942", &DBCSObj ) != ULS_SUCCESS )
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
        printf("UniUconvToUcs = 0x%X\n", rc );

        /* perform the UCS -> SJIS/Wansung/Big-5/GB2312 conversion */
        if (rc == ULS_SUCCESS) {
            pout_uni_str = uni_buffer; /* input UCS string                  */
            pin_char_str = char_data;  /* actually the output string here   */
            in_bytes_left = 3;         /* actually no. of output characters */
            uni_chars_left = 1;
            num_subs = 0;
            rc = UniUconvFromUcs(DBCSObj, &pout_uni_str, &uni_chars_left,
                                 (void**)&pin_char_str, &in_bytes_left, &num_subs);
            printf("UniUconvFromUcs = 0x%X\n", rc );
            if ((rc == ULS_SUCCESS) && (num_subs == 0))
                index = char_data[1] ? ((short)char_data[0] << 8) | char_data[1] : char_data[0];
        }
        UniFreeUconvObject( UGLObj );
        UniFreeUconvObject( DBCSObj );
        printf("%s\n", char_data );
    }

    printf("Final character index = %d (0x%X)\n", index, index );
    return 0;
}

