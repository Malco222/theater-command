/**
 * The gates and what they say (Version 82; the design director's rulings 86 and 90(b), on Brian's ask of 2026-11-16:
 * "make it so ships can go through the panama canal as long as you or your allies or a netutral country owns panama").
 *
 * The rule is canPass in navy.ts, and it is not changed here: this file only reads it, so that he can see it. For
 * the Panama Canal and the three straits it gives the holder, whether the gate is open to the viewer's ships, and
 * the words for it, each from that gate's own rule:
 * - an ordinary gate (the Panama Canal, the Danish straits, the Strait of Gibraltar) is shut only to a country at
 *   war with its holder, so "you are at war with X, who holds P";
 * - the Turkish straits are stricter: in wartime only the holder's allies may pass, and a country at war with nobody.
 *   Shut there never says "you are at war with Turkey" to a player who is not.
 *
 * Version 84.1 (ruling 176(e), on Brian's words of 2026-11-17): the holder may shut his gate to all ships but his
 * own and his allies', and open it again. The view says who shut it, since when and to whom, and whether the
 * viewer may give the order; the order itself and its question are whyNotGate, gateQuestion and orderGate in turn.ts.
 *
 * What an interface calls:
 *   gateViews(g, viewer)     every gate: its name, holder, open or shut to the viewer, and the pointing box's line
 *   gateSpot(g, gate)        where its mark stands on the map: on the water, at the gate
 *   gateBar(g, fleet, sea)   the shut gate that keeps a fleet from a sea it could otherwise reach, with the one line that says why
 */
import { turnToDate } from '../core/ticket';
import { allied, atWar, theName } from './game';
import { NAVY, baseSeas, canPass, gateHolder, gateShutSince, seaAt, seaBorders } from './navy';
import type { Fleet, Game, Gate } from './types';

export interface GateView {
  /** Its place on the game's list of gates. */
  id: number;
  /** Its name as a title: "Panama Canal". */
  name: string;
  /** Its name as it opens a sentence: "The Panama Canal". */
  theName: string;
  /** The country that holds it. */
  holder: string;
  /** The province whose holder holds the gate, or -1 where it is held from off the map. */
  region: number;
  /** Open to the viewer's ships; null when nobody is playing. */
  open: boolean | null;
  /** The line under its name in the pointing box: who holds it, and whether his ships may pass. */
  line: string;
  /** Shut: why, as the end of a sentence. Empty when it is open. */
  why: string;
  /** The month its holder ordered it shut (ruling 176(e)), or -1 while it stands as its own rule leaves it. */
  shutSince: number;
  /** Where its holder has shut it: who shut it, when, and to whom, as one sentence for the plate. Empty otherwise. */
  shutLine: string;
  /** The order the viewer may give: he holds the gate and is the player. Null for anyone else. */
  order: 'shut' | 'open' | null;
  /** The pointing box's last line: for the holder the order's name, for anyone else that only its holder can give it. */
  orderLine: string;
}

const upFirst = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
/** A province's name inside a sentence: "the Canal Zone", "Gibraltar". */
const thePlace = (name: string): string => (/ (Zone|Straits?|Coast|Islands?)$/.test(name) ? `the ${name}` : name);
function view(g: Game, gt: Gate, id: number, viewer: string | null): GateView {
  const holder = gateHolder(g, gt), H = g.nations[holder];
  const who = H ? theName(H) : holder;
  const name = upFirst(gt.name.replace(/^the /i, ''));
  const mine = holder === viewer;
  const since = gateShutSince(g, id);
  const shutLine = since < 0 ? '' : mine ? `You shut it in ${turnToDate(since).label}: only your own ships and your allies' pass.`
    : `${upFirst(who)} shut it in ${turnToDate(since).label}: only its own ships and its allies' pass.`;
  const order: GateView['order'] = mine && viewer === g.human && !!H?.alive ? (since < 0 ? 'shut' : 'open') : null;
  const orderLine = order === 'shut' ? 'Shut the gate' : order === 'open' ? 'Open the gate' : viewer ? `Only its holder can ${since < 0 ? 'shut' : 'open'} it.` : '';
  const base = { id, name, theName: upFirst(gt.name), holder, region: gt.region, shutSince: since, shutLine, order, orderLine };
  if (!viewer || !g.nations[viewer]) return { ...base, open: null, line: `Held by ${who}.${shutLine ? ` ${shutLine}` : ''}`, why: '' };
  const open = canPass(g, gt.a, gt.b, viewer);
  const held = mine ? 'You hold it.' : `Held by ${who}.`;
  if (since >= 0) {
    if (open) return { ...base, open, line: mine ? "You hold it, and you have shut it: only your own ships and your allies' pass." : `${held} It is shut by its holder's order. Open to your ships: you are an ally of ${who}.`, why: '' };
    // Shut to the viewer by the order, unless the gate's own rule would turn his ships back in any case: then that is the reason given.
    const byItsRule = gt.closed ? !g.nations[viewer].atWar.length : !atWar(g, holder, viewer);
    if (byItsRule) {
      const why = `${who} has shut it to all ships but its own and its allies'`;
      return { ...base, open, line: `Shut to your ships: ${why}.`, why };
    }
  }
  if (gt.closed) {
    // In wartime only the holder's allies may pass, and a country at war with nobody.
    const peace = !g.nations[viewer].atWar.length;
    if (open) return { ...base, open, line: `${held} Open to your ships${mine ? '' : allied(g, holder, viewer) ? `: you are an ally of ${who}` : peace ? ': you are at war with nobody' : ''}.`, why: '' };
    const why = mine ? 'in wartime only your allies may pass' : `in wartime only ${who}${/s$/.test(who) ? "'" : "'s"} allies may pass`;
    return { ...base, open, line: `Shut to your ships: ${why}.`, why };
  }
  if (open) return { ...base, open, line: `${held} Open to your ships.`, why: '' };
  const why = `you are at war with ${who}, who holds ${gt.region >= 0 ? thePlace(g.regions[gt.region].name) : 'it'}`;
  return { ...base, open, line: `Shut to your ships: ${why}.`, why };
}

/** Every gate on the map as the viewer finds it. */
export const gateViews = (g: Game, viewer: string | null = g.human): GateView[] => g.gates.map((gt, id) => view(g, gt, id, viewer));

/** True where a point of the map lies on a province: the land, by the provinces' own outlines (the seas' own grid is too coarse for a strait). */
export function onLand(g: Game, x: number, y: number, within = 260): boolean {
  for (const r of g.regions) {
    if (Math.abs(r.x - x) > within || Math.abs(r.y - y) > within) continue;
    for (const poly of r.polys) {
      let inside = false;
      poly.forEach((ring, k) => {
        let odd = false;
        for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
          const [xi, yi] = ring[i], [xj, yj] = ring[j];
          if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) odd = !odd;
        }
        // The first ring is the shore; any other is a hole in it.
        if (odd) inside = k === 0;
      });
      if (inside) return true;
    }
  }
  return false;
}

/** Where a gate's mark stands: on the water where its two seas meet, as near the ground that holds it as may be. */
export function gateSpot(g: Game, gt: Gate): { x: number; y: number } {
  const A = g.seas[gt.a], B = g.seas[gt.b];
  const near = gt.region >= 0 ? { x: g.regions[gt.region].x, y: g.regions[gt.region].y } : { x: (A.x + B.x) / 2, y: (A.y + B.y) / 2 };
  let best: { x: number; y: number } | null = null, bd = Infinity;
  // The line drawn between the two seas, where there is one: its point nearest the holder.
  for (const line of seaBorders(g)) {
    for (let i = 0; i + 1 < line.length; i++) {
      const [x0, y0] = line[i], [x1, y1] = line[i + 1];
      const mx = (x0 + x1) / 2, my = (y0 + y1) / 2, len = Math.hypot(x1 - x0, y1 - y0) || 1;
      const nx = -(y1 - y0) / len, ny = (x1 - x0) / len;
      for (const d of [5, 10, 16]) {
        const p = seaAt(mx + nx * d, my + ny * d, g), q = seaAt(mx - nx * d, my - ny * d, g);
        if (!((p === gt.a && q === gt.b) || (p === gt.b && q === gt.a))) continue;
        const far = Math.hypot(mx - near.x, my - near.y);
        if (far < bd) { bd = far; best = { x: mx, y: my }; }
        break;
      }
    }
  }
  if (best && !onLand(g, best.x, best.y)) return best;
  // No water joins them on the map (a canal through the land), or the line between them runs ashore there: the
  // nearest open water of either sea to the holder, a little off the shore.
  const from = best ?? near;
  best = null;
  for (let r = 3; r <= 120 && !best; r += 2) {
    for (let k = 0; k < 32; k++) {
      const ux = Math.cos((k * Math.PI) / 16), uy = Math.sin((k * Math.PI) / 16);
      const x = from.x + r * ux, y = from.y + r * uy;
      const z = seaAt(x, y, g);
      if ((z !== gt.a && z !== gt.b) || onLand(g, x, y) || onLand(g, x + 3 * ux, y + 3 * uy)) continue;
      const far = Math.hypot(x - (A.x + B.x) / 2, y - (A.y + B.y) / 2);
      if (far < bd) { bd = far; best = { x: x + 3 * ux, y: y + 3 * uy }; }
    }
  }
  return best ?? from;
}

/**
 * The shut gate that keeps a fleet from a sea: the sea is beyond its reach as the gates stand and within it were
 * they open. Null where no gate is the reason. The line is the one a refused order says.
 */
export function gateBar(g: Game, f: Fleet, zone: number): { gate: GateView; line: string } | null {
  const walk = (free: boolean): { d: Int16Array; via: Int16Array } => {
    const d = new Int16Array(g.seas.length).fill(-1), via = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (const z of baseSeas(g, f)) if (d[z] < 0) { d[z] = 1; q.push(z); }
    for (let i = 0; i < q.length; i++) {
      if (d[q[i]] >= NAVY.range) continue;
      for (const j of g.seas[q[i]].adj) {
        if (d[j] >= 0) continue;
        const pass = canPass(g, q[i], j, f.nat);
        if (!pass && !free) continue;
        d[j] = d[q[i]] + 1;
        // The first shut gate on the way there.
        via[j] = via[q[i]] >= 0 ? via[q[i]] : pass ? -1 : g.gates.findIndex((x) => (x.a === q[i] && x.b === j) || (x.a === j && x.b === q[i]));
        q.push(j);
      }
    }
    return { d, via };
  };
  if (walk(false).d[zone] >= 0) return null;
  const open = walk(true);
  if (open.d[zone] < 0 || open.via[zone] < 0) return null;
  const gate = view(g, g.gates[open.via[zone]], open.via[zone], f.nat);
  return { gate, line: `${gate.theName} ${/s$/.test(gate.name) ? 'are' : 'is'} closed to your ships: ${gate.why}.` };
}
