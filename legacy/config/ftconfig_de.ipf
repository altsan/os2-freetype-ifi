:userdoc.

.* ****************************************************************************
:h1 x=left y=bottom width=100% height=100% res=100.FreeType/2-Konfiguration
:p.Diese Anwendung ermîglicht die Konfiguration, des
FreeType/2-Schriftartentreibers fÅr Presentation Manager (FREETYPE.DLL).
Ebenso kann der FreeType/2-Schriftartentreibers hiermit auch
aktiviert oder inaktiviert werden.

:p.Alle in diesem Programm vorgenommenen énderungen erfordern einen
Systemneustart, um wirksam zu werden.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=110.Treiber
:p.Auf der Seite :hp2.Treiber:ehp2. kînnen Sie FreeType/2 aktivieren
oder inaktivieren.

:p.Wenn FreeType/2 inaktiviert ist, kann auf keine der anderen Seiten des
Konfigurations-Notizblocks zugegriffen werden.

:dl break=all.
:dt.:hp2.Aktueller TrueType-Schriftartentreiber:ehp2.
:dd.Dieses Kombinationsfeld zeigt den derzeit aktiven Schriftartentreiber an.
Au·erdem kînnen Sie diesen hier anpassen.
:dl compact tsize=16.
:dt.FREETYPE.DLL
:dd.Der FreeType/2-Schriftartentreiber.  Wenn Sie diesen Punkt auswÑhlen,
wird FreeType/2 aktiviert.
:dt.TRUETYPE.DLL
:dd.Der ursprÅngliche TrueType-Schriftentreiber von IBM. Wenn Sie diesen Punkt
auswÑhlen, wird FreeType/2 inaktiviert.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=120.Einstellungen (1 von 2)
:p.
:dl break=all.
:dt.:hp2.Unicode-Einstellungen:ehp2.
:dd.Diese Einstellungen steuern, wie Unicode-kompatible Schriften ihre
Kodierungen an das System melden.
:dl break=all.
:dt.Immer Unicode-Kodierung verwenden, falls verfÅgbar
:dd.Wenn diese Einstellung aktiviert ist, meldet FreeType/2 Unicode-fÑhige
Schriften immer als solche, anstatt zu versuchen, automatisch die optimale
Kodierung auszuwÑhlen.
Dies gilt vorbehaltlich der Ausnahmen, die durch die folgenden beiden
Einstellungen angezeigt werden, sofern sie aktiviert sind.
:p.Dadurch kann im Allgemeinen eine grî·ere Bandbreite an Zeichen unterstÅtzt
werden, und diese Schriftarten kînnen fÅr die Darstellung von Unicode-Text
unter Verwendung nativer (GPI) Funktionen verwendet werden.
Es hat jedoch auch einige potenzielle Nachteile&colon.
:ul.
:li.Diese Einstellung kann unter UmstÑnden die Leistung beeintrÑchtigen,
zumindest wenn sie in Kombination mit "Unicode-Schriften immer als
MBCS-kompatibel melden" (siehe unten) verwendet wird.
:li.Diese Einstellung verhindert au·erdem, da· jede Unicode-kompatible
Schriftart fÅr die Schriftart DBCS-&osq.Zuordnung&csq. verwendet werden kann,
:hp2.au·er:ehp2. die Einstellung "DBCS anstelle von Unicode fÅr
Assoziationsschriftart verwenden" (siehe unten) ist ebenfalls aktiviert.
:eul.

:dt.Verwenden von DBCS anstatt Unicode fÅr Assoziationsschriftart
:dd.Wenn diese Einstellung aktiviert ist, wird jeder Schriftart, die derzeit
als PM-"Assoziations"-Schriftart (die fÅr die DBCS-Glyphenersetzung verwendet
wird) definiert ist, immer eine native DBCS-Glyphenliste (Chinesisch,
Japanisch oder Koreanisch) anstelle von Unicode zugewiesen.
Da die Assoziation normalerweise fehlschlÑgt, wenn eine Unicode-Schriftart
verwendet wird, ermîglicht diese Einstellung die Verwendung von
Unicode-kompatiblen Schriften fÅr die Assoziation, auch wenn die Einstellung
"Immer Unicode verwenden" aktiv ist.
:p.Diese Einstellung ist standardmÑ·ig aktiv; Es sollte selten
(wenn Åberhaupt) einen Grund geben, sie zu deaktivieren.
:nt.Auch wenn diese Einstellung dies ermîglicht, sollten Sie die Verwendung
von Pan-Unicode-Schriften wie "Times New Roman MT 30" oder "Arial Unicode MS"
als Assoziationsschriftart vermeiden.
Da die Assoziationsschrift eine sprachspezifische CJK-Kodierung erhÑlt,
sind alle Zeichen der Schriftart, die von dieser speziellen Kodierung nicht
unterstÅtzt werden, nicht verwendbar.:ent.

:dt.DBCS anstelle von Unicode fÅr kombinierte Schriften verwenden
:dd.Die DBCS-Versionen (Chinesisch, Japanisch und Koreanisch) von OS/2
enthalten eine Funktion, mit der Schriften an eine Art virtuellen,
&osq.kombinierten&osq. Schriftart-Umsetzer weitergeleitet werden kînnen.
Diese haben die Erweiterung .CMB und befinden sich normalerweise im
Verzeichnis \OS2\SYSDATA.
Die kombinierte Schriftart ermîglicht die Darstellung von Glyphen als Bitmap
(aus einer separaten Datei geladen) in bestimmten Punktgrî·en;
FÅr Glyphen oder Punktgrî·en, fÅr die keine Bitmap verfÅgbar ist, wird die
Originalschriftartdatei verwendet.
Diese Funktion funktioniert jedoch in einigen FÑllen nicht richtig,
wenn die weitergeleitete Schrift die Unicode-Kodierung verwendet.
:p.Wenn diese Einstellung aktiviert ist, wird jeder TrueType-Schriftart,
die derzeit an eine kombinierte Schriftart (wie in OS2.INI unter der Anwendung
"PM_ABRFiles" definiert) weitergeleitet wird, immer eine native
DBCS-Glyphenliste (Chinesisch, Japanisch oder Koreanisch) anstelle von
Unicode zugewiesen.
:p.Diese Einstellung ist standardmÑ·ig aktiviert.

:dt.Unicode-Schriften immer als MBCS-kompatibel melden
:dd.Wenn diese Einstellung aktiviert ist, wird jede Schrift, die als Unicode
gemeldet wird, auch als "MBCS-kompatibel" gemeldet, unabhÑngig davon, wie
viele Glyphen sie enthÑlt.
:p.Die MBCS-KompatibilitÑt zeigt an, da· die Schriftart eine erweiterte
ZeichenunterstÅtzung enthÑlt; Sie wird normalerweise empfohlen, da sonst viele
Grafikzeichen unbrauchbar werden. Es gibt jedoch einige potenzielle Nachteile,
wenn diese Einstellung aktivieren ist&colon.
:ul.
:li.Die Leistung der Textwiedergabe kann bei Unicode-Schriften
etwas langsamer sein.
:li.Wenn die DBCS-Assoziationsfunktion aktiviert ist, kînnen Glyphen nicht in
Text ersetzt werden, der mit einer beliebigen Unicode-Schriftart
gerendert wird.
Diese EinschrÑnkung ist vor allem fÅr Anwender interessant, die chinesische,
japanische oder koreanische Versionen von OS/2 verwenden.:eul.
:edl.

:dt.:hp2.Optionale Merkmale:ehp2.
:dd.Mit diesen Einstellungen kînnen Sie verschiedene optionale
Funktionen von FreeType/2 aktivieren oder inaktivieren.
:dl break=all.
:dt.Standard-Stilnamen festlegen
:dd.Wenn diese Einstellung aktiviert ist, versucht FreeType/2, standardisierte
Namenskonventionen auf die vier gebrÑuchlichsten Schrift-"Stile"
anzuwenden&colon. Normal, Fett, Kursiv (oder SchrÑg), sowie Fett Kursiv
(oder Fett SchrÑg).
FreeType/2 verwendet eine ziemlich intelligente Logik bei der Anpassung der
Schriftnamen; Es ist jedoch mîglich, da· diese Einstellung gelegentlich zu
einer falsch benannten Schrift oder einem falsch benannten Stil fÅhrt.
Die Aktivierung dieser Einstellung wird nur empfohlen, wenn Sie beim Drucken
unter OpenOffice auf fehlerhaften Text sto·en.
:dt.Erstellen einer simulierten Fettschrift fÅr DBCS-Schriften
:dd.Diese Einstellung bewirkt, da· FreeType/2 von jeder
DBCS-TrueType-Schriftart, die keine echte Fettdruckversion enthÑlt,
eine "simulierte" Fettdruckversion erzeugt.
:dt.Optimieren der Glyphenmetrik fÅr Netscape
:dd.Diese Einstellung bewirkt, da· die Metriken (die Zeichenabmessungen und
Positionierungsdaten) aller TrueType-Schriften angepa·t werden, um einige
Probleme bei der Zeichenpositionierung unter alten Versionen des
Netscape-Webbrowsers zu umgehen.
Sofern Sie nicht regelmÑ·ig Netscape verwenden, wird die Aktivierung dieser
Einstellung im Allgemeinen nicht empfohlen.
:dt.Alias TrueType "Times New Roman" bis "TmsRmn"
:dd.Normalerweise aliasiert der Presentation Manager die Type 1-Schriftart
"Times New Roman" (in OS/2 enthalten) automatisch zu "TmsRmn".
Das bedeutet, da· bei Verwendung der Bitmap-Schriftart "TmsRmn" die
Type 1-Schriftart verwendet wird, um jeden Text in anderen Punktgrî·en
als 8, 10, 12, 14, 18 oder 24 zu rendern.
:p.Die Aktivierung dieser Einstellung bewirkt, da· FreeType/2 einen
Ñquivalenten Alias unter Verwendung der TrueType-Version von "Times New Roman"
erstellt, falls diese installiert ist.
Dadurch kann die TrueType-Schriftart zum Rendern der zusÑtzlichen Punktgrî·en
verwendet werden, wenn Sie die Type 1-Schriftart deinstallieren.
:nt.Die Verwendung dieser Einstellung wird nur empfohlen, wenn Sie sowohl die
Type 1-Version von "Times New Roman" deinstallieren, :hp1.und:ehp1. auch die
TrueType-Version (die als Teil des Microsoft-Pakets "Core Fonts"
frei verfÅgbar ist) installieren.:ent.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=130.Einstellungen (2 von 2)
:p.Diese Seite enthÑlt verschiedene erweiterte Einstellungen.
:dl break=all.
:dt.:hp2.Erweiterte Einstellungen:ehp2.
:dd.Normalerweise sollte es nicht notwendig sein, eine dieser Einstellungen
zu Ñndern. Es wird dringend empfohlen, sie auf ihren Standardwerten
zu belassen, es sei denn, Sie sind sich der Konsequenzen einer énderung
voll bewu·t.
:dl break=all.
:dt.Schriftarten-Cache-Grî·e
:dd.Anstatt alle Schriftarten beim Systemstart zu laden, belÑ·t FreeType/2 nur
die :hp1.n:ehp1. zuletzt verwendeten Schriftarten im Speicher.
Mit dieser Einstellung kînnen Sie den Wert von :hp1.n:ehp1. Ñndern.
:dt.Auflîsung der Instanz
:dd.Hiermit kann angepa·t werden, wie der Schriftartentreiber bei der
Einstellung der Schriftgrî·e intern einen 'Punkt' definiert.
Der Standardwert ist der Industriestandard von 72 Punkten pro Zoll.
:p.Diese Einstellung sollte generell :hp2.nicht:ehp2. geÑndert werden.
Wenn Sie sie Ñndern, sollten Sie diese wichtigen Punkte im Auge behalten&colon.
:ul.
:li.Was ein "Zoll" auf dem Bildschirm ausmacht, wird durch Ihre
Video-DPI-Einstellung definiert, d.h. entweder 96 oder 120 Pixel.
AbhÑngig von der Grî·e und Auflîsung Ihres Bildschirms entspricht dies
mîglicherweise nicht genau einem Zoll in physischer Hinsicht.
:li.Diese Einstellung kann die Menge des Bildschirmplatzes, die der
Presentation Manager jedem Zeichen zuweist, nicht verÑndern.
Daher kann eine Erhîhung dieses Wertes dazu fÅhren, da· Zeichen beim
Zeichnen abgeschnitten erscheinen.
:eul.
:p.Die einzigen sinnvollen Werte fÅr diese Einstellung sind 72, 96 und 120.

:edl.
:edl.

:euserdoc.
