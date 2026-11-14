// Generals: the people who command the corps. They gain experience in battle, spend it on
// four skills, earn traits from how they actually fight, and can be wounded, captured,
// killed or sacked. The real commanders of 1939 start the war; unknown officers rise
// through it. Everything they do to a battle shows up on the Battle Ticket.

import data from '../../data/campaign/commanders.json';
import { Rng, hashString } from '../core/rng';
import { atWar, frozen, headline } from './game';
import { KIND, type BattleReport, type Corps, type Game, type General, type Region, type ReportUnit, type ServiceRecord, type Skill } from './types';

export interface TraitDef {
  label: string;
  good: boolean;
  effect: string;
  earned: string;
  attack?: number;
  defence?: number;
  ap?: number;
  loss?: number;
  pocket?: number;
  winter?: number;
  xp?: number;
  needs?: [keyof ServiceRecord, number];
  not?: string[];
  mobile?: boolean;
}

type SkillSet = Partial<Record<Skill, number>>;

/** The rules for generals. See data/campaign/commanders.json. */
export const COMMAND = data as unknown as {
  maxLevel: number; skillMax: number; levelXp: number[];
  xp: { fought: number; won: number; decisive: number; odds: number; held: number; capital: number };
  closeOdds: number; costly: number;
  skills: Record<Skill, { label: string; per?: number; cutOff?: number; steady?: number; rally?: number; blurb: string }>;
  traits: Record<string, TraitDef>;
  hazard: { base: number; perLoss: number; killedShare: number; woundMonths: [number, number]; lost: { captured: number; killed: number } };
  rise: { chance: number; share: number };
  sack: { org: number; pc: number; popularLevel: number; popularWins: number };
  promote: { pc: number };
  ranks: Record<string, [number, string][]>;
  legacy: Record<string, { level: number; skills: SkillSet; traits: string[] }>;
  roster: Record<string, [number, string, number, SkillSet, string[]][]>;
  names: Record<string, { first: string[]; last: string[] }>;
};

export const SKILLS: Skill[] = ['offense', 'defense', 'logistics', 'leadership'];

const blank = (): ServiceRecord => ({ battles: 0, won: 0, attacks: 0, defences: 0, wounds: 0, fast: 0, close: 0, enc: 0, held: 0, winter: 0, costly: 0, streak: 0 });

export function generalOf(g: Game, c: Corps): General | null {
  return c.general === null ? null : g.generals[c.general] ?? null;
}

export function corpsOf(g: Game, gen: General): Corps | null {
  return gen.corps < 0 ? null : g.corps.find((c) => c.id === gen.corps) ?? null;
}

/** Serving generals: fit for a command or recovering from wounds. */
export const serving = (gen: General): boolean => gen.status === 'active' || gen.status === 'wounded';

export function newGeneral(g: Game, nat: string, name: string, o: { level?: number; skills?: SkillSet; traits?: string[]; historical?: boolean }): General {
  const gen: General = {
    id: g.generals.length, nat, name, historical: o.historical ?? true, level: o.level ?? 1, xp: 0, points: 0,
    skills: { offense: 0, defense: 0, logistics: 0, leadership: 0, ...o.skills }, traits: [...(o.traits ?? [])],
    corps: -1, status: 'active', backOn: 0, joined: g.turn, moved: 0, rec: blank(),
  };
  gen.points = Math.max(0, gen.level - 1 - SKILLS.reduce((s, k) => s + gen.skills[k], 0));
  g.generals.push(gen);
  return gen;
}

/** Puts a general at the head of a corps, or takes him off his corps (null). Whoever held the post steps down. */
export function appoint(g: Game, gen: General, c: Corps | null): void {
  const old = corpsOf(g, gen);
  if (old && old.general === gen.id) old.general = null;
  gen.corps = -1;
  if (!c) return;
  const sitting = generalOf(g, c);
  if (sitting && sitting !== gen) sitting.corps = -1;
  c.general = gen.id;
  gen.corps = c.id;
}

/** Seats the named commander a starting corps comes with. The label is the one used in setup-1939.json. */
export function seat(g: Game, c: Corps, name: string, label?: string | null): General {
  // A man already on the list is not invented twice: he takes the new command, unless a player has him busy elsewhere.
  const known = g.generals.find((x) => x.nat === c.nat && x.name === name);
  if (known) {
    if (serving(known) && known.status === 'active' && (c.nat !== g.human || known.corps < 0)) appoint(g, known, c);
    return known;
  }
  const l = COMMAND.legacy[label ?? 'none'] ?? COMMAND.legacy.none;
  const gen = newGeneral(g, c.nat, name, { level: l.level, skills: l.skills, traits: l.traits, historical: true });
  appoint(g, gen, c);
  return gen;
}

// ---- What a general does for his corps ------------------------------------------------

/** His traits that apply to the corps he has: a panzer leader does nothing special for infantry. */
function traitsFor(gen: General, c: Corps): TraitDef[] {
  const out: TraitDef[] = [];
  for (const id of gen.traits) {
    const d = COMMAND.traits[id];
    if (d && (!d.mobile || KIND[c.kind].mobile)) out.push(d);
  }
  return out;
}

export function cmdAttack(g: Game, c: Corps, pocket: boolean): number {
  const gen = generalOf(g, c);
  if (!gen) return 1;
  let m = 1 + COMMAND.skills.offense.per! * gen.skills.offense;
  for (const d of traitsFor(gen, c)) {
    if (d.attack) m *= d.attack;
    if (pocket && d.pocket) m *= d.pocket;
  }
  return m;
}

export function cmdDefence(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  if (!gen) return 1;
  let m = 1 + COMMAND.skills.defense.per! * gen.skills.defense;
  for (const d of traitsFor(gen, c)) if (d.defence) m *= d.defence;
  return m;
}

/** Extra weeks of movement a month. */
export function cmdAp(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  if (!gen) return 0;
  let ap = 0;
  for (const d of traitsFor(gen, c)) ap += d.ap ?? 0;
  return ap;
}

/** Multiplier on the corps' own losses. */
export function cmdLoss(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  if (!gen) return 1;
  let m = 1;
  for (const d of traitsFor(gen, c)) m *= d.loss ?? 1;
  return m;
}

/** Share of the usual cohesion loss the corps takes in battle. */
export function cmdSteady(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  return gen ? 1 - COMMAND.skills.leadership.steady! * gen.skills.leadership : 1;
}

/** Multiplier on the cohesion the corps regains each month. */
export function cmdRally(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  return gen ? 1 + COMMAND.skills.leadership.rally! * gen.skills.leadership : 1;
}

/** Share of the winter penalties the corps still feels. */
export function cmdWinter(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  if (!gen) return 1;
  let m = 1;
  for (const d of traitsFor(gen, c)) if (d.winter !== undefined) m = Math.min(m, d.winter);
  return m;
}

/** A supply-line multiplier as this corps' general makes it: a good quartermaster wastes less. */
export function cmdLine(g: Game, c: Corps, mul: number): number {
  const gen = generalOf(g, c);
  if (!gen || mul >= 1 || !gen.skills.logistics) return mul;
  return 1 - (1 - mul) * (1 - COMMAND.skills.logistics.per! * gen.skills.logistics);
}

/** How much of its strength a cut-off corps fights at. */
export function cmdCutOff(g: Game, c: Corps): number {
  const gen = generalOf(g, c);
  return 0.5 + (gen ? COMMAND.skills.logistics.cutOff! * gen.skills.logistics : 0);
}

/** A one-line summary of the named commanders on one side of a battle. */
export function commandersIn(g: Game, cs: Corps[]): string {
  const names: string[] = [];
  for (const c of cs) {
    const gen = generalOf(g, c);
    if (gen) names.push(`${gen.name} (level ${gen.level})`);
  }
  return names.length > 3 ? `${names.slice(0, 3).join(', ')} and ${names.length - 3} more` : names.join(', ');
}

// ---- Experience, skills and traits ------------------------------------------------------

export const xpToNext = (gen: General): number => (gen.level >= COMMAND.maxLevel ? Infinity : COMMAND.levelXp[gen.level] ?? Infinity);

/** Generals the newsreel cares about: the player's own, the real ones, and anyone who has made a name. */
const notable = (g: Game, gen: General): boolean => gen.nat === g.human || gen.historical || gen.level >= 5;

/** What a general is called: his rank goes up with his level. */
export function rankOf(gen: { nat: string; level: number }): string {
  const rows = COMMAND.ranks[gen.nat] ?? COMMAND.ranks._default;
  let out = rows[0][1];
  for (const [from, title] of rows) if (gen.level >= from) out = title;
  return out;
}

export function gainXp(g: Game, gen: General, amount: number): void {
  if (gen.level >= COMMAND.maxLevel) return;
  let mul = 1;
  for (const t of gen.traits) mul *= COMMAND.traits[t]?.xp ?? 1;
  gen.xp += Math.round(amount * mul);
  while (gen.xp >= xpToNext(gen)) {
    gen.xp -= xpToNext(gen);
    const was = rankOf(gen);
    gen.level++;
    gen.points++;
    if (gen.nat === g.human) headline(g, 'info', `${gen.name} reaches level ${gen.level} and has a skill point to spend.`, gen.nat);
    // A general who has grown into the next rank is promoted, and the army hears of it.
    const now = rankOf(gen);
    if (now !== was && notable(g, gen)) headline(g, 'info', `${gen.name} is promoted to ${now}.`, gen.nat);
  }
  if (gen.level >= COMMAND.maxLevel) gen.xp = 0;
  if (gen.nat !== g.human) autoSpend(gen);
}

export function spendPoint(gen: General, skill: Skill): boolean {
  if (gen.points < 1 || gen.skills[skill] >= COMMAND.skillMax || !serving(gen)) return false;
  gen.skills[skill]++;
  gen.points--;
  return true;
}

/** The computer spends a general's points on what he has been doing. */
export function autoSpend(gen: General): void {
  while (gen.points > 0) {
    const w: Record<Skill, number> = {
      offense: 2 + gen.rec.attacks, defense: 2 + gen.rec.defences,
      logistics: 1.5 + 0.3 * gen.rec.battles, leadership: 1.5 + 0.3 * gen.rec.battles,
    };
    let best: Skill | null = null, bs = -1;
    for (const k of SKILLS) {
      if (gen.skills[k] >= COMMAND.skillMax) continue;
      const s = w[k] / (1 + gen.skills[k]);
      if (s > bs) { bs = s; best = k; }
    }
    if (!best) break;
    gen.skills[best]++;
    gen.points--;
  }
}

/** Gives a general the traits his record has earned. */
function earn(g: Game, gen: General, c: Corps): void {
  for (const [id, d] of Object.entries(COMMAND.traits)) {
    if (!d.needs || gen.traits.includes(id)) continue;
    if ((gen.rec[d.needs[0]] as number) < d.needs[1]) continue;
    if (d.not?.some((x) => gen.traits.includes(x))) continue;
    if (d.mobile && !KIND[c.kind].mobile) continue;
    gen.traits.push(id);
    if (notable(g, gen)) headline(g, 'info', `${gen.name} has earned a reputation: ${d.label}. ${d.earned}`, gen.nat);
  }
}

// ---- What a battle does to the generals in it ---------------------------------------------

function inventName(g: Game, nat: string, rng: Rng): string | null {
  const pool = COMMAND.names[nat];
  if (!pool) return null;
  for (let tries = 0; tries < 12; tries++) {
    const name = `${rng.pick(pool.first)} ${rng.pick(pool.last)}`;
    if (!g.generals.some((x) => x.nat === nat && x.name === name)) return name;
  }
  return null;
}

/** True when the nation's list of generals has room for a new man. */
function room(g: Game, nat: string): boolean {
  let have = 0, corps = 0;
  for (const x of g.generals) if (x.nat === nat && serving(x)) have++;
  for (const c of g.corps) if (c.nat === nat && c.kind !== 'fort') corps++;
  return have < Math.ceil(COMMAND.rise.share * corps);
}

/** A corps with no general does something remarkable, and one of its officers makes his name. */
function rise(g: Game, c: Corps, rng: Rng, place: string): void {
  if (c.kind === 'fort' || c.general !== null || !COMMAND.names[c.nat]) return;
  if (!rng.chance(COMMAND.rise.chance) || !room(g, c.nat)) return;
  const name = inventName(g, c.nat, rng);
  if (!name) return;
  const gen = newGeneral(g, c.nat, name, { historical: false, traits: ['rising_star'] });
  appoint(g, gen, c);
  gen.moved = g.turn;
  if (c.nat === g.human) headline(g, 'info', `${name} makes his name at ${place} and is given command of ${c.name}.`, c.nat, c.reg);
}

function lose(g: Game, gen: General, status: 'captured' | 'dead', text: string, region: number): void {
  appoint(g, gen, null);
  gen.status = status;
  if (notable(g, gen)) headline(g, 'battle', text, gen.nat, region);
}

/** The risks of the job: a general can be hit in any battle, and goes into the bag with his corps. */
function hazard(g: Game, gen: General, c: Corps, lossFrac: number, rng: Rng, rep: BattleReport): void {
  const H = COMMAND.hazard;
  const roll = rng.next();
  if (c.dead) {
    if (roll < H.lost.captured) lose(g, gen, 'captured', `${gen.name} is taken prisoner with ${c.name} at ${rep.regionName}.`, rep.region);
    else if (roll < H.lost.captured + H.lost.killed) lose(g, gen, 'dead', `${gen.name} is killed as ${c.name} is destroyed at ${rep.regionName}.`, rep.region);
    else {
      gen.corps = -1;
      if (gen.nat === g.human) headline(g, 'info', `${gen.name} gets away from the disaster at ${rep.regionName} and is waiting for a new command.`, gen.nat, rep.region);
    }
    return;
  }
  const p = H.base + H.perLoss * lossFrac;
  if (roll < p * H.killedShare) lose(g, gen, 'dead', `${gen.name} is killed in action at ${rep.regionName}.`, rep.region);
  else if (roll < p) {
    const months = H.woundMonths[0] + rng.int(H.woundMonths[1] - H.woundMonths[0] + 1);
    appoint(g, gen, null);
    gen.status = 'wounded';
    gen.backOn = g.turn + months;
    gen.rec.wounds++;
    if (notable(g, gen)) headline(g, 'battle', `${gen.name} is wounded at ${rep.regionName} and will be out for ${months} months.`, gen.nat, rep.region);
  }
}

/**
 * Flows a battle's result back to the generals who fought it: experience, the record their
 * traits are earned from, wounds and capture, and new names made. The rolls come from their
 * own stream, tied to the battle, so they never disturb the dice of the war itself.
 */
export function afterBattle(
  g: Game, rep: BattleReport, target: Region, preOdds: number,
  attackers: Corps[], attGens: (General | null)[], defenders: Corps[], defGens: (General | null)[], luck: string = `${rep.id}`, even = 1.2,
): void {
  const rng = new Rng(hashString(`command:${g.seed}:${luck}`));
  const X = COMMAND.xp;
  const decisive = rep.level === 'Decisive victory' || rep.level === 'Heroic victory';
  const side = (cs: Corps[], gens: (General | null)[], us: ReportUnit[], attacking: boolean): void => cs.forEach((c, i) => {
    const won = attacking ? rep.taken : !rep.taken;
    const againstOdds = attacking ? preOdds < COMMAND.closeOdds : preOdds >= 1;
    const gen = gens[i];
    if (!gen) {
      if (!c.dead && won && (attacking ? decisive : againstOdds)) rise(g, c, rng, rep.regionName);
      return;
    }
    const lossFrac = us[i].men > 0 ? Math.min(1, us[i].lost / us[i].men) : 0;
    const r = gen.rec;
    let xp = X.fought;
    r.battles++;
    if (attacking) r.attacks++; else r.defences++;
    if (frozen(g, c.nat, target)) r.winter++;
    if (won) {
      r.won++;
      r.streak = 0;
      if (r.lost) delete r.lost;
      xp += attacking ? X.won : X.held;
      if (againstOdds) xp += X.odds;
      if (attacking) {
        if (decisive) xp += X.decisive;
        if (decisive && KIND[c.kind].mobile) r.fast++;
        if (againstOdds) r.close++;
        if (rep.prisoners > 0) r.enc++;
        if (target.cap && target.owner === rep.defNat) xp += X.capital;
      } else if (againstOdds) r.held++;
    } else {
      r.streak++;
      if (attacking && lossFrac >= COMMAND.costly) r.costly++;
      // The grand campaign remembers which battles they were and at what odds: a cabinet looking for a head weighs them.
      if (g.flags.open === 1) { (r.lost ??= []).push({ t: g.turn, reg: target.id, odds: Math.round(preOdds * 100) / 100, att: attacking, even: Math.round(even * 100) / 100 }); if (r.lost.length > 6) r.lost.shift(); }
    }
    gainXp(g, gen, xp);
    earn(g, gen, c);
    hazard(g, gen, c, lossFrac, rng, rep);
  });
  side(attackers, attGens, rep.attackers, true);
  side(defenders, defGens, rep.defenders, false);
}

// ---- The monthly round ------------------------------------------------------------------

/** New arrivals on the list, the wounded returning to duty, and the computer handing out commands. */
export function monthlyCommand(g: Game, nat: string): void {
  for (const [from, name, level, skills, traits] of COMMAND.roster[nat] ?? []) {
    if (from > g.turn || g.generals.some((x) => x.nat === nat && x.name === name)) continue;
    newGeneral(g, nat, name, { level, skills, traits, historical: true });
    if (nat === g.human && g.turn > 1) headline(g, 'info', `${name} reports for duty and is waiting for a command.`, nat);
  }
  for (const gen of g.generals) {
    if (gen.nat !== nat || gen.status !== 'wounded' || gen.backOn > g.turn) continue;
    gen.status = 'active';
    if (nat === g.human) headline(g, 'info', `${gen.name} is fit for duty again and is waiting for a command.`, nat);
  }
  if (nat !== g.human) aiAssign(g, nat);
}

/** Generals of a nation who are fit and have no corps. */
export function waiting(g: Game, nat: string): General[] {
  return g.generals.filter((x) => x.nat === nat && x.status === 'active' && x.corps < 0).sort((a, b) => b.level - a.level || a.id - b.id);
}

/** How well a corps suits a general who is looking for a command: a tank man to the tanks, the rest to the big corps in the line. */
export function fit(g: Game, gen: General, c: Corps): number {
  // A tank man is one with the trait for it, or one who has already won at the head of fast troops.
  const fastMan = gen.rec.fast > 0 || gen.traits.some((t) => COMMAND.traits[t]?.mobile);
  const fast = KIND[c.kind].mobile;
  const front = g.regions[c.reg].nb.some((j) => atWar(g, c.nat, g.regions[j].ctrl));
  return (fastMan ? (fast ? (c.kind === 'arm' ? 8 : 6) : 0) : fast ? 1 : 2)
    + (front ? 2 : 0) - (c.hold ? 3 : 0) + c.divs * 0.3 + c.xp;
}

/** Corps of a nation that have no general and could take one. */
export function vacant(g: Game, nat: string): Corps[] {
  return g.corps.filter((c) => c.nat === nat && c.general === null && c.kind !== 'fort');
}

/** The computer gives every waiting general the best corps that has none. */
export function aiAssign(g: Game, nat: string): void {
  const free = waiting(g, nat);
  if (!free.length) return;
  const open = vacant(g, nat);
  for (const gen of free) {
    let best: Corps | null = null, bs = -1e9;
    for (const c of open) {
      if (c.general !== null) continue;
      const s = fit(g, gen, c);
      if (s > bs) { bs = s; best = c; }
    }
    if (!best) break;
    appoint(g, gen, best);
    gen.moved = g.turn;
  }
}

// ---- The player's decisions --------------------------------------------------------------

/** Why a general cannot take this corps, or '' when he can. */
export function whyNotAssign(g: Game, gen: General, c: Corps): string {
  if (gen.status !== 'active') return gen.status === 'wounded' ? 'He is recovering from his wounds.' : 'He is no longer serving.';
  if (gen.nat !== c.nat) return 'He serves another nation.';
  if (c.dead || c.kind === 'fort') return 'This formation takes no general.';
  if (gen.corps === c.id) return 'He already commands this corps.';
  if (gen.moved === g.turn) return 'He took up a new command this month already.';
  return '';
}

/** Gives a general a corps. Whoever commanded it before goes back to the list. */
export function assignGeneral(g: Game, gen: General, c: Corps): boolean {
  if (whyNotAssign(g, gen, c)) return false;
  appoint(g, gen, c);
  gen.moved = g.turn;
  return true;
}

/** Takes a general off his corps and puts him back on the list, with no hard feelings. */
export function relieve(g: Game, gen: General): boolean {
  if (gen.status !== 'active' || gen.corps < 0) return false;
  appoint(g, gen, null);
  return true;
}

export const popular = (gen: General): boolean => gen.level >= COMMAND.sack.popularLevel || gen.rec.won >= COMMAND.sack.popularWins;

/** Dismisses a general for good. If the army thinks well of him, it costs cohesion and political capital. */
export function sack(g: Game, gen: General): boolean {
  if (!serving(gen)) return false;
  const loved = popular(gen);
  const c = corpsOf(g, gen);
  if (loved) {
    if (c) c.org = Math.max(0, c.org - COMMAND.sack.org);
    const n = g.nations[gen.nat];
    n.pc = Math.max(0, n.pc - COMMAND.sack.pc);
  }
  appoint(g, gen, null);
  gen.status = 'sacked';
  headline(g, 'info', loved ? `${gen.name} is dismissed. He was popular with his men, and the army takes it badly.` : `${gen.name} is dismissed.`, gen.nat);
  return true;
}

/** Why an officer cannot be promoted to command this corps, or '' when one can. */
export function whyNotPromote(g: Game, c: Corps): string {
  if (c.general !== null) return 'The corps already has a general.';
  if (c.dead || c.kind === 'fort') return 'This formation takes no general.';
  if (!COMMAND.names[c.nat]) return 'There is no officer ready to step up.';
  if (g.nations[c.nat].pc < COMMAND.promote.pc) return `It takes ${COMMAND.promote.pc} political capital to promote a man over his seniors.`;
  return '';
}

/** Promotes an unknown officer from within the corps over the heads of his seniors. */
export function promote(g: Game, c: Corps): General | null {
  if (whyNotPromote(g, c)) return null;
  const rng = new Rng(hashString(`promote:${g.seed}:${g.turn}:${c.id}:${g.generals.length}`));
  const name = inventName(g, c.nat, rng);
  if (!name) return null;
  g.nations[c.nat].pc -= COMMAND.promote.pc;
  const gen = newGeneral(g, c.nat, name, { historical: false, traits: ['rising_star'] });
  appoint(g, gen, c);
  gen.moved = g.turn;
  return gen;
}
