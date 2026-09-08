REM The XML and the text files take the detour through a staging directory in
REM which Tools\strip_comments.py has removed their comments: the notes in the
REM dialogs and in languages.txt belong in the source files, but not in the
REM archive. Hence two calls to 7za - the second one appends, as with the skins.
REM
REM Without Python those files come straight out of data\ and carry their
REM comments with them. That is worth a note and no reason to stop the build.
REM
REM Everything outside data\ is named through %~dp0 and never relative to the
REM current directory, because the second 7za call runs from %TEMP% whenever
REM Python is there. One idiom throughout rather than two that differ by three
REM lines with nothing saying why.
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
IF NOT DEFINED PY ECHO   NOTE: Python is missing, the comments stay in the packed files.
IF NOT DEFINED PY ECHO         Get it from https://www.python.org/downloads/

SET "STAGE=%TEMP%\blocks5-strip"
SET "STRIPPED=%~dp0data"
IF EXIST "%STAGE%" RD /S /Q "%STAGE%"
IF DEFINED PY %PY% "%~dp0..\Tools\strip_comments.py" --out "%STAGE%" "%~dp0data"
IF DEFINED PY IF ERRORLEVEL 1 EXIT /B 1
IF DEFINED PY SET "STRIPPED=%STAGE%"

PUSHD "%~dp0data"
IF EXIST "%~dp0data.zip" DEL "%~dp0data.zip"
"%~dp0..\Tools\optipng" -o 7 *.png
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.png *.ogg *.dat
POPD
PUSHD "%STRIPPED%"
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml *.txt
POPD
IF DEFINED PY RD /S /Q "%STAGE%"
