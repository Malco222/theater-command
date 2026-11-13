// The technology tree (the grand campaign). Five branches, four steps deep: research teams work on one technology
// at a time, a technology opens when the ones before it are done, and what it gives lasts for the rest of the war.
// The tree lives in data/campaign/tech.json and is read by every version of the game. The historical scenarios
// know none of it: there, research runs on history's clock.
import { describe, expect, it } from 'vitest';
import { offers, orderOffer } from '../src/campaign/arsenal';
import { cancelBuild, corpsCost, deliver, orderCorps } from '../src/campaign/economy';
import { assess, corpsBase, createGame, frozen } from '../src/campaign/game';
import { RESEARCH, foundBureau, setTeams, techMul } from '../src/campaign/research';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  TECH, gateOf, hasTech, idleTeams, locked, monthsFor, paraCount, staffPick, startResearch, stopResearch, teamsOf, techById, techOf, techRule,
  techTick, techTraits, techView, treeLevel, whyNoPara, whyNotResearch, winterReady,
} from '../src/campaign/tech';
import { priceOf, traitAttack, traitDrafts } from '../src/campaign/traits';
import { orderResearch, orderStopResearch, runTurnAI, techTree, whyNotOrderResearch } from '../src/campaign/turn';
import { KIND, type Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const history = (human: string | null = null, seed = 3): Game => createGame(seed, human);
/** January 1941 and January 1942, counting September 1939 as month 1. */
const JAN41 = 17, JAN42 = 29, DEC41 = 28;
const FIELDS = Object.keys(RESEARCH.fields);
const ORDERS = /^(corps:(inf|mtn|mot|arm|cav|art|eng)|work:(fort|aa)|ship:(dd|ss|ca|bb|cv|tp)|air:(ftr|bmr)|para)$/;

/** Puts a team on a technology and lets the months it needs go by. */
function research(g: Game, nat: string, id: string): void {
  expect(startResearch(g, nat, id), `${nat} begins ${id}: ${whyNotResearch(g, nat, id)}`).toBe(true);
  const months = techOf(g, nat)!.work.find((w) => w.id === id)!.left;
  for (let k = 0; k < months; k++) techTick(g, nat);
  expect(hasTech(g, nat, id), id).toBe(true);
}

describe('the data', () => {
  it('is a tree: five branches, four steps, and nothing needs what comes after it', () => {
    const ids = TECH.techs.map((t) => t.id);
    expect(new Set(ids).size).toBe(ids.length);
    expect(TECH.branches.map((b) => b.id)).toEqual(['land', 'air', 'naval', 'industry', 'doctrine']);
    for (const b of TECH.branches) for (const tier of [1, 2, 3, 4]) expect(TECH.techs.some((t) => t.branch === b.id && t.tier === tier), `${b.id} step ${tier}`).toBe(true);
    for (const t of TECH.techs) {
      expect(TECH.branches.some((b) => b.id === t.branch), t.id).toBe(true);
      expect([1, 2, 3, 4], t.id).toContain(t.tier);
      expect(t.year, t.id).toBeGreaterThanOrEqual(1939);
      expect(t.year, t.id).toBeLessThanOrEqual(1944);
      expect(t.name.length, t.id).toBeGreaterThan(3);
      expect(t.text.length, t.id).toBeGreaterThan(20);
      // The words a player reads carry no numbers of their own: those come from the rule.
      expect(t.text, t.id).not.toMatch(/\d%/);
      for (const need of t.needs) {
        const x = techById(need);
        expect(x, `${t.id} needs ${need}`).toBeDefined();
        expect(x!.tier, `${t.id} needs ${need}`).toBeLessThan(t.tier);
        expect(x!.year, `${t.id} needs ${need}`).toBeLessThanOrEqual(t.year);
      }
    }
  });

  it('gives every technology something to do, with numbers that are sane and orders that exist', () => {
    for (const t of TECH.techs) {
      const does = ['field', 'unlock', 'slots', 'winterKit', 'sealift', 'attack', 'defence', 'drafts', 'cost', 'fleet', 'seaLoss', 'oil'].filter((k) => (t as any)[k] !== undefined);
      expect(does.length, t.id).toBeGreaterThan(0);
      if (t.field) expect(FIELDS, t.id).toContain(t.field);
      for (const k of t.unlock ?? []) expect(k, t.id).toMatch(ORDERS);
      for (const m of [t.attack, t.defence, t.drafts, t.fleet]) if (m !== undefined) { expect(m, t.id).toBeGreaterThan(1); expect(m, t.id).toBeLessThanOrEqual(1.25); }
      if (t.oil !== undefined) { expect(t.oil, t.id).toBeGreaterThan(0); expect(t.oil, t.id).toBeLessThan(1); }
      if (t.seaLoss !== undefined) { expect(t.seaLoss).toBeGreaterThanOrEqual(0.7); expect(t.seaLoss).toBeLessThan(1); }
      if (t.cost) { expect(t.cost.mul).toBeGreaterThanOrEqual(0.7); expect(t.cost.mul).toBeLessThan(1); for (const k of t.cost.what) expect(k, t.id).toMatch(ORDERS); }
      for (const k of t.when?.kinds ?? []) expect(KIND[k], t.id).toBeDefined();
      expect(techRule(t).length, t.id).toBeGreaterThan(10);
    }
  });

  it('says in plain words what each technology does, from the rule itself', () => {
    const rule = (id: string): string => techRule(techById(id)!);
    expect(rule('mediums')).toBe('Armoured and motorised corps fight 8% harder.');
    expect(rule('tanks')).toBe('Armoured corps can be raised.');
    expect(rule('carriers')).toBe('Aircraft carriers can be laid down.');
    expect(rule('institutes')).toBe('One more research team.');
    expect(rule('landingcraft')).toBe('Attack up 15% in a landing from the sea. Shipping for one more corps a month.');
    expect(rule('massprod')).toBe('Earnings up 5%. Infantry corps cost 10% less.');
    expect(rule('winterkit')).toMatch(/eastern winter/);
  });

  it('starts every nation with technologies that exist and stand on their own', () => {
    const g = history();
    for (const [nat, list] of Object.entries(TECH.start)) {
      expect(g.nations[nat], nat).toBeDefined();
      expect(new Set(list).size, nat).toBe(list.length);
      for (const id of list) {
        expect(techById(id), `${nat}: ${id}`).toBeDefined();
        for (const need of techById(id)!.needs) expect(list, `${nat}: ${id} needs ${need}`).toContain(need);
      }
    }
    // The great powers begin the war with tanks and lorried infantry, and with two research teams.
    for (const nat of ['GER', 'UK', 'FRA', 'SOV', 'ITA', 'USA']) {
      expect(TECH.start[nat], nat).toEqual(expect.arrayContaining(['tanks', 'motor']));
      expect(TECH.slots[nat], nat).toBe(2);
    }
    for (const order of Object.values(TECH.ai.order)) expect([...order].sort()).toEqual(['air', 'doctrine', 'industry', 'land', 'naval']);
  });
});

describe('the historical scenarios', () => {
  it('know nothing of the tree', () => {
    const g = history('GER');
    expect(techOf(g, 'GER')).toBeNull();
    expect(techTree(g)).toBeNull();
    expect(techView(g, 'GER')).toBeNull();
    expect(teamsOf(g, 'GER')).toBe(0);
    expect(treeLevel(g, 'GER', 'fighters')).toBe(0);
    expect(techTraits(g, 'GER')).toEqual([]);
    expect(locked(g, 'HUN', 'corps:arm')).toBeNull();
    expect(winterReady(g, 'SOV')).toBe(false);
    expect(startResearch(g, 'GER', 'antitank')).toBe(false);
    expect(orderResearch(g, 'antitank')).toBe(false);
    for (const f of FIELDS) expect(techMul(g, 'GER', f as any)).toBe(1);
    techTick(g, 'GER');
    expect(g.nations.GER.tech).toBeUndefined();
    expect(g.flags['tech:open']).toBeUndefined();
    const list = offers(g, 'GER', g.nations.GER.capital);
    expect(list.some((o) => o.tech !== undefined || o.key === 'para')).toBe(false);
    // The old way of steering research still works there.
    expect(setTeams(g, 'GER', 'armour', 2)).toBe(false); // no free team yet
    expect(setTeams(g, 'GER', 'navy', 0)).toBe(true);
    expect(setTeams(g, 'GER', 'armour', 2)).toBe(true);
  });

  it('hold the eastern winter against an invader as they always did', () => {
    const g = history('GER');
    g.turn = DEC41;
    expect(frozen(g, 'GER', g.byName['Moscow'])).toBe(true);
    expect(frozen(g, 'SOV', g.byName['Moscow'])).toBe(false);
  });
});

describe('research', () => {
  it('begins with what each nation had when the war began', () => {
    const g = grand('GER');
    expect(g.flags['tech:open']).toBe(1);
    expect(techOf(g, 'GER')!.done).toEqual(TECH.start.GER);
    expect(techOf(g, 'TUR')!.done).toEqual([]);
    expect(teamsOf(g, 'GER')).toBe(2);
    expect(teamsOf(g, 'HUN')).toBe(1);
    // The player's teams wait for his word; every staff sets its own to work at once.
    expect(techOf(g, 'GER')!.work).toEqual([]);
    expect(idleTeams(g, 'GER')).toBe(2);
    expect(techOf(g, 'UK')!.work.length).toBe(2);
    expect(techOf(g, 'HUN')!.work.length).toBe(1);
    expect(idleTeams(g, 'UK')).toBe(0);
  });

  it('opens a technology when the ones before it are done, and takes a free team to begin', () => {
    const g = grand('GER');
    expect(whyNotResearch(g, 'GER', 'heavies')).toBe('Needs Medium tanks first.');
    expect(whyNotResearch(g, 'GER', 'tankdestroyers')).toBe('Needs Medium tanks and Anti-tank guns first.');
    expect(whyNotResearch(g, 'GER', 'tanks')).toBe('Already researched.');
    expect(whyNotResearch(g, 'GER', 'nonsense')).toBe('There is no such technology.');
    expect(whyNotResearch(g, 'GER', 'antitank')).toBeNull();
    expect(startResearch(g, 'GER', 'heavies')).toBe(false);
    expect(startResearch(g, 'GER', 'antitank')).toBe(true);
    expect(whyNotResearch(g, 'GER', 'antitank')).toBe('A team is already at work on it.');
    expect(startResearch(g, 'GER', 'sonar')).toBe(true);
    expect(whyNotResearch(g, 'GER', 'institutes')).toMatch(/Every research team is busy/);
    expect(startResearch(g, 'GER', 'institutes')).toBe(false);
    expect(techOf(g, 'GER')!.work).toEqual([{ id: 'antitank', left: 5, of: 5 }, { id: 'sonar', left: 5, of: 5 }]);
  });

  it('finishes a technology after its months, tells the player, and frees the team', () => {
    const g = grand('GER');
    startResearch(g, 'GER', 'antitank');
    const before = techMul(g, 'GER', 'infantry');
    for (let k = 0; k < 4; k++) techTick(g, 'GER');
    expect(hasTech(g, 'GER', 'antitank')).toBe(false);
    expect(techOf(g, 'GER')!.work[0].left).toBe(1);
    techTick(g, 'GER');
    expect(hasTech(g, 'GER', 'antitank')).toBe(true);
    expect(techOf(g, 'GER')!.work).toEqual([]);
    expect(techMul(g, 'GER', 'infantry')).toBeCloseTo(before + RESEARCH.fields.infantry.per, 9);
    const news = g.headlines[g.headlines.length - 1].text;
    expect(news).toMatch(/^Research complete: Anti-tank guns\. Every corps that is not armoured or motorised fights 6% harder\./);
    // Nothing is begun for the player: his teams stay idle until he says.
    techTick(g, 'GER');
    expect(techOf(g, 'GER')!.work).toEqual([]);
    expect(idleTeams(g, 'GER')).toBe(2);
  });

  it('takes longer for a technology begun before its year', () => {
    const mediums = techById('mediums')!, heavies = techById('heavies')!;
    expect(monthsFor(mediums, JAN41)).toBe(8);
    expect(monthsFor(mediums, 5)).toBe(12);
    expect(monthsFor(mediums, 1)).toBe(16);
    expect(monthsFor(heavies, 1)).toBe(28);
    expect(monthsFor(heavies, JAN42)).toBe(11);
    expect(monthsFor(heavies, JAN42 + 30)).toBe(11);
    const g = grand('GER');
    startResearch(g, 'GER', 'mediums');
    expect(techOf(g, 'GER')!.work[0]).toEqual({ id: 'mediums', left: 16, of: 16 });
  });

  it('loses the work done when a team is taken off', () => {
    const g = grand('GER');
    startResearch(g, 'GER', 'antitank');
    techTick(g, 'GER'); techTick(g, 'GER');
    expect(stopResearch(g, 'GER', 'antitank')).toBe(true);
    expect(stopResearch(g, 'GER', 'antitank')).toBe(false);
    expect(techOf(g, 'GER')!.work).toEqual([]);
    startResearch(g, 'GER', 'antitank');
    expect(techOf(g, 'GER')!.work[0].left).toBe(5);
  });

  it('adds a research team with the institutes, and another with the central laboratories', () => {
    const g = grand('GER');
    research(g, 'GER', 'institutes');
    expect(teamsOf(g, 'GER')).toBe(3);
    g.turn = JAN42;
    research(g, 'GER', 'labs');
    expect(teamsOf(g, 'GER')).toBe(4);
    for (const id of ['antitank', 'sonar', 'depth', 'mountain']) expect(startResearch(g, 'GER', id), id).toBe(true);
    expect(startResearch(g, 'GER', 'mobile')).toBe(false);
  });

  it('is steered by the player only in his own month', () => {
    const g = grand('GER');
    expect(techTree(g)!.nat).toBe('GER');
    expect(whyNotOrderResearch(g, 'heavies')).toBe('Needs Medium tanks first.');
    g.phase = g.order.indexOf('UK');
    expect(orderResearch(g, 'antitank')).toBe(false);
    g.phase = g.order.indexOf('GER');
    expect(orderResearch(g, 'antitank')).toBe(true);
    expect(orderStopResearch(g, 'antitank')).toBe(true);
    // The old design teams are closed in the grand campaign.
    expect(setTeams(g, 'GER', 'navy', 0)).toBe(false);
    g.nations.GER.pp = 500;
    expect(foundBureau(g, 'GER')).toBe(false);
  });

  it('shows the whole tree to an interface, with the state of every technology', () => {
    const g = grand('GER');
    startResearch(g, 'GER', 'antitank');
    techTick(g, 'GER');
    const v = techView(g, 'GER')!;
    expect(v.total).toBe(TECH.techs.length);
    expect(v.done).toBe(TECH.start.GER.length);
    expect(v.teams).toBe(2);
    expect(v.busy).toBe(1);
    expect(v.branches.map((b) => b.total).reduce((a, b) => a + b, 0)).toBe(v.total);
    const node = (id: string) => v.nodes.find((x) => x.id === id)!;
    expect(node('tanks').state).toBe('done');
    expect(node('antitank')).toMatchObject({ state: 'work', left: 4, of: 5 });
    expect(node('sonar')).toMatchObject({ state: 'ready', months: 5, base: 5, why: null });
    expect(node('mediums')).toMatchObject({ state: 'ready', months: 16, base: 8, year: 1941 });
    expect(node('heavies')).toMatchObject({ state: 'locked', why: 'Needs Medium tanks first.' });
    expect(node('heavies').needs).toEqual([{ id: 'mediums', name: 'Medium tanks', done: false }]);
    expect(node('mobile').needs.every((x) => x.done)).toBe(true);
    expect(node('mediums').rule).toBe('Armoured and motorised corps fight 8% harder.');
  });
});

describe('the staffs', () => {
  it('choose by their own preferences, the same way every time', () => {
    const a = grand(null, 5), b = grand(null, 5);
    expect(staffPick(a, 'GER')).toBe(staffPick(b, 'GER'));
    // Everybody wants another team first.
    for (const nat of ['GER', 'UK', 'SOV', 'HUN']) expect(techOf(a, nat)!.work.map((w) => w.id), nat).toContain('institutes');
    // Germany's second team goes to the army, Britain's to the navy.
    expect(techById(techOf(a, 'GER')!.work.find((w) => w.id !== 'institutes')!.id)!.branch).toBe('land');
    expect(techById(techOf(a, 'UK')!.work.find((w) => w.id !== 'institutes')!.id)!.branch).toBe('naval');
    // A staff does not begin what is years ahead of its time while something of its own time is open.
    for (const nat of ['GER', 'UK', 'SOV']) for (const w of techOf(a, nat)!.work) expect(w.of, `${nat} ${w.id}`).toBeLessThanOrEqual(6);
  });

  it('keep every team at work through a war, and never research a thing before what it needs', () => {
    const g = grand(null, 7);
    while (g.turn <= 40) runTurnAI(g);
    for (const n of Object.values(g.nations)) {
      const s = n.tech!;
      expect(s, n.id).toBeDefined();
      s.done.forEach((id, k) => { for (const need of techById(id)!.needs) expect(s.done.indexOf(need), `${n.id}: ${id} before ${need}`).toBeLessThan(k); });
      expect(new Set(s.done).size, n.id).toBe(s.done.length);
      if (n.alive) expect(s.work.length, n.id).toBe(Math.min(teamsOf(g, n.id), s.work.length + (staffPick(g, n.id) ? 1 : 0), teamsOf(g, n.id)));
    }
    // Forty months in, a great power still standing has more than it began with and a third team.
    for (const nat of ['UK', 'SOV', 'GER']) {
      if (!g.nations[nat].alive) continue;
      expect(techOf(g, nat)!.done.length, nat).toBeGreaterThan(TECH.start[nat].length + 8);
      expect(teamsOf(g, nat), nat).toBeGreaterThanOrEqual(3);
    }
    // The United States goes on with its research at home before it enters the war.
    expect(techOf(g, 'USA')!.done.length).toBeGreaterThan(TECH.start.USA.length + 6);
  }, 120000);

  it('raise infantry where the tank arm has not been researched', () => {
    const g = grand(null, 3);
    expect(locked(g, 'TUR', 'corps:arm')!.id).toBe('tanks');
    expect(orderCorps(g, 'TUR', 'arm', g.nations.TUR.capital)).toBeNull();
    while (g.turn <= 30) runTurnAI(g);
    for (const n of Object.values(g.nations)) {
      if (hasTech(g, n.id, 'tanks')) continue;
      expect(n.queue.some((b) => b.what === 'corps' && b.kind === 'arm'), n.id).toBe(false);
    }
  }, 120000);
});

describe('a campaign begun in a later year', () => {
  it('gives every nation the months of research in between', () => {
    const g = createGame(3, null);
    while (g.turn < JAN42) runTurnAI(g);
    expect(g.nations.GER.tech).toBeUndefined();
    startOpen(g);
    for (const nat of ['GER', 'UK', 'SOV', 'USA']) {
      const s = techOf(g, nat)!;
      expect(s.done.length, nat).toBeGreaterThan(TECH.start[nat].length + 6);
      expect(s.done, nat).toContain('institutes');
      expect(teamsOf(g, nat), nat).toBe(3);
      expect(s.work.length, nat).toBe(3);
      s.done.forEach((id, k) => { for (const need of techById(id)!.needs) expect(s.done.indexOf(need)).toBeLessThan(k); });
    }
    // Nothing years ahead of 1942 has been touched.
    expect(techOf(g, 'GER')!.done.some((id) => techById(id)!.year > 1942)).toBe(false);
    // The same war opened the same day gives the same laboratories.
    const h = createGame(3, null);
    while (h.turn < JAN42) runTurnAI(h);
    startOpen(h);
    expect(h.nations.GER.tech).toEqual(g.nations.GER.tech);
  }, 120000);

  it('does the same for a campaign saved before the tree existed', () => {
    const g = grand('GER', 4);
    for (let k = 0; k < 12; k++) runTurnAI(g);
    const d = JSON.parse(saveGame(g));
    delete d.flags['tech:open'];
    for (const n of Object.values(d.nations) as any[]) delete n.tech;
    const old = loadGame(JSON.stringify(d))!;
    const s = techOf(old, 'UK')!;
    expect(s.done.length).toBeGreaterThan(TECH.start.UK.length);
    expect(old.flags['tech:open']).toBe(1);
  }, 120000);

  it('keeps the laboratories through a save', () => {
    const g = grand('GER', 4);
    startResearch(g, 'GER', 'antitank');
    for (let k = 0; k < 8; k++) runTurnAI(g);
    const back = loadGame(saveGame(g))!;
    for (const nat of ['GER', 'UK', 'SOV', 'HUN']) expect(back.nations[nat].tech, nat).toEqual(g.nations[nat].tech);
    expect(techMul(back, 'UK', 'radar')).toBe(techMul(g, 'UK', 'radar'));
    expect(teamsOf(back, 'UK')).toBe(teamsOf(g, 'UK'));
  }, 120000);
});

describe('what a technology gives', () => {
  it('raises a branch of weapons by a level for good', () => {
    const g = grand('GER');
    expect(treeLevel(g, 'GER', 'fighters')).toBe(1);
    expect(techMul(g, 'GER', 'fighters')).toBeCloseTo(1 + RESEARCH.fields.fighters.per, 9);
    expect(techMul(g, 'GER', 'armour')).toBe(1);
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
    const before = corpsBase(g, tank);
    g.turn = JAN41;
    research(g, 'GER', 'mediums');
    expect(treeLevel(g, 'GER', 'armour')).toBe(1);
    expect(corpsBase(g, tank)).toBeCloseTo(before * (1 + RESEARCH.fields.armour.per), 6);
    g.turn = JAN42;
    research(g, 'GER', 'heavies');
    expect(techMul(g, 'GER', 'armour')).toBeCloseTo(1 + 2 * RESEARCH.fields.armour.per, 9);
  });

  it('names the technologies behind the equipment on a battle ticket', () => {
    const g = grand('GER');
    const inf = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead)!;
    const a = assess(g, [inf], g.byName['Lodz']);
    const line = a.lines.find((l) => l.label === 'Equipment')!;
    expect(line.value).toMatch(/German army doctrine: one technology researched/);
    expect(line.effect).toMatch(/Attack up 5%/);
  });

  it('holds back armoured and motorised corps, carriers and airborne troops until they are researched', () => {
    const g = grand('TUR');
    const cap = g.nations.TUR.capital;
    g.nations.TUR.pp = 300;
    const offer = (key: string) => offers(g, 'TUR', cap).find((o) => o.key === key)!;
    expect(gateOf('corps:arm')!.id).toBe('tanks');
    for (const [key, tech, name] of [['corps:arm', 'tanks', 'Tank arm'], ['corps:mot', 'motor', 'Motorised infantry'], ['ship:cv', 'carriers', 'Carrier aviation'], ['para', 'airborne', 'Airborne forces']]) {
      expect(offer(key).tech, key).toBe(tech);
      expect(offer(key).why, key).toBe(`Needs the technology ${name}. Open Research to begin it.`);
      expect(orderOffer(g, 'TUR', key, cap), key).toBeNull();
    }
    expect(offer('corps:inf').tech).toBeUndefined();
    expect(orderCorps(g, 'TUR', 'mot', cap)).toBeNull();
    research(g, 'TUR', 'motor');
    expect(offer('corps:mot').tech).toBeUndefined();
    expect(locked(g, 'TUR', 'corps:mot')).toBeNull();
    // Germany began the war with all three; Britain with carriers.
    const h = grand('GER');
    for (const key of ['corps:arm', 'corps:mot', 'para']) expect(locked(h, 'GER', key), key).toBeNull();
    expect(locked(h, 'GER', 'ship:cv')!.name).toBe('Carrier aviation');
    expect(locked(h, 'UK', 'ship:cv')).toBeNull();
    expect(locked(h, 'UK', 'para')!.name).toBe('Airborne forces');
  });

  it('raises airborne corps, two at a time, that go in by air', () => {
    const g = grand('GER');
    const cap = g.nations.GER.capital;
    g.nations.GER.pp = 300;
    expect(whyNoPara(g, 'GER')).toBeNull();
    expect(whyNoPara(g, 'ITA')).toBe('Needs the technology Airborne forces.');
    const first = orderOffer(g, 'GER', 'para', cap)!;
    expect(first).toMatchObject({ what: 'corps', kind: 'inf', divs: TECH.units.para.divs, airborne: true, name: 'I Airborne Corps', cost: TECH.units.para.cost });
    expect(g.nations.GER.pp).toBe(300 - TECH.units.para.cost);
    const second = orderOffer(g, 'GER', 'para', cap)!;
    expect(second.name).toBe('II Airborne Corps');
    expect(paraCount(g, 'GER')).toBe(2);
    expect(whyNoPara(g, 'GER')).toMatch(/2 airborne corps at once/);
    expect(orderOffer(g, 'GER', 'para', cap)).toBeNull();
    // Taken back in the month it was ordered, the place and the number are free again.
    expect(cancelBuild(g, 'GER', second.id)).toBe(true);
    expect(paraCount(g, 'GER')).toBe(1);
    expect(orderOffer(g, 'GER', 'para', cap)!.name).toBe('II Airborne Corps');
    g.turn += TECH.units.para.months;
    deliver(g, 'GER');
    const drop = g.corps.filter((c) => c.nat === 'GER' && c.airborne && !c.dead);
    expect(drop.map((c) => c.name).sort()).toEqual(['I Airborne Corps', 'II Airborne Corps']);
    expect(drop.every((c) => c.kind === 'inf' && c.divs === TECH.units.para.divs)).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(paraCount(back, 'GER')).toBe(2);
  });

  it('counts in battle the way a national ability does', () => {
    const g = grand('GER');
    const lodz = g.byName['Lodz'];
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
    const inf = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead)!;
    const tankBefore = traitAttack(g, tank, lodz), infBefore = traitAttack(g, inf, lodz);
    g.turn = 5;
    research(g, 'GER', 'mobile');
    expect(techTraits(g, 'GER').map((t) => t.name)).toContain('Mobile warfare');
    expect(traitAttack(g, tank, lodz)).toBeCloseTo(tankBefore * 1.1, 9);
    expect(traitAttack(g, inf, lodz)).toBeCloseTo(infBefore, 9);
    const line = assess(g, [tank], lodz).lines.find((l) => l.label === 'Abilities and technology')!;
    expect(line.value).toMatch(/Mobile warfare/);
    // Landing craft: a harder landing, and shipping for one more corps, counted once.
    const lift = g.nations.GER.sealift;
    const beach = traitAttack(g, inf, lodz, true);
    g.turn = JAN41;
    research(g, 'GER', 'landingcraft');
    expect(g.nations.GER.sealift).toBe(lift + 1);
    expect(traitAttack(g, inf, lodz, true)).toBeCloseTo(beach * 1.15, 9);
    expect(traitAttack(g, inf, lodz)).toBeCloseTo(infBefore, 9);
    techTick(g, 'GER');
    expect(g.nations.GER.sealift).toBe(lift + 1);
  });

  it('takes its discounts off the price and speeds the drafts', () => {
    const g = grand('ITA');
    const arm = corpsCost(g, 'ITA', 'arm'), ftr = priceOf(g, 'ITA', 'air:ftr', 12), drafts = traitDrafts(g, 'ITA');
    g.turn = 5;
    research(g, 'ITA', 'machinetools');
    research(g, 'ITA', 'assembly');
    expect(corpsCost(g, 'ITA', 'arm')).toBeCloseTo(Math.round(arm * 0.9 * 10) / 10, 9);
    expect(priceOf(g, 'ITA', 'air:ftr', 12)).toBeCloseTo(Math.round(ftr * 0.9 * 10) / 10, 9);
    const inf = corpsCost(g, 'ITA', 'inf');
    research(g, 'ITA', 'mobilise');
    expect(traitDrafts(g, 'ITA')).toBeCloseTo(drafts * techById('mobilise')!.drafts!, 9);
    // Infantry is no cheaper for any of it.
    expect(corpsCost(g, 'ITA', 'inf')).toBe(inf);
  });

  it('equips an army for the eastern winter', () => {
    const g = grand('GER');
    g.turn = DEC41;
    const moscow = g.byName['Moscow'];
    expect(frozen(g, 'GER', moscow)).toBe(true);
    // The Soviet Union and Finland begin the war with it.
    expect(winterReady(g, 'SOV')).toBe(true);
    expect(winterReady(g, 'FIN')).toBe(true);
    expect(winterReady(g, 'GER')).toBe(false);
    research(g, 'GER', 'winterkit');
    expect(winterReady(g, 'GER')).toBe(true);
    expect(frozen(g, 'GER', moscow)).toBe(false);
    expect(frozen(g, 'ITA', moscow)).toBe(true);
  });
});
