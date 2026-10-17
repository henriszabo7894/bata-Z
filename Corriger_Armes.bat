@echo off
rem Viseur point rouge + flash/sang/materiaux + compilation, puis lance le jeu.
rem FERME LE JEU ET UNREAL AVANT. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo [1/3] Materiaux et effets...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\make_gunflash.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_gunflash_full.log"
echo [2/3] Viseur point rouge...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\import_reddot.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_reddot_full.log"
echo [3/3] Compilation et lancement...
call "%~dp0Compiler_Jouer.bat"
