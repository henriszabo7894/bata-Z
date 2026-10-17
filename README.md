# Bata-Z (BodycamUE)

FPS tactique en vue camera corporelle, survie face aux zombies, de nuit. Unreal Engine 5.5 (C++).

## Lancer le projet
1. Installer **Unreal Engine 5.5** (Epic Games Launcher) et **Visual Studio 2022** (C++ jeux).
2. Installer **Git LFS** puis cloner : `git lfs install` puis `git clone <url>`.
3. Double-cliquer **Compiler_Jouer.bat** (compile le code puis lance le jeu).

## Commandes
F1 dans le jeu affiche toutes les touches. T = drone + casque FPV, F = pince, R = retour automatique.

## Contenu
- `Source/BodycamUE` : tout le code du jeu (joueur, armes, zombies, drone, ville exterieure, HUD).
- `Config` : reglages (DefaultGame.ini : armes, recul, nuit, drone, zone exterieure).
- `Scripts` : scripts Python d'import / generation lances par les .bat.
- `Content` : uniquement les assets utilises par le jeu.

Les modeles Fab / Megascans restent soumis a leur licence : depot a garder **prive**.
