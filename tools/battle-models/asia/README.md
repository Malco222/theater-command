# Asia and the Middle East: battle scenery (our own designs)

Run with Blender's Python (`bpy`; Blender 5.x). Shared shape of every region's sets:
`/mnt/project-files/battle-models/asia/SET-FORMAT.md`.

    python export.py <battle-models/asia/models> [Name,...]   models: <Name>.json (near, mid, far) and <Name>.glb, index-asia.json
    python surfaces.py <battle-models/asia/surfaces>            tileable ground pictures (height in alpha)
    python sets.py <battle-models/asia>                         sets/{jungle,monsoon,dryplain,zagros,riverplain}.json
    python look.py ...                                          stills of single models
    python vignette.py <out> [jungle,monsoon,dryplain]          review scenes
    python soldier_look.py <soldier_X.glb> <out.png>            stills of a delivered rifleman (nation colours)

Builders: `houses.py` (thatch and tin, walled town), `flora.py` (palms, bamboo, jungle trees, willow, poplar, pine,
crops), `rocks.py`; `catalogue.py` lists them. `meshkit.py` is the mesh kit, `scenery_io.py` writes the game's files.
Every random choice comes from the model's seed; detail that differs by LOD draws from its own child stream, so
near, mid and far are the same tree.

The Japanese and Chinese riflemen are options in `unreal/TheaterCommand/Tools/battle_units/soldier.py`
(`NATION_KIT` JAP, CHN: puttees, bandolier, pouches, cross straps, grenade bag); export with
`python soldier.py --only export --nations JAP,CHN`.
