@echo off
rem Lance le jeu en plein ecran (la souris ne peut plus atteindre la barre des taches).
rem Rendu interne a 85% de la resolution de l ecran pour garder des FPS sur la carte graphique integree.
rem Alt+Entree = repasser en fenetre. La fenetre noire "-log" affiche la progression du chargement.
start "" "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BodycamUE.uproject" -game -fullscreen -log -ExecCmds="Scalability 1,sg.TextureQuality 3,r.Streaming.PoolSize 1500,r.Streaming.LimitPoolSizeToVRAM 0,r.Streaming.MipBias 0,r.MaxAnisotropy 8,r.FullScreenMode 1,r.ScreenPercentage 85"
