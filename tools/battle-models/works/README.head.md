# Fortifications and battlefield defences: the battle models

Made by `tools/battle-models/works/` (branch `claude/project-thread-q16wh4`), with the same library as the ships and
aircraft (`tools/battle-models/rigid.py`). Read `../shared/sea-air/README.md` first: the file format is the same
(a fine mesh and a plain one, `part` for every point, `parts`, `sockets`, `clips` in degrees and metres, y to the
RIGHT in the file). This page says only what the works add.

Our own plain designs throughout: no army's emblem, no lettering, nothing copied from any game. Personal use.

## What is here

- `<Name>.json`: the game's own model file. `<Name>.glb`, `<Name>.fbx`: the fine mesh on its skeleton, every clip an
  action, for Unreal's importers or any viewer.
- `index-fortifications.json`: every piece with its triangles, parts, sockets and what each clip does.
- `nations.json`: the armies' paints. **One mesh serves every army** (ruling 66(e)): paint the colours named `upper`
  and `upper2` with the owner's from that table; nothing else changes. `upper` is the country's `paint` in the art
  director's unit table (`/mnt/project-files/art-director/generic-units/unit-table.json`, ruling 78(f)), read when
  the files are written; a tag not listed takes `_default`. Paint that changed in the war is under `by_date`, read as
  the unit table's swaps are: **Germany's guns are dark grey until early 1943 and dark yellow from 1943-02**, the
  same two coats and the same month as its tanks. Only the machine gun has two shapes: air-cooled (`MachineGun`,
  `MGNest`, `Pillbox`) and water-cooled (`MachineGun_water`, `MGNest_water`, `Pillbox_water`); the table says which
  each army uses.
- `pictures/`: the line-up sheets. `clips/`: the firing clip and its frames' source is `works/film.py`.

## What the works add to the format

1. **`ground`** (top level of each .json): what the piece does to the ground it stands on, in the file's axes.
   `dig`: outlines (x, y lists) of holes to cut in the terrain under it, with `dig_depth` (the model carries the
   hole's own walls and floor, and an earth lip that overlaps the outline by 0.35 m or more). `worn`: circles
   `[x, y, radius, strength]` of trodden bare earth. `skirt`: more such circles under and just beyond the piece's own
   thrown earth, at half strength: draw them with a ragged, wandering edge so a bank runs into bare earth and the
   earth into grass (without them a bank reads as a mat laid on a lawn). `paths`: footpaths (lines of x, y) from its
   door or its rear, fading out into the grass. `tracks`: wheel ruts likewise, for the big works. `clear`: outlines
   where no grass or scatter should stand. A bank's foot wanders and ends 3 cm under the ground, so it never shows
   an edge; its turf is painted `turf` and its tufts `grass`, `grass.2`, `grass.3`: give those the terrain's own
   grass colours and they vanish into the field.
2. **`takes`**: `{socket: model}`: another model stands at that socket, facing the socket's way (the flak tower's
   four `HeavyFlakTwin` and eight `LightFlak`, a flak pit's gun). Each is its own model so it can be laid, fired and
   knocked out by itself. A socket named `gun` with no entry in `takes` is for the vehicle modeller's towed gun
   (`Gun75`, `25pdr`, `ZiS3`, `leFH18`, `Howitzer105` under `../land/`: origin on the ground under the axle, x toward
   the muzzle). The `GunPit` has room for all five; the `Bunker` takes `Gun75`, `25pdr` and `ZiS3` freely and the
   `leFH18` with its spades at the back wall, but not the `Howitzer105` (0.4 m too long). A `GunPit` or a `Bunker` is
   never shown empty: the pictures stand `Gun75` at the socket in its owner's paint, and so should the game.
   The heavy and the light flak gun are built to stand in a pit, a sandbag ring or the tower, never bare on open
   ground (the art director's note of review 2).
3. **Crew places.** A socket `crew.<role>` is where a man of the crew stands, kneels, sits or lies, facing the
   socket's way; it carries `stance` (`stand`, `kneel`, `sit`, `prone`) and, where he sits, `seat_h` (metres from his
   feet to the seat). `grip.<role>.l` and `grip.<role>.r` are where his hands go. The soldier modeller's men and
   their gun-serving clips fit these; until then a piece works unmanned.
4. **`idle` is the rest pose, not the bind pose.** A piece that can be broken keeps its rubble, craters or broken
   glass as parts hidden at scale 0, and `idle` holds them there: play `idle` (or apply its first key) whenever the
   piece stands unharmed. `knocked_out`, `breach`, `crush.N`, `detonate.N` and the like are one-shot clips with
   `after: "hold"`.
5. **Guns the game lays itself.** `mount` turns in traverse (plus is clockwise seen from above), `cradle` in
   elevation (plus lifts the muzzle), with `limits` in degrees. `fire` lies over whatever the game has laid (recoil,
   breech, cradle kick), with events for the flash, smoke, dust and the empty case. Handwheels (`handwheel.*`) are
   `spin` parts: turn them as the mount turns. `follow` and `sweep` clips are for a look only.
6. **Events** name an effect (`fx`), a socket and a sound cue. Cue names are from the battle sound pack's list only
   (ruling 144: no sound files until Brian has judged them); an empty cue means silence. `scorch` asks for a scorch
   mark laid on the work round the socket (soot climbing above the hole, cracks running out from it): the broken
   faces and bent rods are in the model, the mark is the game's to lay.
7. **Lengths** (wire, fence, fieldworks, ditch, dragon's teeth, minefield) run along y from -5 to +5 so they join end
   to end. Wire and fence break by bay: `breach.N` (cut or blown) and `crush.N` (a tank drives over it).
8. **No line of fieldworks or ditch ends in a cut.** A length's two ends are open sections made to join. Close every
   line with its end pieces: `FieldworksEnd` and `AntiTankDitchEnd` close a line's far end (the join is at the file's
   y = +5, where every length has its start: place it as you would the next length), `FieldworksEnd_start` and
   `AntiTankDitchEnd_start` are their mirror images and close a line's start (the join at the file's y = -5: place it
   as you would a length before the first). In an end piece the hole rounds off in an upright end wall and the banks sink under the
   ground; its `dig` outline is rounded to match. `joins.ends` in each file lists the joins in the file's axes, and
   `joins.far_end_dx` how far the far end is offset in x (the dogleg corner's 3 m). Where two pieces join, their
   `dig` outlines share an edge: cut one hole for both, with no wall of earth between them.

## The campaign's ten pieces and these

wire: `Wire`, `WireFence`. Anti-tank ditch: `AntiTankDitch`. Dragon's teeth: `DragonsTeeth`. Gun pit: `GunPit`.
Pillbox: `Pillbox`. Concrete fort: `Bunker`. Minefield strip: `Minefield`. Machine-gun post: `MGNest`. Mortar pit:
`MortarPit`. Anti-tank gun: `GunPit` with the vehicle modeller's gun at its `gun` socket. The field battle's ground
kinds `works` and `bunker` (fort level 0 to 3): `Foxhole` and `Fieldworks*` for the lower levels, `Pillbox` and
`Bunker` for the upper.

## The pieces
