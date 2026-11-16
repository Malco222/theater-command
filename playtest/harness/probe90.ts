// probe90: how Leningrad falls. One war nobody plays for each seed (TC_DIAL sets the stop, default 3).
//   node out/probe90.mjs SEEDS(a,b,c) [UNTIL=80] [TOWN=Leningrad]
// L lines: each month the enemy stands in or beside the province. FELL: the month it changes hands, how, and what
// held it the month before. SIEGE: the whole story in one line.
import { start, date, T, atWar } from './lib';
import { besieged, garrisonOf, guardOf, monthsShut, storesOf, fedBySea, ringClosed, breachOf, besiegers } from '@tc/campaign/siege';
import { townSize } from '@tc/campaign/towns';

const until = Number(process.argv[3] ?? 80), TOWN = process.argv[4] ?? 'Leningrad', dial = Number(process.env.TC_DIAL ?? 3);
const f2 = (x: number) => x.toFixed(2);
for (const seed of process.argv[2].split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', dial);
  const r = g.regions.find((x: any) => x.name === TOWN);
  if (!r) { console.log(`no province ${TOWN}`); break; }
  const home = r.owner;
  let seenR = 0, prev: any = null, shutMonths = 0, sieges = 0, wasBesieged = false, storms = 0, stormsWon = 0, sorties = 0, reliefs = 0, firstSiege = 0;
  const falls: string[] = [], backs: string[] = [];
  let ctrl = r.ctrl, fellAt = 0;
  while (g.turn <= until) {
    const t = g.turn;
    try { T.runTurnAI(g); } catch (e: any) { console.log(`ERR seed ${seed} ${date(t)}: ${e.message}`); break; }
    const reps = g.reports.slice(seenR).filter((b: any) => b.region === r.id); seenR = g.reports.length;
    const news = (g.headlines ?? []).filter((h: any) => h.turn === t && h.text.includes(TOWN)).map((h: any) => h.text);
    const foes = (x: any) => (x.units ?? []).filter((c: any) => !c.dead && atWar(g, c.nat, home));
    const ours = (x: any) => (x.units ?? []).filter((c: any) => !c.dead && c.nat === home);
    const garr = garrisonOf(r).filter((c: any) => !c.dead);
    const out = besiegers(g, r);
    const nbF = r.nb.reduce((n: number, j: number) => n + (g.regions[j].ctrl !== home && atWar(g, g.regions[j].ctrl, home) ? 1 : 0), 0);
    const nbFoes = r.nb.reduce((n: number, j: number) => n + foes(g.regions[j]).length, 0), nbOurs = r.nb.reduce((n: number, j: number) => n + ours(g.regions[j]).length, 0);
    const now = {
      t, ctrl: r.ctrl, bes: besieged(g, r), garr: garr.length, gdivs: garr.reduce((n: number, c: any) => n + c.divs, 0), gstr: garr.length ? garr.reduce((n: number, c: any) => n + c.str, 0) / garr.length : 0,
      gorg: garr.length ? garr.reduce((n: number, c: any) => n + c.org, 0) / garr.length : 0, guard: guardOf(g, r), field: r.ctrl === home ? ours(r).length : 0,
      out: out.length, odivs: out.reduce((n: number, c: any) => n + c.divs, 0), onat: [...new Set(out.map((c: any) => c.nat))].join('+'),
      shut: monthsShut(g, r), stores: storesOf(g, r), sea: fedBySea(g, r), ring: ringClosed(g, r), breach: breachOf(g, r), nbF, nbN: r.nb.length, nbFoes, nbOurs,
    };
    for (const b of reps) {
      if (b.town) { storms++; if (b.taken) stormsWon++; }
      if (b.sally) sorties++;
      if (b.relief) reliefs++;
    }
    const rs = reps.map((b: any) => `${b.attNat}>${b.defNat} ${f2(b.odds)}${b.town ? ' STORM' : ''}${b.siege ? ' field won, town holds' : ''}${b.gates ? ' gates' : ''}${b.relief ? ' relief' : ''}${b.sally ? ' sortie' : ''}${b.taken ? ' taken' : ''} (${b.attackers.length}v${b.defenders.length})`).join('; ');
    if (now.bes) { shutMonths++; if (!wasBesieged) { sieges++; if (!firstSiege) firstSiege = t; } }
    wasBesieged = now.bes;
    if (r.ctrl !== ctrl || reps.length || news.length || (r.ctrl === home && (now.out || nbF)))
      console.log(`L seed ${seed} ${date(t)}: held by ${now.ctrl}${now.bes ? ' BESIEGED' : ''} | garrison ${now.garr} corps ${now.gdivs} divs str ${f2(now.gstr)} org ${f2(now.gorg)}, guard ${f2(now.guard)}, field ${now.field} | outside ${now.out} corps ${now.odivs} divs ${now.onat} | shut ${now.shut} of stores ${now.stores}, by sea ${now.sea ? 'fed' : 'no'}, ring ${now.ring ? 'closed' : 'open'}, breach ${f2(now.breach)} | roads cut ${nbF} of ${now.nbN}, next door foes ${nbFoes} ours ${nbOurs}${rs ? ' | ' + rs : ''}${news.length ? ' | NEWS ' + news.join(' / ') : ''}`);
    if (r.ctrl !== ctrl) {
      if (ctrl === home) {
        const storm = reps.find((b: any) => b.town && b.taken), field = reps.find((b: any) => !b.town && b.taken);
        const how = storm ? 'STORMED' : news.some((x: string) => x.startsWith('Starved out')) ? 'STARVED' : news.some((x: string) => x.includes('opens its gates')) ? 'GATES' : field ? (prev && prev.bes ? 'EMPTY (nobody left to hold it)' : 'WALKED IN (the field was won and nobody held the town)') : 'EMPTY (nobody left to hold it)';
        const p = prev ?? now;
        const line = `FELL seed ${seed}: ${TOWN} falls ${date(t)} to ${r.ctrl}: ${how}${storm ? ` at ${f2(storm.odds)} by ${storm.attackers.length} corps on ${storm.defenders.length}` : ''} | the month before: ${p.bes ? 'besieged' : 'not besieged'}, garrison ${p.garr} corps ${p.gdivs} divs str ${f2(p.gstr)} org ${f2(p.gorg)}, guard ${f2(p.guard)}, field ${p.field}, outside ${p.out} corps ${p.odivs} divs, shut ${p.shut} of ${p.stores}, by sea ${p.sea ? 'fed' : 'no'}, ring ${p.ring ? 'closed' : 'open'}, breach ${f2(p.breach)}, roads cut ${p.nbF} of ${p.nbN}, next door ours ${p.nbOurs} foes ${p.nbFoes} | so far: besieged ${shutMonths} months, storms ${storms}, sorties ${sorties}, reliefs ${reliefs}`;
        console.log(line); falls.push(`${date(t)} ${how.split(' ')[0]}`); if (!fellAt) fellAt = t;
      } else if (r.ctrl === home) { console.log(`BACK seed ${seed}: ${TOWN} back ${date(t)}`); backs.push(date(t)); }
      ctrl = r.ctrl;
    }
    prev = now;
    if (!g.nations.GER?.alive && t > 30) break;
  }
  console.log(`SIEGE seed ${seed}: ${TOWN} (${townSize(g, r)}${r.fort ? ', fort ' + r.fort.lv : ''}${r.port ? ', port' : ''}) besieged ${shutMonths} months in ${sieges} sieges${firstSiege ? ' from ' + date(firstSiege) : ''}, storms ${storms} (won ${stormsWon}), sorties ${sorties}, reliefs ${reliefs} | fell ${falls.length ? falls.join(', ') : 'never'} | back ${backs.length ? backs.join(', ') : falls.length ? 'never' : '-'}`);
}
