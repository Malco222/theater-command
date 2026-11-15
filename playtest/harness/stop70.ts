// Version 70: hands the rules' setStop to lib.ts (see `dial` there). build70.sh puts this file in front of every
// program built against Version 70 or later, so that lib.ts need not import a module older versions do not have.
import { setStop, stopOf } from '@tc/campaign/history';
(globalThis as any).__tcSetStop = (g: any, n: number): void => { if (!setStop(g, n, true)) throw new Error(`the rules refused stop ${n}`); };
(globalThis as any).__tcStopOf = (g: any): number => stopOf(g);
