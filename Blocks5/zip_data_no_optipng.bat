REM Wie zip_data.bat, nur ohne den langsamen optipng-Schritt. Zu den beiden
REM 7za-Aufrufen und zur Suche nach Python steht dort, warum sie so aussehen.
SETLOCAL
SET "PY=py -3"
%PY% -c "" >NUL 2>&1 || SET "PY=python"
%PY% -c "" >NUL 2>&1 || SET "PY="
IF NOT DEFINED PY ECHO Python fehlt - es nimmt die Kommentare aus den XML-Dateien.
IF NOT DEFINED PY ECHO Zu holen bei https://www.python.org/downloads/
IF NOT DEFINED PY EXIT /B 1

SET "STAGE=%TEMP%\blocks5-xml"
IF EXIST "%STAGE%" RD /S /Q "%STAGE%"
%PY% ..\Tools\strip_xml_comments.py --out "%STAGE%" data
IF ERRORLEVEL 1 EXIT /B 1

PUSHD data
IF EXIST ..\data.zip DEL ..\data.zip
..\tools\7za a -tzip -mx=9 -pargonhydroxid267 ..\data.zip *.png *.ogg *.txt *.dat
POPD
PUSHD "%STAGE%"
"%~dp0tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml
POPD
RD /S /Q "%STAGE%"
