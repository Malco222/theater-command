// Declaring war by marching: in the grand campaign the player can send an army at a neutral's frontier, and the game
// asks whether he means war before anything moves. A blow against a country at peace is a surprise for one month
// (for every nation, the computer's included). When the player strikes a country that belongs to neither bloc, the
// neutral world grows wary of him, the victim's friends most of all, and some stop trading or lean the other way.
// Countries have friends: circles of neighbours (the Nordic countries, the Baltic states...) and single friendships.
// The historical scenarios use none of it.
import { describe, expect, it } from 'vitest';
import { DIPLOMACY, RELATIONS, alarmed, friendsOf, sneakOn, step, tradePartners, warForecast, wouldSurprise } from '../src/campaign/diplomacy';
import { income, ledger } from '../src/campaign/economy';
import { addCorps, assess, atWar, bloc, createGame, declareWar } from '../src/campaign/game';
import { techMul } from '../src/campaign/research';
import { startOpen } from '../src/campaign/staff';
import { declarable, humanUp, invasions, onEve, orderDeclare, passMonth, previewDeclare, reach } from '../src/campaign/turn';
import { KIND, type Corps, type Game, type Kind } from '../src/campaign/types';

const grand = (human: string | null = 'GER', seed = 3): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, at: string, kind: Kind = 'inf', divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at, {});
const army = (g: Game, at: string, nat = 'GER'): Corps[] => g.byName[at].units.filter((c) => c.nat === nat && c.kind !== 'fort');
const ids = (xs: { nat: string }[]): string[] => xs.map((x) => x.nat).sort();
const surprise = (lines: { label: string }[]): boolean => lines.some((l) => l.label === 'Surprise');

describe('friendships', () => {
  it('name countries on the map, with a reason for each', () => {
    const g = grand(null);
    for (const gp of RELATIONS.groups) {
      expect(gp.nations.length).toBeGreaterThanOrEqual(2);
      expect(gp.name.length).toBeGreaterThan(5);
      expect(gp.why.length).toBeGreaterThan(10);
      for (const id of gp.nations) expect(g.nations[id], id).toBeDefined();
    }
    for (const [a, b, why] of RELATIONS.pairs) {
      expect(g.nations[a], a).toBeDefined();
      expect(g.nations[b], b).toBeDefined();
      expect(a).not.toBe(b);
      expect(why.length).toBeGreaterThan(10);
    }
  });

  it('are mutual', () => {
    const g = grand(null);
    for (const a of Object.keys(g.nations)) for (const f of friendsOf(a)) {
      expect(f.nat).not.toBe(a);
      expect(friendsOf(f.nat).map((x) => x.nat), `${f.nat} and ${a}`).toContain(a);
    }
  });

  it('make the Nordic countries a circle, as the Low Countries and the Baltic states are', () => {
    expect(ids(friendsOf('DEN'))).toEqual(['FIN', 'NOR', 'SWE']);
    expect(ids(friendsOf('SWE'))).toEqual(['DEN', 'FIN', 'NOR']);
    expect(ids(friendsOf('EST'))).toEqual(['LAT', 'LIT']);
    expect(ids(friendsOf('LUX'))).toEqual(['BEL', 'NED']);
    expect(friendsOf('NOR')[0].why).toMatch(/Nordic/);
  });

  it('add single friendships to a circle, each friend once', () => {
    // Greece: its partners in the Balkan Pact, and Britain's guarantee.
    expect(ids(friendsOf('GRE'))).toEqual(['ROM', 'TUR', 'UK', 'YUG']);
    expect(friendsOf('GRE').find((f) => f.nat === 'UK')!.why).toMatch(/guarantee/);
    expect(ids(friendsOf('POR'))).toEqual(['ESP', 'UK']);
    expect(ids(friendsOf('BEL'))).toEqual(['FRA', 'LUX', 'NED']);
    for (const a of ['GRE', 'TUR', 'UK', 'HUN']) { const f = friendsOf(a).map((x) => x.nat); expect(new Set(f).size).toBe(f.length); }
    // A country with no entry has no friends, and asking is harmless.
    expect(friendsOf('SUI')).toEqual([]);
    expect(friendsOf('NOWHERE')).toEqual([]);
  });

  it('cost a surprise attack something, and the victim\'s friends more', () => {
    expect(RELATIONS.sneak.attack).toBeGreaterThan(1);
    expect(RELATIONS.sneak.attack).toBeLessThanOrEqual(1.25);
    expect(RELATIONS.wary.all.opinion).toBeGreaterThan(0);
    expect(RELATIONS.wary.friend.opinion).toBeGreaterThan(RELATIONS.wary.all.opinion);
    expect(RELATIONS.wary.friend.home).toBeGreaterThan(RELATIONS.wary.all.home);
  });
});

describe('history is untouched', () => {
  it('knows no surprise and alarms nobody outside the grand campaign', () => {
    const g = createGame(3, 'GER');
    expect(g.flags.open).toBeUndefined();
    expect(wouldSurprise(g, 'GER', 'DEN')).toBe(false);
    expect(alarmed(g, 'GER', 'DEN')).toEqual([]);
    const f = warForecast(g, 'GER', 'DEN');
    expect(f.sneak).toBe(1);
    expect(f.alarm).toEqual([]);
    const before = Object.values(g.nations).map((n) => [n.id, n.opinion, n.home, n.align]);
    const news = g.headlines.length;
    declareWar(g, 'GER', 'DEN');
    expect(atWar(g, 'GER', 'DEN')).toBe(true);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('sneak:'))).toEqual([]);
    expect(sneakOn(g, 'GER', 'DEN')).toBe(false);
    expect(g.headlines.slice(news).some((h) => /alarms the neutral world/.test(h.text))).toBe(false);
    const after = Object.values(g.nations).map((n) => [n.id, n.opinion, n.home, n.align]);
    expect(after.filter((x, i) => x[0] !== 'DEN' && JSON.stringify(x) !== JSON.stringify(before[i]))).toEqual([]);
    expect(f.trade).toBe(false);
    // And the attack the month war is declared carries no surprise line.
    const c = corps(g, 'GER', 'Schleswig-Holstein');
    expect(surprise(assess(g, [c], g.byName['Jutland']).lines)).toBe(false);
  });
});

describe('marching at a neutral frontier', () => {
  it('marks the neutral ground next to the army, with the country that holds it', () => {
    const g = grand();
    expect(humanUp(g)).toBe(true);
    expect(onEve(g)).toBe(false);
    const west = invasions(g, army(g, 'Westphalia'));
    expect(west.get(g.byName['Gelderland'].id)).toBe('NED');
    expect(west.get(g.byName['Friesland'].id)).toBe('NED');
    for (const [id, nat] of west) {
      expect(g.regions[id].ctrl).toBe(nat);
      expect(declarable(g)).toContain(nat);
      expect(atWar(g, 'GER', nat)).toBe(false);
      expect(g.byName['Westphalia'].nb).toContain(id);
    }
    expect(invasions(g, army(g, 'Vienna')).get(g.byName['Budapest'].id)).toBe('HUN');
    expect(invasions(g, army(g, 'Baden')).get(g.byName['Zurich'].id)).toBe('SUI');
    // Several armies picked together show every frontier any of them stands on.
    const both = invasions(g, [...army(g, 'Westphalia'), ...army(g, 'Vienna')]);
    expect(both.get(g.byName['Gelderland'].id)).toBe('NED');
    expect(both.get(g.byName['Budapest'].id)).toBe('HUN');
  });

  it('never marks ground the army can already enter, or ground that is not next to it', () => {
    const g = grand();
    for (const at of ['Westphalia', 'Vienna', 'Rhineland', 'Berlin']) {
      const cs = army(g, at);
      if (!cs.length) continue;
      const marks = invasions(g, cs);
      for (const c of cs) for (const id of reach(g, c).keys()) expect(marks.has(id), `${at} to ${g.regions[id].name}`).toBe(false);
      for (const id of marks.keys()) expect(g.byName[at].nb).toContain(id);
      // No enemy and no ally is ever offered: only countries at peace with the player.
      for (const nat of marks.values()) { expect(atWar(g, 'GER', nat)).toBe(false); expect(bloc(g.nations[nat])).not.toBe('axis'); }
    }
    // Berlin has no neutral neighbour.
    const berlin = corps(g, 'GER', 'Berlin');
    expect(invasions(g, [berlin]).size).toBe(0);
    expect(invasions(g, []).size).toBe(0);
  });

  it('asks nothing of an army that cannot march, of another nation\'s army, or out of turn', () => {
    const g = grand();
    const c = corps(g, 'GER', 'Schleswig-Holstein');
    expect(invasions(g, [c]).get(g.byName['Jutland'].id)).toBe('DEN');
    c.ap = 0;
    expect(invasions(g, [c]).size).toBe(0);
    c.ap = 4;
    c.leave = g.turn + 2; // home on a bond tour
    expect(invasions(g, [c]).size).toBe(0);
    delete c.leave;
    // Tanks are not offered a frontier they could not cross: the high Alps are for the infantry.
    const tanks = corps(g, 'GER', 'Tyrol', 'arm', 2), foot = corps(g, 'GER', 'Tyrol');
    expect(invasions(g, [foot]).get(g.byName['Graubunden'].id)).toBe('SUI');
    expect(invasions(g, [tanks]).has(g.byName['Graubunden'].id)).toBe(false);
    const dane = army(g, 'Jutland', 'DEN');
    expect(dane.length).toBeGreaterThan(0);
    expect(invasions(g, dane).size).toBe(0);
    // Nobody is playing: nothing is marked.
    const idle = grand(null);
    expect(invasions(idle, army(idle, 'Westphalia')).size).toBe(0);
    // Once at war the frontier is an ordinary attack, and the mark is gone.
    expect(orderDeclare(g, 'DEN')).toBe(true);
    expect(invasions(g, [c]).has(g.byName['Jutland'].id)).toBe(false);
    expect(reach(g, c).has(g.byName['Jutland'].id)).toBe(true);
  });
});

describe('the prompt', () => {
  it('tells the player what declaring war on Denmark would mean', () => {
    const g = grand();
    const f = previewDeclare(g, 'DEN')!;
    expect(f.on).toBe('DEN');
    expect(f.joins).toBe('allies');
    expect(f.sneak).toBe(RELATIONS.sneak.attack);
    expect(ids(f.friends)).toEqual(['FIN', 'NOR', 'SWE']);
    // The friends come first on the list.
    expect(ids(f.alarm.filter((a) => a.friend))).toEqual(['FIN', 'NOR', 'SWE']);
    expect(ids(f.alarm.slice(0, 3))).toEqual(['FIN', 'NOR', 'SWE']);
    expect(f.alarm.find((a) => a.nat === 'NOR')!.why).toMatch(/Nordic/);
    expect(f.alarm.some((a) => a.nat === 'DEN' || a.nat === 'GER')).toBe(false);
  });

  it('moves every neutral away from the player, friends furthest, the stubborn least', () => {
    const g = grand();
    const f = previewDeclare(g, 'DEN')!;
    expect(f.alarm.length).toBeGreaterThan(8);
    for (const a of f.alarm) {
      const n = g.nations[a.nat];
      const w = a.friend ? RELATIONS.wary.friend : RELATIONS.wary.all;
      const k = DIPLOMACY.resist[a.nat] ?? 1;
      // Germany is of the Axis: opinion falls toward the Allies.
      expect(a.opinion, a.nat).toBeCloseTo(Math.max(-100, n.opinion - w.opinion * k), 9);
      expect(a.home, a.nat).toBeCloseTo(Math.max(-100, n.home - w.home * k), 9);
      expect(a.from).toBe(n.align);
      expect(n.atWar.length).toBe(0);
    }
    const by = (id: string) => f.alarm.find((a) => a.nat === id)!;
    const fell = (id: string): number => g.nations[id].opinion - by(id).opinion;
    expect(fell('NOR')).toBeCloseTo(25, 9);
    expect(fell('SWE')).toBeCloseTo(25 * 0.7, 9);
    expect(fell('HUN')).toBeCloseTo(8, 9);
    expect(fell('NOR')).toBeGreaterThan(fell('SWE'));
    // Norway begins to lean toward the Allies. Hungary still leans to the Axis and still trades.
    expect(by('NOR').from).toBe('independent');
    expect(by('NOR').to).toBe('leaning_allies');
    expect(by('HUN').to).toBe('leaning_axis');
    expect(by('HUN').trade).toBe(false);
    // Denmark does not trade with Germany. Hungary does, and attacking it would end that.
    expect(f.trade).toBe(false);
    expect(previewDeclare(g, 'HUN')!.trade).toBe(true);
    // Nothing has changed yet.
    expect(atWar(g, 'GER', 'DEN')).toBe(false);
    expect(g.nations['NOR'].align).toBe('independent');
    expect(JSON.stringify(previewDeclare(g, 'DEN'))).toBe(JSON.stringify(f));
  });

  it('is there for every country the player could declare on, and for no other', () => {
    const g = grand();
    for (const on of declarable(g)) {
      const f = previewDeclare(g, on)!;
      expect(f.on).toBe(on);
      expect(f.sneak, on).toBe(RELATIONS.sneak.attack);
    }
    expect(previewDeclare(g, 'ITA')).toBeNull();
    expect(previewDeclare(g, 'POL')).toBeNull(); // already at war
    expect(previewDeclare(g, 'NOWHERE')).toBeNull();
    expect(previewDeclare(grand(null), 'DEN')).toBeNull();
  });

  it('warns when a friendly neutral would stop trading', () => {
    const g = grand();
    expect(tradePartners(g, 'GER').map((n) => n.id)).toContain('HUN');
    g.nations['HUN'].opinion = 25; // a doubtful friend: one more shock and it steps back
    const a = previewDeclare(g, 'DEN')!.alarm.find((x) => x.nat === 'HUN')!;
    expect(a.from).toBe('leaning_axis');
    expect(a.to).toBe('independent');
    expect(a.trade).toBe(true);
    // Those whose place changes are listed ahead of those who only grumble.
    const f = previewDeclare(g, 'DEN')!;
    const order = f.alarm.map((x) => (x.friend ? 0 : x.from !== x.to ? 1 : 2));
    expect(order).toEqual([...order].sort((x, y) => x - y));
  });
});

describe('declaring war', () => {
  it('does what the prompt said it would', () => {
    const g = grand();
    g.nations['HUN'].opinion = 25;
    const f = previewDeclare(g, 'DEN')!;
    const cash = income(g, 'GER');
    const news = g.headlines.length;
    expect(orderDeclare(g, 'DEN')).toBe(true);
    expect(atWar(g, 'GER', 'DEN')).toBe(true);
    expect(bloc(g.nations['DEN'])).toBe(f.joins);
    for (const a of f.alarm) {
      const n = g.nations[a.nat];
      expect(n.opinion, a.nat).toBeCloseTo(a.opinion, 9);
      expect(n.home, a.nat).toBeCloseTo(a.home, 9);
      expect(n.align, a.nat).toBe(a.to);
    }
    expect(g.nations['NOR'].align).toBe('leaning_allies');
    expect(g.nations['HUN'].align).toBe('independent');
    expect(tradePartners(g, 'GER').map((n) => n.id)).not.toContain('HUN');
    // A friendly neutral's trade is worth as much as a factory each month: that much is gone, and the ledger shows it.
    // (Germany's machine tools raise everything it earns by a twentieth, trade included.)
    expect(cash - income(g, 'GER')).toBeCloseTo(DIPLOMACY.tradeIncome * techMul(g, 'GER', 'industry'), 6);
    const line = ledger(g, 'GER').income.find((l) => l.key === 'trade')!;
    expect(line.count).toBe(tradePartners(g, 'GER').length);
    expect(line.amount).toBeCloseTo(line.count! * DIPLOMACY.tradeIncome, 6);
    const told = g.headlines.slice(news).find((h) => /alarms the neutral world/.test(h.text))!;
    expect(told.text).toMatch(/Denmark/);
    for (const name of ['Norway', 'Sweden', 'Finland']) expect(told.text).toContain(name);
    expect(told.nat).toBe('DEN');
  });

  it('catches the defenders unready for that month, and only that month', () => {
    const g = grand();
    const c = corps(g, 'GER', 'Schleswig-Holstein');
    const jutland = g.byName['Jutland'];
    expect(wouldSurprise(g, 'GER', 'DEN')).toBe(true);
    expect(orderDeclare(g, 'DEN')).toBe(true);
    expect(sneakOn(g, 'GER', 'DEN')).toBe(true);
    const hot = assess(g, [c], jutland);
    expect(surprise(hot.lines)).toBe(true);
    expect(hot.lines.find((l) => l.label === 'Surprise')!.effect).toMatch(/15%/);
    // Take the surprise away and the same attack is exactly that much weaker; the defence is the same.
    const when = g.flags['sneak:GER:DEN'];
    delete g.flags['sneak:GER:DEN'];
    const cold = assess(g, [c], jutland);
    expect(surprise(cold.lines)).toBe(false);
    expect(hot.A / cold.A).toBeCloseTo(RELATIONS.sneak.attack, 9);
    expect(hot.D).toBeCloseTo(cold.D, 9);
    g.flags['sneak:GER:DEN'] = when;
    // A month later the Danes are ready.
    expect(passMonth(g)).toBe(true);
    expect(sneakOn(g, 'GER', 'DEN')).toBe(false);
    if (!c.dead && jutland.ctrl === 'DEN') expect(surprise(assess(g, [c], jutland).lines)).toBe(false);
  });

  it('gives the surprise to the one who declared, on that country\'s ground alone', () => {
    const g = grand();
    const c = corps(g, 'GER', 'Schleswig-Holstein');
    expect(orderDeclare(g, 'DEN')).toBe(true);
    // Italy is Germany's ally and now at war with Denmark too, but it did not strike the blow.
    expect(sneakOn(g, 'ITA', 'DEN')).toBe(false);
    expect(sneakOn(g, 'DEN', 'GER')).toBe(false);
    expect(sneakOn(g, 'GER', 'NED')).toBe(false);
    // Poland has been at war since the first day: no surprise there.
    expect(sneakOn(g, 'GER', 'POL')).toBe(false);
    const east = army(g, 'Lower Silesia');
    expect(east.length).toBeGreaterThan(0);
    expect(g.byName['Lodz'].ctrl).toBe('POL');
    expect(surprise(assess(g, east, g.byName['Lodz']).lines)).toBe(false);
    // The Danes hitting back get nothing from it.
    const danes = army(g, 'Jutland', 'DEN');
    expect(surprise(assess(g, danes, g.byName['Schleswig-Holstein']).lines)).toBe(false);
    expect(c.dead).toBe(false);
  });

  it('is no surprise to a country already at war with the declarer\'s ally', () => {
    const g = grand('ITA');
    // France has been at war with Germany, Italy's ally, since September: it is watching the Alps.
    expect(atWar(g, 'GER', 'FRA')).toBe(true);
    expect(atWar(g, 'ITA', 'FRA')).toBe(false);
    expect(wouldSurprise(g, 'ITA', 'FRA')).toBe(false);
    // Italy moves after Germany: its first month comes round.
    expect(humanUp(g)).toBe(false);
    expect(passMonth(g)).toBe(true);
    expect(declarable(g)).toEqual(expect.arrayContaining(['FRA', 'YUG']));
    // Yugoslavia is at peace with everyone: there the blow would be a surprise, and the neutrals would mind.
    expect(wouldSurprise(g, 'ITA', 'YUG')).toBe(true);
    const y = previewDeclare(g, 'YUG')!;
    expect(y.sneak).toBe(RELATIONS.sneak.attack);
    expect(y.joins).toBe('allies');
    expect(ids(y.alarm.filter((a) => a.friend))).toEqual(expect.arrayContaining(['GRE', 'TUR']));
    // France is of the Allies and already fighting: no surprise, and no neutral is shocked that the war widens.
    const f = previewDeclare(g, 'FRA')!;
    expect(f.sneak).toBe(1);
    expect(f.joins).toBeNull();
    expect(f.alarm).toEqual([]);
    const spain = g.nations['ESP'].opinion, news = g.headlines.length;
    expect(orderDeclare(g, 'FRA')).toBe(true);
    expect(atWar(g, 'ITA', 'FRA')).toBe(true);
    expect(sneakOn(g, 'ITA', 'FRA')).toBe(false);
    expect(g.nations['ESP'].opinion).toBe(spain);
    expect(g.headlines.slice(news).some((h) => /alarms the neutral world/.test(h.text))).toBe(false);
  });

  it('lets the computer surprise its victims too, but only the player answers to the neutrals', () => {
    const g = grand('UK');
    const nor = g.nations['NOR'].opinion;
    expect(alarmed(g, 'GER', 'DEN')).toEqual([]);
    declareWar(g, 'GER', 'DEN');
    expect(sneakOn(g, 'GER', 'DEN')).toBe(true);
    expect(g.nations['NOR'].opinion).toBe(nor);
    expect(g.headlines.some((h) => /alarms the neutral world/.test(h.text))).toBe(false);
    const c = corps(g, 'GER', 'Schleswig-Holstein');
    expect(surprise(assess(g, [c], g.byName['Jutland']).lines)).toBe(true);
    // The same with nobody playing.
    const idle = grand(null);
    expect(alarmed(idle, 'GER', 'DEN')).toEqual([]);
    declareWar(idle, 'GER', 'DEN');
    expect(sneakOn(idle, 'GER', 'DEN')).toBe(true);
    expect(idle.headlines.some((h) => /alarms the neutral world/.test(h.text))).toBe(false);
  });

  it('turns the neutrals toward the Axis when the Allies strike first', () => {
    const g = grand('UK');
    const f = warForecast(g, 'UK', 'NOR');
    expect(f.joins).toBe('axis');
    expect(f.sneak).toBe(RELATIONS.sneak.attack);
    expect(f.alarm.length).toBeGreaterThan(5);
    for (const a of f.alarm) expect(a.opinion, a.nat).toBeGreaterThanOrEqual(g.nations[a.nat].opinion);
    const swe = f.alarm.find((a) => a.nat === 'SWE')!;
    expect(swe.friend).toBe(true);
    expect(swe.opinion - g.nations['SWE'].opinion).toBeCloseTo(25 * 0.7, 9);
    // A neutral that leaned toward the Allies and traded with Britain may step back.
    const turkey = g.nations['TUR'];
    expect(step(turkey)).toBe(-1);
    expect(tradePartners(g, 'UK').map((n) => n.id)).toContain('TUR');
    expect(f.alarm.find((x) => x.nat === 'TUR')!.trade).toBe(false);
    turkey.opinion = -22;
    const a = warForecast(g, 'UK', 'NOR').alarm.find((x) => x.nat === 'TUR')!;
    expect(a.to).toBe('independent');
    expect(a.trade).toBe(true);
  });

  it('asks no price of a player outside both blocs, or for a victim inside one', () => {
    const g = grand('SOV');
    expect(bloc(g.nations['SOV'])).toBeNull();
    expect(alarmed(g, 'SOV', 'FIN')).toEqual([]);
    // The surprise is still his.
    expect(wouldSurprise(g, 'SOV', 'FIN')).toBe(true);
    expect(warForecast(g, 'SOV', 'FIN').joins).toBeNull();
    const h = grand('GER');
    expect(alarmed(h, 'GER', 'FRA')).toEqual([]);
    expect(alarmed(h, 'GER', 'ITA')).toEqual([]);
    expect(alarmed(h, 'GER', 'NOWHERE')).toEqual([]);
  });

  it('leaves a country that has just changed its mind where it is for now', () => {
    const g = grand();
    // Norway stepped this very month: opinion still falls, but it does not step twice in one season.
    g.flags['step:NOR'] = g.turn;
    const a = previewDeclare(g, 'DEN')!.alarm.find((x) => x.nat === 'NOR')!;
    expect(a.opinion).toBeLessThan(g.nations['NOR'].opinion);
    expect(a.to).toBe('independent');
    expect(orderDeclare(g, 'DEN')).toBe(true);
    expect(g.nations['NOR'].align).toBe('independent');
    expect(g.nations['NOR'].opinion).toBeCloseTo(a.opinion, 9);
  });
});
