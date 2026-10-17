@echo off
rem Importe la caisse de munitions dans le projet. FERME LE JEU ET UNREAL AVANT.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Import de la caisse de munitions...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_ammocrate.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_ammocrate_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
