// The fall of nations and their return. Every fall goes on a roll that both interfaces announce, in every campaign.
// In the grand campaign a country beaten in the field comes back when an alliance member at war with its conqueror
// holds its capital: it joins that alliance, takes back what its new allies hold of it, raises a small army, and the
// player who lost it is given it back. The historical scenarios keep the war they had.
import { beforeAll, describe, expect, it } from 'vitest';
import lib from '../data/campaign/liberation.json';
import { allied, atWar, bloc, capitulate, capture, checkCollapse, createGame, removeCorps } from '../src/campaign/game';
import { enrol } from '../src/campaign/league';
import { LIB, fallenNations, liberate, passingCard, standsFreed, whyNoReturn, whyNotFreedBy } from '../src/campaign/liberation';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { announcement, announcements, fallenCapitalAt, nationsFallen, playerFell, playerLost, runTurnAI } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

/** Removes every corps from a province, so that it can be walked into. */
const clear = (g: Game, r: Region): void => { for (const c of [...r.units]) removeCorps(g, c); };
/** Takes an emptied province for a nation, as a march into it would. */
const take = (g: Game, name: string, nat: string): Region => { const r = g.byName[name]; clear(g, r); capture(g, r, nat); return r; };
const freed = (g: Game): string[] => g.passings.filter((p) => p.kind === 'freed').map((p) => p.nat);
const corpsOf = (g: Game, nat: string) => g.corps.filter((c) => c.nat === nat && !c.dead);

// One computer-run grand campaign played to the autumn of 1940: Poland, the Low Countries and France have fallen to
// Germany, and the Baltic states have been annexed by the Soviet Union. Each test starts from a copy of it.
let base = '';
let basePlayer = '';
const world = (): Game => loadGame(base)!;
beforeAll(() => {
  const play = (human: string | null): Game => {
    const g = startOpen(createGame(2, human));
    let guard = 0;
    while (g.nations.FRA.alive && guard++ < 30) runTurnAI(g);
    for (let k = 0; k < 3; k++) runTurnAI(g);
    return g;
  };
  base = saveGame(play(null));
  basePlayer = saveGame(play('FRA'));
}, 120000);

describe('the liberation data', () => {
  it('gives a wait, an army and the words of the announcement', () => {
    expect(lib.gap).toBeGreaterThanOrEqual(1);
    expect(lib.never).toContain('VIC');
    expect(lib.army.most).toBeGreaterThanOrEqual(1);
    expect(lib.army.divs).toBeGreaterThan(0);
    expect(lib.army.str).toBeLessThanOrEqual(1);
    for (const line of [lib.lines.freed, lib.lines.freedAfter, lib.lines.freedSelf]) { expect(line).toMatch(/\{Name\}/); expect(line).toMatch(/\{capital\}/); }
    expect(Object.keys(lib.lines.sides).sort()).toEqual(['allies', 'axis', 'league']);
    expect(LIB.army.perVp).toBe(lib.army.perVp);
  });
});

describe('the roll of fallen nations', () => {
  it('is kept in a historical campaign too, where nobody comes back', () => {
    const g = createGame(1, null);
    for (let t = 1; t < 12; t++) runTurnAI(g);
    const roll = g.passings;
    expect(roll.length).toBeGreaterThan(3);
    expect(roll.every((p) => p.kind === 'fell')).toBe(true);
    const pol = roll.find((p) => p.nat === 'POL')!;
    expect(pol.by).toBe('GER');
    expect(pol.how).toBe('beaten');
    expect(pol.provinces).toBeGreaterThan(10);
    expect(pol.capital).toBe(g.byName['Warsaw'].id);
    expect(g.nations.POL.fellOn!.how).toBe('beaten');
    expect(roll.map((p) => p.id)).toEqual(roll.map((_, i) => i + 1));
    // The return is a rule of the grand campaign only.
    expect(whyNoReturn(g, 'POL')).toMatch(/grand campaign/);
    const warsaw = take(g, 'Warsaw', 'UK');
    expect(g.nations.POL.alive).toBe(false);
    expect(warsaw.ctrl).toBe('UK');
    expect(Object.values(g.nations).some((n) => n.freedOn)).toBe(false);
    const card = passingCard(g, pol);
    expect(card.title).toBe('Poland has fallen');
    expect(card.more).toBe('');
  });

  it('tells a defeat, an annexation and who did it, in words both interfaces can show', () => {
    const g = world();
    const cards = announcements(g);
    expect(cards.length).toBe(g.passings.length);
    const fra = cards.find((c) => c.nat === 'FRA')!;
    expect(fra.kind).toBe('fell');
    expect(fra.title).toBe('France has fallen');
    expect(fra.kicker).toMatch(/^Eliminated · \w+ 1940$/);
    expect(fra.text).toMatch(/asked Germany for terms/);
    expect(fra.more).toMatch(/can come back.*takes Paris/);
    expect(fra.region).toBe(g.byName['Paris'].id);
    expect(fra.facts.length).toBe(3);
    const est = cards.find((c) => c.nat === 'EST')!;
    expect(est.title).toBe('Estonia is annexed');
    expect(est.text).toMatch(/annexed by the Soviet Union/);
    expect(est.more).toMatch(/will not return/);
    const ned = cards.find((c) => c.nat === 'NED')!;
    expect(ned.title).toBe('The Netherlands has fallen');
    expect(announcement(g, fra.id)!.title).toBe(fra.title);
    expect(announcement(g, 999)).toBeNull();
    expect(announcements(g, g.nations.FRA.fellOn!.turn).some((c) => c.nat === 'FRA')).toBe(true);
    expect(announcements(g, 1).some((c) => c.nat === 'FRA')).toBe(false);
  });

  it('lists the fallen with what it would take to free each', () => {
    const g = world();
    g.human = 'UK';
    const rows = nationsFallen(g);
    const fra = rows.find((f) => f.nat === 'FRA')!;
    expect(fra.canReturn).toBe(true);
    expect(fra.yours).toBe(true);
    expect(fra.side).toBe('allies');
    expect(fra.capitalName).toBe('Paris');
    expect(fra.holder).toBe('GER');
    expect(fra.note).toMatch(/^Take Paris and France is liberated/);
    const est = rows.find((f) => f.nat === 'EST')!;
    expect(est.canReturn).toBe(false);
    expect(est.yours).toBe(false);
    expect(est.note).toMatch(/annexed outright/);
    expect(fallenCapitalAt(g, g.byName['Paris'].id)!.nat).toBe('FRA');
    expect(fallenCapitalAt(g, g.byName['London'].id)).toBeNull();
    // Seen from Germany, which conquered it, France is nobody's to free.
    const ger = fallenNations(g, 'GER').find((f) => f.nat === 'FRA')!;
    expect(ger.yours).toBe(false);
    expect(ger.note).toMatch(/stands with its conqueror/);
    // Rows come latest fall first.
    for (let i = 1; i < rows.length; i++) expect(rows[i - 1].fell.turn).toBeGreaterThanOrEqual(rows[i].fell.turn);
  });

  it('survives a save, and is written up from memory for a save made before it was kept', () => {
    const g = world();
    const again = loadGame(saveGame(g))!;
    expect(again.passings).toEqual(g.passings);
    const old = JSON.parse(saveGame(g));
    delete old.passings;
    for (const n of Object.values(old.nations) as { fellOn?: { how?: string } }[]) if (n.fellOn) delete n.fellOn.how;
    const g2 = loadGame(JSON.stringify(old))!;
    expect(g2.passings.map((p) => p.nat).sort()).toEqual(g.passings.filter((p) => p.kind === 'fell').map((p) => p.nat).sort());
    expect(g2.passings.find((p) => p.nat === 'EST')!.how).toBe('annexed');
    expect(g2.passings.find((p) => p.nat === 'FRA')!.how).toBe('beaten');
    expect(whyNoReturn(g2, 'EST')).toMatch(/annexed/);
    expect(whyNoReturn(g2, 'FRA')).toBeNull();
    // A campaign with nobody on the roll is saved as it always was.
    expect('passings' in JSON.parse(saveGame(startOpen(createGame(3, null))))).toBe(false);
  });
});

describe('who can free a fallen country', () => {
  it('is an alliance member at war with its conqueror, and nobody else', () => {
    const g = world();
    expect(whyNoReturn(g, 'FRA')).toBeNull();
    expect(whyNotFreedBy(g, 'FRA', 'UK')).toBeNull();
    expect(whyNotFreedBy(g, 'FRA', 'GER')).toMatch(/conqueror/);
    expect(whyNotFreedBy(g, 'FRA', 'ITA')).toMatch(/conqueror/);
    // The Soviet Union stands outside both alliances in 1940 and is at peace with Germany.
    expect(bloc(g.nations.SOV)).toBeNull();
    expect(whyNotFreedBy(g, 'POL', 'SOV')).toMatch(/not at war|no alliance/);
    expect(whyNoReturn(g, 'UK')).toMatch(/not fallen/);
    expect(whyNoReturn(g, 'LAT')).toMatch(/annexed/);
    expect(whyNoReturn(g, 'VIC')).not.toBeNull();
    // Taking the capital without the right to free it changes nothing but the flag over it.
    const warsaw = take(g, 'Warsaw', 'SOV');
    expect(warsaw.ctrl).toBe('SOV');
    expect(g.nations.POL.alive).toBe(false);
    expect(liberate(g, 'POL', 'SOV')).toBe(false);
    expect(freed(g)).toEqual([]);
  });

  it('must wait out the month the country fell in', () => {
    const g = world();
    expect(g.nations.NOR.alive).toBe(true);
    capitulate(g, 'NOR', 'GER');
    expect(g.nations.NOR.alive).toBe(false);
    expect(whyNotFreedBy(g, 'NOR', 'UK')).toMatch(/only this month/);
    expect(whyNotFreedBy(g, 'NOR', 'UK', true)).toBeNull();
    g.human = 'UK';
    expect(nationsFallen(g).find((f) => f.nat === 'NOR')!.note).toMatch(/^From next month, take Oslo/);
    const oslo = take(g, 'Oslo', 'UK');
    expect(g.nations.NOR.alive).toBe(false);
    expect(oslo.ctrl).toBe('UK');
    // Next month the capital is already in friendly hands: Norway returns the next time any of its ground is taken.
    g.turn++;
    expect(nationsFallen(g).find((f) => f.nat === 'NOR')!.note).toMatch(/^Your side holds Oslo/);
    const other = g.regions.find((r) => r.owner === 'NOR' && !r.overseas && r.id !== oslo.id)!;
    clear(g, other);
    other.ctrl = 'GER';
    capture(g, other, 'UK');
    expect(g.nations.NOR.alive).toBe(true);
    expect(oslo.ctrl).toBe('NOR');
    expect(other.ctrl).toBe('NOR');
  });

  it('is nobody once the last act has dissolved the alliances', () => {
    const g = world();
    g.flags.dom = g.turn;
    expect(whyNoReturn(g, 'FRA')).toMatch(/last act/);
    take(g, 'Paris', 'UK');
    expect(g.nations.FRA.alive).toBe(false);
  });

  it('lets the League restore a small country but not a great power', () => {
    const g = world();
    enrol(g, g.nations.SWE);
    g.nations.SWE.atWar.push('GER'); g.nations.GER.atWar.push('SWE');
    expect(bloc(g.nations.SWE)).toBe('league');
    expect(whyNotFreedBy(g, 'FRA', 'SWE')).toMatch(/League restores only/);
    expect(whyNotFreedBy(g, 'DEN', 'SWE')).toBeNull();
    take(g, 'Zealand', 'SWE');
    expect(g.nations.DEN.alive).toBe(true);
    expect(g.nations.DEN.league).toBe(true);
    expect(bloc(g.nations.DEN)).toBe('league');
    expect(allied(g, 'DEN', 'SWE')).toBe(true);
    expect(atWar(g, 'DEN', 'GER')).toBe(true);
    expect(g.passings[g.passings.length - 1].side).toBe('league');
  });
});

describe('a liberated country', () => {
  it('returns when its capital is retaken: an ally again, at war, with its ground and a small army', () => {
    const g = world();
    const F = g.nations.FRA;
    const paris = g.byName['Paris'], rouen = g.byName['Rouen'];
    // The British already hold Rouen as occupied ground, and have for a while.
    clear(g, rouen);
    rouen.ctrl = 'UK'; rouen.since = g.turn - 2;
    const heads = g.headlines.length, rolled = g.passings.length;
    take(g, 'Paris', 'UK');
    expect(F.alive).toBe(true);
    expect(F.align).toBe('allies');
    expect(F.freedOn).toEqual({ turn: g.turn, week: g.week, by: 'UK' });
    expect(atWar(g, 'FRA', 'GER')).toBe(true);
    expect(atWar(g, 'GER', 'FRA')).toBe(true);
    expect(F.atWar.every((e) => atWar(g, 'UK', e))).toBe(true);
    expect(allied(g, 'FRA', 'UK')).toBe(true);
    // The capital and the ground its ally held are French again. The roads on ground that did not change sides stay open.
    expect(paris.ctrl).toBe('FRA');
    expect(paris.cap).toBe(true);
    expect(rouen.ctrl).toBe('FRA');
    expect(rouen.since).toBe(g.turn - 2);
    // What Germany still holds stays German until it is fought for.
    expect(g.regions.some((r) => r.owner === 'FRA' && r.ctrl === 'GER')).toBe(true);
    // The army: three corps for a country of France's size, green and under strength, with no moves this month.
    const army = corpsOf(g, 'FRA');
    expect(army.length).toBe(3);
    for (const c of army) {
      expect(c.kind).toBe('inf');
      expect(c.divs).toBe(lib.army.divs);
      expect(c.str).toBe(lib.army.str);
      expect(c.org).toBe(lib.army.org);
      expect(c.ap).toBe(0);
      expect(g.regions[c.reg].ctrl).toBe('FRA');
      expect(c.name).toMatch(/Corps$/);
    }
    expect(new Set(army.map((c) => c.name)).size).toBe(3);
    expect(F.armyPeak).toBeCloseTo(3 * lib.army.divs * lib.army.str, 6);
    expect(F.queue).toEqual([]);
    expect(F.pp).toBeGreaterThanOrEqual(0);
    // The news, with the nation's id for the 3D game, and the roll.
    const news = g.headlines.slice(heads).find((h) => h.nat === 'FRA' && h.kind === 'event')!;
    expect(news.text).toBe('France is liberated. British troops hold Paris, and France returns to the war beside the Allies.');
    expect(news.region).toBe(paris.id);
    expect(g.passings.length).toBe(rolled + 1);
    const p = g.passings[rolled];
    expect(p).toMatchObject({ kind: 'freed', nat: 'FRA', by: 'UK', how: 'freed', capital: paris.id, provinces: 2, corps: 3, side: 'allies', turn: g.turn });
    const card = passingCard(g, p);
    expect(card.title).toBe('France is free');
    expect(card.kicker).toMatch(/^Liberated · /);
    expect(card.text).toMatch(/British troops hold Paris\. France returns to the war beside the Allies: 2 provinces are back under its own flag, and 3 new corps are forming at Paris\./);
    expect(card.more).toMatch(/stands for as long as Paris does/);
    // The card of its fall now says it came back.
    expect(passingCard(g, g.passings.find((x) => x.nat === 'FRA' && x.kind === 'fell')!).more).toMatch(/since been liberated/);
    expect(nationsFallen(g).some((f) => f.nat === 'FRA')).toBe(false);
  });

  it('is handed whatever its allies win back of it from then on', () => {
    const g = world();
    take(g, 'Paris', 'UK');
    const next = g.regions.find((r) => r.owner === 'FRA' && r.ctrl === 'GER' && !r.overseas)!;
    clear(g, next);
    capture(g, next, 'UK');
    expect(next.ctrl).toBe('FRA');
    // Ground the armistice gave to Vichy is France's again once France's allies take it.
    const south = g.regions.find((r) => r.owner === 'VIC' && r.vichy && r.ctrl === 'VIC');
    expect(south).toBeTruthy();
    clear(g, south!);
    capture(g, south!, 'UK');
    expect(south!.owner).toBe('FRA');
    expect(south!.ctrl).toBe('FRA');
    expect(south!.cap).toBe(false);
    // Vichy keeps what nobody has taken from it.
    expect(g.regions.some((r) => r.owner === 'VIC' && r.ctrl === 'VIC')).toBe(true);
  });

  it('stands for as long as its capital does, and can fall a second time when the capital is lost', () => {
    const g = world();
    const F = g.nations.FRA;
    const paris = take(g, 'Paris', 'UK');
    expect(standsFreed(g, F)).toBe(true);
    // Its little army destroyed and nearly all its ground in enemy hands: it still stands.
    for (const c of corpsOf(g, 'FRA')) removeCorps(g, c);
    checkCollapse(g, 'FRA', 'GER');
    expect(F.alive).toBe(true);
    // Germany retakes Paris: France falls again, to a plain conquest this time, with no second armistice.
    const vichy = g.regions.filter((r) => r.owner === 'VIC').length;
    const rolled = g.passings.length;
    clear(g, paris);
    capture(g, paris, 'GER');
    expect(F.alive).toBe(false);
    expect(F.freedOn).toBeUndefined();
    expect(F.fellOn!.turn).toBe(g.turn);
    expect(g.regions.filter((r) => r.owner === 'VIC').length).toBe(vichy);
    expect(g.passings[rolled]).toMatchObject({ kind: 'fell', nat: 'FRA', by: 'GER' });
    // And it can be freed again, once the month is out.
    expect(whyNotFreedBy(g, 'FRA', 'UK')).toMatch(/only this month/);
    g.turn++;
    take(g, 'Paris', 'UK');
    expect(F.alive).toBe(true);
    expect(freed(g)).toEqual(['FRA', 'FRA']);
  });

  it('is set free by its conqueror\'s own fall, and the conqueror\'s card says so', () => {
    const g = world();
    const rolled = g.passings.length;
    capitulate(g, 'GER', 'UK');
    expect(g.nations.GER.alive).toBe(false);
    const now = g.passings.slice(rolled);
    expect(now[0]).toMatchObject({ kind: 'fell', nat: 'GER', by: 'UK' });
    const back = now.filter((p) => p.kind === 'freed');
    expect(back.map((p) => p.nat)).toEqual(expect.arrayContaining(['FRA', 'BEL', 'NED', 'POL']));
    for (const p of back) { expect(p.after).toBe('GER'); expect(g.nations[p.nat].alive).toBe(true); expect(bloc(g.nations[p.nat])).toBe('allies'); }
    const card = passingCard(g, now[0]);
    expect(card.freed).toEqual(back.map((p) => p.nat));
    const fra = passingCard(g, back.find((p) => p.nat === 'FRA')!);
    expect(fra.after).toBe('GER');
    expect(fra.text).toMatch(/^With Germany beaten, Paris is in British hands\./);
    expect(g.headlines.some((h) => h.nat === 'FRA' && /With Germany beaten/.test(h.text))).toBe(true);
    // Countries the Soviet Union annexed stay annexed.
    expect(g.nations.EST.alive).toBe(false);
  });

  it('joins the Axis when the Axis frees it', () => {
    const g = world();
    // Britain has brought Italy down; Germany takes Rome back.
    capitulate(g, 'ITA', 'UK');
    expect(g.nations.ITA.alive).toBe(false);
    g.turn++;
    expect(whyNotFreedBy(g, 'ITA', 'GER')).toBeNull();
    expect(whyNotFreedBy(g, 'ITA', 'UK')).toMatch(/conqueror/);
    take(g, 'Rome', 'GER');
    expect(g.nations.ITA.alive).toBe(true);
    expect(bloc(g.nations.ITA)).toBe('axis');
    expect(atWar(g, 'ITA', 'UK')).toBe(true);
    expect(allied(g, 'ITA', 'GER')).toBe(true);
    expect(g.byName['Rome'].ctrl).toBe('ITA');
    expect(g.passings[g.passings.length - 1].side).toBe('axis');
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/Italy is liberated\. German troops hold Rome, and Italy returns to the war beside the Axis\./);
  });
});

describe('the player\'s own country', () => {
  it('is given back to the player who lost it', () => {
    const g = loadGame(basePlayer)!;
    expect(g.human).toBe('FRA');
    expect(g.nations.FRA.alive).toBe(false);
    expect(g.passings.find((p) => p.nat === 'FRA')!.player).toBe(true);
    expect(passingCard(g, g.passings.find((p) => p.nat === 'FRA')!).more).toMatch(/you are given it back to lead/);
    expect(playerLost(g)).toBeNull();
    expect(playerFell(g)).toBe('FRA');
    expect(g.human).toBeNull();
    expect(playerLost(g)).toBe('FRA');
    expect(playerFell(g)).toBeNull();
    // The seat is remembered across a save.
    const g2 = loadGame(saveGame(g))!;
    expect(g2.human).toBeNull();
    expect(playerLost(g2)).toBe('FRA');
    take(g2, 'Paris', 'UK');
    expect(g2.nations.FRA.alive).toBe(true);
    expect(g2.human).toBe('FRA');
    expect(playerLost(g2)).toBeNull();
    const p = g2.passings[g2.passings.length - 1];
    expect(p.player).toBe(true);
    expect(g2.headlines[g2.headlines.length - 1].text).toMatch(/the country is yours to lead again, beside the Allies/);
    expect(passingCard(g2, p).more).toMatch(/^The country is yours to lead again/);
    // The month plays on with the player's country in it.
    const turn = g2.turn;
    runTurnAI(g2);
    expect(g2.turn).toBe(turn + 1);
    expect(g2.human).toBe('FRA');
  });

  it('is not handed to a player who was never its leader', () => {
    const g = world();
    expect(g.human).toBeNull();
    take(g, 'Paris', 'UK');
    expect(g.nations.FRA.alive).toBe(true);
    expect(g.human).toBeNull();
  });
});

describe('a computer-run grand campaign with liberation', () => {
  it('plays four years without a country both alive and fallen, or a freed country without its capital', () => {
    const g = world();
    for (let t = 0; t < 40; t++) {
      runTurnAI(g);
      for (const n of Object.values(g.nations)) {
        if (!n.freedOn) continue;
        expect(n.alive).toBe(true);
        // A freed country that has lost its capital again is still standing only if it could bear the loss.
        if (n.capital >= 0 && allied(g, n.id, g.regions[n.capital].ctrl)) expect(standsFreed(g, n)).toBe(true);
      }
      for (const r of g.regions) expect(g.nations[r.ctrl]).toBeTruthy();
      for (const c of g.corps) expect(g.nations[c.nat].alive).toBe(true);
    }
    const roll = g.passings;
    expect(roll.map((p) => p.id)).toEqual(roll.map((_, i) => i + 1));
    for (const p of roll.filter((x) => x.kind === 'freed')) {
      expect(roll.some((q) => q.kind === 'fell' && q.nat === p.nat && q.id < p.id)).toBe(true);
      expect(p.side).toBeTruthy();
    }
  }, 120000);
});
