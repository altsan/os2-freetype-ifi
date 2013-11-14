/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs

/* Find drive where OS/2 is installed        */
bootdrive = SysSearchPath('PATH', 'OS2.INI')
/* say os2path */
bootdrive = left(bootdrive, 2)

repmod "FREETYPE.DLL " || bootdrive || "\os2\dll\FREETYPE.DLL"
pause
