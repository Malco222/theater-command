import type { Rng } from '../core/rng';
import type { TicketLine, VictoryLevel } from '../core/ticket';

export type Kind = 'inf' | 'mot' | 'arm' | 'cav' | 'mtn' | 'fort' | 'art' | 'eng';
export type Terrain = 'plains' | 'forest' | 'hills' | 'mountain' | 'marsh' | 'urban' | 'desert';
export type Align = 'allies' | 'allies_cobelligerent' | 'leaning_allies' | 'independent' | 'leaning_axis' | 'axis_cobelligerent' | 'axis';

export interface Region {
  id: number;
  name: string;
  owner: string;
  ctrl: string;
  terrain: Terrain;
  vp: number;
  fac: number;
  cap: boolean;
  port: boolean;
  fort: { lv: number; vs: string[] } | null;
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
  work: { id: string; left: number; of: number }[];
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
  /** An airborne corps, unlocked by the technology tree (the grand campaign). */
  airborne?: boolean;
  /** A corps raised again round its cadre (the grand campaign): the id of the corps that was folded away. */
  cadre?: number;
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

export interface Corps extends Storied {
  id: number;
  nat: string;
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
  /** It was attacked since its own last month ended (the grand campaign): it does no digging this month. */
  pressed?: boolean;
  /** It took over fieldworks left on this ground this month (the grand campaign): it keeps them at the month's end though it marched. */
  relief?: boolean;
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
  /** In garrison inside its province's town since this month (the grand campaign): it is on the region's `garr` list, not among its `units`. */
  town?: number;
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
  /** The share of the defence that was dug in and in supply, where any was (the grand campaign): luck ran against the attack by that much. */
  prepared?: number;
  /** An attack thrown back from dug-in troops (the grand campaign): the months it took off their works, and the months of works they have left. */
  worn?: number;
  works?: number;
  /** The fight was for the town itself: an assault on its garrison (the grand campaign). */
  town?: boolean;
  /** The field was won but the town holds out: the province is under siege, not taken (the grand campaign). */
  siege?: boolean;
  /** The field was won and the town, held only by its guard, opened its gates without a fight (the grand campaign). */
  gates?: boolean;
  /** A relief: the attackers came to raise a siege of their own side's town. */
  relief?: boolean;
  /** The garrison of a besieged town came out against the army round it. */
  sally?: boolean;
}

// ---- The war at sea ---------------------------------------------------------------------

export type ShipClass = 'bb' | 'cv' | 'ca' | 'dd' | 'ss';
export type Ships = Record<ShipClass, number>;
/**
 * What a fleet is doing: holding a sea, hunting merchant ships, or lying in harbour. In the grand campaign it can also
 * be bound to a shipping lane as its escort, or sent to a sea to hunt submarines.
 */
export type Stance = 'patrol' | 'raid' | 'port' | 'escort' | 'hunt';

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
  /** Region of the home port, or -1 when the base is off the map. */
  base: number;
  /** What the base is called: Scapa Flow, Wilhelmshaven. */
  port: string;
  /** The sea the base opens on. */
  home: number;
  /** The sea the fleet is in. A fleet in harbour is counted in its home sea but takes no part there. */
  zone: number;
  stance: Stance;
  /** Where a computer-run fleet goes when it has nothing better to do, and what it does there. */
  station: number;
  duty: Stance;
  /** A fleet tied to its station is never sent anywhere else by the computer. */
  tied: boolean;
  /** 0..1: how fit for sea the ships are. Battle and long patrols wear it down; time in harbour restores it. */
  ready: number;
  admiral: Admiral | null;
  /** Month the fleet last fought. */
  fought: number;
  /** The shipping lane the fleet is escorting, while its stance is 'escort' (the grand campaign). */
  lane?: string;
  /** Month the fleet last shelled a coast: its guns speak once a month (the grand campaign). */
  shelled?: number;
  /** The month each nation last had this fleet in contact: it fought it, was shelled by it, or was struck by its aircraft (the grand campaign). */
  seen?: Record<string, number>;
  /** The month it was last struck from the air, how many times that month (a fleet struck once is scattered and warned), and the sea it was in (-1 in harbour). */
  struck?: [number, number, number?];
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
}

export interface SeaSide {
  nat: string;
  fleet: string;
  admiral: string | null;
  ships: Ships;
  sunk: Ships;
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
  /** The grand campaign: how many waves a staff pressing a won war will send at one place in a month (two otherwise). */
  waves?: number;
  /**
   * An army kept to the letter of its orders: besides attacking only its targets, nobody steps into ground the
   * enemy has left empty, or counter-attacks to win ground back, unless the place is one of them.
   */
  strict?: boolean;
  /** The phase only applies while these two nations are at war with each other. */
  ifAtWar?: [string, string];
  /** Corps sent across the sea, each to the first place on its list it can reach: an enemy coast is assaulted, a friendly port reinforced. */
  landings?: { corps: string; with?: string[]; to: string[]; minOdds?: number }[];
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
export type AirMission = 'support' | 'superiority' | 'bomb' | 'night' | 'strike' | 'rest' | 'ground' | 'fleet' | 'harbour' | 'town' | 'line';

export interface AirLeader {
  name: string;
  level: number;
  xp: number;
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
  /** A region for superiority and bombing, a sea for a strike on shipping, -1 for none. */
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
  /** A carrier wing: the id of the fleet whose carriers it flies from. It goes where that fleet goes and has no airfield (the grand campaign). */
  ship?: number;
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
  /** The grand campaign: what the raid achieved, on the scale its losses are counted on (one aircraft is 1). */
  worth?: number;
  /** A strike on a fleet or a raid on a harbour: the fleets hit, as they were before it, and what each lost. */
  ships?: SeaSide[];
  /** The fleet a strike was aimed at. */
  fleet?: number;
  /** True when a harbour was caught unready in the month war was declared. */
  surprise?: boolean;
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
  links: [number, number][];
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

/** What an agent is doing: watching, reading a government's books, an act of sabotage, or guarding the home country. */
export type Mission = 'watch' | 'books' | 'factory' | 'rail' | 'oil' | 'guard';

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
}

/** A line in the secret service's file: what happened, to whom, where, and whether it was good news. */
export interface IntelNote { turn: number; agent: string; nat: string; reg: number; what: string; text: string; good: boolean }

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
}

export const MEN_PER_DIV = 15000;

export const KIND: Record<Kind, { label: string; atk: number; def: number; move: number; mobile: boolean }> = {
  inf: { label: 'Infantry', atk: 10, def: 14, move: 2, mobile: false },
  mot: { label: 'Motorized', atk: 12, def: 12, move: 1, mobile: true },
  arm: { label: 'Armor', atk: 24, def: 11, move: 1, mobile: true },
  cav: { label: 'Cavalry', atk: 8, def: 8, move: 1, mobile: true },
  mtn: { label: 'Mountain', atk: 9, def: 15, move: 2, mobile: false },
  fort: { label: 'Fortress', atk: 1, def: 15, move: 99, mobile: false },
  // Massed guns, raised only in the grand campaign: see the artillery rule in the battle assessment.
  art: { label: 'Artillery', atk: 14, def: 8, move: 2, mobile: false },
  // Engineers, raised only in the grand campaign: they are there for what they build and breach, not for their rifles.
  eng: { label: 'Engineers', atk: 4, def: 9, move: 2, mobile: false },
};

export const TERRAIN_DEF: Record<Terrain, number> = { plains: 1, forest: 1.2, hills: 1.25, mountain: 1.6, marsh: 1.3, urban: 1.35, desert: 1 };
export const FORT_MUL = [1, 1.3, 1.7, 2.6];
export const STACK = 5;
