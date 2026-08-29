@ECHO OFF
REM ===========================================================================
REM  build.bat - build Blocks 5 from a fresh Git checkout, on the command line.
REM
REM  Usage:  build.bat [Release^|Debug] [options]
REM
REM    /nodata         do not rebuild data.zip and the skin archives
REM    /optipng        run tools\optipng over the PNGs before packing. Lossless
REM                    but slow, and it rewrites files that are under version
REM                    control, so it is off by default
REM    /stage          also run Blocks5\stage.bat afterwards (Release only)
REM    /rebuild        clean first, then build
REM    /toolset:vNNN   override the platform toolset - read the note below
REM                    before using this
REM    /?              show this text
REM
REM  WHY v120 (Visual Studio 2013), AND NOTHING NEWER
REM  ------------------------------------------------
REM  libs\bin\tinyxml_STL.lib and tinyxmld_STL.lib were compiled by VS2013 and
REM  carry the linker directive
REM
REM      /FAILIFMISMATCH:"_MSC_VER=1800"
REM
REM  which the VS2013 C++ headers emit. Any other toolset makes link.exe stop
REM  with LNK2038, "mismatch detected for '_MSC_VER'". That is deliberate and
REM  it is not a warning to wave through: VS2013's C++ ABI is not compatible
REM  with VS2015 and later, and TinyXML has std::string in its interface. The
REM  same two libraries also pin the runtime they were built against -
REM  RuntimeLibrary=MD_DynamicRelease with _ITERATOR_DEBUG_LEVEL=0 for the
REM  release one, MDd_DynamicDebug with _ITERATOR_DEBUG_LEVEL=2 for the debug
REM  one - and the projects already match both.
REM
REM  Nothing else in libs\bin pins anything: those two are the only files in it
REM  that carry a FAILIFMISMATCH directive at all. sdlmain.lib, zlibstat.lib and
REM  zlibstatd.lib are C static libraries with no _MSC_VER lock; sdl.lib,
REM  SDL_image.lib, OpenAL32.lib, libogg.lib, libvorbis*.lib and the ffmpeg
REM  libraries are plain import libraries; and hq2x32.obj references exactly two
REM  symbols, _LUT16to32 and _RGBtoYUV, both of which this project defines
REM  itself. So TinyXML alone is what holds the build at VS2013.
REM
REM  Moving to a modern toolset therefore means rebuilding those two libraries
REM  from the TinyXML 2.6.2 sources - only its headers are vendored here - and
REM  then passing /toolset:v143. Expect zlibstat.lib and sdlmain.lib to want
REM  legacy_stdio_definitions.lib and a __iob_func shim at that point: both were
REM  built against the pre-UCRT runtime, and sdlmain.lib imports __iob_func,
REM  which the UCRT no longer has.
REM ===========================================================================

SETLOCAL ENABLEEXTENSIONS
PUSHD "%~dp0"

SET "CONFIG=Release"
SET "TOOLSET="
SET "TARGET=Build"
SET "PACKDATA=1"
SET "OPTIPNG=0"
SET "DOSTAGE=0"

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
IF /I "%ARG%"=="/?"       GOTO usage
IF /I "%ARG%"=="-h"       GOTO usage
IF /I "%ARG%"=="--help"   GOTO usage
IF /I "%ARG:~0,9%"=="/toolset:" GOTO opt_toolset
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
:opt_toolset
SET "TOOLSET=%ARG:~9%"
SHIFT
GOTO parseargs
:argsdone

REM ------------------------------------------------------- locate MSBuild.exe
REM Quote %ProgramFiles(x86)% everywhere: its value contains parentheses, which
REM would otherwise end an IF block early.
SET "PF86=%ProgramFiles(x86)%"
IF NOT DEFINED PF86 SET "PF86=%ProgramFiles%"

SET "MSBUILD="
SET "VSPATH="

REM Prefer Visual Studio 2013's own MSBuild. The projects declare
REM ToolsVersion 12.0, and the v120 toolset only exists where VS2013 (or its
REM build tools) is installed, so this is the copy that is certain to match.
IF EXIST "%PF86%\MSBuild\12.0\Bin\MSBuild.exe" SET "MSBUILD=%PF86%\MSBuild\12.0\Bin\MSBuild.exe"

REM Otherwise the newest MSBuild on the machine. It can still drive v120, as
REM long as VS2013's C++ tools are installed alongside.
SET "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT DEFINED MSBUILD IF EXIST "%VSWHERE%" (
	FOR /F "usebackq tokens=*" %%I IN (`"%VSWHERE%" -latest -products * -property installationPath`) DO SET "VSPATH=%%I"
)
IF NOT DEFINED MSBUILD IF DEFINED VSPATH IF EXIST "%VSPATH%\MSBuild\Current\Bin\MSBuild.exe" SET "MSBUILD=%VSPATH%\MSBuild\Current\Bin\MSBuild.exe"
IF NOT DEFINED MSBUILD IF DEFINED VSPATH IF EXIST "%VSPATH%\MSBuild\15.0\Bin\MSBuild.exe"  SET "MSBUILD=%VSPATH%\MSBuild\15.0\Bin\MSBuild.exe"
IF NOT DEFINED MSBUILD IF EXIST "%PF86%\MSBuild\14.0\Bin\MSBuild.exe" SET "MSBUILD=%PF86%\MSBuild\14.0\Bin\MSBuild.exe"

IF NOT DEFINED MSBUILD (
	ECHO.
	ECHO ERROR: could not find MSBuild.exe.
	ECHO        Looked for VS2013's copy under
	ECHO          "%PF86%\MSBuild\12.0\Bin"
	ECHO        and for a newer Visual Studio via vswhere.
	GOTO fail
)

REM ------------------------------------------------- is the v120 toolset here?
SET "V120PROPS=%PF86%\MSBuild\Microsoft.Cpp\v4.0\V120\Microsoft.Cpp.v120.props"
IF DEFINED TOOLSET GOTO toolsetok
IF EXIST "%V120PROPS%" GOTO toolsetok
ECHO.
ECHO ERROR: the v120 (Visual Studio 2013) platform toolset is not installed.
ECHO        Expected
ECHO          "%V120PROPS%"
ECHO.
ECHO        Install "Visual Studio 2013" or the "Visual C++ Build Tools 2013",
ECHO        or - in a newer Visual Studio installer - the component
ECHO        "Visual Studio 2013 (v120) toolset". See the note at the top of
ECHO        this file for why nothing newer will link.
GOTO fail
:toolsetok

REM ------------------------------------------------------------------- build
SET "MSBUILDARGS=/nologo /m /v:minimal /t:%TARGET% /p:Configuration=%CONFIG% /p:Platform=Win32"
IF DEFINED TOOLSET SET "MSBUILDARGS=%MSBUILDARGS% /p:PlatformToolset=%TOOLSET%"

ECHO.
ECHO === Building Blocks5.sln [%CONFIG%^|Win32] ===
ECHO     MSBuild: %MSBUILD%
IF DEFINED TOOLSET ECHO     Toolset: %TOOLSET% (overridden)
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
ECHO.
ECHO     Run it with Blocks5\ as the working directory - the game opens
ECHO     data.zip relative to the current directory:
ECHO.
ECHO         cd Blocks5
ECHO         ..\%CONFIG%\blocks5.exe -windowed
ECHO.
POPD
ENDLOCAL
EXIT /B 0

:usage
ECHO.
ECHO Usage: build.bat [Release^|Debug] [/nodata] [/optipng] [/stage] [/rebuild]
ECHO                  [/toolset:vNNN]
ECHO.
ECHO Builds Blocks5.sln for Win32 with the v120 (Visual Studio 2013) toolset,
ECHO then packs data.zip and the skin archives, which are not in Git.
ECHO Open this file in an editor for why the toolset cannot simply be newer.
ECHO.
POPD
ENDLOCAL
EXIT /B 2

:fail
ECHO.
POPD
ENDLOCAL
EXIT /B 1
