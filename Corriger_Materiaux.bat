@echo off
rem Corrige les materiaux gris (zombies, map). FERME LE JEU ET UNREAL AVANT.
set "UE=C:\Program Files\Epic Games\UE_5.5"
echo Correction des materiaux (5 a 20 min)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=pythonscript -script="%~dp0Scripts\fix_materials.py" -unattended -nosplash -nop4 -abslog="%~dp0Saved\Logs\claude_fixmat_full.log"
echo.
echo  TERMINE. Dis "fini" a Claude.
pause
