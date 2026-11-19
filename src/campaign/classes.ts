// STUB of the shared sandbox core (the planes-and-tanks thread writes the real one; /mnt/project-files/sandbox/SHAPE.md).
import data from '../../data/campaign/classes.json';
import { locked } from './tech';
import { stopOf } from './history';
import type { Game } from './types';

export interface ClassRow { id: string; of: string; label: string; whatif?: boolean; own?: boolean; tech?: string; blurb?: string; model?: string; cost?: number; months?: number; count?: number; yard?: number; factories?: number; steel?: number; [k: string]: unknown }
export const CLASS_ROWS = (data as unknown as { classes: ClassRow[] }).classes;
export const sandbox = (g: Game): boolean => g.flags.open === 1 && g.flags.sandbox === 1;
export function classById(g: Game, nat: string, id: string): ClassRow | undefined {
  return CLASS_ROWS.find((r) => r.id === id) ?? (g.nations[nat] as unknown as { designs?: ClassRow[] }).designs?.find((r) => r.id === id);
}
export function classesFor(g: Game, nat: string, of: string): ClassRow[] {
  if (!sandbox(g)) return [];
  const own = (g.nations[nat] as unknown as { designs?: ClassRow[] }).designs ?? [];
  return [...CLASS_ROWS, ...own].filter((r) => r.of === of);
}
export const classLocked = (g: Game, nat: string, id: string) => locked(g, nat, `class:${id}`);
export const whatifShare = (g: Game): number => [0, 0, 0, 0.1, 1, 1][stopOf(g)] ?? 0.1;
export const computerDesigns = (g: Game): boolean => stopOf(g) >= 4;
