"""
Brings the campaign's music into Unreal. The music itself is written by Tools/make_music.mjs (the theme),
Tools/make_music_battle.mjs (the march for battles) and Tools/make_music_map_room.mjs (the quiet piece for a
country at peace), each of which composes and synthesises an original piece and saves it in Saved/Music; this
turns those files into sounds the game can play on a loop. Run them all with Tools/build_music.sh.

The results are large and can be remade at any time, so they are not kept in the repository.
"""
import os
import unreal

folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Music")
pieces = [("theme.wav", "Theme"), ("battle.wav", "Battle"), ("map_room.wav", "MapRoom")]
if not os.path.exists(os.path.join(folder, "theme.wav")):
    raise RuntimeError("theme.wav is missing: run Tools/make_music.mjs first")

for wav, name in pieces:
    path = os.path.join(folder, wav)
    if not os.path.exists(path):
        unreal.log(f"Theater Command: {wav} has not been made; passing over it")
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", "/Game/Campaign/Music")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(f"/Game/Campaign/Music/{name}")
    if sound is None:
        raise RuntimeError(f"{name} was not imported")
    sound.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_asset(f"/Game/Campaign/Music/{name}", only_if_is_dirty=False)
    unreal.log(f"Theater Command: music imported, {name}, {sound.get_editor_property('duration'):.1f} seconds")
