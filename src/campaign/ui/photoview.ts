// The recon photo on a raid's card in the browser (photos.ts, photoprint.ts): the print taken before the raid beside
// the one taken the day after, and what the photo interpreters wrote under them. Drawn to read as an air photograph,
// not a map: one sun throws every shadow to the same side, craters are dark pits in rings of thrown earth, smoke drifts
// downwind with its shadow beside it, and the print is soft, grainy, darker at its corners and set in its film edge,
// with the target inked round in white by the interpreter's hand. Shown, never counted. The 3D game's card draws the
// same print data by the same rules (3d-handoff.md).

import { photoCaption } from '../photos';
import { PRINT_H, PRINT_W, photoPrint, type PhotoPrint } from '../photoprint';
import { Rng, hashString } from '../../core/rng';
import { turnToDate } from '../../core/ticket';
import type { AirReport, Game } from '../types';

const grey = (v: number): string => { const c = Math.round(Math.max(0, Math.min(1, v)) * 255); return `rgb(${c},${c},${c})`; };
const n1 = (v: number): number => Math.round(v * 10) / 10;
const pts = (l: [number, number][]): string => l.map(([x, y]) => `${n1(x)},${n1(y)}`).join(' ');
const rad = (deg: number): number => (deg * Math.PI) / 180;

/** A ragged ring round (x, y): never a clean circle. */
function ragged(rng: Rng, x: number, y: number, r: number, k = 10): string {
  const l: [number, number][] = [];
  const a0 = rng.next() * Math.PI * 2;
  for (let i = 0; i < k; i++) { const a = a0 + (i / k) * Math.PI * 2, d = r * rng.range(0.72, 1.18); l.push([x + Math.cos(a) * d, y + Math.sin(a) * d]); }
  return pts(l);
}

/** The corners of a box of w by h centred on (x, y), turned by deg. */
function box(x: number, y: number, w: number, h: number, deg: number): [number, number][] {
  const c = Math.cos(rad(deg)), s = Math.sin(rad(deg));
  return ([[-w / 2, -h / 2], [w / 2, -h / 2], [w / 2, h / 2], [-w / 2, h / 2]] as [number, number][]).map(([u, v]) => [x + u * c - v * s, y + u * s + v * c]);
}

/** A box's shadow: the box swept from where it stands to where its top's shadow falls. */
function sweep(l: [number, number][], dx: number, dy: number): string {
  // The hull of the box and its moved copy: for a convex box, its corners and theirs, wound round the middle.
  const all = [...l, ...l.map(([x, y]) => [x + dx, y + dy] as [number, number])];
  const mx = all.reduce((a, p) => a + p[0], 0) / all.length, my = all.reduce((a, p) => a + p[1], 0) / all.length;
  const hull: [number, number][] = [];
  for (const p of [...all].sort((a, b) => a[0] - b[0] || a[1] - b[1])) {
    while (hull.length >= 2) {
      const [o, a] = [hull[hull.length - 2], hull[hull.length - 1]];
      if ((a[0] - o[0]) * (p[1] - o[1]) - (a[1] - o[1]) * (p[0] - o[0]) <= 0) hull.pop(); else break;
    }
    hull.push(p);
  }
  const top = hull.length;
  for (const p of [...all].sort((a, b) => b[0] - a[0] || b[1] - a[1])) {
    while (hull.length > top && hull.length >= 2) {
      const [o, a] = [hull[hull.length - 2], hull[hull.length - 1]];
      if ((a[0] - o[0]) * (p[1] - o[1]) - (a[1] - o[1]) * (p[0] - o[0]) <= 0) hull.pop(); else break;
    }
    hull.push(p);
  }
  hull.pop();
  return pts(hull.length >= 3 ? hull : [[mx, my]]);
}

function print(id: string, p: PhotoPrint, after: boolean, label: string, frame: string, no: string): string {
  const W = PRINT_W, H = PRINT_H;
  const rng = new Rng(hashString(`print:${id}`));
  const sx = Math.cos(rad(p.sun)), sy = Math.sin(rad(p.sun));
  const ground: string[] = [], shade: string[] = [], spoil: string[] = [], things: string[] = [], pits: string[] = [], air: string[] = [], ink: string[] = [];
  const poly = (l: [number, number][] | string, fill: string, extra = ''): string => `<polygon points="${typeof l === 'string' ? l : pts(l)}" fill="${fill}"${extra}/>`;

  // The land: fields with their hedges, plough stripes on two in three and crop rows finer still; the river, the road.
  ground.push(`<rect width="${W}" height="${H}" fill="${grey(0.64)}"/>`);
  p.fields.forEach(([x, y, w, h, t, v], i) => {
    const rot = `transform="rotate(${t} ${n1(x + w / 2)} ${n1(y + h / 2)})"`;
    ground.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${grey(v)}" stroke="${grey(0.38)}" stroke-width="1.2" ${rot}/>`);
    if (i % 3 === 2) return;
    const step = i % 3 === 0 ? 3.2 : 2.2, shade2 = i % 3 === 0 ? -0.07 : -0.045;
    const rows: string[] = [];
    for (let k = step; k < h - 1; k += step) rows.push(`M${n1(x + 1.5)} ${n1(y + k)}H${n1(x + w - 1.5)}`);
    ground.push(`<path d="${rows.join('')}" stroke="${grey(v + shade2)}" stroke-width="${i % 3 === 0 ? 0.9 : 0.6}" ${rot}/>`);
  });
  if (p.river.length) ground.push(`<polyline points="${pts(p.river)}" fill="none" stroke="${grey(0.26)}" stroke-width="7" stroke-linejoin="round" stroke-linecap="round"/><polyline points="${pts(p.river)}" fill="none" stroke="${grey(0.34)}" stroke-width="3" stroke-linejoin="round"/>`);
  ground.push(`<polyline points="${pts(p.road)}" fill="none" stroke="${grey(0.9)}" stroke-width="3" stroke-linejoin="round"/>`);
  // The works' own ground: worn yards and roads inside the fence, a shade darker than the fields.
  const [fx, fy, fw, fh] = p.fence;
  ground.push(`<rect x="${fx}" y="${fy}" width="${fw}" height="${fh}" fill="${grey(0.52)}"/>`);
  // The tracks of a yard on their ballast, broken where a crater lies across them, the rails bent up at its edge.
  for (const [x1, y1, x2, y2] of p.tracks) {
    const len = Math.hypot(x2 - x1, y2 - y1), ux = (x2 - x1) / len, uy = (y2 - y1) / len;
    const gaps: [number, number][] = [];
    if (after) for (const [cx, cy] of p.cuts) {
      const t = (cx - x1) * ux + (cy - y1) * uy, d = Math.abs((cx - x1) * uy - (cy - y1) * ux);
      if (d < 14 && t > 0 && t < len) gaps.push([t - 11 + d * 0.3, t + 11 - d * 0.3]);
    }
    const runs: [number, number][] = [];
    let a = 0;
    for (const [g0, g1] of gaps.sort((u, v) => u[0] - v[0])) { runs.push([a, g0]); a = g1; }
    runs.push([a, len]);
    for (const [t0, t1] of runs) {
      const ax = x1 + ux * t0, ay = y1 + uy * t0, bx = x1 + ux * t1, by = y1 + uy * t1;
      ground.push(`<line x1="${n1(ax)}" y1="${n1(ay)}" x2="${n1(bx)}" y2="${n1(by)}" stroke="${grey(0.6)}" stroke-width="3.4"/><line x1="${n1(ax)}" y1="${n1(ay)}" x2="${n1(bx)}" y2="${n1(by)}" stroke="${grey(0.2)}" stroke-width="1.3"/>`);
    }
    for (const [g0, g1] of gaps) for (const [t, sgn] of [[g0, -1], [g1, 1]] as const) {
      const ex = x1 + ux * t, ey = y1 + uy * t, turn = rad(rng.range(25, 60) * (rng.chance(0.5) ? 1 : -1));
      const bx = ux * Math.cos(turn) - uy * Math.sin(turn), by = ux * Math.sin(turn) + uy * Math.cos(turn);
      things.push(`<line x1="${n1(ex)}" y1="${n1(ey)}" x2="${n1(ex - sgn * bx * 6)}" y2="${n1(ey - sgn * by * 6)}" stroke="${grey(0.88)}" stroke-width="1" stroke-linecap="round"/>`);
    }
  }

  // Shadows, all to one side: trees two to four times their height, sheds a third to a half of their span, chimneys
  // long and thin. Drawn as one layer so where two shadows cross they are no darker.
  for (const [x, y, r] of p.trees) { const l = r * 3.2; shade.push(`<line x1="${x}" y1="${y}" x2="${n1(x + sx * l)}" y2="${n1(y + sy * l)}" stroke-width="${n1(r * 1.8)}" stroke-linecap="round"/>`); }
  for (const [x, y, s] of p.houses) { const l = s * 0.55; shade.push(poly(sweep(box(x, y + s * 0.1 - s / 2 + (s * 0.8) / 2, s, s * 0.8, 0), sx * l, sy * l), 'black')); }
  for (const [x, y, w, h] of p.sheds) { const l = Math.min(14, Math.min(w, h) * 0.45 + 2); shade.push(poly(sweep(box(x + w / 2, y + h / 2, w, h, 0), sx * l, sy * l), 'black')); }
  for (const [x, y, r] of p.tanks) shade.push(`<circle cx="${n1(x + sx * r * 0.6)}" cy="${n1(y + sy * r * 0.6)}" r="${r}"/>`);
  p.chimneys.forEach(([x, y], i) => { const l = 30 + (i % 3) * 5; shade.push(`<line x1="${x}" y1="${y}" x2="${n1(x + sx * l)}" y2="${n1(y + sy * l)}" stroke-width="1.8" stroke-linecap="round"/>`); });
  for (const [x, y, l, w, d] of p.wagons) shade.push(poly(box(x + sx * 1.4, y + sy * 1.4, l, w, d), 'black'));
  if (after && p.smoke.length) for (const [x, y, r] of p.smoke) shade.push(`<circle cx="${n1(x + sx * 16)}" cy="${n1(y + sy * 16)}" r="${n1(r * 0.75)}" opacity="0.5"/>`);

  // After the raid: the thrown earth round every crater first, so close craters run together; a cut's crater is the
  // largest, with spoil thrown over the tracks beside it.
  if (after) {
    for (const [x, y, r] of p.craters) spoil.push(poly(ragged(rng, x, y, r * rng.range(1.5, 2)), grey(rng.range(0.76, 0.86))));
    for (const [x, y] of p.cuts) {
      spoil.push(poly(ragged(rng, x, y, 19, 14), grey(0.8)));
      for (let i = 0; i < 6; i++) spoil.push(poly(ragged(rng, x + rng.range(-34, 34), y + rng.range(-12, 12), rng.range(3, 6), 7), grey(rng.range(0.74, 0.84))));
    }
  }

  // The trees: dark crowns, lit on the sun's side.
  for (const [x, y, r] of p.trees) things.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${grey(0.2)}"/><circle cx="${n1(x - sx * r * 0.35)}" cy="${n1(y - sy * r * 0.35)}" r="${n1(r * 0.5)}" fill="${grey(0.3)}"/>`);
  // The village: roofs with their ridges; after the raid, those unroofed show their dark insides inside pale walls.
  p.houses.forEach(([x, y, s], i) => {
    const gone = after && p.ruined.includes(i), hy = y - s / 2;
    if (gone) { things.push(`<rect x="${n1(x - s / 2)}" y="${n1(hy)}" width="${s}" height="${n1(s * 0.8)}" fill="${grey(0.22)}" stroke="${grey(0.82)}" stroke-width="0.6"/>`); return; }
    things.push(`<rect x="${n1(x - s / 2)}" y="${n1(hy)}" width="${s}" height="${n1(s * 0.8)}" fill="${grey(0.86)}"/><rect x="${n1(x - s / 2)}" y="${n1(hy + s * 0.4)}" width="${s}" height="${n1(s * 0.4)}" fill="${grey(0.74)}"/>`);
  });
  // The works' halls: plain roofs with a ridge; a bearing works' great halls under sawtooth roofs (light glass, dark
  // slope); a burnt hall roofless: pale wall tops, a dark floor of rubble, a few girders still across it.
  p.sheds.forEach(([x, y, w, h], i) => {
    if (after && p.burnt.includes(i)) {
      things.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${grey(0.26)}"/>`);
      for (let k = 0; k < Math.round((w * h) / 14); k++) things.push(`<rect x="${n1(x + rng.next() * (w - 2))}" y="${n1(y + rng.next() * (h - 2))}" width="${n1(rng.range(0.8, 2.2))}" height="${n1(rng.range(0.8, 1.8))}" fill="${grey(rng.range(0.12, 0.48))}"/>`);
      const across = w > h, span = across ? w : h, girders: string[] = [];
      for (let k = 6; k < span - 3; k += rng.range(7, 13)) if (rng.chance(0.55)) girders.push(across ? `M${n1(x + k)} ${y}v${h}` : `M${x} ${n1(y + k)}h${w}`);
      things.push(`<path d="${girders.join('')}" stroke="${grey(0.74)}" stroke-width="0.7"/><rect x="${x}" y="${y}" width="${w}" height="${h}" fill="none" stroke="${grey(0.86)}" stroke-width="1"/>`);
      return;
    }
    things.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${grey(0.8)}" stroke="${grey(0.5)}" stroke-width="0.5"/>`);
    if (p.style === 'sawtooth' && i < 2) {
      const bands: string[] = [];
      for (let k = 2; k < w - 1; k += 4.5) bands.push(`M${n1(x + k)} ${y + 0.5}v${h - 1}`);
      things.push(`<path d="${bands.join('')}" stroke="${grey(0.6)}" stroke-width="2.2"/>`);
    } else if (p.style === 'oil' && i === 0) {
      // The cracking towers: a dark lattice block.
      things.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${grey(0.36)}"/><path d="M${x} ${y}l${w} ${h}M${x + w} ${y}l${-w} ${h}M${x} ${n1(y + h / 2)}h${w}" stroke="${grey(0.6)}" stroke-width="0.6"/>`);
    } else {
      const along = w >= h;
      things.push(`<line x1="${along ? x : n1(x + w / 2)}" y1="${along ? n1(y + h / 2) : y}" x2="${along ? n1(x + w) : n1(x + w / 2)}" y2="${along ? n1(y + h / 2) : n1(y + h)}" stroke="${grey(0.64)}" stroke-width="0.7"/>`);
    }
  });
  // An oil plant's pipe runs: from every tank across to the towers.
  if (p.style === 'oil' && p.sheds.length) {
    const [tx, ty, , th] = p.sheds[0];
    const runs = p.tanks.map(([x, y]) => `M${x} ${y}H${n1(tx - 3 - (y % 3))}V${n1(Math.max(ty + 2, Math.min(ty + th - 2, y)))}H${tx}`);
    ground.push(`<path d="${runs.join('')}" fill="none" stroke="${grey(0.76)}" stroke-width="0.8"/>`);
  }
  // Tanks: pale roofs with a rim; a tank hit after the raid shows its roof fallen in, dark, and soot streaked downwind.
  const wx = Math.cos(rad(p.wind)), wy = Math.sin(rad(p.wind));
  p.tanks.forEach(([x, y, r], i) => {
    if (after && p.hitTanks.includes(i)) {
      things.push(`<ellipse cx="${n1(x + wx * r * 1.6)}" cy="${n1(y + wy * r * 1.6)}" rx="${n1(r * 2.2)}" ry="${n1(r * 0.7)}" transform="rotate(${p.wind} ${n1(x + wx * r * 1.6)} ${n1(y + wy * r * 1.6)})" fill="${grey(0.14)}" opacity="0.55" filter="url(#${id}b)"/>`);
      // The floating roof sunk and tilted in the burnt-out shell: a dull plate lying to one side of the dark inside.
      things.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${grey(0.16)}" stroke="${grey(0.8)}" stroke-width="0.9"/><ellipse cx="${n1(x + sx * r * 0.28)}" cy="${n1(y + sy * r * 0.28)}" rx="${n1(r * 0.62)}" ry="${n1(r * 0.42)}" transform="rotate(${n1(p.sun + 90)} ${n1(x + sx * r * 0.28)} ${n1(y + sy * r * 0.28)})" fill="${grey(0.34)}"/>`);
      return;
    }
    things.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${grey(0.88)}" stroke="${grey(0.42)}" stroke-width="0.8"/><circle cx="${x}" cy="${y}" r="${n1(r * 0.35)}" fill="none" stroke="${grey(0.7)}" stroke-width="0.5"/>`);
  });
  for (const [x, y] of p.chimneys) things.push(`<circle cx="${x}" cy="${y}" r="1.9" fill="${grey(0.84)}"/><circle cx="${x}" cy="${y}" r="1" fill="${grey(0.12)}"/>`);
  for (const [x, y, l, w, d] of p.wagons) things.push(poly(box(x, y, l, w, d), grey(0.3)) + `<line x1="${n1(x - Math.cos(rad(d)) * l * 0.45)}" y1="${n1(y - Math.sin(rad(d)) * l * 0.45)}" x2="${n1(x + Math.cos(rad(d)) * l * 0.45)}" y2="${n1(y + Math.sin(rad(d)) * l * 0.45)}" stroke="${grey(0.55)}" stroke-width="0.6"/>`);

  // The pits: dark, never round, over the earth they threw.
  if (after) {
    for (const [x, y, r] of p.craters) pits.push(poly(ragged(rng, x, y, r, 8), grey(0.15)));
    for (const [x, y] of p.cuts) pits.push(poly(ragged(rng, x, y, 9, 11), grey(0.13)));
    // Smoke: a pale plume drifting downwind from its fire, widening as it goes.
    p.smoke.forEach(([x, y, r], i) => air.push(`<ellipse cx="${x}" cy="${y}" rx="${n1(r * 1.25)}" ry="${n1(r * 0.85)}" transform="rotate(${p.wind} ${x} ${y})" fill="${grey(0.82 - i * 0.008)}" opacity="${n1(0.92 - i * 0.06)}"/>`));
  }

  // The interpreter's white ink: a hand line round the target, and its letter.
  const m = 4, corners: [number, number][] = [[fx - m, fy - m], [fx + fw + m, fy - m], [fx + fw + m, fy + fh + m], [fx - m, fy + fh + m]];
  const hand: [number, number][] = [];
  const ir = new Rng(hashString(`ink:${id}`));
  corners.forEach(([ax, ay], i) => {
    const [bx, by] = corners[(i + 1) % 4], len = Math.hypot(bx - ax, by - ay), k = Math.max(2, Math.round(len / 11));
    for (let j = 0; j < k; j++) { const t = j / k, jit = ir.range(-0.9, 0.9); hand.push([ax + (bx - ax) * t + (by - ay) / len * jit, ay + (by - ay) * t - (bx - ax) / len * jit]); }
  });
  hand.push([hand[0][0] + ir.range(2, 4), hand[0][1] + ir.range(-0.6, 0.6)]);
  ink.push(`<polyline points="${pts(hand)}" fill="none" stroke="${grey(0.97)}" stroke-width="1.3" stroke-linejoin="round" stroke-linecap="round"/>`);
  const lx = fx + fw + m + 14 < W - 6 ? fx + fw + m + 4 : fx - m - 14;
  ink.push(`<text x="${n1(lx)}" y="${n1(fy + 10)}" font-family="Georgia, 'Times New Roman', serif" font-style="italic" font-weight="700" font-size="17" fill="${grey(0.97)}">${p.letter}</text>`);

  // The film: soft focus, grain, darker corners, set in its black edge with the frame's number along it.
  const E = 7, B = 19;
  return `<figure class="recon-print"><svg viewBox="${-E} ${-E} ${W + E * 2} ${H + E + B}" role="img" aria-label="${label} the raid">
    <defs>
      <clipPath id="${id}c"><rect width="${W}" height="${H}"/></clipPath>
      <filter id="${id}f" filterUnits="userSpaceOnUse" x="0" y="0" width="${W}" height="${H}" color-interpolation-filters="sRGB">
        <feGaussianBlur in="SourceGraphic" stdDeviation="0.5" result="soft"/>
        <feTurbulence type="fractalNoise" baseFrequency="0.95" numOctaves="2" seed="${after ? 11 : 4}" result="noise"/>
        <feColorMatrix in="noise" type="matrix" values="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 0 0 0 0 1" result="mono"/>
        <feComposite in="soft" in2="mono" operator="arithmetic" k1="0" k2="1" k3="0.2" k4="-0.1"/>
      </filter>
      <filter id="${id}b" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="1.6"/></filter>
      <filter id="${id}h" filterUnits="userSpaceOnUse" x="0" y="0" width="${W}" height="${H}"><feGaussianBlur stdDeviation="0.8"/></filter>
      <filter id="${id}s" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="2.2"/></filter>
      <radialGradient id="${id}v" cx="0.5" cy="0.5" r="0.72"><stop offset="0.55" stop-color="black" stop-opacity="0"/><stop offset="1" stop-color="black" stop-opacity="0.16"/></radialGradient>
    </defs>
    <rect x="${-E}" y="${-E}" width="${W + E * 2}" height="${H + E + B}" fill="${grey(0.05)}"/>
    <g filter="url(#${id}f)"><g clip-path="url(#${id}c)">
      ${ground.join('')}
      <g opacity="0.6" fill="black" stroke="black" filter="url(#${id}h)">${shade.join('')}</g>
      ${spoil.join('')}${things.join('')}${pits.join('')}
      <g filter="url(#${id}s)">${air.join('')}</g>
      ${ink.join('')}
      <rect width="${W}" height="${H}" fill="url(#${id}v)"/>
    </g></g>
    <rect x="6" y="6" width="${label.length * 9 + 10}" height="20" fill="${grey(0.06)}"/>
    <text x="11" y="21" font-family="'Courier New', monospace" font-size="14" font-weight="700" fill="${grey(0.97)}">${label}</text>
    <text x="0" y="${H + 13}" font-family="'Courier New', monospace" font-size="10" fill="${grey(0.9)}">${frame}</text>
    <text x="${W}" y="${H + 13}" text-anchor="end" font-family="'Courier New', monospace" font-size="10" fill="${grey(0.9)}">${no}</text>
  </svg></figure>`;
}

const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);

/** The recon photo block of a raid's card: before and after, and the interpreters' lines. '' with no photo. */
export function photoBlock(g: Game, r: AirReport): string {
  if (!r.photo) return '';
  const p = photoPrint(`${g.seed}:${r.id}`, r.photo, `${g.seed}:${r.place}`);
  const d = turnToDate(r.turn);
  const sortie = 1000 + ((g.seed * 7 + r.id * 13) % 9000);
  const id = `rp${r.id}`;
  const lines = photoCaption(r.photo, r.place);
  return `<h3>The reconnaissance photographs</h3>
    <div class="recon-prints" style="display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:8px;margin:6px 0 8px">
      ${print(`${id}a`, p, false, 'BEFORE', `SORTIE ${sortie} · ${esc(d.label).toUpperCase()}`, `FRAME ${(sortie % 47) + 3}`)}
      ${print(`${id}b`, p, true, 'AFTER', `SORTIE ${sortie + 1} · THE DAY AFTER`, `FRAME ${((sortie + 1) % 47) + 3}`)}
    </div>
    <div class="note recon-words">${lines.map((l) => esc(l)).join(' ')}</div>`;
}
