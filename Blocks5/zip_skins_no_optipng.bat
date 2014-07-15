PUSHD levels\skins\blocks_01
IF EXIST ..\blocks_01.zip DEL ..\blocks_01.zip
..\..\..\tools\7za a -tzip -mx=9 -ptrockeneiskaefer ..\blocks_01.zip *.xml *.png
..\..\..\tools\7za a -tzip -mx=9 ..\blocks_01.zip password.txt
POPD
PUSHD levels\skins\blocks_02
IF EXIST ..\blocks_02.zip DEL ..\blocks_02.zip
..\..\..\tools\7za a -tzip -mx=9 -ptrockeneiskaefer ..\blocks_02.zip *.xml *.png
..\..\..\tools\7za a -tzip -mx=9 ..\blocks_02.zip password.txt
POPD
PUSHD levels\skins\blocks_03
IF EXIST ..\blocks_03.zip DEL ..\blocks_03.zip
..\..\..\tools\7za a -tzip -mx=9 -ptrockeneiskaefer ..\blocks_03.zip *.xml *.png
..\..\..\tools\7za a -tzip -mx=9 ..\blocks_03.zip password.txt
POPD
PUSHD levels\skins\space
IF EXIST ..\space.zip DEL ..\space.zip
..\..\..\tools\7za a -tzip -mx=9 ..\space.zip *.xml *.png
POPD