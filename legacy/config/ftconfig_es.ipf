:userdoc.

.* ****************************************************************************
:h1 x=left y=bottom width=100% height=100% res=100.Configuraci¢n de FreeType/2
:p.Esta interfaz gr†fica de usuario le permite configurar, activar o desactivar
el controlador FreeType/2 de fuentes para Presentation Manager (FREETYPE.DLL).

:p.Los cambios realizados con este programa requieren reiniciar el sistema para
tener efecto.

.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=110.Controlador
:p.La p†gina :hp2.Controlador:ehp2. le permite activar y desactivar FreeType/2.

:p.Si se desactiva FreeType/2, no puede accederse a m†s p†ginas del cuaderno de
configuraci¢n.

:dl break=all.
:dt.:hp2.Controlador actual de fuentes TrueType:ehp2.
:dd.Esta lista desplegable muestra el controlador de fuentes activo actualmente
y le permite cambiarlo.
:dl compact tsize=16.
:dt.FREETYPE.DLL
:dd.El controlador de fuentes FreeType/2. Seleccionar esta opci¢n habilitar†
FreeType/2.
:dt.TRUETYPE.DLL
:dd.El controlador de fuentes TrueType original de IBM. Seleccionar esta opci¢n
inhabilitar† FreeType/2.
:edl.
:edl.


.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=120.Opciones (1 de 2)
:p.
:dl break=all.
:dt.:hp2.Opciones de Unicode:ehp2.
:dd.Estas opciones controlan c¢mo se informa al sistema de la codificaci¢n de
las fuentes compatibles con Unicode.
:dl break=all.
:dt.Utilizar codificaci¢n Unicode siempre que estÇ disponible
:dd.Si se activa esta opci¢n, FreeType/2 siemprÇ presentar† las fuentes
compatibles con Unicode como tales, en vez de intentar seleccionar
autom†ticamente la codificaci¢n de caracteres ¢ptima. (Esto est† sujeto a
excepciones indicadas por las dos opciones siguientes, si est†n activas).
:p.Esto permite generalmente dar soporte a un rango m†s amplio de caracteres, y
que se utilice estas fuentes para representar texto unicode utilizando
funciones nativas (GPI). Sin embargo, tambiÇn tiene algunas desventajas
potenciales&colon.
:ul.
:li.Puede afectar al rendimiento bajo algunas circunstancias, al menos si
se utiliza en combinaci¢n con ÆMostrar fuentes Unicode como compatibles con
MBCSØ (ver m†s abajo).
:li.Evita que ninguna fuente compatible con Unicode se pueda utilizar como
fuente &osq.de asociaci¢n&csq. DBCS, :hp2.a no ser que:ehp2. ÆUtilizar DBCS en
vez de Unicode para fuente de asociaci¢nØ (ver m†s abajo) estÇ activa tambiÇn.
:eul.

:dt.Utilizar DBCS en vez de Unicode para fuente de asociaci¢n
:dd.Si esta opci¢n est† activa, a cualquier fuente que se encuentre definida
como fuente PM Æde asociaci¢nØ (utilizada para sustituci¢n de glifos DBCS) se
le asociar† siempre una lista nativa de glifos DBCS (chinos, japoneses o
coreanos) en vez de Unicode. Puesto que la asociaci¢n falla habitualmente
cuando se utiliza una fuente Unicode, esta opci¢n permite utilizar para la
asociaci¢n fuentes compatibles con Unicode incluso si la opci¢n ÆUtilizar
siempre UnicodeØ est† activa.
:p.Por omisi¢n, esta opci¢n se encuentra activa; raramente habr† motivo (si lo
hay alguna vez) para desactivarla.
:nt.Aunque esta opci¢n lo haga posible, deber°a evitarse el uso de fuentes
pan-Unicode como ÆTimes New Roman MT 30Ø o ÆArial Unicode MSØ como fuente de
asociaci¢n. Como se dar† a la fuente de asociaci¢n una codificaci¢n CJK
espec°fica del idioma, no se podr†n utilizar los caracteres de la fuente no
soportados por esa codificaci¢n concreta.:ent.

:dt.Utilizar DBCS en vez de Unicode para fuentes combinadas
:dd.Las versiones DBCS (china, japonesa y coreana) de OS/2 incluyen una funci¢n
mediante la cual se pueden redigir las fuentes a une especie de contenedor
virtual llamado fuente &osq.combinada&osq.. (êstas tienen la extensi¢n .CMB y
normalmente se encuentran en el directorio \OS2\SYSDATA.) La fuente combinada
permite que los caracteres se representen mediante im†genes (cargadas de un
archivo separado) a ciertos tama§os; para los caracteres o tama§os que no
tengan disponible una imagen, se utilizar† el archivo original de la fuente.
Sin embargo, esta caracter°stica no funciona correctamente en algunos casos si
la fuente redirigida utiliza la codificaci¢n Unicode.
:p.Si esta opci¢n est† activa, a cada fuente TrueType redirigida a una fuente
combinada (seg£n se define en OS2.INI en la aplicaci¢n ÆPM_ABRFilesØ) se
asignar† siempre una lista de glifos DBCS nativa (china, japonesa o coreana) en
vez de Unicode.
:p.Por omisi¢n, esta opci¢n esta activa.

:dt.Mostrar fuentes Unicode como compatibles con MBCS
:dd.Si esta opci¢n est† activa, cualquier fuente presentada como Unicode se
mostrar† como Æcompatible con MBCSØ. independientemente de cu†ntos glifos
contenga.
:p.La compatibilidad MBCS indica que la fuente contiene soporte extendido de
caracteres; normalmente se recomienda, puesto que de otro modo muchos
caracteres gr†ficos no se podr†n utilizar. Sin embargo, hay algunas desventajas
potenciales si se activa esta opci¢n&colon.
:ul.
:li.La velocidad con que se genere el texto mostrado puede ser un poco menor
para las fuentes Unicode.
:li.Al activar la funci¢n de asociaci¢n DBCS, los
glifos no pueden sustituir texto generado utilizando ninguna fuente Unicode.
(Esta limitaci¢n es de interÇs casi exclusivamente para los usuarios de las
versiones chinas, japonesa o coreana de OS/2.)
:eul.
:edl.

:dt.:hp2.Funciones adicionales:ehp2.
:dd.Estas opciones le permiten activar o desactivar varias funciones
adicionales de FreeType/2.

:dl break=all.
:dt.Ajustar nombres de estilo est†ndar
:dd.Cuando esta opci¢n est† activa, FreeType/2 intenta aplicar las convenciones
de nomenclatura est†ndar a los cuatro ÆestilosØ m†s comunes de fuente&colon.
normal, negrita, cursiva (o it†lica), y negrita cursiva (o cursiva negrita).
FreeType/2 utiliza una l¢gica bastante inteligente para ajustar los nombres de
fuente; sin embargo, es posible que esta opci¢n dÇ como resultado
ocasionalmente fuentes o estilos con un nombre incorrecto. S¢lo se recomienda
activar esta opci¢n si encuentra texto malformado al imprimir desde OpenOffice.

:dt.Crear negrita simulada para fuentes DBCS
:dd.Esta opci¢n hace que FreeType/2 cree una versi¢n negrita ÆfalsaØ (por
simulaci¢n) de cualquier fuente TrueType DBCS que no incluya una verdadera
versi¢n en negrita.

:dt.Optimizar mÇtricas de glifo para Netscape
:dd.Esta opci¢n hace que se ajusten las mÇtricas (dimensiones de los caracteres
y datos de posicionamiento) de todas las fuentes TrueType para evitar
algunos problemas en el posicionamiento de caracteres en versiones antiguas
del navegador Netscape. En general no se recomienda activar esta opci¢n a no
ser que utilice Netscape con frecuencia.

:dt.Utilizar ÆTmsRmnØ como alias de ÆTimes New RomanØ TrueType
:dd.Normalmente, el Presentation Manager autom†ticamente asigna el alias
ÆTmsRmnØ a la fuente tipo 1 ÆTimes New RomanØ (incluida con OS/2). Esto
significa que, al utilizar la fuente de caracteres representados por im†genes
llamada ÆTmsRmnØ, se utiliza la fuente tipo 1 para representar cualquier texto
en tama§os distintos de los 8, 10, 12, 14 y 24 puntos.
:p.Activar esta opci¢n hace que FreeType/2 cree un alias equivalente utilizando
la versi¢n TrueType de ÆTimes New RomanØ si est† instalada. Esto permite que se
utilice la fuente TrueType para generar el texto en los tama§os adicionales si
se desinstala la fuente tipo 1.

:nt.S¢lo se recomienda utilizar esta opci¢n si se desinstala la versi¢n
tipo 1 de ÆTimes New RomanØ y adem†s se instala la versi¢n TrueType (que
se encuentra disponible como parte del paquete ÆCore FontsØ de Microsoft).:ent.

:edl.
:edl.

.* ----------------------------------------------------------------------------
:h2 x=left y=bottom width=100% height=100% res=130.Opciones (2 de 2)
:p.Esta p†gina contiene algunas opciones avanzadas.
:dl break=all.
:dt.:hp2.Opciones avanzadas:ehp2.
:dd.Normalmente no deber°a ser necesario modificar ninguna de estas opciones.
Se recomienda encarecidamente que las mantenga en sus valores por omisi¢n a no
ser que sea completamente consciente de las consecuencias de cambiarlas.
:dl break=all.
:dt.CachÇ de fuentes abiertas
:dd.En vez de abrir todos los tipos de letra al iniciar el sistema, FreeType/2
s¢lo mantiene en memoria a la vez las :hp1.n:ehp1. £ltimas utilizadas.
Esta opci¢n le permite modificar el valor :hp1.n:ehp1..
:dt.Resoluci¢n del texto:
:dd.Le permite cambiar c¢mo define internamente el controlador de fuentes
los ÆpuntosØ al establecer el tama§o del texto. El valor por omisi¢n es el
est†ndar en la industria de 72 puntos por pulgada.
:p.En general, esta opci¢n :hp2.no:ehp2. deber°a cambiarse. Si lo hace, tenga
en mente las siguientes consideraciones importantes:
:ul.
:li.QuÇ constituye una ÆpulgadaØ se define mediante la opci¢n de v°deo de
puntos por pulgada (DPI: dots per inch). Dependiendo del tama§o y resoluci¢n de
su pantalla, esto puede no corresponder exactamente a una pulgada en tÇrminos
f°sicos de longitud.
:li.Esta opci¢n no puede alterar el espacio que el Presentation Manager asigna
en pantalla a cada car†cter. Por tanto, aumentar este valor puede hacer que los
caracteres aparezcan amontonados al dibujarse.
:eul.
:p.Los £nicos valores aceptables son 72, 96 y 120.
:edl.
:edl.

:euserdoc.
