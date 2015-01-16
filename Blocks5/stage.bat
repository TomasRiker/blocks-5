IF EXIST stage RMDIR /S /Q stage
IF NOT EXIST stage MKDIR stage
COPY .update_checker stage
COPY _config_de.xml stage
COPY _config_en.xml stage
COPY avcodec-53.dll stage
COPY avformat-53.dll stage
COPY avutil-51.dll stage
COPY "Blocks 5 Website.url" stage
COPY ..\Release\blocks5.exe stage
COPY config.xml stage
COPY data.zip stage
COPY "Donate (de).url" stage
COPY "Donate (en).url" stage
COPY hq2x.bat stage
COPY libogg.dll stage
COPY libpng15-15.dll stage
COPY libvorbis.dll stage
COPY libvorbisfile.dll stage
COPY makeconfig.bat stage
COPY oalinst.exe stage
COPY ogg.dll stage
COPY ..\Release\pwencrypt.exe stage
COPY readme.txt stage
COPY sdl.dll stage
COPY sdl_image.dll stage
COPY ..\Release\showuserdir.exe stage
COPY swscale-2.dll stage
COPY update_checker_disable.bat stage
COPY update_checker_enable.bat stage
COPY vcredist_x86.exe stage
COPY windowed.bat stage
COPY zlib1.dll stage
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