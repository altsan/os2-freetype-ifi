/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs


say "This utility will disable the FreeType/2 font driver and install either "
say "FREETYPE.DLL or the original TRUETYPE.DLL if they exist. If you do not "
say "wish to continue, please just press ENTER. Otherwise please enter ""y"""

pull letter
if letter <> "Y" then exit

bootdrive = SysBootDrive()

app = "PM_Font_Drivers"
key = "TRUETYPE"

/* look for TRUETYPE.DLL */
tt_dll = STREAM( bootdrive'\OS2\DLL\TRUETYPE.DLL', 'C', 'QUERY EXISTS')
ft_dll = STREAM( bootdrive'\OS2\DLL\FREETYPE.DLL', 'C', 'QUERY EXISTS')
SAY
IF tt_dll <> '' & ft_dll <> '' THEN DO
    SAY 'Both FREETYPE.DLL and TRUETYPE.DLL were found.  Which of these '
    SAY 'drivers do you wish to revert to?'
    SAY
    SAY ' 1  -  FREETYPE.DLL (FreeType/2 IFI version 1.x)'
    SAY ' 2  -  TRUETYPE.DLL (original TrueType driver from IBM)'
    SAY
    CALL CHAROUT, 'Select "1" or "2" (or anything else to cancel) >'
    PULL choice
    IF choice == "1" then restore_drv = 'FREETYPE.DLL'
    ELSE IF choice == "2" then restore_drv = 'TRUETYPE.DLL'
    ELSE EXIT
END
ELSE IF ft_dll <> '' THEN restore_drv = 'FREETYPE.DLL'
ELSE IF tt_dll <> '' THEN restore_drv = 'TRUETYPE.DLL'
ELSE restore_drv = ''

IF restore_drv <> '' THEN DO
   SAY 'Restoring' restore_drv '...'
   val = '\OS2\DLL\'restore_drv || d2c(0)
   SysIni('USER', app, key, val)
end
else do
   say "Uninstalling FT2IFI.DLL ..."
   SysIni('USER', app, key, "DELETE:")
end

say "FT2IFI is uninstalled. Please reboot."
pause

