@echo off
rem Importe le decor exterieur (batiment) et releve les fenetres de la carte. FERME LE JEU ET UNREAL AVANT.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Import du decor exterieur (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_exterior.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_exterior_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
