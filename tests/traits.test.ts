// National abilities and special units (the grand campaign). Every country fights, builds and earns in its own way:
// one row per nation in data/campaign/traits.json, read by every version of the game. The abilities count in
// battles, in the books, in what things cost and in how fast losses are made good; a special formation is raised
// from the arsenal like a corps of its kind, a few at a time. The historical scenarios know none of it.
import { describe, expect, it } from 'vitest';
import grandData from '../data/campaign/grand.json';
import { offers, orderOffer } from '../src/campaign/arsenal';
import { warForecast, wouldSurprise } from '../src/campaign/diplomacy';
import { BUDGET, cancelBuild, corpsCost, deliver, fortCost, income, ledger, orderCorps } from '../src/campaign/economy';
import { addCorps, assess, createGame, declareWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  TRAITS, alertNation, costMul, eliteCount, eliteIn, eliteOf, eliteRule, openTraits, priceOf, traitAttack, traitDefence, traitDrafts,
  traitFleet, traitMoney, traitNames, traitRule, traitSeaLoss, traitSealift, traitsMonth, traitsOf, whyNoElite,
} from '../src/campaign/traits';
import { TECH } from '../src/campaign/tech';
import { runTurnAI } from '../src/campaign/turn';
import { KIND, type Corps, type Game } from '../src/campaign/types';

// These tests measure the abilities alone: what the technology tree adds on top of them is tested in tech.test.ts.
for (const t of TECH.techs) for (const k of ['attack', 'defence', 'drafts', 'cost', 'fleet', 'seaLoss', 'sealift', 'field']) delete (t as any)[k];

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const history = (human: string | null = null, seed = 3): Game => createGame(seed, human);
const corps = (g: Game, nat: string, kind: string): Corps => g.corps.find((c) => c.nat === nat && c.kind === kind && !c.dead)!;
/** January 1940, July 1940 and January 1942, counting September 1939 as month 1. */
const JAN40 = 5, JUL40 = 11, JAN42 = 29;
const KEYS = /^(corps:(inf|mtn|mot|arm|cav|art|eng)|work:(fort|aa)|ship:(dd|ss|ca|bb|cv|tp)|air:(ftr|bmr))$/;

describe('the data', () => {
  it('gives every country a player can lead a row of its own, and the United States too', () => {
    for (const n of (grandData as any).nations) {
      expect(traitsOf(n.id).length, n.id).toBeGreaterThan(0);
      expect(traitsOf(n.id).length + (eliteOf(n.id) ? 1 : 0), n.id).toBeGreaterThanOrEqual(2);
    }
    expect(traitsOf('USA').map((t) => t.id)).toEqual(['arsenal', 'bombers']);
    for (const id of ['GER', 'UK', 'FRA', 'SOV', 'ITA', 'USA', 'POL']) expect(eliteOf(id), id).not.toBeNull();
  });

  it('is well formed: names, sane numbers, orders that exist, kinds that exist', () => {
    const g = history();
    for (const [nat, row] of Object.entries(TRAITS.nations)) {
      expect(g.nations[nat], nat).toBeDefined();
      expect(new Set(row.traits.map((t) => t.id)).size).toBe(row.traits.length);
      for (const t of row.traits) {
        expect(t.name.length, `${nat}.${t.id}`).toBeGreaterThan(3);
        expect(t.text.length, `${nat}.${t.id}`).toBeGreaterThan(20);
        for (const m of [t.attack, t.defence, t.fleet, t.drafts, t.incomeMul]) if (m !== undefined) { expect(m).toBeGreaterThan(1); expect(m).toBeLessThanOrEqual(1.5); }
        if (t.cost) { expect(t.cost.mul).toBeGreaterThanOrEqual(0.7); expect(t.cost.mul).toBeLessThan(1); for (const k of t.cost.what) expect(k, `${nat}.${t.id}`).toMatch(KEYS); }
        for (const k of t.when?.kinds ?? []) expect(KIND[k], `${nat}.${t.id}`).toBeDefined();
        // A trait either adds a rule or says it is one the game already has: never neither.
        const rules = ['attack', 'defence', 'income', 'incomeMul', 'oil', 'relocate', 'drafts', 'cost', 'sealift', 'alert', 'partisans', 'fleet', 'seaLoss'].filter((k) => (t as any)[k] !== undefined);
        expect(t.builtin ? rules.length === 0 && !!t.rule : rules.length > 0 && !t.rule, `${nat}.${t.id}`).toBe(true);
        expect(traitRule(t).length, `${nat}.${t.id}`).toBeGreaterThan(10);
      }
      const u = row.unit;
      if (!u) continue;
      expect(KIND[u.kind], nat).toBeDefined();
      expect(u.most).toBeGreaterThan(0);
      expect(u.starts ?? 0).toBeLessThanOrEqual(u.most);
      expect(u.attack !== undefined || u.defence !== undefined, nat).toBe(true);
      expect(eliteRule(u)).toMatch(/At most \d at once\.$/);
    }
  });

  it('says in plain words what each rule does, from the rule itself', () => {
    const t = (nat: string, id: string) => traitsOf(nat).find((x) => x.id === id)!;
    expect(traitRule(t('GER', 'panzer'))).toBe('Attack up 15% for tanks and motorised troops in plains and desert.');
    expect(traitRule(t('SOV', 'reserves'))).toBe('Losses are made good 25% faster. Infantry corps cost 10% less.');
    expect(traitRule(t('SUI', 'banks'))).toBe('Earns 100 a month while at peace.');
    expect(traitRule(t('FIN', 'winterwar'))).toBe('Defence up 25% on its own ground from December to March.');
    expect(traitRule(t('UK', 'radar'))).toBe('Fighters over home ground meet every raid at full strength. Air forces without a radar chain fight at 70%.');
    expect(eliteRule(eliteOf('ITA')!)).toBe('2 mountain divisions. Attack and defence up 25% in mountains and hills. At most 3 at once.');
    expect(eliteRule(eliteOf('GER')!)).toBe('2 armoured divisions. Attack up 25%, defence up 30%. Raised from 1942. At most 2 at once.');
    expect(eliteRule(eliteOf('SOV')!)).toBe('3 infantry divisions. Attack and defence up 15%. Raised from 1942. At most 3 at once.');
  });
});

describe('the historical scenarios', () => {
  it('know nothing of any of it', () => {
    const g = history('GER');
    const tank = corps(g, 'GER', 'arm');
    expect(traitAttack(g, tank, g.byName['Lodz'])).toBe(1);
    expect(traitDefence(g, corps(g, 'POL', 'inf'), g.byName['Warsaw'])).toBe(1);
    expect(traitMoney(g, 'SUI').flat).toBe(0);
    expect(traitDrafts(g, 'SOV')).toBe(1);
    expect(costMul(g, 'SOV', 'corps:inf')).toBe(1);
    expect(corpsCost(g, 'SOV', 'inf')).toBe(48);
    expect(traitFleet(g, 'UK')).toBe(1);
    expect(traitSeaLoss(g, 'UK')).toBe(1);
    expect(alertNation(g, 'POL')).toBe(false);
    expect(whyNoElite(g, 'ITA')).not.toBeNull();
    expect(offers(g, 'GER', g.nations.GER.capital).some((o) => o.key === 'elite')).toBe(false);
    expect(g.corps.some((c) => c.elite)).toBe(false);
    expect(g.nations.NOR.sealift).toBe(0);
    openTraits(g);
    expect(g.nations.NOR.sealift).toBe(0);
    expect(assess(g, [tank], g.byName['Lodz']).lines.some((l) => l.label === 'Abilities and technology')).toBe(false);
  });
});

describe('in battle', () => {
  it('counts a nation\'s ability only where its conditions hold', () => {
    const g = grand('GER');
    const tank = corps(g, 'GER', 'arm'), foot = corps(g, 'GER', 'inf');
    expect(g.byName['Lodz'].terrain).toBe('plains');
    expect(traitAttack(g, tank, g.byName['Lodz'])).toBeCloseTo(1.15);
    expect(traitAttack(g, corps(g, 'GER', 'mot'), g.byName['Lodz'])).toBeCloseTo(1.15);
    expect(traitAttack(g, foot, g.byName['Lodz'])).toBe(1);
    expect(traitAttack(g, tank, g.byName['Polish Silesia'])).toBe(1); // a city is no country for it
    expect(traitDefence(g, tank, g.byName['Lodz'])).toBe(1);
  });

  it('names the abilities that count on the battle ticket', () => {
    const g = grand('GER');
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && g.regions[c.reg].nb.includes(g.byName['Lodz'].id))!;
    const a = assess(g, [tank], g.byName['Lodz']);
    const line = a.lines.find((l) => l.label === 'Abilities and technology')!;
    expect(line.value).toBe('Attacking: Armoured spearheads. Defending: The cipher bureau');
    expect(line.effect).toBe('Attack up as much as 15% for the corps it covers, defence up as much as 5% for the corps it covers');
    expect(traitNames(g, [tank], g.byName['Lodz'], 'attack')).toEqual(['Armoured spearheads']);
    // The same attack in a historical scenario has no such line.
    const h = history('GER');
    const same = h.corps.find((c) => c.id === tank.id)!;
    const b = assess(h, [same], h.byName['Lodz']);
    expect(b.lines.some((l) => l.label === 'Abilities and technology')).toBe(false);
  });

  it('knows the season and the year', () => {
    const g = grand('FIN');
    const finn = corps(g, 'FIN', 'inf');
    const home = g.byName['Karelia'], abroad = g.byName['Leningrad'];
    g.turn = JAN40;
    expect(traitDefence(g, finn, home)).toBeCloseTo(1.25);
    expect(traitDefence(g, finn, abroad)).toBe(1);
    expect(traitAttack(g, finn, home)).toBeCloseTo(1.15); // forest fighters, in any season
    g.turn = JUL40;
    expect(traitDefence(g, finn, home)).toBe(1);
    // France's heavy tanks are proof against the guns of 1940 and 1941, and no later.
    const char = corps(g, 'FRA', 'arm');
    expect(traitDefence(g, char, g.byName['Champagne'])).toBeCloseTo(1.15);
    g.turn = JAN42;
    expect(traitDefence(g, char, g.byName['Champagne'])).toBe(1);
  });

  it('counts home ground, works and the kind of troops', () => {
    const g = grand('SUI');
    const swiss = corps(g, 'SUI', 'mtn');
    expect(traitDefence(g, swiss, g.byName['Graubunden'])).toBeCloseTo(1.3);
    expect(traitDefence(g, swiss, g.byName['Savoy'])).toBe(1);
    const poilu = corps(g, 'FRA', 'inf');
    expect(traitDefence(g, poilu, g.byName['Lorraine'])).toBeCloseTo(1.1);
    expect(traitDefence(g, poilu, g.byName['Champagne'])).toBe(1);
    const rom = addCorps(g, 'ROM', 'Mountain Corps', 'mtn', 2, 'Transylvania', {});
    expect(traitAttack(g, rom, g.byName['Transylvania'])).toBeCloseTo(1.15);
    expect(traitAttack(g, corps(g, 'ROM', 'inf'), g.byName['Transylvania'])).toBe(1);
  });
});

describe('special formations', () => {
  it('start the campaign where history had raised them', () => {
    const g = grand('ITA');
    const tagged = g.corps.filter((c) => c.elite).map((c) => `${c.nat}:${c.kind}:${c.elite}`).sort();
    expect(tagged).toEqual(['GRE:mtn:evzones', 'ITA:mtn:alpine', 'POL:cav:lancers']);
    const alpini = g.corps.find((c) => c.elite === 'alpine')!;
    expect(eliteIn(alpini)!.label).toBe('Alpine corps');
    expect(eliteIn(corps(g, 'ITA', 'inf'))).toBeNull();
    expect(traitAttack(g, alpini, g.byName['Savoy'])).toBeCloseTo(1.25);
    expect(traitDefence(g, alpini, g.byName['Piedmont'])).toBeCloseTo(1.25);
    expect(traitAttack(g, alpini, g.byName['Veneto'])).toBe(1);
    // An ordinary mountain corps has the ground rules of its kind and nothing more.
    const plain = addCorps(g, 'ITA', 'Mountain Corps', 'mtn', 2, 'Piedmont', {});
    expect(traitAttack(g, plain, g.byName['Savoy'])).toBe(1);
  });

  it('are ordered from the arsenal, a few at a time', () => {
    const g = grand('ITA');
    const n = g.nations.ITA;
    n.pp = 200;
    const sel = n.capital;
    const o = offers(g, 'ITA', sel).find((x) => x.key === 'elite')!;
    expect(o).toMatchObject({ group: 'army', label: 'Alpine corps', cost: 36, months: 5, why: null, size: '2 divisions' });
    expect(o.note).toContain('1 of 3 in the field or on order');
    const b = orderOffer(g, 'ITA', 'elite', sel)!;
    expect(b).toMatchObject({ what: 'corps', kind: 'mtn', divs: 2, elite: 'alpine', name: 'I Alpine Corps', cost: 36, eta: g.turn + 5 });
    expect(n.pp).toBe(164);
    expect(orderOffer(g, 'ITA', 'elite', sel)!.name).toBe('II Alpine Corps');
    expect(eliteCount(g, 'ITA')).toBe(3);
    expect(whyNoElite(g, 'ITA')).toBe('The nation can keep 3 of these at once, in the field and on order.');
    expect(orderOffer(g, 'ITA', 'elite', sel)).toBeNull();
    expect(n.pp).toBe(128);
    // An order taken back the same month frees its place and its number.
    const last = n.queue[n.queue.length - 1];
    expect(cancelBuild(g, 'ITA', last.id)).toBe(true);
    expect(n.pp).toBe(164);
    expect(orderOffer(g, 'ITA', 'elite', sel)!.name).toBe('II Alpine Corps');
    // The ordinary corps numbers are untouched by all this.
    const no = n.corpsNo;
    orderCorps(g, 'ITA', 'inf', sel);
    expect(n.corpsNo).toBe(no + 1);
  });

  it('arrive as corps of their kind that carry the mark, and a place opens when one is lost', () => {
    const g = grand('ITA');
    const n = g.nations.ITA;
    n.pp = 200;
    orderOffer(g, 'ITA', 'elite', n.capital);
    g.turn += 5;
    deliver(g, 'ITA');
    const c = g.corps.find((x) => x.name === 'I Alpine Corps')!;
    expect(c).toMatchObject({ nat: 'ITA', kind: 'mtn', divs: 2, elite: 'alpine' });
    expect(eliteCount(g, 'ITA')).toBe(2);
    c.dead = true;
    expect(eliteCount(g, 'ITA')).toBe(1);
    // The mark survives a save.
    const back = loadGame(saveGame(g))!;
    expect(back.corps.filter((x) => x.elite === 'alpine' && !x.dead).length).toBe(1);
  });

  it('wait for their year', () => {
    const g = grand('GER');
    g.nations.GER.pp = 500;
    const sel = g.nations.GER.capital;
    expect(offers(g, 'GER', sel).find((x) => x.key === 'elite')!.why).toBe('Not before 1942.');
    expect(orderOffer(g, 'GER', 'elite', sel)).toBeNull();
    g.turn = JAN42;
    const b = orderOffer(g, 'GER', 'elite', sel)!;
    expect(b).toMatchObject({ kind: 'arm', elite: 'heavy', name: 'I Heavy Tank Corps', cost: 88 });
    g.turn += 8;
    deliver(g, 'GER');
    const tiger = g.corps.find((x) => x.elite === 'heavy')!;
    // Heavy tanks on open ground: the spearheads' edge and their own quarter.
    expect(traitAttack(g, tiger, g.byName['Lodz'])).toBeCloseTo(1.15 * 1.25);
    expect(traitDefence(g, tiger, g.byName['Lodz'])).toBeCloseTo(1.3);
  });

  it('are raised by the computer too', () => {
    const g = grand(null, 5);
    while (g.turn <= 14) runTurnAI(g);
    const raised = new Set([...g.corps.filter((c) => c.elite).map((c) => c.nat), ...Object.values(g.nations).flatMap((n) => n.queue.filter((b) => b.elite).map(() => n.id))]);
    expect(raised.size).toBeGreaterThanOrEqual(3);
    // Nobody is over the number his country can keep.
    for (const id of Object.keys(TRAITS.nations)) if (eliteOf(id)) expect(eliteCount(g, id), id).toBeLessThanOrEqual(eliteOf(id)!.most);
    // And no heavy tanks or Guards before their year.
    expect(g.corps.some((c) => c.elite === 'heavy' || c.elite === 'guards')).toBe(false);
  });
});

describe('in the books', () => {
  it('pays a country at peace what peace is worth to it, and stops when war comes', () => {
    const g = grand('SUI');
    const L = ledger(g, 'SUI');
    expect(L.income.find((l) => l.key === 'trait:banks')).toMatchObject({ label: 'The banking haven', amount: 1 });
    expect(L.gross).toBeCloseTo(income(g, 'SUI'), 9);
    const before = income(g, 'SUI');
    declareWar(g, 'GER', 'SUI');
    expect(ledger(g, 'SUI').income.some((l) => l.key === 'trait:banks')).toBe(false);
    expect(ledger(g, 'SUI').gross).toBeCloseTo(income(g, 'SUI'), 9);
    // At war its industry runs at full speed, but the banks' business is gone.
    expect(traitMoney(g, 'SUI').flat).toBe(0);
    expect(before).toBeGreaterThan(0);
  });

  it('pays a merchant fleet in peace and in war', () => {
    const g = grand('NOR');
    expect(traitMoney(g, 'NOR').lines).toEqual([{ key: 'trait:merchant', label: 'The merchant fleet', amount: 0.8, count: undefined }]);
    declareWar(g, 'GER', 'NOR');
    expect(traitMoney(g, 'NOR').flat).toBeCloseTo(0.8);
  });

  it('doubles what Romania\'s oilfields earn', () => {
    const g = grand('ROM');
    expect(traitMoney(g, 'ROM').lines).toEqual([{ key: 'trait:oil', label: 'The oil of Ploesti', amount: BUDGET.oil, count: 1 }]);
    g.byName['Ploiesti'].ctrl = 'SOV';
    expect(traitMoney(g, 'ROM').flat).toBe(0);
  });

  it('keeps a quarter of the Soviet factories an enemy has overrun at work in the east', () => {
    const g = grand('SOV');
    expect(traitMoney(g, 'SOV').flat).toBe(0);
    declareWar(g, 'GER', 'SOV');
    const kiev = g.byName['Kiev'], minsk = g.byName['Minsk'];
    kiev.ctrl = 'GER'; minsk.ctrl = 'GER';
    const m = traitMoney(g, 'SOV');
    expect(m.lines).toEqual([{ key: 'trait:urals', label: 'Factories to the east', amount: (kiev.fac + minsk.fac) * 0.25, count: kiev.fac + minsk.fac }]);
    expect(ledger(g, 'SOV').gross).toBeCloseTo(income(g, 'SOV'), 9);
    // Ground given up to a country it is not at war with earns nothing.
    g.byName['Odessa'].ctrl = 'ROM';
    expect(traitMoney(g, 'SOV').flat).toBeCloseTo(m.flat);
  });

  it('raises American earnings once the United States is at war', () => {
    const g = grand('UK');
    const peace = income(g, 'USA');
    expect(traitMoney(g, 'USA').mul).toBe(1);
    g.nations.USA.atWar.push('GER'); g.nations.GER.atWar.push('USA');
    const m = traitMoney(g, 'USA');
    expect(m).toMatchObject({ mul: 1.15, mulBy: 'Arsenal of democracy' });
    const L = ledger(g, 'USA');
    expect(L.gross).toBeCloseTo(income(g, 'USA'), 9);
    const line = L.changes.find((l) => l.key === 'trait')!;
    expect(line.label).toBe('Arsenal of democracy');
    expect(line.amount).toBeCloseTo(L.gross * (1 - 1 / 1.15), 9);
    expect(income(g, 'USA')).toBeGreaterThan(peace * 2);
  });

  it('makes good Soviet losses a quarter faster', () => {
    const g = grand('SOV');
    expect(traitDrafts(g, 'SOV')).toBe(1.25);
    expect(traitDrafts(g, 'GER')).toBe(1);
  });
});

describe('what things cost', () => {
  it('takes a nation\'s discount off the list price, to a tenth of a point', () => {
    const g = grand('SOV');
    expect(corpsCost(g, 'SOV', 'inf')).toBe(43.2);
    expect(corpsCost(g, 'SOV', 'arm')).toBe(66);
    expect(corpsCost(g, 'GER', 'inf')).toBe(48);
    expect(corpsCost(g, 'ITA', 'mot')).toBe(40);
    expect(priceOf(g, 'UK', 'ship:dd', 10)).toBe(8.5);
    expect(priceOf(g, 'UK', 'ship:ss', 8)).toBe(8);
    expect(priceOf(g, 'GER', 'ship:ss', 8)).toBe(6);
    expect(priceOf(g, 'USA', 'air:bmr', 18)).toBe(13.5);
    expect(priceOf(g, 'SWE', 'work:aa', 8)).toBe(6);
  });

  it('charges that price in the arsenal and at the till', () => {
    const g = grand('SOV');
    const n = g.nations.SOV;
    n.pp = 100;
    const o = offers(g, 'SOV', n.capital).find((x) => x.key === 'corps:inf')!;
    expect(o.cost).toBe(43.2);
    expect(orderOffer(g, 'SOV', 'corps:inf', n.capital)!.cost).toBe(43.2);
    expect(n.pp).toBeCloseTo(56.8);
    // French engineers build forts for three quarters of what anyone else pays.
    const f = grand('FRA');
    const fr = f.byName['Champagne'], de = f.regions[f.nations.GER.capital];
    expect(fortCost(f, 'FRA', fr)! / fortCost(f, 'GER', de)!).toBeCloseTo(0.75);
    const uk = grand('UK');
    uk.nations.UK.pp = 100;
    const dd = offers(uk, 'UK', uk.byName['Clydeside'].id).find((x) => x.key === 'ship:dd')!;
    expect(dd.cost).toBe(8.5);
    expect(offers(grand('GER'), 'GER', -1).find((x) => x.key === 'ship:dd')!.cost).toBe(10);
  });
});

describe('the rest', () => {
  it('counts the merchant fleets into the shipping, once', () => {
    const g = grand('NOR');
    expect(traitSealift('NOR')).toBe(2);
    expect(g.nations.NOR.sealift).toBe(2);
    expect(g.nations.GRE.sealift).toBe(2);
    expect(g.nations.DEN.sealift).toBe(1);
    expect(g.nations.GER.sealift).toBe(4);
    openTraits(g);
    startOpen(g);
    expect(g.nations.NOR.sealift).toBe(2);
    expect(loadGame(saveGame(g))!.nations.NOR.sealift).toBe(2);
  });

  it('lets nobody catch Poland by surprise', () => {
    const g = grand('SOV');
    expect(alertNation(g, 'POL')).toBe(true);
    expect(wouldSurprise(g, 'SOV', 'POL')).toBe(false);
    expect(wouldSurprise(g, 'SOV', 'ROM')).toBe(true);
    // The forecast says why a blow is no surprise, so both versions of the game can word it the same way.
    expect(warForecast(g, 'SOV', 'POL').ready).toBe('alert');
    expect(warForecast(g, 'SOV', 'ROM').ready).toBeNull();
    expect(warForecast(g, 'ITA', 'FRA').ready).toBe('war');
    const h = history('GER');
    expect(warForecast(h, 'GER', 'DEN')).toMatchObject({ sneak: 1, ready: 'watch' });
    expect(warForecast(h, 'ITA', 'FRA').ready).toBe('war');
  });

  it('gives the Royal Navy its edge and steers its convoys', () => {
    const g = grand('UK');
    expect(traitFleet(g, 'UK')).toBe(1.1);
    expect(traitFleet(g, 'GER')).toBe(1);
    expect(traitSeaLoss(g, 'UK')).toBe(0.8);
    expect(traitSeaLoss(g, 'FRA')).toBe(1);
  });

  it('wears down an occupier in Yugoslavia\'s mountains, hills and forests, and nowhere else', () => {
    const g = grand('GER');
    declareWar(g, 'GER', 'YUG');
    const bosnia = g.byName['Bosnia'], belgrade = g.byName['Belgrade'];
    expect([bosnia.terrain, belgrade.terrain]).toEqual(['mountain', 'plains']);
    bosnia.ctrl = 'GER'; belgrade.ctrl = 'GER';
    const up = addCorps(g, 'GER', 'Garrison A', 'inf', 3, 'Bosnia', {}), down = addCorps(g, 'GER', 'Garrison B', 'inf', 3, 'Belgrade', {});
    const own = g.corps.find((c) => c.nat === 'YUG')!;
    const was = own.str;
    traitsMonth(g);
    expect(up.str).toBeCloseTo(0.96);
    expect(down.str).toBe(1);
    expect(own.str).toBe(was);
    // It goes on after the country has fallen, against the conqueror, down to a floor.
    g.nations.YUG.alive = false;
    g.nations.YUG.fellOn = { turn: g.turn, week: 1, to: 'GER' };
    for (let k = 0; k < 30; k++) traitsMonth(g);
    expect(up.str).toBeCloseTo(0.3);
    expect(down.str).toBe(1);
    // A liberator is left alone.
    bosnia.ctrl = 'UK';
    const tommy = addCorps(g, 'UK', 'Liberators', 'inf', 3, 'Bosnia', {});
    traitsMonth(g);
    expect(tommy.str).toBe(1);
  });
});
