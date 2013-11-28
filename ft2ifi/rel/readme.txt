FREETYPE/2-IFI
INTELLIGENT FONT INTERFACE DRIVER FOR PRESENTATION MANAGER 
Version 2.0.0 public beta 1

  (C) 2013 Alexander Taylor
  (C) 1997-2013 The FreeType Project

  Based on previous work by Michal Necasek (C) 1997-2000

  And including additional work by:
   - Seo, Hyun-Tae (C) 2003-2004 
   - KO Myung-Hun (C) 2002-2007 


  FreeType/2-IFI is an OS/2 Presentation Manager font driver that provides
  system-wide support for TrueType and OpenType fonts.  It is designed to
  replace OS/2's built-in TRUETYPE.DLL driver, and uses the open source
  FreeType library (version 2.x) for font rendering.  See below for more
  details.

  This is version 2.0 of FreeType/2 IFI, which represents a major change from 
  previous releases.  The old FreeType v1 engine has been completely replaced 
  in favour of the (radically different) FreeType v2 library.  This change
  necessitated heavily rewriting large sections of the driver.

  Version 1.x of FreeType/2 was originally written by Michal Necasek, and 
  quite a lot of his code is still present, in modified form.  More recent
  releases were maintained by KO Myung-hun (v1.2) and myself (v1.3).


INTRODUCTION

  What This Is
  ------------

  FreeType/2-IFI is a system-wide font 'driver' or support module (something
  like a plug-in) for Presentation Manager, which in OS/2 terminology is
  technically known as an 'Intelligent Font Interface' driver.  

  What it actually does is enhance OS/2's own native font support in two main
  ways, when compared to the default, out-of-the-box TrueType driver provided
  by IBM.  First, it enables the use of the more modern OpenType fonts (OTFs),
  whereas the IBM driver supported only basic TrueType fonts (TTFs).  Second,
  even with fonts which were supported by the IBM driver, it improves the
  appearance of on-screen text.
  
  Because FreeType/2-IFI is written as a special Presentation Manager driver,
  all graphical programs which rely on Presentation Manager (the OS/2
  graphical environment) for rendering text will benefit from these
  improvements.

  What This Is NOT
  ----------------
  FreeType/2-IFI should not be confused with the FreeType library itself.  It
  uses FreeType, as mentioned, but the two are not the same by any means.*

  To explain: the FreeType library itself is basically just a set of routines
  for drawing characters in a particular font.  In order to draw properly
  laid-out text (with support for such concepts as lines and paragraphs, flow
  and direction, styles and formatting, etc.), a higher-level text layout 
  engine is required.  OS/2 Presentation Manager (specifically, the GPI
  layer) includes such an engine.  FreeType/2-IFI is a special driver which 
  interfaces the drawing functions from the FreeType library with the layout
  functions of Presentation Manager.

  There are some applications, such as Mozilla, OpenOffice, or the QT4 
  toolkit, which use their own entirely different layout engines (which have
  names like Cairo or Pango).  Many of these also happen to use a version of
  the FreeType library (which they provide themselves).  They generally do
  this specifically to avoid using Presentation Manager for rendering text,
  which allows them to offer certain additional features.  FreeType/2-IFI
  has no effect on how such applications render text...  HOWEVER, it should
  allow them to make use of OpenType fonts which you install on your system--
  since these programs generally still rely on Presentation Manager to report
  what fonts are available.  So programs like OpenOffice and Mozilla should
  also be able to benefit from the new OpenType font support.

  * It should be noted that the version of FreeType used by FreeType/2-IFI
    is a customized one (built with special options to allow it to pass data
    to and from Presentation Manager) which has been compiled into the driver
    itself.  FreeType/2-IFI neither knows nor cares about any other version
    of FreeType which may be present on the system.


NEW IN VERSION 2

  As mentioned above, FreeType/2-IFI now uses version 2 of the open-source
  FreeType library (which is so different from version 1 that it can't really
  be called the same product).

  The main advantage of version 2 is that OpenType fonts are now supported.

  OpenType fonts are essentially an extended version of the TrueType font
  format.  There are various technical enhancements, but the biggest
  difference is that an OpenType font can contain glyph data in either
  TrueType or PostScript format.  As such, there are actually two varieties
  of OpenType font: OpenType with TrueType data (or OpenType/TT), and
  OpenType with PostScript data (called OpenType/PS or OpenType/CFF).

  I should note that the old FreeType/2 driver actually supported OpenType/TT
  fonts already.  In fact, an OpenType/TT font is really just a normal
  TrueType font with some optional extra data.  OpenType/TT fonts usually 
  have the file extension '.TTF'; indeed, most 'TrueType' fonts out there
  nowadays are really OpenType/TT fonts.

  The big difference is in the other format, OpenType/PS.  These fonts
  usually have the extension '.OTF', and they are not compatible with version
  1.x of FreeType/2, or with the original IBM TrueType driver.  This type of
  font is becoming increasingly prevalent nowadays.  Many (if not most)
  commercial fonts from the big foundries like Adobe, Monotype, etc. were
  originally designed as Adobe Type 1 fonts, meaning they were in PostScript
  format already; when it came time to upgrade them it was both simpler and
  more technically sound to convert them to OpenType/PS files.  So when you
  go to buy a high-quality commercial font nowadays, you will generally find
  them offered in OTF format, sometimes exclusively so.

  Anyway, starting with this release of FreeType/2-IFI, both OpenType font
  formats should now be supported equally well! 


LIMITATIONS & KNOWN PROBLEMS

  THIS IS A BETA RELEASE.  It has only been tested in a limited number of
  environments, and may have undesirable side effects.  You are strongly
  advised not to install this release on a production system.

  No configuration GUI is yet available for this version.  (Note that FT2IFI
  will ignore the OS2.INI settings used by the older FREETYPE.DLL.) 

  At present, Unicode encoding is always turned on for fonts which support
  it, unless a more appropriate DBCS encoding is found (and excluding DBCS 
  'combined' fonts as well as any font used for PM glyph substitution).  The
  MBCS flag is also always set for Unicode fonts.  At least some of these
  behaviours will eventually be configurable, similar to version 1.3x.

  Kerning support is currently disabled under native PM rendering, due to
  changes in the FreeType v2 engine which require extensive work to accomodate.
  (It should be noted that the original IBM TrueType driver does not appear to
  have supported kerning either.)

  Fonts from version 5.x of the 'Linux Libertine' collection do not display
  properly in Presentation Manager (in either TTF or OTF format).  I have not
  yet determined why this is.  Versions 4.75 and older do appear to work.

  OpenType-specific layout tables (e.g. contextual alternates, advanced 
  positioning, etc.) are not currently supported (and may never be); this is
  mainly because the FreeType library itself does not support them.  In any
  case, features such as these generally have to be implemented at the
  application level.

  While you can, as always, install font files to any arbitrary location you
  want, you should avoid installing them on a FAT32 drive.  The OS/2 FAT32
  driver has poor performance in a number of ways which makes loading and
  parsing font files quite sluggish.

  FreeType/2-IFI only replaces the TrueType support module within Presentation
  Manager.  It cannot circumvent any of the limitations inherent in PM's 
  graphics layer (GRE/GPI).  In particular, there is no support for anti-
  aliased fonts, or for font names longer than 31 characters (both of which 
  are restrictions imposed by PM, and not by the font driver).



INSTALLATION

  ** Before installing, you are strongly advised to perform an archive of 
     your current desktop.  (Go into Desktop Properties, 'Archive' page, and
     enable archiving.  Then reboot.  Go back into Desktop Properties and 
     turn archiving off again).  It is possible, though hopefully unlikely,
     that your system might not boot with FT2IFI enabled.  This way you can
     disable it by reverting to your archived configuration if necessary.

  Run the script INSTALL.CMD from the directory where you unpacked FT2IFI.DLL.
  This script will copy FT2IFI.DLL to your \OS2\DLL directory and modify the
  OS/2 user profile (OS2.INI) to use it instead of either TRUETYPE.DLL or 
  FREETYPE.DLL.  (Neither of those older files will be physically deleted
  from the \OS2\DLL directory.)


FACE NAME NORMALIZATION FIX FOR OPENOFFICE

  This version of FreeType/2 IFI includes an optional workaround for 
  OpenOffice.org printing problems caused by problems with non-standard font
  face names (the same as in FREETYPE.DLL version 1.33 and up).  It can be
  enabled by defining the key "Style_Fixup" with a string value of "1" in 
  OS2.INI under the application "FreeType IFI v2".

  If enabled, the driver will attempt to apply standardized naming conventions
  to the four most common font "styles": Normal, Bold, Italic (or Oblique),
  and Bold Italic (or Bold Oblique).  It does this by using a concatenation of
  the OpenType Family and Subfamily names in place of the Face name, for fonts
  where the Subfamily name is one of those just listed (or Medium [Italic/
  Oblique], which it tries to adjust to Normal or Bold if possible).  


UNINSTALLATION

  Run the included script UNINST.CMD.  This script will modify the OS/2 user
  profile (OS2.INI) to disable FT2IFI.DLL; if either TRUETYPE.DLL (the original
  IBM driver) or FREETYPE.DLL (version 1.x of FreeType/2) is found, one of 
  those will be enabled instead.  (If both exist, you will be prompted to 
  choose one.)

  FT2IFI.DLL will not be deleted from the \OS2\DLL directory; you may wish to
  do so yourself.


NOTICES

  FreeType/2 Intelligent Font Interface Driver Version 2
  (C) 2013 Alexander Taylor
  (C) 1997-2000 Michal Necasek 

  Also includes work by Seo Hyun-Tae, (C) 2003-2004; and KO Myung-Hun,
  (C) 2002-2007 

  Portions of this software are copyright © 2013 The FreeType Project
  (www.freetype.org).  All rights reserved.

  This software is licensed under the terms of the FreeType Project License.
  See the file FTL.TXT for details.


-- 
Alexander Taylor 
alex at altsan dot org
November 2013
