#pragma once

#include "Battle/LandField.h"

#include <cstdint>
#include <vector>

/**
 * A Norman farmhouse built to the vehicles' detail (release 2, Brian 24 November 2026: "can we make the buildings the
 * same level of detail as the vehicles?"; the pilot, behind the bocage's trial word "houses", -TheaterLandTrial=houses).
 *
 * The bocage's farmhouse as LandField lays it (its footprint, storeys, roof and hurt unchanged, so the rules and the
 * sight are as before), built as three levels, as a vehicle is. Two looks by its key: the bocage's grey-brown schist and
 * granite rubble with pale dressed limestone quoins and surrounds (about two in three), or cream limestone throughout.
 * - near (about 30 to 40 thousand triangles; a shell some 16 thousand): random rubble of every size, 13 to 95 cm, each
 *   stone its own pillow set proud of wide lime joints, courses that wander, now and then a through-stone or a tall one
 *   standing up into the course above; dressed quoins and jambs in alternate long and short blocks, lintels (a keystone
 *   over the door) and projecting sills; deep reveals; two-leaf casements with their glazing bars, the sky in their panes;
 *   plank shutters on their ledges and brace, open against the wall or shut; a ledged plank door, weathered oak or old
 *   paint, on strap hinges, with its fanlight and stone step; a damp plinth; the roof's Norman kick at the eaves, slate by
 *   slate (or flat tile by tile) in staggered courses, each butt standing proud of the course below, with its verges,
 *   half-round ridge tiles, rafter feet, a zinc gutter and downpipes; stone chimney stacks at the gables with their caps
 *   and clay pots; iron wall anchors; a water butt;
 * - middle (about 4 to 5.5 thousand), drawn from some 90 m: the same stone laid as cells, each stone its own colour as
 *   the eye mixes it with its mortar from there, the dressed work where the near level lays it; the roof's courses in
 *   bands;
 * - far (under a thousand), drawn from 300 m: its masses, the quoins painted up its corners, the windows' glass, the
 *   roof's two slopes. Each level's wall matches the near one's within a few levels of each colour at its switch.
 *
 * Damage as LandField's (DmRoofHole, DmGable, DmShell): a hole torn in the roof with its battens and charred rafters
 * showing, slates slipped round it and fallen below, the attic open; a gable fallen, the walls broken down to it stone by
 * stone, the roof's end broken off over protruding rafters and purlins, the upper floor's joists sticking out over a
 * heap of rubble, the chimney gone; a burnt-out shell, roofless, its walls' tops broken stone by stone along a ragged
 * line (their steps' edges worn away, so none stands up alone), rubble heaped at their feet, soot licked up above every
 * opening, charred joist stubs, fallen beams, one gable left standing, ragged along its rake, with its chimney.
 *
 * Its own frame: metres, x along its length, y across it (its door on the +y side), z up from its floor (the floor
 * LandField's FloorOf gives it); the origin at the middle of its footprint. Placed at a house's middle, turned by the
 * house's yaw (and a half turn where its door faces -V). Colours 8-bit sRGB, as the field's; triangles wound
 * counter-clockwise seen from their front.
 */
namespace LandHouse
{
	struct Spec
	{
		double Long = 14.0, Deep = 7.2;
		int Storeys = 2;
		int Roof = 0;     // 0 slate, 1 flat clay tile
		int Hurt = 0;     // 0 whole, 1 its roof holed, 2 a gable fallen, 3 burnt out to a shell
		uint64_t Key = 1; // its look: the stone's tone, the shutters' paint and which stand open, the chimneys, the damage's side
		double Below = 2.2; // how far its plinth runs down below its floor (the ground falls away under it on a slope)
	};

	struct Levels
	{
		LandField::Mesh Near, Mid, Far;
	};

	/** The eaves' height over the floor and the ridge's, metres. */
	double EavesOf(const Spec& S);
	double RidgeOf(const Spec& S);

	/** Where its windows and door are, for the pictures and the tests: each opening's middle on the wall's outer face
	 * (its own frame), its sill and head heights, its width, and what it is (0 a window, 1 the door, 2 an attic window). */
	struct Hole
	{
		int Face = 0; // 0 the front (+y), 1 the gable at -x, 2 the back, 3 the gable at +x
		double X = 0.0, Y = 0.0, Z0 = 0.0, Z1 = 0.0, Wide = 0.0;
		int What = 0;
	};
	void Holes(const Spec& S, std::vector<Hole>& Out);

	/** The farmhouse's three levels (each emptied first). */
	void Build(const Spec& S, Levels& Out);
}
