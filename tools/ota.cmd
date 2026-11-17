@echo off
rem Builds the cape's MIDI firmware and sends it to the cape over Wi-Fi.
rem
rem     tools\ota                 send to 192.168.1.34 (the address the router reserves for the cape)
rem     tools\ota 192.168.1.50    send to another address
rem
rem The cape must be powered, at home, and already running a firmware with
rem Wi-Fi in it. It restarts into the new firmware by itself. If the upload
rem fails, the old firmware stays in place.
setlocal
set "ADDRESS=%~1"
if "%ADDRESS%"=="" set "ADDRESS=192.168.1.34"

call "%~dp0pio.cmd" run
if errorlevel 1 exit /b 1

echo Sending firmware to %ADDRESS% ...
curl --fail --silent --show-error --max-time 180 -F "firmware=@%~dp0..\firmware\cape-midi\.pio\build\cape\firmware.bin" http://%ADDRESS%/update
exit /b %ERRORLEVEL%
