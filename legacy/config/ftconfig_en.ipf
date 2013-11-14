:userdoc.

.* ****************************************************************************
:h1 x=left y=bottom width=100% height=100% res=100.FreeType/2 Configuration
:p.This graphical user interface allows you to configure, enable, or disable
the FreeType/2 font driver for Presentation Manager (FREETYPE.DLL).

:p.All changes made in this program will require a system reboot to take effect.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=110.Driver
:p.The :hp2.Driver:ehp2. page allows you to enable or disable FreeType/2.

:p.If FreeType/2 is disabled, no other pages of the configuration notebook
may be accessed.

:dl break=all.
:dt.:hp2.Current TrueType font driver:ehp2.
:dd.This combo-box control shows the currently-active font driver, and allows
you to change it.
:dl compact tsize=16.
:dt.FREETYPE.DLL
:dd.The FreeType/2 font driver.  Selecting this item will cause FreeType/2 to
be enabled.
:dt.TRUETYPE.DLL
:dd.The original TrueType font driver from IBM.  Selecting this item will cause
FreeType/2 to be disabled.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=120.Settings (1 of 2)
:p.
:dl break=all.
:dt.:hp2.Unicode Settings:ehp2.
:dd.These settings control how Unicode-compatible fonts will have their
encodings reported to the system.
:dl break=all.
:dt.Always use Unicode encoding if available
:dd.If this setting is enabled, FreeType/2 will always report Unicode-capable
fonts as such, instead of attempting to automatically select the optimal
encoding.  (This is subject to the exceptions indicated by the following two
settings, if they are enabled.)
:p.This generally allows a greater range of characters to be supported, and
allows these fonts to be used for rendering Unicode text using native (GPI)
functions.  However, it has some potential drawbacks as well&colon.
:ul.
:li.It may impact performance under some circumstances, at least when used in
combination with "Always report Unicode fonts as MBCS-compatible" (see below).
:li.It prevents any Unicode-compatible font from being usable for the DBCS
&osq.association&csq. font, :hp2.unless:ehp2. "Use DBCS instead of Unicode for
association font" (see below) is also enabled.
:eul.

:dt.Use DBCS instead of Unicode for association font
:dd.If this setting is enabled, whatever font is currently defined as the PM
"association" font (used for DBCS glyph substitution) will always be assigned a
native DBCS glyphlist (Chinese, Japanese, or Korean) instead of Unicode.  Since
association normally fails when a Unicode font is used, this setting allows
Unicode-compatible fonts to be used for association even when the "Always use
Unicode" setting is active.
:p.This setting defaults to active; there should rarely (if ever) be any reason
to deactivate it.
:nt.Even though this setting makes it possible, you should avoid using
pan-Unicode fonts like "Times New Roman MT 30" or "Arial Unicode MS" as the
association font.  Because the association font will be given a
language-specific CJK encoding, any characters from the font which are not
supported by that particular encoding will not be usable.:ent.

:dt.Use DBCS instead of Unicode for combined fonts
:dd.DBCS (Chinese, Japanese, and Korean) versions of OS/2 include a feature
whereby fonts can be forwarded to a kind of virtual font wrapper called a
&osq.combined&osq. font.  (These have the extension .CMB and are normally
located in the \OS2\SYSDATA directory.)  The combined font allows glyphs
to be rendered as bitmaps (loaded from a separate file) at certain point
sizes; for glyphs or point sizes where no bitmap is available, the original
font file will be used.  However, this feature fails to work properly in some
cases if the font being forwarded is using the Unicode encoding.
:p.If this setting is enabled, any TrueType font which is currently forwarded
to a combined font (as defined as in OS2.INI under the "PM_ABRFiles"
application) will always be assigned a native DBCS glyphlist (Chinese,
Japanese, or Korean) instead of Unicode.
:p.This setting is enabled by default.

:dt.Always report Unicode fonts as MBCS-compatible
:dd.If this setting is enabled, any font which is reported as Unicode will also
be reported as "MBCS-compatible", regardless of how many glyphs it contains.
:p.MBCS compatibility indicates that the font contains extended character
support; it is normally recommended, as otherwise many graphics characters will
be unusable.  However, there are some potential drawbacks to enabling this
setting&colon.
:ul.
:li.Text rendering performance may be slightly slower for Unicode fonts.
:li.When the DBCS association feature is enabled, glyphs cannot be
substituted into text rendered using any Unicode font.  (This limitation is
mostly of interest to users running Chinese, Japanese, or Korean versions of
OS/2.)
:eul.
:edl.

:dt.:hp2.Optional Features:ehp2.
:dd.These settings allow you to enable or disable various optional features of
FreeType/2.
:dl break=all.
:dt.Fix standard style names
:dd.When this setting is enabled, FreeType/2 attempts to apply standardized
naming conventions to the four most common font "styles"&colon. Normal, Bold,
Italic (or Oblique), and Bold Italic (or Bold Oblique).  FreeType/2 uses
fairly intelligent logic when adjusting the font names; however, it is
possible that this setting might result in the occasional misnamed font or
or style.  Enabling this setting is only recommended if you are encountering
malformed text when printing under OpenOffice.
:dt.Create simulated bold face for DBCS fonts
:dd.This setting causes FreeType/2 to create a "fake" bold version (via
simulation) of any DBCS TrueType font which does not include an actual bold
version.
:dt.Optimize glyph metrics for Netscape
:dd.This setting causes the metrics (the character dimensions and positioning
data) of all TrueType fonts to be adjusted in order to work around some problems
in character positioning under old versions of the Netscape web browser.  Unless
you regularly use Netscape, enabling this setting is not generally recommended.
:dt.Alias TrueType "Times New Roman" to "TmsRmn"
:dd.Normally, Presentation Manager automatically aliases the Type 1 "Times New
Roman" font (included with OS/2) to "TmsRmn".  This means that, when you use the
"TmsRmn" bitmap font, the Type 1 font will be used to render any text at point
sizes other than 8, 10, 12, 14, 18, or 24.
:p.Enabling this setting causes FreeType/2 to create an equivalent alias using
the TrueType version of "Times New Roman", if it is installed.  This allows the
TrueType font to be used to render the additional point sizes if you uninstall
the Type 1 font.
:nt.Use of this setting is only recommended if you both uninstall the Type 1
version of "Times New Roman", :hp1.and:ehp1. install the TrueType version (which
is freely available as part of Microsoft's "Core Fonts" package).:ent.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=130.Settings (2 of 2)
:p.This page contains various advanced settings.
:dl break=all.
:dt.:hp2.Advanced Settings:ehp2.
:dd.It should not normally be necessary to modify any of these settings.  You
are strongly recommended to leave them at their default values unless you are
fully aware of the consequences of changing them.
:dl break=all.
:dt.Open face cache size
:dd.Rather than open all font faces at system startup, FreeType/2 only keeps
the :hp1.n:ehp1. most-recently-used font faces open in memory at any one
time.  This setting allows you to modify the value of :hp1.n:ehp1..
:dt.Instance resolution
:dd.This allows you to change how the font driver internally defines a 'point'
when setting the font size.  The default value is the industry standard of 72
points per inch.
:p.This setting should :hp2.not:ehp2. generally be changed.  If you do change
it, you should keep these important points in mind&colon.
:ul.
:li.What constitutes one 'inch' on-screen is defined by your video DPI setting;
namely, either 96 or 120 pixels.  Depending on your screen size and resolution,
this may not correspond exactly to one inch in physical terms.
:li.This setting cannot alter the amount of screen space that Presentation
Manager assigns to each character. Therefore, increasing this value may lead
to characters appearing cropped when drawn.
:eul.
:p.The only acceptable values for this setting are 72, 96, and 120.

:edl.
:edl.

:euserdoc.
