@echo off
rem Compile, met a jour les materiaux (zombies, effets, vision nocturne) puis lance le jeu.
rem FERME LE JEU ET UNREAL AVANT. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
set "CMD=%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

echo [1/4] Compilation...
call "%UE%\Engine\Build\BatchFiles\Build.bat" BodycamUEEditor Win64 Development -Project="%~dp0BodycamUE.uproject" -WaitMutex > "%LOGS%\claude_build.txt" 2>&1
if errorlevel 1 (
  echo  ERREUR DE COMPILATION. Dis "erreur" a Claude.
  pause
  exit /b 1
)
echo [2/4] Textures des zombies + materiaux de la map...
"%CMD%" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\fix_materials.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_fixmat_full.log"
echo [3/4] Vision nocturne / contamination / drone...
"%CMD%" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\create_pp_materials.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_pp_full.log"
echo [4/4] Effets (flash, sang, impacts)...
"%CMD%" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\create_fx_materials.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_fx_full.log"
echo.
echo  Lancement du jeu (les shaders des nouveaux materiaux peuvent prendre quelques minutes)...
call "%~dp0Jouer.bat"
