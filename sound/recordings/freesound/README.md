# Freesound finds for the battle pack

Done 18 November 2026 for the job in `JOB.md`. Brian asked for it (17 Nov, 23:49Z) and said the game is personal use and will not be sold, so non-commercial licences are allowed (23:51Z).

## How it was done

- **Search:** by hand in the Claude app's browser on Brian's Mac, at his request ("can you use my browser to search?"). Freesound refuses the cloud workspace. Freesound's robots.txt asks crawlers to stay off its search pages; these were about 110 searches done in Brian's own browser session, one every few seconds, not a crawl. Licence details were read from each sound's own page (allowed by robots.txt).
- **Files:** the public high-quality previews (Ogg Vorbis, roughly 100 to 190 kbps) from Freesound's own CDN, fetched on Brian's Mac by `fetch.sh` from `urls.tsv`: a plain client with User-Agent `sound-research-client/1.0`, one file at a time, 2 s apart. No login was used and no originals were downloaded (originals need Brian's Freesound sign-in). 150 files, 0 failed, 86 MB.
- **Checked:** every row's id, author, title and licence was matched against the sound's Freesound page. `peak_dbfs` and `mean_dbfs` were measured with ffmpeg. 53 files touch full scale (peak at or above -0.1 dBFS); that alone does not mean clipping, but check those before using a loud stretch.

## What is here

- `<id>.ogg`: 150 previews, named by Freesound id.
- `found.json`: one row a sound: item, id, title, author, licence and its URL, `credit_owed`, page URL, duration, a one-line description, file name, bytes, peak and mean level, and language for voices.
- `CREDITS.md`: the 59 sounds whose licence asks for credit (Attribution, Attribution-NonCommercial, Sampling+). The other 91 are CC0 and owe nothing. **Attribution sounds owe a credit to their author; keep CREDITS.md with the game.**
- `urls.tsv`, `fetch.sh`: the list and the script used to fetch, to fetch again if needed.

## Kept per item (best first in `found.json`)

| Item | Kept | Note |
|---|---|---|
| voices_orders_english | 5 | Go go go, Move, Charge, drill orders, 36 s of battlefield lines |
| voices_orders_german_russian | 5 | German: Schnell, Los, drop the gun. Russian: Ogon, Davai, Pobeda; Soviet re-enactors |
| voices_battle_cry | 4 | Charging scream, victory cry, cheering, Chinese soldiers (Mandarin) |
| footsteps_dirt_gravel | 6 | Boots on gravel and dirt, walk and run, single steps |
| footsteps_marching | 2 | Squad marching in boots |
| body_fall | 6 | Very heavy fall on dirt, on grass, into debris |
| knee_down | 2 | Thin: short foley only |
| gear_rattle | 4 | Harness, backpack, canteen |
| rifle_bolt | 6 | Real Mauser K98, Mosin-Nagant, Lee-Enfield |
| grenade_pin_throw | 5 | Pin handling, throw, landing |
| turret_traverse | 3 | Weak: two are designed (blender, printer), none a real turret |
| brakes | 4 | Truck hydraulic and air brakes, brake squeal |
| shell_case_drop | 4 | Weak: small brass casings only, no big shell case found |
| fire_loop | 6 | Large fire, burning building, loops |
| fire_crackle | 3 | Close crackle and snaps |
| ship_engine_room | 5 | 1885 steam engine, large ship engine room, diesel start |
| ship_bow_wash | 6 | Bow waves at sea, ferry wake |
| ship_horn | 3 | Large ship fog horns |
| ship_bell | 4 | A 1936 ship's bell, HMS Chepstow's WW1 bell |
| shell_splash_water_explosion | 6 | Depth charges, sea mine, water explosion |
| bullet_whiz_crack | 4 | One real flyby, the rest designed |
| ricochet | 4 | Real ricochets recorded at ranges |
| bugle | 4 | Cavalry charge, Last Post, close bugler |
| officer_whistle | 3 | Old army whistle, pea whistle |
| upgrade_rifles | 6 | Mauser 98k at a re-enactment, Lee-Enfield, M1 Garand and its ping |
| upgrade_automatic_weapons | 6 | PPSh-41, MP 38, MG 42, Maxim, Bren |
| upgrade_battle_ambience | 2 | Distant WW2 re-enactment battle |
| upgrade_mortar_artillery | 4 | Field gun, 120 mm mortar |
| upgrade_explosions | 6 | Near blast with debris, quarry blast at 800 m, demolitions |
| upgrade_aircraft_fighters | 6 | Spitfire, Fw 190, P-47, P-51, Zero |
| upgrade_aircraft_bombers | 6 | B-17, B-24, B-25, Stuka drone |
| upgrade_air_raid_siren | 4 | Real siren tests (Devonport, Prague, Bulgaria) |
| upgrade_tank_engines | 6 | Sherman E8 start and drive, Soviet T-26, tanks on terrain (1984 tape) |

## Flags to check before use

- **Recorded on location for a documentary or film set** (the uploader's own field recording, not audio lifted from a film): Lubini 338321, 338240, 338323, 338046, 338241, 337773; skowix 871765. Ruling 144 bars audio taken from a film; these were recorded live by the uploader, so they look fine, but the sound director decides.
- **Taken from a military training video** (public domain, not a film or series): qubodup 855248, 854478, 184382.
- **Effects library of a retired film sound editor** (craigsmith, CC0; library reels, not a named film): 482791, 479580, 675923, 483313, 483323, 438837, 438713, 674886, 438633, 675591, 437862, 437771. Any craigsmith upload naming a film or series was skipped.
- **Clipped at source:** 42968 (the uploader says it clipped and was EQ'd).
- **Source not stated:** 175177, 386842, 607382.
- **Designed, not a real recording:** 607311, 418881 (turret), 123222, 134024 (whiz), 372086 (explosion), 617018 (gear).

## Refused, and why

- From a video game: 575304 (a clip from a commercial shooter), 387508 (MG 42 built from a commercial game's sounds).
- From a film or TV series: 675954 (ricochets from a TV western), 486343 (derived from a craigsmith upload, possibly that one), 95749 (mixed from an archive film), 792800 (made from a film studio's siren effect).
- Unclear origin: 449907 (siren from a Mellotron effects reel); smill.and.welson's weapon clips (cut from online videos).
- AI-generated: 866009 (battle cry), 810765 (underwater explosion).
- Present-day war recordings, left out on taste: 180165, 800710, 627175.
- No NoDerivatives sounds were met (Freesound does not offer that licence).

## Still missing

A big brass artillery case landing on steel; a real tank turret traverse; Japanese voices (no "Banzai" or Japanese orders found); a Tiger tank (one "Tiger II audio test" exists, BY-NC, source unclear, not taken); a knee-on-ground with gear.
