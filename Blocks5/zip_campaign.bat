REM Rebuilds levels\campaigns\blocks.zip out of its sources.
REM
REM It has to be run after a level changes, and the reason is easy to miss:
REM Campaign::load() serves the levels loose wherever all of them lie in
REM levels\, which is true in a working tree and never on an installed game -
REM stage.bat ships this archive and the two examples, not the 42 sources. So a
REM level edited and not packed changes what a developer sees and nothing a
REM player sees, with no error anywhere.
REM
REM Every member has a source in the tree and none is taken out of the archive
REM being replaced: the levels are levels\level_NN.xml, the ten music tracks
REM are the loose ones beside them, and campaign.xml lies in a folder of its
REM own next to the archive, the way a skin's sources do.
REM
REM The member names come from the index rather than from the text in
REM campaign.xml - makeMemberName() reads entry i as level_{i+1}.xml - so the
REM two loops below number them the same way instead of copying the names.
SETLOCAL
SET "STAGE=%TEMP%\blocks5-campaign"
SET "ARCHIVE=%~dp0levels\campaigns\blocks.zip"
IF EXIST "%STAGE%" RD /S /Q "%STAGE%"
MKDIR "%STAGE%"

COPY "%~dp0levels\campaigns\blocks\campaign.xml" "%STAGE%" >NUL
IF ERRORLEVEL 1 ECHO   ERROR: levels\campaigns\blocks\campaign.xml is missing
IF ERRORLEVEL 1 EXIT /B 1
COPY "%~dp0levels\music*.ogg" "%STAGE%" >NUL
IF ERRORLEVEL 1 ECHO   ERROR: the music next to the levels is missing
IF ERRORLEVEL 1 EXIT /B 1

FOR /L %%i IN (1,1,9) DO COPY "%~dp0levels\level_0%%i.xml" "%STAGE%\level_%%i.xml" >NUL
FOR /L %%i IN (10,1,42) DO COPY "%~dp0levels\level_%%i.xml" "%STAGE%\level_%%i.xml" >NUL

PUSHD "%STAGE%"
IF EXIST "%ARCHIVE%" DEL "%ARCHIVE%"
"%~dp0..\Tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%ARCHIVE%" campaign.xml level_*.xml *.ogg >NUL
POPD
RD /S /Q "%STAGE%"
