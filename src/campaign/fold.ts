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
import { headline, men, removeCorps, worksOf } from './game';
import { carryBattle } from './drawn';
import { appoint, generalOf } from './commanders';
import { MANPOWER, men1000, menScale, menWorth, returnMen } from './manpower';
import { cadreTerms } from './economy';
import { binds, inWords, orderNoun, riflesBack, riflesFold, storeRifles } from './rifles';
import { noteCorps } from './stories';
import { atItsPost, postWords } from './pacific';

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
  // A volunteer legion keeps its own name and flag (legions.ts; release 2): it is never folded into its patron's corps.
  if ((keep.vol ?? '') !== (gone.vol ?? '')) return 'Volunteers fold only into a legion of their own country.';
  if (keep.joint || gone.joint) return 'A corps marching with an ally cannot be folded.';
  // (The far side: Vichy's corps at their posts in Indochina are folded into nobody, nor anybody into them: pacific.ts atItsPost.)
  if (atItsPost(keep) || atItsPost(gone)) return postWords(atItsPost(keep) ? keep : gone, 'post') ?? 'It takes no orders.';
  if (keep.leave !== undefined || gone.leave !== undefined) return 'A corps at home cannot be folded.';
  if (!keep.supplied || !gone.supplied) return 'Cut off, the cadre has no way home.';
  // (A corps its row keeps small is read against its own establishment, `full`: a regiment at a quarter of a division's
  // strength is whole, not worn. Rider b594 to ruling 234. No corps has an establishment with the far side off.)
  for (const c of [keep, gone]) if (c.full !== undefined && c.str >= c.full) return `${c.name} is already at full strength for its size.`;
  const worn = MANPOWER.fold.worn;
  if (keep.str >= worn * (keep.full ?? 1) || gone.str >= worn * (gone.full ?? 1)) return `Both corps must be under ${Math.round(worn * 100)}% of their strength.`;
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
  /** The same two in divisions' worth: what fills the kept corps, and what goes back to the reserve (for foldWords). */
  fill: number; over: number;
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
  // (The kept corps is filled no fuller than its own establishment: rider b594 to ruling 234.)
  const fill = Math.min(keep.divs * (keep.full ?? 1), mine + theirs), from = Math.max(0, fill - mine);
  const mix = (a: number, b: number): number => (fill > 0 ? (a * mine + b * from) / fill : a);
  const { lead, out } = command(g, keep, gone);
  const t = cadreTerms(g, gone.nat, gone);
  const v: FoldView = {
    keep: keep.id, gone: gone.id, keepName: keep.name, goneName: gone.name, why: whyNotFold(g, keep, gone),
    str: fill / keep.divs, men: Math.round(fill * MEN_PER_DIV), xp: mix(keep.xp, gone.xp),
    org: clamp(mix(keep.org, gone.org) - MANPOWER.fold.org, 0, 1), ap: Math.min(keep.ap, gone.ap),
    back: Math.round((mine + theirs - fill) * MEN_PER_DIV), fill, over: Math.max(0, mine + theirs - fill),
    general: lead?.name ?? null, stepsDown: out?.name ?? null,
    cadreCost: t.cost, cadreMonths: t.months, cadreMen: t.divs * MEN_PER_DIV,
  };
  if (worksOf(gone) > worksOf(keep)) v.works = worksOf(gone);
  return v;
}

/** The fold's men in words, for each of its speakers: the news (foldCorps), the page's card and the 3D game's card. */
export interface FoldWords {
  /** The men in one corps: "30,000 men", or at a nation's own scale "about 134,000 men". */
  men: string;
  /** The news' clause for the men sent back, or null where none go back: "6,000 sent back to the reserve", and for a
   * nation with a store "about 27,000 sent back to the reserve, and their rifles to the store". */
  back: string | null;
  /** The cards' line for the men sent back, or null: "6,000 men over go back to the reserve.", and for a nation with a
   * store "About 27,000 men over go back to the reserve, and their rifles to the store." */
  over: string | null;
  /** The news' clause for the rifles the fold gave the store where no men are said sent back, or null: "and 2,300
   * spare rifles to the store" (set only where the news passes what the fold gave, foldCorps). */
  spare?: string | null;
}

/**
 * The fold's men in words (the UI director's read of 26 November 2026, point 4): a nation's men at its own scale with
 * "about", to the thousand, as the reserve line beside them counts them; for a nation with a store, the rifles as a
 * clause on the men sent back ("..., about 27,000 sent back to the reserve, and their rifles to the store."). Every
 * other nation as built, byte for byte. Where no men go back (or, at a nation's own scale, fewer than rounds to a
 * thousand), nothing is said of men or rifles going back, even where the moved men's rifles pass the kept corps' full
 * and go to the store: the clause belongs to the men sent back.
 */
export function foldWords(g: Game, nat: string, v: FoldView, gave?: number): FoldWords {
  const per = menScale(nat), rifles = binds(g, nat) ? ', and their rifles to the store' : '';
  const men = per === null ? `${v.men.toLocaleString('en-US')} men` : `about ${men1000(v.fill * per)} men`;
  const n = per === null ? (v.back > 0 ? v.back.toLocaleString('en-US') : null) : Math.round((v.over * per) / 1000) > 0 ? `about ${men1000(v.over * per)}` : null;
  // The news (gave set, foldCorps: what passed the kept corps' full and the emptied corps' worn part, in divisions'
  // worth) says the rifles the fold gave the store, at the store line's own measure, where the nation keeps a store:
  // the UI director's sixth read of 26 November 2026, 18:55Z, point 3. With no men said sent back, all it gave (the
  // leftover men's too, where they round to no thousand): "..., and 2,300 spare rifles to the store." With men sent
  // back, what came besides theirs: "..., and their rifles and 1,200 more to the store." Where nothing came besides,
  // and wherever no store is kept, as built.
  if (n === null) {
    const spare = gave === undefined ? null : storeRifles(g, nat, gave + v.over);
    return { men, back: null, over: null, ...(gave === undefined ? {} : { spare: spare === null ? null : `and ${spare} spare rifle${spare === 'one' ? '' : 's'} to the store` }) };
  }
  const up = n[0].toUpperCase() + n.slice(1);
  const more = gave === undefined || !rifles ? null : storeRifles(g, nat, gave);
  return { men, back: `${n} sent back to the reserve${more === null ? rifles : `, and their rifles and ${more} more to the store`}`, over: `${up} men over go back to the reserve${rifles}.` };
}

/** The fold card's own figures, for both cards (the page's foldCard and the 3D game's foldPage): see foldCardWords. */
export interface FoldCardWords {
  /** A corps' men, for the heading of each pair: "18,000 men", or at a nation's own scale "about 80,000 men". */
  men: (c: Corps) => string;
  /** What raising the cadre again takes, after "for", with the treasury's figure in the speaker's own dress: "0 from
   * the treasury and 30,000 men from the reserve", or at a nation's own scale "two divisions' worth of men from the
   * reserve" (with "1,700 from the treasury and " before it where the treasury's figure is not nothing). */
  cadre: (v: FoldView, cost: string) => string;
  /** A strength to show as "60% of its strength", for the heading of each pair and the kept corps' line: the corps'
   * strength as it stands, or at a nation's own scale against the corps' own size, so a corps its row keeps small reads
   * whole at its full, as whyNotFold reads it ("already at full strength for its size"). */
  share: (str: number, c: Corps | undefined) => number;
  /** The cadre's months, after "in": the speaker's own dress ('own', "5 months"), or at a nation's own scale in words
   * to ten ("five months", "one month", "11 months"). */
  months: (k: number, own: string) => string;
}

/**
 * The fold card at one scale (the UI director's second read of 26 November 2026, 03:16Z, point 1). For a nation with a
 * scale of its own, the heading of each pair says each corps' men with "about", to the thousand, the figure foldWords
 * gives for that corps ("Central reserve: about 80,000 men, 60% of its strength."), and the cadre line says the men in
 * divisions' worth as line 8 says them, the treasury clause dropped where it is nothing ("... can be raised again from
 * the arsenal for two divisions' worth of men from the reserve"). Her third read (04:58Z, point A) keeps the months
 * clause, in words to ten ("..., in five months."). Every other nation as built, byte for byte, each card's months in
 * its own dress ("for 0 from the treasury" and ", in 5 months." there are shipped lines: her later-patch list).
 */
export function foldCardWords(nat: string): FoldCardWords {
  const per = menScale(nat);
  if (per === null) return { men: (c) => `${men(c).toLocaleString('en-US')} men`, cadre: (v, cost) => `${cost} from the treasury and ${Math.round(v.cadreMen).toLocaleString('en-US')} men from the reserve`, share: (str) => str, months: (_k, own) => own };
  return {
    men: (c) => `about ${men1000(c.divs * c.str * per)} men`,
    share: (str, c) => str / (c?.full ?? 1),
    months: (k) => `${inWords(k)} month${k === 1 ? '' : 's'}`,
    cadre: (v, cost) => {
      // (A cadre of a corps its row keeps small can come back as part of a division's worth: said in men at the scale.)
      return `${v.cadreCost > 0 ? `${cost} from the treasury and ` : ''}${menWorth(per, v.cadreMen / MEN_PER_DIV)} from the reserve`;
    },
  };
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
  const gave = riflesFold(g, keep, gone, from);
  if (gone.fought) keep.fought = true;
  carryBattle(g, gone.id, keep.id);
  if (gone.pressed) keep.pressed = true;
  if ((gone.blows ?? 0) > (keep.blows ?? 0)) keep.blows = gone.blows;
  returnMen(g, keep.nat, gone.str * gone.divs - from);
  riflesBack(g, keep.nat, gone.str * gone.divs - from);
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
  // (A corps a dated row had the loan of is a cadre under its own name, and its loan is over: nothing of it goes
  // back anywhere, and raised again it is its own corps. A division is laid once, drawn.ts.)
  // (And no row's time, when it is up, finds the corps raised again from that cadre: it would be sent off whole.)
  if (gone.drew || gone.rowed) for (const k of Object.keys(g.flags)) if (k.startsWith('lent:') && g.flags[k] === gone.id) g.flags[k] = -1;
  const lentAs = gone.rowed ? gone.name : null;
  if (gone.rowed) { gone.name = gone.rowed.name; gone.hold = gone.rowed.hold; if (gone.rowed.stay) gone.stay = true; else delete gone.stay; }
  delete gone.drew; delete gone.rowed;
  (n.cadres ??= []).push(gone);
  // (The men in words from foldWords, which the page's card and the 3D game's card speak too; the noun follows the kept
  // unit's kind, "brigade" for a support team: the UI director's sixth read, point 2.)
  const w = foldWords(g, keep.nat, v, gave);
  if (keep.nat === g.human) headline(g, 'info', `${lentAs ?? gone.name} is folded into ${keep.name} at ${g.regions[reg].name}: ${w.men} in one ${orderNoun(keep.kind)}${w.back ? `, ${w.back}` : w.spare ? `, ${w.spare}` : ''}. Its cadre has gone home and can be raised again from the arsenal.`, keep.nat, reg);
  return true;
}
