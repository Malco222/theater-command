// The recon photo on a raid's card in the browser (photos.ts, photoprint.ts): the print taken before the raid beside
// the one taken the day after, and what the photo interpreters wrote under them. Grey prints with a film's grain, the
// target's fence picked out, craters as dark pits with a pale rim. Shown, never counted.

import { photoCaption } from '../photos';
import { PRINT_H, PRINT_W, photoPrint, type PhotoPrint } from '../photoprint';
import { turnToDate } from '../../core/ticket';
import type { AirReport, Game } from '../types';

const grey = (v: number): string => { const c = Math.round(Math.max(0, Math.min(1, v)) * 255); return `rgb(${c},${c},${c})`; };
const pts = (l: [number, number][]): string => l.map(([x, y]) => `${x},${y}`).join(' ');

function print(id: string, p: PhotoPrint, after: boolean, label: string, frame: string): string {
  const W = PRINT_W, H = PRINT_H;
  const out: string[] = [];
  out.push(`<rect width="${W}" height="${H}" fill="${grey(0.66)}"/>`);
  // Fields with their hedges, and the furrows of every other one.
  p.fields.forEach(([x, y, w, h, t, v], i) => {
    const rot = `rotate(${t} ${x + w / 2} ${y + h / 2})`;
    out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${grey(v)}" stroke="${grey(0.36)}" stroke-width="1.1" transform="${rot}"/>`);
    if (i % 2 === 0) for (let k = 4; k < h; k += 4) out.push(`<line x1="${x + 1}" y1="${y + k}" x2="${x + w - 1}" y2="${y + k}" stroke="${grey(v - 0.06)}" stroke-width="0.7" transform="${rot}"/>`);
  });
  for (const [x, y, r] of p.trees) out.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${grey(0.24)}"/>`);
  if (p.river.length) out.push(`<polyline points="${pts(p.river)}" fill="none" stroke="${grey(0.3)}" stroke-width="7" stroke-linejoin="round"/>`);
  out.push(`<polyline points="${pts(p.road)}" fill="none" stroke="${grey(0.88)}" stroke-width="3"/>`);
  // The target: its ground darker for the yards and roads inside the fence.
  const [fx, fy, fw, fh] = p.fence;
  out.push(`<rect x="${fx}" y="${fy}" width="${fw}" height="${fh}" fill="${grey(0.5)}" stroke="${grey(0.85)}" stroke-width="1" stroke-dasharray="3 2"/>`);
  for (const [x1, y1, x2, y2] of p.tracks) out.push(`<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${grey(0.22)}" stroke-width="1.6"/>`);
  if (after) for (const [x, y] of p.cuts) out.push(`<rect x="${x - 6}" y="${y - 24}" width="12" height="48" fill="${grey(0.55)}"/><circle cx="${x}" cy="${y}" r="9" fill="${grey(0.12)}" stroke="${grey(0.85)}" stroke-width="1.2"/>`);
  p.sheds.forEach(([x, y, w, h], i) => {
    const burnt = after && p.burnt.includes(i);
    out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${grey(burnt ? 0.1 : 0.8)}" stroke="${grey(burnt ? 0.05 : 0.35)}" stroke-width="0.8"/>`);
    if (!burnt) out.push(`<line x1="${x}" y1="${y + h / 2}" x2="${x + w}" y2="${y + h / 2}" stroke="${grey(0.62)}" stroke-width="0.6"/>`);
  });
  for (const [x, y, r] of p.tanks) out.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="${grey(0.86)}" stroke="${grey(0.3)}" stroke-width="0.8"/><circle cx="${x + r * 0.35}" cy="${y + r * 0.35}" r="${r}" fill="none" stroke="${grey(0.25)}" stroke-width="0.6" opacity="0.5"/>`);
  p.houses.forEach(([x, y, s], i) => {
    const gone = after && p.ruined.includes(i);
    out.push(`<rect x="${x - s / 2}" y="${y - s / 2}" width="${s}" height="${s * 0.8}" fill="${grey(gone ? 0.18 : 0.84)}" stroke="${grey(0.35)}" stroke-width="0.4"/>`);
  });
  if (after) {
    for (const [x, y, r] of p.craters) out.push(`<circle cx="${x}" cy="${y}" r="${r + 1.4}" fill="${grey(0.9)}" opacity="0.75"/><circle cx="${x}" cy="${y}" r="${r}" fill="${grey(0.1)}"/>`);
    for (const [x, y, r] of p.smoke) out.push(`<circle cx="${x}" cy="${y}" r="${r}" fill="url(#${id}s)"/>`);
  }
  return `<figure class="recon-print"><svg viewBox="0 0 ${W} ${H}" role="img" aria-label="${label} the raid">
    <defs><filter id="${id}g"><feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="3"/><feColorMatrix type="saturate" values="0"/><feComponentTransfer><feFuncA type="table" tableValues="0 0.22"/></feComponentTransfer><feBlend in="SourceGraphic" mode="multiply"/></filter>
    <radialGradient id="${id}s"><stop offset="0" stop-color="${grey(0.95)}" stop-opacity="0.85"/><stop offset="1" stop-color="${grey(0.95)}" stop-opacity="0"/></radialGradient></defs>
    <g filter="url(#${id}g)">${out.join('')}</g>
    <rect x="0.5" y="0.5" width="${W - 1}" height="${H - 1}" fill="none" stroke="${grey(0.08)}" stroke-width="1"/>
    <rect x="6" y="6" width="${label.length * 9 + 10}" height="20" fill="${grey(0.06)}"/>
    <text x="11" y="21" font-family="'Courier New', monospace" font-size="14" font-weight="700" fill="${grey(0.97)}">${label}</text>
    <rect x="${W - frame.length * 6.1 - 14}" y="${H - 22}" width="${frame.length * 6.1 + 8}" height="16" fill="${grey(0.06)}"/>
    <text x="${W - 10}" y="${H - 10}" text-anchor="end" font-family="'Courier New', monospace" font-size="10" fill="${grey(0.97)}">${frame}</text>
  </svg></figure>`;
}

const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);

/** The recon photo block of a raid's card: before and after, and the interpreters' lines. '' with no photo. */
export function photoBlock(g: Game, r: AirReport): string {
  if (!r.photo) return '';
  const p = photoPrint(`${g.seed}:${r.id}`, r.photo);
  const d = turnToDate(r.turn);
  const sortie = 1000 + ((g.seed * 7 + r.id * 13) % 9000);
  const id = `rp${r.id}`;
  const lines = photoCaption(r.photo, r.place);
  return `<h3>The reconnaissance photographs</h3>
    <div class="recon-prints" style="display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:8px;margin:6px 0 8px">
      ${print(`${id}a`, p, false, 'BEFORE', `SORTIE ${sortie} · ${esc(d.label).toUpperCase()}`)}
      ${print(`${id}b`, p, true, 'AFTER', `SORTIE ${sortie + 1} · THE DAY AFTER`)}
    </div>
    <div class="note recon-words">${lines.map((l) => esc(l)).join(' ')}</div>`;
}
