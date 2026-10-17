@echo off
rem Importe les armes FPS (M4 et Glock 17 avec bras gantes et animations) dans le projet.
rem FERME LE JEU ET UNREAL AVANT. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Import du M4 et du Glock 17 (5 a 15 min)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_fps_rigs.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_import_fps_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
