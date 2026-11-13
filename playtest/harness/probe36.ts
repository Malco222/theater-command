// Version 59 on: what switching the cabinet off and on again in the same month clears.
// usage: probe36 SAVE.json [SAVE.json ...]
import { readFileSync } from 'node:fs';
import { T, loadGame, date } from './lib';
const X = T as any;
const brief = (g: any): string => {
  const v = X.governmentOf(g), st = g.nations[g.human]?.gov;
  if (!v) return 'no cabinet';
  return `trust ${v.trust} (${v.trustWord}), low for ${v.low} months (falls at ${v.fallsAt}), missed in a row ${v.missed}, directive ${v.directive ? `"${v.directive.title}" (${v.directive.left} months left, price if missed ${v.directive.price}, aside ${v.directive.asidePrice})` : 'none'}, promise ${v.promise ? `"${v.promise.title}" (broken -${v.promise.price}, taken back -${v.promise.back})` : 'none'}, head ${v.head ? `${v.head.name} (refuse -${v.head.cost})` : 'none'}, watched ${v.stoodBy ? v.stoodBy.name ?? JSON.stringify(v.stoodBy).slice(0, 60) : 'none'}, next directive in ${v.nextIn}, capital ${v.capital}; raw next ${st?.next}, vowNext ${st?.vowNext}, headNext ${st?.headNext}`;
};
for (const f of process.argv.slice(2)) {
  const raw = readFileSync(f, 'utf8');
  const g = loadGame(raw.startsWith('"') ? JSON.parse(raw) : raw) as any;
  console.log(`== ${f.split('/').pop()}: ${g.human} ${date(g.turn)}`);
  console.log(`   before : ${brief(g)}`);
  const off = X.orderCabinet(g, false);
  console.log(`   off (${off}): cabinetIsOff ${X.cabinetIsOff(g)}`);
  const on = X.orderCabinet(g, true);
  console.log(`   on  (${on}) : ${brief(g)}`);
}
