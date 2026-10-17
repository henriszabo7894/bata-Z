# Prepare le projet pour GitHub : trouve les assets reellement utilises par le jeu (carte + code),
# ecrit .gitignore (fichiers inutiles exclus), .gitattributes (Git LFS) et README.md.
# Lance par Publier_GitHub.bat. Resultat : Saved/Logs/claude_github.txt
import os, re, traceback
import unreal

PROJECT_DIR = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
CONTENT = os.path.join(PROJECT_DIR, "Content")
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_github.txt")
lines = []

# Dossiers gardés en entier (le code charge des assets par nom construit a l'execution).
KEEP_FOLDERS = ["/Game/Bodycam", "/Game/Drone_Scavenger", "/Game/DerelictCorridor",
                "/Game/__ExternalActors__", "/Game/__ExternalObjects__"]

GITIGNORE_BASE = """# --- Genere par Publier_GitHub.bat (prepare_github.py) ---
# Fichiers regeneres par Unreal / Visual Studio : jamais dans Git
/Binaries/
/Intermediate/
/Saved/
/DerivedDataCache/
/Build/
/Backup/
/.vs/
/.idea/
*.sln
*.suo
*.opensdf
*.sdf
*.VC.db
*.VC.opendb
UpgradeLog*.htm
Plugins/*/Binaries/
Plugins/*/Intermediate/
"""

GITATTRIBUTES = """# Gros fichiers binaires -> Git LFS
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.obj filter=lfs diff=lfs merge=lfs -text
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.tga filter=lfs diff=lfs merge=lfs -text
*.exr filter=lfs diff=lfs merge=lfs -text
*.hdr filter=lfs diff=lfs merge=lfs -text
*.wav filter=lfs diff=lfs merge=lfs -text
*.mp3 filter=lfs diff=lfs merge=lfs -text
*.ogg filter=lfs diff=lfs merge=lfs -text
*.ttf filter=lfs diff=lfs merge=lfs -text
*.otf filter=lfs diff=lfs merge=lfs -text
*.mp4 filter=lfs diff=lfs merge=lfs -text
# Texte
*.bat text eol=crlf
*.cpp text
*.h text
*.cs text
*.ini text
*.py text
*.md text
"""

README = """# Bata-Z (BodycamUE)

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
"""


def size_of(pkg):
    rel = pkg[len("/Game/"):]
    for ext in (".uasset", ".umap"):
        p = os.path.join(CONTENT, rel.replace("/", os.sep) + ext)
        if os.path.exists(p):
            return p, os.path.getsize(p)
    return None, 0


try:
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.search_all_assets(True)
    all_pkgs = set()
    for a in ar.get_assets_by_path("/Game", recursive=True):
        all_pkgs.add(str(a.package_name))
    lines.append("Paquets dans /Game : %d" % len(all_pkgs))

    # Racines : dossiers gardés + chemins /Game/... ecrits dans le code et la config.
    roots = set(p for p in all_pkgs if any(p == f or p.startswith(f + "/") for f in KEEP_FOLDERS))
    rx = re.compile(r"/Game/[A-Za-z0-9_/\-]+")
    for folder in ("Source", "Config"):
        for dp, _, files in os.walk(os.path.join(PROJECT_DIR, folder)):
            for f in files:
                if not f.endswith((".cpp", ".h", ".ini", ".cs")):
                    continue
                try:
                    txt = open(os.path.join(dp, f), encoding="utf-8", errors="ignore").read()
                except Exception:
                    continue
                for m in rx.findall(txt):
                    m = m.rstrip("/")
                    if m in all_pkgs:
                        roots.add(m)
                    else:  # dossier cité : tout son contenu
                        roots.update(p for p in all_pkgs if p.startswith(m + "/"))
    lines.append("Racines : %d" % len(roots))

    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                 include_searchable_names=False, include_soft_management_references=True,
                                                 include_hard_management_references=True)
    used = set()
    stack = list(roots)
    while stack:
        p = stack.pop()
        if p in used:
            continue
        used.add(p)
        deps = ar.get_dependencies(p, opts) or []
        for d in deps:
            d = str(d)
            if d.startswith("/Game/") and d not in used:
                stack.append(d)

    unused = sorted(all_pkgs - used)
    used_size = sum(size_of(p)[1] for p in used)
    unused_size = sum(size_of(p)[1] for p in unused)
    lines.append("Utilises : %d paquets, %.2f Go" % (len(used), used_size / 1e9))
    lines.append("Inutiles (exclus de GitHub) : %d paquets, %.2f Go" % (len(unused), unused_size / 1e9))

    # .gitignore : dossiers entierement inutiles regroupes, sinon fichier par fichier.
    unused_set = set(unused)
    folder_pkgs = {}
    for p in all_pkgs:
        parts = p.split("/")
        for i in range(3, len(parts)):
            folder_pkgs.setdefault("/".join(parts[:i]), []).append(p)
    ignored_folders = []
    for f in sorted(folder_pkgs, key=lambda x: x.count("/")):
        if any(f.startswith(g + "/") for g in ignored_folders):
            continue
        if all(p in unused_set for p in folder_pkgs[f]):
            ignored_folders.append(f)
    entries = ["/Content/" + f[len("/Game/"):] + "/" for f in ignored_folders]
    for p in unused:
        if any(p.startswith(g + "/") for g in ignored_folders):
            continue
        path, _ = size_of(p)
        if path:
            entries.append("/" + os.path.relpath(path, PROJECT_DIR).replace(os.sep, "/"))
    with open(os.path.join(PROJECT_DIR, ".gitignore"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(GITIGNORE_BASE + "\n# Assets non utilises par le jeu (restent sur le PC, pas sur GitHub)\n" + "\n".join(entries) + "\n")
    with open(os.path.join(PROJECT_DIR, ".gitattributes"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(GITATTRIBUTES)
    readme = os.path.join(PROJECT_DIR, "README.md")
    if not os.path.exists(readme):
        with open(readme, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(README)
    lines.append("Dossiers exclus : " + ", ".join(ignored_folders[:40]))
    # Plus gros fichiers envoyés (pour info)
    big = sorted(((size_of(p)[1], p) for p in used), reverse=True)[:10]
    for s, p in big:
        lines.append("  gros : %4d Mo  %s" % (s / 1e6, p))
    lines.append("OK")
except Exception:
    lines.append("ERREUR " + traceback.format_exc())

with open(LOG_PATH, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines) + "\n")
