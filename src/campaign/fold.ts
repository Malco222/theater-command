// Folding two worn corps into one (the grand campaign).
//
// Two corps of the player's, of the same kind, standing in the same province and both worn thin, can be made
// into one: the men of both fill the corps that is kept, the better of the two generals commands it, and the
// corps that is emptied goes home as a cadre (its staff, its name, its record and its honours) to be raised
// again from the arsenal at a share of the price of a new corps. The numbers are in data/campaign/manpower.json
// under 'fold'. No computer nation folds its corps, and the historical scenarios have no such order.
//
// Both interfaces read foldChoices(g, region) for what can be done and call foldCorps(g, keep, gone) to do it.
// The cadres themselves are kept on the nation (economy.ts: cadresOf, cadreTerms, orderCadre).

import { clamp } from '../core/math';
import { MEN_PER_DIV, type Corps, type Game, type General } from './types';
import { headline, removeCorps, worksOf } from './game';
import { appoint, generalOf } from './commanders';
import { MANPOWER, returnMen } from './manpower';
import { cadreTerms } from './economy';
import { noteCorps } from './stories';

export const foldRules = (g: Game): boolean => g.flags.open === 1 && !!MANPOWER.fold;

/** Why 'gone' cannot be folded into 'keep', or null if it can. */
export function whyNotFold(g: Game, keep: Corps, gone: Corps): string | null {
  if (!foldRules(g)) return 'Corps are folded together in the grand campaign.';
  if (keep === gone || keep.dead || gone.dead) return 'There is nothing to fold.';
  if (keep.nat !== gone.nat) return 'The two corps serve different countries.';
  if (keep.kind === 'fort' || gone.kind === 'fort') return 'Fortress troops stay in their works.';
  if (keep.reg !== gone.reg || keep.town !== undefined || gone.town !== undefined) return 'The two corps must stand together in the field, in the same province.';
  if (keep.kind !== gone.kind) return 'The two corps must be of the same kind.';
  if ((keep.elite ?? '') !== (gone.elite ?? '') || !!keep.airborne !== !!gone.airborne) return 'A special formation folds only into another of its own.';
  if (keep.joint || gone.joint) return 'A corps marching with an ally cannot be folded.';
  if (keep.leave !== undefined || gone.leave !== undefined) return 'A corps at home cannot be folded.';
  if (!keep.supplied || !gone.supplied) return 'Cut off, the cadre has no way home.';
  const worn = MANPOWER.fold.worn;
  if (keep.str >= worn || gone.str >= worn) return `Both corps must be under ${Math.round(worn * 100)}% of their strength.`;
  return null;
}

/** The better commander of two: the higher level, then the more experience, then the more battles won. */
const better = (a: General, b: General): boolean => a.level !== b.level ? a.level > b.level : a.xp !== b.xp ? a.xp > b.xp : a.rec.won > b.rec.won;

/** Who commands the kept corps once the two are one, and who is left without a corps. */
function command(g: Game, keep: Corps, gone: Corps): { lead: General | null; out: General | null } {
  const a = generalOf(g, keep), b = generalOf(g, gone);
  const lead = b && (!a || better(b, a)) ? b : a;
  return { lead, out: lead === b ? a : b };
}

/** What folding one corps into another would make of them. */
export interface FoldView {
  /** The corps that is kept and the corps that is emptied, by id and by name. */
  keep: number; gone: number; keepName: string; goneName: string;
  /** Why it cannot be done, or null when it can. */
  why: string | null;
  /** The kept corps afterwards: its strength (0 to 1), its men, its experience, its organisation and the moves it has left. */
  str: number; men: number; xp: number; org: number; ap: number;
  /** Men over, who go back to the reserve. */
  back: number;
  /** The general who commands the kept corps afterwards, and the one who goes to the list of generals waiting. */
  general: string | null; stepsDown: string | null;
  /** The kept corps takes over the other's fieldworks, better than its own: their months. */
  works?: number;
  /** What raising the cadre again will cost, the months it takes and the men it draws from the reserve. */
  cadreCost: number; cadreMonths: number; cadreMen: number;
}

/** What folding 'gone' into 'keep' would do, whether or not it is allowed. */
export function foldView(g: Game, keep: Corps, gone: Corps): FoldView {
  const mine = keep.str * keep.divs, theirs = gone.str * gone.divs;
  const fill = Math.min(keep.divs, mine + theirs), from = Math.max(0, fill - mine);
  const mix = (a: number, b: number): number => (fill > 0 ? (a * mine + b * from) / fill : a);
  const { lead, out } = command(g, keep, gone);
  const t = cadreTerms(g, gone.nat, gone);
  const v: FoldView = {
    keep: keep.id, gone: gone.id, keepName: keep.name, goneName: gone.name, why: whyNotFold(g, keep, gone),
    str: fill / keep.divs, men: Math.round(fill * MEN_PER_DIV), xp: mix(keep.xp, gone.xp),
    org: clamp(mix(keep.org, gone.org) - MANPOWER.fold.org, 0, 1), ap: Math.min(keep.ap, gone.ap),
    back: Math.round((mine + theirs - fill) * MEN_PER_DIV),
    general: lead?.name ?? null, stepsDown: out?.name ?? null,
    cadreCost: t.cost, cadreMonths: t.months, cadreMen: t.divs * MEN_PER_DIV,
  };
  if (worksOf(gone) > worksOf(keep)) v.works = worksOf(gone);
  return v;
}

/** Every way two of this nation's corps in a province could be folded together now: each allowed pair, both ways round. */
export function foldChoices(g: Game, reg: number, nat: string): FoldView[] {
  if (!foldRules(g)) return [];
  const cs = g.regions[reg].units.filter((c) => c.nat === nat && !c.dead);
  const out: FoldView[] = [];
  for (const keep of cs) for (const gone of cs) if (keep !== gone && whyNotFold(g, keep, gone) === null) out.push(foldView(g, keep, gone));
  return out;
}

/**
 * Folds one corps into another: the men of both fill the one that is kept, the better general commands it, and
 * the one that is emptied leaves the map as a cadre. Returns false if it is not allowed.
 */
export function foldCorps(g: Game, keep: Corps, gone: Corps): boolean {
  if (whyNotFold(g, keep, gone) !== null) return false;
  const v = foldView(g, keep, gone), n = g.nations[keep.nat], reg = keep.reg;
  const mine = keep.str * keep.divs, fill = v.str * keep.divs, from = Math.max(0, fill - mine);
  if (keep.mor !== undefined && gone.mor !== undefined && fill > 0) keep.mor = (keep.mor * mine + gone.mor * from) / fill;
  keep.xp = v.xp; keep.org = v.org; keep.str = v.str; keep.ap = v.ap;
  if (gone.fought) keep.fought = true;
  if (gone.pressed) keep.pressed = true;
  if ((gone.blows ?? 0) > (keep.blows ?? 0)) keep.blows = gone.blows;
  returnMen(g, keep.nat, gone.str * gone.divs - from);
  // The works the emptied corps stood in are the kept corps' own, where they are the better.
  if (v.works !== undefined) { keep.dug = true; keep.dugFor = gone.dugFor; if (keep.ap < keep.apMax) keep.relief = true; }
  // The better of the two generals commands; the other goes to the list of generals waiting for a corps.
  const { lead, out } = command(g, keep, gone);
  if (out) appoint(g, out, null);
  if (lead && lead.corps !== keep.id) appoint(g, lead, keep);
  noteCorps(g, keep, 'folded', reg, gone.id);
  noteCorps(g, gone, 'cadre', reg, keep.id);
  removeCorps(g, gone);
  gone.str = 0; gone.ap = 0;
  (n.cadres ??= []).push(gone);
  if (keep.nat === g.human) headline(g, 'info', `${gone.name} is folded into ${keep.name} at ${g.regions[reg].name}: ${v.men.toLocaleString('en-US')} men in one corps${v.back > 0 ? `, ${v.back.toLocaleString('en-US')} sent back to the reserve` : ''}. Its cadre has gone home and can be raised again from the arsenal.`, keep.nat, reg);
  return true;
}
