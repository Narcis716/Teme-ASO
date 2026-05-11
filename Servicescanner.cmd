@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo  [ATENTIE] Rulati ca Administrator pentru informatii complete.
    echo.
)

set "CSV_MODE=0"
set "ALL_MODE=0"
set "FILTER="

:parse_args
if "%~1"=="" goto :main
if /i "%~1"=="/csv"  set "CSV_MODE=1"  & shift & goto :parse_args
if /i "%~1"=="/all"  set "ALL_MODE=1"  & shift & goto :parse_args
if /i "%~1"=="/f"    set "FILTER=%~2"  & shift & shift & goto :parse_args
if /i "%~1"=="/?"    goto :help
if /i "%~1"=="/help" goto :help
shift
goto :parse_args

:help
echo.
echo  SERVICE SCANNER - CMD Edition
echo.
echo  UTILIZARE: ServiceScanner.cmd [/csv] [/all] [/f termen]
echo.
echo    /csv         Export CSV automat
echo    /all         Include si serviciile oprite
echo    /f termen    Filtreaza dupa termen
echo    /? /help     Acest ajutor
echo.
goto :eof

:main
cls
echo.
echo  SERVICE SCANNER - CMD Edition
echo  Masina    : %COMPUTERNAME%
echo  Utilizator: %USERNAME%
echo  Data/Ora  : %DATE% %TIME%
echo.

set "TMPFILE=%TEMP%\svc_%RANDOM%.tmp"
set "CSVFILE=servicii_%DATE:~6,4%%DATE:~3,2%%DATE:~0,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%.csv"
set "CSVFILE=%CSVFILE: =0%"

if "%CSV_MODE%"=="1" (
    echo Nr,NumeInternal,NumeAfisat,Stare,TipPornire,PID > "%CSVFILE%"
)

echo  -----------------------------------------------------------------------
echo  Se interogheaza lista serviciilor...
echo  -----------------------------------------------------------------------
echo.

echo  Nr   Nume Afisat                           Stare       Tip Pornire     PID
echo  ---- -------------------------------------- ----------- --------------- ----------

set /a COUNT=0
set /a RUNNING=0
set /a STOPPED=0

if "%ALL_MODE%"=="1" (
    set "WMIC_FILTER="
) else (
    set "WMIC_FILTER= WHERE State='Running'"
)

wmic service%WMIC_FILTER% get Name,Caption,State,StartMode,ProcessId /format:csv > "%TMPFILE%" 2>nul

set /a LINE_NUM=0
for /f "usebackq tokens=1-6 delims=," %%A in ("%TMPFILE%") do (
    set /a LINE_NUM+=1
    if !LINE_NUM! gtr 2 (
        set "SVC_CAPTION=%%B"
        set "SVC_NAME=%%C"
        set "SVC_PID=%%D"
        set "SVC_START=%%E"
        set "SVC_STATE=%%F"

        set "SVC_STATE=!SVC_STATE: =!"
        set "SVC_STATE=!SVC_STATE:~0,-1!"

        if defined FILTER (
            echo !SVC_NAME!!SVC_CAPTION! | findstr /i "!FILTER!" >nul 2>&1
            if !errorLevel! neq 0 goto :skip_line
        )

        set /a COUNT+=1
        if "!SVC_STATE!"=="Running" set /a RUNNING+=1
        if "!SVC_STATE!"=="Stopped" set /a STOPPED+=1

        if "!SVC_PID!"=="0" set "SVC_PID=   -"

        set "CAPTION_SHORT=!SVC_CAPTION!"
        if "!SVC_CAPTION:~36,1!" neq "" set "CAPTION_SHORT=!SVC_CAPTION:~0,34!.."

        echo  !COUNT:~0,4! !CAPTION_SHORT:~0,38! !SVC_STATE:~0,11! !SVC_START:~0,15! !SVC_PID:~0,10!

        if "%CSV_MODE%"=="1" (
            echo !COUNT!,"!SVC_NAME!","!SVC_CAPTION!","!SVC_STATE!","!SVC_START!","!SVC_PID!" >> "%CSVFILE%"
        )

        :skip_line
    )
)

echo  -----------------------------------------------------------------------
echo.
echo  Total afisate : !COUNT!
echo  Running       : !RUNNING!
echo  Stopped/Altele: !STOPPED!
echo.

if exist "%TMPFILE%" del /q "%TMPFILE%"

if "%CSV_MODE%"=="1" (
    echo  [OK] CSV exportat: %CSVFILE%
    echo.
    goto :end
)

echo  -----------------------------------------------------------------------
echo  Procese svchost (servicii grupate):
echo  -----------------------------------------------------------------------
echo.
tasklist /svc /fi "IMAGENAME eq svchost.exe" 2>nul | findstr /i "svchost"
echo.

:menu
echo  -----------------------------------------------------------------------
echo  [1] Reincarca Running
echo  [2] Toate serviciile (inclusiv oprite)
echo  [3] Detalii serviciu (sc query)
echo  [4] Export CSV
echo  [0] Iesire
echo  -----------------------------------------------------------------------
set /p CHOICE="  Alegere: "

if "%CHOICE%"=="0" goto :end
if "%CHOICE%"=="1" (
    set "ALL_MODE=0"
    goto :main
)
if "%CHOICE%"=="2" (
    set "ALL_MODE=1"
    goto :main
)
if "%CHOICE%"=="3" (
    set /p SVC_QUERY="  Nume intern serviciu: "
    echo.
    sc query "!SVC_QUERY!"
    echo.
    sc qc "!SVC_QUERY!"
    echo.
    goto :menu
)
if "%CHOICE%"=="4" (
    set "CSV_MODE=1"
    goto :main
)
echo  Optiune necunoscuta.
goto :menu

:end
echo  La revedere!
echo.
endlocal