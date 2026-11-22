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
const G = globalThis as { TC_FAR?: unknown; TC_JOIN?: unknown; process?: { env?: Record<string, string | undefined> } };
const forced = G.TC_FAR ?? G.process?.env?.TC_FAR;

/** True where the grand campaign plays on the whole world: the far side's chart is read and its countries exist. */
export const FAR_STAGE: boolean = forced !== undefined ? !!forced && forced !== '0' : (stage as { far: boolean }).far === true;

// Neighbours join the fight (release 2; the design director's ruling of 22 November, 03:15Z): stage.json's 'join'.
// Off, as it ships, every battle is fought as release 1 fought it. TC_JOIN=1 or 0 turns it for one run (a war of the
// gate), and a test turns JOIN.on for its own file (src/campaign/join.ts reads it at every battle).
const forcedJoin = G.TC_JOIN ?? G.process?.env?.TC_JOIN;
/** The switch of the rule that brings corps next door into a battle (join.ts). */
export const JOIN = { on: forcedJoin !== undefined ? !!forcedJoin && forcedJoin !== '0' : (stage as { join?: boolean }).join === true };
