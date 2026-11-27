// Generated people. Every pilot, hero and commander who is not a historical figure
// gets a name from these pools, drawn with the seeded generator.
import type { Rng } from './rng';

const POOLS: Record<string, { first: string[]; last: string[]; ranks: string[]; leader: string }> = {
  UK: {
    first: ['Alan', 'Arthur', 'Basil', 'Charles', 'Colin', 'David', 'Denis', 'Douglas', 'Edward', 'Eric', 'Frank', 'Geoffrey', 'George', 'Harold', 'Henry', 'Hugh', 'Ian', 'Jack', 'James', 'John', 'Kenneth', 'Leslie', 'Michael', 'Norman', 'Patrick', 'Peter', 'Philip', 'Ronald', 'Roy', 'Stanley', 'Thomas', 'Walter', 'William'],
    last: ['Abbott', 'Baker', 'Barrow', 'Bennett', 'Carver', 'Chapman', 'Cooper', 'Dalton', 'Dawson', 'Ellis', 'Fenwick', 'Fletcher', 'Garrett', 'Gibbs', 'Harcourt', 'Hayes', 'Holt', 'Jarvis', 'Kemp', 'Lambert', 'Marsh', 'Mercer', 'Naylor', 'Osborne', 'Parrish', 'Pryce', 'Radcliffe', 'Sadler', 'Sharpe', 'Tanner', 'Thorne', 'Underhill', 'Wainwright', 'Whitlock', 'Yardley'],
    ranks: ['Sgt', 'Sgt', 'P/O', 'P/O', 'F/O', 'F/Lt'],
    leader: 'S/Ldr',
  },
  PL: {
    first: ['Adam', 'Antoni', 'Bogdan', 'Jan', 'Jerzy', 'Kazimierz', 'Marian', 'Stefan', 'Tadeusz', 'Witold', 'Zbigniew'],
    last: ['Borowski', 'Czajka', 'Dudek', 'Gajewski', 'Kaczmarek', 'Lis', 'Majewski', 'Nowicki', 'Pawlak', 'Sobczak', 'Wrona', 'Zielinski'],
    ranks: ['Sgt', 'P/O', 'P/O', 'F/O', 'F/Lt'],
    leader: 'S/Ldr',
  },
  CZ: {
    first: ['Alois', 'Bohumil', 'Emil', 'Frantisek', 'Josef', 'Karel', 'Ladislav', 'Otakar', 'Vaclav', 'Vilem'],
    last: ['Benes', 'Cerny', 'Dolezal', 'Havel', 'Kolar', 'Kucera', 'Marek', 'Novotny', 'Pokorny', 'Vesely'],
    ranks: ['Sgt', 'P/O', 'P/O', 'F/O', 'F/Lt'],
    leader: 'S/Ldr',
  },
  CA: {
    first: ['Arthur', 'Bruce', 'Dale', 'Gordon', 'Howard', 'Lloyd', 'Murray', 'Ross', 'Russell', 'Wallace'],
    last: ['Beaumont', 'Carmichael', 'Dufresne', 'Fraser', 'Gauthier', 'Lachance', 'MacLeod', 'Munro', 'Pelletier', 'Sinclair'],
    ranks: ['P/O', 'P/O', 'F/O', 'F/O', 'F/Lt'],
    leader: 'S/Ldr',
  },
  DE: {
    first: ['Albert', 'Bruno', 'Dieter', 'Erich', 'Ernst', 'Franz', 'Friedrich', 'Georg', 'Gerhard', 'Hans', 'Heinz', 'Helmut', 'Horst', 'Joachim', 'Karl', 'Kurt', 'Ludwig', 'Martin', 'Otto', 'Paul', 'Rolf', 'Rudolf', 'Siegfried', 'Walter', 'Werner', 'Wilhelm'],
    last: ['Ahrens', 'Bader', 'Brandt', 'Dietz', 'Eckert', 'Fuchs', 'Gruber', 'Hahn', 'Hartwig', 'Jung', 'Keller', 'Kessler', 'Lang', 'Lorenz', 'Maurer', 'Neumann', 'Pohl', 'Reuter', 'Sauer', 'Schubert', 'Seidel', 'Thiel', 'Vogt', 'Wendt', 'Winkler', 'Ziegler'],
    ranks: ['Uffz', 'Uffz', 'Fw', 'Ofw', 'Lt', 'Oblt'],
    leader: 'Hptm',
  },
};

export function makeName(rng: Rng, pool: string, leader: boolean): { name: string; rank: string } {
  const p = POOLS[pool] ?? POOLS.UK;
  const rank = leader ? p.leader : rng.pick(p.ranks);
  return { name: `${rng.pick(p.first)} ${rng.pick(p.last)}`, rank };
}
