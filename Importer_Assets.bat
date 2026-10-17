@echo off
chcp 65001 >nul
rem Copie la map Derelict Corridor dans BodycamUE, extrait le pack zombie,
rem compile le projet puis importe les zombies et audite la map.
rem Ferme l'editeur Unreal avant. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "SRC=%USERPROFILE%\Documents\Unreal Projects\DerelictCorridorMegascans\Content"
set "DST=%~dp0Content"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"

echo [1/4] Copie de la map Derelict Corridor...
robocopy "%SRC%\DerelictCorridor" "%DST%\DerelictCorridor" /E /NFL /NDL /NJH /NP
robocopy "%SRC%\__ExternalActors__\DerelictCorridor" "%DST%\__ExternalActors__\DerelictCorridor" /E /NFL /NDL /NJH /NP
robocopy "%SRC%\__ExternalObjects__\DerelictCorridor" "%DST%\__ExternalObjects__\DerelictCorridor" /E /NFL /NDL /NJH /NP

echo [2/4] Extraction du pack zombie...
powershell -NoProfile -Command "Expand-Archive -LiteralPath '%USERPROFILE%\Downloads\Scary Zombie Pack.zip' -DestinationPath '%~dp0..\Assets_Import\Zombies' -Force"

echo [3/4] Compilation (2 a 10 min)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" BodycamUEEditor Win64 Development -Project="%~dp0BodycamUE.uproject" -WaitMutex > "%LOGS%\claude_build.txt" 2>&1
if errorlevel 1 (
  echo  ERREUR de compilation - previens Claude.
  pause
  exit /b 1
)

echo [4/4] Import des zombies + audit de la map (10 a 30 min la 1re fois)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_assets.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_import_full.log"

echo.
echo  TERMINE. Dis "fini" a Claude.
pause
