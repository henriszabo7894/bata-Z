@echo off
rem Importe le fusil a pompe FPS (bras + Remington + animations). FERME LE JEU ET UNREAL AVANT.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Import du fusil a pompe (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_shotgun_fps.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_import_shotgun_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
