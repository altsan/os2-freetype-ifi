#include "ft2conf.h"

/* ------------------------------------------------------------------------- *
 * ------------------------------------------------------------------------- */
int main( int argc, char *argv[] )
{
    FTCGLOBAL global;
    HAB       hab;                          // anchor block handle
    HMQ       hmq;                          // message queue handle
    HWND      hwndFrame,                    // window handle
              hwndHelp;                     // help instance
    QMSG      qmsg;                         // message queue
    HELPINIT  helpInit;                     // help init structure
    SWP       wp;                           // window position
    CHAR      szRes[ US_RES_MAXZ ],         // buffer for string resources
              szHelp[ US_HELP_MAXZ ],       // buffer for help file name
              szError[ US_RES_MAXZ ];       // buffer for error message
    BOOL      fInitFailure = FALSE;

//    memset( &global, 0, sizeof( FTCGLOBAL ));
    global.fInitDone = FALSE;

    hab = WinInitialize( 0 );
    if ( hab == NULLHANDLE ) {
        sprintf( szError, "WinInitialize() failed.");
        fInitFailure = TRUE;
    }

    if ( ! fInitFailure ) {
        hmq = WinCreateMsgQueue( hab, 0 );
        if ( hmq == NULLHANDLE ) {
            sprintf( szError, "Unable to create message queue:\nWinGetLastError() = 0x%X\n", WinGetLastError(hab) );
            fInitFailure = TRUE;
        }
    }

    if ( ! fInitFailure ) {
        global.hab       = hab;
        global.hmq       = hmq;
        global.iLangID   = SetLanguage( hmq );
        // if this language's resources don't exist, fall back to English
        if ( global.iLangID > ID_BASE_EN &&
             (!WinLoadString( global.hab, 0, global.iLangID + IDS_PROGRAM_TITLE,
                              US_RES_MAXZ, szRes )))
            global.iLangID   = ID_BASE_EN;

        hwndFrame = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP,
                                MainDlgProc, 0, ID_MAINPROGRAM, &global );
        if ( hwndFrame == NULLHANDLE ) {
            sprintf( szError, "Failed to load dialog resource:\nWinGetLastError() = 0x%X\n", WinGetLastError(hab) );
            fInitFailure = TRUE;
        }
    }

    if ( fInitFailure ) {
        WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, szError, "Program Initialization Error", 0, MB_CANCEL | MB_ERROR );
    } else {

        // Initialize online help
        SetHelpFile( global.iLangID, szHelp );
        if ( ! WinLoadString( hab, 0, global.iLangID + IDS_PROGRAM_TITLE, US_RES_MAXZ-1, szRes ))
            sprintf( szRes, "Help");

        helpInit.cb                       = sizeof( HELPINIT );
        helpInit.pszTutorialName          = NULL;
        helpInit.phtHelpTable             = (PHELPTABLE) MAKELONG( ID_MAINPROGRAM, 0xFFFF );
        helpInit.hmodHelpTableModule      = 0;
        helpInit.hmodAccelActionBarModule = 0;
        helpInit.fShowPanelId             = 0;
        helpInit.idAccelTable             = 0;
        helpInit.idActionBar              = 0;
        helpInit.pszHelpWindowTitle       = szRes;
        helpInit.pszHelpLibraryName       = szHelp;

        hwndHelp = WinCreateHelpInstance( hab, &helpInit );
        WinAssociateHelpInstance( hwndHelp, hwndFrame );

        // Now run the main program message loop
        global.hwndMain = hwndFrame;
        while ( WinGetMsg( hab, &qmsg, 0, 0, 0 )) WinDispatchMsg( hab, &qmsg );

        // Save the window position
        if ( WinQueryWindowPos( hwndFrame, &wp )) {
            ULONG ulCoordinates[ 4 ];
            ulCoordinates[0] = wp.x;
            ulCoordinates[1] = wp.y;
            ulCoordinates[2] = wp.cx;
            ulCoordinates[3] = wp.cy;
            PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_WINDOWPOS,
                                 ulCoordinates, sizeof(ulCoordinates) );
        }
    }

    // Clean up and exit
    WinDestroyWindow( hwndFrame );
    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    return ( 0 );
}


/* ------------------------------------------------------------------------- *
 * Window procedure for the main program dialog.                             *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY MainDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    static PFTCGLOBAL pGlobal;
    PSWP              swp;
    HPOINTER          hicon;
    CHAR              szRes[ US_RES_MAXZ ];
    PPAGESELECTNOTIFY psn;

    switch( msg ) {

        case WM_INITDLG:
            // Save the global data to a window word
            pGlobal = (PFTCGLOBAL) mp2;
            WinSetWindowPtr( hwnd, 0, pGlobal );

            // Load the current settings (must be before notebook is populated)
            ReadSettings( hwnd );

            // Set the window mini-icon
            hicon = WinLoadPointer( HWND_DESKTOP, 0, ID_MAINPROGRAM );
            WinSendMsg( hwnd, WM_SETICON, MPFROMP(hicon), MPVOID );

            // Set the UI text
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_PROGRAM_TITLE, US_RES_MAXZ, szRes ))
                strcpy( szRes, "FreeType/2 Configuration");
            WinSetDlgItemText( hwnd, FID_TITLEBAR, szRes );

            // Populate the notebook
            if ( ! PopulateNotebook( hwnd )) {
                ErrorPopup("Error populating notebook.");
                WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                return 0;
            }
            pGlobal->fInitDone = TRUE;
            if ( !pGlobal->fFreeTypeActive )
                EnableTabs( hwnd, FALSE );

            WinShowWindow( hwnd, TRUE );
            return (MRESULT) FALSE;


        case WM_CONTROL:
            switch( SHORT1FROMMP( mp1 )) {
                case IDD_NOTEBOOK:
                    if ( SHORT2FROMMP( mp1 ) == BKN_PAGESELECTEDPENDING ) {
                        psn = (PPAGESELECTNOTIFY) mp2;
                        if ( !pGlobal->fFreeTypeActive )
                            psn->ulPageIdNew = 0;
                    }
                    break;
                default: break;
            } // end of WM_CONTROL messages
            return (MRESULT) 0;


        case WM_MINMAXFRAME:
            swp = (PSWP) mp1;
            WinShowWindow( WinWindowFromID(hwnd, IDD_NOTEBOOK),
                           (swp->fl & SWP_MINIMIZE) ? FALSE : TRUE );
            break;


        case WM_CLOSE:
            WinPostMsg( hwnd, WM_QUIT, 0, 0 );
            return (MRESULT) 0;


    } /* end event handlers */

    return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * CentreWindow                                                              *
 *                                                                           *
 * Centres the given window on the screen.                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd: Handle of the window to be centred.                          *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void CentreWindow( HWND hwnd )
{
    LONG scr_width, scr_height;
    LONG x, y;
    SWP wp;

    scr_width = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    scr_height = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    if ( WinQueryWindowPos( hwnd, &wp )) {
        x = ( scr_width - wp.cx ) / 2;
        y = ( scr_height - wp.cy ) / 2;
        WinSetWindowPos( hwnd, HWND_TOP, x, y, wp.cx, wp.cy, SWP_MOVE | SWP_ACTIVATE );
    }

}


/* ------------------------------------------------------------------------- *
 * PopulateNotebook                                                          *
 *                                                                           *
 * Loads & adds all the page dialogs to the notebook control.                *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd: Handle of the window to be centred.                          *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE if all the notebook pages were added, FALSE if an error occurred.  *
 * ------------------------------------------------------------------------- */
BOOL PopulateNotebook( HWND hwnd )
{
    PFTCGLOBAL pGlobal;                 // global data
    ULONG pageId;
    HWND hwndBook, hwndPage;
    // NOTEBOOKBUTTON button;
    // NOTEBOOKBUTTON bookButtons[ 3 ];
    UCHAR szStatus[ US_RES_MAXZ ],
          szTab[ US_RES_MAXZ ];
          //szBtnOK[ US_RES_MAXZ ],
          //szBtnCancel[ US_RES_MAXZ ],
          //szBtnHelp[ US_RES_MAXZ ];
    USHORT flCommon = BKA_AUTOPAGESIZE | BKA_STATUSTEXTON;


    pGlobal = WinQueryWindowPtr( hwnd, 0 );

    hwndBook = WinWindowFromID( hwnd, IDD_NOTEBOOK );
    WinSendMsg( hwndBook, BKM_SETDIMENSIONS,
                MPFROM2SHORT( 75, 22 ), MPFROMSHORT( BKA_MAJORTAB ));
    WinSendMsg( hwndBook, BKM_SETDIMENSIONS,
                MPFROM2SHORT( 50, 20 ), MPFROMSHORT( BKA_PAGEBUTTON ));

    /* Common buttons
    // -- 2011-01-02  moved to individual page dialogs
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_BTN_OK, US_RES_MAXZ, szBtnOK ))
        strcpy( szBtnOK, "~OK");
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_BTN_CANCEL, US_RES_MAXZ, szBtnCancel ))
        strcpy( szBtnCancel, "~Cancel");
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_BTN_HELP, US_RES_MAXZ, szBtnHelp ))
        strcpy( szBtnHelp, "~Help");

    button.hImage = NULLHANDLE;
    button.flStyle = BS_NOTEBOOKBUTTON | BS_AUTOSIZE;
    button.pszText = szBtnOK;
    button.idButton = ID_SAVE;
    bookButtons[ 0 ] = button;
    button.pszText = szBtnCancel;
    button.idButton = ID_CANCEL;
    bookButtons[ 1 ] = button;
    button.pszText = szBtnHelp;
    button.idButton = ID_HELP;
    button.flStyle  = BS_HELP;
    bookButtons[ 2 ] = button;
    WinSendMsg( hwndBook, BKM_SETNOTEBOOKBUTTONS, MPFROMLONG( 3 ), MPFROMP( bookButtons ));
    */

    // Driver page
    hwndPage = WinLoadDlg( hwndBook, hwndBook, DriverPageProc, 0, IDD_INSTALL, pGlobal );
    pageId = (LONG) WinSendMsg( hwndBook, BKM_INSERTPAGE, NULL,
                                MPFROM2SHORT( flCommon | BKA_MAJOR, BKA_FIRST ));
    if ( !pageId ) return FALSE;
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_PAGE_DRIVER,
                         US_RES_MAXZ, szTab ))
        strcpy( szTab, "~Driver");
    if ( !WinSendMsg( hwndBook, BKM_SETTABTEXT,
                      MPFROMLONG( pageId ), MPFROMP( szTab )))
        return FALSE;
    if ( !WinSendMsg( hwndBook, BKM_SETPAGEWINDOWHWND,
                      MPFROMLONG(pageId), MPFROMP(hwndPage) ))
        return FALSE;
    pGlobal->hwndDriverPage = hwndPage;

    // Options page
    hwndPage = WinLoadDlg( hwndBook, hwndBook, OptionsPageProc, 0, IDD_CONFIG, pGlobal );
    pageId = (LONG) WinSendMsg( hwndBook, BKM_INSERTPAGE, NULL,
                                MPFROM2SHORT( flCommon | BKA_MAJOR | BKA_MINOR,
                                              BKA_LAST ));
    if ( !pageId ) return FALSE;
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_PAGE_1OF2,
                         US_RES_MAXZ, szStatus ))
        strcpy( szStatus, "Page 1 of 2");
    WinSendMsg( hwndBook, BKM_SETSTATUSLINETEXT,
                MPFROMLONG( pageId ), MPFROMP( szStatus ));
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_PAGE_SETTINGS,
                         US_RES_MAXZ, szTab ))
        strcpy( szTab, "~Settings");
    if ( !WinSendMsg( hwndBook, BKM_SETTABTEXT,
                      MPFROMLONG( pageId ), MPFROMP( szTab )))
        return FALSE;
    if ( !WinSendMsg( hwndBook, BKM_SETPAGEWINDOWHWND,
                      MPFROMLONG( pageId ), MPFROMP( hwndPage )))
        return FALSE;
    pGlobal->hwndOptionsPage = hwndPage;

    // Advanced options page
    hwndPage = WinLoadDlg( hwndBook, hwndBook, AdvancedPageProc, 0, IDD_ADVANCED, pGlobal );
    pageId = (LONG) WinSendMsg( hwndBook, BKM_INSERTPAGE, NULL,
                                MPFROM2SHORT( flCommon, BKA_LAST ));
    if ( !pageId ) return FALSE;
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_PAGE_2OF2,
                         US_RES_MAXZ, szStatus ))
        strcpy( szStatus, "Page 2 of 2");
    WinSendMsg( hwndBook, BKM_SETSTATUSLINETEXT,
                MPFROMLONG( pageId ), MPFROMP( szStatus ));
    if ( !WinSendMsg( hwndBook, BKM_SETPAGEWINDOWHWND,
                      MPFROMLONG( pageId ), MPFROMP( hwndPage )))
        return FALSE;
    pGlobal->hwndAdvancedPage = hwndPage;

    // About page
    hwndPage = WinLoadDlg( hwndBook, hwndBook, AboutPageProc, 0, IDD_ABOUT, pGlobal );
    pageId = (LONG) WinSendMsg( hwndBook, BKM_INSERTPAGE, NULL,
                                MPFROM2SHORT( flCommon | BKA_MAJOR, BKA_LAST ));
    if ( !pageId ) return FALSE;
    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_PAGE_ABOUT,
                         US_RES_MAXZ, szTab ))
        strcpy( szTab, "~About");
    if ( !WinSendMsg( hwndBook, BKM_SETTABTEXT,
                      MPFROMLONG( pageId ), MPFROMP( szTab )))
        return FALSE;
    if ( !WinSendMsg( hwndBook, BKM_SETPAGEWINDOWHWND,
                      MPFROMLONG( pageId ), MPFROMP( hwndPage )))
        return FALSE;
    pGlobal->hwndAboutPage = hwndPage;

    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * EnableTabs                                                                *
 *                                                                           *
 * Changes all notebook tabs after the first one to give them an appropriate *
 * enabled/disabled appearance.                                              *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd   : Handle of the main program window.                        *
 *   BOOL fEnable: TRUE = enable, FALSE = disable                            *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void EnableTabs( HWND hwnd, BOOL fEnable )
{
    ULONG i, ulColour, ulPage = 0;

    ulColour = fEnable? BKA_AUTOCOLOR: 0xCCCCCC;

    ulPage = (ULONG) WinSendDlgItemMsg( hwnd, IDD_NOTEBOOK, BKM_QUERYPAGEID,
                                        MPVOID, MPFROM2SHORT( BKA_FIRST, 0 ));
    if ( !ulPage || ulPage == BOOKERR_INVALID_PARAMETERS )
        return;
    for ( i = 0; i < 2; i++ ) {
        ulPage = (ULONG) WinSendDlgItemMsg( hwnd, IDD_NOTEBOOK, BKM_QUERYPAGEID,
                                            MPFROMLONG( ulPage ),
                                            MPFROM2SHORT( BKA_NEXT, BKA_MAJOR ));
        if ( !ulPage || ulPage == BOOKERR_INVALID_PARAMETERS ) break;
        WinSendDlgItemMsg( hwnd, IDD_NOTEBOOK, BKM_SETTABCOLOR,
                           MPFROMLONG( ulPage ), MPFROMLONG( ulColour ));
    }
    WinSendDlgItemMsg( hwnd, IDD_NOTEBOOK, BKM_SETNOTEBOOKCOLORS,
                       MPFROMLONG( fEnable? CLR_BLACK: CLR_DARKGRAY ),
                       MPFROMSHORT( BKA_FOREGROUNDMAJORCOLORINDEX ));
}


/* ------------------------------------------------------------------------- *
 * Window procedure for "Driver" page.                                       *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY DriverPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    static PFTCGLOBAL pGlobal;
    static BOOL fFTStartedActive;
    CHAR szRes[ US_RES_MAXZ ];

    switch( msg ) {
        case WM_INITDLG:
            pGlobal = (PFTCGLOBAL) mp2;

            // Set the UI text
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_MESSAGE_REBOOT,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Changes will take effect when the system is rebooted.");
            WinSetDlgItemText( hwnd, IDD_TXTREBOOT, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_CTL_CURRENT_DRIVER,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Current TrueType driver:");
            WinSetDlgItemText( hwnd, IDD_TXTDRIVER, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_TRUETYPEDLL_DESC,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "TRUETYPE.DLL - IBM TrueType IFI");
            WinSendDlgItemMsg( hwnd, IDD_DRIVER, LM_INSERTITEM,
                               MPFROMSHORT( LIT_END ), MPFROMP( szRes ));
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_FREETYPEDLL_DESC,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "FREETYPE.DLL - FreeType IFI");
            WinSendDlgItemMsg( hwnd, IDD_DRIVER, LM_INSERTITEM,
                               MPFROMSHORT( LIT_END ), MPFROMP( szRes ));

            // (2011-01-02 notebook buttons)
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_OK,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~OK");
            WinSetDlgItemText( hwnd, ID_SAVE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_CANCEL,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Cancel");
            WinSetDlgItemText( hwnd, ID_CANCEL, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_RESET,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Undo");
            WinSetDlgItemText( hwnd, ID_RESET, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_HELP,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Help");
            WinSetDlgItemText( hwnd, ID_HELP, szRes );

            // Select the current setting
            fFTStartedActive = pGlobal->fFreeTypeActive;
            if ( pGlobal->fFreeTypeActive ) {
                WinSendDlgItemMsg( hwnd, IDD_DRIVER, LM_SELECTITEM,
                                   MPFROMSHORT( pGlobal->fFreeTypeActive ), MPFROMSHORT( TRUE ));
            }
            else if ( strcmpi("TRUETYPE.DLL", pGlobal->szDriver )) {
                WinSendDlgItemMsg( hwnd, IDD_DRIVER, LM_INSERTITEM,
                                   MPFROMSHORT( LIT_END ), MPFROMP( pGlobal->szDriver ));
                WinSendDlgItemMsg( hwnd, IDD_DRIVER, LM_SELECTITEM,
                                   MPFROMSHORT( 2 ), MPFROMSHORT( TRUE ));
            }
            UpdateVersionDisplay( hwnd, pGlobal );
            //WinEnableControl( hwnd, ID_RESET, FALSE );
            return (MRESULT) TRUE;


        case WM_COMMAND:
            switch( SHORT1FROMMP( mp1 )) {

                case ID_RESET:
                    // Restore the setting from startup
                    if ( pGlobal->fFreeTypeActive != fFTStartedActive ) {
                        pGlobal->fFreeTypeActive = fFTStartedActive;
                        WinSendDlgItemMsg( hwnd, IDD_DRIVER, LM_SELECTITEM,
                                           MPFROMSHORT( pGlobal->fFreeTypeActive ), MPFROMSHORT( TRUE ));
                        EnableTabs( pGlobal->hwndMain, pGlobal->fFreeTypeActive );
                    }
                    ClearChanged( hwnd );
                    //WinEnableControl( hwnd, ID_RESET, FALSE );
                    break;

                // common buttons
                case ID_CANCEL:
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    break;

                case ID_SAVE:
                    WriteSettings( hwnd, pGlobal );
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    return (MRESULT) 0;

                default: break;
            }
            // end of WM_COMMAND messages
            return (MRESULT) 0;


        case WM_CONTROL:
            switch( SHORT1FROMMP( mp1 )) {

                case IDD_DRIVER:
                    if ( SHORT2FROMMP( mp1 ) == LN_SELECT ) {
                        ULONG ulSel = (ULONG) WinSendDlgItemMsg( hwnd, IDD_DRIVER,
                                                                 LM_QUERYSELECTION,
                                                                 MPFROMSHORT( LIT_CURSOR ), 0 );
                        if ( ulSel == pGlobal->fFreeTypeActive )
                            break;
                        if ( ulSel != 1 )
                            pGlobal->fFreeTypeActive = FALSE;
                        else {
                            if ( CheckFTDLL( pGlobal ))
                                pGlobal->fFreeTypeActive = TRUE;
                            else
                                WinPostMsg( WinWindowFromID(hwnd, IDD_DRIVER), LM_SELECTITEM,
                                            MPFROMSHORT( 0 ), MPFROMSHORT( TRUE ));
                        }
                        if ( pGlobal->fInitDone ) {
                            UpdateVersionDisplay( hwnd, pGlobal );
                            EnableTabs( pGlobal->hwndMain, pGlobal->fFreeTypeActive );
                            ShowChanged( hwnd );
                            //WinEnableControl( hwnd, ID_RESET, TRUE );
                        }
                    }
                    break;

                default: break;
            }
            // end of WM_CONTROL messages
            return (MRESULT) 0;

        default: break;
    }

    return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * Window procedure for "Settings" page.                                     *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY OptionsPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    static PFTCGLOBAL pGlobal;
    CHAR szRes[ US_RES_MAXZ ];

    switch( msg ) {
        case WM_INITDLG:
            pGlobal = (PFTCGLOBAL) mp2;

            // Set the UI text
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_MESSAGE_REBOOT,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Changes will take effect when the system is rebooted.");
            WinSetDlgItemText( hwnd, IDD_TXTREBOOT, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_UNICODE_SETTINGS,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Unicode Settings");
            WinSetDlgItemText( hwnd, IDD_GRPUNICODE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_FORCEUNICODE,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Always use Unicode");
            WinSetDlgItemText( hwnd, IDD_FORCEUNICODE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_FORCEMBCS,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Always report as MBCS-compatible");
            WinSetDlgItemText( hwnd, IDD_FORCEMBCS, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_EXCEPTASSOC,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Disable Unicode for association font");
            WinSetDlgItemText( hwnd, IDD_EXCEPTASSOC, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_EXCEPTCMB,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Disable Unicode for combined fonts");
            WinSetDlgItemText( hwnd, IDD_EXCEPTCMB, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_OPTIONAL_FEATURES,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Optional Features");
            WinSetDlgItemText( hwnd, IDD_GRPOPTION, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_STYLEFIX,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Use style name fix");
            WinSetDlgItemText( hwnd, IDD_STYLEFIX, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_FAKEBOLD,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Use fake bold");
            WinSetDlgItemText( hwnd, IDD_FAKEBOLD, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_NETSCAPEFIX,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Use Netscape fix");
            WinSetDlgItemText( hwnd, IDD_NETSCAPEFIX, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_FACENAMEALIAS,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Enable ""TmsRmn"" alias");
            WinSetDlgItemText( hwnd, IDD_FACENAMEALIAS, szRes );

            // (2011-01-02 notebook buttons)
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_OK,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~OK");
            WinSetDlgItemText( hwnd, ID_SAVE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_CANCEL,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Cancel");
            WinSetDlgItemText( hwnd, ID_CANCEL, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_RESET,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Undo");
            WinSetDlgItemText( hwnd, ID_RESET, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_HELP,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Help");
            WinSetDlgItemText( hwnd, ID_HELP, szRes );

            // Set the controls state according to the current settings
            WinSendDlgItemMsg( hwnd, IDD_FORCEUNICODE, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fForceUnicode), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_FORCEMBCS, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fForceUniMBCS), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_EXCEPTASSOC, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fAssocDBCS), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_EXCEPTCMB, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fCombinedDBCS), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_STYLEFIX, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fStyleFixup), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_FAKEBOLD, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fFakeBold), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_NETSCAPEFIX, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fNetscapeFix), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_FACENAMEALIAS, BM_SETCHECK,
                               MPFROMLONG(pGlobal->fFacenameAlias), MPVOID );

            //WinEnableControl( hwnd, ID_RESET, FALSE );
            return (MRESULT) TRUE;


        case WM_COMMAND:
            switch( SHORT1FROMMP( mp1 )) {

                case ID_RESET:
                    // Set the controls state according to the current settings
                    WinSendDlgItemMsg( hwnd, IDD_FORCEUNICODE, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fForceUnicode), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_FORCEMBCS, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fForceUniMBCS), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_EXCEPTASSOC, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fAssocDBCS), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_EXCEPTCMB, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fCombinedDBCS), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_STYLEFIX, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fStyleFixup), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_FAKEBOLD, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fFakeBold), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_NETSCAPEFIX, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fNetscapeFix), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_FACENAMEALIAS, BM_SETCHECK,
                                       MPFROMLONG(pGlobal->fFacenameAlias), MPVOID );
                    ClearChanged( hwnd );
                    //WinEnableControl( hwnd, ID_RESET, FALSE );
                    break;

                // common buttons
                case ID_CANCEL:
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    break;

                case ID_SAVE:
                    WriteSettings( hwnd, pGlobal );
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    return (MRESULT) 0;

                default: break;
            }
            // end of WM_COMMAND messages
            return (MRESULT) 0;


        case WM_CONTROL:
            switch( SHORT1FROMMP( mp1 )) {

                case IDD_FORCEUNICODE:
                case IDD_FORCEMBCS:
                case IDD_EXCEPTASSOC:
                case IDD_EXCEPTCMB:
                case IDD_STYLEFIX:
                case IDD_FAKEBOLD:
                case IDD_NETSCAPEFIX:
                case IDD_FACENAMEALIAS:
                    if ( pGlobal->fInitDone && SHORT2FROMMP( mp1 ) == BN_CLICKED ) {
                        ShowChanged( hwnd );
                        //WinEnableControl( hwnd, ID_RESET, TRUE );
                    }
                    break;

                default: break;
            }
            // end of WM_CONTROL messages
            return (MRESULT) 0;


        default: break;
    }

    return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * Window procedure for "Advanced Settings" page.                            *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY AdvancedPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    static PFTCGLOBAL pGlobal;
    CHAR szRes[ US_RES_MAXZ ];
    PSZ pszResolutions[] = {"72", "96", "120"};
    ULONG ulIdx;

    switch( msg ) {
        case WM_INITDLG:
            pGlobal = (PFTCGLOBAL) mp2;

            // Set the UI text
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_MESSAGE_REBOOT,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Changes will take effect when the system is rebooted.");
            WinSetDlgItemText( hwnd, IDD_TXTREBOOT, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_ADVANCED_SETTINGS,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Advanced Settings");
            WinSetDlgItemText( hwnd, IDD_GRPADVANCED, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTCACHE,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Open face limit:");
            WinSetDlgItemText( hwnd, IDD_TXTCACHE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTRESOLUTION,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Interface DPI:");
            WinSetDlgItemText( hwnd, IDD_TXTRESOLUTION, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTFONTS,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "fonts");
            WinSetDlgItemText( hwnd, IDD_TXTFONTS, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTDPI,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "dpi");
            WinSetDlgItemText( hwnd, IDD_TXTDPI, szRes );

            // (2011-01-02 notebook buttons)
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_OK,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~OK");
            WinSetDlgItemText( hwnd, ID_SAVE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_CANCEL,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Cancel");
            WinSetDlgItemText( hwnd, ID_CANCEL, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_RESET,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Undo");
            WinSetDlgItemText( hwnd, ID_RESET, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_HELP,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Help");
            WinSetDlgItemText( hwnd, ID_HELP, szRes );

            // Set the control values according to the current settings
            WinSendDlgItemMsg( hwnd, IDD_MAXOPEN, SPBM_SETLIMITS,
                               MPFROMLONG( US_CACHE_MAX ), MPFROMLONG( US_CACHE_MIN  ));
            WinSendDlgItemMsg( hwnd, IDD_MAXOPEN, SPBM_SETCURRENTVALUE,
                               MPFROMLONG( pGlobal->iMaxFiles ), MPVOID );
            WinSendDlgItemMsg( hwnd, IDD_INTERFACEDPI, SPBM_SETARRAY,
                               MPFROMP( pszResolutions ), MPFROMSHORT( 3 ));
            switch( pGlobal->iDPI ) {
                case 96:  ulIdx = 1; break;
                case 120: ulIdx = 2; break;
                default:  ulIdx = 0; break;
            }
            WinSendDlgItemMsg( hwnd, IDD_INTERFACEDPI, SPBM_SETCURRENTVALUE,
                               MPFROMLONG( ulIdx ), MPVOID );
            return (MRESULT) TRUE;


        case WM_COMMAND:
            switch( SHORT1FROMMP( mp1 )) {

                case ID_RESET:
                    // Reset the control values
                    WinSendDlgItemMsg( hwnd, IDD_MAXOPEN, SPBM_SETLIMITS,
                                       MPFROMLONG( US_CACHE_MAX ), MPFROMLONG( US_CACHE_MIN  ));
                    WinSendDlgItemMsg( hwnd, IDD_MAXOPEN, SPBM_SETCURRENTVALUE,
                                       MPFROMLONG( pGlobal->iMaxFiles ), MPVOID );
                    WinSendDlgItemMsg( hwnd, IDD_INTERFACEDPI, SPBM_SETARRAY,
                                       MPFROMP( pszResolutions ), MPFROMSHORT( 3 ));
                    switch( pGlobal->iDPI ) {
                        case 96:  ulIdx = 1; break;
                        case 120: ulIdx = 2; break;
                        default:  ulIdx = 0; break;
                    }
                    WinSendDlgItemMsg( hwnd, IDD_INTERFACEDPI, SPBM_SETCURRENTVALUE,
                                       MPFROMLONG( ulIdx ), MPVOID );
                    ClearChanged( hwnd );
                    //WinEnableWindow( WinWindowFromID( hwnd, ID_RESET ), FALSE );
                    break;

                // common buttons
                case ID_CANCEL:
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    break;

                case ID_SAVE:
                    WriteSettings( hwnd, pGlobal );
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    return (MRESULT) 0;

                default: break;
            }
            // end of WM_COMMAND messages
            return (MRESULT) 0;


        case WM_CONTROL:
            switch( SHORT1FROMMP( mp1 )) {

                case IDD_MAXOPEN:
                case IDD_INTERFACEDPI:
                    if ( pGlobal->fInitDone && SHORT2FROMMP( mp1 ) == SPBN_CHANGE ) {
                        ShowChanged( hwnd );
                        //WinEnableWindow( WinWindowFromID( hwnd, ID_RESET ), TRUE );
                    }
                    break;

                default: break;
            }
            // end of WM_CONTROL messages
            return (MRESULT) 0;

        default: break;
    }

    return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * Window procedure for "About" page.                                        *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY AboutPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    static PFTCGLOBAL pGlobal;
    CHAR szRes[ US_RES_MAXZ ],
         szBuf[ US_RES_MAXZ + 10 ];

    switch( msg ) {
        case WM_INITDLG:
            pGlobal = (PFTCGLOBAL) mp2;

            // Set the UI text
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_PROGRAM_TITLE, US_RES_MAXZ, szRes ))
                strcpy( szRes, "FreeType/2 Configuration");
            WinSetDlgItemText( hwnd, IDD_TXTPROGRAM, szRes );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTVERSION,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "Version %s");
            sprintf( szBuf, szRes, SZ_VERSION );
            WinSetDlgItemText( hwnd, IDD_TXTVERSION, szBuf );
            if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTCOPYRIGHT,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "%s Alex Taylor");
            sprintf( szBuf, szRes, SZ_COPYRIGHT );
            WinSetDlgItemText( hwnd, IDD_TXTCOPYRIGHT, szBuf );

            // (2011-01-02 notebook buttons)
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_OK,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~OK");
            WinSetDlgItemText( hwnd, ID_SAVE, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_CANCEL,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Cancel");
            WinSetDlgItemText( hwnd, ID_CANCEL, szRes );
            if ( !WinLoadString( pGlobal->hab, 0,
                                 pGlobal->iLangID + IDS_BTN_HELP,
                                 US_RES_MAXZ, szRes ))
                strcpy( szRes, "~Help");
            WinSetDlgItemText( hwnd, ID_HELP, szRes );

            return (MRESULT) TRUE;


        case WM_COMMAND:
            switch( SHORT1FROMMP( mp1 )) {

                // common buttons
                case ID_CANCEL:
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    break;

                case ID_SAVE:
                    WriteSettings( hwnd, pGlobal );
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    return (MRESULT) 0;

                default: break;
            }
            return (MRESULT) 0;

        default: break;
    }

    return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/* ------------------------------------------------------------------------- *
 * ReadSettings                                                              *
 *                                                                           *
 * Read the current settings from OS2.INI (if available).                    *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd: Handle of the main application window.                       *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void ReadSettings( HWND hwnd )
{
    PFTCGLOBAL pGlobal;
    ULONG      ulCoordinates[ 4 ] = {0},
               ulCount = 0;
    LONG       scr_width, scr_height;
    CHAR       szDllPath[ CCHMAXPATH ] = {0};

    pGlobal = WinQueryWindowPtr( hwnd, 0 );

    pGlobal->iMaxFiles = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                             PRF_KEY_MAXFILES, US_CACHE_DEFAULT );
    if ( pGlobal->iMaxFiles < US_CACHE_MIN || pGlobal->iMaxFiles > US_CACHE_MAX )
        pGlobal->iMaxFiles = US_CACHE_DEFAULT;

    pGlobal->iDPI = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                        PRF_KEY_DPI, 72 );
    if (( pGlobal->iDPI != 72 ) && ( pGlobal->iDPI != 96 ) && ( pGlobal->iDPI != 120 ))
        pGlobal->iDPI = 72;

    pGlobal->fNetscapeFix   = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_NETSCAPEFIX, FALSE );
    pGlobal->fFacenameAlias = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_FACENAMEALIAS, FALSE );
    pGlobal->fStyleFixup    = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_STYLEFIX, FALSE );
    pGlobal->fFakeBold      = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_FAKEBOLD, FALSE );
    pGlobal->fForceUnicode  = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_FORCEUNICODE, TRUE );
    pGlobal->fForceUniMBCS  = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_FORCEUNIMBCS, TRUE );
    pGlobal->fAssocDBCS     = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_EXCEPTASSOC, TRUE );
    pGlobal->fCombinedDBCS  = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP_FTIFI,
                                                  PRF_KEY_EXCEPTCMB, TRUE );

    pGlobal->fFreeTypeActive = FALSE;
    if ( PrfQueryProfileString( HINI_USERPROFILE, PRF_APP_FONTDRIVERS,
                                PRF_KEY_TRUETYPE, NULL, szDllPath, CCHMAXPATH-1 ) > 0 )
    {
        ULONG ulDrv;
        CHAR  *psz;

        if ( DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulDrv, sizeof(ULONG) ))
            ulDrv = 2;

        psz = strrchr( szDllPath, '\\');
        if ( !psz ) psz = szDllPath;
        else psz++;
        if ( strnicmp( psz, "FREETYPE.DLL", 12 ) == 0 ) {
            pGlobal->fFreeTypeActive = TRUE;
            if ( szDllPath[0] == '\\')
                sprintf( pGlobal->szDLL, "%c:%s", ulDrv+64, szDllPath );
            else
                strcpy( pGlobal->szDLL, szDllPath );
        } else
            sprintf( pGlobal->szDLL, "%c:\\OS2\\DLL\\FREETYPE.DLL", ulDrv+64 );
        strncpy( pGlobal->szDriver, psz, US_DLL_MAXZ-1 );
    }

    ulCount = sizeof( ulCoordinates );
    if ( PrfQueryProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_WINDOWPOS,
                              (PVOID) ulCoordinates, &ulCount ))
    {
        scr_width  = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
        scr_height = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
        if (( ulCoordinates[0] < scr_width ) &&
            ( ulCoordinates[1] < scr_height ) &&
            ( ulCoordinates[2] <= scr_width ) && ( ulCoordinates[2] > 20 ) &&
            ( ulCoordinates[3] <= scr_height ) && ( ulCoordinates[3] > 20 ))
        {
            if ( WinSetWindowPos( hwnd, 0, ulCoordinates[0], ulCoordinates[1],
                                  ulCoordinates[2], ulCoordinates[3],
                                  SWP_SIZE | SWP_MOVE | SWP_ACTIVATE ))
                return;
        }
    }

    CentreWindow( hwnd );

}


/* ------------------------------------------------------------------------- *
 * WriteSettings                                                             *
 *                                                                           *
 * Saves various settings to the INI file.                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd         : Main window handle.                                 *
 *   PFTCGLOBAL pGlobal: Pointer to global settings                          *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void WriteSettings( HWND hwnd, PFTCGLOBAL pGlobal )
{
    ULONG ulDrv;
    CHAR  szBuf[ 2 ],
          szDPI[ 4 ],
          szLimit[ 4 ],
          szPath[ CCHMAXPATH ];

    // Retrieve the current selections from each page
    pGlobal->fForceUnicode  = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_FORCEUNICODE  );
    pGlobal->fForceUniMBCS  = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_FORCEMBCS     );
    pGlobal->fAssocDBCS     = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_EXCEPTASSOC   );
    pGlobal->fCombinedDBCS  = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_EXCEPTCMB     );
    pGlobal->fNetscapeFix   = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_NETSCAPEFIX   );
    pGlobal->fFakeBold      = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_FAKEBOLD      );
    pGlobal->fFacenameAlias = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_FACENAMEALIAS );
    pGlobal->fStyleFixup    = WinQueryButtonCheckstate( pGlobal->hwndOptionsPage, IDD_STYLEFIX      );

    WinSendDlgItemMsg( pGlobal->hwndAdvancedPage, IDD_MAXOPEN, SPBM_QUERYVALUE,
                       MPFROMP( szLimit ),
                       MPFROM2SHORT( sizeof(szLimit), SPBQ_UPDATEIFVALID ));
    WinSendDlgItemMsg( pGlobal->hwndAdvancedPage, IDD_INTERFACEDPI, SPBM_QUERYVALUE,
                       MPFROMP( szDPI ),
                       MPFROM2SHORT( sizeof(szDPI), SPBQ_UPDATEIFVALID ));

    // Now save the settings to the user INI file
    if ( pGlobal->fFreeTypeActive ) {
        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP_FONTDRIVERS,
                               PRF_KEY_TRUETYPE, pGlobal->szDLL );
    } else {
        if ( DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulDrv, sizeof(ULONG) ))
            strcpy( szPath, "\\OS2\\DLL\\TRUETYPE.DLL");
        else
            sprintf( szPath, "%c:\\OS2\\DLL\\TRUETYPE.DLL", ulDrv+64 );
        PrfWriteProfileString( HINI_USERPROFILE, PRF_APP_FONTDRIVERS,
                               PRF_KEY_TRUETYPE, szPath );
    }
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_MAXFILES,
                         szLimit, strlen(szLimit)+1 );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_DPI,
                         szDPI, strlen(szDPI)+1 );
    sprintf( szBuf, "%u", pGlobal->fNetscapeFix );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_NETSCAPEFIX,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fFacenameAlias );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_FACENAMEALIAS,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fFakeBold );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_FAKEBOLD,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fForceUnicode );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_FORCEUNICODE,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fForceUniMBCS );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_FORCEUNIMBCS,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fAssocDBCS );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_EXCEPTASSOC,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fCombinedDBCS );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_EXCEPTCMB,
                         szBuf, strlen(szBuf)+1 );
    sprintf( szBuf, "%u", pGlobal->fStyleFixup );
    PrfWriteProfileData( HINI_USERPROFILE, PRF_APP_FTIFI, PRF_KEY_STYLEFIX,
                         szBuf, strlen(szBuf)+1 );
}


/* ------------------------------------------------------------------------- *
 * SetLanguage                                                               *
 *                                                                           *
 * Get the current base language ID (used for string resource lookup).       *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HMQ hmq                                                                 *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The calculated language ID.                                             *
 * ------------------------------------------------------------------------- */
ULONG SetLanguage( HMQ hmq )
{
    PSZ    pszEnv;
    USHORT usCC;
    ULONG  ulCP;

    pszEnv = getenv("LANG");
    if ( !pszEnv ) return ID_BASE_EN;

    ulCP = WinQueryCp( hmq );

    if ( strnicmp(pszEnv, "EN_", 3 ) == 0 ) usCC = 1;
    // You can comment out a country's statement to use English for that language
    // Note: we only need to check the codepage for languages that don't use 850
    else if ( ISRUCODEPG(ulCP) && strnicmp(pszEnv, "RU_", 3 ) == 0 ) usCC = 7;
    else if ( strnicmp(pszEnv, "NL_", 3 ) == 0 ) usCC = 31;
    else if ( strnicmp(pszEnv, "FR_", 3 ) == 0 ) usCC = 33;
    else if ( strnicmp(pszEnv, "ES_", 3 ) == 0 ) usCC = 34;
    else if ( strnicmp(pszEnv, "IT_", 3 ) == 0 ) usCC = 39;
    else if ( strnicmp(pszEnv, "SV_", 3 ) == 0 ) usCC = 46;
    else if ( strnicmp(pszEnv, "DE_", 3 ) == 0 ) usCC = 49;
    else if ( ISJPCODEPG(ulCP) && strnicmp(pszEnv, "JA_", 3 ) == 0 ) usCC = 81;
    else if ( ISKOCODEPG(ulCP) && strnicmp(pszEnv, "KO_", 3 ) == 0 ) usCC = 82;
    else if ( ISCNCODEPG(ulCP) && (( strnicmp(pszEnv, "ZH_CN", 5 ) == 0 ) ||
                                   ( strnicmp(pszEnv, "ZH_SG", 5 ) == 0 ))) usCC = 86;
    else if ( ISTWCODEPG(ulCP) && (( strnicmp(pszEnv, "ZH_TW", 5 ) == 0 ) ||
                                   ( strnicmp(pszEnv, "ZH_HK", 5 ) == 0 ))) usCC = 88;
    else usCC = 1;

    return (usCC * 100) + 10000;
}


/* ------------------------------------------------------------------------- *
 * SetHelpFile                                                               *
 *                                                                           *
 * Determine the correct help file name for the active languge.  This will   *
 * "ftconfig.hlp" (HELP_FILE) for English; for other languages, the "ig" is  *
 * replaced by the two-letter language code, but only if that file exists.   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   ULONG ulID   : The language ID as returned by SetLanguage           (I) *
 *   PSZ   achHelp: Buffer for help file name, must be >= US_HELP_MAXZ   (O) *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void SetHelpFile( ULONG ulID, PSZ achHelp )
{
    UCHAR  achFound[ CCHMAXPATH ] = {0};
    ULONG  flSearch = SEARCH_IGNORENETERRS | SEARCH_ENVIRONMENT | SEARCH_CUR_DIRECTORY;
    APIRET rc;

    strncpy( achHelp, HELP_FILE, US_HELP_MAXZ-1 );
    switch ( ulID ) {
        case ID_BASE_RU: achHelp[6] = 'r'; achHelp[7] = 'u'; break;
        case ID_BASE_NL: achHelp[6] = 'n'; achHelp[7] = 'l'; break;
        case ID_BASE_FR: achHelp[6] = 'f'; achHelp[7] = 'r'; break;
        case ID_BASE_ES: achHelp[6] = 'e'; achHelp[7] = 's'; break;
        case ID_BASE_IT: achHelp[6] = 'i'; achHelp[7] = 't'; break;
        case ID_BASE_SV: achHelp[6] = 's'; achHelp[7] = 'v'; break;
        case ID_BASE_DE: achHelp[6] = 'd'; achHelp[7] = 'e'; break;
        case ID_BASE_JA: achHelp[6] = 'j'; achHelp[7] = 'a'; break;
        case ID_BASE_KO: achHelp[6] = 'k'; achHelp[7] = 'o'; break;
        case ID_BASE_CN: achHelp[6] = 'c'; achHelp[7] = 'n'; break;
        case ID_BASE_TW: achHelp[6] = 't'; achHelp[7] = 'w'; break;
        default:         return;     // includes EN
    }

    rc = DosSearchPath( flSearch, "HELP", achHelp, achFound, sizeof( achFound ));
    if ( rc != NO_ERROR ) {
        // Couldn't find that help file, revert to English
        achHelp[6] = 'i'; achHelp[7] = 'g';
    }
}


/* ------------------------------------------------------------------------- *
 * CheckFTDLL                                                                *
 *                                                                           *
 * Make sure that FREETYPE.DLL exists in the expected location.  If it does  *
 * not, prompt the user for the correct path.  (The new path provided by the *
 * user is not actually checked again, since the user selects it from a file *
 * dialog and it is therefore assumed that the file exists.)                 *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PFTCGLOBAL pGlobal: Pointer to global application data.                 *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE if a new path was provided                                         *
 *   FALSE if the user cancelled or some error occurred                      *
 * ------------------------------------------------------------------------- */
BOOL CheckFTDLL( PFTCGLOBAL pGlobal )
{
    FILEFINDBUF3 ffBuf3;
    FILEDLG      dialog;
    HDIR         hdir = HDIR_SYSTEM;
    CHAR         szRes1[ US_RES_MAXZ ],
                 szRes2[ US_RES_MAXZ ],
                 szBuf[ US_RES_MAXZ + CCHMAXPATH ];
    ULONG        ulFind = 1;

    if ( DosFindFirst( pGlobal->szDLL, &hdir, 0x0027, &ffBuf3,
                       sizeof(FILEFINDBUF3), &ulFind, FIL_STANDARD ))
    {
        // if we haven't finished creating the GUI yet, just return
        if ( !pGlobal->fInitDone ) return FALSE;

        // FREETYPE.DLL was not found, so prompt the user for it
        if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_ERROR_NOTFOUND, US_RES_MAXZ, szRes1 ))
            strcpy( szRes1, "%s not found.  Select manually?");
        sprintf( szBuf, szRes1, pGlobal->szDLL );
        if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_ERRT_NOTFOUND,  US_RES_MAXZ, szRes2 ))
            strcpy( szRes2, "Not Found");
        if ( WinMessageBox( HWND_DESKTOP, pGlobal->hwndMain, szBuf, szRes2, 0,
                            MB_YESNO | MB_WARNING | MB_MOVEABLE ) == MBID_YES )
        {
            memset( &dialog, 0, sizeof(FILEDLG) );
            dialog.cbSize = sizeof( FILEDLG );
            dialog.fl = FDS_OPEN_DIALOG | FDS_CENTER;
            dialog.pszTitle = NULL;
            sprintf( dialog.szFullFile, "*.DLL");
            if ( WinFileDlg( HWND_DESKTOP, pGlobal->hwndMain, &dialog ) &&
                 dialog.lReturn == DID_OK )
            {
                strcpy( pGlobal->szDLL, dialog.szFullFile );
                return TRUE;
            }
        }
        return FALSE;
    }
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * UpdateVersionDisplay                                                      *
 *                                                                           *
 * Update the IFI version field.                                             *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND       hwnd   : Handle to driver page window.                       *
 *   PFTCGLOBAL pGlobal: Pointer to global application data.                 *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void UpdateVersionDisplay( HWND hwnd, PFTCGLOBAL pGlobal )
{
    BLDLEVEL blinfo = {0};
    ULONG    ulDrv;
    CHAR     szBuf[ CCHMAXPATH ];
    CHAR     szRes[ US_RES_MAXZ ];

    if ( pGlobal->fFreeTypeActive )
        strcpy( szBuf, pGlobal->szDLL );
    else {
        if ( DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulDrv, sizeof(ULONG) ))
            sprintf( szBuf, "\\OS2\\DLL\\TRUETYPE.DLL");
        else
            sprintf( szBuf, "%c:\\OS2\\DLL\\TRUETYPE.DLL", ulDrv+64 );
    }

    if ( !WinLoadString( pGlobal->hab, 0, pGlobal->iLangID + IDS_CTL_TXTVERSION,
                         US_RES_MAXZ, szRes ))
        strcpy( szRes, "Version %s");

    if ( GetBldLevel( szBuf, &blinfo ))
        sprintf( szBuf, szRes, blinfo.szVersion );
    else
        sprintf( szBuf, "");
    WinSetDlgItemText( hwnd, IDD_TXTIFIVERSION, szBuf );
}

/* ------------------------------------------------------------------------- *
 * GetBldLevel                                                               *
 *                                                                           *
 * Get the BLDLEVEL signature from a file.                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PSZ       pszfile : Name of the file to query.                      (I) *
 *   PBLDLEVEL pInfo   : Pointer to BLDLEVEL information structure.      (O) *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE if the signature was read successfully.                            *
 *   FALSE if the signature could not be read.                               *
 * ------------------------------------------------------------------------- */
BOOL GetBldLevel( PSZ pszFile, PBLDLEVEL pInfo )
{
    CHAR achSig[ US_BLSIG_MAXZ ] = {0};
    HPIPE hpR, hpW;
    HFILE hfSave = -1,
          hfNew  = HF_STDOUT;
    ULONG ulRead = 0,
          ulRC;
    PSZ   psz, tok;

    if ( !pszFile || !pInfo ) return FALSE;

    DosDupHandle( HF_STDOUT, &hfSave ); // save handle to STDOUT
    DosCreatePipe( &hpR, &hpW, 0 );     // create a new pipe with R/W handles
    DosDupHandle( hpW, &hfNew );        // redirect STDOUT to our pipe

    ulRC = spawnlp( P_WAIT, "bldlevel", "bldlevel", pszFile, NULL );

    DosClose( hpW );
    DosDupHandle( hfSave, &hfNew );     // restore STDOUT
    DosRead( hpR, achSig, US_BLSIG_MAXZ, &ulRead ); // get our redirected output (1 char)
    DosClose( hpR );
    if ( ulRC != 0 ) return FALSE;

    // Find the start of the actual signature
    psz = strpbrk( achSig, "@#");
    if ( !psz || ( strlen( psz ) < 3 )) return FALSE;
    psz += 2;

    if (( tok = strchr( psz, '\r')) != NULL ) *tok = 0;

    // Parse the vendor name
    tok = strtok( psz, ":");
    strncpy( pInfo->szVendor, psz, 31 );

    // Parse the version string
    tok = strtok( NULL, "#");
    if ( !tok ) return TRUE;
    strncpy( pInfo->szVersion, tok, 10 );

#if 0
    if ( tok ) tok = strtok( NULL, "#");
    if ( tok ) tok = strtok( NULL, "#");
    if ( tok ) tok = strtok( NULL, "#");
    if ( tok ) {
        strncpy( pInfo->szTime, tok, 26 );
    }
#endif

    return TRUE;
}

