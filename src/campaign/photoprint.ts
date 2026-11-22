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
  /** After the raid: the craters (x, y, radius, 1 inside the fence), the sheds burnt out, smoke, and where the tracks are cut. */
  craters: [number, number, number, 0 | 1][];
  burnt: number[];
  smoke: [number, number, number][];
  cuts: [number, number][];
}

const inFence = (f: PhotoPrint['fence'], x: number, y: number): boolean => x >= f[0] && x <= f[0] + f[2] && y >= f[1] && y <= f[1] + f[3];
const r1 = (v: number): number => Math.round(v * 10) / 10;

/** A normal draw, by the sum of three. */
const gauss = (rng: Rng): number => (rng.next() + rng.next() + rng.next() - 1.5) * 2;

/** The prints of a raid's photo, from its key (the campaign's seed and the report's id) and what the raid did. */
export function photoPrint(key: string, p: RaidPhoto): PhotoPrint {
  const rng = new Rng(hashString(`photo:${key}`));
  const W = PRINT_W, H = PRINT_H, cx = W / 2, cy = H / 2;
  // The fields: long strips, laid at one turn for the whole print as the land is parcelled.
  const turn = rng.range(-25, 25);
  const fields: PhotoPrint['fields'] = [];
  for (let i = 0; i < 26; i++) {
    const w = rng.range(50, 140), h = rng.range(18, 46);
    fields.push([r1(rng.range(-60, W)), r1(rng.range(-40, H)), r1(w), r1(h), r1(turn + rng.range(-6, 6)), r1(rng.range(0.52, 0.82) * 100) / 100]);
  }
  // Trees: copses and lines along the hedges.
  const trees: PhotoPrint['trees'] = [];
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
  let fence: PhotoPrint['fence'];
  if (p.aim === 'rail') {
    fence = [r1(cx - 120), r1(cy - 26), 240, 52];
    for (let i = 0; i < 7; i++) { const y = cy - 21 + i * 7; tracks.push([0, r1(y + (i - 3) * 1.5), W, r1(y - (i - 3) * 1.5)]); }
    sheds.push([r1(cx - 90), r1(cy - 44), 70, 14], [r1(cx + 30), r1(cy + 30), 50, 12]);
    tanks.push([r1(cx + 95), r1(cy - 40), 14]);
  } else if (p.aim === 'oil') {
    fence = [r1(cx - 65), r1(cy - 48), 130, 96];
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) tanks.push([r1(cx - 40 + i * 28), r1(cy - 22 + j * 24), r1(rng.range(7, 10))]);
    sheds.push([r1(cx + 38), r1(cy - 40), 18, 40], [r1(cx - 60), r1(cy + 30), 30, 12]);
  } else {
    fence = [r1(cx - 70), r1(cy - 45), 140, 90];
    const n = 4 + rng.int(3);
    for (let i = 0; i < n; i++) {
      const row = i % 2, col = Math.floor(i / 2);
      sheds.push([r1(cx - 62 + col * 42), r1(cy - 38 + row * 40), r1(rng.range(30, 38)), r1(rng.range(22, 32))]);
    }
    tanks.push([r1(cx + 58), r1(cy - 36), 4], [r1(cx + 58), r1(cy - 24), 4]);
  }
  // The village: a cluster of houses off to one side of the target, on the road.
  const vx = rng.chance(0.5) ? rng.range(40, 90) : rng.range(W - 90, W - 40), vy = Math.min(H - 30, Math.max(30, ry + rng.range(-20, 20)));
  const houses: PhotoPrint['houses'] = [];
  for (let i = 0; i < 22; i++) {
    const x = vx + gauss(rng) * 18, y = vy + gauss(rng) * 12;
    if (!inFence(fence, x, y)) houses.push([r1(x), r1(y), r1(rng.range(4, 8))]);
  }
  // The craters: about one for every four bombers that bombed. The share the count put inside the fence fall there;
  // the rest fall round it, wider at night and under cloud.
  const n = Math.max(10, Math.min(160, Math.round(p.bombed / 4)));
  const inside = Math.round(n * p.on);
  const spread = (p.night ? 120 : 75) * (p.sky === 'cloud' ? 1.25 : 1);
  const craters: PhotoPrint['craters'] = [];
  for (let i = 0; i < inside; i++) craters.push([r1(fence[0] + rng.next() * fence[2]), r1(fence[1] + rng.next() * fence[3]), r1(rng.range(1.6, 3.2)), 1]);
  for (let i = inside; i < n; i++) {
    for (let k = 0; k < 12; k++) {
      const x = cx + gauss(rng) * spread, y = cy + gauss(rng) * spread * 0.8;
      if (x < 2 || y < 2 || x > W - 2 || y > H - 2 || inFence(fence, x, y)) continue;
      craters.push([r1(x), r1(y), r1(rng.range(1.6, 3.2)), 0]);
      break;
    }
  }
  // A factory burnt out: the hall nearest the craters' middle, and smoke over it.
  const burnt: number[] = [], smoke: PhotoPrint['smoke'] = [];
  if (p.wrecked && sheds.length) {
    burnt.push(rng.int(sheds.length));
    const s = sheds[burnt[0]];
    for (let i = 0; i < 4; i++) smoke.push([r1(s[0] + s[2] / 2 + rng.range(-10, 30)), r1(s[1] + s[3] / 2 - i * 9), r1(8 + i * 4)]);
  }
  // A railway cut: the tracks broken under a crater inside the yards.
  const cuts: PhotoPrint['cuts'] = [];
  if (p.rail) {
    const x = fence[0] + rng.range(0.25, 0.75) * fence[2];
    cuts.push([r1(x), r1(fence[1] + fence[3] / 2)]);
  }
  // The town hit: the houses with a crater near them are unroofed.
  const ruined: number[] = [];
  if ((p.city ?? 0) > 0) houses.forEach((h, i) => { if (craters.some((c) => Math.hypot(c[0] - h[0], c[1] - h[1]) < 9)) ruined.push(i); });
  for (let i = trees.length - 1; i >= 0; i--) if (inFence(fence, trees[i][0], trees[i][1])) trees.splice(i, 1);
  return { fields, trees, road, river, houses, ruined, fence, sheds, tanks, tracks, craters, burnt, smoke, cuts };
}
