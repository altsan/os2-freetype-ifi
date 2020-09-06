
                        * * *   FreeType/2   * * *

                      (Version 1.3.6, 23 March 2020)

 Copyright (C) 2010--2018 Alexander Taylor <alex@altsan.org>
 Copyright (C) 2002--2007 KO Myung-Hun <komh@chollian.net>
 Copyright (C) 2003--2004 Seo, Hyun-Tae <acrab001@hitel.net>
 Copyright (C) 1997--2000 Michal Necasek <mike@mendelu.cz>
 Copyright (C) 1997,1998 The FreeType Development Team


 Motto: "OS/2 is dead? Again? Thanks for telling me, I'd never notice!"


 *** if you are upgrading from previous version, please see the FAQ (Q15) ***



- First a short Q&A:

Q1: What's this?
A1: This is what OS/2 users have been waiting for only too long - a free,
    high-quality TrueType renderer a.k.a. Font Driver conforming to the
    OS/2 Intelligent Font Interface specification. It is based on FreeType -
    a free portable library for using TrueType fonts.

    Please note that although this code is free the FreeType team and
    I will cheerfully accept any donations by happy users ;-) (not that I
    expect to get any)

Q2: How do I use this?
A2: Go to OS/2 command line and run INSTALL.CMD from the directory containing
    FREETYPE.DLL. This will replace the original IBM TrueType driver if it is
    installed.

    Also, you should create a program object for ftconfig.exe if you want to
    use the graphical configuration program.

    If you have a previous version installed, you can run UPDATE.CMD instead.
    That will replace the previous FREETYPE.DLL with the new one.  (Note that
    this will NOT back up the previous version first, so if you use UPDATE.CMD
    you should manually back up \OS2\DLL\FREETYPE.DLL yourself.)

    In either case, you need to reboot for the changes to take effect.

Q3: Where's the disclaimer?
A3: No, don't worry, I didn't forget that. I of course provide NO WARRANTY
    that this code will work as you expect. Use only at your OWN RISK!

Q4: What should I do RIGHT NOW?
A4: Before attempting to install this driver, you are STRONGLY advised
    to archive your current configuration (Set Desktop Properties/Archive/
    Create archive at each startup, then reboot. Then of course switch archiving
    off). It is always possible your system won't boot with the font driver
    installed. You can risk it, but I warned you! You know how nasty the
    computers can be ;-)

Q5: What about the license?
A5: This code is distributed under the FreeType license.
    It is free and the source code should be available from wherever you
    downloaded this package.

Q6: How do I get rid of this?
A6: Ah, right question. Just run UNINST.CMD. That removes the font driver
    (not physically, it just isn't used on next startup) and restores the
    original TRUETYPE.DLL if it exists.  (You can also use the ftconfig.exe
    GUI to select the IBM TrueType driver, which will achieve the same effect.)

Q7: Is there something else?
A7: Yes, be prepared that the fonts just kick ass! You will no longer have
    to envy those poor souls still using the so-called 95% OS from THAT
    unspeakable company starting with the letter M ;-)

Q8: How can I see DBCS chars on non-DBCS OS/2 system ?
A8: Short answer: They should (hopefully) display correctly as long as you are
    using the appropriate codepage.  Fonts which support Unicode (most modern
    TrueType fonts do) should also be able to display Unicode text correctly
    under applications which use it.

    Long answer: In practise things aren't ALWAYS quite that simple. ;)

    If the font in question does not support Unicode, or if FreeType/2 is not
    configured to use Unicode by default, then some complex heuristics are
    used to try and determine the best character lookup logic.  Unfortunately,
    some badly-behaved fonts may not report their encodings correctly, which
    can make things awkward.  (See Q13 in the FAQ for more information.)

    If all else fails, you can maximize your chances of getting the correct
    characters to display by changing your country and/or LANG settings, as
    well as your system codepage, to match whatever language the font in
    question is designed for.

    All of these can be changed by editing your CONFIG.SYS file.

    First, to modify a country code, find and edit the COUNTRY statement.
    For example,
        COUNTRY=082,X:\OS2\SYSTEM\COUNTRY.SYS
    where 082 is the country code for Korean.

    Second, set the 'LANG' env. var.
    For example,
        LANG=ko_KR
    where ko_KR is the value for Korean.

    Finally, if you want to see the native font name displayed properly, you
    can change your primary system codepage.
    For example,
        CODEPAGE=949,850
    949 is the codepage for Korean.  But this is not recommended because using
    a primary DBCS codepage on non-DBCS systems can cause unexpected problems.



- Current features/bugs/limitations:

 Features   : - outlines
              - scaled/rotated text
              - supports printed output
              - works with TTCs (TrueType collections)
              - national characters (if provided in the font, of course);
                should work with all Latin codepages, Cyrillic and Greek.
              - decode font name according to primary codepage.
              - DBCS support
                - supports SBIT fonts.
                - supports fake-bold for DBCS fonts.
                - supports unicode encoded face name other than english.
                  If your language is not supported correctly, you can
                  send a report to one of the contact addresses below.
              - In case of unicode, you can configure whether unicode or
                codepage encoding will be used.

 Bug/feature: - Unharmonious glyph spacing in some applications. This seems
                to come from OS/2's WYSIWYG glyph placement policy. This
                is more or less visible depending on the application. We
                can't do a lot about this... At least it's true WYSIWYG and
                no nasty surprises when printing.

              - Some unicode (WorldType) fonts for DBCS shipped with OS/2
                (on MCP/ACP CD#2) don't work with FreeType/2.  Recommend to
                use the older versions on CD#1 instead, or even (gasp) fonts
                from M$...

              - If the "fake bold" feature is activated, it may conflict with
                the equivalent functionality of the Innotek Font Engine.  (As
                near as I can tell, the IFE will "see" the fake-bold fonts
                reported by the system, which apparently prevents it from being
                able to create its own fake-bold versions.)  This may cause
                IFE-enabled programs, such as older versions of Mozilla, to
                substitute different fonts for bold text if there isn't an
                actual bold version of the current font installed.

 Limitations: - No grayscaling (a.k.a. antialiasing) - this is a limitation
                of OS/2, not FreeType/2.  If OS/2 starts supporting it, I'll
                implement it the moment I lay my hands on the specs :)
                Unfortunately it most probably won't happen any too soon.
                Anyway, you have to bug IBM about this one, not me!

                Of course, individual applications can support antialiasing
                if they use a separate library (e.g. the standalone FreeType
                library) for rendering text.  But the operating system itself
                (on which this driver operates) cannot support antialiased text
                in the general case unless IBM decides to implement it.

                (Note: There was at one time a third-party product known as the
                Innotek Font Engine, which did allow antialiasing by actually
                replacing key pieces of OS/2's graphics engine.  However, the
                mechanism used was not documented, and was vulnerable to some
                instabilities, at least if used for anything other than a
                specific set of supported applications.)

              - Font face names longer than 31 characters will be truncated.
                This is another OS/2 limitation.  If you need the full
                (un-truncated) name of a font, you will have to call the OS/2
                GpiQueryFullFontFileDescs() function on the font filename
                (see the GPI documentation from the OS/2 toolkit for details).

              - The User Defined Character (UDC) feature of DBCS versions of
                OS/2 is not currently supported by FreeType/2.  At least, no
                specific logic for dealing with it has been implemented.

                The original TrueType driver from IBM appears to have done
                *something* related to UDC support, but the snippets of source
                code that IBM did make available are highly incomplete, so I
                don't really know what that something might have been.

                At any rate, UDC support has not been tested at all.  It may
                work (partially or completely), or it may not.

              - Getting font name information using the OS/2 GPI functions
                GpiQueryFonts() and GpiQueryFullFontFileDescs() can be quite
                slow in the case of large font files, especially compared to
                the same routines in IBM's TrueType driver.  Among other things
                this makes the XWorkplace font folder populate rather slowly.


- Planned features and features under consideration:
   - possibly adding even support for Type 1 fonts, but that depends on
     further FreeType engine development. Looks quite probable now.
   - add UDC support, if possible (may not be, I have no real information
     on how this is supposed to work on the IFI level).


- File lists

  - readme.txt     This file
  - FAQ            User's FAQ
  - FreeType.dll   Font driver
  - INSTALL.CMD    Installation script
  - UNINST.CMD     Uninstallation script
  - UPDATE.CMD     Update script
  - QUERY.CMD      Query for font driver script
  - repmod.exe     Replacement program for lock files
  - ftconfig.exe   Graphical configuration tool for setting driver options
  - ftconfig.hlp   Help file for configuration tool
  - nlv_src.zip    Language source for configuration tool (for translators)


- History of version 1.3

  - v1.36 ( 2020-03-20 )
    .Fixed a couple of bugs in DBCS font name lookup. 
    .Configuration GUI help is now NLS-aware.

  - v1.35 ( 2018-03-11 )
    .Some of the Hangul system fonts included with OS/2 Korean are broken in
     that they report their names and cmaps as Unicode, but actually use Wansung
     and PMKOR. Since this situation can't really be detected heuristically, an
     internal blacklist of known offenders (currently the five TTFs included in
     Warp 4-K) has been added.  This means FREETYPE.DLL should hopefully now
     work properly with the default Warp 4 Korean fonts.
    .Configuration GUI now includes Spanish translation, courtesy of Alfredo
     Fern ndez D¡az.

  - v1.34 ( 2013-07-18 )
    .When the Style_Fixup feature was enabled, fonts with the subfamily name
     "Medium" were being changed to "Bold" in many cases where this was not
     appropriate. This has now been fixed and the logic made more intelligent.
    .The PANOSE table in FONTMETRICS should now be set correctly (finally).

  - v1.33 ( 2012-11-29 )
    .Added new optional workaround for printing problems in OpenOffice.org
     3.x.  This is designed to prevent a common problem in which certain
     fonts show up with badly mis-spaced or overlapping text when printed.
     It is turned off by default but can be enabled using the "Fix standard
     style names" option in the configuration GUI, or by setting the key
     "Style_Fixup" to "1" (under "FreeType/2" in OS2.INI).
    .Added some additional build information to BLDLEVEL signature.

  - v1.32 ( 2011/03/28 )
    .Fixed broken processing of non-ASCII SBCS characters in CJK glyphlists.
    .SBCS (halfwidth) katakana should now be properly rendered (where they
     exist) even for non-Unicode fonts.
    .Charset and DBCS information flags in FONTMETRICS fsDefn and fsSelection
     are now set.
    .Setting of the license bit in FONTMETRICS fsType now behaves the same way
     as the IBM TrueType driver.
    .FONTMETRICS fsSelection now retains the bold flag (0x20); this reflects
     both the OS/2 toolkit headers and the TrueType spec.
    .New option to auto-disable Unicode for fonts which are forwarded to
     "combined" (CMB) wrappers on DBCS systems; GUI updated to match.

  - v1.31 ( 2011/01/02 )
    .The GPI FONTMETRICS fsType field now correctly indicates whether the face
     and/or family name has been truncated.
    .Implemented the QueryFullFaces API.  This enables use of the GPI function
     GpiQueryFullFontFileDescs() for TrueType font files.
    .The configuration GUI now correctly reads the DPI setting from OS2.INI.
    .Added an Undo button to most pages of the configuration GUI; this resets
     the current page (only) to whatever values were set at program statup.

  - v1.30 ( 2010/04/19 )
    .New option to auto-disable Unicode encoding for DBCS association font.
    .New option to toggle setting of MBCS flag for all Unicode fonts.
    .Integrated improved name lookup logic from KO Myung-hun.
    .Codepage-based name lookup now recognizes additional Japanese and
     Sim.Chinese codepages.
    .Added workaround for name lookup of broken RF* fonts included with
     Japanese MCP.
    .New configuration GUI.


Thanks & acknowledgements (for version 1.3):
 - Michal Necasek, the original author, for sending me loads of information a
   few years back, including a lot of documentation not available elsewhere.
 - KO Myung-hun, both for letting me take over maintenance, and for giving
   me lots of tips and feedback (and putting up with all my tedious questions).
 - Knut St. Osmundson for helping me sort out a particularly difficult problem.
 - all the people (listed at the top of this file) who've worked on FreeType
   and FreeType/2 over the years.

(The following acknowledgements are from Michal)
And finally, thanks go to:
 - the FreeType team, the makers of FreeType. Without them, my work would be
   impossible.
 - especially David Turner of FreeType for his help, advice and support
 - Robert Muchsel, I used one or two ideas from his code
 - Marc L Cohen, Ken Borgendale and Tetsuro Nishimura from IBM. They provided
   me with lots of extremely valuable 'inside' information.
 - and last but not least - IBM, for providing us with the wonderful OS/2.
   And for giving out the necessary docs for free. If all companies did
   that, the world would be a better place.

Information on FreeType is available at
 http://www.freetype.org

Bug reports, suggestions greetings and comments can be sent directly to the
author (see the email address at the top of this file).

And if you didn't know, IBM and OS/2 are registered trademarks of the
International Business Machines Corporation.
