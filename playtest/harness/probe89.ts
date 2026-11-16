// Version 83: the film of the eastern front (Brian, 2026-11-16: "how about russian army i dont hear much about them").
// How the Red Army fights: as the computer in a war nobody plays, against a German player by hand, and from the
// Soviet seat by hand. The probe only reads; in the hand modes the lib's scripted player gives the orders.
// usage: probe89 SEEDS(a,b,c) [UNTIL=80] [ME=none|GER|SOV] [PLAN-JSON]     (build with build70.sh; TC_DIAL sets the stop)
// Lines, each with "seed N":
//   F     one a month once the Soviet Union is at war with Germany: Soviet corps (and those at the front), the enemy's
//         corps at the front, home provinces lost, provinces held beyond the line of the war's first month, how far east
//         the enemy stands and how far west the Red Army, the month's attacks each way (ordered, won, mean odds), corps
//         dead each side, prisoners each way
//   CITY  when each town fell and came back
//   ARMY  each January: corps by kind, divisions, mean strength and experience, for the Soviet Union and Germany
//   PRE   what the Soviet Union did before the war with Germany (Finland, the Baltic states, eastern Poland, Bessarabia)
//   EAST  the war in one line: the opening, the deepest advance, the turn of the tide, the border, German soil, Berlin
//   RAIL  the Soviet staff and its railways: engineer corps, cut lines on ground it holds each half year, relays by year,
//         how long a cut stood on its ground, corps out of supply
//   MEN   months each great power's reserve of men stood at nothing while it was at war
//   RING  each month the Red Army or its enemy lost three corps or more in the east, with the news of it
import { start, date, T, atWar, campaign } from './lib';
import { railCut, onNet, cutCause } from '@tc/campaign/rails';
import { menLeft } from '@tc/campaign/manpower';
const [seedsS = '17', untilS = '80', meS = 'none', planS = '{}'] = process.argv.slice(2);
const UNTIL = Number(untilS), ME = meS === 'none' ? null : meS;
const CITIES = ['Minsk', 'Kiev', 'Smolensk', 'Leningrad', 'Moscow', 'Kharkov', 'Rostov', 'Stalingrad', 'Crimea', 'Odessa', 'Latvia', 'Warsaw', 'Bucharest', 'Budapest', 'Konigsberg', 'Vienna', 'Berlin'];
const mean = (a: number[]): number => (a.length ? a.reduce((s, v) => s + v, 0) / a.length : 0);

function film(seed: number) {
  let g: any = null;
  let home = new Set<number>(), line = new Set<number>(), warT = 0, struck = '', lastRep = 0, lastHead = 0;
  const alive = new Map<number, { nat: string; reg: number }>();
  const city: Record<string, { id: number; was: string; log: string[] }> = {};
  const F: any[] = []; const pre: string[] = []; const rings: string[] = []; const army: string[] = [];
  let gerFell = 0, berlin = '', seenFin = false; const took = new Set<string>();
  const cutSince = new Map<number, { t: number; why: string }>(); const relays: { t: number; months: number; why: string }[] = []; const newCuts: Record<string, number> = {};
  const POW = ['GER', 'SOV', 'UK', 'FRA', 'ITA', 'USA']; const men: Record<string, { war: number; zero: number; first: number }> = {};
  const foe = (n: string): boolean => n !== 'SOV' && !!g.nations[n] && atWar(g, 'SOV', n);
  const open = (game: any): void => {
    g = game; home = new Set(g.regions.filter((r: any) => r.owner === 'SOV').map((r: any) => r.id));
    for (const n of CITIES) { const r = g.regions.find((x: any) => x.name === n); if (r) city[n] = { id: r.id, was: r.ctrl, log: [] }; }
    for (const c of g.corps) if (!c.dead) alive.set(c.id, { nat: c.nat, reg: c.reg });
  };
  const armyLine = (): void => {
    const one = (n: string): string => {
      const cs = g.corps.filter((c: any) => c.nat === n && !c.dead); if (!cs.length) return `${n} none`;
      const k: Record<string, number> = {}; for (const c of cs) k[c.kind] = (k[c.kind] ?? 0) + 1;
      return `${n} ${cs.length} corps (${Object.entries(k).sort((a, b) => b[1] - a[1]).map(([a, b]) => `${a} ${b}`).join(', ')}), ${cs.reduce((s: number, c: any) => s + c.divs, 0)} divisions, strength ${Math.round(mean(cs.map((c: any) => c.str)) * 100)}%, experience ${Math.round(mean(cs.map((c: any) => c.xp)) * 100)}`;
    };
    army.push(`ARMY seed ${seed}: ${date(g.turn)}: ${one('SOV')}; ${one('GER')}`);
  };
  // Called once a month, after the month t has been played by everybody.
  const step = (t: number): void => {
    if (!g) return;
    // Before the war: what the Soviet Union took.
    if (!seenFin && g.nations.FIN && atWar(g, 'SOV', 'FIN')) { seenFin = true; pre.push(`war on Finland ${date(t)}`); }
    for (const r of g.regions) if (r.ctrl === 'SOV' && r.owner !== 'SOV' && !home.has(r.id) && !warT) { const key = r.owner; if (!took.has(key)) { took.add(key); pre.push(`${r.owner} ground from ${date(t)} (${r.name})`); } }
    if (!warT && g.nations.GER?.alive && atWar(g, 'SOV', 'GER')) {
      warT = t; line = new Set(g.regions.filter((r: any) => r.ctrl === 'SOV').map((r: any) => r.id));
      // the line of the war's first month is taken from control as the month before stood: add what fell this month
      for (const r of g.regions) if (r.prev === 'SOV' && r.since >= t && r.ctrl !== 'SOV') line.add(r.id);
      const first = g.reports.find((b: any) => b.turn >= t - 1 && ((b.attNat === 'GER' && b.defNat === 'SOV') || (b.attNat === 'SOV' && b.defNat === 'GER')));
      struck = first ? first.attNat : '?';
    }
    // battles of the month
    let sa = 0, sw = 0, aa = 0, aw = 0, sp = 0, ap = 0; const so: number[] = [], ao: number[] = [];
    for (let i = lastRep; i < g.reports.length; i++) {
      const b = g.reports[i]; const won = !!(b.taken || b.siege || b.gates);
      if (b.attNat === 'SOV' && b.defNat !== 'SOV') { sa++; if (won) sw++; so.push(b.odds); sp += b.prisoners ?? 0; }
      else if (b.defNat === 'SOV') { aa++; if (won) aw++; ao.push(b.odds); ap += b.prisoners ?? 0; }
    }
    lastRep = g.reports.length;
    // the dead of the month
    let sd = 0, ad = 0; const now = new Set<number>();
    for (const c of g.corps) { if (c.dead) continue; now.add(c.id); }
    for (const [id, v] of alive) if (!now.has(id)) { if (v.nat === 'SOV') sd++; else if (warT && (v.nat === 'GER' || foe(v.nat) || ['ROM', 'HUN', 'FIN', 'ITA', 'SVK', 'BUL'].includes(v.nat)) && (g.regions[v.reg]?.x ?? 0) >= 690) ad++; alive.delete(id); }
    for (const c of g.corps) if (!c.dead) alive.set(c.id, { nat: c.nat, reg: c.reg });
    // news of the month about rings
    const news: string[] = [];
    for (let i = lastHead; i < g.headlines.length; i++) { const h = g.headlines[i]; if (/surrender|ring closes|cut off|lays down|encircl|pocket/i.test(h.text) && /Soviet|German|Romanian|Hungarian|Finnish|Italian/i.test(h.text)) news.push(h.text); }
    lastHead = g.headlines.length;
    if (warT && (sd >= 3 || ad >= 3)) rings.push(`RING seed ${seed}: ${date(t)}: Soviet corps dead ${sd}, enemy corps dead in the east ${ad} | ${news.slice(0, 4).join(' | ').slice(0, 420)}`);
    // railways on ground the Soviet Union holds: new cuts, relays, what stands cut
    let cutNow = 0, relaidNow = 0;
    for (const r of g.regions) {
      if (!onNet(r.id)) continue; const cut = railCut(g, r.id), was = cutSince.get(r.id);
      if (cut && r.ctrl === 'SOV') { cutNow++; if (!was) { const why = String(cutCause(g, r.id) ?? '?'); cutSince.set(r.id, { t, why }); newCuts[why] = (newCuts[why] ?? 0) + 1; } }
      else if (was) { if (!cut && r.ctrl === 'SOV') { relays.push({ t, months: t - was.t, why: was.why }); relaidNow++; } cutSince.delete(r.id); }
    }
    for (const n of POW) { const N = g.nations[n]; if (!N?.alive || !N.atWar?.length) continue; const m = (men[n] ??= { war: 0, zero: 0, first: 0 }); m.war++; const left = menLeft(g, n); if (Number.isFinite(left) && left < 1) { m.zero++; if (!m.first) m.first = t; } }
    // towns
    for (const n of Object.keys(city)) { const c = city[n], r = g.regions[c.id]; if (r.ctrl !== c.was) { c.log.push(`${r.ctrl} ${date(t)}`); c.was = r.ctrl; if (n === 'Berlin' && !berlin && r.ctrl !== 'GER') berlin = `${r.ctrl} ${date(t)}`; } }
    if (!gerFell && g.nations.GER && !g.nations.GER.alive) gerFell = t;
    if (t % 12 === 5) armyLine();
    if (!warT) return;
    // the front
    const S = (r: any): boolean => r.ctrl === 'SOV', E = (r: any): boolean => foe(r.ctrl);
    const fs = new Set<number>(), fe = new Set<number>();
    for (const r of g.regions) if (S(r)) for (const j of r.nb) { const o = g.regions[j]; if (E(o)) { fs.add(r.id); fe.add(o.id); } }
    const wide = (set: Set<number>, ok: (r: any) => boolean): Set<number> => { const w = new Set(set); for (const id of set) for (const j of g.regions[id].nb) if (ok(g.regions[j])) w.add(j); return w; };
    const ws = wide(fs, S), we = wide(fe, E);
    const count = (set: Set<number>, ok: (c: any) => boolean): number => { let n = 0; for (const id of set) for (const c of g.regions[id].units ?? []) if (!c.dead && ok(c)) n++; return n; };
    const sov = g.corps.filter((c: any) => c.nat === 'SOV' && !c.dead);
    const lostHome = [...home].filter((id) => E(g.regions[id])).length;
    const lostLine = [...line].filter((id) => !S(g.regions[id])).length;
    const beyond = g.regions.filter((r: any) => S(r) && !line.has(r.id) && !r.overseas).length;
    const eastX = Math.max(0, ...[...line].filter((id) => E(g.regions[id])).map((id) => g.regions[id].x));
    const westX = Math.min(9999, ...g.regions.filter((r: any) => S(r) && !r.overseas && fs.has(r.id)).map((r: any) => r.x));
    const ger = g.corps.filter((c: any) => c.nat === 'GER' && !c.dead).length;
    F.push({ t, sov: sov.length, sovDivs: sov.reduce((s: number, c: any) => s + c.divs, 0), sf: count(ws, (c) => c.nat === 'SOV'), ef: count(we, (c) => foe(c.nat)), ger, lostHome, lostLine, beyond, eastX: Math.round(eastX), westX: westX === 9999 ? 0 : Math.round(westX), sa, sw, so: mean(so), aa, aw, ao: mean(ao), sd, ad, sp, ap, len: fs.size, cutNow, relaidNow, eng: sov.filter((c: any) => c.kind === 'eng').length, engJob: sov.filter((c: any) => c.kind === 'eng' && c.job?.what === 'rail').length, unsup: sov.filter((c: any) => !c.supplied).length });
  };
  const close = (err: string): void => {
    const nearest = (x: number): string => { let best = '', d = 1e9; for (const n of ['Berlin', 'Warsaw', 'Minsk', 'Kiev', 'Smolensk', 'Moscow', 'Stalingrad']) { const r = g.regions[city[n]?.id]; if (r && Math.abs(r.x - x) < d) { d = Math.abs(r.x - x); best = n; } } return best; };
    for (const f of F) console.log(`F seed ${seed} t${f.t} ${date(f.t)}: Soviet ${f.sov} corps ${f.sovDivs} divs (front ${f.sf}), enemy front ${f.ef}, German corps ${f.ger}, home lost ${f.lostHome}, line lost ${f.lostLine}, beyond ${f.beyond}, enemy east to ${f.eastX}, Soviet west to ${f.westX}, front ${f.len} provinces | Soviet attacks ${f.sa} won ${f.sw} odds ${f.so.toFixed(2)} | enemy attacks ${f.aa} won ${f.aw} odds ${f.ao.toFixed(2)} | dead Soviet ${f.sd} enemy ${f.ad} | prisoners taken by Soviet ${f.sp} from Soviet ${f.ap} | rail: cut ${f.cutNow} on Soviet-held ground, relaid ${f.relaidNow}, engineers ${f.eng} (${f.engJob} on a line), unsupplied ${f.unsup}`);
    for (const a of army) console.log(a);
    for (const r of rings) console.log(r);
    const ry = [1941, 1942, 1943, 1944, 1945].map((n) => { const m = relays.filter((r) => Math.floor((r.t + 7) / 12) + 1939 === n); const fm = F.filter((f) => Math.floor((f.t + 7) / 12) + 1939 === n); return fm.length ? `${n}: relaid ${m.filter((r) => r.why !== 'hasty').length}, cut at the year's worst ${Math.max(...fm.map((f) => f.cutNow))}, engineers ${Math.max(...fm.map((f) => f.eng))}, unsupplied a month ${(fm.reduce((a, f) => a + f.unsup, 0) / fm.length).toFixed(1)}` : ''; }).filter(Boolean);
    const eng = relays.filter((r) => r.why !== 'hasty');
    console.log(`RAIL seed ${seed}: ${ry.join(' | ')} | new cuts on Soviet-held ground by cause: ${Object.entries(newCuts).map(([k, v]) => `${k} ${v}`).join(', ') || 'none'} | relaid by engineers ${eng.length}, after ${eng.length ? (eng.reduce((a, r) => a + r.months, 0) / eng.length).toFixed(1) : '-'} months on average (longest ${eng.length ? Math.max(...eng.map((r) => r.months)) : '-'}) | hasty cuts made good by time ${relays.length - eng.length} | still cut at the end ${cutSince.size}`);
    console.log(`MEN seed ${seed}: ${POW.filter((n) => men[n]).map((n) => `${n} ${men[n].zero} of ${men[n].war} months at war${men[n].first ? ` (first ${date(men[n].first)})` : ''}`).join('; ')}`);
    console.log(`PRE seed ${seed}: ${pre.join('; ') || 'nothing'}`);
    console.log(`CITY seed ${seed}: ${Object.keys(city).map((n) => `${n}: ${city[n].log.join(', ') || 'never changed hands'}`).join(' | ')}`);
    if (!warT) { console.log(`EAST seed ${seed}: no war between Germany and the Soviet Union by ${date(g.turn)}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}${err}`); return; }
    const deep = F.reduce((a, b) => (b.lostLine > a.lostLine ? b : a), F[0]);
    const back = F.find((f) => f.t > deep.t && f.lostHome === 0), all = F.find((f) => f.t > deep.t && f.lostLine === 0);
    const first = F[0], y = (n: number): any[] => F.filter((f) => Math.floor((f.t + 7) / 12) + 1939 === n);
    const years = [1941, 1942, 1943, 1944, 1945].map((n) => { const m = y(n); if (!m.length) return ''; const s = (k: string): number => m.reduce((a, f) => a + f[k], 0); return `${n}: Soviet attacks ${s('sa')} won ${s('sw')}, enemy attacks ${s('aa')} won ${s('aw')}, dead Soviet ${s('sd')} enemy ${s('ad')}`; }).filter(Boolean);
    console.log(`EAST seed ${seed}: war from ${date(warT)} struck by ${struck} | at the start Soviet ${first.sov} corps, German ${first.ger} | deepest ${date(deep.t)}: ${deep.lostLine} provinces of the line lost (${deep.lostHome} of ${home.size} home), east to ${deep.eastX} (${nearest(deep.eastX)}) | home ground whole again ${back ? date(back.t) : 'never'} | the line whole again ${all ? date(all.t) : 'never'} | on German home ground ${soilT ? date(soilT) : 'never'} | Berlin ${berlin || 'never taken'} | Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'} | Moscow ${city.Moscow?.log.join(', ') || 'never lost'} | Leningrad ${city.Leningrad?.log.join(', ') || 'never lost'} | Stalingrad ${city.Stalingrad?.log.join(', ') || 'never lost'}${err}`);
    console.log(`YEARS seed ${seed}: ${years.join(' | ')}`);
  };
  let soilT = 0;
  const soil = (t: number): void => { if (!soilT && g && g.regions.some((r: any) => r.owner === 'GER' && !r.overseas && r.ctrl === 'SOV')) soilT = t; };
  return { open, step: (t: number) => { step(t); soil(t); }, close, fell: () => gerFell };
}

for (const seed of seedsS.split(',').map(Number)) {
  const f = film(seed); let err = '';
  if (!ME) {
    const g: any = start(null, 1, seed, 'colonel', Number(process.env.TC_DIAL ?? 3)); f.open(g);
    try { while (g.turn <= UNTIL) { const t = g.turn; T.runTurnAI(g); f.step(t); if (f.fell() && t >= f.fell() + 3) break; } }
    catch (e: any) { err = ` | ERROR in ${date(g.turn)}: ${String(e?.stack ?? e).split('\n').slice(0, 3).join(' / ')}`; }
    f.close(err);
  } else {
    const extra = JSON.parse(planS);
    const GERDEC = [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']];
    const plan: any = { minOdds: ME === 'GER' ? 1.4 : 1.5, land: true, buy: ['corps:arm', 'corps:inf', 'corps:mot'], ...(ME === 'GER' ? { declare: GERDEC } : {}), ...extra };
    let opened = false, lastT = 0;
    plan.first = (g: any): void => { if (!opened) { f.open(g); opened = true; } };
    try {
      const run: any = campaign(ME, 1, seed, plan, { until: UNTIL, level: process.env.TC_LEVEL || 'colonel', quiet: true, onMonth: (run: any, row: any) => { const t = row?.turn ?? run.g.turn - 1; if (t !== lastT) { f.step(t); lastT = t; } } });
      const g = run.g; const me = g.nations[ME];
      console.log(`SEAT seed ${seed}: ${ME} by hand ${me.alive ? `stands at ${date(g.turn)}` : `fell ${date(g.turn)}`}; provinces ${g.regions.filter((r: any) => r.ctrl === ME).length}`);
    } catch (e: any) { err = ` | ERROR: ${String(e?.stack ?? e).split('\n').slice(0, 3).join(' / ')}`; }
    f.close(err);
  }
}
