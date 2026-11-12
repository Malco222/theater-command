// The flags the armies marched under, 1939 to 1945, drawn as plain shapes in a 22 by 14 box. They are the
// national flags of the time (the Kingdom of Italy's Savoy arms, the 48-star flag, the Kingdom of Iraq), with
// one deliberate exception: Germany flies the black, white and red tricolour with an iron cross, not the party
// flag, as most games of the period do.

const W = 22, H = 14;
const WHITE = '#f3f1ea', BLACK = '#1b1b1d', RED = '#c8102e', BLUE = '#21468b', GOLD = '#d9a520';

const rect = (x: number, y: number, w: number, h: number, fill: string): string => `<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${fill}"/>`;
const field = (fill: string): string => rect(0, 0, W, H, fill);
/** Bands from top to bottom. */
const hb = (...c: string[]): string => c.map((col, i) => rect(0, +((H * i) / c.length).toFixed(2), W, +(H / c.length + 0.06).toFixed(2), col)).join('');
/** Bands from the hoist to the fly. */
const vb = (...c: string[]): string => c.map((col, i) => rect(+((W * i) / c.length).toFixed(2), 0, +(W / c.length + 0.06).toFixed(2), H, col)).join('');
/** A Scandinavian cross, set toward the hoist. */
const nordic = (bg: string, fg: string, t: number, inner = '', ti = 0): string =>
  field(bg) + rect(7.2 - t / 2, 0, t, H, fg) + rect(0, 7 - t / 2, W, t, fg) + (inner ? rect(7.2 - ti / 2, 0, ti, H, inner) + rect(0, 7 - ti / 2, W, ti, inner) : '');
const disc = (cx: number, cy: number, r: number, fill: string): string => `<circle cx="${cx}" cy="${cy}" r="${r}" fill="${fill}"/>`;
/** A star with the given number of points, one of them straight up. */
function star(cx: number, cy: number, r: number, fill: string, points = 5, inner = 0.4): string {
  const pts: string[] = [];
  for (let i = 0; i < points * 2; i++) {
    const a = (Math.PI * i) / points - Math.PI / 2, q = i % 2 ? r * inner : r;
    pts.push(`${(cx + q * Math.cos(a)).toFixed(2)} ${(cy + q * Math.sin(a)).toFixed(2)}`);
  }
  return `<path d="M${pts.join('L')}Z" fill="${fill}"/>`;
}
/** A small shield, as on the arms of Savoy or Hungary. */
const shield = (cx: number, top: number, w: number, h: number, fill: string, edge: string): string =>
  `<path d="M${cx - w / 2} ${top}H${cx + w / 2}V${top + h * 0.6}Q${cx + w / 2} ${top + h * 0.9} ${cx} ${top + h}Q${cx - w / 2} ${top + h * 0.9} ${cx - w / 2} ${top + h * 0.6}Z" fill="${fill}" stroke="${edge}" stroke-width=".6"/>`;
/** The Union Flag, in a box of its own so that it can sit in the corner of an ensign. */
const union = (w: number, h: number): string => {
  const k = Math.min(w / W, h / H);
  return `<svg x="0" y="0" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">${rect(0, 0, w, h, '#1f3a7a')}
    <path d="M0 0L${w} ${h}M${w} 0L0 ${h}" stroke="${WHITE}" stroke-width="${(3 * k).toFixed(2)}"/><path d="M0 0L${w} ${h}M${w} 0L0 ${h}" stroke="${RED}" stroke-width="${(1 * k).toFixed(2)}"/>
    <path d="M${w / 2} 0V${h}M0 ${h / 2}H${w}" stroke="${WHITE}" stroke-width="${(4.4 * k).toFixed(2)}"/><path d="M${w / 2} 0V${h}M0 ${h / 2}H${w}" stroke="${RED}" stroke-width="${(2.6 * k).toFixed(2)}"/></svg>`;
};
/** A British ensign: the Union Flag in the corner of a plain field, with the country's badge in the fly. */
const ensign = (bg: string, badge: string): string => field(bg) + union(11, 7) + badge;

/** The iron cross: four arms that widen toward their ends. */
function ironCross(cx: number, cy: number, a: number, b: number, c: number): string {
  const p: [number, number][] = [[-b, -a], [b, -a], [c, -c], [a, -b], [a, b], [c, c], [b, a], [-b, a], [-c, c], [-a, b], [-a, -b], [-c, -c]];
  return `<path d="M${p.map(([x, y]) => `${cx + x} ${cy + y}`).join('L')}Z" fill="${BLACK}" stroke="${WHITE}" stroke-width=".8" stroke-linejoin="round"/>`;
}

function stripes(): string {
  let s = '';
  for (let i = 0; i < 13; i++) s += rect(0, +((H * i) / 13).toFixed(2), W, +(H / 13 + 0.06).toFixed(2), i % 2 ? WHITE : '#b22234');
  s += rect(0, 0, 9.4, +((H * 7) / 13).toFixed(2), '#3c3b6e');
  for (let r = 0; r < 4; r++) for (let c = 0; c < 6; c++) s += disc(+(0.95 + c * 1.5).toFixed(2), +(1 + r * 1.85).toFixed(2), 0.33, WHITE);
  return s;
}

/** What is drawn inside each nation's flag. */
export const FLAGS: Record<string, string> = {
  GER: hb(BLACK, WHITE, RED) + ironCross(11, 7, 4.3, 2.4, 0.8),
  ITA: vb('#1b8a4b', WHITE, RED) + shield(11, 4.4, 3.8, 5.4, RED, BLUE) + `<path d="M11 4.9V9.2M9.5 6.7H12.5" stroke="${WHITE}" stroke-width=".9"/>`,
  UK: union(W, H),
  FRA: vb(BLUE, WHITE, RED),
  VIC: vb(BLUE, WHITE, RED) + `<path d="M11 3V11" stroke="${GOLD}" stroke-width=".8"/><path d="M11 3.8Q8.5 4 8.7 6.6Q9.8 5.5 11 5.8Q12.2 5.5 13.3 6.6Q13.5 4 11 3.8Z" fill="${BLUE}"/>`,
  POL: hb(WHITE, '#d4213d'),
  SOV: field('#cc1b1b') + star(4.4, 2.5, 1.5, GOLD) + `<path d="M3 5.2A2.3 2.3 0 1 0 6.1 8.1" fill="none" stroke="${GOLD}" stroke-width=".8" stroke-linecap="round"/><path d="M3.1 8.4L5.6 5.6M4.9 5L6.3 6.3" stroke="${GOLD}" stroke-width=".9" stroke-linecap="round"/>`,
  USA: stripes(),
  BEL: vb(BLACK, '#fdda24', '#ef3340'),
  NED: hb('#ae1c28', WHITE, BLUE),
  LUX: hb('#ed2939', WHITE, '#40b4e5'),
  DEN: nordic(RED, WHITE, 2),
  NOR: nordic('#ba0c2f', WHITE, 3.4, '#00205b', 1.7),
  SWE: nordic('#006aa7', '#fecc00', 2.4),
  FIN: nordic(WHITE, '#003580', 3),
  EST: hb('#0072ce', BLACK, WHITE),
  LAT: field('#9e3039') + rect(0, 5.6, W, 2.8, WHITE),
  LIT: hb('#fdb913', '#006a44', '#c1272d'),
  IRL: vb('#169b62', WHITE, '#ff883e'),
  SUI: field('#d52b1e') + rect(9.6, 2.8, 2.8, 8.4, WHITE) + rect(6.8, 5.6, 8.4, 2.8, WHITE),
  ESP: field('#c60b1e') + rect(0, 3.5, W, 7, '#ffc400') + `<path d="M7.6 5.2Q11 3.4 14.4 5.2L13.2 6.3V8.6Q11 10.6 8.8 8.6V6.3Z" fill="${BLACK}"/>` + rect(10, 6.1, 2, 2.3, '#c60b1e'),
  POR: rect(0, 0, 8.8, H, '#046a38') + rect(8.8, 0, 13.2, H, '#da291c') + disc(8.8, 7, 2.7, '#ffe900') + shield(8.8, 5.6, 2.2, 2.9, '#da291c', WHITE),
  HUN: hb('#ce2939', WHITE, '#477050') + rect(10, 3.6, 2, 0.9, GOLD) + shield(11, 4.5, 3.4, 5, '#ce2939', GOLD) + `<path d="M9.5 5.7H11M9.5 6.9H11" stroke="${WHITE}" stroke-width=".5"/>` + rect(11, 4.8, 1.4, 3.6, '#477050'),
  ROM: vb('#002b7f', '#fcd116', '#ce1126'),
  YUG: hb('#0c4076', WHITE, RED),
  BUL: hb(WHITE, '#00966e', '#d62612'),
  GRE: field('#0d5eaf') + rect(9.6, 0, 2.8, H, WHITE) + rect(0, 5.6, W, 2.8, WHITE),
  TUR: field('#e30a17') + disc(8.6, 7, 3.4, WHITE) + disc(9.5, 7, 2.7, '#e30a17') + star(12.9, 7, 1.6, WHITE),
  IRQ: hb(BLACK, WHITE, '#007a3d') + `<path d="M0 0L6.6 4.67V9.33L0 14Z" fill="#ce1126"/>` + star(2.5, 5.2, 1, WHITE, 7, 0.5) + star(2.5, 8.8, 1, WHITE, 7, 0.5),
  IRN: hb('#239f40', WHITE, '#da0000') + disc(11.6, 6.6, 1.5, GOLD) + `<path d="M8.8 8.4Q10.4 6.6 12.6 8.4" fill="none" stroke="${GOLD}" stroke-width=".9" stroke-linecap="round"/>`,
  SVK: hb(WHITE, '#0b4ea2', '#ee1c25'),
  // Nations of the wider war.
  JPN: field(WHITE) + disc(11, 7, 4.2, '#bc002d'),
  CHN: field(RED) + rect(0, 0, 11, 7, '#000095') + star(5.5, 3.5, 2.7, WHITE, 12, 0.62) + disc(5.5, 3.5, 1.3, WHITE),
  CAN: ensign('#c8102e', shield(16.5, 5.4, 3.6, 4.6, WHITE, GOLD) + rect(15.2, 8, 2.6, 1.2, '#1b8a4b')),
  AUS: ensign('#012169', star(5.5, 10.6, 1.9, WHITE, 7, 0.5) + star(16.5, 2.6, 0.9, WHITE, 7, 0.5) + star(13.6, 6.4, 0.9, WHITE, 7, 0.5) + star(19.2, 5.6, 0.9, WHITE, 7, 0.5) + star(16.5, 11.2, 0.9, WHITE, 7, 0.5)),
  NZL: ensign('#012169', star(16.5, 2.8, 1.1, RED, 5, 0.45) + star(14, 6.6, 1.1, RED, 5, 0.45) + star(19, 6, 1.1, RED, 5, 0.45) + star(16.5, 11, 1.1, RED, 5, 0.45)),
  IND: ensign('#c8102e', disc(16.5, 7, 2.6, GOLD) + star(16.5, 7, 1.7, WHITE, 5, 0.45)),
  SAF: hb('#ff7900', WHITE, '#003580') + rect(9.2, 5.5, 3.6, 3, '#21468b') + rect(10.4, 5.5, 1.2, 3, '#ff7900'),
};

/** Every flag as a symbol the map's pieces can stamp out by name. */
export function flagDefs(): string {
  return Object.entries(FLAGS).map(([id, inner]) => `<symbol id="fl-${id}" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none">${inner}</symbol>`).join('');
}

/** A flag on the map, stamped from its symbol. A nation without one flies its map colour. */
export function flagUse(nat: string, color: string, w = W, h = H): string {
  return FLAGS[nat] ? `<use href="#fl-${nat}" width="${w}" height="${h}"/>` : `<rect width="${w}" height="${h}" fill="${color}"/>`;
}

/** A flag for a panel or a card: a small picture of its own. */
export function flagIcon(nat: string, color: string, w = 22, cls = ''): string {
  const h = Math.round((w * H) / W);
  return `<svg class="flag${cls ? ' ' + cls : ''}" width="${w}" height="${h}" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none" aria-hidden="true">${FLAGS[nat] ?? rect(0, 0, W, H, color)}<rect x=".3" y=".3" width="${W - 0.6}" height="${H - 0.6}" fill="none" stroke="rgba(10,12,14,.55)" stroke-width=".6"/></svg>`;
}
