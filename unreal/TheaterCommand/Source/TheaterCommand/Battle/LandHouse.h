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
 * line (their steps' edges worn away, so none stands up alone; here and there a break slanting where the stones came off
 * at an angle), rubble of the walls' own stone heaped at their feet under its dust, the stone darkened by the fire, a
 * black plume of soot licked up over every opening, the inside's plaster sooted and fallen away in patches to the
 * stone, charred joist stubs, fallen beams, one gable left standing, ragged along its rake, with its chimney.
 *
 * The farm's barn and byre (release 2, the same trial; Brian 24 November, "then the other houses"), built of the same
 * stone, roof and hand as its house, their footprints, eaves and pitch as LandField lays them: the barn's tall walls with
 * its cart doors under an oak lintel, two ledged and braced plank leaves on their pintles (one now and then standing
 * open on the dark), slits for its air, a pitching door to the hay loft in each gable, no chimney and no gutter; the
 * byre low, its two stable doors (the upper leaf open on one), a small barred window beside each, a pitching door to its
 * loft in a gable. Inside where they are hurt, bare rubble, unplastered, sooted where they burnt.
 *
 * The village church (Kind 3) whole, as LandField's LayChurch stands it: the nave (its Long and Deep) at the middle, the
 * chancel east of it (+x), the tower west; rubble walls under a corbelled cornice, round-headed windows leaded in
 * quarries under rings of voussoirs, the south door's oak under its hood mould; the tower in ashlar, its string courses,
 * a louvred belfry, a parapet with four pinnacles, a stone spire with four lucarnes, its cross and gilt cock. Only the
 * nave is ever hurt (its roof holed).
 *
 * The village's houses (Kind 6, LandField's HtHouse; release 2, the same trial: "then the other houses"), each to its own
 * footprint, storeys (one to three), roof and hurt as LandField lays it, in the farmhouse's hand: its bays, its windows
 * and shutters, its door, its chimneys, its damage. Their walls the bourg's: the bocage's rubble, the cream limestone,
 * or (about half) a lime render over the rubble, off-white, cream, grey, buff or a faded ochre, its quoins and its
 * openings' dressed stone left bare, the render come away to the stones in patches, low along the foot where the damp
 * lifts it and here and there up the walls (more on a house that was hit, most on a shell; along a broken top a band of
 * it gone), and the pits of shell splinters and bullets in it, a spray round a burst or a line along a burst of fire. Now
 * and then a shop on the ground floor of a house of two storeys: pilasters and a fascia under its cornice applied to the
 * wall round it, its display windows over a stall riser, its glazed door set back; its boards up or down; its glass
 * broken by a blast, gone in a wreck; burnt, charred, its fascia half gone.
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
		int Kind = 0;     // 0 the farmhouse, 1 its barn, 2 its byre (LandField's HtFarmhouse, HtBarn, HtByre; Storeys unread for these),
		                  // 3 the church whole (its nave's Long and Deep; Roof, Storeys unread; Hurt 0 or 1), 6 a village house (HtHouse),
		                  // 7 the east's log izba (Roof 0 planks, 1 thatch; Storeys, Hurt unread), 8 the steppe's whitewashed khata
		                  // (its hipped thatch; Roof, Storeys, Hurt unread), 9 a burnt-out house, its stove and chimney standing
		                  // (Roof 0 an izba's logs burnt down, 1 a khata's clay walls roofless; Storeys, Hurt unread), 10 the desert's
		                  // flat-roofed house round its walled yard (Roof 0 limewashed, 1 its bare mud, 2 limewashed, its main room of
		                  // two storeys; Storeys, Hurt, Snow unread)
		double Snow = 0.0; // the snow lying (LandField's cover, 0 to 1): an east house's roof built under its blanket of snow
	};

	struct Levels
	{
		LandField::Mesh Near, Mid, Far;
	};

	/** The eaves' height over the floor and the ridge's, metres. */
	double EavesOf(const Spec& S);
	double RidgeOf(const Spec& S);

	/** Where its windows and doors are, for the pictures and the tests: each opening's middle on the wall's outer face
	 * (its own frame), its sill and head heights, its width, and what it is (0 a window, 1 the door, 2 an attic window; the
	 * barn's and byre's: 3 the cart doors, 4 a slit, 5 a loft's pitching door, 6 a stable door, 7 a byre's barred window;
 * a church's, its nave's only: 9 a round-headed window, 10 its door; a village house's shop: 12 its front; and on a
 * village house's render, 13 where it has come away to the rubble, 14 a strike in it; a burnt-out house's, 15 its stove's
 * mouth, 16 its chimney from where its plaster ends to its top, 17 its stove's side to the room; a desert house's: 0 a
	 * window, 1 a door, 18 its yard's gate, 19 its oven, 20 its stair, 21 a spout, 22 its shade, 23 its water jars, 24 a
	 * place its plaster has come away under a parapet, 25 its wing's roof, 26 a wall's top never plastered, 27 a wall's
	 * foot the damp has stripped, 31 a niche over its door, 34 the rug over its wing's parapet). */
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
