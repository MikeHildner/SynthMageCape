@echo off
rem Runs PlatformIO for the cape's MIDI firmware.
rem
rem     tools\pio run               build
rem     tools\pio run -t upload     build and flash the board on USB
rem     tools\pio device monitor    show what the board prints
rem
rem Why a wrapper: Norton 360's web shield re-signs secure traffic with its own
rem root certificate. Windows trusts it, but PlatformIO brings its own trust
rem list and refuses every download. If the extended list exists (the standard
rem list plus Norton's root, copied from the Windows store), this points
rem PlatformIO at it for this one command. Certificate checking stays on.
rem See firmware\README.md.
setlocal
set "BUNDLE=%USERPROFILE%\.platformio\ca-bundle-with-norton.pem"
if exist "%BUNDLE%" set "REQUESTS_CA_BUNDLE=%BUNDLE%"
python -m platformio %* -d "%~dp0..\firmware\cape-midi"
exit /b %ERRORLEVEL%
