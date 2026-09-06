REM Die XML-Dateien nehmen den Umweg ueber ein Zwischenverzeichnis, in dem
REM Tools\strip_xml_comments.py ihre Kommentare entfernt hat: die Notizen in den
REM Dialogen sollen in den Quelldateien stehen bleiben, aber nicht im Archiv.
REM Daher zwei Aufrufe an 7za - der zweite haengt an, wie bei den Skins auch.
REM
REM Die Suche nach Python laeuft ueber zwei Zeilen, weil ein IF mit ERRORLEVEL
REM in einem geklammerten Block erst zur Laufzeit zaehlt und diese Datei keine
REM Bloecke haben soll: %PY% steht beim Lesen jeder Zeile schon auf dem Stand
REM der vorigen.
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
..\tools\optipng -o 7 *.png
..\tools\7za a -tzip -mx=9 -pargonhydroxid267 ..\data.zip *.png *.ogg *.txt *.dat
POPD
PUSHD "%STAGE%"
"%~dp0tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml
POPD
RD /S /Q "%STAGE%"
