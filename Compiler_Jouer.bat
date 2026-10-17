@echo off
rem Compile le code puis lance le jeu. FERME UNREAL AVANT.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Compilation...
call "%UE%\Engine\Build\BatchFiles\Build.bat" BodycamUEEditor Win64 Development -Project="%~dp0BodycamUE.uproject" -WaitMutex > "%LOGS%\claude_build.txt" 2>&1
if errorlevel 1 (
  echo  ERREUR DE COMPILATION. Dis "erreur" a Claude.
  pause
  exit /b 1
)
echo Compilation OK. Lancement du jeu...
call "%~dp0Jouer.bat"
