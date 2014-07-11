PUSHD data
IF EXIST ..\data.zip DEL ..\data.zip
..\tools\optipng\optipng -o 7 *.png
..\tools\7z\7za a -tzip -mx=9 -pargonhydroxid267 ..\data.zip *.xml *.png *.ogg *.txt *.dat
POPD