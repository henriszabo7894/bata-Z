# Importe les sons du jeu (WAV) dans /Game/Bodycam/Audio. Lance par Importer_Sons.bat.
# Resultat : Saved/Logs/claude_sons.txt
import os, traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Sons"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_sons.txt")
DEST = "/Game/Bodycam/Audio"
LOOPS = ("Drone_Loop", "Siren_Loop")
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
lines = []

try:
    eal.make_directory(DEST)
    files = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".wav"))
    for f in files:
        name = os.path.splitext(f)[0]
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(SRC, f))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        t.set_editor_property("replace_existing", True)
        tools.import_asset_tasks([t])
        snd = unreal.load_asset(DEST + "/" + name)
        if not snd:
            lines.append("%s : ECHEC" % name)
            continue
        if name in LOOPS:
            snd.set_editor_property("looping", True)
        eal.save_loaded_asset(snd)
        lines.append("%s : ok (%.1f s%s)" % (name, snd.get_editor_property("duration"), ", boucle" if name in LOOPS else ""))
except Exception:
    lines.append("ERREUR " + traceback.format_exc())

with open(LOG_PATH, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines) + "\n")
