// The sandbox's ship numbers (data/campaign/sandbox-ships.json), typed. A file of its own so that the sea war can
// read the one number it needs (aaMost) without loading the arsenal.

import data from '../../data/campaign/sandbox-ships.json';
import type { ShipClass } from './types';

export type Style = 'guns' | 'carrier' | 'trade' | 'convoy';
type Stat = 'power' | 'tough' | 'escort' | 'raid' | 'aa' | 'deck' | 'eyes' | 'oil';

export interface RefitDef {
  id: string;
  label: string;
  months: number;
  share: number;
  needs?: string;
  kinds: ShipClass[];
  add?: Partial<Record<Stat, number>>;
  mul?: Partial<Record<Stat, number>>;
  /** Rebuilt as another class: a class id of classes.json, or 'cv' for the standard carrier. */
  to?: string;
  /** For 'to': cv, what a cruiser's hull becomes instead. */
  light?: { label: string } & Partial<Record<Stat, number>>;
  once?: boolean;
  blurb: string;
}

export interface Step { label: string; room?: number; cost?: number; months?: number; needs?: string; [k: string]: unknown }

export const SANDBOX_SHIPS = data as unknown as {
  aaMost: number;
  styles: Record<string, Style>;
  likes: Record<Style, string[]>;
  refits: RefitDef[];
  board: {
    value: number;
    weights: { power: number; escort: number; raid: number };
    hull: (Step & { room: number; cost: number; months: number; yard: number; tough: number; deck: number; oil: number; escort: number; count: number })[];
    guns: (Step & { room: number; cost: number; months: number; power: number })[];
    armour: (Step & { room: number; cost: number; months: number; tough: number })[];
    speed: (Step & { room: number; cost: number; oil: number; raid: number; escort: number })[];
    aa: (Step & { room: number; cost: number; aa: number; escort: number; power: number })[];
    air: (Step & { room?: number; cost?: number; roomShare?: number; costShare?: number; months: number; deck: number; escort: number })[];
    deck: { power: number; escort: number; raid: number };
    gunsRaid: number;
    turrets: [number, number];
    sub: {
      size: (Step & { cost: number; months: number; yard: number; count: number; power: number; tough: number; raid: number })[];
      tubes: (Step & { cost: number; raid: number })[];
      range: (Step & { cost: number; raid: number; oil: number })[];
      hangar: (Step & { cost: number; eyes: number; size?: number })[];
    };
  };
  names: { flotilla: string };
};
