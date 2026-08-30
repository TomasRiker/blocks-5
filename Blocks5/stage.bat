IF EXIST stage RMDIR /S /Q stage
IF NOT EXIST stage MKDIR stage
COPY .update_checker stage
COPY "Blocks 5 Website.url" stage
COPY "Scherfgen-Software Website.url" stage
COPY ..\Release\blocks5.exe stage
COPY data.zip stage
COPY "Donate (de).url" stage
COPY "Donate (en).url" stage
COPY OpenAL32.dll stage
COPY ..\Release\pwencrypt.exe stage
COPY readme.txt stage
COPY ..\Release\showuserdir.exe stage
COPY update_checker_disable.bat stage
COPY update_checker_enable.bat stage
COPY windowed.bat stage
MKDIR stage\levels
COPY levels\example01.xml stage\levels
COPY levels\example02.xml stage\levels
COPY levels\readme.txt stage\levels
MKDIR stage\levels\campaigns
COPY levels\campaigns\blocks.zip stage\levels\campaigns
MKDIR stage\levels\skins
COPY levels\skins\blocks_01.zip stage\levels\skins
COPY levels\skins\blocks_02.zip stage\levels\skins
COPY levels\skins\blocks_03.zip stage\levels\skins
COPY levels\skins\space.zip stage\levels\skins
MKDIR stage\screenshots
COPY screenshots\readme.txt stage\screenshots
MKDIR stage\videos
COPY videos\readme.txt stage\videos