// Version 59: a campaign with the war cabinet switched off.
// usage: probe33 ME FROM SEED UNTIL PLAN-JSON [ON-AGAIN-TURN]
// PLAN-JSON carries "noCabinet":true for the war under test; the same plan without it is the war to compare with.
// Each month: is the switch still off (cabinetIsOff true, governmentOf null); any headline that still speaks of the
// cabinet, a directive, a minister, a decree or the cabinet's trust (LEAK lines); in month 20 a save and a load must keep the switch.
// Every six months: provinces, divisions, net income, treasury, political capital, and with the cabinet on its trust.
// With ON-AGAIN-TURN the cabinet is switched on again in that month: its state is printed then and each six months after.
import { campaign, date, T, ledger, saveGame, loadGame } from './lib';
const [me, fromS, seedS, untilS, planS, onS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5,"noCabinet":true}');
const onAgain = onS ? Number(onS) : 0;
const X = T as any;
let seen = 0, leaks = 0, offMonths = 0, onMonths = 0, wrong = 0, lastTurn = 0, sumNet = 0, months = 0, capSum = 0, saveOk = '-';
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g: any) => {
  lastTurn = g.turn; months++;
  if (onAgain && g.turn === onAgain) { const ok = X.orderCabinet(g, true); const v = T.governmentOf(g) as any; console.log(`${date(g.turn)}  SWITCHED ON AGAIN: ${ok}; trust ${v?.trust}, seats ${v?.seats?.map((s: any) => s.minister.id).join(',')}, next directive in ${v?.nextIn} months, directive now ${v?.directive ? v.directive.title : 'none'}`); }
  const off = X.cabinetIsOff ? X.cabinetIsOff(g) : false, v = T.governmentOf(g) as any;
  if (off) { offMonths++; if (v !== null) { wrong++; console.log(`${date(g.turn)}  WRONG: the cabinet is off but governmentOf is not null`); } } else onMonths++;
  for (; seen < g.headlines.length; seen++) {
    const h = g.headlines[seen];
    if (off && (!h.nat || h.nat === me) && /cabinet|directive|minister|decree|\btrust\b/i.test(h.text)) { leaks++; console.log(`   LEAK ${date(h.turn)} [${h.kind}] ${h.text}`); }
  }
  if (g.turn === 20) { const s = saveGame(g); const g2 = loadGame(s) as any; saveOk = String((X.cabinetIsOff ? X.cabinetIsOff(g2) : false) === off && (T.governmentOf(g2) === null) === (v === null)); }
  const L = ledger(g, me) as any; sumNet += L.net * L.scale; capSum += g.nations[me].pc;
  if ((g.turn - 1) % 6 === 0) console.log(`${date(g.turn).padEnd(15)} cabinet ${off ? 'off' : 'on'}${v ? ` (trust ${v.trust}${v.doubts ? ', doubts' : ''}${v.directive ? ', directive: ' + v.directive.title : ''})` : ''}; provinces ${g.regions.filter((r: any) => r.ctrl === me).length}, divisions ${Math.round(g.corps.filter((c: any) => c.nat === me && !c.dead).reduce((s: number, c: any) => s + c.divs, 0))}, net ${Math.round(L.net * L.scale)}, gross ${Math.round(L.gross * L.scale)}, treasury ${Math.round(L.treasury * L.scale)}, political capital ${g.nations[me].pc.toFixed(1)}`);
} }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
console.log(`TOTAL ${me} seed ${seedS} ${plan.noCabinet ? 'CABINET OFF' : 'cabinet on'}${onAgain ? ` (on again ${date(onAgain)})` : ''}: played to ${date(lastTurn)}${run.fellOn ? ` (fell ${run.fellOn})` : ''}; months off ${offMonths}, on ${onMonths}; wrong ${wrong}; leaks ${leaks}; save kept the switch: ${saveOk}; mean net income ${Math.round(sumNet / Math.max(1, months))}; mean political capital ${(capSum / Math.max(1, months)).toFixed(1)}; provinces at the end ${run.g.regions.filter((r: any) => r.ctrl === me).length}`);
