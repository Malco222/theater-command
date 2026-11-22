// The prints of a raid's recon photo (photos.ts): where the fields, the village, the target and the craters lie on
// them. Shown, never counted. Every place on the print comes from a stream of the report's own key, never from the
// game's dice, so the same raid always shows the same photo; what the bombs did is the raid's count, as photos.ts
// keeps it. Both games draw these shapes: the browser's card (ui/photoview.ts) and the 3D game's, through the bridge.

import { Rng, hashString } from '../core/rng';
import type { RaidPhoto } from './types';

/** A print is W by H, in the units both games scale to their card. */
export const PRINT_W = 400;
export const PRINT_H = 300;

export interface PhotoPrint {
  /** Where the shadows fall (degrees, 0 to the right, 90 down the print) and where smoke drifts: one sun and one wind a print. */
  sun: number;
  wind: number;
  /** The kind of target as laid: plain halls, a bearing works under sawtooth roofs, a steelworks, an oil plant, rail yards. */
  style: 'halls' | 'sawtooth' | 'steel' | 'oil' | 'yard';
  /** The interpreters' letter for the target, inked beside the fence. */
  letter: string;
  /** Chimneys (x, y) and the wagons in a yard (x, y, length, width, turn in degrees, 1 knocked askew by the bombs). */
  chimneys: [number, number][];
  wagons: [number, number, number, number, number, 0 | 1][];
  /** After the raid: the oil tanks hit (their places in the list). */
  hitTanks: number[];
  /** Strips of field: x, y, width, height (before turning), the turn in degrees, and the grey (0 black, 1 white). */
  fields: [number, number, number, number, number, number][];
  /** Trees in the hedgerows and copses: x, y, radius. */
  trees: [number, number, number][];
  /** The road across the print, and the river where there is one. */
  road: [number, number][];
  river: [number, number][];
  /** The village's houses: x, y, size; and those the bombs unroofed (their places in the list). */
  houses: [number, number, number][];
  ruined: number[];
  /** The target's fence: x, y, width, height. */
  fence: [number, number, number, number];
  /** Its sheds and halls (x, y, width, height), oil tanks (x, y, radius) and the tracks of a yard (x1, y1, x2, y2). */
  sheds: [number, number, number, number][];
  tanks: [number, number, number][];
  tracks: [number, number, number, number][];
  /** After the raid: the craters (x, y, radius of the pit, 1 inside the fence), the sheds burnt out, the smoke's puffs from
   *  its fire downwind (x, y, radius), and where the tracks are cut. */
  craters: [number, number, number, 0 | 1][];
  burnt: number[];
  smoke: [number, number, number][];
  cuts: [number, number][];
}

const inFence = (f: PhotoPrint['fence'], x: number, y: number): boolean => x >= f[0] && x <= f[0] + f[2] && y >= f[1] && y <= f[1] + f[3];
const r1 = (v: number): number => Math.round(v * 10) / 10;

/** A normal draw, by the sum of three. */
const gauss = (rng: Rng): number => (rng.next() + rng.next() + rng.next() - 1.5) * 2;

/** The prints of a raid's photo, from its key (the campaign's seed and the report's id) and what the raid did. The
 *  land and the works on it come from `ground` (the seed and the place) when given, so every raid on one town shows the
 *  same fields, village and works, and only the craters differ; without it, from the key. */
export function photoPrint(key: string, p: RaidPhoto, ground?: string): PhotoPrint {
  const rng = new Rng(hashString(`land:${ground ?? key}`));
  const W = PRINT_W, H = PRINT_H, cx = W / 2, cy = H / 2;
  // The fields: long strips, laid at one turn for the whole print as the land is parcelled.
  const turn = rng.range(-25, 25);
  const fields: PhotoPrint['fields'] = [];
  for (let i = 0; i < 26; i++) {
    const w = rng.range(50, 140), h = rng.range(18, 46);
    fields.push([r1(rng.range(-60, W)), r1(rng.range(-40, H)), r1(w), r1(h), r1(turn + rng.range(-6, 6)), r1(rng.range(0.52, 0.82) * 100) / 100]);
  }
  // Trees: copses, and lines along the hedges of every third field.
  const trees: PhotoPrint['trees'] = [];
  fields.forEach(([fx, fy, fw, fh, ft], i) => {
    if (i % 3 !== 0) return;
    const c = Math.cos((ft * Math.PI) / 180), s = Math.sin((ft * Math.PI) / 180), mx = fx + fw / 2, my = fy + fh / 2;
    for (let d = 2; d < fw; d += rng.range(4, 7)) {
      const lx = fx + d - mx, ly = fy - my;
      trees.push([r1(mx + lx * c - ly * s), r1(my + lx * s + ly * c), r1(rng.range(1.6, 2.8))]);
    }
  });
  for (let i = 0; i < 9; i++) {
    const tx = rng.range(0, W), ty = rng.range(0, H), k = 3 + rng.int(8);
    for (let j = 0; j < k; j++) trees.push([r1(tx + gauss(rng) * 7), r1(ty + gauss(rng) * 5), r1(rng.range(1.8, 3.2))]);
  }
  // A road across, bending once; a river on one print in two.
  const ry = rng.range(0.2, 0.8) * H;
  const road: [number, number][] = [[0, r1(ry)], [r1(W * rng.range(0.35, 0.65)), r1(ry + rng.range(-40, 40))], [W, r1(ry + rng.range(-60, 60))]];
  const river: [number, number][] = [];
  if (rng.chance(0.5)) {
    let x = rng.range(0.1, 0.9) * W;
    for (let y = -10; y <= H + 10; y += 30) { river.push([r1(x), y]); x += rng.range(-22, 22); }
  }
  // The target, in the middle of the print.
  const sheds: PhotoPrint['sheds'] = [], tanks: PhotoPrint['tanks'] = [], tracks: PhotoPrint['tracks'] = [];
  const chimneys: PhotoPrint['chimneys'] = [], wagons: PhotoPrint['wagons'] = [];
  let fence: PhotoPrint['fence'];
  let style: PhotoPrint['style'];
  if (p.aim === 'rail') {
    // A yard: the running lines across the print, and a fan of sidings opening from them, with wagons standing.
    style = 'yard';
    fence = [r1(cx - 120), r1(cy - 30), 240, 60];
    for (let i = 0; i < 4; i++) { const y = cy - 12 + i * 7; tracks.push([0, r1(y), W, r1(y)]); }
    for (let i = 0; i < 6; i++) { const y = cy - 26 + i * 4.5; tracks.push([r1(cx - 110), r1(cy - 12), r1(cx + 110), r1(y)]); }
    for (let i = 0; i < 18; i++) {
      const tr = tracks[rng.int(tracks.length)], u = rng.range(0.15, 0.85);
      const x = tr[0] + (tr[2] - tr[0]) * u, y = tr[1] + (tr[3] - tr[1]) * u;
      if (x < fence[0] || x > fence[0] + fence[2]) continue;
      wagons.push([r1(x), r1(y), 9, 3, r1((Math.atan2(tr[3] - tr[1], tr[2] - tr[0]) * 180) / Math.PI), 0]);
    }
    sheds.push([r1(cx - 90), r1(cy - 46), 70, 13], [r1(cx + 30), r1(cy + 34), 50, 12]);
    tanks.push([r1(cx + 95), r1(cy - 42), 14]);
  } else if (p.aim === 'oil') {
    // An oil plant: tank farm, the cracking towers and their stacks.
    style = 'oil';
    fence = [r1(cx - 68), r1(cy - 50), 136, 100];
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) tanks.push([r1(cx - 42 + i * 27), r1(cy - 24 + j * 24), r1(rng.range(7, 10))]);
    sheds.push([r1(cx + 36), r1(cy - 42), 18, 40], [r1(cx - 62), r1(cy + 32), 30, 12]);
    for (let i = 0; i < 3; i++) chimneys.push([r1(cx + 45 + (i % 2) * 6), r1(cy + 6 + i * 9)]);
  } else {
    style = rng.pick(['halls', 'sawtooth', 'steel'] as const);
    fence = [r1(cx - 72), r1(cy - 46), 144, 92];
    if (style === 'sawtooth') {
      // A bearing works: two great halls under sawtooth roofs, offices by the gate.
      sheds.push([r1(cx - 64), r1(cy - 38), 82, 34], [r1(cx - 64), r1(cy + 4), 82, 32], [r1(cx + 26), r1(cy - 36), 36, 16], [r1(cx + 26), r1(cy + 14), 30, 20]);
      chimneys.push([r1(cx + 50), r1(cy - 10)]);
    } else if (style === 'steel') {
      // A steelworks: long sheds, blast furnaces and a row of chimneys.
      sheds.push([r1(cx - 66), r1(cy - 40), 120, 18], [r1(cx - 66), r1(cy + 20), 100, 16], [r1(cx + 40), r1(cy + 16), 24, 22]);
      for (let i = 0; i < 3; i++) tanks.push([r1(cx - 40 + i * 22), r1(cy), 6]);
      for (let i = 0; i < 5; i++) chimneys.push([r1(cx - 50 + i * 20), r1(cy - 14)]);
    } else {
      const n = 4 + rng.int(3);
      for (let i = 0; i < n; i++) {
        const row = i % 2, col = Math.floor(i / 2);
        sheds.push([r1(cx - 64 + col * 42), r1(cy - 38 + row * 40), r1(rng.range(30, 38)), r1(rng.range(22, 32))]);
      }
      chimneys.push([r1(cx + 60), r1(cy - 34)], [r1(cx + 60), r1(cy - 22)]);
    }
  }
  // The village: a cluster of houses off to one side of the target, on the road.
  const vx = rng.chance(0.5) ? rng.range(40, 90) : rng.range(W - 90, W - 40), vy = Math.min(H - 30, Math.max(30, ry + rng.range(-20, 20)));
  const houses: PhotoPrint['houses'] = [];
  for (let i = 0; i < 22; i++) {
    const x = vx + gauss(rng) * 18, y = vy + gauss(rng) * 12;
    if (!inFence(fence, x, y)) houses.push([r1(x), r1(y), r1(rng.range(4, 8))]);
  }
  const letter = 'ABCDEFGH'[rng.int(8)];
  // What the bombs did comes from the raid's own stream.
  const bomb = new Rng(hashString(`photo:${key}`));
  // One sun and one wind for the raid's prints. The shadows fall towards the bottom of the print (30 to 150 degrees),
  // as the interpreters turned their prints so the shadows fell towards them and a crater never read as a mound.
  const sun = r1(90 + bomb.range(-60, 60)), wind = r1(bomb.range(0, 360));
  // The craters: about one for every four bombers that bombed. The share the count put inside the fence fall there;
  // the rest fall round it, wider at night and under cloud.
  const n = Math.max(10, Math.min(160, Math.round(p.bombed / 4)));
  const inside = Math.round(n * p.on);
  const spread = (p.night ? 120 : 75) * (p.sky === 'cloud' ? 1.25 : 1);
  const craters: PhotoPrint['craters'] = [];
  for (let i = 0; i < inside; i++) craters.push([r1(fence[0] + bomb.next() * fence[2]), r1(fence[1] + bomb.next() * fence[3]), r1(bomb.range(1.2, 4)), 1]);
  for (let i = inside; i < n; i++) {
    for (let k = 0; k < 12; k++) {
      const x = cx + gauss(bomb) * spread, y = cy + gauss(bomb) * spread * 0.8;
      if (x < 2 || y < 2 || x > W - 2 || y > H - 2 || inFence(fence, x, y)) continue;
      craters.push([r1(x), r1(y), r1(bomb.range(1.2, 4)), 0]);
      break;
    }
  }
  // A factory burnt out: one of its halls.
  const burnt: number[] = [];
  if (p.wrecked && sheds.length) burnt.push(bomb.int(sheds.length));
  // A railway cut: the tracks broken under a crater inside the yards.
  const cuts: PhotoPrint['cuts'] = [];
  if (p.rail) {
    const x = fence[0] + bomb.range(0.25, 0.75) * fence[2];
    cuts.push([r1(x), r1(fence[1] + fence[3] / 2)]);
  }
  // Wagons near a cut are knocked askew.
  for (const w of wagons) if (cuts.some(([x, y]) => Math.hypot(w[0] - x, w[1] - y) < 26)) { w[4] = r1(w[4] + bomb.range(25, 70) * (bomb.chance(0.5) ? 1 : -1)); w[5] = 1; }
  // The oil tanks hit: those with a crater on them.
  const hitTanks: number[] = [];
  tanks.forEach((t, i) => { if (craters.some((c) => c[3] === 1 && Math.hypot(c[0] - t[0], c[1] - t[1]) < t[2] + 2)) hitTanks.push(i); });
  // Smoke from the burnt hall, or from the first oil tank hit: a plume drifting downwind, four times as long as wide.
  const smoke: PhotoPrint['smoke'] = [];
  const fire = burnt.length ? [sheds[burnt[0]][0] + sheds[burnt[0]][2] / 2, sheds[burnt[0]][1] + sheds[burnt[0]][3] / 2] : hitTanks.length ? tanks[hitTanks[0]] : null;
  if (fire) {
    const wx = Math.cos((wind * Math.PI) / 180), wy = Math.sin((wind * Math.PI) / 180);
    for (let i = 0; i < 9; i++) smoke.push([r1(fire[0] + wx * i * 10), r1(fire[1] + wy * i * 10), r1(3 + i * 1.3)]);
  }
  // The town hit: the houses with a crater near them are unroofed.
  const ruined: number[] = [];
  if ((p.city ?? 0) > 0) houses.forEach((h, i) => { if (craters.some((c) => Math.hypot(c[0] - h[0], c[1] - h[1]) < 9)) ruined.push(i); });
  for (let i = trees.length - 1; i >= 0; i--) if (inFence(fence, trees[i][0], trees[i][1])) trees.splice(i, 1);
  return { sun, wind, style, letter, chimneys, wagons, hitTanks, fields, trees, road, river, houses, ruined, fence, sheds, tanks, tracks, craters, burnt, smoke, cuts };
}
