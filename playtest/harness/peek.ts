import { campaign } from './lib';
const run = campaign('UK', 1, 1, { idle: true, each: () => {} } as any, { until: 1, quiet: true } as any);
const g: any = run.g;
const r = g.regions[0]; console.log(Object.keys(r).join(' '));
const by: Record<string, string[]> = {};
for (const x of g.regions) (by[x.owner] ??= []).push(`${x.name}${x.home === false ? '*' : ''}`);
for (const k of ['UK', 'FRA', 'ITA']) console.log(k, by[k].length, by[k].join(', '));
console.log(JSON.stringify({ ...g.regions.find((x: any) => x.name === 'Paris'), units: undefined, nb: undefined }).slice(0, 700));
console.log(JSON.stringify({ ...g.regions.find((x: any) => x.name === 'Faiyum'), units: undefined, nb: undefined }).slice(0, 500));
