REM Die XML-Dateien nehmen den Umweg ueber ein Zwischenverzeichnis, in dem
REM Tools\strip_xml_comments.py ihre Kommentare entfernt hat: die Notizen in den
REM Dialogen sollen in den Quelldateien stehen bleiben, aber nicht im Archiv.
REM Daher zwei Aufrufe an 7za - der zweite haengt an, wie bei den Skins auch.
REM
REM Ohne Python bleibt es beim alten Verhalten: dann kommen die XML-Dateien
REM unmittelbar aus data\ und tragen ihre Kommentare mit. Das ist eine Notiz
REM wert und kein Grund, den Build anzuhalten.
REM
REM Erst der Starter py, dann python auf dem Pfad. Gefragt wird mit einem Lauf
REM ohne Aufgabe, und geprueft mit IF ERRORLEVEL statt mit ||: nur einzeilige
REM IFs, keine geklammerten Bloecke und kein GOTO - diese Datei hat, wie der
REM Rest des Baums, keine Zeilenenden nach Windows-Art, und cmd verzaehlt sich
REM darauf beim Springen.
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
..\tools\optipng -o 7 *.png
..\tools\7za a -tzip -mx=9 -pargonhydroxid267 ..\data.zip *.png *.ogg *.txt *.dat
POPD
PUSHD "%XMLSRC%"
"%~dp0tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml
POPD
IF DEFINED PY RD /S /Q "%STAGE%"
