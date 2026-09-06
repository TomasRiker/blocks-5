REM Wie zip_data.bat, nur ohne den langsamen optipng-Schritt. Zu den beiden
REM 7za-Aufrufen, zur Suche nach Python und dazu, warum sein Fehlen den Build
REM nicht anhaelt, steht dort das Noetige.
SETLOCAL
SET "PY="
py -3 -c "" >NUL 2>&1
IF NOT ERRORLEVEL 1 SET "PY=py -3"
IF NOT DEFINED PY python -c "" >NUL 2>&1
IF NOT DEFINED PY IF NOT ERRORLEVEL 1 SET "PY=python"
IF NOT DEFINED PY ECHO   Hinweis: Python fehlt, die Kommentare bleiben in den XML-Dateien.
IF NOT DEFINED PY ECHO            Zu holen bei https://www.python.org/downloads/

SET "STAGE=%TEMP%\blocks5-xml"
SET "XMLSRC=data"
IF EXIST "%STAGE%" RD /S /Q "%STAGE%"
IF DEFINED PY %PY% ..\Tools\strip_xml_comments.py --out "%STAGE%" data
IF DEFINED PY IF ERRORLEVEL 1 EXIT /B 1
IF DEFINED PY SET "XMLSRC=%STAGE%"

PUSHD data
IF EXIST ..\data.zip DEL ..\data.zip
..\tools\7za a -tzip -mx=9 -pargonhydroxid267 ..\data.zip *.png *.ogg *.txt *.dat
POPD
PUSHD "%XMLSRC%"
"%~dp0tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml
POPD
IF DEFINED PY RD /S /Q "%STAGE%"
