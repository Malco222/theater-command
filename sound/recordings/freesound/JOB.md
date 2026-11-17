# Freesound job, for the session on Brian's Mac

Brian asked on 17 November 2026 (23:49Z): "go on this website to try and found better sound effects for us to use:
Freesound.org". Freesound turns away the cloud workspace (its server answers 403), so this runs on his Mac.
He added (23:51Z): "this game wont be sold", "its personal use", so non-commercial licences are fine.

The Mac is also building the 3D game: keep the load light, and never touch anything inside
`/Users/malco/Total War WW2`. Work only in `~/theater-command-sound`.

1. `cd ~/theater-command-sound`, `git fetch origin claude/project-thread-qjxyv5` and fast-forward to it. Check free disk
   space (`df -h ~`); stop and report if under 3 GB free.
2. Read https://freesound.org/robots.txt and follow it. Use a plain client with User-Agent `sound-research-client/1.0`,
   never pretend to be a browser, never get round a block, a robot check or a login, and pause at least 2 s between
   page requests. If Freesound refuses, stop and report exactly what it said.
3. Search Freesound for each item below. Keep sounds licensed Creative Commons 0, Attribution (3.0 or 4.0),
   Attribution-NonCommercial, or Sampling+. Skip NoDerivatives (we cut and mix every sound) and anything unclear. Skip
   anything taken from a film, TV series or video game. For each kept sound record: id, title, author (username),
   licence name and URL, page URL, duration, a one-line description. Keep up to 6 per item, best first: clean, close,
   not clipped, no music, long enough to cut from.

   **Gaps in the battle pack:** soldier footsteps on dirt or gravel (walk and run, single steps); a body falling to the
   ground; a knee on the ground; soldier gear rattle; a bolt-action rifle's bolt worked (Mauser, Lee-Enfield, Mosin);
   a grenade pin and throw; a tank turret traversing; tank or truck brakes; a big brass shell case dropping on metal; a
   large fire burning (a loop) and fire crackle; a ship's engine room; a ship's bow wash at sea; a ship's horn; a
   ship's bell; a shell splash or water explosion; bullets passing close (whiz, crack) and ricochets; soldiers
   shouting orders or battle cries (any language: note the language and the words if the page gives them); a bugle
   call; an officer's whistle.

   **Upgrades:** World War Two weapons (M1 Garand, Kar98k, Thompson, MG42, BAR, Bren, PPSh, Mosin, mortar);
   explosions close and far that are not clipped; World War Two aircraft (Spitfire, P-51 Mustang, B-17, Stuka, any
   piston warbird flyby); an air raid siren; World War Two tank engines (Sherman, Tiger, any museum tank).
4. For each kept sound, download its public high-quality preview (the hq ogg if offered, else the hq mp3) into
   `sound/recordings/freesound/<id>.<ext>`. Do not try to download originals (they need Brian's login). Cap the whole
   job at 150 files and 400 MB.
5. Write `sound/recordings/freesound/found.json` (one row per sound with the fields above, plus the item it is for and
   its file name) and a short `sound/recordings/freesound/README.md`: what was searched, what was kept, what was
   refused and why, and that Attribution sounds owe a credit to their author.
6. Commit with the message "Freesound finds for the battle pack" and push to `origin claude/project-thread-qjxyv5`
   only. If the push is rejected because the branch moved, fetch and merge, then push again. Never force-push.
7. Report back: how many kept per item, total size, and anything refused.
