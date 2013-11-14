#define INCL_DOSERRORS
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_DOSRESOURCES
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource.h"

// ----------------------------------------------------------------------------
// CONSTANTS

#define HELP_FILE               "ftconfig.hlp"

#define SZ_VERSION              "1.03"
#define SZ_COPYRIGHT            "(C) 2010-2012"

// Default settings values
#define US_CACHE_MIN            8       // minimum value for font face cache
#define US_CACHE_MAX            256     // maximum value for font face cache
#define US_CACHE_DEFAULT        12      // default value for font face cache

// Buffer limits
#define US_RES_MAXZ             256     // string resource buffer size

// Profile (INI) file entries
#define PRF_APP_GLOBAL          "FreeType IFI v2"
#define PRF_APP_PERFONT         "FreeType IFI v2:FontFlags"
#define PRF_KEY_MAXFILES        "OpenFaces"             // # of faces to cache
#define PRF_KEY_STYLEFIX        "Style_Fixup"           // use fake bold?
#define PRF_KEY_NETSCAPEFIX     "Use_Netscape_Fix"      // use Netscape fix?
#define PRF_KEY_FACENAMEALIAS   "Use_Facename_Alias"    // use TmsRmn alias?
#define PRF_KEY_FAKEBOLD        "Use_Fake_Bold"         // use fake bold?
#define PRF_KEY_FORCEUNICODE    "Use_Unicode_Encoding"  // always force Unicode?
#define PRF_KEY_FORCEUNIMBCS    "Set_Unicode_MBCS"      // always use MBCS flag?
#define PRF_KEY_EXCEPTASSOC     "Force_DBCS_Association"// no Unicode for assoc. font
#define PRF_KEY_EXCEPTCMB       "Force_DBCS_Combined"   // no Unicode for CMB alias
#define PRF_KEY_DPI             "DPI"                   // interface dpi
#define PRF_KEY_WINDOWPOS       "GUI"                   // settings for this GUI

#define PRF_APP_SYSTEMFONTS     "PM_SystemFonts"
#define PRF_KEY_ASSOCIATEFONT   "PM_AssociateFont"

#define PRF_APP_FONTDRIVERS     "PM_Font_Drivers"
#define PRF_KEY_TRUETYPE        "TRUETYPE"

#define PRF_APP_ABRFILES        "PM_ABRFiles"

// ----------------------------------------------------------------------------
// MACROS

// Set the reboot prompt control to visible
#define ShowChanged( hwnd ) \
    WinShowWindow( WinWindowFromID(hwnd, IDD_TXTREBOOT), TRUE );

// Set the reboot prompt control to invisible
#define ClearChanged( hwnd ) \
    WinShowWindow( WinWindowFromID(hwnd, IDD_TXTREBOOT), FALSE );

// Handy message box for errors and debug messages
#define ErrorPopup( text ) \
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, text, "Error", 0, MB_OK | MB_ERROR )

// Indicates codepages compatible with a given language
#define ISRUCODEPG( cp )    ( cp == 866 )
#define ISJPCODEPG( cp )    ( cp == 932 || cp == 942 || cp == 943 )
#define ISKOCODEPG( cp )    ( cp == 949 )
#define ISTWCODEPG( cp )    ( cp == 950 )
#define ISCNCODEPG( cp )    ( cp == 1381 || cp == 1386 )

// ----------------------------------------------------------------------------
// TYPEDEFS

typedef struct _Global_Data {
    HAB   hab;                  // anchor-block handle
    HMQ   hmq;                  // main message queue
    HWND  hwndMain,             // main window-handle
          hwndDriverPage,       // page window-handles
          hwndOptionsPage,      // ...
          hwndAdvancedPage,     // ...
          hwndAboutPage;        // ...
    BOOL  fInitDone,            // is program initialization complete?
          fFreeTypeActive,      // is FreeType/2 active?
          fNetscapeFix,         // current settings values
          fStyleFixup,          // ...
          fFacenameAlias,       // ...
          fFakeBold,            // ...
          fForceUnicode,        // ...
          fForceUniMBCS,        // ...
          fAssocDBCS,           // ...
          fCombinedDBCS;        // ...
    INT   iMaxFiles,            // ...
          iDPI;                 // ...
    ULONG iLangID;              // base message ID for current language
    CHAR  szDLL[ CCHMAXPATH ];  // path of FREETYPE.DLL
} FTCGLOBAL, *PFTCGLOBAL;


// ----------------------------------------------------------------------------
// FUNCTIONS

MRESULT EXPENTRY MainDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
void             CentreWindow( HWND hwnd );
BOOL             PopulateNotebook( HWND hwnd );
void             EnableTabs( HWND hwnd, BOOL fEnable );
MRESULT EXPENTRY DriverPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY OptionsPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY AdvancedPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY AboutPageProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
void             ReadSettings( HWND hwnd );
void             WriteSettings( HWND hwnd, PFTCGLOBAL pGlobal );
ULONG            SetLanguage( HMQ hmq );
BOOL             CheckFTDLL( PFTCGLOBAL pGlobal );

