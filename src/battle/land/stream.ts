// A capital's or a city's own river through its streets (Brian, 18 November: the battle's ground matches the map; the
// design director, b526): the hand list of real cities (data/battle/city-rivers.json, checked by the historian), and how
// the river lies to an attack coming on from a given way. The field lays it (field.ts, layStream); the card never prices
// it, so it lies where no attack must cross it: up the field along the attack's line, or across it behind the defence,
// or as a shore along the city's edge.

import riverData from '../../../data/battle/city-rivers.json';
import type { Stream } from './field';

/** A city's river as the hand list has it: compass bearings in degrees, the way the water runs past the city's heart. */
export interface CityRiver {
  river: string;
  form: 'through' | 'meet' | 'shore';
  flows: number;
  great: boolean;
  /** A shore's water, as a bearing from the city's heart. */
  water?: number;
  /** Where two meet: the second, and the bearing from the meeting toward where it comes from. */
  join?: string;
  joinFrom?: number;
  /** Its road bridges at the city in the war years, and the second river's (b596, the historian's record). */
  bridges?: number;
  joinBridges?: number;
  /** A shore city that runs along its water as a strip, long and shallow (Stalingrad along the Volga; the art director, 19 Nov). */
  strip?: boolean;
  note?: string;
}

export const CITY_RIVERS = (riverData as unknown as { cities: Record<string, CityRiver> }).cities;

/** Bearing a, as seen from bearing b: -180 to 180, clockwise positive (to the right as one faces b). */
const rel = (a: number, b: number): number => ((((a - b) % 360) + 540) % 360) - 180;

/**
 * How a city's river lies to an attack heading the given compass bearing: the field's north is the way the attack goes,
 * its west the attack's left. A river within 45 degrees of the attack's line runs up the field; any other across it,
 * behind the defence. A shore lies on the side of the field its water is on as the attack faces the city; one whose water
 * lies straight ahead of the attack (within 45 degrees) lies behind the defence, along the field's back edge, never
 * between the armies (the Volga at Stalingrad's back, the design director's b595).
 */
export function streamOf(r: CityRiver, heading: number): Stream {
  const side = (b: number): 'west' | 'east' => (rel(b, heading) < 0 ? 'west' : 'east');
  const f = rel(r.flows, heading), along = Math.abs(f) <= 45 || Math.abs(f) >= 135;
  if (r.form === 'shore') {
    const water = r.water ?? r.flows + 90;
    if (Math.abs(rel(water, heading)) <= 45) return { name: r.river, lie: 'shore', flows: f > 0 ? 'east' : 'west', side: 'back', great: r.great, ...(r.strip ? { strip: true } : {}) };
    return { name: r.river, lie: 'shore', flows: Math.abs(f) < 90 ? 'north' : 'south', side: side(water), great: r.great };
  }
  return {
    name: r.river, lie: along ? 'up' : 'across', flows: along ? (Math.abs(f) < 90 ? 'north' : 'south') : f > 0 ? 'east' : 'west', great: r.great,
    // The record's count, where it is one: 6 stands for six or more (a great city's every avenue has its bridge).
    ...(r.bridges !== undefined && r.bridges < 6 ? { bridges: r.bridges } : {}),
    ...(r.form === 'meet' && r.join ? { join: { name: r.join, side: side(r.joinFrom ?? r.flows + 180), ...(r.joinBridges !== undefined && r.joinBridges < 6 ? { bridges: r.joinBridges } : {}) } } : {}),
  };
}

/** The river of the town by this name, as it lies to an attack heading the given bearing; none where the list has none. */
export function streamFor(town: string | null, heading: number): Stream | undefined {
  const r = town ? CITY_RIVERS[town] : undefined;
  return r ? streamOf(r, heading) : undefined;
}

/**
 * The compass bearing from one point of the campaign map to another. Every chart is drawn the same way (tools/build-geo-*):
 * 40 to a degree of latitude from 71.4 N down (its y runs south), and a degree of longitude 40 times the cosine of 50
 * degrees, so east and west are put back to the ground's true width at the two points' latitude: the hand list's
 * bearings are a compass's, and the field is metres.
 */
export function bearingOf(x0: number, y0: number, x1: number, y1: number): number {
  const lat = 71.4 - (y0 + y1) / 80, k = Math.cos((lat * Math.PI) / 180) / Math.cos((50 * Math.PI) / 180);
  return ((Math.atan2((x1 - x0) * k, -(y1 - y0)) * 180) / Math.PI + 360) % 360;
}
