# The set-piece stages: Pearl Harbor (Oahu) and Midway (ruling 238)

The ground the two set-piece air battles are fought over, built from open map data. Python 3 with shapely 2.2,
numpy, pillow and pyarrow. Everything is written under `STAGE_WORK` (default `/tmp/stage-work`).

1. `fetch.py base land <lon0> <lat0> <lon1> <lat1> $STAGE_WORK/oahu-land.json` (and `midway-land.json`, and
   `base infrastructure` round Pearl Harbor for `pearl-infra.json`): the coast, reefs, piers and tanks from
   Overture Maps (anonymous S3, release 2026-11-18.0).
2. `build_stage.py`: the outlines and the height grid (Terrarium tiles at zoom 12, fetched by `dem.py`) in metres
   east and north of each stage's origin: `out/oahu.json`, `out/midway.json`.
3. `berths.py`: Pearl Harbor's berths of 7 December 1941 and the moorings for the rest (copied by hand into
   `data/battle/air-stages.json`, where the historian's names and notes live).
4. `mesh.py`: the 3D game's stage files, the island as ready-made triangles with a colour per point, the shallows,
   reef and lagoon, the piers and runways, and lists of tanks, hangars and palms: `out/oahu-3d.json`,
   `out/midway-3d.json`, for `unreal/TheaterCommand/Content/Data/Stages/`. The palette is at the top of `mesh.py`
   for the art director.

Sources: Overture Maps Foundation / OpenStreetMap contributors (ODbL); Terrarium elevation tiles (Mapzen, AWS Open
Data; SRTM and others). The shapes are today's: the historian marks what differed in 1941 and 1942 (Midway's Sand
Island has grown since; Eastern Island's three runways are placed by hand).
