// Where the shells land (Brian, 18 November: "very dramatic and life like"; the art director's explosions sheet,
// art-director/explosions/explosions-look.md). Data for the 3D game only, as the wrecks' places are: the fight never
// reads any of it, nothing here is drawn from the battle's own luck, and no result moves by a hair
// (tests/battle-shells.test.ts plays the same fights with it and without it). The browser page draws none of it.
//
// What a block throws ('cal', a Calibre in core/types.ts) is read from what the 3D game draws it as. unit-table.json
// (ruling 78(f), the one table the 3D game draws its models from: unreal/TheaterCommand/Content/Data/Campaign/) gives
// each nation's gun and tank model and the year each comes in; the land model index (battle-models/index-land.json:
// each model's title, kind and role) and the models' own files say what each is. DRAWN copies the table's gun and tank
// rows (the test holds the two together), and MODEL_CAL gives each gun and tank model of the index its class. The year
// is the battle's own, but a tank's is its nation's design year where the campaign keeps one (armourYear in
// src/campaign/tech.ts, which the 3D bridge passes at battleStart), as the 3D game draws its tanks by it.
//
// Infantry: every army's infantry division of the war carried 81 or 82 mm mortars in its battalions, and a field block
// is one such division or one of its regiments (units.ts), its power the whole division's as the campaign counts it,
// mortars and all (the campaign keeps no mortar units of its own). So foot, mountain and lorried infantry carry
// 'mortar'. Their blows land no shells all the same: a block's blows are one rate, reaching as far as its rifles
// (LAND.reach, 200 m), its mortars' share not told apart from its rifles' and machine guns', and the field counts them
// as small arms. On such a block 'cal' only says what its own heavy weapons throw. Cavalry, engineers and fortress
// troops carry none.
//
// Where a shell lands: a model a block loses (its figures fall, sim.ts suffer()) lands one shell where a gun block or a
// tank block cost it that model, at the model's place within the struck block's ground, fixed by the block and the
// model and never drawn from the battle's random stream, as the wrecks are (sim.ts wreck()); on tanks, at the wreck of
// the tank it knocked out. Who cost it is the share rule (the design director, b507: "Guns that fire should be seen
// firing, and it changes no result"): each block keeps what every striker has done to it over the fight, and each
// model it loses goes to the striker of that tick it owes most, who is then paid a model's worth. Guns firing from far
// back strike lightly (LAND.guns.punch), but their blows add up, and in time a model falls to them; where the striker
// owed is a block of men or lorries, it was small arms, and no shell lands. Their barrage between hits is the mix
// plan's own timed shots (battle-bridge.ts). Each shell is a 'shell' event (FieldEvent in core/types.ts), sent once, in
// the view that follows it.
//
// Craters that stay: every shell on the ground leaves a crater to the end of the battle (the sheet: "no crater that
// disappears"), as a zone in the view beside the wrecks: { id, kind: 'crater', x, y, r, until, at, cal }, r half the
// sheet's crater across for its class (CRATER), ids -1, -2 and on. None on armour (the shell struck a tank) and none in
// the river (its splash column leaves nothing). A fight lands few: the six cold-check campaign fights land 2, 5, 0, 5,
// 0 and 0 shells, the nine practice grounds over five seeds 2.8 a fight, 6 at most (a quarter of them from guns), and
// twenty river crossings of the campaign's 5.8 a fight, 24 at most, all on the ground. Their craters add at most 464
// bytes, under 4 per cent, to the last view of the six fights (9,000 to 12,800 bytes), under 600 on the practice
// grounds, and in the river crossings at most 2,200 bytes (24 craters), never over 13 per cent of the view. That small,
// the view carries them whole, so a crater can never be lost with a view the 3D game did not read (the 'shell' events
// are sent once), and they stand as the wrecks do (b477). Had a fight landed hundreds, they would have stayed in the
// events alone. Craters change no rule: no cover, no slowing, nothing the fight reads. They are looks only, until the
// design director rules otherwise.
//
// Which units feel a hit (the sheet, "Things react"): the 3D game works out who stands near from where everyone stands;
// what it cannot know is how far the blast reaches, and the event carries that (REACH).

import type { Calibre } from '../core/types';
import { turnToDate } from '../../core/ticket';
import type { Kind } from './rules';

/**
 * How far a shell's blast reaches, metres from where it lands, by class: men within it drop or are knocked flat, and
 * vehicles rock on their springs (the sheet, "Things react"). Not the sheet's camera shake radius, which is where the
 * camera feels a hit. The sheet's own figure for the blast of a 105 mm shell is its shock, the ring of dust that
 * skids out along the ground to about 15 m by frame 4; the other classes are that 15 m scaled by their shake radius
 * against the 105's 40 m, so they keep the sheet's steps: three-eighths of 10, 25, 30, 40 and 60 m, to the metre.
 * 'light' is the sheet's hand grenade row. 'bomb', a fighter's bomb in an air strike: the sheet's 250 kg row.
 */
export const REACH: Record<Calibre, number> = { light: 4, mortar: 9, field: 11, howitzer: 15, heavy: 23, bomb: 45 };

/**
 * A crater's width across in metres, by class: the sheet's "Sizes" table (about 1.3 times real, so they read at the
 * battle camera). 'light' is the hand grenade row's "scuff only", taken as half a metre.
 */
export const CRATER: Record<Calibre, number> = { light: 0.5, mortar: 0.8, field: 1, howitzer: 1.5, heavy: 3, bomb: 7 };

/** The blocks whose blows are shells: guns, and tanks with their guns. Every other block's blows are small arms. */
export const SHELLS_FROM: readonly Kind[] = ['art', 'arm'];

/** The foot whose divisions carried 81 or 82 mm mortars (above): what their own heavy weapons throw. */
const MORTARS: readonly Kind[] = ['inf', 'mtn', 'mot'];

/**
 * Each gun and tank model of the land index, by its file name, and the class of what it throws. Towed guns by the bore
 * in the index's title, or by its role where the title gives none; tanks, assault guns, tank destroyers and
 * self-propelled guns (the index's kind 'tank') by the main gun each is built with, as the record has it.
 *   - The 87.6 mm gun-howitzer, towed and self-propelled, falls between the field gun's row and the howitzer's: the
 *     index calls it a field gun, and its shell's charge is near a 75's and well short of a 105's. 'field'.
 *   - The 85 and 88 mm tank guns were built to pierce armour; their high explosive is a 75's or a little more. 'field'.
 *   - The French heavy tank's 75 mm hull howitzer (its model file names that muzzle), and the plain medium tank every
 *     nation without its own draws once it has medium tanks, a 75 or near it. 'field'.
 *   - The light tanks and the early tanks with 37 to 57 mm guns, and the 57 mm anti-tank gun: a shell of a grenade's
 *     charge to a few grenades'. 'light'.
 *   - The self-propelled 105 mm howitzer. 'howitzer'. The self-propelled 15 cm howitzer. 'heavy'.
 */
export const MODEL_CAL: Record<string, Calibre> = {
  Gun75: 'field', ZiS3: 'field', '25pdr': 'field', Pak40: 'field', AntiTankGun: 'light',
  leFH18: 'howitzer', Howitzer105: 'howitzer',
  PanzerIV: 'field', Panther: 'field', StuG3: 'field', Tiger: 'field', T34: 'field', 'T34-85': 'field',
  Sherman: 'field', CharB1: 'field', MediumTank: 'field', SU76: 'field', SU85: 'field', M10: 'field', Sexton: 'field',
  LightTank: 'light', '7TP': 'light', Matilda: 'light', M13: 'light', Type97: 'light',
  M7: 'howitzer', Hummel: 'heavy',
};

/** A nation's gun model, and its tank models with the year each comes in (0: from the start). */
export interface Drawn { gun: string; tank: [string, number][] }

/**
 * Every nation without models of its own: the plain field gun, the plain light tank, and the plain medium tank once it
 * has medium tanks, which the campaign gives the year 1942 (campaign-bridge.ts, designs).
 */
const GENERIC: Drawn = { gun: 'Gun75', tank: [['LightTank', 0], ['MediumTank', 1942]] };
const BRITISH: Drawn = { gun: '25pdr', tank: [['Matilda', 0], ['Sherman', 1943]] };
const FRENCH: Drawn = { gun: 'Gun75', tank: [['CharB1', 0]] };

/** unit-table.json's gun and tank rows, nation by nation (shared models under the nation that shares them). */
export const DRAWN: Record<string, Drawn> = {
  GER: { gun: 'leFH18', tank: [['PanzerIV', 0], ['Panther', 1944]] },
  UK: BRITISH, CAN: BRITISH, NZL: BRITISH, SAF: BRITISH, IND: BRITISH,
  AUS: { gun: '25pdr', tank: [['Matilda', 0]] },
  USA: { gun: 'Howitzer105', tank: [['LightTank', 0], ['Sherman', 1942]] },
  SOV: { gun: 'ZiS3', tank: [['LightTank', 0], ['T34', 1941], ['T34-85', 1944]] },
  FRA: FRENCH, VIC: FRENCH, FFR: FRENCH,
  ITA: { gun: 'Gun75', tank: [['M13', 0]] },
  JAP: { gun: 'Gun75', tank: [['Type97', 0]] },
  POL: { gun: 'Gun75', tank: [['7TP', 0]] },
};

/** The model a block of this kind and nation is drawn with in that year, where it is a gun or tank block. */
export function modelOf(kind: Kind, nat: string, year: number): string | null {
  const d = DRAWN[nat] ?? GENERIC;
  if (kind === 'art') return d.gun;
  if (kind !== 'arm') return null;
  let m = d.tank[0][0];
  for (const [name, from] of d.tank) if (year >= from) m = name;
  return m;
}

/**
 * What a block throws, from the model it is drawn with in the battle's month (the ticket's turn); 'mortar' for the
 * infantry's own (above); none for a block whose blows are small arms alone. A model missing from MODEL_CAL takes its
 * kind's plain model's class: the plain field gun's, or the plain tank's of that year.
 */
export function calFor(kind: Kind, nat: string, turn: number, armour?: number | null): Calibre | undefined {
  if (MORTARS.includes(kind)) return 'mortar';
  // A tank by the year its nation's designs have reached where the campaign knows it (armourYear, src/campaign/tech.ts),
  // as the 3D game draws it; else by the calendar.
  const year = kind === 'arm' && armour ? armour : turnToDate(turn).year;
  const m = modelOf(kind, nat, year);
  if (!m) return undefined;
  return MODEL_CAL[m] ?? MODEL_CAL[modelOf(kind, '', year)!];
}
