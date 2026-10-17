# Projet BodycamUE — contexte

## Qui / quoi
- Lycéen STI2D (bon niveau en code, débutant Unreal), parle français, clavier **AZERTY**. Réponses en français, directes, sans blabla ; dire franchement quand une demande est irréaliste.
- Jeu : FPS tactique réaliste en vue **caméra corporelle** (inspiré de *Bodycam*, Reissad Studio). Claude écrit tout le code et automatise l'éditeur ; l'utilisateur teste et envoie captures/erreurs.
- **Échéance : dimanche 18/10/2026** → démo **solo** jouable + build .exe. Multijoueur reporté après.

## Technique
- **Unreal Engine 5.5.4** (launcher, `C:\Program Files\Epic Games\UE_5.5`), projet C++ `C:\Users\Alfayad\Documents\Bodycam_Game\BodycamUE`, module `BodycamUE`. VS 2022 17.14 / MSVC 14.44.
- PC : i5-14500T, 16 Go, **Intel UHD 770 (iGPU seulement)** → pas de Lumen / raytracing / VSM, TAA, éclairage **précalculé** (Lightmass CPU) + quelques lumières mobiles sans ombre. ~60-74 FPS en Moyen sur les premières cartes.
- Compiler : `Engine\Build\BatchFiles\Build.bat BodycamUEEditor Win64 Development -Project=...` (et `BodycamUE` pour le jeu). L'éditeur doit être fermé pour compiler la cible éditeur.
- Scripts Python éditeur lancés par `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<fichier>` (dossier `Scripts/`).

## Pièges connus (importants)
- **Un Unreal lancé par Claude ne peut pas démarrer ShaderCompileWorker** ("Accès refusé") → le jeu et Lightmass se figent. Tout ce qui compile des shaders doit être lancé **par l'utilisateur** via les .bat : `Jouer.bat` (jeu fenêtré, `-game`), `Construire_Eclairage.bat` (Lightmass puis lance le jeu), `Lancer_BodycamUE.bat` (éditeur). Les commandlets Python sans shaders et Build.bat marchent depuis Claude.
- Double-clic sur le `.uproject` ne marche pas chez lui (pas d'UnrealVersionSelector).
- Les matériaux créés par script doivent avoir `used_with_static_lighting` (et `used_with_instanced_static_meshes`) sinon damier gris en jeu (`_ensure_usage` dans `bodycam_materials.py`).
- Unreal lit les lettres selon la disposition clavier : AZERTY détecté auto (ZQSD + A/E pour pencher). Commande `BodycamLayout qwerty|azerty|auto`.
- En PowerShell, ne pas nommer une fonction `R` (alias d'Invoke-History).

## Fait
- `Core/BodycamInputConfig` : toutes les Input Actions créées en C++ à l'exécution (pas d'assets). Touches : ZQSD, souris, clic G/D, R, Shift, Ctrl/C, A/E, Espace, F, V, 1/2 (&/é), H soin, Tab, Échap/P, **N infrarouge**.
- `Player/BodycamCharacter` : marche/course/stamina, accroupi (bascule), penché répliqué, saut limité, inertie, projecteur IR. `Player/BodycamPlayerController` : commandes console `BodycamDebug`, `BodycamOverlay`, `BodycamFX`, `BodycamHeadBob x`, `BodycamSensitivity x`, `BodycamLayout`.
- `Camera/BodycamCameraComponent` : FOV 100, head bob, penché avec anti-mur, roulis, à-coup d'atterrissage, post-process (grain, aberration, flou 0.3, vignette, Panini, expo lente bornée), **mode infrarouge** (N&B, grain, bias).
- `UI/BodycamHUD` : overlay REC clignotant, horodatage, "BODYCAM 3 X6039F2A", batterie, "IR" ; ligne debug jaune (à couper pour la version finale).
- `World/BodycamFlickerLight` (néons/lampes qui clignotent : Steady/Flicker/Faulty/Buzz), `World/BodycamRain` (pluie instanciée autour de la caméra, coupée sous un toit).
- Carte **`/Game/Bodycam/Maps/L_Quartier`** générée par `Scripts/build_quartier.py` (+ `bodycam_materials.py` : matériau maître aligné monde + instances, émissifs, pluie) : rue de nuit sous la pluie, immeubles, ruelle, commerce abandonné, cour industrielle, entrepôt abandonné (mezzanine, toit percé), ~975 objets. Carte de test `L_Sandbox`. Canaux de collision `Weapon` (trace) et `Hitbox` (objet) déjà créés.

## En cours / à faire
1. Attendre le retour de l'utilisateur sur : textures corrigées, nuit, pluie, IR, FPS (régler exposition nuit/IR si besoin).
2. Voitures/props en cubes = pas réalistes : proposer des modèles gratuits Fab (ex. "City Sample Vehicles", à vérifier) que l'utilisateur télécharge, puis les intégrer.
3. Puis plan : armes (pistolet + fusil, hitscan, recul, dispersion, modes de tir, rechargement chargeur vide/plein) → dégâts par zone + saignement + soins → IA (vue, ouïe, couverture) → mission data-driven + UI → son (pas par surface, pluie) → destructibles → options/menu → build .exe. Multijoueur coop après dimanche.
