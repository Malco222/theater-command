// What an appointment costs and the political capital a government opens with, by country.
import { campaign, T } from './lib';
for (const me of process.argv.slice(2)) {
  let said = false;
  campaign(me, 1, 7, { idle: true, first: (g: any) => { if (said) return; said = true; const v = (T as any).governmentOf(g); console.log(`${me}: ${v ? `appointment costs ${v.appointCost}, capital at the start ${v.capital}, trust ${v.trust}, form ${v.form?.name ?? v.formName ?? ''}` : 'no cabinet'}`); } } as any, { until: 1 });
}
