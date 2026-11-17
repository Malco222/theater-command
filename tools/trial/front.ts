// TRIAL (the Pacific seat builder's reading, never for main). The war in China with nobody playing: what moved, month by month.
//   node dist/trial/front.mjs [SEED] [MONTHS] [A] [B]     default 7 24 CHN JAP
import { T, date, start, type Game } from '@pt/lib';
const out = (s = ''): void => { console.log(s); };
const r1 = (x: number): string => (Math.round(x * 10) / 10).toString();
const args = process.argv.slice(2);
const seed = Number(args[0] ?? 7), months = Number(args[1] ?? 24), A = args[2] ?? 'CHN', B = args[3] ?? 'JAP';
const g: Game = start(null, 1, seed);
const live = (nat: string) => g.corps.filter((c) => c.nat === nat && !c.dead);
const divs = (nat: string) => live(nat).reduce((s, c) => s + c.divs * c.str, 0);
const onA = (nat: string) => live(nat).filter((c) => g.regions[c.reg].owner === A);
const stand = () => `${A} holds ${g.regions.filter((r) => r.ctrl === A).length} (of its own ${g.regions.filter((r) => r.owner === A && r.ctrl === A).length}/${g.regions.filter((r) => r.owner === A).length}), ${live(A).length} corps ${r1(divs(A))} divs; ${B} ${live(B).length} corps ${r1(divs(B))} divs, on ${A}'s ground ${onA(B).length} corps ${r1(onA(B).reduce((s, c) => s + c.divs * c.str, 0))} divs, at home ${live(B).filter((c) => g.regions[c.reg].owner === B).length}`;
out(`=== the front, seed ${seed}, ${months} months, ${A} against ${B}, nobody playing`);
out(`${date(g.turn)} (first day): ${stand()}`);
out(`  ${B} on ${A}'s ground: ${onA(B).map((c) => `${c.name} ${c.divs}x${Math.round(c.str * 100)}% at ${g.regions[c.reg].name}`).join('; ')}`);
while (g.turn <= months && g.nations[A].alive) {
  const ctrl = g.regions.map((r) => r.ctrl), h0 = g.headlines.length;
  const before = new Map(g.corps.filter((c) => !c.dead && (c.nat === A || c.nat === B)).map((c) => [c.id, { name: c.name, nat: c.nat, reg: c.reg, str: c.str }]));
  const when = date(g.turn);
  T.runTurnAI(g);
  const moved = g.regions.filter((r) => r.ctrl !== ctrl[r.id] && (r.owner === A || ctrl[r.id] === A || r.ctrl === A));
  const toB = moved.filter((r) => r.ctrl === B).map((r) => r.name), toA = moved.filter((r) => r.ctrl === A).map((r) => r.name);
  const gone: string[] = [];
  for (const [id, b] of before) { const c = g.corps.find((x) => x.id === id); if (!c || c.dead) gone.push(`${b.nat} ${b.name} (was at ${g.regions[b.reg].name}, ${Math.round(b.str * 100)}%)`); }
  const fresh = g.corps.filter((c) => !c.dead && (c.nat === A || c.nat === B) && !before.has(c.id)).map((c) => `${c.nat} ${c.name} at ${g.regions[c.reg].name}`);
  const news = g.headlines.slice(h0).filter((h: any) => /armistice|capitulat|cut off|surround|lays down|lifted off|land at|siege|surrender/.test(h.text) && (h.text.includes(g.nations[A].name) || h.text.includes(g.nations[A].adj) || h.text.includes(g.nations[B].adj))).map((h: any) => h.text);
  out(`${when}: ${stand()}`);
  if (toB.length) out(`  to ${B} (${toB.length}): ${toB.join(', ')}`);
  if (toA.length) out(`  to ${A} (${toA.length}): ${toA.join(', ')}`);
  if (gone.length) out(`  corps lost (${gone.length}): ${gone.join('; ')}`);
  if (fresh.length) out(`  corps raised (${fresh.length}): ${fresh.join('; ')}`);
  const unsup = live(A).filter((c) => !c.supplied);
  if (unsup.length) out(`  ${A} corps out of supply (${unsup.length}): ${unsup.map((c) => `${c.name} at ${g.regions[c.reg].name}`).join('; ')}`);
  if (news.length) out(`  news: ${[...new Set(news)].slice(0, 6).join(' | ')}`);
  if (!g.nations[A].atWar.includes(B)) out(`  ${A} and ${B} are no longer at war`);
}
out(`end ${date(g.turn)}: ${g.nations[A].alive ? 'standing' : `FELL ${g.nations[A].fellOn ? `to ${g.nations[A].fellOn!.to}` : ''}`}; ${stand()}`);
