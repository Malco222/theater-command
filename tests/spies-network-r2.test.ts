// A service in being (the spies thread, 24 November 2026, release 2): Brian played and saw no spies, because a grand
// campaign opened with an empty service, the agents waited behind the SPIES door and the enemy's were never on the
// map. With the switch secret.network ('sw:net') the player's service opens the war with agents already at work,
// standing on the map from the first month; the 3D game shows the enemy agents his hunters uncover as pieces, puts
// the secret service in the to-do column and a number on the SPIES door; and the options page has one switch for all
// of release 2's secret war, in the campaign's first month.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame, setWar } from '../src/campaign/game';
import { INTEL, atWork, layNetwork, likeliestFoes, networkPlan, orderSecretWar, spyRoom } from '../src/campaign/intel';
import { NETWORK, SECRET_PARTS, networkOn, secretWarOn, secretWarSettable } from '../src/campaign/secretwar';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { FoeAgent, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Todo = { k: string; t: string; tip: string };
type View = {
  agents: { name: string; where: string; mission: string; status: string }[];
  foeAgents: { id: number; nat: string; region: number; watch: boolean; text: string }[];
  spiesWaiting: number; secretWar: boolean; secretWarCan: boolean; todo: Todo[];
};
const TC = (globalThis as unknown as { TC: {
  start: (o: object) => View; act: (a: string) => { ok: boolean; note: string; view: View };
  load: (t: string) => View | null; save: () => { text: string };
} }).TC;

const keep = structuredClone(INTEL.secret);
afterEach(() => { Object.assign(INTEL.secret, structuredClone(keep)); });
const allOff = (): void => { for (const k of ['hunters', 'orders', 'everyone', 'watchers', 'doubles', 'network'] as const) INTEL.secret[k].on = false; };
const open = (nat: string | null, seed = 1939): Game => startOpen(createGame(seed, nat));
const spies = (v: View): Todo[] => v.todo.filter((t) => t.k === 'page:spies');

describe('the network the service opens the war with', () => {
  it('gives a great power a guard at home, a hunter on its front and two watchers in its likeliest enemies\' capitals, at work at once and for nothing', () => {
    const g = open('UK');
    expect(networkOn(g)).toBe(true);
    const pp = g.nations.UK.pp;
    const laid = layNetwork(g);
    expect(laid.length).toBe(NETWORK().great);
    expect(g.nations.UK.pp).toBe(pp);
    expect(laid.map((a) => [g.regions[a.reg].name, a.mission])).toEqual([['London', 'guard'], ['East Anglia', 'hunt'], ['Berlin', 'watch'], ['Rome', 'watch']]);
    for (const a of laid) { expect(atWork(g, a)).toBe(true); expect(a.net).toBe(true); expect(a.xp).toBe(0); }
    // Each is in the service's file, and none is "in training": they were recruited before the war.
    const room = spyRoom(g)!;
    expect(room.agents.every((a) => a.atWork && !a.training)).toBe(true);
    expect(room.notes.filter((n) => n.what === 'network').map((n) => n.text)).toContain(`Agent ${laid[2].name} is in place at Berlin, watching and reporting.`);
    expect(new Set(laid.map((a) => a.name)).size).toBe(laid.length);
    // Once only.
    expect(layNetwork(g)).toEqual([]);
    expect(room.agents.length).toBe(4);
  });

  it('puts the hunter where the enemy\'s watchers are likeliest, and none for a country at peace', () => {
    const ger = open('GER');
    expect(networkPlan(ger, 'GER').map((p) => [ger.regions[p.reg].name, p.mission])).toEqual([['Berlin', 'guard'], ['Masuria', 'hunt'], ['Paris', 'watch'], ['London', 'watch']]);
    // The Soviet Union is at peace in September 1939: three watchers abroad and no hunter.
    const sov = open('SOV');
    const plan = networkPlan(sov, 'SOV');
    expect(plan.map((p) => p.mission)).toEqual(['guard', 'watch', 'watch', 'watch']);
    expect(sov.regions[plan[1].reg].name).toBe('Berlin');
  });

  it('gives a smaller country a guard and a watcher, the hunter only where there is room for a watcher too', () => {
    const pol = open('POL');
    expect(networkPlan(pol, 'POL').map((p) => [pol.regions[p.reg].name, p.mission])).toEqual([['Warsaw', 'guard'], ['Berlin', 'watch']]);
    const fin = open('FIN');
    expect(networkPlan(fin, 'FIN').map((p) => [fin.regions[p.reg].name, p.mission])).toEqual([['Helsinki', 'guard'], ['Moscow', 'watch']]);
  });

  it('watches those at war with him first, then the other camp, then the great powers, the nearest first; never an ally', () => {
    const g = open('UK');
    const foes = likeliestFoes(g, 'UK');
    expect(foes[0]).toBe('GER');
    expect(foes).not.toContain('FRA');
    expect(foes).not.toContain('UK');
    // A new war comes first.
    setWar(g, 'UK', 'SWE');
    expect(likeliestFoes(g, 'UK').slice(0, 2)).toEqual(['GER', 'SWE']);
    for (const f of likeliestFoes(g, 'UK')) expect(g.regions[g.nations[f].capital].ctrl).toBe(f);
  });

  it('never opens with more agents than the service can run', () => {
    INTEL.agents.most.UK = 1;
    try {
      const g = open('UK');
      expect(layNetwork(g).map((a) => a.mission)).toEqual(['guard']);
    } finally { INTEL.agents.most.UK = 8; }
  });

  it('is laid only for the player, in a grand campaign begun with it', () => {
    expect(layNetwork(open(null), null)).toEqual([]);
    expect(layNetwork(createGame(1939, 'UK'))).toEqual([]);
    INTEL.secret.network.on = false;
    const g = open('UK');
    expect(networkOn(g)).toBe(false);
    expect(layNetwork(g)).toEqual([]);
    expect(g.intel.agents.length).toBe(0);
  });

  it('is kept with the save', () => {
    const g = open('UK');
    layNetwork(g);
    const back = loadGame(saveGame(g))!;
    expect(back.intel.agents.map((a) => [a.name, a.net])).toEqual(g.intel.agents.map((a) => [a.name, true]));
    expect(layNetwork(back)).toEqual([]);
  });

  it('leaves a war nobody plays as it was', () => {
    const on = open(null, 11), off = (() => { allOff(); return open(null, 11); })();
    Object.assign(INTEL.secret, structuredClone(keep));
    layNetwork(on, null);
    expect(on.intel.agents.length).toBe(0);
    for (let k = 0; k < 4; k++) { runTurnAI(on); runTurnAI(off); }
    const strip = (t: string): string => t.replace(/"sw:[a-z]+":1,?/g, '');
    expect(strip(saveGame(on))).toBe(strip(saveGame(off)));
  }, 120_000);
});

describe('the secret war on the options page', () => {
  it('switches all of release 2\'s spying in the first month, and the off campaign is the one opened without it', () => {
    const v = TC.start({ human: 'UK', view: 'grand', from: 1, seed: 1939 });
    expect(v.secretWar).toBe(true);
    expect(v.secretWarCan).toBe(true);
    expect(v.agents.length).toBe(4);
    const off = TC.act('secretwar:off');
    expect(off.ok).toBe(true);
    expect(off.note).toMatch(/^The secret war is off: /);
    expect(off.view.secretWar).toBe(false);
    expect(off.view.agents.length).toBe(0);
    expect(spies(off.view)).toEqual([]);
    const switchedOff = TC.save().text;
    allOff();
    TC.start({ human: 'UK', view: 'grand', from: 1, seed: 1939 });
    // (What the network's watchers saw of the enemy's corps in the hour before it was taken back stays known: they had
    // already reported it. Everything else is the campaign opened without the secret war.)
    const seen = (t: string): string => { const x = JSON.parse(t); delete x.intel.known; delete x.intel.how; return JSON.stringify(x); };
    expect(seen(TC.save().text)).toBe(seen(switchedOff));
    Object.assign(INTEL.secret, structuredClone(keep));
    // On again: the network is laid afresh.
    TC.load(switchedOff);
    const on = TC.act('secretwar:on');
    expect(on.ok).toBe(true);
    expect(on.note).toMatch(/^The secret war is on: /);
    expect(on.view.secretWar).toBe(true);
    expect(on.view.agents.map((a) => a.mission)).toEqual(['guard', 'hunt', 'watch', 'watch']);
  }, 120_000);

  it('cannot be switched after the first month', () => {
    const g = open('UK');
    layNetwork(g);
    g.turn += 1;
    expect(secretWarSettable(g)).toBe(false);
    expect(orderSecretWar(g, false)).toBe(false);
    expect(secretWarOn(g)).toBe(true);
    for (const p of SECRET_PARTS) expect(g.flags[`sw:${p}`]).toBe(1);
    // A later start: its own first month.
    const h = open('UK');
    h.flags.openFrom = h.turn;
    expect(secretWarSettable(h)).toBe(true);
  });

  it('turned off, takes back only the network, and keeps an agent he recruited', () => {
    const g = open('UK');
    layNetwork(g);
    const own = { id: g.intel.next++, nat: 'UK', name: 'Recruit', xp: 0, reg: g.nations.UK.capital, ready: g.turn + 1, mission: 'guard' as const, born: g.turn };
    g.intel.agents.push(own);
    expect(orderSecretWar(g, false)).toBe(true);
    expect(g.intel.agents.map((a) => a.name)).toEqual(['Recruit']);
    expect(g.intel.notes.some((n) => n.what === 'network')).toBe(false);
    expect(secretWarOn(g)).toBe(false);
  });
});

describe('the 3D game: the to-do column, the SPIES door and the enemy\'s agents on the map', () => {
  it('asks for him in the month the network opens the war, and puts its count on the door', () => {
    const v = TC.start({ human: 'GER', view: 'grand', from: 1, seed: 1939 });
    expect(v.spiesWaiting).toBe(4);
    const row = spies(v);
    expect(row.length).toBe(1);
    expect(row[0].t).toBe('YOUR SPIES ARE IN PLACE');
    expect(row[0].tip).toMatch(/^Your secret service has four agents in place: \w+ on guard at Berlin, \w+ hunting enemy agents in Masuria, \w+ watching at Paris and \w+ watching at London\. /);
  });

  it('shows an uncovered enemy agent as a piece in its country\'s colours, with a row and a count, until arrested', () => {
    TC.start({ human: 'UK', view: 'grand', from: 1, seed: 1939 });
    const g = loadGame(TC.save().text)!;
    g.turn += 1; // (the network's month is over)
    const kent = g.byName.Kent.id;
    const f: FoeAgent = { id: 1, nat: 'GER', reg: kent, kind: 'watch', strike: g.turn, found: g.turn };
    g.intel.foes = [f];
    g.intel.foeNext = 2;
    const v = TC.load(saveGame(g))!;
    expect(v.foeAgents).toEqual([{ id: 1, nat: 'GER', region: kent, watch: true, text: expect.stringMatching(/^A German agent watching your defences in Kent, uncovered this month\. /) }]);
    expect(v.spiesWaiting).toBe(1);
    const row = spies(v);
    expect(row.map((r) => r.t)).toEqual(['AN ENEMY WATCHER IS UNCOVERED']);
    expect(row[0].tip).toMatch(/^A German watcher in Kent\. A watcher helps their side's attacks until arrested\. They stand on the map in their country's colours\. Open THE SECRET SERVICE to arrest them or turn them\.$/);
    // Not yet found: not on the map.
    delete g.intel.foes[0].found;
    expect(TC.load(saveGame(g))!.foeAgents).toEqual([]);
  });

  it('with the secret war off, has no enemy agents to show and no network row', () => {
    allOff();
    const v = TC.start({ human: 'UK', view: 'grand', from: 1, seed: 1939 });
    expect(v.secretWar).toBe(false);
    expect(v.foeAgents).toEqual([]);
    expect(v.spiesWaiting).toBe(0);
    expect(spies(v)).toEqual([]);
  });
});
