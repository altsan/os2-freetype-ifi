/* */
call RxFuncAdd "SysLoadFuncs", "RexxUtil", "SysLoadFuncs"
call SysLoadFuncs


say "Warning: This is your last chance to back out. If you do not wish to"
say "continue, please just press ENTER. Otherwise please type ""yes"""

pull letter
if letter <> "YES" then exit

bootdrive = SysBootDrive()
IF STREAM(bootdrive"\os2\dll\ft2ifi.dll", 'c', 'query exists') <> '' THEN
    '@unlock' bootdrive'\os2\dll\ft2ifi.dll'
'@'copy "FT2IFI.DLL" bootdrive"\os2\dll"
if rc <> 0 then do
   say "Error: Could not copy file!"
   pause
   exit
end

app = "PM_Font_Drivers"
key = "TRUETYPE"
val = "\OS2\DLL\FT2IFI.DLL" || d2c(0)

SysIni('USER', app, key, val)
say "Font Driver is installed. Please reboot."
pause
return
