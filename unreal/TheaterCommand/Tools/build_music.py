"""
Brings the campaign's music into Unreal. The music itself is written by Tools/make_music.mjs,
which composes and synthesises an original piece and saves it as Saved/Music/theme.wav; this
turns that file into a sound the game can play on a loop. Run both with Tools/build_music.sh.

The result is large and can be remade at any time, so it is not kept in the repository.
"""
import os
import unreal

wav = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Music", "theme.wav")
if not os.path.exists(wav):
    raise RuntimeError(f"{wav} is missing: run Tools/make_music.mjs first")

task = unreal.AssetImportTask()
task.set_editor_property("filename", wav)
task.set_editor_property("destination_path", "/Game/Campaign/Music")
task.set_editor_property("destination_name", "Theme")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

sound = unreal.load_asset("/Game/Campaign/Music/Theme")
if sound is None:
    raise RuntimeError("the music was not imported")
sound.set_editor_property("looping", True)
unreal.EditorAssetLibrary.save_asset("/Game/Campaign/Music/Theme", only_if_is_dirty=False)
unreal.log(f"Theater Command: music imported, {sound.get_editor_property('duration'):.1f} seconds")
