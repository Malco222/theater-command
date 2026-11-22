// How far the grand campaign's chart reaches (data/campaign/stage.json).
//
// The far side of the world (the Pacific and the globe, step 13 and the globe's stage) is built on its own branch
// while the versions before it ship. Everything of it is keyed on this one constant, read as the modules load: the
// chart files the game reads, and the rows of the countries that live out there. While it is false the game is,
// byte for byte, the game without them.
//
// A test of the far side turns it on for its own file, before anything of the game is imported:
//
//   vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
//
// (Vitest gives every test file its own copy of the modules, so one run of the suite plays the older tests on the
// Americas and the far side's on the whole chart.) The browser page can be opened on the far chart the same way, by
// setting TC_FAR before the bundle loads. This file imports nothing but the switch, so any module may read it first.

import stage from '../../data/campaign/stage.json';

// A run outside the browser can be given the switch by its environment too: TC_FAR=1 node ... (the hash checks, a seat's trial).
const G = globalThis as { TC_FAR?: unknown; process?: { env?: Record<string, string | undefined> } };
const forced = G.TC_FAR ?? G.process?.env?.TC_FAR;

/** True where the grand campaign plays on the whole world: the far side's chart is read and its countries exist. */
export const FAR_STAGE: boolean = forced !== undefined ? !!forced && forced !== '0' : (stage as { far: boolean }).far === true;

// The wolf pack and convoy battle (release 2, ruling 241): the grand campaigns opened while this is true carry the flag
// 'convoyBattle' (navy.ts convoyRules). A run outside the browser can be given it by its environment: TC_CONVOY=1 node ...
const C = globalThis as { TC_CONVOY?: unknown; process?: { env?: Record<string, string | undefined> } };
const convoyForced = C.TC_CONVOY ?? C.process?.env?.TC_CONVOY;

/** True where new grand campaigns open with the convoy battle (data/campaign/stage.json 'convoyBattle'). */
export const CONVOY_STAGE: boolean = convoyForced !== undefined ? !!convoyForced && convoyForced !== '0' : (stage as { convoyBattle?: boolean }).convoyBattle === true;

// The month's submarine scene (release 2, ruling 241 D1) is a viewing setting, kept outside the rules' save: the
// screen sets navy.ts subScene. A run outside the browser can start with it on: TC_SUBSCENE=1 node ...
const subForced = C.process?.env?.TC_SUBSCENE ?? (globalThis as { TC_SUBSCENE?: unknown }).TC_SUBSCENE;

/** True where a run outside the browser starts with the submarine scene's record on. */
export const SUBSCENE_ENV: boolean = subForced !== undefined && !!subForced && subForced !== '0';

// The fortress rows (data/campaign/stage.json 'fortRows'): forced by TC_FORTS, as the far side's switch is by TC_FAR.
const forts = (globalThis as { TC_FORTS?: unknown }).TC_FORTS ?? G.process?.env?.TC_FORTS;
/** True where a grand campaign opens with the fortress rows (the flag is set as it opens: game.ts fortRows). */
export const FORTS_STAGE: boolean = forts !== undefined ? !!forts && forts !== '0' : (stage as { fortRows?: boolean }).fortRows === true;

// The yield form (data/campaign/stage.json 'yieldRule'; the design director's 07:20Z of 22 November 2026): forced by
// TC_YIELD, as the far side's switch is by TC_FAR. Off, nothing of the form is read.
const yields = (globalThis as { TC_YIELD?: unknown }).TC_YIELD ?? G.process?.env?.TC_YIELD;
/** True where a beaten country yields by cause, and at stops 1-2 by the record's month (src/campaign/yield.ts). */
export const YIELD_STAGE: boolean = yields !== undefined ? !!yields && yields !== '0' : (stage as { yieldRule?: boolean }).yieldRule === true;

// A neutral's bargains (release 2): the grand campaigns opened while this is true carry the flag 'bargains'
// (bargains.ts). A run outside the browser can be given it by its environment: TC_BARGAINS=1 node ...
const B = globalThis as { TC_BARGAINS?: unknown; TC_LEGIONS?: unknown; process?: { env?: Record<string, string | undefined> } };
const bargainsForced = B.TC_BARGAINS ?? B.process?.env?.TC_BARGAINS;

/** True where new grand campaigns open with a neutral's bargains (data/campaign/stage.json 'bargains'). */
export const BARGAINS_STAGE: boolean = bargainsForced !== undefined ? !!bargainsForced && bargainsForced !== '0' : (stage as { bargains?: boolean }).bargains === true;

// Volunteer legions (release 2): the grand campaigns opened while this is true carry the flag 'legions' (legions.ts).
// A run outside the browser can be given it by its environment: TC_LEGIONS=1 node ...
const legionsForced = B.TC_LEGIONS ?? B.process?.env?.TC_LEGIONS;

/** True where new grand campaigns open with volunteer legions (data/campaign/stage.json 'legions'). */
export const LEGIONS_STAGE: boolean = legionsForced !== undefined ? !!legionsForced && legionsForced !== '0' : (stage as { legions?: boolean }).legions === true;
