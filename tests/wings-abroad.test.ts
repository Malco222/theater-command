// Wings bought abroad (Brian, 21 November 2026: "In terms of the general mechanics of the battles such as bombing
// missions and stuff like that, it should apply to all playable nations"). In release 1's rules 28 of the 51 seats could
// never field an aircraft (ruling 213(d): no works and no dated row from abroad). The design director's shape of 21
// November: a smaller power orders a wing its own works do not build from a great power that is not at war with it and
// not allied to its enemies, an ally first, then the nearest; half as much again, 6 months for fighters and 10 for
// bombers (the historian's middles for new-built export orders); one wing of a kind at a time. Release 1: the player's
// own order only, so a war fought by staffs alone is the war it was.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { ABROAD, ARSENAL, abroadRules, aircraftTag, boughtFrom, deliverArsenal, greatPower, offers, orderOffer, sellerOf, theGroupName, whyNoWorks, worksOpening, type Offer } from '../src/campaign/arsenal';
import { airFleetsOf } from '../src/campaign/air';
import { groupJobs, setGroupJob } from '../src/campaign/airwar';
import { majorsOf } from '../src/campaign/diplomacy';
import { allied, atWar, createGame, setWar, theName } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const offer = (g: Game, nat: string, key: string, sel = -1): Offer => offers(g, nat, sel).find((o) => o.key === key)!;
const A = ABROAD()!;
let was = true;
beforeEach(() => { was = A.on; });
afterEach(() => { A.on = was; });

/** The 28 seats of release 1 that had no works and no dated row: none could ever field an aircraft. */
const NEVER = ['CHL', 'COL', 'DEN', 'GRE', 'IRL', 'NOR', 'PER', 'VEN', 'BOL', 'COS', 'CUB', 'DOM', 'ECU', 'ELS', 'EST', 'GUA', 'HAI', 'HON', 'IRN', 'IRQ', 'LAT', 'LIT', 'LUX', 'NIC', 'PAN', 'PAR', 'SVK', 'URU'];

describe('wings bought abroad: the data', () => {
  it('ships on, at the design director\'s figures', () => {
    expect(A.on).toBe(true);
    expect(A.price).toBe(1.5);
    expect(A.months).toEqual({ ftr: 6, bmr: 10 });
  });
});

describe('wings bought abroad: the player\'s own order', () => {
  it('offers both wings to every seat that could never field an aircraft, from a great power that may sell', () => {
    for (const nat of NEVER) {
      const g = grand(3, nat);
      if (!g.nations[nat]?.alive) continue;
      expect(greatPower(g, nat), nat).toBe(false);
      expect(abroadRules(g, nat), nat).toBe(true);
      const seller = sellerOf(g, nat);
      expect(seller, nat).not.toBeNull();
      expect(majorsOf(g), nat).toContain(seller);
      expect(atWar(g, seller!, nat), nat).toBe(false);
      for (const e of Object.keys(g.nations)) if (e !== nat && g.nations[e].alive && atWar(g, nat, e)) expect(allied(g, seller!, e), `${nat} ${seller} ${e}`).toBe(false);
      for (const wing of ['ftr', 'bmr'] as const) {
        const o = offer(g, nat, `air:${wing}`);
        expect(o.from, `${nat} ${wing}`).toBe(seller);
        // Nothing stands in the way but an airfield to land on or the money: never the want of an aircraft industry.
        expect([null, 'The treasury cannot cover it.', 'Needs airfields to fly from. Build airfields in a province first.'], `${nat} ${wing}: ${o.why}`).toContain(o.why);
        expect(o.months, `${nat} ${wing}`).toBe(A.months[wing]);
        if (o.at >= 0) expect(o.note, `${nat} ${wing}`).toMatch(/^Bought from (the )?[A-Z][A-Za-z ]+\. (Forms a new air group on the airfield at|Joins) /);
        // Half as much again as it would cost at home.
        A.on = false;
        const homePrice = offer(g, nat, `air:${wing}`).cost;
        A.on = true;
        expect(o.cost, `${nat} ${wing}`).toBe(Math.round(homePrice * A.price));
      }
      expect(aircraftTag(g, nat, 'ftr'), nat).toBeNull();
      expect(worksOpening(g, nat), nat).toBeNull();
    }
  });

  it('is the player\'s alone: no computer nation, no staff of his own, and nobody with the switch off', () => {
    const g = grand(3, 'GRE');
    // Another smaller power, led by the computer, in the same war.
    for (const nat of ['NOR', 'DEN', 'IRN', 'BUL']) {
      expect(abroadRules(g, nat), nat).toBe(false);
      expect(boughtFrom(g, nat, 'ftr'), nat).toBeNull();
      expect(offer(g, nat, 'air:ftr').why, nat).toBe(whyNoWorks(g, nat, 'ftr'));
      expect(offer(g, nat, 'air:ftr').from, nat).toBeUndefined();
    }
    // His own staff, once he hands it his building.
    g.flags['dk:build'] = 1;
    expect(abroadRules(g, 'GRE')).toBe(false);
    expect(offer(g, 'GRE', 'air:ftr').why).toBe('Greece has no aircraft industry: more aircraft come only from abroad.');
    delete g.flags['dk:build'];
    expect(offer(g, 'GRE', 'air:ftr').from).toBeTruthy();
    // The switch off: release 1's words, to the letter.
    A.on = false;
    expect(offer(g, 'GRE', 'air:ftr').why).toBe('Greece has no aircraft industry: more aircraft come only from abroad.');
    expect(offer(g, 'GRE', 'air:bmr').why).toBe('Greece has no aircraft industry: more aircraft come only from abroad.');
    expect(aircraftTag(g, 'GRE', 'ftr')).toBe('No aircraft industry');
    expect(worksOpening(g, 'GRE')).toBe('Greece has no aircraft industry: more aircraft come only from abroad.');
    // A game nobody leads.
    A.on = true;
    const h = grand(3, null);
    for (const nat of NEVER) expect(abroadRules(h, nat), nat).toBe(false);
  });

  it('leaves a great power and a works that builds as they were', () => {
    const g = grand(3, 'ROM');
    for (const wing of ['ftr', 'bmr'] as const) {
      expect(whyNoWorks(g, 'ROM', wing)).toBeNull();
      const o = offer(g, 'ROM', `air:${wing}`);
      expect(o.from).toBeUndefined();
      expect(o.note).not.toMatch(/^Bought from/);
      A.on = false;
      const off = offer(g, 'ROM', `air:${wing}`);
      A.on = true;
      expect({ cost: o.cost, months: o.months, why: o.why, note: o.note }).toEqual({ cost: off.cost, months: off.months, why: off.why, note: off.note });
    }
    // Sweden builds bombers at home from the first day, and no fighters before October 1943: only its fighters are bought.
    const s = grand(3, 'SWE');
    expect(offer(s, 'SWE', 'air:ftr').from).toBeTruthy();
    expect(offer(s, 'SWE', 'air:ftr').months).toBe(A.months.ftr);
    expect(offer(s, 'SWE', 'air:bmr').from).toBeUndefined();
    // A great power buys nothing abroad.
    const k = grand(3, 'UK');
    expect(abroadRules(k, 'UK')).toBe(false);
    expect(offer(k, 'UK', 'air:ftr').from).toBeUndefined();
  });

  it('takes the order, lands the wing as a new air group, names the seller, and the group has every order a fighter group has', () => {
    const g = grand(3, 'GRE');
    g.nations.GRE.pp = 99999;
    const o = offer(g, 'GRE', 'air:ftr');
    expect(o.why).toBeNull();
    expect(o.at).toBeGreaterThanOrEqual(0);
    const seller = g.nations[o.from!].name;
    const from = theName(g.nations[o.from!]);
    const place = g.regions[o.at].name;
    // The UI director's words, 21 November (ui-director/words-bought-wings.md): strings 1 to 5, each as ruled.
    expect(o.note).toBe(`Bought from ${from}. Forms a new air group on the airfield at ${place}.`);
    const before = g.nations.GRE.pp;
    const b = orderOffer(g, 'GRE', 'air:ftr')!;
    expect(b).not.toBeNull();
    expect(g.nations.GRE.pp).toBe(before - o.cost);
    expect(b.from).toBe(o.from);
    expect(b.eta).toBe(g.turn + A.months.ftr);
    expect(b.name).toBe(`Fighter wing from ${from} for ${place}`);
    expect(b.name).toContain(seller);
    // One wing of a kind at a time: a second waits for the first (218(d)).
    expect(offer(g, 'GRE', 'air:ftr').months).toBe(A.months.ftr * 2);
    expect(offer(g, 'GRE', 'air:bmr').months).toBe(A.months.bmr);
    // The day it comes.
    expect(airFleetsOf(g, 'GRE').length).toBe(0);
    const h0 = g.headlines.length;
    deliverArsenal(g, 'GRE', b);
    const mine = airFleetsOf(g, 'GRE');
    expect(mine.length).toBe(1);
    const f = mine[0];
    expect(f.ftr).toBe(ARSENAL.air.ftr.count);
    expect(f.bmr).toBe(0);
    expect(f.kind).toBe('ftr');
    expect(f.base).toBe(o.at);
    const news = g.headlines.slice(h0).map((x) => x.text);
    expect(news.length).toBe(1);
    expect(f.name).toBe('1st Fighter Group');
    expect(news[0]).toBe(`100 fighters (the 1st Fighter Group) arrive from ${from} and form on the airfield at ${place}.`);
    // (Brian, 21 November: "Fighter wing choice should go for every playable faction".)
    const jobs = groupJobs(g, f.id).map((j) => j.key);
    expect(jobs).toContain('patrol');
    expect(jobs).toContain('close');
    expect(jobs).toContain('rest');
    expect(setGroupJob(g, f.id, 'close')).toBe(true);
    expect(f.close).toBe(true);
    expect(setGroupJob(g, f.id, 'patrol')).toBe(true);
    expect(f.close).toBeUndefined();
    // A second wing of the kind joins the group it founded, and its line names the seller too.
    expect(offer(g, 'GRE', 'air:ftr').note).toBe(`Bought from ${from}. Joins the 1st Fighter Group at ${place}.`);
    const b2 = orderOffer(g, 'GRE', 'air:ftr')!;
    const h1 = g.headlines.length;
    deliverArsenal(g, 'GRE', b2);
    expect(airFleetsOf(g, 'GRE').length).toBe(1);
    expect(f.ftr).toBe(ARSENAL.air.ftr.count * 2);
    expect(g.headlines.slice(h1).map((x) => x.text)[0]).toBe(`100 fighters from ${from} join the 1st Fighter Group at ${place}.`);
  });

  it("puts 'the' before a group named in English and leaves a name in its own language bare", () => {
    expect(theGroupName('1st Fighter Group')).toBe('the 1st Fighter Group');
    expect(theGroupName('12th Bomber Group')).toBe('the 12th Bomber Group');
    expect(theGroupName('3rd Squadron')).toBe('the 3rd Squadron');
    for (const own of ['1. Hava Alayı', 'Regimiento Aéreo 1', '1º Regimento de Aviação', '1. Iztrebitelen Polk', 'Grupo Aéreo 1', '21 Mira Dioxeos', 'No. 1 Squadron RIrAF']) expect(theGroupName(own)).toBe(own);
    // Turkey's own works build nothing: its bought fighters take the name of its own ladder, and the lines leave it bare.
    const g = grand(3, 'TUR');
    g.nations.TUR.pp = 99999;
    const o = offer(g, 'TUR', 'air:ftr');
    expect(o.from).toBeDefined();
    const from = theName(g.nations[o.from!]);
    const h0 = g.headlines.length;
    deliverArsenal(g, 'TUR', orderOffer(g, 'TUR', 'air:ftr')!);
    const f = airFleetsOf(g, 'TUR').find((x) => x.kind === 'ftr' && x.ftr === ARSENAL.air.ftr.count)!;
    expect(f).toBeDefined();
    expect(/^\d+(st|nd|rd|th) /.test(f.name)).toBe(false);
    expect(g.headlines.slice(h0).map((x) => x.text)[0]).toBe(`100 fighters (${f.name}) arrive from ${from} and form on the airfield at ${g.regions[f.base].name}.`);
    expect(offer(g, 'TUR', 'air:ftr').note).toBe(`Bought from ${from}. Joins ${f.name} at ${g.regions[f.base].name}.`);
  });

  it('a bomber wing bought abroad can be ordered and lands the same way', () => {
    const g = grand(3, 'NOR');
    g.nations.NOR.pp = 99999;
    const o = offer(g, 'NOR', 'air:bmr');
    expect(o.why).toBeNull();
    const b = orderOffer(g, 'NOR', 'air:bmr')!;
    expect(b.eta).toBe(g.turn + A.months.bmr);
    deliverArsenal(g, 'NOR', b);
    const f = airFleetsOf(g, 'NOR')[0];
    expect(f.bmr).toBe(ARSENAL.air.bmr.count);
    expect(f.kind).toBe('bmr');
    expect(groupJobs(g, f.id).map((j) => j.key)).not.toContain('close');
  });
});

describe('wings bought abroad: who sells', () => {
  it('an ally before any other great power', () => {
    const g = grand(3, 'GRE');
    g.nations.GRE.align = 'allies';
    setWar(g, 'GRE', 'ITA');
    const seller = sellerOf(g, 'GRE')!;
    expect(allied(g, seller, 'GRE')).toBe(true);
    expect(atWar(g, seller, 'GRE')).toBe(false);
  });

  it('never an enemy, and never a friend of an enemy', () => {
    const g = grand(3, 'GRE');
    setWar(g, 'GRE', 'ITA');
    const seller = sellerOf(g, 'GRE')!;
    expect(seller).not.toBe('ITA');
    expect(allied(g, seller, 'ITA')).toBe(false);
    for (const x of majorsOf(g)) if (x !== 'ITA' && allied(g, x, 'ITA')) expect(seller, x).not.toBe(x);
  });

  it('refuses in words when every great power is an enemy or stands with one', () => {
    const g = grand(3, 'GRE');
    for (const x of majorsOf(g)) if (g.nations[x]?.alive) setWar(g, 'GRE', x);
    expect(sellerOf(g, 'GRE')).toBeNull();
    for (const wing of ['ftr', 'bmr'] as const) {
      const o = offer(g, 'GRE', `air:${wing}`);
      expect(o.why).toBe('No great power will sell aircraft to Greece now: each is at war with it or allied to its enemies.');
      expect(o.from).toBeUndefined();
      expect(o.at).toBe(-1);
      expect(orderOffer(g, 'GRE', `air:${wing}`)).toBeNull();
    }
  });
});

describe('wings bought abroad: a war nobody leads is the war it was', () => {
  it('plays the same months with the switch on and off', () => {
    const play = (on: boolean): string => {
      A.on = on;
      const g = grand(11, null);
      for (let t = 0; t < 8; t++) runTurnAI(g);
      return saveGame(g);
    };
    expect(play(true)).toBe(play(false));
  });
});
