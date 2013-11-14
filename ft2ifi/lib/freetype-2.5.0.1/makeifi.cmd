@echo off
SETLOCAL
SET C_INCLUDE_PATH=E:/Development/ft2ifi/lib/freetype-2.5.0.1/ifi;%C_INCLUDE_PATH$%
make 2>&1 |tee build.log
ENDLOCAL
