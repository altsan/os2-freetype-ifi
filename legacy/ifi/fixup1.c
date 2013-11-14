From QueryFaces:

      if ( flFixStyle &&
          (( style = LookupName(pface->face, TT_NAME_ID_FONT_SUBFAMILY)) != NULL ))
      {
          LONG   cbfamily,
                 cbname;
          USHORT reported_style = 0;

          /* Some poorly-behaved fonts report each style (bold, italic, etc.)
           * as a separate family, and may even fail to set the style bits
           * properly.  This plays merry hell with word processors and similar applications.
           * The following bit of logic checks for & tries to correct this problem
           * for several standard face style names.
           */

          cbname = strlen(ifi.szFamilyname);
          cbfamily = cbname;

          if ( pOS2->fsSelection & TTFMETRICS_BOLD )
              reported_style &= TTFMETRICS_BOLD;
          if ( pOS2->fsSelection & TTFMETRICS_ITALIC )
              reported_style &= TTFMETRICS_ITALIC;

          if ( pOS2->fsSelection & TTFMETRICS_REGULAR )
          {
              if ( strcmp(ifi.szFamilyname+(cbname-8), " Regular") == 0 )
                  cbfamily -= 8;
              else if ( strcmp(style, "Bold") == 0 )
                  reported_style = TTFMETRICS_BOLD;
              else if (( strcmp(style, "Italic") == 0 ) ||
                       ( strcmp(style, "Oblique") == 0 ))
                  reported_style = TTFMETRICS_ITALIC;
              else if (( strcmp(style, "Bold Italic") == 0 ) ||
                       ( strcmp(style, "Bold Oblique") == 0 ))
                  reported_style = TTFMETRICS_BOLD | TTFMETRICS_ITALIC;
          }

          if (( reported_style == TTFMETRICS_BOLD ) &&
              ( strcmp(ifi.szFamilyname+(cbname-5), " Bold") == 0 ))
              cbfamily -= 5;
          else if (( reported_style == ( TTFMETRICS_BOLD | TTFMETRICS_ITALIC )) &&
                   ( strcmp(ifi.szFamilyname+(cbname-12), " Bold Italic") == 0 ))
              cbfamily -= 12;
          else if (( reported_style == ( TTFMETRICS_BOLD | TTFMETRICS_ITALIC )) &&
                   ( strcmp(ifi.szFamilyname+(cbname-13), " Bold Oblique") == 0 ))
              cbfamily -= 13;
          else if (( reported_style == TTFMETRICS_ITALIC ) &&
                   ( strcmp(ifi.szFamilyname+(cbname-7), " Italic") == 0 ))
              cbfamily -= 7;
          else if (( reported_style == TTFMETRICS_ITALIC ) &&
                   ( strcmp(ifi.szFamilyname+(cbname-8), " Oblique") == 0 ))
              cbfamily -= 8;

          //ifi.fsSelection |= reported_style;
          if (( flFixStyle & STYLE_FIX_FAMILY ) && ( cbfamily < cbname )) {
              ifi.szFamilyname[cbfamily] = '\0';
              fsTrunc |= IFIMETRICS_FAMTRUNC;
          }




#if 1
      /* The following looks to see if the face name is equal to the family
       * name plus " Regular" or " Roman"; if so, it strips said string out
       * of the face name.  Same reason as above.
       */
      if (( flFixStyle & STYLE_FIX_FACE ) &&
          ( pOS2->fsSelection & TTFMETRICS_REGULAR ))
      {
          LONG cbface = strlen(ifi.szFacename);
          LONG cbnew = cbface;
          if ( strcmp(ifi.szFacename+(cbface-8), " Regular") == 0 )
              cbnew -= 8;
          else if ( strcmp(ifi.szFacename+(cbface-6), " Roman") == 0 )
              cbnew -= 6;
          if (( cbnew < cbface ) &&
              ( strncmp(ifi.szFamilyname, ifi.szFacename, cbnew ) == 0 ))
          {
              COPY( " Adjusting facename: " ); CAT( ifi.szFacename );
              ifi.szFacename[cbnew] = '\0';
              COPY( "  --> " ); CAT( ifi.szFacename ); CAT("\r\n"); WRITE;
              fsTrunc |= IFIMETRICS_FACETRUNC;
          }
      }
#endif


