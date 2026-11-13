/**
 * Cities and towns: where the valuable places are, and what taking one earns.
 *
 * Every province has a chief place: the city it is named for, or the chief town of the stretch of country it is. This
 * module gives both interfaces the same list to draw (name, size, where it stands in the province, the signs it
 * carries for factories, harbour, oilfield, fortress, shipyard and airfield, and what it is worth), and the same
 * answer to the question a player asks before an attack: what do I get if I take it?
 *
 * Nothing here changes the war. It reads what is already true of each province and says it in one place. The names
 * and the size rules are in data/campaign/towns.json.
 */
import data from '../../data/campaign/towns.json';
import mapData from '../generated/campaign-map.json';
import type { Game, Region } from './types';
import { allied, atWar, theName } from './game';
import { BUDGET, budgeted, provinceYield } from './economy';
import { workLevel, yardLevel } from './arsenal';
import { railsAt, workingFactories } from './rails';
import { fieldYield, resRules } from './resources';
import { whyNoReturn, whyNotFreedBy } from './liberation';

export const TOWNS = data as unknown as {
  sizes: { city: { vp: number; fac: number }; town: { vp: number } };
  ranks: number[];
  words: Record<string, string>;
  names: Record<string, string>;
};

export type TownSize = 'capital' | 'city' | 'town' | 'village';
export type TownSign = 'factory' | 'harbour' | 'oil' | 'fort' | 'yard' | 'airfield';

/** The chief place of a province: its own name if it is named for a city, otherwise its chief town. */
export const townName = (r: Region): string => TOWNS.names[r.name] ?? r.name;

/** How big the chief place of a province is. A capital is the seat of a government that still stands. */
export function townSize(g: Game, r: Region): TownSize {
  const seat = g.nations[r.owner];
  if (r.cap && seat && seat.capital === r.id && seat.alive) return 'capital';
  const S = TOWNS.sizes;
  if (r.cap || r.vp >= S.city.vp || r.fac >= S.city.fac || r.terrain === 'urban') return 'city';
  if (r.vp >= S.town.vp || r.fac > 0 || r.port || r.oil) return 'town';
  return 'village';
}

// ---- Where the town stands in its province ------------------------------------------------------

type Ring = [number, number][];

function inRing(x: number, y: number, ring: Ring): boolean {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}

const inside = (polys: Ring[][], x: number, y: number): boolean => polys.some((p) => p.length > 0 && inRing(x, y, p[0]) && !p.slice(1).some((hole) => inRing(x, y, hole)));

/**
 * Places for a town beside the centre of its province, where the army's flag stands: up and to the left of the flag
 * first, as a map sets a city's mark beside its name, then to the right, above, and at its foot. The first that lies
 * well inside the province is taken.
 */
const SPOTS: [number, number][] = [[-22, -8], [22, -8], [-18, -9], [18, -9], [0, -22], [-24, 5], [24, 5], [-15, -10], [15, -10], [0, -16], [0, 34]];
/** Where a province is too small for any of those, nearer in, and at the last on its centre. */
const TIGHT: [number, number][] = [[-12, -7], [12, -7], [0, -10], [-9, 2], [9, 2], [0, 0]];
const ROOM = 3.5;

const SITES: [number, number][] = ((mapData as any).regions as { x: number; y: number; polys: Ring[][] }[]).map((r) => {
  const fits = (dx: number, dy: number, room: number): boolean => {
    const x = r.x + dx, y = r.y + dy;
    return inside(r.polys, x, y) && (room === 0 || [[-room, 0], [room, 0], [0, -room], [0, room]].every(([a, b]) => inside(r.polys, x + a, y + b)));
  };
  const spot = SPOTS.find(([dx, dy]) => fits(dx, dy, ROOM)) ?? SPOTS.find(([dx, dy]) => fits(dx, dy, 0)) ?? TIGHT.find(([dx, dy]) => fits(dx, dy, 0)) ?? [0, 0];
  return [Math.round((r.x + spot[0]) * 10) / 10, Math.round((r.y + spot[1]) * 10) / 10];
});

/** Where the town of a province stands on the map: a fixed place beside the province's centre, the same in every interface. */
export const townSite = (region: number): [number, number] => SITES[region] ?? [0, 0];

// ---- The list for a map -------------------------------------------------------------------------

export interface TownView {
  region: number;
  /** The town, and the province it is the chief place of (the same word where the province is named for its city). */
  name: string;
  province: string;
  size: TownSize;
  sizeWord: string;
  x: number;
  y: number;
  owner: string;
  holder: string;
  /** What the province pays its own country a month when all is well, in money as the interface shows it, and that worth ranked 0 to 5. */
  worth: number;
  rank: number;
  signs: TownSign[];
  factories: number;
  vp: number;
}

/** What a province pays its own country a month at its best: land, harbour, oilfield, factories, and the seat of government. */
export function worthOf(g: Game, r: Region): number {
  if (!budgeted(g)) return Math.round(r.fac * BUDGET.scale);
  const k = r.overseas ? BUDGET.overseas : 1;
  const seat = g.nations[r.owner];
  const capital = r.cap && seat && seat.capital === r.id ? BUDGET.capital : 0;
  return Math.round((BUDGET.land[r.terrain] * k + (r.port ? BUDGET.port * k : 0) + (r.oil ? BUDGET.oil * k : 0) + r.fac + capital) * BUDGET.scale);
}

const rankOf = (worth: number): number => TOWNS.ranks.filter((x) => worth >= x).length;

function signsOf(g: Game, r: Region): TownSign[] {
  const out: TownSign[] = [];
  if (r.fac > 0) out.push('factory');
  if (r.port) out.push('harbour');
  if (r.oil) out.push('oil');
  if (r.fort) out.push('fort');
  if (yardLevel(g, r) > 0) out.push('yard');
  if (workLevel(g, 'field', r.id) > 0) out.push('airfield');
  return out;
}

export function townAt(g: Game, region: number): TownView | null {
  const r = g.regions[region];
  if (!r) return null;
  const size = townSize(g, r);
  const [x, y] = townSite(region);
  const worth = worthOf(g, r);
  return {
    region, name: townName(r), province: r.name, size, sizeWord: TOWNS.words[size], x, y, owner: r.owner, holder: r.ctrl,
    worth, rank: rankOf(worth), signs: signsOf(g, r), factories: r.fac, vp: r.vp,
  };
}

/** Every city, town and village on the map, for an interface to draw. */
export const townsOf = (g: Game): TownView[] => g.regions.map((r) => townAt(g, r.id)!);

// ---- The prize ----------------------------------------------------------------------------------

export interface PrizeItem { key: string; label: string; text: string }

export interface Prize {
  region: number;
  town: string;
  nat: string;
  /** 'yours' if the nation holds it already, 'ally' if an ally does, 'taken' if taking it would make it the nation's own to hold, 'restored' if it would go back to an allied owner or to a fallen country it frees. */
  kind: 'yours' | 'ally' | 'taken' | 'restored';
  /** Who would hold the province afterwards. */
  to: string;
  /** Money a month to the nation's treasury if it holds the province (as the interface shows money), and what the present holder draws from it now. */
  income: number;
  incomeNow: number;
  factories: number;
  /** Oil a month to the nation's depots, and what the present holder draws now (the grand campaign; 0 elsewhere). */
  oil: number;
  oilNow: number;
  harbour: boolean;
  yard: number;
  airfield: number;
  fort: number;
  vp: number;
  /** The nation whose seat of government this is, if it still stands; and the fallen country that taking the place would bring back. */
  capitalOf: string | null;
  frees: string | null;
  /** How many railway lines meet here (the grand campaign). */
  rails: number;
  items: PrizeItem[];
  /** One sentence: "Taking Lodz is worth 59 a month to you: 1 factory and 1 victory point of Poland's 17." */
  line: string;
}

const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;

/** What holding a province is worth to a nation: what it has there if it is its own, or what taking it would bring. */
export function prizeOf(g: Game, region: number, nat: string | null = g.human): Prize | null {
  const r = g.regions[region];
  if (!r || !nat || !g.nations[nat]) return null;
  const owner = g.nations[r.owner], holder = g.nations[r.ctrl];
  const mine = r.ctrl === nat;
  const friendly = !mine && allied(g, nat, r.ctrl);
  // A fallen country whose capital this is, and which this nation's taking it would bring back into the war.
  let frees: string | null = null;
  if (!mine && !friendly) {
    for (const X of Object.values(g.nations)) {
      if (X.alive || X.capital !== r.id || X.id === nat) continue;
      if (!whyNoReturn(g, X.id) && !whyNotFreedBy(g, X.id, nat, true)) frees = X.id;
    }
  }
  // Ground taken from an enemy goes back to its own country if that country is an ally still in the war, or is freed by it.
  const restored = !mine && !friendly && (frees !== null || (r.owner !== nat && owner.alive && allied(g, nat, r.owner)));
  const kind: Prize['kind'] = mine ? 'yours' : friendly ? 'ally' : restored ? 'restored' : 'taken';
  const to = kind === 'restored' ? (frees ?? r.owner) : kind === 'ally' ? r.ctrl : nat;
  const scale = BUDGET.scale;
  const income = Math.round(provinceYield(g, r, to === nat ? nat : to).total * scale);
  // What its holder stands to lose for good: a siege that stops its pay for now is left out of account.
  const incomeNow = Math.round(provinceYield(g, r, r.ctrl, true).total * scale);
  const oil = resRules(g) ? fieldYield(g, r, to) : 0, oilNow = resRules(g) ? fieldYield(g, r, r.ctrl) : 0;
  const factories = workingFactories(g, r);
  const yard = yardLevel(g, r), airfield = workLevel(g, 'field', r.id), fort = r.fort?.lv ?? 0;
  const capitalOf = r.cap && owner.capital === r.id && owner.alive ? r.owner : null;
  const rails = g.flags.open === 1 ? railsAt(r.id).length : 0;
  let vpAll = 0;
  for (const x of g.regions) if (x.owner === r.owner && !x.overseas) vpAll += x.vp;

  const town = townName(r);
  const items: PrizeItem[] = [];
  const whose = to === nat ? 'your treasury' : `${theName(g.nations[to])}'s treasury`;
  if (budgeted(g) || factories) {
    const y = provinceYield(g, r, to);
    const parts = [y.land ? `land ${Math.round(y.land * scale)}` : '', y.port ? `harbour ${Math.round(y.port * scale)}` : '', y.oil ? `oilfield ${Math.round(y.oil * scale)}` : '',
      y.fac ? `${plural(factories, 'factory', 'factories')} ${Math.round(y.fac * scale)}` : '', y.capital ? `seat of government ${Math.round(y.capital * scale)}` : ''].filter(Boolean);
    items.push({ key: 'income', label: 'Money', text: `${income} a month to ${whose}${parts.length ? ` (${parts.join(', ')})` : ''}${to !== r.owner ? '. Conquered ground pays less than it pays its own country' : ''}` });
  }
  if (r.fac > 0) items.push({ key: 'factory', label: 'Industry', text: `${plural(r.fac, 'factory', 'factories')}${factories < r.fac ? `, ${r.fac - factories} wrecked by bombing` : ''}${to !== r.owner ? '. Working for a conqueror they turn out less, and heavy equipment can only be built at home' : ''}` });
  if (r.oil) items.push({ key: 'oil', label: 'Oil', text: resRules(g) ? `An oilfield: ${Math.round(oil * 10) / 10} a month to ${to === nat ? 'your' : `${g.nations[to].adj}`} depots${to !== r.owner ? ', less than its owner draws' : ''}` : 'An oilfield' });
  if (r.port) items.push({ key: 'harbour', label: 'Harbour', text: yard > 0 ? `A harbour with a shipyard of level ${yard}: a base for fleets, and a port for supply and for landings` : 'A harbour: a base for fleets, and a port for supply and for landings' });
  if (airfield > 0) items.push({ key: 'airfield', label: 'Airfield', text: `An airfield of level ${airfield} to fly from` });
  if (rails >= 3) items.push({ key: 'rail', label: 'Railway', text: `A junction where ${rails} lines meet` });
  if (r.vp > 0 && !r.overseas) items.push({ key: 'vp', label: 'Victory points', text: `${r.vp} of ${theName(owner)}'s ${vpAll}: a country that has lost its capital and enough of these gives up the war` });
  if (capitalOf && capitalOf !== nat && !allied(g, nat, capitalOf)) items.push({ key: 'capital', label: 'Capital', text: `The capital of ${theName(owner)}. Its fall may knock ${owner.name} out of the war, and shakes the morale of every corps it has` });
  if (frees) items.push({ key: 'frees', label: 'Liberation', text: `The capital of ${theName(g.nations[frees])}, which has fallen. Take it and ${g.nations[frees].name} returns to the war on your side` });
  if (fort > 0 && !mine && !friendly) items.push({ key: 'fort', label: 'Fortress', text: `Works of level ${fort} stand in the way` });

  const gains = [
    factories ? plural(factories, 'factory', 'factories') : '', r.oil ? 'an oilfield' : '', r.port ? (yard > 0 ? 'a harbour and shipyard' : 'a harbour') : '',
    r.vp > 0 && !r.overseas ? `${plural(r.vp, 'victory point')} of ${theName(owner)}'s ${vpAll}` : '',
  ].filter(Boolean);
  const tail = gains.length ? `: ${listed(gains)}` : '';
  const line = kind === 'yours' ? `${town} pays ${income} a month into your treasury${tail}.`
    : kind === 'ally' ? `${town} is held by ${theName(holder)}, your ally, and pays ${incomeNow} a month into its treasury${tail}.`
    : frees ? `Taking ${town} frees ${g.nations[frees].name}, which returns to the war on your side and takes its capital back. ${theName(holder, true)} loses ${incomeNow} a month${tail}.`
    : kind === 'restored' ? `Taking ${town} gives it back to ${theName(owner)}, your ally, and takes ${incomeNow} a month from ${theName(holder)}${tail}.`
    : `Taking ${town} is worth ${income} a month to you and costs ${theName(holder)} ${incomeNow}${tail}.${capitalOf && atWar(g, nat, capitalOf) ? ` It is ${theName(owner)}'s capital.` : ''}`;
  return { region, town, nat, kind, to, income, incomeNow, factories, oil, oilNow, harbour: r.port, yard, airfield, fort, vp: r.vp, capitalOf, frees, rails, items, line };
}

/**
 * The richest prizes a nation could take next: enemy-held provinces that border ground it or its allies hold, or that
 * its troops stand beside, the most valuable first. A capital, and a capital whose fall frees a country, come ahead of
 * money; among the rest, money to the treasury, then victory points.
 */
export function prizesInReach(g: Game, nat: string | null = g.human, most = 5): Prize[] {
  if (!nat || !g.nations[nat]?.alive) return [];
  const out: Prize[] = [];
  for (const r of g.regions) {
    if (!atWar(g, nat, r.ctrl)) continue;
    const near = r.nb.some((j) => {
      const x = g.regions[j];
      return x.ctrl === nat || x.units.some((c) => c.nat === nat && !c.dead);
    });
    if (!near) continue;
    const p = prizeOf(g, r.id, nat);
    if (p) out.push(p);
  }
  const weight = (p: Prize): number => (p.frees ? 2000 : 0) + (p.capitalOf ? 1000 : 0) + Math.max(p.income, p.incomeNow / 2) + p.vp * 25 + (p.oil > 0 ? 60 : 0);
  return out.sort((a, b) => weight(b) - weight(a) || a.region - b.region).slice(0, most);
}
