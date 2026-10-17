@echo off
rem Importe les zombies de Assets_Import\Zombies\Persos et les anims de Assets_Import\Zombies\Anims.
rem Ferme l'editeur Unreal avant. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
echo Import des zombies (5 a 15 min)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_zombies.py" -unattended -nosplash -nop4 -abslog="%~dp0Saved\Logs\claude_import_zombies_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
