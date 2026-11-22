// The month's facts for three sample staged recordings of the submarine scene (form D1), for the sea scene's cloud test:
//   Atlantic, autumn 1941: German boats against a British convoy's escort group (two boats booked sunk);
//   Mediterranean, 1942: a British submarine against an Italian convoy to Libya (no boat booked);
//   Pacific, 1944: an American submarine against a Japanese convoy (one boat booked sunk).
// Each is staged from a month's facts in the shape the bridge reads them (Rules/sub-scene.ts factsOf), with the boats
// at sea as the month closed (boatsAt) and who raided the route (raidBy): in the Mediterranean, Malta in British hands
// raids the Libya convoys too, so its caption says "sunk on the convoy routes", not "by submarines". The Pacific facts are written by hand: today's rules keep no convoy route to
// Japan (navy.json convoys: the Atlantic and Libya only), so in a campaign a Pacific scene comes from a torpedoing or a
// hunt; the stager takes a convoy month for any navy alike.
//
// Written to a folder by samples.ts (Tools/sub_scene/run.sh [out-dir]); the stager's tests stage them too.
import type { MonthFacts } from '../../Rules/sub-scene';

const NAMES = {
  UK: { name: 'United Kingdom', adj: 'British' }, GER: { name: 'Germany', adj: 'German' }, ITA: { name: 'Italy', adj: 'Italian' },
  USA: { name: 'United States', adj: 'American' }, JAP: { name: 'Japan', adj: 'Japanese' }, SOV: { name: 'Soviet Union', adj: 'Soviet' },
};
/** turn: September 1939 is 1 (src/core/ticket.ts turnToDate). */
const turnOf = (year: number, month1: number): number => (year - 1939) * 12 + month1 - 9 + 1;

export const SAMPLES: { file: string; facts: MonthFacts }[] = [
  {
    file: 'sub-atlantic-1941.json',
    facts: {
      human: 'UK', seed: 7919, turn: turnOf(1941, 10) + 1, names: NAMES,
      foes: { UK: ['GER', 'ITA'], GER: ['UK', 'SOV'], ITA: ['UK'], SOV: ['GER'] },
      facts: [
        { kind: 'convoy', sea: 'natl', turn: turnOf(1941, 10), week: 3, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 156000, boats: 2, escortsLost: 1, boatsAt: { GER: 14 }, raidBy: { boats: true, surface: false, base: false } },
        { kind: 'hunt', sea: 'natl', turn: turnOf(1941, 10), week: 3, by: 'UK', on: 'GER', boats: 2, escortsLost: 0 },
      ],
    },
  },
  {
    file: 'sub-mediterranean-1942.json',
    facts: {
      human: 'UK', seed: 104729, turn: turnOf(1942, 9) + 1, names: NAMES,
      foes: { UK: ['GER', 'ITA'], GER: ['UK', 'SOV'], ITA: ['UK'], SOV: ['GER'] },
      facts: [
        { kind: 'convoy', sea: 'central', turn: turnOf(1942, 9), week: 2, by: '', on: 'ITA', route: 'libya', routeName: 'the Libya convoys', tons: 12000, boats: 0, escortsLost: 0, boatsAt: { UK: 3 }, raidBy: { boats: true, surface: false, base: true } },
      ],
    },
  },
  {
    file: 'sub-pacific-1944.json',
    facts: {
      human: 'USA', seed: 1299709, turn: turnOf(1944, 10) + 1, names: NAMES,
      foes: { USA: ['JAP', 'GER'], JAP: ['USA', 'UK'], UK: ['JAP', 'GER'], GER: ['USA', 'UK'] },
      facts: [
        { kind: 'convoy', sea: 'luzonstrait', turn: turnOf(1944, 10), week: 4, by: '', on: 'JAP', route: 'sample:japan', routeName: 'the convoys to Japan', tons: 21000, boats: 1, escortsLost: 0, boatsAt: { USA: 6 }, raidBy: { boats: true, surface: false, base: false } },
      ],
    },
  },
];
