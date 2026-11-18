# Motion capture credits

The soldier's movement clips (`clips.json`, made by `../mocap.py`) are retargeted from the
Carnegie Mellon University Graphics Lab Motion Capture Database.

> The data used in this project was obtained from mocap.cs.cmu.edu.
> The database was created with funding from NSF EIA-0196217.

CMU's terms, as quoted in the BVH conversion's README (2008): "Use this data! This data is free for use in
research and commercial projects worldwide." CMU's site has since worded it as: "You may include this data in
commercially-sold products, but you may not resell this data directly, even in altered form." So the game may
ship motion made from these takes, but the takes themselves must not be sold or handed out as a product. The
BVH files are not kept in this repository: `fetch_cmu.py` downloads them into `bvh/`, which git ignores.

The BVH files are B. Hahne's 2010 MotionBuilder-friendly conversion of the database (cgspeed.com), from the
GitHub mirror una-dinosauria/cmu-mocap. The take titles below come from his index of the database.

## Takes used

Capture frames are at 120 fps, counted from 1 after the T pose that the conversion adds as frame 0.

| Take | CMU subject | CMU title | Our clip | Capture frames used |
|---|---|---|---|---|
| 35_01 | 35 (walk, run) | walk | `walk` | 92-226 (one two-step cycle) |
| 35_18 | 35 (walk, run) | run/jog | `run`, `advance` (with a crouch added) | 72-166 |
| 77_02 | 77 (careful actions) | standing | `idle` | 455-871 |
| 21_06 | 21 (formations, subject B) | soldiers march | `march` | 106-288 |
| 136_09 | 136 (weird walks) | Walk Crouched | `creep` | 899-1045 |
| 77_16 | 77 (careful actions) | laying down, getting up, careful ready pose | `get_up`; `go_prone` (played backwards) | 150-520; 150-470 |
| 140_01 | 140 (getting up from ground) | Get Up Face Down | `fall_forward` (played backwards) | 170-560 |
| 90_18 | 90 (acrobatics) | RugPullFall | `fall_back` | 55-300 |
| 77_19 | 77 (careful actions) | limping, hurt right leg | `stumble` | 150-232 |
| 79_73 | 79 (actor everyday activities) | scared | `cower` | 450-620 |
| 77_09 | 77 (careful actions) | duck to avoid flying object | `duck` | 200-540 |
| 111_03 | 111 | Crawling | `crawl_high` | 955-1161 |

`crawl`, the low belly crawl, uses no capture: CMU has no belly crawl, so it is keyed by hand on our rig
(`belly_crawl` in `../mocap.py`).

Each clip in `clips.json` also names its take and frames in its `source` field, for example
`"CMU 35_01 frames 92-226"`.

No motion was taken from any game. Everything the clips add to the capture is our own procedural work in
`mocap.py`: the retarget, the foot planting, the loop seams, the crouch over the run, the timing, jolt and slack arms
of the forward fall, the rifle arms, the hand-keyed low crawl and the floor contact.
