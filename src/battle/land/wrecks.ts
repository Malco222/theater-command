// Old wrecks (release 2, looks only; Brian, 23 November 20:09Z): about how many armoured vehicles a division of each
// kind fields, for the wrecks a battle leaves on its field (campaign/game.ts wreckBump) and for what one tank model
// stands for in a battle (live.ts, the header's oldWrecks.per). Tanks, assault guns and tank destroyers, armoured cars
// and half-tracks: an armoured division about 150, the middle of the historian's division table
// (historian/division-equipment-by-year.md); a motorised one about 40, its armoured cars and the tank or assault gun
// battalion it came to have. The rest field none. Nothing in any fight reads these.
export const AFV_PER_DIV: Readonly<Record<string, number>> = { arm: 150, mot: 40 };
