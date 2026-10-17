@echo off
rem Etape 1 qualite graphique : repare les materiaux gris et les reglages de textures, puis lance le jeu.
rem FERME LE JEU ET UNREAL AVANT. Ne ferme pas cette fenetre.
set "UE=C:\Program Files\Epic Games\UE_5.5"
set "LOGS=%~dp0Saved\Logs"
if not exist "%LOGS%" mkdir "%LOGS%"
echo Reglage des textures et des materiaux (5 a 20 min la premiere fois)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\graphics_step1.py" -unattended -nosplash -nop4 -abslog="%LOGS%\claude_graphics_full.log"
echo.
echo  Lancement du jeu (les shaders peuvent prendre quelques minutes)...
call "%~dp0Jouer.bat"
