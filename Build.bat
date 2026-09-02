@ECHO OFF
REM ===========================================================================
REM  Build.bat - build Blocks 5 from a fresh Git checkout, on the command line.
REM
REM  Usage:  Build.bat [Release^|Debug] [options]
REM
REM    /toolset:vNNN   platform toolset. Without it, the newest one the
REM                    installed Visual Studio provides; see the note below
REM    /sdk:VERSION    Windows SDK version for v141 and newer. Without it, 10.0,
REM                    which MSBuild resolves to the newest installed 10.x
REM    /nodata         do not rebuild data.zip and the skin archives
REM    /optipng        run tools\optipng over the PNGs before packing. Lossless
REM                    but slow, and it rewrites files that are under version
REM                    control, so it is off by default
REM    /stage          also run Blocks5\stage.bat afterwards (Release only)
REM    /rebuild        clean first, then build
REM    /clean          delete every build product and exit without building.
REM                    Both configurations, both the compiler output and the
REM                    packed archives. Nothing else is touched - see the list
REM                    at :doclean below
REM    /run [args]     after a successful build, run the game. Everything after
REM                    /run is passed to blocks5.exe untouched, so it has to come
REM                    last: Build.bat /run -windowed. The game runs with
REM                    Blocks5\ as the working directory, because it opens
REM                    data.zip relative to the current directory
REM    -h, --help, /?  show a short usage summary
REM
REM  ABOUT THE TOOLSET
REM  -----------------
REM  TESTED: v143 (Visual Studio 2022) and v145. Both were built and run. That
REM  is the whole list - anything below v143 is reasoning, not a build.
REM
REM  There is no /toolset: default any more. The three .vcxproj files ask for
REM  $(DefaultPlatformToolset), which is whatever the Visual Studio doing the
REM  build calls its own newest, so a version newer than this script needs no
REM  change here. /toolset:vNNN still pins one explicitly.
REM
REM  This tree was pinned to v120 (Visual Studio 2013) for a decade, not by
REM  choice but by two files: libs\bin\tinyxml_STL.lib and tinyxmld_STL.lib
REM  carried /FAILIFMISMATCH:"_MSC_VER=1800", so link.exe refused any other
REM  toolset with LNK2038. They were the only files in libs\bin with any linker
REM  directive at all. TinyXML 2.6.2 is now compiled from vendored source
REM  instead and those libraries are gone, so the constraint is gone with them.
REM
REM  Getting off v120 needed one more thing, which the tree now has: SDL
REM  compiled from source out of libs\SDL-1.2.15\src, in place of
REM  libs\bin\sdlmain.lib and libs\bin\sdl.lib. sdlmain.lib was pre-UCRT and
REM  imported __iob_func, which the Universal CRT removed, so it linked only on
REM  v120; sdl.dll was the last file in the tree that needed MSVCR120.
REM
REM  PWEncrypt also lost a call to gets(), which the Universal CRT no longer has,
REM  and the tree no longer includes ^<hash_map^> at all - stdext::hash_map and
REM  hash_multimap were replaced by the standard unordered containers, which
REM  every toolset from v120 on has. libs\msinttypes-r26 went with ffmpeg, which
REM  was the only thing that needed it, and __STDC_CONSTANT_MACROS and
REM  __STDC_LIMIT_MACROS went with the shim.
REM
REM  Whether v120 or v140 still builds is an open question - the code has no
REM  dependency that says otherwise, but nobody has tried since the libraries
REM  came out. The /toolset: plumbing for them is kept for whoever does.
REM ===========================================================================

SETLOCAL ENABLEEXTENSIONS
PUSHD "%~dp0"

SET "CONFIG=Release"
REM Both empty means "not asked for": the toolset then comes from the project
REM files ($(DefaultPlatformToolset)) and the SDK from the rule below.
SET "TOOLSET="
SET "WINSDK="
SET "TARGET=Build"
SET "PACKDATA=1"
SET "OPTIPNG=0"
SET "DOSTAGE=0"
SET "DOCLEAN=0"
SET "DORUN=0"
SET "GAMEARGS="

REM --------------------------------------------------------------- arguments
:parseargs
IF "%~1"=="" GOTO argsdone
SET "ARG=%~1"
IF /I "%ARG%"=="Release"  GOTO opt_release
IF /I "%ARG%"=="Debug"    GOTO opt_debug
IF /I "%ARG%"=="/nodata"  GOTO opt_nodata
IF /I "%ARG%"=="/optipng" GOTO opt_optipng
IF /I "%ARG%"=="/stage"   GOTO opt_stage
IF /I "%ARG%"=="/rebuild" GOTO opt_rebuild
IF /I "%ARG%"=="/clean"   GOTO opt_clean
IF /I "%ARG%"=="/run"     GOTO opt_run
IF /I "%ARG%"=="/?"       GOTO usage
IF /I "%ARG%"=="-h"       GOTO usage
IF /I "%ARG%"=="--help"   GOTO usage
IF /I "%ARG:~0,9%"=="/toolset:" GOTO opt_toolset
IF /I "%ARG:~0,5%"=="/sdk:"     GOTO opt_sdk
ECHO ERROR: unknown option "%ARG%".
GOTO usage

:opt_release
SET "CONFIG=Release"
SHIFT
GOTO parseargs
:opt_debug
SET "CONFIG=Debug"
SHIFT
GOTO parseargs
:opt_nodata
SET "PACKDATA=0"
SHIFT
GOTO parseargs
:opt_optipng
SET "OPTIPNG=1"
SHIFT
GOTO parseargs
:opt_stage
SET "DOSTAGE=1"
SHIFT
GOTO parseargs
:opt_rebuild
SET "TARGET=Rebuild"
SHIFT
GOTO parseargs
:opt_clean
SET "DOCLEAN=1"
SHIFT
GOTO parseargs
REM /run swallows the rest of the command line, so that the game's own
REM switches cannot collide with Build.bat's. %1 rather than %~1 below: whatever
REM quoting the caller used is handed on unchanged.
:opt_run
SET "DORUN=1"
SHIFT
:collectgameargs
IF "%~1"=="" GOTO argsdone
SET "GAMEARGS=%GAMEARGS% %1"
SHIFT
GOTO collectgameargs
:opt_toolset
SET "TOOLSET=%ARG:~9%"
SHIFT
GOTO parseargs
:opt_sdk
SET "WINSDK=%ARG:~5%"
SHIFT
GOTO parseargs
:argsdone

IF "%DOCLEAN%"=="1" (
	IF "%DORUN%"=="1" ECHO NOTE: /clean deletes and exits; /run is ignored.
	GOTO doclean
)

REM v141 and newer resolve the Windows SDK themselves and default to 8.1, which
REM is usually not installed any more - hence MSB8036. Pass a version for them;
REM v120 and v140 must NOT get one, they predate the property.
REM
REM Without /toolset: there is nothing to decide here - the project files carry
REM the same rule, written against the toolset they picked themselves - and an
REM empty TOOLSET makes both tests below false by itself. /sdk: still forces a
REM version on either path.
SET "NEEDSDK="
IF DEFINED TOOLSET SET "NEEDSDK=1"
IF /I "%TOOLSET%"=="v120" SET "NEEDSDK="
IF /I "%TOOLSET%"=="v140" SET "NEEDSDK="
IF DEFINED WINSDK SET "NEEDSDK=1"
IF DEFINED NEEDSDK IF NOT DEFINED WINSDK SET "WINSDK=10.0"

REM ------------------------------------------------------- locate MSBuild.exe
REM Quote %ProgramFiles(x86)% everywhere: its value contains parentheses, which
REM would otherwise end an IF block early.
SET "PF86=%ProgramFiles(x86)%"
IF NOT DEFINED PF86 SET "PF86=%ProgramFiles%"

SET "MSBUILD="
SET "VSPATH="

REM Match the MSBuild to the toolset where a matching one is known to exist.
REM MSBuild 12.0 predates v140 entirely and answers /p:PlatformToolset=v140
REM with MSB8020, so it is only ever right for v120. vswhere cannot see Visual
REM C++ Build Tools 2015 at all (microsoft/vswhere#129, wontfix), so a v140-only
REM machine has to be found by probing MSBuild\14.0 directly.
IF /I "%TOOLSET%"=="v120" IF EXIST "%PF86%\MSBuild\12.0\Bin\MSBuild.exe" SET "MSBUILD=%PF86%\MSBuild\12.0\Bin\MSBuild.exe"
IF /I "%TOOLSET%"=="v140" IF EXIST "%PF86%\MSBuild\14.0\Bin\MSBuild.exe" SET "MSBUILD=%PF86%\MSBuild\14.0\Bin\MSBuild.exe"

REM Visual Studio 2017 and newer, including the Build Tools, via vswhere.
REM -requires first: -latest on its own would hand back a newest installation
REM that has no C++ in it - a Visual Studio installed for C# beside an older one
REM that does have the compiler - and the build would then fail a long way from
REM here, with the toolset now coming from whatever vswhere answered.
SET "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT DEFINED MSBUILD IF EXIST "%VSWHERE%" (
	FOR /F "usebackq tokens=*" %%I IN (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) DO SET "VSPATH=%%I"
)
IF NOT DEFINED MSBUILD IF NOT DEFINED VSPATH IF EXIST "%VSWHERE%" (
	FOR /F "usebackq tokens=*" %%I IN (`"%VSWHERE%" -latest -products * -property installationPath`) DO SET "VSPATH=%%I"
)
IF NOT DEFINED MSBUILD IF DEFINED VSPATH IF EXIST "%VSPATH%\MSBuild\Current\Bin\MSBuild.exe" SET "MSBUILD=%VSPATH%\MSBuild\Current\Bin\MSBuild.exe"
IF NOT DEFINED MSBUILD IF DEFINED VSPATH IF EXIST "%VSPATH%\MSBuild\15.0\Bin\MSBuild.exe"  SET "MSBUILD=%VSPATH%\MSBuild\15.0\Bin\MSBuild.exe"
IF NOT DEFINED MSBUILD IF EXIST "%PF86%\MSBuild\14.0\Bin\MSBuild.exe" SET "MSBUILD=%PF86%\MSBuild\14.0\Bin\MSBuild.exe"
IF NOT DEFINED MSBUILD IF EXIST "%PF86%\MSBuild\12.0\Bin\MSBuild.exe" SET "MSBUILD=%PF86%\MSBuild\12.0\Bin\MSBuild.exe"

IF NOT DEFINED MSBUILD (
	ECHO.
	ECHO ERROR: could not find MSBuild.exe. Looked under
	ECHO          "%PF86%\MSBuild\12.0\Bin"
	ECHO          "%PF86%\MSBuild\14.0\Bin"
	ECHO        and asked vswhere for a Visual Studio 2017 or newer install.
	ECHO.
	ECHO        For a compiler-only setup install the "Build Tools for Visual
	ECHO        Studio" of any year from 2022 on - no IDE needed.
	GOTO fail
)

REM ------------------------------------------- is the requested toolset here?
REM Probe the toolset DIRECTORY, not a props file: the per-toolset files are
REM Toolset.props and Toolset.targets under Platforms\Win32\PlatformToolsets\.
REM Only VS2013 and VS2015 register there; VS2017 and newer keep their VCTargets
REM tree inside the installation, so when MSBuild came from vswhere this probe
REM is skipped and MSBuild reports MSB8020 itself if the toolset is absent.
SET "TSDIR="
IF /I "%TOOLSET%"=="v120" SET "TSDIR=%PF86%\MSBuild\Microsoft.Cpp\v4.0\V120\Platforms\Win32\PlatformToolsets\v120"
IF /I "%TOOLSET%"=="v140" SET "TSDIR=%PF86%\MSBuild\Microsoft.Cpp\v4.0\V140\Platforms\Win32\PlatformToolsets\v140"
IF NOT DEFINED TSDIR GOTO toolsetok
IF DEFINED VSPATH   GOTO toolsetok
IF EXIST "%TSDIR%"  GOTO toolsetok
ECHO.
ECHO ERROR: the %TOOLSET% platform toolset is not installed. Expected
ECHO          "%TSDIR%"
ECHO.
ECHO        v120 means a Visual Studio 2013 edition - there has never been a
ECHO        C++-only build-tools package for 2013. v140 comes from "Microsoft
ECHO        Visual C++ Build Tools 2015", or as the "MSVC v140" component of a
ECHO        newer Visual Studio installer.
ECHO.
ECHO        Unless you specifically need an old toolset, drop the /toolset:
ECHO        option and install "Build Tools for Visual Studio 2022" instead.
GOTO fail
:toolsetok

REM ------------------------------------------------------------------- build
REM No /p:PlatformToolset unless one was asked for: left alone, the project
REM files resolve $(DefaultPlatformToolset), which is this Visual Studio's own
REM newest. A global /p: cannot be overridden from inside a project, so passing
REM one here unconditionally would be hardcoding a version all over again.
SET "MSBUILDARGS=/nologo /m /v:minimal /t:%TARGET% /p:Configuration=%CONFIG% /p:Platform=Win32"
IF DEFINED TOOLSET SET "MSBUILDARGS=%MSBUILDARGS% /p:PlatformToolset=%TOOLSET%"
IF DEFINED NEEDSDK SET "MSBUILDARGS=%MSBUILDARGS% /p:WindowsTargetPlatformVersion=%WINSDK%"

REM For the banner only, so that a build log says which compiler produced it:
REM ask MSBuild what the project resolved the toolset to. -getProperty arrived
REM in MSBuild 17.8; an older one prints an error instead, which :onlytoolset
REM throws away so that the line stays vague rather than wrong. One evaluation
REM of one project, no build.
SET "SHOWTS=%TOOLSET%"
IF DEFINED SHOWTS GOTO showtsdone
FOR /F "usebackq delims=" %%T IN (`"%MSBUILD%" "Blocks5\Blocks5.vcxproj" -nologo -getProperty:PlatformToolset -p:Configuration=%CONFIG% -p:Platform=Win32 2^>NUL`) DO SET "SHOWTS=%%T"
CALL :onlytoolset
:showtsdone

ECHO.
ECHO === Building Blocks5.sln [%CONFIG%^|Win32] ===
IF DEFINED TOOLSET ECHO     Toolset: %TOOLSET% (asked for with /toolset:)
IF NOT DEFINED TOOLSET IF DEFINED SHOWTS     ECHO     Toolset: %SHOWTS% (the newest this Visual Studio has)
IF NOT DEFINED TOOLSET IF NOT DEFINED SHOWTS ECHO     Toolset: the newest this Visual Studio has
ECHO     MSBuild: %MSBUILD%
IF DEFINED NEEDSDK ECHO     Windows SDK: %WINSDK%
ECHO.
"%MSBUILD%" Blocks5.sln %MSBUILDARGS%
IF ERRORLEVEL 1 (
	ECHO.
	ECHO ERROR: the build failed.
	GOTO fail
)

IF NOT EXIST "%CONFIG%\blocks5.exe" (
	ECHO.
	ECHO ERROR: MSBuild reported success but "%CONFIG%\blocks5.exe" is not there.
	GOTO fail
)

REM ------------------------------------------------------------ pack the data
REM data.zip and levels\skins\*.zip are build products, not checked in, and the
REM game cannot start without them. Repacked on every build so that an edit to
REM data\ cannot be left behind - pass /nodata to skip.
IF "%PACKDATA%"=="0" GOTO nodata

ECHO.
ECHO === Packing data.zip and the skin archives ===
IF "%OPTIPNG%"=="1" ECHO     (optipng is on - this takes several minutes)
PUSHD Blocks5
IF "%OPTIPNG%"=="1" (
	CALL zip_data.bat
	CALL zip_skins.bat
) ELSE (
	CALL zip_data_no_optipng.bat
	CALL zip_skins_no_optipng.bat
)
POPD

SET "MISSING="
IF NOT EXIST "Blocks5\data.zip"                    SET "MISSING=%MISSING% data.zip"
IF NOT EXIST "Blocks5\levels\skins\blocks_01.zip"  SET "MISSING=%MISSING% blocks_01.zip"
IF NOT EXIST "Blocks5\levels\skins\blocks_02.zip"  SET "MISSING=%MISSING% blocks_02.zip"
IF NOT EXIST "Blocks5\levels\skins\blocks_03.zip"  SET "MISSING=%MISSING% blocks_03.zip"
IF NOT EXIST "Blocks5\levels\skins\space.zip"      SET "MISSING=%MISSING% space.zip"
IF DEFINED MISSING (
	ECHO.
	ECHO ERROR: packing did not produce:%MISSING%
	ECHO        tools\7za.exe is needed for this step.
	GOTO fail
)
:nodata

REM ----------------------------------------------------------------- staging
IF "%DOSTAGE%"=="0" GOTO done
IF /I NOT "%CONFIG%"=="Release" (
	ECHO.
	ECHO NOTE: /stage skipped - stage.bat copies from ..\Release, so it only
	ECHO       makes sense for a Release build.
	GOTO done
)
ECHO.
ECHO === Staging into Blocks5\stage ===
PUSHD Blocks5
CALL stage.bat
POPD

:done
ECHO.
ECHO === Done ===
ECHO     %CD%\%CONFIG%\blocks5.exe
IF "%DORUN%"=="1" GOTO rungame
ECHO.
ECHO     Run it with Blocks5\ as the working directory - the game opens
ECHO     data.zip relative to the current directory:
ECHO.
ECHO         cd Blocks5
ECHO         ..\%CONFIG%\blocks5.exe -windowed
ECHO.
ECHO     Or let Build.bat do it:  Build.bat /run -windowed
ECHO.
POPD
ENDLOCAL
EXIT /B 0

REM -------------------------------------------------------------- /run
REM Exactly what the message above describes: Blocks5\ as the working directory,
REM because FileSystem opens data.zip relative to it. A Release build is a
REM Windows-subsystem binary, so cmd does not wait for it and the prompt comes
REM back immediately; a Debug build is Console subsystem and does block here.
:rungame
IF NOT EXIST "%CONFIG%\blocks5.exe" (
	ECHO.
	ECHO ERROR: %CONFIG%\blocks5.exe is not there, so /run has nothing to run.
	GOTO fail
)
ECHO.
ECHO === Running blocks5.exe%GAMEARGS% ===
PUSHD Blocks5
"..\%CONFIG%\blocks5.exe"%GAMEARGS%
SET "GAMEEXIT=%ERRORLEVEL%"
POPD
POPD
ENDLOCAL & EXIT /B %GAMEEXIT%

REM ------------------------------------------------------------------- clean
REM Everything removed here is a build product: MSBuild writes it, or
REM zip_data.bat / zip_skins.bat / stage.bat do, and none of it is under version
REM control. Both configurations go, not just the one named on the command line.
REM
REM Deliberately NOT touched:
REM   *.suo, *.vcxproj.user   the IDE's per-user settings - debugger arguments,
REM                           working directory. Regenerating those loses work,
REM                           and they are not compiler output
REM   levels\campaigns\blocks.zip and misc\3p_campaigns\*.zip
REM                           shipped files that happen to be archives. This is
REM                           why the skin archives below are named one by one
REM                           instead of matched with a wildcard
REM   My Documents\Blocks 5\   saves, progress, screenshots, videos. Nothing the
REM                           build ever wrote
:doclean
IF NOT EXIST "Blocks5.sln" (
	ECHO.
	ECHO ERROR: Blocks5.sln is not next to this script, so this is not the
	ECHO        Blocks 5 tree - refusing to delete anything.
	GOTO fail
)

ECHO.
ECHO === Cleaning build products ===
ECHO.
SET "REMOVED=0"
SET "CLEANFAILED="

REM OutDir is $(SolutionDir)$(Configuration) and IntDir is $(Configuration)
REM under each project, so the compiler output lands in eight directories.
CALL :rmdir "Release"
CALL :rmdir "Debug"
CALL :rmdir "Blocks5\Release"
CALL :rmdir "Blocks5\Debug"
CALL :rmdir "PWEncrypt\Release"
CALL :rmdir "PWEncrypt\Debug"
CALL :rmdir "ShowUserDir\Release"
CALL :rmdir "ShowUserDir\Debug"

REM stage.bat
CALL :rmdir "Blocks5\stage"

REM zip_data.bat and zip_skins.bat
CALL :rmfile "Blocks5\data.zip"
CALL :rmfile "Blocks5\levels\skins\blocks_01.zip"
CALL :rmfile "Blocks5\levels\skins\blocks_02.zip"
CALL :rmfile "Blocks5\levels\skins\blocks_03.zip"
CALL :rmfile "Blocks5\levels\skins\space.zip"

REM IntelliSense and browse-information caches. Pure caches, rebuilt on demand,
REM and large enough to be worth removing.
CALL :rmdir ".vs"
CALL :rmdir "ipch"
IF EXIST *.sdf (
	ECHO     del    *.sdf
	DEL /F /Q *.sdf
	SET /A REMOVED+=1
)
IF EXIST *.opensdf (
	ECHO     del    *.opensdf
	DEL /F /Q *.opensdf
	SET /A REMOVED+=1
)

ECHO.
IF DEFINED CLEANFAILED GOTO cleanfailed
IF "%REMOVED%"=="0"     ECHO     Nothing to remove - the tree was already clean.
IF NOT "%REMOVED%"=="0" ECHO     Removed %REMOVED% entries.
ECHO.
POPD
ENDLOCAL
EXIT /B 0

:cleanfailed
ECHO ERROR: some build products could not be removed. Close whatever still has
ECHO        them open - a running blocks5.exe, Visual Studio, an editor - and
ECHO        run Build.bat /clean again.
ECHO.
POPD
ENDLOCAL
EXIT /B 1

:usage
ECHO.
ECHO Usage: Build.bat [Release^|Debug] [/toolset:vNNN] [/sdk:VERSION]
ECHO                  [/nodata] [/optipng] [/stage] [/rebuild]
ECHO                  [/run [arguments for the game]]
ECHO        Build.bat /clean
ECHO.
ECHO Builds Blocks5.sln for Win32 with the newest toolset this Visual Studio
ECHO has - tested with v143 and v145 - then packs data.zip and the skin
ECHO archives, which are not in Git.
ECHO The /clean option removes all of that again, for both configurations.
ECHO.
ECHO /run runs the game afterwards, with Blocks5\ as the working directory.
ECHO It must come last: everything after it goes to blocks5.exe untouched.
ECHO     Build.bat /run -windowed
ECHO     Build.bat Debug /rebuild /run -windowed
ECHO.
ECHO Open this file in an editor for the toolset notes.
ECHO.
POPD
ENDLOCAL
EXIT /B 2

:fail
ECHO.
POPD
ENDLOCAL
EXIT /B 1

REM Keep SHOWTS only if it is a plain vNNN. Everything else - an MSB1001 from an
REM MSBuild too old to know -getProperty, a v143_xp, a ClangCL - is not a name
REM this banner should be printing.
:onlytoolset
IF NOT DEFINED SHOWTS GOTO :EOF
IF /I NOT "%SHOWTS:~0,1%"=="v" SET "SHOWTS="
IF DEFINED SHOWTS IF NOT "%SHOWTS:~5%"=="" SET "SHOWTS="
GOTO :EOF

REM ------------------------------------------------------------ clean helpers
REM Called, not jumped to, and neither of them does SETLOCAL, so REMOVED and
REM CLEANFAILED are the caller's variables.
:rmdir
IF NOT EXIST "%~1" GOTO :EOF
ECHO     rmdir  %~1
RMDIR /S /Q "%~1"
IF EXIST "%~1" (
	ECHO            ... could not be removed
	SET "CLEANFAILED=1"
	GOTO :EOF
)
SET /A REMOVED+=1
GOTO :EOF

:rmfile
IF NOT EXIST "%~1" GOTO :EOF
ECHO     del    %~1
DEL /F /Q "%~1"
IF EXIST "%~1" (
	ECHO            ... could not be deleted
	SET "CLEANFAILED=1"
	GOTO :EOF
)
SET /A REMOVED+=1
GOTO :EOF
