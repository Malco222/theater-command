// The tank crews' names (b477, Brian's pick), from the historian's final table (historian/tank-names-b477.json, 13:16Z
// 18 Nov 2026), kept to the names that fit the army's line tank in the modeller's names-fit.json (13:20Z): the
// Sherman for America; the Matilda for Britain (a British Sherman has room for any of them); the T-34 and T-34-85
// for the Soviet Union; the Char B1, the M13/40, the 7TP and the Type 97 (her ten with characters drawn, 14:05Z; the
// other ten are never given). Germany paints kill rings, never names (b477(4)); its list is kept only for the record. Every name is
// invented, or a common word, place or first name with no one person behind it. 'en' is the English meaning for the
// hover line; where the name is English, or a place, it is the name itself. The 3D game paints a name from its model's
// 'fits' entry under the same name (the rows, in Russian letters for the Soviet lines, and the letter height).

export interface CrewName { name: string; en: string }

const same = (xs: string[]): CrewName[] => xs.map((name) => ({ name, en: name }));
const pairs = (xs: [string, string][]): CrewName[] => xs.map(([name, en]) => ({ name, en }));

export const CREW_NAMES: Record<'USA' | 'UK' | 'FRA' | 'SOV' | 'GER' | 'ITA' | 'JAP' | 'POL', CrewName[]> = {
  USA: same(["Alabama Belle", "Ace High", "Atlas", "Buckeye", "Blue Ridge Rambler", "Brass Hat", "Cactus Kate", "Cornhusker", "Cyclone", "Dakota Dust", "Desert Rose", "Dixie Doodle", "Erie Lass", "Eight Ball", "Jersey Lightning", "Lone Star Lady", "Lucky Seven", "Ozark Queen", "Prairie Fire", "Texas Twister"]),
  UK: same(["Agincourt", "Ardent", "Aberdour", "Badger", "Bramble", "Kent Lass", "Wolfhound", "Cheshire Cat", "Clydesdale", "Cormorant", "Doughty", "Dorset Lad", "Hambledon", "Heather", "Kestrel", "Moorhen", "Wayfarer", "Stalwart", "Tynesider", "Moorland"]),
  FRA: same(["Aunis", "Quercy", "Vivarais", "Rouergue", "Limousin", "Morvan", "Durance", "Ardèche", "Vézère", "Loing", "Autan", "Bise", "Orage", "Grêle", "Sologne", "Camargue", "Cévennes", "Margeride", "Bigorre", "Dombes"]),
  SOV: pairs([["Za Rodinu", "For the Motherland"], ["Mstitel", "Avenger"], ["Smelyy", "Bold"], ["Vpered na zapad", "Forward to the West"], ["Za nashu Moskvu", "For our Moscow"], ["Za Odessu", "For Odessa"], ["Bogatyr", "Warrior of the old tales"], ["Sokol", "Falcon"], ["Burya", "Storm"], ["Molniya", "Lightning"], ["Podarok frontu", "A gift to the front"], ["Kolkhoznik", "The collective farmer"], ["Ne otstupim", "We will not retreat"], ["Pobeda", "Victory"], ["Sibiryak", "The Siberian"], ["Volga", "Volga"], ["Groza", "Thunderstorm"], ["Druzhba", "Friendship"], ["Za Tulu", "For Tula"], ["Ural", "Ural"]]),
  GER: pairs([["Liesel", "Liesel"], ["Gretl", "Gretl"], ["Annemie", "Annemie"], ["Bärbel", "Bärbel"], ["Rübezahl", "Mountain spirit of the old tales"], ["Gewitter", "Thunderstorm"], ["Hagelschlag", "Hailstorm"], ["Stier", "Bull"], ["Igel", "Hedgehog"], ["Kuckuck", "Cuckoo"], ["Heimweh", "Homesick"], ["Schwarzwald", "Black Forest"], ["Allgäu", "Allgäu"], ["Waldschrat", "Forest goblin"], ["Kleiner Bär", "Little bear"], ["Spätzle", "Noodles"], ["Wetterhexe", "Weather witch"], ["Dickhäuter", "Thick-skin"], ["Feldmaus", "Field mouse"], ["Nebelkrähe", "Hooded crow"]]),
  ITA: pairs([["Rosina", "Rosina"], ["Ninetta", "Ninetta"], ["Vesuvio", "Vesuvius"], ["Etna", "Etna"], ["Tuono", "Thunder"], ["Leone", "Lion"], ["Cicala", "Cicada"], ["Grillo", "Cricket"]]),
  JAP: pairs([["Inoshishi", "Boar"], ["Washi", "Eagle"], ["Hōō", "Phoenix"], ["Fūjin", "Wind spirit"], ["Raijin", "Thunder spirit"], ["Shishi", "Lion"], ["Ōkami", "Wolf"], ["Kiji", "Pheasant"], ["Fuji", "Mount Fuji"], ["Taka", "Hawk"]]),
  POL: pairs([["Ryś", "Lynx"], ["Kania", "Kite"], ["Dzik", "Boar"], ["Smok", "Dragon"], ["Wisła", "Vistula"], ["Warta", "Warta"], ["Mazur", "The Masurian"], ["Kurpie", "Kurpie"], ["Wawel", "Wawel"], ["Jadzia", "Jadzia"], ["Halinka", "Halinka"], ["Kasia", "Kasia"], ["Śmiały", "Bold"], ["Wierny", "Faithful"], ["Bystry", "Swift"], ["Sokół", "Falcon"], ["Góral", "Highlander"]]),
};
