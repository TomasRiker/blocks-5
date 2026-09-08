REM Packs this one skin, run from inside its own folder. zip_skins.bat does the
REM same for all four from Blocks5\ and is what Build.bat calls.
del ..\space.zip
..\..\..\..\Tools\optipng -o 7 *.png
..\..\..\..\Tools\7za a -tzip -mx=9 ..\space.zip *.xml *.png
