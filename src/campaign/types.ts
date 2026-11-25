import type { Rng } from '../core/rng';
import type { ColumnRun } from './columns';
import type { Dump } from './logistics';
import type { TicketLine, VictoryLevel } from '../core/ticket';

/** The arms of a corps. 'mg' and 'mor' are the support teams (release 2, src/campaign/teams.ts): off unless switched on. */
export type Kind = 'inf' | 'mot' | 'arm' | 'cav' | 'mtn' | 'fort' | 'art' | 'eng' | 'mg' | 'mor';
export type Terrain = 'plains' | 'forest' | 'hills' | 'mountain' | 'marsh' | 'urban' | 'desert';
export type Align = 'allies' | 'allies_cobelligerent' | 'leaning_allies' | 'independent' | 'leaning_axis' | 'axis_cobelligerent' | 'axis';

export interface Region {
  id: number;
  name: string;
  owner: string;
  ctrl: string;
  /** Who owned it on the first day (ruling 214: a smaller power's land pays by its worth, and the worth goes with the ground). Rebuilt from the map on loading, never saved. */
  first?: string;
  terrain: Terrain;
  vp: number;
  fac: number;
  cap: boolean;
  port: boolean;
  /** Works: their level, the nations whose ground they face, and who dug them (ruling 232 rider b624; unset in an old save: the owner). */
  fort: { lv: number; vs: string[]; by?: string; held?: string } | null;
  vichy: boolean;
  sov: boolean;
  oil: boolean;
  /** Overseas ground: fought over like any other, but it does not count toward the home country's will to fight on. */
  overseas: boolean;
  /** The side theatre this region belongs to (see data/campaign/theatres.json), or -1. Battles in a theatre roll their own dice. */
  theatre: number;
  x: number;
  y: number;
  nb: number[];
  polys: [number, number][][][];
  units: Corps[];
  /** Sea zones this region has a coast on. Empty for inland regions. */
  seas: number[];
  /** Turn this region last changed hands, and who held it before. */
  since: number;
  prev: string | null;
  /**
   * The corps inside the province's town (the grand campaign): its garrison. They are not among `units`, which
   * are the armies in the field, so nothing that moves or fights in the open ever counts them. See siege.ts.
   */
  garr?: Corps[];
}

export interface Nation {
  id: string;
  name: string;
  adj: string;
  color: string;
  quality: number;
  air: number;
  resilience: number;
  align: Align;
  blitz: boolean;
  slowCommand: boolean;
  /** The most corps this nation's staffs can bring into one battle. 0 means no limit. */
  maxAttack: number;
  seaSupply: boolean;
  /** How many corps this nation's shipping can carry across the sea in a month. 0 means it has no sea lift. */
  sealift: number;
  atWar: string[];
  alive: boolean;
  dormant: boolean;
  /** A homeland off the map: the sea its convoys come in by (a zone key) and the factories it has there. */
  offmap: { sea: string; industry: number } | null;
  capital: number;
  /** Production points in hand. Losses are made good from these first; the rest can be spent on new formations. */
  pp: number;
  /** New formations and works on order. */
  queue: Build[];
  /** Cadres (the grand campaign): corps whose men were folded into another, kept off the map to be raised again. */
  cadres?: Corps[];
  /** Number the next new corps will carry. */
  corpsNo: number;
  /** Public opinion, from -100 (wants the Allies) to +100 (wants the Axis), and the leaning it drifts back to. */
  opinion: number;
  /** A smaller nation's plans of battleships or carriers, taken or stolen from a great power (plans.ts), in the order gained. */
  plans?: PlanHeld[];
  /** Fighter marks got from other nations' plans (the fighter's plans, intel.ts; the fighter types switch): each mark, whose it was, and when. */
  stolenMarks?: { mark: string; from: string; turn: number }[];
  home: number;
  /** Political capital: what a government can spend on diplomacy. */
  pc: number;
  armyPeak: number;
  /** Turn each current offensive began, by enemy. */
  offStart: Record<string, number>;
  lastFight: Record<string, number>;
  /** When it fell, and to whom. 'how' is absent in a save from before it was kept: such a fall counts as a defeat in the field. */
  fellOn?: { turn: number; week: number; to: string; how?: Passing['how'] };
  /** The grand campaign: when it was liberated, and by whom. A freed country stands for as long as its capital does. */
  freedOn?: { turn: number; week: number; by: string };
  /** The nation's design teams and how far each branch of research has come. Null for a nation that buys its weapons abroad. */
  lab: Lab | null;
  /** A member of the League, the third alliance of smaller nations (the grand campaign). It stands off the track between the two blocs. */
  league?: boolean;
  /** The technology tree (the grand campaign): what the nation has researched and what its teams are working on. */
  tech?: TechState;
  /** Oil and supplies in the depots (the grand campaign), for a power that keeps stocks of its own. */
  res?: Stores;
  /** The player's government (the grand campaign): his war cabinet, his decrees, the cabinet's trust and its directive. */
  gov?: GovState;
  /** The nation's footing (ruling 172, the grand campaign with the lever switched on): see footing.ts. */
  foot?: Footing;
  /** The atomic project (Brian, 25 November 2026, release 2): its plants, its bombs, and the strikes it has taken. See atom.ts. */
  atom?: AtomState;
}

/** The pace the atomic plants draw at: a share of the nation's earnings every month (data/campaign/atom.json). */
export type AtomPace = 'steady' | 'full' | 'crash';

/** A nation's atomic project (atom.ts), and the strikes it has taken. Absent for a country that has never touched it. */
export interface AtomState {
  /** The province the plants stand in, or will stand in once started. */
  site?: number;
  /** The work done on the plants, towards the bill; absent until they are started. */
  plants?: number;
  /** The pace they draw at; 'full' when absent. */
  pace?: AtomPace;
  /** Stopped by order: the plants draw nothing and their work stands. */
  paused?: 1;
  /** What the project drew from the earnings in that month: [month, amount] (economy.ts upkeep). */
  paid?: [number, number];
  /** The month the plants were finished. */
  done?: number;
  /** The month the first bomb was ready. */
  ready?: number;
  /** Bombs in hand. */
  bombs?: number;
  /** The making of the next bomb, in months of work. */
  next?: number;
  /** Ordered to make no more bombs. */
  stopMaking?: 1;
  /** A strike armed: [month, city]: the next bombing raid on that city in that month carries the bomb. */
  armed?: [number, number];
  /** Bombs that reached their city. */
  used?: number;
  /** The month of the last strike flown. */
  struck?: number;
  /** The strikes this country has taken: [month, by whom, city]. */
  hits?: [number, string, number][];
  /** Countries whose project papers this one took with their plants' ground. */
  papers?: string[];
  /** The month the newest bomb was ready (the bridge's chip shows from then until the page is opened). */
  fresh?: number;
  /** The month the player last opened THE ATOMIC PROJECT with a bomb in hand (display only: no rule reads it). */
  seen?: number;
}

/**
 * The nation's footing, where it differs from the one that comes of itself (ruling 172; footing.ts). Absent for a
 * country that has never touched the lever: it earns exactly what it always did.
 */
export interface Footing {
  /** The footing ordered above the one that comes of itself: Rearmament at peace, All-out war at war. */
  step?: 'rearm' | 'allout';
  /** How far the climb has come: the share of industry on war work, all of it on one shift = 1. */
  share?: number;
  /** The month the climb was ordered. */
  since?: number;
  /** No footing may be raised again before this month (a step down, by choice or for want of political capital). */
  wait?: number;
  /** The share of the will to fight all-out war has worn away: its own count, above what shortage takes. */
  worn?: number;
  /** The month the worn share first reached the amber mark in this war, so that the month card says it once. */
  amber?: number;
}

/** A job the war cabinet, or an ally, has set the player (the grand campaign): optional, with a deadline and a reward. */
export interface Directive {
  /** The month it was set: one at a time, so that names it. */
  id: number;
  nat: string;
  /** Take a town, hold one of his own, bring the army up to strength, sign a trade agreement, strike an ally's enemy, or spare the army. */
  kind: 'take' | 'hold' | 'raise' | 'relief' | 'trade' | 'spare' | 'build' | 'agent';
  /** Who set it: the nation's own cabinet, or an ally. */
  from: string;
  region?: number;
  /** For 'raise': the army to reach, in divisions, in the field or forming. For 'trade': the agreements to have signed. For 'spare': the most men that may be lost in battle while it runs. For 'build': the buildings to have standing or ordered. For 'agent': the agents to have in the service. */
  target?: number;
  /** For 'spare': the men lost in battle since the order came. */
  lost?: number;
  /** A town the cabinet knows is strongly held: it gave more months and a larger reward. */
  hard?: boolean;
  /** A town the cabinet has asked for before in this war. */
  again?: boolean;
  /** For 'hold': true once the place has been attacked or besieged while the order ran, and the last battle report read. */
  tested?: boolean;
  seen?: number;
  issued: number;
  /** The last month it can be done in. */
  due: number;
  /** The reward: political capital, money and, from an ally, a corps. */
  capital: number;
  money: number;
  corps?: Kind;
  title: string;
  text: string;
}

/** A promise the player made the war cabinet of his own accord (the grand campaign): an enemy town by a day he named. */
export interface Vow {
  region: number;
  issued: number;
  /** The last month it can be kept in, and the months he allowed himself. */
  due: number;
  months: number;
  /** How bold the cabinet reckoned it on the day, from the odds, the depth and the time: that set the terms. */
  band: string;
  odds: number;
  depth: number;
  /** Kept: trust, political capital and money. Broken: `price` in trust. */
  trust: number;
  capital: number;
  money: number;
  price: number;
  title: string;
}

/** The war cabinet's demand for a general's dismissal, and the general the player stood by while the cabinet watches him. */
export interface HeadAsk { general: number; issued: number; /** The last month the player can answer in. */ due: number; why: string; /** The defeats the cabinet holds against him: battles that could have been won. */ lost?: LostBattle[] }
export interface StoodBy { general: number; until: number; battles: number; won: number }

/** The player's government (the grand campaign). Rules in government.ts. */
export interface GovState {
  /** The war cabinet's trust in him, 0 to 100. */
  trust: number;
  /** The minister in each seat, by id; an empty seat is a caretaker's. */
  seats: Record<string, string>;
  /** The step each decree track stands at. */
  steps: Record<string, number>;
  /** The month a seat ('seat:<id>') can change hands again, or a track ('track:<id>') be raised again. */
  wait: Record<string, number>;
  /** The month the next directive is set. */
  next: number;
  /** Directives ignored in a row, and carried out in all. */
  missed: number;
  done: number;
  /** Months running the cabinet has had no trust left. */
  low: number;
  log: { turn: number; text: string }[];
  /** How things stood when the cabinet last read the news. */
  mark: { held: number; home: number; caps: number; report: number };
  /** What moved the cabinet's trust since it last read the news, cause by cause. */
  last?: { label: string; amount: number }[];
  dir?: Directive;
  lastKind?: string;
  /** The month the government was formed. */
  since?: number;
  /** Towns the cabinet asked for and did not get, by province: the month the order ended. */
  asked?: Record<string, number>;
  /** The player's own promise to the cabinet, the month he may make another, and promises kept and broken. */
  vow?: Vow;
  vowNext?: number;
  kept?: number;
  broken?: number;
  /** The general the cabinet wants dismissed, the month it may ask again, and the one the player stood by. */
  head?: HeadAsk;
  headNext?: number;
  stood?: StoodBy;
  /** The month a directive was last failed: cause for the cabinet to look for someone to blame. */
  blame?: number;
  /** What the attack card read against the towns in reach as each of the last months opened, by province: a promise is reckoned on the best of them. */
  reach?: { t: number; odds: Record<string, number>; depth?: Record<string, number> }[];
  /** The month the cabinet was switched off, while it is: the watch on a general waits that long. */
  offAt?: number;
  /** The wing of the cabinet that was on top when it last made the news. */
  wing?: 'hawks' | 'doves' | 'balanced';
}

/** A trade agreement between two countries (the grand campaign). It pays both every month. */
export interface TradeDeal {
  /** A land route that runs only between colonies: the first country's province at that border. */
  via?: number;
  /** A land route across other countries: those countries, in order. */
  through?: string[];
  a: string;
  b: string;
  /** The month it was signed. */
  since: number;
  /** What it is worth to each side a month with nothing sunk, before either's own commerce is counted. */
  worth: number;
  /** How the goods go, the seas they cross, the share sunk on the way, and how the agreement stands. */
  way: 'land' | 'sea' | 'none';
  seas: number[];
  lost: number;
  state: 'open' | 'raided' | 'cut';
}

/**
 * A neutral's bargain as it stands (bargains.ts; release 2, with its switch on): who buys the good this term and at
 * what price, how the goods go, and the round of bids open for it, if one is.
 */
export interface BargainHeld {
  /** The country buying it this term, or null while nobody does. */
  buyer: string | null;
  /** What the buyer pays the seller a month, in production points, for the goods that arrive. */
  price: number;
  /** The month the term began and the last month it runs. */
  since: number;
  until: number;
  /** How the goods go this month (trade.ts tradeRoute), the seas crossed, the share sunk, and the share that arrives. */
  way: 'land' | 'sea' | 'none';
  seas: number[];
  lost: number;
  share: number;
  /** Months running the way has been shut. */
  shut?: number;
  /** A round of bids: the month it opened, each bidder's offer a month, and the month the seller in the player's seat was reminded. */
  round?: { opened: number; bids: Record<string, number>; reminded?: number };
}

/** Every bargain's state, and each great power's patience with a seller that feeds its enemy. */
export interface BargainBook {
  rows: Record<string, BargainHeld>;
  /** By 'power:seller': what is left of the power's patience, 0 to 100. Absent at 100. */
  patience: Record<string, number>;
  /** Every other country's exports, a month, by seller: fixed as the campaign opens from what it has to trade. */
  generic: Record<string, number>;
  /** By 'bargain:country': the last month the country had the goods, for a good its production comes to depend on. */
  got: Record<string, number>;
}

/** A volunteer legion abroad (legions.ts; release 2, with its switch on), by its home. */
export interface LegionAbroad {
  /** Its corps in the patron's army; -1 in the month it is on its way. */
  corps: number;
  patron: string;
  /** The enemy it was sent to fight. */
  against: string;
  name: string;
  /** The record's row it is (legions.json), if it is one. */
  row?: string;
  /** Its size in divisions' worth of men. */
  est: number;
  /** The month it reaches the line, and the last month of its tour. */
  since: number;
  until: number;
  /** Its home has called it back: it leaves at the next month's start. */
  recall?: boolean;
}

/** The volunteer legions (legions.ts; release 2): who has one abroad, and the record's rows already played out. */
export interface LegionBook {
  abroad: Record<string, LegionAbroad>;
  /** By the record's row: the month it ended. */
  done: Record<string, number>;
}

/** A power's stocks of oil and supplies, and the share of last month's need of each that was met (1 unless the depots ran dry). */
export interface Stores {
  oil: number; sup: number; oilMet: number; supMet: number;
  /** Months of privation at home, counted up while the depots are empty and back down once they are filled. It wears away the will to fight. */
  hunger?: number;
}

export interface TechState {
  /** Technologies researched, in the order they were finished. */
  done: string[];
  /** One entry for each research team at work: the technology, the months still to go and the months it was begun with. */
  work: { id: string; left: number; of: number; spy?: boolean }[];
}

/** The branches of research. */
export type Field = 'armour' | 'infantry' | 'doctrine' | 'fighters' | 'bombers' | 'navy' | 'radar' | 'industry';

export interface Lab {
  /** Design teams at work on each branch. */
  teams: Record<Field, number>;
  /** How far each branch has come, in months of history's clock counted from August 1939. */
  clock: Record<Field, number>;
  /** Months the teams have been at work: where history's own clock stands for this nation. */
  months: number;
  /** Teams founded since the war began, on top of the one per branch the nation started with. */
  founded: number;
}

export interface Build {
  id: number;
  /** A corps, a level of fortification, from the arsenal (aircraft, ships, or a work built in a province), or a level of a town's building ('civic', its line in 'item'). */
  what: 'corps' | 'fort' | 'air' | 'ship' | 'work' | 'civic';
  /** A corps' kind. (Airfields as targets: a 'field' work carries 'strip' here where the level is a field strip, not concrete; read it as a string, isStripBuild.) */
  kind?: Kind;
  divs?: number;
  /** For the arsenal: which aircraft ('ftr', 'bmr'), ship class ('dd', 'tp' and so on) or work ('factory', 'yard', 'aa', 'field'), and how many. */
  item?: string;
  count?: number;
  name: string;
  region: number;
  /** Turn it was ordered and turn it arrives. */
  ordered: number;
  eta: number;
  cost: number;
  /** A corps raised again under the name of one that was destroyed: that formation's place on the memorial wall. */
  revive?: number;
  /** One of the nation's special formations (the grand campaign): the id of its unit in traits.json. */
  elite?: string;
  /** Works or ships ordered in a conquered town that had settled (release 2, buildings.ts settledFor): delivered while the town is held, whatever the switch says by then. */
  conq?: boolean;
  /** An airborne corps, unlocked by the technology tree (the grand campaign). */
  airborne?: boolean;
  /** A corps raised again round its cadre (the grand campaign): the id of the corps that was folded away. */
  cadre?: number;
  /** A wing bought abroad (arsenal.json smallAir.abroad): the great power that sells it. */
  from?: string;
  /** Its fighters' type, the seller's in production in the order month (the switch 'Fighter types'; the design director, 23 Nov 07:00Z). */
  mark?: string;
}

/** One line of a unit's diary: the month, what happened, where (a region, or a sea for a fleet), and a number that goes with it. */
export type DiaryLine = [turn: number, what: string, where: number, n?: number];

/** What every unit carries with it through the war: its record, its diary, its honours and the name the army gives it. */
export interface Storied {
  /** What it has done: the counters its honours are earned from. Only what has happened is written down. See stories.json. */
  rec?: Record<string, number>;
  /** Its diary, oldest first. */
  log?: DiaryLine[];
  /** The honours it carries: ids on the roll of honour. */
  hon?: number[];
  /** The name the army knows it by, once it has earned one. */
  nick?: string;
}

/**
 * (Release 2, the marked army; marked.ts) An enemy corps that took one of the player's towns: where, and in which month.
 * It wears a red tag on the table until he beats it.
 */
export interface CorpsMark {
  place: number;
  turn: number;
}

export interface Corps extends Storied {
  id: number;
  nat: string;
  /** (Release 2, the marked army; marked.ts) It took one of the player's towns, and is marked until he beats it. */
  mark?: CorpsMark;
  name: string;
  kind: Kind;
  divs: number;
  /** 0..1: how much of its men and equipment the corps still has. */
  str: number;
  /** 0..1: cohesion. Drops in battle, comes back with rest and supply. */
  org: number;
  xp: number;
  reg: number;
  /** Weeks of action left this month. */
  ap: number;
  /** Action points this corps started the month with. */
  apMax: number;
  /** Id of the general in command, if the corps has one. */
  general: number | null;
  hold: boolean;
  /** The far side's ship move: the port a computer staff's corps is bound for by sea, a month a leg, where it lies further off than a month's sailing (navy.ts `sailOn`). */
  sail?: number;
  /** A garrison row marked to stay (ruling 221, H): a general offensive's release does not take its hold away. */
  stay?: boolean;
  /**
   * On garrison in ground its country has taken (rider C of 20:45Z, 20 November 2026, to ruling 234; staff.ts,
   * garrisonMonth). 1: a corps its staff named, held and marked to stay for as long as it is named. 0: a corps that
   * stood there held already, or a client's, and counts as it stands. Such a corps is not free: a staff's sums of
   * strength leave it out. None with the far side off.
   */
  garr?: 0 | 1;
  /** The fullest this corps ever gets, where that is under a division's full strength (a setup row's 'est'): its drafts stop there. */
  full?: number;
  /** A record garrison marked to stay inside its town (ruling 232, rider b606): a computer's staff does not send it back to the field for want of an enemy near. */
  fortress?: boolean;
  dyle: string | null;
  supplied: boolean;
  fought: boolean;
  dead: boolean;
  /** Airborne troops: they can take a coast across one sea from the air, with no ships and no sea lane. */
  airborne?: boolean;
  /** The player's standing order to give ground rather than be broken on it. No computer nation sets it. */
  /** The ally whose army this corps marches with, by the player's asking: it takes that nation's orders in that nation's month (the grand campaign). */
  joint?: string;
  yields?: boolean;
  /** The corps has already given ground this month, and will stand if it is attacked again. */
  gaveWay?: boolean;
  /** The player's standing order to entrench. No computer nation sets it. */
  digs?: boolean;
  /** The corps is entrenched: it spent its last month where it stood, under orders to dig in. Lost the moment it leaves the ground. */
  dug?: boolean;
  /** Months it has been dug in where it stands (the grand campaign): its works grow with them, and attacks wear them down. */
  dugFor?: number;
  /** Months it has dug in the open desert with no works to show for them yet (the grand campaign): the works there come a month late. */
  digging?: number;
  /** It was attacked since its own last month ended (the grand campaign): it does no digging this month. */
  pressed?: boolean;
  /** It took over fieldworks left on this ground this month (the grand campaign): it keeps them at the month's end though it marched. */
  relief?: boolean;
  /**
   * The country that raised it, where it now answers to another: a fallen country's corps on far ground that passed
   * into keeping (pacific.ts keptForces). By the book it still fights at the figure of the army that raised it
   * (history.ts raisedQuality; rider b626 to ruling 234). No corps carries it with the far side off.
   */
  raised?: string;
  /** The book has stationed it (stations.ts; ruling 234's rider b568): the months of works the record gives it where it stands, and it digs no more. No corps carries it with the far side off. */
  bookWorks?: number;
  /**
   * Its row's own works, from the record (ruling 234's note of 22:05Z of 20 November 2026, item 5): [province, months].
   * While it stands in that province its works stand at no more than those months (0 none, 1 begun), in place of the
   * general rule that every corps left alone digs in whole. No corps carries it with the far side off.
   */
  ownWorks?: [number, number];
  /**
   * Its row's own will, from the record (the same note): where its morale settles when nothing is happening to it, in
   * place of morale.json's level. No corps carries it with the far side off.
   */
  will?: number;
  /** The book counts it a coast work (stations.ts; rider b579): fortress troops whose figure is in the record's own for the place. It adds nothing to a field fight that outflanks the place's works. No corps carries it with the far side off. */
  bookCoast?: true;
  /**
   * Set only while a dated fight of the book is fought at the record's figures (stations.ts, rider b580): the share
   * of this corps that is in the fight, and its strength as it went in. The rest of it is not on the field.
   */
  bookPart?: [share: number, str: number];
  /**
   * The weight of the attacks it has stood against since its own month began (the grand campaign): 1 for each
   * blow at full weight, less for a light one. Men who have fought all month fight the next blow tired.
   */
  blows?: number;
  /** The month it was raised: the first month of the war for the armies that began it. */
  born?: number;
  /** The region it was raised in, if that was its own country's ground: home. */
  home?: number;
  /** The player's standing order to take no drafts, so that the veterans are not thinned out with green men. No computer nation sets it. */
  lean?: boolean;
  /** Sent home on a bond tour or to the training schools: out of the line until this month. */
  leave?: number;
  /**
   * At the battle schools this month (release 2): the player's order, for one month, to a corps standing on its own
   * country's soil. It takes no orders while it is there, and comes back a grade better if it is left in peace. No computer nation sets it.
   */
  school?: boolean;
  /** Weeks in the line since it was last rested or formed (release 2, looks only): see stories.json "wear". */
  line?: number;
  /** How worn its uniforms and kit look, 0.1 (fresh) to 1 (six weeks or more in the line); absent: fresh. Looks only. */
  wear?: number;
  /** Its share of men who joined it this month, in new kit (release 2, looks only): absent, none. 'wear' counts them in. */
  fresh?: number;
  /** The work an engineer corps has in hand: what, where, and the month it is finished. Lost if the corps leaves the place. */
  job?: { what: string; reg: number; eta: number; ap?: number };
  /** The rivalry it is part of: an id on the list of rivalries. */
  rival?: number;
  /** The destroyed formation whose name and colours it carries: that formation's place on the memorial wall. */
  heir?: number;
  /** One of its nation's special formations (the grand campaign): the id of the unit in traits.json. */
  elite?: string;
  /** Morale, 0 to 1 (the grand campaign). A corps in a historical scenario carries none. See morale.json. */
  mor?: number;
  /** Why its morale stands where it does: [reason, change, 0 for since its nation's last reckoning or 1 for the month before]. */
  mw?: [string, number, number][];
  /** The number its first division carries (the grand campaign): its divisions are numbered on from there. */
  dn?: number;
  /** Its divisions' numbers in order, where they do not run on from 'dn' (the grand campaign: real ones of 1939 beside stand-ins, armies.ts). */
  nos?: number[];
  /** In garrison inside its province's town since this month (the grand campaign): it is on the region's `garr` list, not among its `units`. */
  town?: number;
  /** A volunteer legion (legions.ts; release 2): the country at peace whose men these are. It fights in its patron's army under its own name and flag. */
  vol?: string;
  /** Its divisions' own names in order, [name, short], where they are not the ones its corps' name gives (breakup.ts; release 2). */
  names?: [string, string][];
  /** The corps it was split off from (breakup.ts; release 2): it can rejoin that one, or another part split off it. */
  part?: number;
  /** The numbers the divisions of the corps it belongs to carried, in order, before it was broken up (breakup.ts): no rejoining makes one bigger, and the parts go back in this order. */
  whole?: number[];
  /**
   * (Release 2, the grand campaign: a division is laid once, drawn.ts.) The divisions a dated row brought into this
   * corps from living corps, instead of laying them a second time: each one's name, its number, and the corps it came
   * out of ('from': this corps' own id where the row took the whole corps, and the divisions are its own). 'conv'
   * marks one formed out of another division (the 14th Panzer, out of the 4th Infantry): it never goes back. Read when
   * the row's time with its campaign is up ('until'), or the war it was raised for is over: each goes back to its
   * corps, or stands on where it is. 'rec' (the stood war rows, counting.json): drawn by share, it stands for this
   * many of the divisions the record counts in the corps it came out of.
   */
  drew?: { name: [string, string]; no: number; from: number; conv?: true; rec?: number }[];
  /** (The same.) Its own name and standing orders from before a dated row took the whole corps under the row's name: it has them back when the row's time is up. */
  rowed?: { name: string; hold: boolean; stay?: boolean };
  /**
   * (The stood war rows, counting.json.) How many of the divisions the record counts in this corps dated rows have
   * drawn out of it by share: the next share is taken of what is left. None where nothing was drawn.
   */
  out?: number;
  /**
   * (The stood war rows, counting.json.) How many divisions the record counts in this corps, from its first draw by
   * share: with `out`, the share of its establishment the drafts still make good (drawn.ts ownShare).
   */
  outOf?: number;
  /**
   * (The stood war rows, counting.json.) How many of this corps' cards earlier rows counted as staying with it: the
   * player's own, or a corps that stands where the row is for. A row after them counts the cards before those, so no
   * card stands for two rows, and where none is left it raises new what the corps no longer holds. None where none was counted.
   */
  kept?: number;
  /**
   * A rearguard (release 2, rearguard.ts): the month it was ordered to hold where it stands, the weeks of marching it
   * gave up for it (given back if it is stood down in the same month), and the province it holds (driven out of it, it
   * is a rearguard no longer). Gone at the start of its side's next month.
   */
  rear?: [number, number, number];
}

/** What an army does on the day the enemy comes: fight where it stands, fight from trenches it has dug, or give ground. */
export type Posture = 'stand' | 'dig' | 'back';

export type Skill = 'offense' | 'defense' | 'logistics' | 'leadership';

/** A general's service record: what he has done, which is what his traits are earned from. */
export interface ServiceRecord {
  battles: number;
  won: number;
  attacks: number;
  defences: number;
  wounds: number;
  /** Decisive wins at the head of fast troops. */
  fast: number;
  /** Attacks won when the odds were close. */
  close: number;
  /** Wins that ended with the defenders laying down their arms. */
  enc: number;
  /** Positions held against a stronger attacker. */
  held: number;
  /** Battles fought through a Russian winter without winter equipment. */
  winter: number;
  /** Failed assaults that cost his corps dearly. */
  costly: number;
  /** Defeats in a row. */
  streak: number;
  /** The battles he has lost since he last won one, the newest last, with the odds they were fought at (the grand campaign): what a cabinet weighs before it asks for his head. */
  lost?: LostBattle[];
}

/** A battle a general lost: the month, the province, the attacker's odds as the fight opened, and whether he was the one attacking. */
export interface LostBattle { t: number; reg: number; odds: number; att: boolean; /** The odds at which the attack had an even chance on the day: absent on battles from before Version 63, read as 1.2. */ even?: number }

export interface General {
  id: number;
  nat: string;
  name: string;
  /** A real commander of the war, or an officer who rose from the ranks in this one. */
  historical: boolean;
  level: number;
  /** Experience toward the next level. */
  xp: number;
  /** Skill points not yet spent. */
  points: number;
  skills: Record<Skill, number>;
  traits: string[];
  /** Id of the corps he commands, or -1 while he waits for a command. */
  corps: number;
  status: 'active' | 'wounded' | 'captured' | 'dead' | 'sacked';
  /** Month a wounded general is fit again. */
  backOn: number;
  /** Month he joined the list, and the month he was last given a new command. */
  joined: number;
  moved: number;
  rec: ServiceRecord;
}

export interface ReportUnit {
  name: string;
  nat: string;
  kind: Kind;
  divs: number;
  men: number;
  lost: number;
  fate: '' | 'retreated' | 'surrendered' | 'destroyed' | 'advanced' | 'evacuated' | 'yielded' | 'sheltered';
  /** Not a corps: the town's own guard of depot troops, police and militia (the grand campaign). */
  guard?: boolean;
  /** Ruling 232: the province a beaten field army fell back to, and a field army cut off that laid down its arms. */
  to?: number;
  cut?: boolean;
  /** Units decide (#7 4(b)): men taken by a faster pursuit as the corps fell back; and the roads out a faster enemy cut first. */
  chased?: number;
  roadsCut?: number[];
}

export interface BattleReport {
  id: number;
  turn: number;
  week: number;
  region: number;
  regionName: string;
  attNat: string;
  defNat: string;
  attackers: ReportUnit[];
  defenders: ReportUnit[];
  /** The Battle Ticket: what the campaign map fed into this fight. */
  lines: TicketLine[];
  aPower: number;
  dPower: number;
  odds: number;
  level: VictoryLevel;
  taken: boolean;
  attLost: number;
  defLost: number;
  prisoners: number;
  /** No battle was fought: the defenders were under orders to give ground and fell back before the attack. */
  yielded?: boolean;
  /**
   * Fought on the field, that side's general ordered ALL FALL BACK (release 1.5; Brian, 23 November 14:47Z): its army
   * withdrew behind the ground it started from to fight another day. The result is that side's defeat, as booked.
   */
  fellBack?: 'att' | 'def';
  /** The share of the defence that was dug in and in supply, where any was (the grand campaign): luck ran against the attack by that much. */
  prepared?: number;
  /** An attack thrown back from dug-in troops (the grand campaign): the months it took off their works, and the months of works they have left. */
  worn?: number;
  works?: number;
  /** The fight was for the town itself: an assault on its garrison (the grand campaign). */
  town?: boolean;
  /** Units decide (#7 4(b)): a beaten army attacking the enemy on its road out, before a cut road takes it whole. */
  breakout?: boolean;
  /** The field was won but the town holds out: the province is under siege, not taken (the grand campaign). */
  siege?: boolean;
  /**
   * Fought by hand on the field in a capital's or a city's streets (src/battle/land/field.ts streetsOf): the field drew the
   * battle in its outer streets, so the after card says so, and that its old town holds out under a siege (the third
   * playtester's hand check of 4086e97).
   */
  streets?: 'capital' | 'city';
  /** The field was won and the town, held only by its guard, opened its gates without a fight (the grand campaign). */
  gates?: boolean;
  /** The town's guard fought beside the field army (ruling 232, the grand campaign): won, the town fell with the field. */
  guarded?: boolean;
  /** A relief: the attackers came to raise a siege of their own side's town. */
  relief?: boolean;
  /** Damaged cities (the grand campaign with its switch): what the fighting did to the province's town, in percent points added, and where it now stands. */
  city?: { region: number; added: number; now: number };
  /** Close air support (the grand campaign, air.json 'close'): the air strikes each side's groups on ground support flew over the battle. */
  air?: { att: number; def: number };
  /** The garrison of a besieged town came out against the army round it. */
  sally?: boolean;
  /**
   * Who paid, and to what (the grand campaign, rulings 146 and 149): the result's line as the attacker reads it and as
   * the defender does. "Your infantry paid most of it, to their machine guns. The tanks' shock counted for a third."
   */
  paid?: { att: string; def: string; sign?: 'tank' | 'anti-tank' | 'machine-gun' | 'mortar' | 'sniper' | 'gun' | 'scouted' };
  /** Weeks of their month each attacking corps lost to a rearguard holding the field (release 2, rearguard.ts; may be a part of a week). */
  rearguard?: number;
}

// ---- The war at sea ---------------------------------------------------------------------

export type ShipClass = 'bb' | 'cv' | 'ca' | 'dd' | 'ss';
export type Ships = Record<ShipClass, number>;
/**
 * What a fleet is doing: holding a sea, hunting merchant ships, or lying in harbour. In the grand campaign it can also
 * be bound to a shipping lane as its escort, or sent to a sea to hunt submarines.
 */
export type Stance = 'patrol' | 'raid' | 'port' | 'escort' | 'hunt';
/**
 * What a fleet is doing, its orders and one state no order gives: 'passage', on its way to a new base beyond a month's
 * steaming (release 2, the far chart: the design director's ruling of 04:33Z, 25 November 2026; navy.ts, rebase).
 */
export type FleetStance = Stance | 'passage';

export interface SeaZone {
  id: number;
  key: string;
  name: string;
  x: number;
  y: number;
  adj: number[];
  /** Regions with a coast on this sea. */
  coast: number[];
  /** How much of this sea land-based aircraft can cover: 1 for narrow waters, less for open ocean. */
  air: number;
  /** Tight water: its name is drawn in smaller type. */
  narrow: boolean;
}

/** A strait someone holds. */
export interface Gate {
  a: number;
  b: number;
  name: string;
  /** The region whose controller holds the strait, or -1 when a nation holds it from off the map. */
  region: number;
  nation: string | null;
  /** Shut to every navy at war. */
  closed: boolean;
}

export interface Admiral {
  name: string;
  level: number;
  xp: number;
}

export interface Fleet extends Storied {
  id: number;
  nat: string;
  name: string;
  ships: Ships;
  /** Ships lost since the war began. */
  lost: Ships;
  /** Units decide (the convoy war by cause), written only under that switch: the boats it had when it first lost any this month and those lost since, for the hunt's monthly cap. Saved, so a month reloaded part-way plays on as the live one. */
  capBook?: { turn: number; had: number; lost: number };
  /**
   * Units decide's quality store (DD 24 Nov 15:53Z, #2 form Part 2), written only under that switch: its crews' level,
   * in navy.json's quality units, blended by ship count as ships join and leave. Unset, the fleet reads its navy's
   * crew (QualityStore sea.crew).
   */
  crew?: number;
  /** Its crews' experience, 0 to 1. Not laid yet: nothing writes or reads it. */
  sxp?: number;
  /** Boats working up after joining: how many, and the months each batch still has to go. They do not yet count on station. */
  workup?: [boats: number, months: number][];
  /** Region of the home port, or -1 when the base is off the map. */
  base: number;
  /** What the base is called: Scapa Flow, Wilhelmshaven. */
  port: string;
  /** The sea the base opens on. */
  home: number;
  /** The book has moved its base forward for a dated cover (stations.ts; riders b575, b577): the base, port, home sea and station it goes back to. No fleet carries it with the far side off. */
  bookBase?: [base: number, port: string, home: number, station: number];
  /** The sea the fleet is in. A fleet in harbour is counted in its home sea but takes no part there. */
  zone: number;
  stance: FleetStance;
  /**
   * A fleet in passage to a new base (release 2, the far chart; navy.ts, rebase): the base, port and home sea it left
   * (`from`, `fromPort`, `fromHome`, where it is turned back to), the seas of its way in order (`way`), the month it set
   * out on that way (`set`) and the month it comes in (`due`), and whether it is on its way back (`back`). `start`: the
   * month it first set out, where a strait shut on the way has put it on another way since. Its base, port and home
   * are the new harbour's from the month it sets out. Only while its stance is 'passage'. `adrift`: the month it began
   * to wait at sea with no harbour it can reach (the design director's ruling of 15:48Z, 25 November 2026, 3): it is
   * not scuttled, and waits there, doing nothing and open to attack, until one opens (navy.ts goAdrift).
   */
  passage?: { from: number; fromPort: string; fromHome: number; way: number[]; set: number; due: number; back?: true; start?: number; adrift?: number };
  /**
   * Its air strikes at anchor this month and last (release 2, the far chart; the design director's ruling of 04:33Z, 25
   * November 2026, 2; navy.ts anchorLoss): for each, the month (`t`), the harbour (`at`), the ships sunk (`n`) and
   * their share of its surface ships (`lost`), the strikers' weight in the air (`w`), and where they flew from as its
   * side saw them (`from`: the sea of their carriers, or -1 and the airfield, and how far their bombers reach in km;
   * with the strikers' nation and their carrier fleet, release 2's ruling of 15:48Z, for where they could be next month).
   */
  anchor?: { t: number; at: number; n: number; lost: number; w: number; from: { sea: number; field: number; km: number; nat?: string; fleet?: number }[] }[];
  /** Where a computer-run fleet goes when it has nothing better to do, and what it does there. */
  station: number;
  duty: Stance;
  /**
   * A fleet tied to its station is never sent anywhere else by the computer. The tie lapses the month its home port
   * falls, or once its station is out of reach from the port it is in (navy.ts tieLapses; ruling 233 rider b598).
   */
  tied: boolean;
  /** 0..1: how fit for sea the ships are. Battle and long patrols wear it down; time in harbour restores it. */
  ready: number;
  admiral: Admiral | null;
  /** Month the fleet last fought. */
  fought: number;
  /** The shipping lane the fleet is escorting, while its stance is 'escort' (the grand campaign). */
  lane?: string;
  /** Month the player last sent this fleet straight at an enemy fleet: once a month (release 2, fleetattack.ts; written only under that switch). */
  attacked?: number;
  /**
   * Month this fleet last sought battle: on the seeking side of a fleet action, by order or in a month-end meeting, or
   * sent at the enemy by an order that missed (release 2, the switch 'seekOnce'; written only under it). It seeks no
   * more that month; it can still be found and fought.
   */
  sought?: number;
  /** A submarine group of the far side's beats: how many seas off its base its boats raid (navy.ts boatsReach). No fleet carries it with the far side off. */
  reach?: number;
  /** Month the fleet last shelled a coast: its guns speak once a month (the grand campaign). */
  shelled?: number;
  /** The month each nation last had this fleet in contact: it fought it, was shelled by it, or was struck by its aircraft (the grand campaign). */
  seen?: Record<string, number>;
  /** The month it was last struck from the air, how many times that month (a fleet struck once is scattered and warned), and the sea it was in (-1 in harbour). */
  struck?: [number, number, number?];
  /**
   * Deck room standing empty because a wing was ordered ashore, in aircraft of each kind (the grand campaign; the
   * player's order only). No new wing is raised for it, and the fleet's carriers fight as empty carriers for that
   * share of their decks until the wing comes back aboard.
   */
  bare?: { ftr: number; bmr: number };
  /** The far side (ruling 234(d)), from its row: [sea, chance, whose], where it may spend a month of peace at sea, and how likely, at a stop that keeps no book; 'whose', the country its peace is read against, where the row names one (navy.ts, navalUpkeep). */
  cruise?: [string, number, string?];
  /** The month it is at sea though its country is at peace, by the record or on such a cruise. */
  cruised?: number;
  /**
   * Battle stars (b477), ship by ship: for each class, an entry for each of its ships, those longest in the fleet
   * first, each the fleet actions it came through on the winning side (places in g.stories.seaActions). Ships at the
   * back with none are left off, and a fleet with none has no record. Kept in step with the counts by stars.ts.
   */
  stars?: Partial<Record<ShipClass, number[][]>>;
  /**
   * The player gave this fleet an order himself (1.5.1; Brian, 24 November 2026 18:32Z: "when i move my navy they go
   * right back to where they were before the next turn"). While his admirals run the navy for him (depth.ts) they leave
   * it where he sent it, until he hands it back to them or hands them the navy afresh. Never set on a computer's fleet.
   */
  ordered?: true;
  /**
   * A base the staff's book moved it to while it was his (the far side's dated rows, navy.ts `bookWaits`; the audit's
   * re-check of 25 November 2026): he keeps the fleet by hand, or runs the navy himself, so it waits. Handed back to
   * the admirals it goes there with their next month; his fleet page offers the move meanwhile. Never on a computer's.
   */
  planBase?: { base: string; port: string; home: string; words: string; turn: number };
  /**
   * The share of its navy's wing each of its carriers brings, where its row says less than a whole one (ruling 189(b):
   * a light carrier that sailed alone, Ryujo with the 3rd Fleet and Shoho with the 4th, a third of a wing). Carriers
   * joined from another fleet bring theirs, and the fleet's share is the mean a deck (carriers.ts `moveWings`).
   */
  deck?: number;
  /**
   * A task force's standing job (release 2, taskforce.ts): the sea and what it does there, or the lane it escorts. It
   * comes in to refit when worn and goes back to the job when fit, every month, until he changes or ends it.
   */
  task?: { zone: number; stance: Stance; lane?: string };
  /** The month the player's admirals last ordered it and their reason, in a sentence (release 2, fleetwhy.ts). */
  why?: [number, string];
  /** The enemy fleet it shadows (release 2, shadow.ts): it follows that fleet each month and keeps it in contact. Written only under that switch. */
  shadow?: number;
  /** The month its shadow went on station (release 2, shadow.ts, the shadow relay): its fuel lasts SHADOW.relay.months from then. Written only under that switch. */
  shadowAt?: number;
  /** The month it last laid a field of mines (release 2, mines.ts): once a month. Written only under that switch. */
  mined?: number;
  /**
   * Its voyage log (release 2, voyage.ts), oldest first: for each month, what it was doing at the month's end (`s`), the
   * sea (`z`) or harbour (`p`), and the fleet actions it fought (`f`, a sentence each). A record only, apart from its
   * diary (`log`, stories.ts). Written only under that switch.
   */
  voyage?: { t: number; s: FleetStance; z: number; p: string; f?: string[] }[];
}

/** A coast shelled from the sea: the troops on it, or the works of its city (the grand campaign). */
export interface ShoreReport {
  id: number;
  turn: number;
  nat: string;
  fleet: string;
  admiral: string | null;
  /** The ships that stood in, as they were before the shore answered. */
  ships: Ships;
  /** What the shore batteries sank. */
  sunk: Ships;
  region: number;
  place: string;
  zone: number;
  defNat: string;
  what: 'troops' | 'city' | 'town';
  /** A town's defences shelled (the grand campaign's sieges): the percent of its works broken down. */
  works?: number;
  /** The weight of the guns that fired. */
  guns: number;
  /** Troops: the share of their organisation the shelling cost them, and how many divisions it fell on. */
  cut: number;
  divs: number;
  /** City: the share of the province's output stopped for a month, and of the nation's production. */
  output: number;
  nation: number;
  lines: TicketLine[];
  /** What the shelling did, in a sentence. */
  result: string;
  /** Damaged cities (the grand campaign with its switch): what the shells did to the town, in percent points added, and where it now stands. */
  city?: { region: number; added: number; now: number };
}

/** A battleship or a carrier that went down: her name (null once the record of names runs out), her crew and the men lost with her (hulls.ts). */
export interface LostHull {
  name: string | null;
  cls: 'bb' | 'cv';
  crew: number;
  lost: number;
  /** Two light carriers counted as one (both names, both crews). */
  pair?: boolean;
  /** Her type where it is not her class's (the game counts Hood, Renown and Repulse as battleships). */
  type?: 'battlecruiser';
}

/** A battleship or a carrier that went down: her name (null once the record of names runs out), her crew and the men lost with her (hulls.ts). */
export interface LostHull {
  name: string | null;
  cls: 'bb' | 'cv';
  crew: number;
  lost: number;
  /** Two light carriers counted as one (both names, both crews). */
  pair?: boolean;
  /** Her type where it is not her class's (the game counts Hood, Renown and Repulse as battleships). */
  type?: 'battlecruiser';
}

export interface SeaSide {
  nat: string;
  fleet: string;
  admiral: string | null;
  ships: Ships;
  sunk: Ships;
  /** The battleships and carriers it lost, by name (a record only: hulls.ts). */
  hulls?: LostHull[];
  /** A strike from the air (ruling 240): the ships damage control kept afloat, where it kept any (a record, for the after card). */
  kept?: Ships;
}

export interface SeaReport {
  id: number;
  turn: number;
  zone: number;
  zoneName: string;
  attNat: string;
  defNat: string;
  att: SeaSide[];
  def: SeaSide[];
  lines: TicketLine[];
  aPower: number;
  dPower: number;
  odds: number;
  level: VictoryLevel;
  won: boolean;
  /** The harbour, where the attacker went in after a fleet at anchor there (release 2, fleetattack.ts). Left out at sea. */
  port?: string;
}

/** One month on a convoy route. */
export interface ConvoyMonth {
  turn: number;
  /** Raiders hunting the route and escorts guarding it, as strength. */
  raid: number;
  guard: number;
  /** Tons of shipping sunk this month, and in the whole war. */
  tons: number;
  total: number;
  /** Share of the receiving nation's production lost with it. */
  share: number;
  /** Share of the cargo on the route that was sunk. */
  loss?: number;
  subsLost: number;
  escortsLost: number;
  /** Release 2, with the convoy battle on (ruling 241), record only: the month's swing on the share sunk, and whether a night the player fought set it ('field'), or set it with a count held inside the dice's run ('clamped': a bare close result, b331). */
  swing?: number;
  fought?: 'field' | 'clamped';
  /** Release 2, with Units decide (the convoy war by cause), record only: of subsLost, the boats the convoy's escorts sank in their attacks on it. */
  byEscorts?: number;
  /** Release 2, with Units decide and the sea patrol live (seapatrol.ts), record only: of subsLost, the boats the receiving nation's aircraft on patrol sank in the route's seas. */
  byAircraft?: number;
  /** Release 2, with Units decide and the route's convoy count laid (the one-pass ruling), record only: the boats on station on the route, the convoys that sailed (both ways), those sighted and attacked, the incidents with the escort at OEG T2's rate (the boats that fired), the boats a convoy attacked (the pack), and, where the tons are counted from the convoys' side, the ships a convoy attacked lost, on average. With the escorts' losses form laid, 'met' is the incidents with the escort a convoy attacked by cause (navy.ts escortWork), which the escort's kills read. */
  count?: { boats: number; sailed: number; sighted: number; attacked: number; incidents: number; pack: number; sunk?: number; torps?: number; watched?: number; met?: number };
}

/** A boat or ship group in the submarine scene's record: its fleet, its nation and its boats or destroyers as the month began. */
/** A fleet as the submarine scene stages it: 'n' its boats (or a hunter's destroyers); 'heavy', for a convoy's raider only, its battleships, carriers and cruisers. */
export interface SubLookFleet { fleet: number; nat: string; n: number; heavy?: number }

/**
 * One nation's month at sea as the submarine scene stages it (release 2, ruling 241 D1; record only, read by no rule).
 * Written in the nation's own month (navy.ts navalUpkeep) while the viewing setting is on; this month and last are kept.
 */
export interface SubLook {
  turn: number;
  /** The nation whose month it was: the hunter, the torpedoed fleets' owner and the convoys' receiver. */
  nat: string;
  /** Each sea its hunters went after enemy boats in: its hunting fleets (destroyers), the weight of its own aircraft over the sea, the enemy boats there, and the boats sunk and destroyers lost. */
  hunts: { zone: number; hunters: SubLookFleet[]; air: number; quarry: SubLookFleet[]; sunk: number; cost: number }[];
  /** Each heavy ship of this nation's torpedoed and sunk: its fleet, sea and class; the enemy boats' fleets in the sea that could have fired (as torpedoRisk counts them) and their nations, in the same order; and one of them picked for the look on its own key ('look:torpedo:<fleet>'), so no keyed roll moves, with its nation. */
  /** sunk: false where damage control kept her afloat (ruling 240, C3): the same hit, told the same. */
  torpedoes: { fleet: number; zone: number; cls: ShipClass; sunk: boolean; could: number[]; by: number | null; couldNat: string[]; byNat: string | null }[];
  /** Each convoy route feeding this nation, sea by sea: the raiders (boats) and escorts (destroyers) there, the air guard's strength over it, and the month's tons sunk in it (the month's tons shared by the raiders' strength in each sea). */
  /** With Units decide only (the convoy war by cause): the kit of the navy with most destroyers answering attacks in the sea (HF/DF has no rule yet, so always false; groups is its escort-group training), the air cover's kind, and the boats its escorts sank there. */
  /** Also with Units decide only, kits: each allied navy with destroyers answering attacks in the sea, keyed by its nation, with its own kit; where none answers, the convoy's own nation alone. The scene keys its escorts' looks by it, so each navy's escorts are drawn with their own kit. It is look only: no rule reads it. */
  convoys: { route: string; tons: number; seas: { zone: number; raiders: SubLookFleet[]; escorts: SubLookFleet[]; air: number; tons: number; kit?: { asdic: boolean; radar: boolean; hedgehog: boolean; hfdf: boolean; groups: boolean }; kits?: Record<string, { asdic: boolean; radar: boolean; hedgehog: boolean; hfdf: boolean; groups: boolean }>; cover?: 'none' | 'shore' | 'carrier' | 'long-range' | 'patrol'; lost?: number }[] }[];
}

export interface Headline {
  turn: number;
  week: number;
  text: string;
  /** 'goal' is news of the player's own campaign goals: nothing in the war writes it. 'honour' is a unit's citation, and 'letter' a note from the front. */
  kind: 'war' | 'fall' | 'battle' | 'event' | 'info' | 'goal' | 'honour' | 'letter';
  nat?: string;
  region?: number;
  /** For an honour, its id on the roll. For a letter, who wrote it and the corps it came from. */
  award?: number;
  by?: string;
  unit?: number;
  /** The grand campaign: the month's line from a desk the player's staff runs for him (depth.ts), so that an interface can offer to take it back. */
  desk?: string;
  /** A warning to the player, with its short title ('Coast watch'): an interface sets it out as a card of its own. */
  warn?: string;
  /**
   * (Release 2; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered".) News that a unit the
   * player raised or bought has been handed over and is ready for orders: a corps (its id in `unit`), aircraft, or
   * ships. Display only: no rule reads it.
   */
  ready?: 'corps' | 'air' | 'ship';
  /**
   * A title of its own over a line of news that is no warning ('Britain stands by Canada', 'Britain stays out of
   * Canada's war': ruling 211(b)). An interface heads the news with it.
   */
  head?: string;
  /** For a warning: the country the news is about, where that is not the player's own (display only: a band flies its flag). */
  about?: string;
  /**
   * Its line on the month card, where the first sentence of the news is too long for a row there (display only: 'War
   * has reached Japan's home ground.', the UI director's sheet 25). All reports and the newsreel keep the whole text.
   * Among a country's news of the month it stands next after a fall.
   */
  card?: string;
  /**
   * Two tags on news of fighting (Version 80; display only, no rule reads them). `arm` is the arm that did the
   * fighting: on land, at sea or from the air. `won` and `lost` are the nation that had the better of it and the one
   * that had the worse, where the news has a winner or a loser; either may be missing. An interface asks `newsSide`
   * (turn.ts) whether that is good news or bad for whoever is looking.
   */
  arm?: 'land' | 'sea' | 'air';
  won?: string;
  lost?: string;
}

export type UnitKind = 'corps' | 'air' | 'fleet';
/** What the player has done with an honour: nothing yet (the unit carries it at the front), a bond tour at home, or instructors for the schools. */
export type Cash = 'bonds' | 'school';

/** One entry on the roll of honour. */
export interface Award {
  id: number;
  turn: number;
  /** Which honour: an id in stories.json. */
  hon: string;
  kind: UnitKind;
  /** The unit's id, and its name and nation on the day. */
  unit: number;
  name: string;
  nat: string;
  /** Where it was earned: a region, or a sea for a fleet. -1 if nowhere in particular. */
  at: number;
  /** A number the citation quotes, such as months under siege. */
  n?: number;
  /** The other side of a rivalry, by name. */
  rival?: string;
  /** No unit of its nation had earned this honour before. */
  first?: boolean;
  /** Earned by a formation that was destroyed, and carried now as a ribbon by the one raised in its name. */
  old?: boolean;
  /** What the player did with the fame, and when. */
  cash?: Cash;
  cashed?: number;
  /** The player has seen the citation. */
  seen?: boolean;
}

/** A formation on the memorial wall. */
export interface Fallen extends Storied {
  id: number;
  nat: string;
  name: string;
  kind: Kind;
  divs: number;
  /** When and where it was lost, how, and to whom. */
  turn: number;
  reg: number;
  how: 'destroyed' | 'surrendered' | 'starved';
  by: string;
  born: number;
  home: number;
  xp: number;
  /** The general who was with it at the end. */
  general?: string;
  /** The corps raised again in its name, once there is one. */
  heir?: number;
}

/** Two corps that keep meeting: one of the player's and one of the enemy's. */
export interface Rivalry {
  id: number;
  mine: number;
  theirs: number;
  mineName: string;
  theirsName: string;
  theirsNat: string;
  /** When and where it was first noticed, and where the two first met. */
  since: number;
  at: number;
  first: number;
  met: number;
  won: number;
  lost: number;
  /** How it ended, and when: the player's corps destroyed the other, was destroyed by it, or one of them was lost elsewhere. */
  over?: 'won' | 'lost' | 'gone';
  ended?: number;
}

/** Everything the unit stories keep that does not sit on a unit. */
export interface Stories {
  awards: Award[];
  fallen: Fallen[];
  rivals: Rivalry[];
  /** How often one of the player's corps has met an enemy corps, 'mine:theirs', and where they first met. */
  meets: Record<string, [number, number]>;
  /** Instructor postings each nation has made: every one trains its later drafts a little better. */
  schools: Record<string, number>;
  /** Moments of the month that someone may write home about. The weightiest become letters when the player's month opens. */
  mail: Moment[];
  /** How many letters of each kind have been written, so that the same one is not sent twice running. */
  told: Record<string, number>;
  /** Bomber crews who have finished a tour, by nation: where the player sends them, and those at the schools and selling bonds. */
  crews?: Record<string, CrewsHome>;
  /** The holders of each nation's highest award for valour (valour.ts), oldest first. */
  heroes?: Hero[];
  /**
   * The fleet actions of the war that one side won, in the order they were fought: the month and the sea. A ship's
   * battle stars are places in this list (stars.ts). Absent until the first is won, and in a save from before.
   */
  seaActions?: [turn: number, zone: number][];
}

/** A man given his nation's highest award for valour (valour.ts). */
export interface Hero {
  id: number;
  turn: number;
  nat: string;
  kind: UnitKind;
  /** His unit's id, and its name on the day. */
  unit: number;
  name: string;
  /** Who he is: 'A sergeant', 'A bomber pilot'. */
  who: string;
  /** Where it was won: a region, or a sea for a fleet. -1 if nowhere in particular. */
  at: number;
  /** The honour his unit earned that day (Award.id). */
  award: number;
  /** What the player did with him and when: a war bond tour, with the months of it left, or training recruits. */
  sent?: Cash;
  sentOn?: number;
  left?: number;
}

/** Something that happened to one of the player's units: how much it matters, what it was, to whom and where. */
export interface Moment {
  w: number;
  m: string;
  /** The unit's id and its name on the day. */
  id: number;
  unit: string;
  reg: number;
  /** Words for the letter to quote: the general, the enemy, a rival, a title, a number. */
  gen?: string;
  foe?: string;
  x?: string;
  n?: number;
}

export interface PlanPhase {
  name?: string;
  from: number;
  to: number;
  posture: 'attack' | 'defend' | 'redeploy';
  enemies?: string[];
  declare?: string[];
  declareWhenCollapsing?: string;
  collapseAt?: number;
  objectives?: string[];
  /** The line the armoured thrust follows. */
  axis?: string[];
  /** Ground the plan leaves alone in its first month. */
  haltOrder?: string[];
  /** Prizes that troops who closed up to them late in the month attack at once, without waiting for next month. */
  lastPush?: string[];
  assembly?: string[];
  armour?: string[];
  garrison?: string[];
  /** Garrisons in these places are stood down and join the field army. */
  release?: string[];
  /**
   * Corps kept at a named place. In a redeployment they are railed there to wait; in an attack or a defence the nearest
   * that can be spared march to it and stay. Such a force is infantry: once two corps are there, tanks are not held back.
   */
  staging?: Record<string, number>;
  /** Staging places that are a coast watch (the grand campaign): one with no corps within three marches is sent one by rail. */
  watch?: string[];
  /**
   * The capitals of allies this staff guards while their leaving is in play, and the corps each is to have (ruling 233,
   * riders b612 and b627). A guard is named once and kept: it goes by rail as far as the trains run and marches on.
   */
  ward?: Record<string, number>;
  minOdds?: number;
  force?: number;
  /** Places the plan means to starve out rather than storm: never assaulted while the phase lasts. */
  siege?: string[];
  /** In a theatre: enemy garrisons left to others. Nobody attacks them, closes up to them or stays to watch them. */
  mask?: string[];
  /** A limited offensive: only places the plan has named as objectives so far are attacked. */
  limited?: boolean;
  /** A single stroke: only these places are attacked, whatever else is in reach. */
  targets?: string[];
  /** The grand campaign: this phase is a staff's main effort for the season, aimed at this province (its id). */
  effort?: number;
  /**
   * The grand campaign: the places the staff's named offensive is aimed at this month, on its main front alone
   * (where 'targets' holds every live front's). Its air groups are based in reach of these battles (ruling 227, b447).
   */
  main?: string[];
  /** The grand campaign: the month a main effort is named. The army masses opposite the target and attacks nowhere. */
  massing?: boolean;
  /** The grand campaign: how many waves a staff pressing a won war will send at one place in a month (two otherwise). */
  waves?: number;
  /**
   * An army kept to the letter of its orders: besides attacking only its targets, nobody steps into ground the
   * enemy has left empty, or counter-attacks to win ground back, unless the place is one of them.
   */
  strict?: boolean;
  /** The phase only applies while these two nations are at war with each other. */
  ifAtWar?: [string, string];
  /** A war for a strip of frontier: only corps within this many marches of the front close up to it (the Winter War, ruling 228). */
  near?: number;
  /** Corps sent across the sea, each to the first place on its list it can reach: an enemy coast is assaulted, a friendly port reinforced. */
  landings?: { corps: string; with?: string[]; to: string[]; minOdds?: number }[];
  /** Harbours the nation holds without owning them from which the main plan's named landings may also sail (the far side: Japan's quays on Hainan and in Indochina, ruling 200 and the planner's paper 18). */
  quays?: string[];
  /**
   * Strikes the plan orders of a carrier fleet in the month the blow falls: the fleet by name and the enemy harbour its
   * wing goes for (the far side: the carrier striking force on Pearl Harbor, ruling 200(c)). `home`, where the row
   * has it, is the base the fleet makes for once the strike is flown, in the same month (a base, the harbour's name,
   * its sea's key and the news line, as a row of navy.json 'moves').
   */
  strikes?: { fleet: string; harbour: string; home?: { base: string; port: string; home: string; words: string } }[];
  /**
   * An expedition on someone else's ground that leaves rather than be lost with it: once the enemy holds any of the
   * places in 'when', its corps make for a port and are taken off, to one of the theatre's havens or else home.
   */
  withdraw?: { when: string[] };
  /** Stand on the defensive without local counter-attacks: the plan is saving the army for a later stroke. */
  quiet?: boolean;
  /** False for a plan that allows each place one assault a month: an attack that fails is not renewed until the next. */
  renew?: boolean;
  /** Places the plan wants at a price of their own: the least odds at which each is attacked, in place of minOdds. */
  oddsFor?: Record<string, number>;
  /**
   * Sectors of the front handed over to an ally's troops, by ally. Once two of that ally's corps stand in such a
   * place, the nation counts it as held: it sends no reserves there and takes its own corps out, all but one.
   */
  entrust?: Record<string, string[]>;
  /**
   * Fast troops gathered for the next offensive while the line stands on the defensive: place, and how many tank and
   * lorried corps to have there. Each comes by rail out of a part of the line that keeps two corps without it, and
   * infantry makes room for it where the place is full.
   */
  concentrate?: Record<string, number>;
  /**
   * Ground the plan gives up before the road home is cut. Corps standing there march out toward home, those
   * nearest home first so that the ones behind find room, and no reserves are sent back into it.
   */
  abandon?: string[];
  /**
   * In a side theatre, where several phases may cover the same months: this one is the plan only while the enemy
   * holds every place named here. The first phase in the list whose month and conditions fit is the one followed.
   */
  ifLost?: string[];
  /** In a side theatre: the place this phase falls back on when it gives ground up, in place of the theatre's usual base. */
  base?: string;
  /** Orders for named fleets while the phase lasts. */
  fleets?: { fleet: string; zone: string; stance: Stance }[];
}

// ---- The war in the air -----------------------------------------------------------------

/**
 * What an air fleet is doing this month. Left on support it flies for the armies and defends
 * its own sky; the others are month-long air campaigns against a region or a sea.
 */
/** What an air fleet is doing. 'ground' is a raid on troops, flown only in the grand campaign's air war. */
/** 'fleet' is a strike on one fleet at sea (its target is that fleet's id) and 'harbour' a raid on the ships lying in a port: the grand campaign's carriers and air-sea war. 'town' is a raid on the defences of a town an army means to take (the grand campaign's sieges). */
/** 'line' is a raid on the fieldworks of an enemy line (the grand campaign): the bombers take months off the works of the troops dug in there. */
/** 'airfield' is a raid on one enemy airfield (airfields as targets, behind its switch): its runway, hangars and the aircraft parked there. */
/** 'seapatrol' is a standing patrol of one sea against submarines (release 2, Units decide, once it is live; seapatrol.ts): its target is the sea. */
export type AirMission = 'support' | 'superiority' | 'bomb' | 'night' | 'strike' | 'rest' | 'ground' | 'fleet' | 'harbour' | 'town' | 'line' | 'airfield' | 'seapatrol';

export interface AirLeader {
  name: string;
  level: number;
  xp: number;
}

/**
 * One bomber of a group, kept by name (bombers.ts): the number of its name on the nose-art list (0: none yet, a new
 * replacement), which of that name in this group it is (1, or 2 for 'II'), the missions the aircraft has flown (the
 * bombs painted on its nose), the fighters its gunners have shot down, the experience its present crew arrived with
 * from the schools, and the missions that crew has flown (its tour runs on these: a bomber outlives its first crew).
 */
export type BomberRow = [art: number, ord: number, missions: number, kills: number, head: number, crew: number, by?: Record<string, number>];

/**
 * One bomber of a group as an air battle is handed it (bombers.ts seatsOf; battle/air AirPlace.bombers): what the 3D
 * game paints on it, and what the battle hands back about it. Shown, never counted.
 */
export interface BomberSeat {
  /** What it is called ('Prize Pumpkin', 'G for George', '5J+DM') and its marking as painted ('KM-G', 'K-312'): both '' for a new aircraft not yet named. */
  name: string;
  mark: string;
  /** Its number on the nose-art list, for a nation that paints nose art (America alone); 0 for a bare nose, and for every other nation. */
  art: number;
  /** The missions it has flown (a bomb painted for each), the aircraft its gunners have shot down (a symbol for each), and whose they were. */
  missions: number;
  kills: number;
  by?: Record<string, number>;
  /** Its present crew's missions, and what their experience is called. */
  crew: number;
  grade: string;
  /** How likely a loss of the group is to be this one, beside the others: a green crew's the likeliest (stories.json bombers.risk). */
  risk: number;
  /** The campaign's own row for it, handed back as it is with what the battle saw of it. */
  row: BomberRow;
}

/** A group's bombers for an air battle: how its nation marks them (bomber-marks.ts), and the seats, old hands first. */
export interface BomberSeats {
  style: string;
  /** The missions from which a bomber's loss is told by name, and what its nation calls one ('mission', 'operation'). */
  told: number;
  flown: string;
  seats: BomberSeat[];
}

/** What an air battle saw a bomber's gunners shoot down: the bomber's row as the battle was handed it, and how many of each nation's. */
export interface BomberShot { row: BomberRow; by: Record<string, number> }

/** A bomber group's aircraft one by one, and what became of those that have gone (bombers.ts). */
export interface BomberRoster {
  /** The bombers on strength, in the order they joined: as many as the group has bombers. */
  on: BomberRow[];
  /** The group's bomber losses already struck off here: AirFleet.lost.bmr when it was last made up. */
  seen: number;
  /** How many of this group have carried each name, by its number. */
  used: Record<number, number>;
  /** Crews that finished their tour and went home, and bombers with a name that were lost. */
  home: number;
  gone: number;
  /** The last of them, oldest first: the month, how it went, and the bomber as it was. */
  roll: { t: number; how: 'home' | 'lost'; b: BomberRow }[];
  /** Draws made so far, so that no two of this group's are the same. */
  n: number;
  /** The month crews last finished a tour here, and what that has added to the group's readiness in it. */
  lift?: [turn: number, by: number];
}

/** Where a nation's bomber crews go when they finish a tour: the training schools, a war bond tour, or one and then the other. */
export type CrewOrder = 'school' | 'bonds' | 'split';

/** A nation's bomber crews who have finished a tour (bombers.ts): the player's standing order, and what those at home are doing. */
export interface CrewsHome {
  order: CrewOrder;
  /** Crews at the schools as instructors: new bombers arrive with more experience for them. */
  school: number;
  /** Bond tours under way: the months each has left. Each pays the treasury every month. */
  bonds: number[];
  /** Crews sent home so far, and the production points their bond tours have brought in. */
  n: number;
  sold: number;
}

export interface AirFleet extends Storied {
  id: number;
  nat: string;
  name: string;
  /** Region its airfields are in. */
  base: number;
  /** Fighters with pilots, and bombers. */
  ftr: number;
  bmr: number;
  /** The strength it is kept at. */
  est: { ftr: number; bmr: number };
  mission: AirMission;
  /** A region for superiority and bombing, a sea for a strike on shipping or a sea patrol, -1 for none. */
  target: number;
  /** 0..1: how fit the fleet is to fly. */
  ready: number;
  leader: AirLeader | null;
  /** Month it last changed base. */
  moved: number;
  /** Aircraft lost since the war began. */
  lost: { ftr: number; bmr: number };
  /** Wings bought from the arsenal in the grand campaign, kept on top of the strength the staff allots. */
  extra?: { ftr: number; bmr: number };
  /** The grand campaign's air war: the one kind of aircraft this air group flies. Unset, the fleet is mixed, as history lists it. */
  kind?: 'ftr' | 'bmr';
  /** The command the group was formed from: the name of its row in air.json, which still sets what it is kept at and how far it reaches. */
  parent?: string;
  /** Its share of that command's aircraft of its kind. */
  share?: number;
  /** The month it last flew a raid planned by its owner: one raid a month. */
  flew?: number;
  /** The month it last flew a reconnaissance: one a month, beside its raid. */
  scouted?: number;
  /** A fighter group's standing order to stick with one bomber group: that group's id. It goes on every day raid the bombers fly, as far as its fighters reach. */
  escorts?: number;
  /**
   * A land fighter group's standing order 'Ground support' (the grand campaign, air.json 'close'): it flies over its
   * own side's land battles within its fighters' reach, and does not meet raids or escort.
   */
  close?: boolean;
  /** The week it last flew over a battle on that order, as month x 10 + week: one battle a week. */
  closeAt?: number;
  /** The air campaign it flies in (ruling 236.2): the campaign's id, while the campaign stands. */
  camp?: number;
  /** A carrier wing: the id of the fleet whose carriers it flies from. It goes where that fleet goes and has no airfield (the grand campaign). */
  ship?: number;
  /**
   * A carrier wing's room a deck, where it is not its navy's (carriers.ts `wingSize`): a lone light carrier's share
   * (ruling 189(b)), or the navy's wing by date (the rider to rulings 189 and 202). Kept so that a change of room is
   * known for what it is: nothing is given and nothing lost by it. Absent while the room is the navy's own.
   */
  size?: number;
  /**
   * A carrier wing ordered ashore: the id of the fleet whose decks it left. It is based on an airfield like any air
   * group, with its own aircraft, pilots and fitness, and keeps a carrier wing's reach and its crews' training.
   */
  ashore?: number;
  /**
   * The country that raised it, where it now answers to another: a fallen country's group on far ground that passed
   * into keeping (ruling 199, pacific.ts keptForces). Its row in air.json is still read under the country that raised it.
   */
  raised?: string;
  /**
   * The far side (ruling 184): the months running that its field has not been fed, counted as its country's month in
   * the air opens. A group on an unfed field counts nothing toward the air over a sea. Unset while it is fed.
   */
  unfed?: number;
  /** Its bombers one by one, where its nation's bombers carry names (bombers.ts): each with its missions and its gunners' kills. */
  bombers?: BomberRoster;
  /**
   * Fighters bought abroad, by type, as shares of the group (the switch 'Fighter types'; 07:00Z): a bought wing keeps the
   * seller's type, flown by this nation's pilots, and never upgrades with the seller's research. The rest is its own.
   */
  bought?: Record<string, number>;
  /**
   * Units decide's quality store (DD 24 Nov 15:53Z, #2 form Part 2), written only under that switch: its pilots' mean
   * hours before first combat. None while its nation's school (or deck school, for a carrier wing) is not laid.
   */
  hours?: number;
  /** Its one experience counter, 0 to 1, written only under that switch. */
  fxp?: number;
  /**
   * Under that switch: the pilots who joined it in month `on`, how many, and its experience before the first of them
   * (`was`), for the card's refit note (the UI director 24 Nov 18:10Z).
   */
  joined?: { on: number; n: number; was: number };
  /**
   * On its way to another airfield by stages (release 2, the switch 'ferry'; ferry.ts), written only under that switch:
   * where it is going, the fields still to land at in order (the last is `to`), or `sea` when it goes crated by sea, and
   * the month and the hops it has flown in it. Out of the fight while it is set. `base` is the field it has reached.
   */
  way?: { to: number; stops: number[]; sea?: boolean; at?: [number, number] };
  /**
   * (Release 2, better air unit names; the switch 'airNames') Written only as a group is named under that switch: the
   * short form for the map counter, where its rung has one, and true where its name itself says fighters or bombers
   * (so the role is not shown beside it). Words only; no rule reads them.
   */
  short?: string;
  roleNamed?: boolean;
  /**
   * (Release 2, earned names under the switch 'nicknames') A group of an arm that gives the Guards title, once it has
   * earned it: its number in the Guards series, and the name it bore before. Words only; no rule reads them.
   */
  guards?: number;
  was?: string;
  /**
   * (Release 2, earned names for air groups under the switch 'nicknames'; airnicks.ts, Brian's answer of 25 November
   * 2026 03:31Z) Its missions completed: the count (a hard fight counts double), the month it last completed one (one a
   * month), whether that month's was already counted double, and its hard fights in all. Read by no rule but the naming.
   */
  done?: { n: number; at: number; x: number; h: number };
  /**
   * (Release 2, earned names under the switch 'nicknames'; airnicks.ts, the design director 12:59Z question 5) The place
   * of its latest hard fight in this war: a region id, or -1 for one over no region (at sea). Written whenever a mission
   * of its is hard; the city of a Guards title, and the place of a place name, is taken from it once, when the name is
   * given. Absent until its first hard fight, and in a save from before it: no city then. Words only; no rule reads it.
   */
  hardAt?: number;
  /**
   * (The same) Its earned colours: the scheme it wears from its nation's palette (the 3D side's, data/campaign/
   * air-colours.json 'count' per nation), the tier of marking reached (1 spinner, 2 nose band, 3 tail) and the month
   * it was first earned. Words and paint only: no edge.
   */
  colours?: { scheme: number; tier: number; since: number };
}

/** A nation's reserve of aircraft and fighter pilots, and its tally for the war. */
export interface AirPool {
  ftr: number;
  bmr: number;
  pilots: number;
  lostFtr: number;
  lostBmr: number;
  lostPilots: number;
}

export interface AirSide {
  nat: string;
  fleet: string;
  /** The air fleet or group itself, and the region whose airfield it flew from: enough to draw the flight again. */
  id?: number;
  base?: number;
  /** A carrier wing: where its fleet was on the map when it flew. */
  at?: [number, number];
  leader: string | null;
  ftr: number;
  bmr: number;
  lostFtr: number;
  lostBmr: number;
}

export interface AirReport {
  id: number;
  turn: number;
  mission: AirMission;
  /** The region fought over, or -1 for a strike at sea. */
  region: number;
  /** The sea struck, or -1. */
  zone: number;
  place: string;
  attNat: string;
  defNat: string;
  att: AirSide[];
  def: AirSide[];
  lines: TicketLine[];
  odds: number;
  level: VictoryLevel;
  won: boolean;
  /** True when the attacker won the sky over the region. */
  sky: boolean;
  /** In a fight for the sky: the attacker's fighters left at the end against what the defender can still put up. */
  hold: number;
  /** What the bombers did, in a sentence. */
  result: string;
  /** A raid fought out as an air battle (src/battle/air): the bombers that bombed, for its after card. */
  dropped?: number;
  /** A group caught by enemy fighters on its way to another airfield (ferry.ts): the field it was flying to. Never set with that switch off. */
  ferry?: number;
  /** The grand campaign: what the raid achieved, on the scale its losses are counted on (one aircraft is 1). */
  worth?: number;
  /** A strike on a fleet or a raid on a harbour: the fleets hit, as they were before it, and what each lost. */
  ships?: SeaSide[];
  /** The fleet a strike was aimed at. */
  fleet?: number;
  /** True when a harbour was caught unready in the month war was declared. */
  surprise?: boolean;
  /** Damaged cities (the grand campaign with its switch): what the bombs did to the town, in percent points added, and where it now stands. */
  city?: { region: number; added: number; now: number };
  /** Airfields as targets (behind its switch): what the bombs did to the field: whether its runway is shut, the levels burned now, the aircraft destroyed on the ground. */
  field?: { region: number; shut: boolean; levels: number; ground: number };
  /**
   * A raid on troops with the strafers' rule on (release 2, strafers.ts): the names of the raid's fighter groups that
   * went down on the troops, as the report names them, in the raid's order; empty where none did. Written only with
   * the switch on; a report without it (the switch off, or an older save) says nothing of strafing.
   */
  strafed?: string[];
}

/** Ruling 236.2: how a battle for the sky over one province stands, for the side fighting for it. */
export type SkyState = 'contested' | 'held' | 'lost';

/**
 * The battle for the sky over one province (ruling 236.2): the side fighting for it, where it stands, and the months
 * running it has gone that side's way (above 0) or the defender's (below 0). It carries over from month to month.
 */
export interface SkyFight {
  by: string;
  state: SkyState;
  streak: number;
  /** The month of the last fight, and the month the fight began. */
  turn: number;
  since: number;
  /** The month it was won, while held. */
  won?: number;
  /** This month's best result so far (2 the defender's fields knocked out, 1 a month won, 0 even, -1 lost), and where the fight stood before it. */
  best: number;
  was: { state: SkyState; streak: number };
}

/**
 * What the bombs have kept down in one province, of one kind ('works' its output, 'oil' its oilfield or fuel plants),
 * ruling 236.2: damage carries over and comes back at the historian's rate for the target (air-campaign-236).
 * 'v' is the share held down at the end of month 'at'; 'land' what raids have landed for later months; 'carry' what
 * is still down this month from earlier raids, after the month's repair.
 */
export interface BombStock {
  region: number;
  kind: 'works' | 'oil';
  by: string;
  victim: string;
  v: number;
  rate: number;
  at: number;
  land: Record<number, number>;
  carry: number;
  /** This month: what came back, and what the raids did. */
  mended: number;
  done: number;
  /** The last raid that landed here: its month, its side and what it did. */
  last?: { turn: number; by: string; cut: number };
}

export type CampaignKind = 'sky' | 'bomb' | 'support';

/** One month of an air campaign: where it went, what it sent and lost, and what it did (ruling 236.2). */
export interface CampaignMonth {
  turn: number;
  target: number;
  sent: number;
  lost: number;
  downed: number;
  done: number;
  sky?: SkyState;
}

/**
 * An air campaign (ruling 236.2): a standing order to an air force, flown every month until it is called off. It
 * holds its targets, its groups and escorts, day or night, and what it has done month by month.
 */
export interface AirCampaign {
  id: number;
  nat: string;
  kind: CampaignKind;
  name: string;
  targets: number[];
  /** The next target in turn, for a bombing campaign. */
  next: number;
  groups: number[];
  escorts: number[];
  night: boolean;
  /** What a bombing campaign aims at: the works, the oil, or the railways. */
  aim: 'works' | 'oil' | 'rail';
  since: number;
  /** The month it ended, and why. */
  off?: number;
  why?: string;
  /** The row of the campaign table it was opened from, at the stops that follow the book. */
  book?: number;
  log: CampaignMonth[];
}

/** The air war's memory (ruling 236.2): the campaigns, the battles for the sky, and the damage the bombs left. */
export interface AirWarMemory {
  campaigns: AirCampaign[];
  next: number;
  sky: Record<number, SkyFight>;
  stock: Record<string, BombStock>;
  /** The last month each nation's ground was bombed by night (b569(2): its staff reaches for Fighter control). */
  night?: Record<string, number>;
}

/**
 * A corps on a long passage by sea (ruling 229, part 1; the design director's b460): it left `from` in its own
 * country's harbours and lands at `to`, or the nearest harbour its side holds on that front, in month `due`. While
 * at sea it stands nowhere: it is off the map and out of every count but its country's field army.
 */
export interface Voyage { nat: string; corps: Corps; from: number; to: number; due: number }

// ---- Units decide's quality store (release 2; DD 24 Nov 15:53Z, #2 form Part 2) ---------------------------------
// The nation holds what its schools and staffs know; the unit holds what its own men have lived through. Every arm has
// the same three parts: a school level per nation, a doctrine per nation, and the unit's own figure (the corps' xp, an
// air group's hours and fxp, a fleet's crew). Each cause has one price, in one place. Seeded by units.ts openQuality,
// from data/campaign/units.json and the tables it names; absent while the switch is off.

/**
 * A doctrine slot. Readers read `id` only: a change ordered by the player, chosen by a staff or taught by a research row
 * goes in `next`, and takes over in month `ready`, so the old way holds while the new one trains in.
 */
export interface Way<T extends string> { id: T; next?: T; ready?: number; by: 'start' | 'chosen' | 'research' }

/** A land staff's way (data/battle/lean.json): 'mass' attacks by orders; night and last-stand are fought as bold until they are built. */
export type LandWay = 'mass' | 'methodical' | 'bold' | 'night' | 'last-stand';

/** What a nation's army knows. Training rows, command c and the drafts' level are worked out from it and the tree, never stored. */
export interface Land {
  /** The army's training at the start (officers, NCOs, drill): a seed only. The level read is the higher of it and the ladder's rung for the training rows held. */
  train: number;
  /** Command earned in war, 0 to 1. Command c = the radios row's share if held, plus this, at most 1. */
  earned: number;
  /** The drafts' schools: new men and new corps join at the level they give. */
  school: number;
  /** The staff's way, attacking and defending. */
  way: { att: Way<LandWay>; def: Way<LandWay> };
}

/** What a nation's air arm knows. Nothing reads these slots yet: they wait on the air thread's build. */
export interface Air {
  /** Hours before first combat a new pilot leaves school with; null where not laid. */
  school: number | null;
  /** The same for carrier pilots, kept apart; null where not laid. */
  deck: number | null;
  /** The level read wherever hours are not priced: today's air quality (air.json). */
  held: number;
  /** The reserve's mean hours (null while school is) and mean experience. */
  pool: number | null;
  poolXp: number;
  /** Land bombers' training against ships (carriers.json naval.land). */
  strike: number;
  /** Fighter doctrine; null where no row is read. */
  fighter: Way<'turn' | 'dive-zoom'> | null;
  /** Escort doctrine; null where not laid. */
  escort: Way<'tied' | 'free'> | null;
}

/** What a nation's navy knows. Nothing reads these slots yet: they wait on the naval thread's build. */
export interface Sea {
  /** A new crew's level from the navy's schools, in navy.json's quality units. */
  crew: number;
  /** Night drill and lookouts' training: null, not laid. */
  night: number | null;
  lookout: number | null;
  /** The fleet staff's way. */
  way: Way<'methodical' | 'bold' | 'mass'>;
  /** Convoy doctrine: boats hunt alone, are homed from ashore, or are led at sea; and whether they go for warships first. */
  pack: Way<'alone' | 'homed' | 'led'>;
  target: Way<'merchants' | 'warships'>;
}

export interface Arms { land: Land; air: Air; sea: Sea }

export interface QualityStore {
  /** The month the campaign opened: 1, or a later start's first month (g.flags.openFrom). */
  from: number;
  /** Every nation in g.nations, seeded by one rule. */
  nat: Record<string, Arms>;
  /** News fired once when a cause passes a mark: '<nat>:<field>:<mark>' to the month it fired. */
  news: Record<string, number>;
}

export interface Game {
  seed: number;
  rng: Rng;
  turn: number;
  /** Index into order of the nation whose phase it is. */
  phase: number;
  order: string[];
  nations: Record<string, Nation>;
  regions: Region[];
  byName: Record<string, Region>;
  corps: Corps[];
  /** Every general of the war, by id. Generals who are killed, captured or sacked stay on the list. */
  generals: General[];
  nextCorps: number;
  nextBuild: number;
  human: string | null;
  reports: BattleReport[];
  headlines: Headline[];
  flags: Record<string, number>;
  supplyDirty: boolean;
  /** Regions each nation can draw supply in, rebuilt when the map changes. */
  supply: Record<string, Uint8Array>;
  /** How long each nation's supply line is in every region deep inside the Soviet Union. Rebuilt with supply. */
  reach: Record<string, Float32Array>;
  /**
   * The grand campaign: how many provinces of road lie between each province a nation draws supply in and its
   * nearest working railway (0 on it, -1 where it draws none). Emptied whenever supply is redrawn and filled
   * nation by nation as it is asked for (railheads() in rails.ts); never saved.
   */
  railhead: Record<string, Int16Array>;
  capDist: Record<string, Int16Array>;
  week: number;
  seas: SeaZone[];
  gates: Gate[];
  fleets: Fleet[];
  nextFleet: number;
  seaReports: SeaReport[];
  /** Coasts shelled from the sea, oldest first (the grand campaign). */
  shoreReports: ShoreReport[];
  /** Trade agreements in force (the grand campaign). */
  trade: TradeDeal[];
  /** Last month's result on each convoy route, by route id. */
  convoys: Record<string, ConvoyMonth>;
  /**
   * Release 2, with Units decide and the sea patrol live (seapatrol.ts): the boats each nation's aircraft on patrol sank
   * in its month, by the boats' nation ('sunk'), by sea ('seas'), and by sea and the boats' nation ('at'), which the
   * boats' own side reads as its boats lost to aircraft there (seasight.ts); and its part of the watch its patrols kept
   * against the boats on station in each sea that month, by sea and the boats' nation ('met': the parts of a boat's time
   * watched by its aircraft with anti-submarine kit and without), which the boats' own side reads as the aircraft its
   * boats met there.
   */
  patrolSunk?: Record<string, { turn: number; sunk: Record<string, number>; seas: Record<number, number>; at?: Record<number, Record<string, number>>; met?: Record<number, Record<string, { kit: number; none: number }>> }>;
  /** Release 2 (columns.ts), the grand campaign with supply columns on: each nation's lorry columns of its last month, by nation. Absent otherwise. */
  columns?: Record<string, ColumnRun[]>;
  /** Release 2 (logistics.ts), forward dumps on: the dumps stocked or being stocked beyond the railheads. Absent otherwise. */
  dumps?: Dump[];
  /** Release 2 (logistics.ts), moving the lorries on: the front each player nation sends its lorries to first, by nation. Absent otherwise. */
  lorryFirst?: Record<string, string>;
  /** Release 2 (ruling 241 D1), record only, written only while the submarine scene's viewing setting is on: this month's and last month's hunts, torpedoings and convoy months, for the scene to stage. Absent otherwise. */
  subLook?: SubLook[];
  airFleets: AirFleet[];
  nextAir: number;
  airReports: AirReport[];
  /** Each nation's reserve of aircraft and pilots. */
  airPool: Record<string, AirPool>;
  /** Who has won the sky over a region, and in which month. */
  sky: Record<number, { nat: string; turn: number }>;
  /** The share of a nation's production stopped by bombing and strikes on its coastal trade, and the month it happened. */
  raids: Record<string, { turn: number; share: number }>;
  /** The roll of honour, the memorial wall, rivalries and the rest of what the unit stories keep. */
  stories: Stories;
  /** The player's secret service (the grand campaign): the agents, what they have reported, and where the alarm is up. */
  intel: Intel;
  /** The roll of nations that have left the war and of those that came back into it, oldest first. */
  passings: Passing[];
  /** Corps at sea on a long passage, off the map until the month they land (ruling 229, part 1: the Cape). Absent while there are none. */
  atSea?: Voyage[];
  /** The air campaigns (ruling 236.2, the grand campaign with the switch on): absent while the switch is off. */
  ac?: AirWarMemory;
  /**
   * (Release 2, better air unit names; air.ts groupName under the switch 'airNames') The forming-order position of each
   * nation's series of names ('at', keyed nation:kind:rung, or nation:day:rung, or nation:base:province) and the largest
   * number handed out in it ('top'). Absent until a group is named under the switch. Words only.
   */
  airNames?: { at: Record<string, number>; top: Record<string, number> };
  /**
   * (Release 2, earned names for air groups under the switch 'nicknames'; airnicks.ts) Each air arm's running tally by
   * kind of mission, nation:mission: aircraft that flew it and aircraft lost on it, for its running loss rate. Absent
   * while that switch is off. Read by no rule but the naming.
   */
  airRates?: Record<string, [number, number]>;
  /** A neutral's bargains (bargains.ts; release 2): absent while the switch is off. */
  bargains?: BargainBook;
  /** The volunteer legions (legions.ts; release 2): absent while the switch is off. */
  legions?: LegionBook;
  /**
   * Units decide's quality store (release 2; DD 24 Nov 15:53Z, #2 form Part 2): what each nation's schools and staffs
   * know, for soldiers, pilots and crews. Absent while the switch is off; read and seeded only through units.ts
   * qualityStore.
   */
  qs?: QualityStore;
  /**
   * Scarred ground (release 2): for each region a land battle was fought in, how many have been fought there and the
   * month of the last, and of those how many in the mud and the month of the last of them (mud memory), so a battle
   * there is fought over ground already torn up. Kept apart from the reports, which a
   * save keeps only the last of. Absent until the first battle.
   */
  scars?: Record<number, { n: number; last: number; mudN?: number; mudLast?: number }>;
  /**
   * Old wrecks (release 2, looks only): for each region a land battle was fought in, the armoured vehicles its battles
   * left burnt out or knocked out on the field, by nation (game.ts wreckBump), and the month of the last that left any.
   * Absent until the first; nothing in any fight reads it.
   */
  wrecks?: Record<number, { n: Record<string, number>; last: number }>;
  /**
   * Mines at sea (release 2, mines.ts): each field's sea (`z`), the nation that laid it, its strength (`s`, 1 new, worn
   * by the months and the sweepers), the month laid (`t`), and the enemies who have found it (`known`). Kept only under
   * that switch.
   */
  mines?: { z: number; nat: string; s: number; t: number; known?: string[] }[];
  /**
   * Captured kit (release 2, looks only): by nation, the enemy tanks it lately took in a heavy win, whose they were, how
   * many tank blocks they make, and the month; kept six months, the three latest.
   */
  captured?: Record<string, { from: string; n: number; turn: number }[]>;
  /**
   * A queue for a town (release 2, townqueue.ts; the design director's ruling seamless-r2-1125.md part B): by nation,
   * the buildings the player has queued, oldest first, each the line of building, the province and the level it is to
   * be raised to. Each is a press of BUILD made for him at the opening of his month. Kept only under the switch
   * 'townQueue', and only while something is queued.
   */
  townQueue?: Record<string, { region: number; line: string; level: number }[]>;
  links: [number, number][];
  /** The chart's west edge. 0 on the map of Europe; further west (negative) once the wider world is attached (world.ts). */
  x0: number;
  /** How wide the chart is from its west edge, and how tall. */
  width: number;
  height: number;
  land: [number, number][][][];
}

/**
 * A nation leaving the war, or coming back into it. Both interfaces announce these.
 */
export interface Passing {
  id: number;
  turn: number;
  week: number;
  kind: 'fell' | 'freed';
  nat: string;
  /** Who it fell to, or who freed it. */
  by: string;
  /** Beaten in the field, taken over whole by treaty, or divided by the terms of a peace; or freed. */
  how: 'beaten' | 'annexed' | 'terms' | 'freed';
  /** Its capital (a region id), or -1 for a country that has none on the map. */
  capital: number;
  /** For a fall: the home provinces that passed out of its hands. For a return: the provinces handed back to it that day. */
  provinces: number;
  /** For a fall: how many weeks of war it had been in. */
  weeks?: number;
  /** For a return: the corps it put in the field, and the alliance it joined. */
  corps?: number;
  side?: 'allies' | 'axis' | 'league';
  /** For a return that nobody fought for at the capital: the nation whose fall left that capital in friendly hands. */
  after?: string;
  /** True if this was the player's own country. */
  player?: boolean;
}

/** What an agent is doing: watching, reading a government's books, an act of sabotage, a theft of plans, or work at home (guarding, hunting enemy agents). */
export type Mission = 'watch' | 'books' | 'factory' | 'rail' | 'oil' | 'airfield' | 'orders' | 'bbPlans' | 'cvPlans' | 'bomberPlans' | 'fighterPlans' | 'jetPlans' | 'rocketPlans' | 'guidedPlans' | 'guard' | 'hunt';

/**
 * Plans a smaller nation holds of a weapon only the great powers make (Brian, 18 November; ruling 235 and its rider
 * b542): a battleship, a carrier, or one of four technologies. Whose they are, how it got them, and when.
 */
export interface PlanHeld { what: 'bb' | 'cv' | 'heavybombers' | 'jets' | 'rockets' | 'guided'; from: string; how: 'taken' | 'stolen'; turn: number }

/** A spy: a piece the player recruits, sends abroad and may lose. */
export interface Agent {
  id: number;
  nat: string;
  /** A codename. */
  name: string;
  /** What the agent has learned from work that came off: rank is earned from it. */
  xp: number;
  reg: number;
  /** The month from which the agent is at work where it stands. Before it the agent is in training or on the road. */
  ready: number;
  mission: Mission;
  /** The month it was recruited. */
  born: number;
  /** After an act of sabotage: the month until which the agent must lie low. */
  rest?: number;
  /** Quiet months of work abroad, counted toward experience. */
  months?: number;
  /**
   * (Double agents, Brian, 22 November, release 2.) The month the enemy turned this agent, and the service it now
   * works for. Kept from the player: no view he is sent reads them. Only its reports give it away.
   */
  turned?: number;
  turnedFor?: string;
  /**
   * (Release 2, the spies thread of 24 November 2026.) Set on an agent of the network the player's service opens the
   * war with (intel.ts layNetwork), so that the options page can take the network back in the campaign's first month.
   */
  net?: boolean;
}

/**
 * An enemy agent the player turned (Brian, 22 November, release 2: double agents): it stays in its own service and
 * works for him, warning of every new agent that service sends against him and opening its books, until it is blown.
 */
export interface DoubleAgent {
  id: number;
  /** A codename from intel.json 'names'. */
  name: string;
  /** The service it seems to work for, and the player's nation it truly works for. */
  of: string;
  by: string;
  /** The month it was turned. */
  since: number;
}

/**
 * An agent of the other side's service on the player's home ground (ruling 151(c), spy hunters): hidden until one of
 * his hunters uncovers it, it strikes in its month unless it is arrested first or a guard stops it.
 */
export interface FoeAgent {
  id: number;
  /** The service it works for. */
  nat: string;
  reg: number;
  /**
   * What it means to strike at; or 'watch' (Brian, 22 November, release 2: 'Busier, mostly watchers'), an agent that
   * never strikes and maps the province's defences for its own side's attacks.
   */
  kind: 'factory' | 'rail' | 'airfield' | 'watch';
  /** The month it strikes; for a watcher, the month from which its side's attacks on the province go in harder. */
  strike: number;
  /** The month a hunter of the player's uncovered it, if one has. */
  found?: number;
}

/**
 * An enemy air staff's order for next month, read by the player's agent (ruling 135, spy orders): the groups on one
 * field, the target, by day or by night. The staff flies it in its month if it still can; a stolen order is never false.
 */
export interface StolenOrder {
  /** The staff whose order it is, and the field its groups fly from. */
  nat: string;
  field: number;
  groups: number[];
  target: number;
  mission: 'bomb' | 'night';
  bombers: number;
  /** The month it is to be flown. */
  turn: number;
  /** The agent who read it, and the player's side. */
  agent: string;
  by: string;
  /** Once its month has come: flown, or why it lapsed. */
  done?: 'flown' | 'lapsed';
}

/** A line in the secret service's file: what happened, to whom, where, and whether it was good news. */
export interface IntelNote { turn: number; agent: string; nat: string; reg: number; what: string; text: string; good: boolean }

/** How a side saw the other at sea: its convoys attacked, its escort met them, its patrols sighted them, it sank them, its search found them, or its boats were kept down or attacked from the air. */
export type SightHow = 'attack' | 'incident' | 'sighting' | 'loss' | 'search' | 'keptDown';

/**
 * What one side last saw of the other in one sea (seasight.ts): the enemy boats it saw on station there, and the enemy
 * aircraft on patrol its own boats met there. Each part has its own month, since a side may watch a sea for boats in a
 * month its own boats are not there, and the other way round.
 */
export interface SeaSight {
  /** The enemy boats seen on station there (an expected count, as the convoy count is), the month, and how they were seen. */
  boats?: { t: number; n: number; how: SightHow[] };
  /** The enemy patrol as its own boats met it there: the parts of a boat's time watched by aircraft with anti-submarine kit and without (seapatrol.ts Watch), its boats lost to aircraft there, the month, and how. */
  air?: { t: number; kit: number; none: number; lost: number; how: SightHow[] };
}

export interface Intel {
  agents: Agent[];
  next: number;
  notes: IntelNote[];
  /** Provinces on alert after an act of sabotage, and the month the alert ends. */
  heat: Record<number, number>;
  /** Provinces the player's aircraft have photographed, and the month they did. */
  scout?: Record<number, number>;
  /** Foreign corps the player's side has identified (in battle, by an agent, from the air), and the month it last did. */
  known?: Record<number, number>;
  /** How each of those corps was last identified. */
  how?: Record<number, 'battle' | 'agent' | 'air'>;
  /** Seas the player's aircraft have searched, and the month they did. */
  sea?: Record<number, number>;
  /** Seas each computer staff's aircraft have searched, and the month they did (ruling 52): by nation, then by sea. */
  seaBy?: Record<string, Record<number, number>>;
  /**
   * Where each side last saw the other at sea (release 2, Units decide, written and read only while the sea patrol is
   * live; seasight.ts): by nation, then by sea, the enemy boats it last saw there and the enemy aircraft its own boats
   * last met there, each with its month. Absent until the patrol is live.
   */
  seaSight?: Record<string, Record<number, SeaSight>>;
  /**
   * Fog on raids (airfog.ts; only with its switch on): the provinces each nation's aircraft have photographed and the
   * month they did, by nation, then by province. A flight gives sight for the month flown and the next.
   */
  scoutBy?: Record<string, Record<number, number>>;
  /**
   * Fog on raids (airfog.ts; only with its switch on): what each nation last saw of the air defence of an enemy
   * province, by nation, then by province: [the month, the enemy fighters in reach of its sky (to the nearest ten),
   * the groups they were in, the level of its guns, the percent of the town in ruins].
   */
  airSeen?: Record<string, Record<number, number[]>>;
  /**
   * Cut railways as the player last knew them (rulings 113 and 114): the month, whether he saw the cut or has it by
   * the report of his own raid, agent or troops, and the cause where it was known. One record a province.
   */
  cuts?: Record<number, { t: number; how: 'seen' | 'report'; why?: 'bombed' | 'agent' | 'saboteurs' | 'retreat' | 'hasty' | 'order' }>;
  /** The other side's agents on the player's ground (ruling 151(c)), and the next id. */
  foes?: FoeAgent[];
  foeNext?: number;
  /** Enemy air orders the player's agents have read (ruling 135). */
  orders?: StolenOrder[];
  /** The player's double agents in enemy services (Brian, 22 November, release 2), and the next id. */
  doubles?: DoubleAgent[];
  doubleNext?: number;
}

export const MEN_PER_DIV = 15000;

export const KIND: Record<Kind, { label: string; atk: number; def: number; move: number; mobile: boolean }> = {
  inf: { label: 'Infantry', atk: 10, def: 14, move: 2, mobile: false },
  mot: { label: 'Motorised', atk: 12, def: 12, move: 1, mobile: true },
  arm: { label: 'Armor', atk: 24, def: 11, move: 1, mobile: true },
  cav: { label: 'Cavalry', atk: 8, def: 8, move: 1, mobile: true },
  mtn: { label: 'Mountain', atk: 9, def: 15, move: 2, mobile: false },
  fort: { label: 'Fortress', atk: 1, def: 15, move: 99, mobile: false },
  // Massed guns, raised only in the grand campaign: see the artillery rule in the battle assessment.
  art: { label: 'Artillery', atk: 14, def: 8, move: 2, mobile: false },
  // Engineers, raised only in the grand campaign: they are there for what they build and breach, not for their rifles.
  eng: { label: 'Engineers', atk: 4, def: 9, move: 2, mobile: false },
  // Support teams (release 2, src/campaign/teams.ts): a machine-gun brigade and a mortar brigade, a division's worth
  // each. Their own weight is small; what they do besides (a defence stiffened against foot, works broken up) is on the
  // attack card. Never raised while the teams are off.
  mg: { label: 'Machine guns', atk: 3, def: 7, move: 2, mobile: false },
  mor: { label: 'Mortars', atk: 6, def: 4, move: 2, mobile: false },
};

export const TERRAIN_DEF: Record<Terrain, number> = { plains: 1, forest: 1.2, hills: 1.25, mountain: 1.6, marsh: 1.3, urban: 1.35, desert: 1 };
export const FORT_MUL = [1, 1.3, 1.7, 2.6];
export const STACK = 5;
