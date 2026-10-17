@echo off
rem Precalcule l'eclairage de la carte L_Quartier puis lance le jeu automatiquement.
rem A relancer seulement quand la carte change. Ne ferme pas cette fenetre.
echo ============================================================
echo  Precalcul de l'eclairage de la carte L_Quartier...
echo  Ne ferme pas cette fenetre. Duree : 10 a 40 minutes.
echo  Le jeu se lancera tout seul a la fin.
echo ============================================================
"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0BodycamUE.uproject" -run=ResavePackages -BuildLighting -Quality=Medium -AllowCommandletRendering -Map=/Game/Bodycam/Maps/L_Quartier -ProjectOnly -unattended -nosplash -nop4 -abslog="%~dp0Saved\Logs\build_lighting.log"
echo.
if %ERRORLEVEL%==0 (
  echo  TERMINE ! Lancement du jeu...
  call "%~dp0Jouer.bat"
) else (
  echo  ERREUR code %ERRORLEVEL% - previens Claude.
  pause
)
