REM The tools are named through %~dp0 so that the PUSHD into each
REM skin folder does not have to be counted out in "..".
REM hintscroll.txt is named here rather than swept up as *.txt:
REM password.txt must stay unencrypted and is packed on its own.
REM Another skin with this marker file needs it named in its own line.
PUSHD levels\skins\blocks_01
IF EXIST ..\blocks_01.zip DEL ..\blocks_01.zip
"%~dp0..\Tools\7za" a -tzip -mx=9 -ptrockeneiskaefer ..\blocks_01.zip *.xml *.png hintscroll.txt
"%~dp0..\Tools\7za" a -tzip -mx=9 ..\blocks_01.zip password.txt
POPD
PUSHD levels\skins\blocks_02
IF EXIST ..\blocks_02.zip DEL ..\blocks_02.zip
"%~dp0..\Tools\7za" a -tzip -mx=9 -ptrockeneiskaefer ..\blocks_02.zip *.xml *.png
"%~dp0..\Tools\7za" a -tzip -mx=9 ..\blocks_02.zip password.txt
POPD
PUSHD levels\skins\blocks_03
IF EXIST ..\blocks_03.zip DEL ..\blocks_03.zip
"%~dp0..\Tools\7za" a -tzip -mx=9 -ptrockeneiskaefer ..\blocks_03.zip *.xml *.png
"%~dp0..\Tools\7za" a -tzip -mx=9 ..\blocks_03.zip password.txt
POPD
PUSHD levels\skins\space
IF EXIST ..\space.zip DEL ..\space.zip
"%~dp0..\Tools\7za" a -tzip -mx=9 ..\space.zip *.xml *.png
POPD