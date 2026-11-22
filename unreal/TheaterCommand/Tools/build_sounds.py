"""
Brings the game's sound effects into Unreal. The sounds themselves are made by Tools/make_sounds.mjs,
which synthesises each one and saves it under Saved/Sounds; this turns every .wav there into a sound
the game can play. Run both with Tools/build_sounds.sh.

The real recordings in Sounds/Real are brought in beside them, each under its own name.
"""
import os
import unreal

folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Sounds")
if not os.path.isdir(folder):
    raise RuntimeError(f"{folder} is missing: run Tools/make_sounds.mjs first")

tasks = []
for name in sorted(os.listdir(folder)):
    if not name.endswith(".wav"):
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(folder, name))
    task.set_editor_property("destination_path", "/Game/Campaign/Sounds")
    task.set_editor_property("destination_name", name[:-4])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tasks.append(task)
# The real recordings (Sounds/Real in the project folder, kept in the repository with their credits): each keeps
# its own name, under /Game/Campaign/Sounds/Real. CampaignSounds.cpp says which of the game's moments plays which.
real = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Sounds", "Real")
if os.path.isdir(real):
    for name in sorted(os.listdir(real)):
        if not name.endswith(".wav"):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(real, name))
        task.set_editor_property("destination_path", "/Game/Campaign/Sounds/Real")
        task.set_editor_property("destination_name", name[:-4])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

made = 0
for task in tasks:
    path = task.get_editor_property("destination_path") + "/" + task.get_editor_property("destination_name")
    if unreal.load_asset(path) is not None:
        made += 1
    else:
        unreal.log_warning(f"Theater Command: {path} was not imported")
unreal.log(f"Theater Command: {made} of {len(tasks)} sound effects imported")
