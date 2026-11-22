// Recon photos after a raid (Brian, 22 November 2026: "a before-and-after aerial photo card shows whether the bombs hit
// the factory or the fields next to it"). The grand campaign only, behind a switch: data/campaign/photos.json 'on',
// kept with each campaign in g.flags.photos.
//
// The day after the player's raid on works, oil or a railway, a reconnaissance aircraft photographs the target. The
// card puts the print taken before the raid beside the one taken after: the craters inside the target's fence, and
// those in the fields and the village round it. Nothing is drawn here and nothing is counted: the photo is made from
// what the raid's own count already decided (air.ts), the bombers that bombed and the share of their bombs that found
// the target, with the output it stopped, a factory wrecked and a railway cut. Where the craters fall on the print is
// the interface's business, from a stream of the report's own id, so the same raid always shows the same photo.
//
// With the switch off nothing here runs and no report carries a photo.

import data from '../../data/campaign/photos.json';
import { clamp } from '../core/math';
import type { Sky } from './weather';
import type { Game, RaidPhoto } from './types';

export const PHOTOS = data as unknown as { on: boolean; inside: number; least: number; most: number };

/** True in a campaign played with recon photos: the grand campaign, begun with the switch on. */
export const photosOn = (g: Game): boolean => g.flags.open === 1 && g.flags.photos === 1;

/** True when a new grand campaign opens with recon photos (photos.json 'on'). The interfaces read it as a campaign starts. */
export const photosShipped = (): boolean => PHOTOS.on === true;

/** Turns recon photos on or off (a new grand campaign as shipped, or the test kitchen). The bridge calls it by name. */
export function setPhotos(g: Game, on: boolean): void {
  if (on) g.flags.photos = 1;
  else delete g.flags.photos;
}

/**
 * The photo of a raid on works, oil or a railway, from what its count decided: the bombers that bombed, those whose
 * bombs found the target (`through`, on the scale of a perfect raid), what the raid aimed at, the share of the
 * province's output it stopped for a month, and whether a factory was wrecked or the railway cut. Null with nothing
 * to photograph.
 */
export function raidPhoto(
  g: Game, o: { aim: 'works' | 'oil' | 'rail'; fac: number; bombed: number; through: number; cut: number; wrecked: boolean; rail: boolean; city: number; night: boolean; sky: Sky },
): RaidPhoto | null {
  if (!photosOn(g) || o.bombed <= 0) return null;
  const on = clamp((o.through / o.bombed) * PHOTOS.inside, PHOTOS.least, PHOTOS.most);
  // A province with no factories is bombed for its railway (air.ts): the photo is of the yards.
  const aim = o.aim === 'works' && o.fac <= 0 ? 'rail' : o.aim;
  const p: RaidPhoto = { aim, bombed: Math.round(o.bombed), on: Math.round(on * 100) / 100, cut: Math.round(o.cut * 1000) / 1000 };
  if (o.wrecked) p.wrecked = 1;
  if (o.rail) p.rail = 1;
  if (o.city > 0) p.city = Math.round(o.city * 10) / 10;
  if (o.night) p.night = 1;
  if (o.sky === 'cloud') p.sky = 'cloud';
  return p;
}

const WORDS = ['', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];

/** "about one bomb in four": the share on target as the photo interpreters wrote it, in whole bombs and in words (the UI director, 22 November). */
export function bombsIn(on: number): string {
  if (on >= 0.45) return 'about half the bombs';
  const n = Math.max(2, Math.round(1 / Math.max(on, 0.01)));
  return n > 10 ? 'fewer than one bomb in ten' : `about one bomb in ${WORDS[n]}`;
}

/** What the interpreters wrote under the prints: where the bombs fell, and what that did. */
export function photoCaption(p: RaidPhoto, place: string): string[] {
  const target = p.aim === 'oil' ? 'the oil plant' : p.aim === 'rail' ? 'the rail yards' : 'the works';
  const round = p.aim === 'rail' ? 'the streets and fields round them' : 'the fields and the village round it';
  const lines = [`Photographed the day after the ${p.night ? 'night raid' : 'raid'}${p.sky === 'cloud' ? ', through breaks in the cloud' : ''}.`];
  const head = bombsIn(p.on);
  lines.push(`${head[0].toUpperCase()}${head.slice(1)} fell inside ${target} at ${place}; the rest fell in ${round}.`);
  if (p.aim !== 'rail' && p.cut > 0) lines.push(`${p.aim === 'oil' ? 'The oil plant makes' : 'The works make'} ${Math.max(1, Math.round(p.cut * 100))}% less for a month.`);
  if (p.wrecked) lines.push('One factory is wrecked: it makes nothing until engineers rebuild it.');
  if (p.rail) lines.push('The tracks are cut: no train runs until engineers mend them.');
  else if (p.aim === 'rail') lines.push('The tracks are still whole: the trains still run.');
  return lines;
}
