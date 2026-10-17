@echo off
rem Importe les 3 armes + les jumelles (Assets_Import\Armes et Assets_Import\JVN).
rem Ferme l editeur Unreal avant. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "DL=%USERPROFILE%\Downloads"
set "AI=%~dp0..\Assets_Import"
set "TMPX=%TEMP%\bodycam_extract"

echo [0/2] Copie du drone Drone_Scavenger...
robocopy "%USERPROFILE%\Documents\Unreal Projects\DerelictCorridorMegascans\Content\Drone_Scavenger" "%~dp0Content\Drone_Scavenger" /E /NFL /NDL /NJH /NP

echo [1/2] Extraction du pistolet et du fusil a pompe...
if exist "%TMPX%" rmdir /s /q "%TMPX%"
powershell -NoProfile -Command "Expand-Archive -LiteralPath '%DL%\pistol_001_fbx.zip' -DestinationPath '%TMPX%\pistol' -Force; Expand-Archive -LiteralPath '%DL%\model-12-pump-action-shotgun-free-standard.zip' -DestinationPath '%TMPX%\shot' -Force; Expand-Archive -LiteralPath '%TMPX%\shot\source\Model 12 Trench gun model.zip' -DestinationPath '%TMPX%\shot2' -Force"
if not exist "%AI%\Armes\Pistolet" mkdir "%AI%\Armes\Pistolet"
copy /y "%TMPX%\pistol\Pistol_001.fbx" "%AI%\Armes\Pistolet\Pistol.fbx"
copy /y "%TMPX%\shot2\Pump action shotgun\Model12_shotgun.fbx" "%AI%\Armes\Fusil_Pompe\Shotgun.fbx"
rmdir /s /q "%TMPX%"

echo [2/2] Import des armes et des jumelles (10 a 20 min, le pistolet est tres lourd)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_armes.py" -unattended -nosplash -nop4 -abslog="%~dp0Saved\Logs\claude_import_armes_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
