:userdoc.

.* ****************************************************************************
:h1 x=left y=bottom width=100% height=100% res=100.FreeType/2-Konfiguration
:p.Mit dieser grafischen Benutzeroberfl„che k”nnen Sie den FreeType/2-
Fonttreiber fr den Presentation Manager (FREETYPE.DLL) konfigurieren,
aktivieren oder deaktivieren.

:p.Alle in diesem Programm vorgenommenen nderungen erfordern einen Neustart
des Systems, um wirksam zu werden.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=110.Treiber
:p.Die :hp2.Treiber:ehp2.-Seite erm”glicht, FreeType/2 zu aktivieren oder
zu deaktivieren.

:p.Wenn FreeType/2 deaktiviert ist, drfen keine weiteren Seiten des
Konfigurationsnotizbuches aufgerufen werden.

:dl break=all.
:dt.:hp2.Aktueller TrueType-Schriftartentreiber:ehp2.
:dd.Dieses Kombinationsfeld zeigt den aktuell aktiven Fonttreiber an,
und erm”glicht es ihn zu „ndern.
:dl compact tsize=16.
:dt.FREETYPE.DLL
:dd.Der FreeType/2-Schriftartentreiber. Wenn Sie dieses Element ausw„hlen,
wird FreeType/2 aktiviert.
:dt.TRUETYPE.DLL
:dd.Der originale TrueType-Schriftartentreiber von IBM. Wenn Sie dieses Element
ausw„hlen, wird FreeType/2 deaktiviert.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=120.Einstellungen (1 von 2)
:p.
:dl break=all.
:dt.:hp2.Unicode-Einstellungen:ehp2.
:dd.Diese Einstellungen steuern, wie Unicode-kompatible Schriften ihre
Kodierungen an das System gemeldet bekommen.
:dl break=all.
:dt.Verwenden Sie immer Unicode-Kodierung, falls vorhanden.
:dd.Wenn diese Einstellung aktiviert ist, meldet FreeType/2 immer Unicode-f„hige
Schriften als solche, anstatt zu versuchen, automatisch die optimale Kodierung
auszuw„hlen. (Dies gilt vorbehaltlich der Ausnahmen, die durch die folgenden
beiden Einstellungen angezeigt werden, sofern sie aktiviert sind.)
:p.Dies erm”glicht im Allgemeinen die Untersttzung einer gr”áeren Anzahl
an Zeichen, und erm”glicht die Verwendung dieser Schriften fr die Darstellung
von Unicode-Text mit nativen (GPI)-Funktionen. Dies birgt jedoch auch einige
potenzielle Nachteile&colon.
:ul.
:li.Unter bestimmten Umst„nden kann dies die Leistung beeintr„chtigen,
zumindest in Kombination mit "Unicode-Schriften immer als
MBCS-kompatibel melden" (siehe unten).
Dadurch wird verhindert, daá jede Unicode-kompatible Schriftart fr die
DBCS-&osq.Zuordnungs&csq.schriftart verwendet werden kann,
:hp2.es sei denn:ehp2., "DBCS statt Unicode fr Zuordnungsschrift verwenden"
(siehe unten) ist ebenfalls aktiviert.
:eul.

:dt.Verwenden von DBCS anstelle von Unicode fr Zuordnungsschriften
:dd.Wenn diese Einstellung aktiviert ist, wird der Schriftart, die derzeit als
PM "association"-Schriftart (verwendet fr die DBCS-Glyphenersetzung)
definiert ist, anstelle von Unicode immer eine native DBCS-Glyphenliste
(Chinesisch, Japanisch oder Koreanisch) zugewiesen. Da die Zuordnung bei der
Verwendung einer Unicodeschriftart normalerweise fehlschl„gt, k”nnen mit dieser
Einstellung Unicode-kompatible Schriftarten fr die Zuordnung verwendet werden,
auch wenn die Einstellung "Immer Unicode verwenden" aktiv ist.
:p.Diese Einstellung ist standardm„áig auf aktiv gesetzt; es sollte selten
(wenn berhaupt) einen Grund geben, sie zu deaktivieren.
:nt.Auch wenn diese Einstellung es erm”glicht, sollten Sie Pan-Unicode-Fonts
wie "Times New Roman MT 30" oder "Arial Unicode MS" nicht als
Assoziationsschrift verwenden. Da die Zuordnungsschriftart eine
sprachspezifische CJK-Kodierung erh„lt, sind Zeichen aus der Schrift, die von
dieser speziellen Kodierung nicht untersttzt werden, nicht verwendbar.:ent.

:dt.Verwenden Sie DBCS anstelle von Unicode fr kombinierte Schriften.
:dd.Die DBCS-Versionen (chinesische, japanische und koreanische) von OS/2
enthalten eine Funktion, mit der Schriften an eine Art virtuellen
Schriftwrapper weitergeleitet werden k”nnen, der als &osq.kombinierter&osq.
Font bezeichnet wird. (Diese haben die Erweiterung .CMB und befinden sich
normalerweise im Verzeichnis \OS2\SYSDATA.) Die kombinierte Schrift erlaubt es,
Glyphen als Bitmaps (aus einer separaten Datei geladen) in bestimmten
Punktgr”áen darzustellen; fr Glyphen oder Punktgr”áen, bei denen keine Bitmap
verfgbar ist, wird die Originalschriftdatei verwendet.
Diese Funktion funktioniert jedoch in einigen F„llen nicht richtig, wenn die
weiterzuleitende Schriftart die Unicode-Kodierung verwendet.

:p.Wenn diese Einstellung aktiviert ist, wird jedem TrueType-Font, der derzeit
an einen kombinierten Font (wie in OS2.INI unter der Anwendung "PM_ABRFiles"
definiert) weitergeleitet wird, immer eine native DBCS-Glyphliste
(Chinesisch, Japanisch oder Koreanisch) anstelle von Unicode zugewiesen.
:p.Diese Einstellung ist standardm„áig aktiviert.

:dt.Unicode-Schriften immer als MBCS-kompatibel ausweisen
:dd.Wenn diese Einstellung aktiviert ist, wird jede Schrift, die als Unicode
gemeldet wird, auch als "MBCS-kompatibel" gemeldet, unabh„ngig davon,
wie viele Glyphen sie enth„lt.
:p.Die MBCS-Kompatibilit„t zeigt an, dass die Schriftart erweiterte
Zeichenuntersttzung enth„lt; Sie wird normalerweise empfohlen, da sonst viele
Grafikzeichen unbrauchbar werden. Es gibt jedoch einige potenzielle Nachteile
bei der Aktivierung dieser Einstellung&colon.
:ul.
:li.Die Leistung der Textwiedergabe kann bei Unicode-Fonts etwas geringer sein.
:li.Wenn die DBCS-Zuordnungsfunktion aktiviert ist, k”nnen Glyphen nicht durch
Text ersetzt werden, der mit einer Unicode-Font gerendert wurde.
Diese Einschr„nkung ist vor allem fr Benutzer mit chinesischen, japanischen
oder koreanischen Versionen von OS/2 von Interesse.)
:eul.
:edl.

:dt.:hp2.Optionale Merkmale:ehp2.
:dd.Mit diesen Einstellungen k”nnen Sie verschiedene optionale Funktionen von
FreeType/2 aktivieren oder deaktivieren.
:dl break=all.
:dt.Fixierung von Standardstilnamen
:dd.Wenn diese Einstellung aktiviert ist, versucht FreeType/2, standardisierte
Namenskonventionen auf die vier g„ngigsten Schriftarten-"Styles"
anzuwenden&colon. Normal, fett, kursiv (oder schr„g) und fett kursiv (oder
fett schr„g). FreeType/2 verwendet eine ziemlich intelligente Logik bei der
Anpassung der Schriftartennamen. Es ist jedoch m”glich, daá diese Einstellung
zu einer gelegentlichen Fehlbenennung der Schriftart oder des Stils fhren kann.
Die Aktivierung dieser Einstellung wird nur empfohlen, wenn beim Drucken unter
OpenOffice Fehlinformationen auftreten.
:dt.Erstellen von simulierten fetten Schrifts„tzen fr DBCS-Schriften
:dd.Diese Einstellung bewirkt, dass FreeType/2 eine "gef„lschte" fette Version
(mittels Simulation) eines beliebigen DBCS TrueType-Fontes erstellt, der keine
tats„chliche fette Version enth„lt.
:dt.Optimierung der Glyphenmetriken fr Netscape
:dd.Diese Einstellung bewirkt, dass die Metriken (die Zeichendimensionen und
Positionsdaten) aller TrueType-Schriften angepasst werden, um einige Probleme
bei der Zeichenpositionierung unter alten Versionen des Netscape-Webbrowsers
zu umgehen. Wenn Sie Netscape nicht regelm„áig verwenden, wird die Aktivierung
dieser Einstellung nicht generell empfohlen.
:dt.Alias TrueType "Times New Roman" nach "TmsRmn".
:dd.Normalerweise bertr„gt der Presentation Manager automatisch den
Schrifttyp 1 "Times New Roman" (im Lieferumfang von OS/2 enthalten) auf
"TmsRmn". Das bedeutet, daá bei Verwendung der Bitmap-Schriftart "TmsRmn"
die Schriftart Typ 1 verwendet wird, um jeden Text in anderen Punktgr”áen
als 8, 10, 12, 14, 18 oder 24 darzustellen.
:p.Wenn Sie diese Einstellung aktivieren, erstellt FreeType/2 einen
gleichwertigen Alias mit der TrueType-Version von "Times New Roman",
falls diese installiert ist. Dadurch kann die TrueType-Schriftart verwendet
werden, um die zus„tzlichen Punktgr”áen darzustellen, wenn Sie die
Type 1-Schriftart deinstallieren.
:nt.Die Verwendung dieser Einstellung wird nur empfohlen, wenn Sie beide, die
Typ-1-Version von "Times New Roman", :hp1.und:ehp1.die TrueType-Version, die
als Teil des Microsoft-Pakets "Core Fonts" frei verfgbar ist,
deinstallieren.:ent.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=130.Einstellungen (2 von 2)
:p.Diese Seite enth„lt verschiedene erweiterte Einstellungen.
:dl break=all.
:dt.:hp2.Erweiterte Einstellungen:ehp2.
:dd.Normalerweise sollte es nicht notwendig sein, eine dieser Einstellungen zu
„ndern. Es wird dringend empfohlen, sie auf ihren Standardwerten zu belassen,
es sei denn, Sie sind sich der Folgen einer solchen nderung bewusst.
:dl break=all.
:dt.Gr”áe des Open Face Caches
:dd.Anstatt alle Schriften des Systems beim Start zu laden, h„lt FreeType/2 nur
die :hp1.n:ehp1. krzlich-genutzten Schriftarten im Speicher ge”ffnet.
Diese Einstellung erm”glicht es Ihnen diesen :hp1.n:ehp1.-Wert anzupassen.
:dt.Instanzaufl”sung
:dd.Dies erm”glicht es Ihnen, die standardm„áige Ger„teaufl”sung zu
berschreiben, die fr die Skalierung von Glyphen auf dem Bildschirm verwendet
wird (in Punkten pro Zoll). Die einzigen zul„ssigen Werte sind 72, 96 und 120.
72 dpi ist die Voreinstellung.
:edl.
:edl.

:euserdoc.
