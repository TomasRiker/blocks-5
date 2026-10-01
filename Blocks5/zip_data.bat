REM zip_data.bat [/optipng]
REM
REM /optipng runs Tools\optipng over data\*.png first: lossless but slow, and it
REM rewrites files that are under version control, so it is off unless asked
REM for. Build.bat passes it on for its own /optipng. Anything else is refused
REM before a file is touched.
REM
REM RUN_OPTIPNG and not OPTIPNG, and cleared first: SETLOCAL only keeps this
REM script's variables from leaking out, not the caller's from leaking in, and
REM Build.bat CALLs this with an OPTIPNG of its own that is 0 when off - which
REM IF DEFINED would count as on.
REM
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
SETLOCAL
SET "RUN_OPTIPNG="
IF /I "%~1"=="/optipng" SET "RUN_OPTIPNG=1"
IF NOT "%~1"=="" IF NOT DEFINED RUN_OPTIPNG ECHO   Unknown argument %1 - the only one is /optipng.
IF NOT "%~1"=="" IF NOT DEFINED RUN_OPTIPNG EXIT /B 2

REM The py launcher first, then python on the path, each asked with a run that
REM has nothing to do.
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
IF DEFINED RUN_OPTIPNG "%~dp0..\Tools\optipng" -o 7 *.png
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.png *.ogg *.dat
POPD
PUSHD "%STRIPPED%"
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml *.txt
POPD
IF DEFINED PY RD /S /Q "%STAGE%"
