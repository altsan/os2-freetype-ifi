/******************************************************************************
 * RESOURCE.H - Resource ID definitions                                       *
 ******************************************************************************/

// The main program window ID

#define ID_MAINPROGRAM              1


// Action IDs

#define ID_SAVE                     90
#define ID_CANCEL                   91
#define ID_RESET                    92
#define ID_HELP                     99


// String resource ID offsets (these will be added to the NLV base number)

#define IDS_PROGRAM_TITLE           10
#define IDS_PAGE_DRIVER             11
#define IDS_PAGE_SETTINGS           12
#define IDS_PAGE_ABOUT              13
#define IDS_PAGE_1OF2               14
#define IDS_PAGE_2OF2               15
#define IDS_TRUETYPEDLL_DESC        16
#define IDS_FREETYPEDLL_DESC        17
#define IDS_MESSAGE_REBOOT          18

#define IDS_ERRT_GEN                30
#define IDS_ERROR_NOTEBOOK          31
#define IDS_ERRT_NOTFOUND           32
#define IDS_ERROR_NOTFOUND          33

#define IDS_BTN_OK                  40
#define IDS_BTN_CANCEL              41
#define IDS_BTN_HELP                42
#define IDS_BTN_RESET               43

#define IDS_CTL_CURRENT_DRIVER      50
#define IDS_CTL_UNICODE_SETTINGS    51
#define IDS_CTL_FORCEUNICODE        52
#define IDS_CTL_FORCEMBCS           53
#define IDS_CTL_EXCEPTASSOC         54
#define IDS_CTL_EXCEPTCMB           55
#define IDS_CTL_OPTIONAL_FEATURES   56
#define IDS_CTL_FAKEBOLD            57
#define IDS_CTL_NETSCAPEFIX         58
#define IDS_CTL_FACENAMEALIAS       59
#define IDS_CTL_ADVANCED_SETTINGS   60
#define IDS_CTL_TXTCACHE            61
#define IDS_CTL_TXTRESOLUTION       62
#define IDS_CTL_TXTFONTS            63
#define IDS_CTL_TXTDPI              64
#define IDS_CTL_TXTVERSION          65
#define IDS_CTL_TXTCOPYRIGHT        66
#define IDS_CTL_STYLEFIX            67


// Now the NLV base numbers (these are all 100 * country code + 10000)

#define ID_BASE_EN                  10100       // English
#define ID_BASE_RU                  10700       // Russian
#define ID_BASE_NL                  13100       // Dutch
#define ID_BASE_FR                  13300       // French
#define ID_BASE_ES                  13400       // Spanish
#define ID_BASE_IT                  13900       // Italian
#define ID_BASE_SV                  14600       // Swedish
#define ID_BASE_DE                  14900       // German
#define ID_BASE_JA                  18100       // Japanese
#define ID_BASE_KO                  18200       // Korean
#define ID_BASE_CN                  18600       // Simplified Chinese
#define ID_BASE_TW                  18800       // Traditional Chinese


// Help subtable IDs

#define HSTB_MAIN                   20000
#define HSTB_INSTALL                20010
#define HSTB_SETTINGS1              20020
#define HSTB_SETTINGS2              20030


// Dialog resource IDs

#define IDD_NOTEBOOK                FID_CLIENT
#define IDD_SPLASH                  3
#define IDD_TXTREBOOT               110

#define IDD_INSTALL                 100
#define IDD_TXTDRIVER               101
#define IDD_DRIVER                  102

#define IDD_CONFIG                  200
#define IDD_GRPUNICODE              201
#define IDD_FORCEUNICODE            202
#define IDD_FORCEMBCS               203
#define IDD_EXCEPTASSOC             204
#define IDD_GRPOPTION               205
#define IDD_FAKEBOLD                206
#define IDD_NETSCAPEFIX             207
#define IDD_FACENAMEALIAS           208
#define IDD_EXCEPTCMB               209
#define IDD_STYLEFIX                210

#define IDD_ADVANCED                300
#define IDD_GRPADVANCED             301
#define IDD_TXTCACHE                302
#define IDD_MAXOPEN                 303
#define IDD_TXTRESOLUTION           304
#define IDD_INTERFACEDPI            305
#define IDD_TXTFONTS                306
#define IDD_TXTDPI                  307

#define IDD_ABOUT                   900
#define IDD_TXTPROGRAM              901
#define IDD_TXTCOPYRIGHT            902
#define IDD_TXTVERSION              903
#define IDD_TXTIFIVERSION           904

