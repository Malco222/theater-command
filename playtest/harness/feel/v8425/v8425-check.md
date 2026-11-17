# Version 84.2.5 pressed in the browser (main 2eb7806), 2026-11-17 18:25Z to 18:50Z, in the cloud

Ruling 211(a) and (b), with Brian's change of 16:49Z ("it should be 60/40 if brittain declares war on you"). 1,600 by 1,000.

## The short answer

All of it holds. In Brian's own save the Canadian guns no longer take Pittsburgh: they pull back to Buffalo, and Pittsburgh, Washington and Norfolk are still American five months on (May 1941). Britain declares war on him in January 1941. A new America that declares war on Canada is told the odds first, and Britain's answer follows them. A player cannot attack with guns or engineers alone. Three small things are below.

## What was pressed

| Check | Result |
|---|---|
| His save (December 1940), five months with nothing done | Pittsburgh, Washington, Norfolk American at every month's end (7 of 7 checks). XI Artillery Corps at Buffalo (held by Canada), strength 97%; its log has no storm after Buffalo (November 1940). |
| Britain in his save | January 1941, week 1: "Britain stands by Canada / The United States has been at war with Canada since October 1939. Britain now declares war on the United States: it answers for its dominion." The month card leads with "WAR: Britain stands by Canada as January opens". The same answer on two separate loads. |
| The declare card | "Britain may stand by it. Canada is a dominion: six times in ten Britain declares war on you the same month, and four times in ten it stays out of this war." Picture: `pictures/v8425-declare-canada-odds.png`. Nothing on the card promises she will come. |
| Six seeds (25 to 30), war on Canada in September 1939 | Britain in on 25, 26, 27, 28 ("Britain stands by Canada"); out on 29 and 30 ("Britain stays out of Canada's war / Britain does not declare war on the United States: Canada fights this war without it."). Both answers are news the same month. |
| She stays out | Seed 29 played to month 7: Britain at war with Germany only. Seed 29 declared again from the start: the same answer. |
| Guns alone, the player (Germany 1939, staged: Elbing's corps made guns, Neumark's made engineers) | The attack card gathers every corps the roads carry, so infantry and tanks join and the attack is allowed, as it should be. With ONLY THE CORPS I PICKED the card reads 0 men, the foot says "Guns cannot take ground alone. Send infantry or tanks in with them.", and AUTO-RESOLVE is shut. Danzig Corridor and Poznan stay Polish; no battle is fought. Picture: `pictures/v8425-guns-alone-refused.png`. |

## Three small things

1. **The guns' own box on the card gives the wrong reason.** It reads "NO ROOM ON THE ROADS"; the true reason is the foot's ("Guns cannot take ground alone"). In `ui/main.ts` the box's reason falls back to "No room on the roads" for any chosen corps left out of the attack.
2. **The siege of Pittsburgh ends without a word.** The guns leave and the ring goes, but no headline, month card line or tray row says the siege is lifted or where the guns went.
3. **The declare card says "Canada has no close friends among the nations"** two lines above "Britain may stand by it". The two read as a contradiction.

## Files

Step lists in `/mnt/project-files/playtest/harness/feel/v8425/`. The 3D form of the same checks, with the two saves: `/mnt/project-files/playtest/click-through/3d-lists/v8425/`.
