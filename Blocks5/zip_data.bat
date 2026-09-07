REM The XML files take the detour through a staging directory in which
REM Tools\strip_xml_comments.py has removed their comments: the notes in the
REM dialogs belong in the source files, but not in the archive. Hence two calls
REM to 7za - the second one appends, as with the skins.
REM
REM Without Python the XML files come straight out of data\ and carry their
REM comments with them. That is worth a note and no reason to stop the build.
REM
REM The py launcher first, then python on the path. Asked with a run that has
REM nothing to do, and checked with IF ERRORLEVEL rather than with ||: single
REM line IFs only, no bracketed blocks and no GOTO - this file, like the rest
REM of the tree, has no Windows-style line endings, and cmd miscounts when it
REM jumps in such a file.
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
