# Studio-library sounds to replace (22 Nov 2026)

Brian's decision, 22 Nov 02:50Z (decision card, option C, "Replace them as others turn up"): the game's recordings
from old Hollywood studio sound-effect libraries stay until a real field recording he marks Good replaces each one.
New searches skip those libraries.

**Skip rule for every search from now on:** craigsmith's Freesound uploads from his studio libraries (the G "Gold",
R "Red" and S "Sunset Editorial" series: titles starting G, R or S and a number, e.g. "G07-18", "R29-28", "S03-19"),
and any page that says its sound comes from a film, a series, a game, or a film or studio library. Licences allowed:
CC0, public domain, CC BY, CC BY-NC (personal use). Every take goes to Brian's ear before it is wired.

## The sounds to replace

| Recording in the game | Plays | Cut from (studio library) | What a replacement needs |
|---|---|---|---|
| `b17_drone_loop` | Every B-17 formation's engines; the bomber bed | craigsmith G07-18 | A B-17 (or any four-engined radial bomber) in steady flight, recorded outside, 20 s or more without wind noise or talk |
| `air_battle_far_loop` (its bomber drone, one of ten sources) | The far air battle bed (G07-18 plays the whole 32 s as two of its three bomber streams) | craigsmith G07-18 | As above, 32 s or loopable; the other nine sources stay |
| `b17_interior_loop` | Riding a B-17 (patch 0017) | craigsmith G07-06 | Recorded inside a B-17 in flight (waist, radio room or nose), steady cruise, no talk over it |
| `stuka_drone_loop` | Ju 87 formations | craigsmith G04-07 | A Jumo 211 engine (Ju 87, He 111 or Ju 88), running or flying; inverted-V12 if no Jumo |
| `torpedo_launch` | A submarine fires | craigsmith G45-13 | A torpedo tube firing (air impulse and splash); a navy's own release is best |
| `torpedo_run_loop` | A torpedo running | craigsmith G45-20 | A small motor or propeller heard underwater (hydrophone) |
| `ship_sinking` | A ship goes down | craigsmith G45-35 | Distant underwater explosions, or a hull breaking up (hydrophone or navy footage) |
| `depth_charge_thrower` | The thrower fires | craigsmith G33-01 | A mortar or projector firing close (a real range) |
| `bomb_release` | A stick falls | craigsmith G33-08 | Bomb shackles releasing and the fall; a falling whistle only if real |
| `aircraft_break_up` | A plane breaks up | craigsmith R29-28 | A large metal structure crashing or tearing (a real wreck, demolition or crash test) |
| `body_fall_cry_01..05` (ground) | A man falls hit | craigsmith S03-19 | The ground sound thread's list (qubodup 166943/166944 there) |

## Leads found 22 Nov 03:00Z (ALL NOT CHECKED)

The cloud can't read Freesound or DVIDS pages, so each page must be read on the Mac (title, licence, what it is,
where it came from) before anything is fetched.

- **B-17, outside and inside.** Freesound IDs that came up for B-17 searches, with their titles hidden behind the
  site's sign-in: 142897, 143559 (Fight2FlyPhoto, whose P-47 flyby 143560 is already in the game, made airshow
  recordings), 161103, 161114, 84740, 84741, 40625, 83324, 69866, 249439, 321169, everythingsounds 752824, Figowitz
  76428, ShawnyBoy 166041, griffon_designs 400501. Read each; keep only field recordings of a real B-17 (or another
  four-engined radial bomber).
- **B-17 from US Air Force video (public domain if shot by the military; check each page's credit).** DVIDS:
  video/852294 (CAF brings Flying Fortress to Sioux City), video/846004 (Moody AFB pilot flies in a historic
  aircraft, B-roll), video/600861 (B-17 fly-over), video/531502 (Wings Over Whiteman B-roll). B-roll has the raw
  sound with no music; a ride along gives the interior.
- **Ju 87 / Jumo 211.** No flying Ju 87 found. The Flying Heritage museum's Ju 87 R-4 restoration is the one to
  watch for an engine run. A Jumo 211 in a flying He 111 (CASA 2.111 flies Merlins, so no) or Ju 88 is unlikely; an
  inverted V12 (DB 605 in a Bf 109 or Buchón) may be the honest stand-in.
- **Torpedo, underwater, depth charge, bomb release, metal crash:** not searched yet.

## Round nine, 22 Nov 03:35Z: first candidates on Brian's listening page

Cut from the Mac fetch (sound/incoming/ground-2026-11-22/); details in sound/round9-b17-replacements/README.md.
- `b17_interior_loop`: two candidates, Sioux City DVIDS 852294 (steady, but thin and slower engines, clipped) and
  Whiteman DVIDS 531502 (same engine speed as the old take, windy, 12.5 s).
- `b17_drone_loop` and the far bed's G07-18 part: the Memphis Belle fly-over DVIDS 600861 (three B-17s with five
  P-51s; approach looped with a gain ramp), and a B-17 flying away from DVIDS 852294.
- Still open, not searched: Stuka/Jumo 211, torpedo, underwater, depth charge thrower, bomb release, metal crash.

## Round two of leads, 22 Nov 18:00Z

Searched with a skeptic per sound: `studio-library-leads-2.md` (kept leads with licence, what, fit, concerns, and what
was set aside); fetch list `studio-library-urls-2.txt` (29 URLs, sound name then URL); raw results
`studio-library-search-2026-11-22.json`. Nothing heard yet. Strongest by licence: Australian War Memorial
"operational sounds" tapes (HMAS Oxley submarine: torpedo launch and cavitation; a mortar), US National Park Service
Glacier Bay hydrophone clips (propeller whine, outboard at 20 knots), a NOAA small-vessel hydrophone clip, US Navy
SINKEX and the USS Gerald R. Ford shock trial, US military mortar B-roll, NASA crash tests (the 1984 Boeing 720 impact,
Langley fuselage drops). The cloud can't download any of them (nps.gov and noaa.gov are refused too), so they ride
the next Mac fetch. Still nothing free and real for a Jumo 211; Bf 109 (DB 601) stand-ins only.

## Round three of leads, 24 Nov 20:25Z

Round two's 29 leads were fetched on the Mac on 22 Nov (`/mnt/project-files/sound-leads-1..4*.zip`; licences read from
the saved pages in `/mnt/project-files/sound-leads-licences.md`) and are being cut for Brian's listening page as round
12. A second search for what round two missed: `studio-library-leads-3.md` (kept leads with the skeptic's view, set
asides, gaps), fetch list `studio-library-urls-3.txt` (13 URLs a Mac can download; YouTube-only and navy.mil items left
out), raw results `studio-library-search-3-2026-11-24.json`. Still no free, real Jumo 211 or Ju 87, depth-charge
thrower, or plane breaking up in the air; new stand-ins only (a frigate's chaff launcher, chaff and mortar B-roll, an
NPS cannon, NASA crash tests onto dirt, more SINKEX videos). A new fetch needs Brian's yes first.
