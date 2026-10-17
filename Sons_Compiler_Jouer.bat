@echo off
rem Importe les sons puis compile et lance le jeu. FERME LE JEU ET UNREAL AVANT.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Import des sons...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_sons.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_sons_full.log"
echo.
echo Sons importes. Compilation et lancement du jeu...
call "%~dp0Compiler_Jouer.bat"
