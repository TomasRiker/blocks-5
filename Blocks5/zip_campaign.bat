REM Rebuilds levels\campaigns\blocks.zip out of the level sources next to it.
REM
REM Unlike data.zip and the skin archives this one IS checked in, because the
REM installer ships it and nothing else in the tree can produce the music it
REM carries. It still has to be rebuilt after a level changes, and the reason is
REM easy to miss: Campaign::load() serves the levels loose wherever all of them
REM lie in levels\, which is true in a working tree and never on an installed
REM game - stage.bat ships this archive and the two examples, not the 42
REM sources. So a level edited and not packed changes what a developer sees and
REM nothing a player sees, with no error anywhere.
REM
REM The member names come from the index rather than from the text in
REM campaign.xml - makeMemberName() reads entry i as level_{i+1}.xml - so the
REM two loops below number them the same way instead of copying the names.
REM
REM campaign.xml and the music are taken out of the archive that is already
REM there: neither is generated from anything, and the tracks have no source
REM beside them. That extract therefore has to succeed before the old archive
REM is deleted.
SETLOCAL
SET "STAGE=%TEMP%\blocks5-campaign"
SET "ARCHIVE=%~dp0levels\campaigns\blocks.zip"
IF EXIST "%STAGE%" RD /S /Q "%STAGE%"
MKDIR "%STAGE%"

"%~dp0tools\7za" x -y -pargonhydroxid267 -o"%STAGE%" "%ARCHIVE%" campaign.xml *.ogg >NUL
IF ERRORLEVEL 1 ECHO   ERROR: could not read campaign.xml and the music out of blocks.zip
IF ERRORLEVEL 1 EXIT /B 1

FOR /L %%i IN (1,1,9) DO COPY "%~dp0levels\level_0%%i.xml" "%STAGE%\level_%%i.xml" >NUL
FOR /L %%i IN (10,1,42) DO COPY "%~dp0levels\level_%%i.xml" "%STAGE%\level_%%i.xml" >NUL

PUSHD "%STAGE%"
IF EXIST "%ARCHIVE%" DEL "%ARCHIVE%"
"%~dp0tools\7za" a -tzip -mx=9 -pargonhydroxid267 "%ARCHIVE%" campaign.xml level_*.xml *.ogg >NUL
POPD
RD /S /Q "%STAGE%"
