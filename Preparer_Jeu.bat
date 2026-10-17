@echo off
rem Compile le code C++ puis prepare la map et les materiaux du jeu.
rem FERME UNREAL AVANT. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"

echo [1/3] Compilation du code (2 a 10 min)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" BodycamUEEditor Win64 Development -Project="%~dp0BodycamUE.uproject" -WaitMutex > "%LOGS%\claude_build.txt" 2>&1
if errorlevel 1 (
  echo.
  echo  ERREUR DE COMPILATION. Dis "erreur" a Claude, il lit le log lui-meme.
  pause
  exit /b 1
)

echo [2/3] Materiaux vision nocturne / contamination / drone...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\create_pp_materials.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_pp_full.log"

echo [3/3] Preparation de la map (navigation, releve des zones)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\prepare_map.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_map_full.log"

echo.
echo  TERMINE. Dis "fini" a Claude (ne lance pas encore le jeu).
pause
