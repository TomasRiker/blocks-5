REM Like zip_data.bat, only without the slow optipng step. The two 7za calls,
REM the search for Python, why its absence does not stop the build and why
REM everything outside data\ is named through %~dp0 are explained there.
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
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.png *.ogg *.dat
POPD
PUSHD "%STRIPPED%"
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%~dp0data.zip" *.xml *.txt
POPD
IF DEFINED PY RD /S /Q "%STAGE%"
