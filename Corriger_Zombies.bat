@echo off
rem Repare les couleurs des zombies et leurs animations (compilation + materiaux), puis lance le jeu.
rem FERME LE JEU ET UNREAL AVANT. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"

echo [1/2] Compilation (animations des zombies)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" BodycamUEEditor Win64 Development -Project="%~dp0BodycamUE.uproject" -WaitMutex > "%LOGS%\claude_build.txt" 2>&1
if errorlevel 1 (
  echo  ERREUR DE COMPILATION. Dis "erreur" a Claude.
  pause
  exit /b 1
)

echo [2/2] Materiaux des zombies (quelques minutes)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\fix_zombie_materials.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_zombiemat_full.log"

echo.
echo  Lancement du jeu (les shaders des zombies peuvent prendre quelques minutes)...
call "%~dp0Jouer.bat"
