// Real dogfights (release 2, row realDogfights; Brian, 27 November 2026: "i never see the fighters shooting each other
// down or dog fighting", "it looks like they loop around vs actually realistic dog fights", "i want the pilots AI to be
// very smart and realistic as real fighter pilots back then"). Included by AirShow.cpp at its end, inside namespace
// AirShow: the pilots' brain for a fighter against fighters while Show::SetDogfights is on. Off, nothing here is called
// and the fights fly as before, byte for byte.
//
// What it flies, from the historian's doctrine (air-battles/dogfights-r2/DOCTRINE.md, its rules of thumb):
// - surprise is the kill: he sees what his eyes, the sun and his own target let him see (SpotChance);
// - the height first: the diving types (109, 190, P-47, P-51, P-38, Hellcat, Corsair) bounce from above and out of the
//   sun, make one pass, zoom back up on the speed of it and set up again; never round and round (DogEnergy);
// - the turning types (Spitfire, Hurricane, Zero, Oscar, Yak) turn, with yo-yos over and under the turn (DogAngles);
// - close in and short bursts: a green pilot opens at 400 m and more and hoses three-second bursts, an ace closes to
//   150 m and fires a second; nobody fires past the deflection he can lay (DogShoot);
// - each type's own figures: its speeds, its dive limit and the stiffening past it, its turn, its roll at speed, its
//   climb (FitType, RollAtSpeed; Fly);
// - the breaks by type: the energy fighter half-rolls and dives away, the turner breaks into him; the Navy pair weave
//   (DogFlees, Evading);
// - one hit or out of ammunition goes home straight and low, far from the fight, his wingman with him (DogAway).
// The battle still decides who falls (ruling 217): the scene only finds the pilot to show it. A loss the battle has
// counted is shown under the guns of the group it named: the one of that group best placed comes in behind the one who
// fell, the one who fell flies on with his eyes on his own target, and the guns bear (DogExecute, DogMarked).

	// ---------------------------------------------------------------------------------------------------------------
	// The types.

	void Show::FitType(Craft& C)
	{
		if (!FindType || !C.bFighter) return;
		const std::string Key = C.Model + "|" + C.Nation;
		auto It = Types.find(Key);
		if (It == Types.end())
		{
			TypeFigures F;
			if (!FindType(C.Model, C.Nation, F)) F.Style = -1;
			It = Types.emplace(Key, F).first;
		}
		const TypeFigures& F = It->second;
		if (F.Style < 0) return;
		// (the scene's own fighting speeds about the table's: the slowest a fight is flown at is over the stall by what a
		// pilot keeps in hand; the fastest, the type's own; each held inside what the pilots' own flying was made for)
		if (F.VMin > 0.0) C.VMin = Clamp(F.VMin + 28.0, 70.0, 95.0);
		if (F.VMax > 0.0) C.VMax = Clamp(F.VMax, 118.0, 175.0);
		if (F.VTop > 0.0) C.VTop = Clamp(F.VTop, C.VMax + 15.0, 245.0);
		C.VDive = std::max(C.VTop, std::min(F.VDive, 265.0));
		if (F.RMin > 0.0) C.RMin = Clamp(F.RMin, 170.0, 460.0);
		if (F.GMax > 0.0) C.GMax = Clamp(F.GMax, 4.0, 7.0);
		if (F.RollMax > 0.0) C.RollMax = Clamp(F.RollMax, 50.0 * Deg, 170.0 * Deg);
		if (F.RollTime > 0.0) C.RollTime = Clamp(F.RollTime, 0.1, 0.45);
		C.RollPeakV = F.RollPeakV;
		C.RollFast = F.RollFast;
		C.RollFastV = F.RollFastV;
		C.ClimbRate = F.Climb;
		C.Style = F.Style;
	}

	double Show::RollAtSpeed(const Craft& C, double Speed) const
	{
		// The wartime roll charts: rising to its peak at one speed, falling off beyond it as the ailerons stiffen, to its
		// high-speed rate and on down (never under a third of that). By the speed the air gives the ailerons (indicated:
		// thinner air up high gives them less).
		if (C.RollPeakV <= 0.0 || C.RollFastV <= C.RollPeakV || C.RollFast <= 0.0) return C.RollMax;
		const double Shown = Speed * std::sqrt(std::exp(-std::max(0.0, C.P.Z) / 9300.0));
		const double Peak = C.RollMax;
		if (Shown <= C.RollPeakV) return Peak * Clamp(0.45 + 0.55 * (Shown - 40.0) / std::max(1.0, C.RollPeakV - 40.0), 0.45, 1.0);
		const double Fall = (Peak - C.RollFast) / (C.RollFastV - C.RollPeakV);
		return std::max(Peak - Fall * (Shown - C.RollPeakV), C.RollFast / 3.0);
	}

	int Show::StyleOf(const Craft& C, const Group& G) const
	{
		if (C.Style > 0) return C.Style;
		return G.Style == "dive" ? 2 : G.Style == "both" ? 3 : 1;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Eyes.

	double Show::SpotChance(const Craft& C, const Craft& F) const
	{
		// Each look behind him (three a second): the chance he picks out an enemy fighter coming for him. A good pilot
		// sees more; his partner beside him doubles his eyes (LookBehind).
		double P = 0.05 + 0.12 * C.Skill;
		const Vec Rel = F.P - C.P;
		const double D = std::max(1.0, Len(Rel));
		const Vec Way = Rel * (1.0 / D);
		// Out of the sun: within 15 degrees of it, all but unseen until he is close; half as easy within 30.
		const double Sun = Between(Way, Norm(SunWay, Up));
		if (Sun < 15.0 * Deg) P *= D < 300.0 ? 0.5 : 0.12;
		else if (Sun < 30.0 * Deg) P *= 0.5;
		// From below and behind, under his tail: the blind cone.
		const Vec Nose = Ahead(C.Hdg, C.Gam);
		if (Rel.Z < -0.25 * D && Way.Dot(Nose) < -0.5) P *= 0.35;
		// His own eyes on his own target: a pilot tracking or firing sees nothing else.
		if (C.bFiring || C.Burst > 0.0) P *= 0.15;
		else if (C.Target >= 0 && (C.Step == Phase::Run || C.Step == Phase::Chase || C.Step == Phase::Turn))
		{
			const Craft* T = Find(C.Target);
			if (T && Len(T->P - C.P) < 800.0) P *= 0.4;
		}
		return P;
	}

	bool Show::DogFlees(const Craft& C, const Group& G, const Craft& F, bool bHigh, double Over)
	{
		// An enemy close behind him and seen: the energy fighter half-rolls and dives away (the 190's and the 109's way
		// out, the P-47's dive, the Wildcat's against the Zero), the turner breaks into him; either, by which of the two
		// turns the tighter. Away from one that cannot follow him down, the more.
		const double D = Len(F.P - C.P);
		if (!bHigh || Over < 1200.0 || D > 700.0) return false;
		const int Style = StyleOf(C, G);
		const bool bOutTurned = F.RMin < C.RMin * 0.9;
		double P = Style == 2 ? 0.7 : Style == 3 ? (bOutTurned ? 0.6 : 0.15) : (bOutTurned ? 0.25 : 0.05);
		if (C.VDive > F.VDive + 10.0) P += 0.15;
		return DogRoll.U() < P;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The guns.

	void Show::DogShoot(Craft& C, Craft& T, const Group& G, double Dt, double Least)
	{
		// He opens at his own range (green 400 m and more, an ace 150), for a burst of his own length (a green pilot
		// hoses three seconds and more on his tracer, an ace a second, re-aiming between), only with the nose on the
		// lead and inside the deflection he can lay (10 degrees green, 45 an ace; past it he holds his fire).
		// (one the battle has counted lost at his group's guns: the burst that brings him down may come from a little
		// farther out, as a good burst does)
		const double Owed = DueOn(T.Group, C.Group) > 0 ? 320.0 : 0.0;
		const double Open = std::max(std::max(Least, Owed), 450.0 - 250.0 * C.Skill);
		const double Most = (12.0 - 6.0 * C.Skill) * Deg;
		const Vec Rel = T.P - C.P;
		const double Cross = std::acos(Clamp(std::fabs(Ahead(T.Hdg, T.Gam).Dot(Norm(Rel))), 0.0, 1.0));
		const bool bDares = Cross < (10.0 + 35.0 * C.Skill) * Deg;
		const double Lo = 0.5 + 1.2 * (1.0 - C.Skill), Hi = 1.2 + 2.6 * (1.0 - C.Skill);
		Squeeze(C, T, G, Open, bDares ? Most : 0.0, Lo, Hi, Dt);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A loss the battle counted, shown under the guns that made it.

	bool Show::DogMarked(const Craft& C) const
	{
		for (const auto& Pair : Crafts)
		{
			const Craft& Q = Pair.second;
			if (Q.Executes == C.Key && Q.Side != C.Side && Live(Q) && Q.Doing == Role::Fight && Len(Q.P - C.P) < 2200.0) return true;
		}
		return false;
	}

	bool Show::DogInbound(const Event& E) const
	{
		// One of the group whose guns made a loss going in behind one of the group it was made on to show it, and
		// closing on him: the loss waits on him while he comes (Advance), as it waits on one all but in reach (Inbound).
		if (E.What != "air:lost" || BoxOf(E) >= 0) return false;
		for (const auto& Pair : Crafts)
		{
			const Craft& S = Pair.second;
			if (S.Group != E.By || S.Executes < 0 || !Live(S) || S.Doing != Role::Fight || S.bScored) continue;
			const Craft* T = Find(S.Executes);
			if (!T || !Live(*T) || T->Group != E.Unit || (E.Strag >= 0 ? T->StragId != E.Strag : T->StragId >= 0)) continue;
			const Vec Rel = T->P - S.P;
			const double D = Len(Rel);
			if (D < 2500.0 && (Velocity(S) - Velocity(*T)).Dot(Rel) > 15.0 * D) return true;
		}
		return false;
	}

	bool Show::DogExecute(Craft& C, const Group& G, double Dt)
	{
		// A loss the battle has counted on an enemy fighter group at his group's guns, which the scene has yet to show:
		// the one of his group best placed to have one of that group goes in behind him now, leader or wingman, one to
		// each, and the loss is shown when his guns bear. The battle decided it; the scene only finds the pilot to act it.
		if (!C.bFighter) return false;
		// (one the battle has counted lost himself, an enemy coming in behind him to show it, has his eyes on his own man
		// and goes for nobody: DogDuel flies him steady, and a chain of pilots each running from the one behind him at
		// full throttle, nobody closing, never forms)
		if (DogMarked(C))
		{
			if (C.Executes >= 0)
			{
				C.Executes = -1;
				if (C.Step == Phase::Chase) { C.Step = Phase::None; C.StepT = 0.0; }
				C.Plan = PlanNone;
			}
			return false;
		}
		Craft* V = C.Executes >= 0 ? Find(C.Executes) : nullptr;
		if (C.Executes >= 0)
		{
			bool bOn = V && Live(*V) && (V->Doing == Role::Fight || V->Doing == Role::Slot || V->Doing == Role::Home) && DueOn(V->Group, G.Id) > 0
				&& AirT - C.ExecSince < 30.0 && Len(V->P - C.P) < 5500.0;
			// (shot out in front of him, close: he gives it up for a few seconds, for whoever is better placed)
			if (bOn && Len(V->P - C.P) < 900.0 && Between(Norm(C.P - V->P), Ahead(V->Hdg, V->Gam)) < 60.0 * Deg && AirT - C.ExecSince > 3.0)
			{
				bOn = false;
				C.ExecNext = AirT + 4.0;
			}
			if (!bOn)
			{
				C.Executes = -1;
				if (C.Step == Phase::Chase) { C.Step = Phase::None; C.StepT = 0.0; }
				C.Plan = PlanNone;
				C.Think = 0.0;
				return false;
			}
		}
		else
		{
			if (Due.empty() || AirT < C.ExecNext) return false;
			// (not out of a break or an evasion, nor off a bomber he is about to fire on; a zoom of his own he may leave)
			if (C.Step == Phase::Break || (C.Step == Phase::Zoom && C.Plan != PlanZoom) || Dodging(C) || C.Burst > 0.0) return false;
			if (C.Step == Phase::Run)
			{
				const Craft* Mine = C.Target >= 0 ? Find(C.Target) : nullptr;
				if (Mine && !Mine->bFighter && Len(Mine->P - C.P) < 900.0) return false;
			}
			{
				const auto Box = Now.find(C.TargetGroup);
				if (Box != Now.end() && !Box->second.bFighter && DueAllOn(C.TargetGroup, G.Id) > 0) return false;
			}
			// (as many of his group after each enemy group as it has losses to show, one to each of its aircraft)
			std::map<int, int> Taken;
			std::set<int> Had;
			for (int Key_ : MembersOf(G.Id))
			{
				const Craft& O = Crafts.find(Key_)->second;
				if (O.Group != G.Id || O.Executes < 0 || O.Key == C.Key || !Live(O) || O.Doing != Role::Fight || O.Step == Phase::Away) continue;
				const Craft* Q = Find(O.Executes);
				if (!Q) continue;
				Taken[Q->Group]++;
				Had.insert(Q->Key);
			}
			// (how long the battle's loss waits for a gun to bear, as the scene holds it: Play, Advance)
			auto Left = [this, &G](int Victim)
			{
				double Most = 0.0;
				for (const Held& H : Later)
				{
					if (!H.bForShot || H.E.What != "air:lost" || H.E.Unit != Victim || H.E.By != G.Id || BoxOf(H.E) >= 0) continue;
					Most = std::max(Most, H.At - AirT + (H.bMore ? 0.0 : 7.0));
				}
				return Most;
			};
			Craft* Pick = nullptr;
			double Best = 1e18;
			std::map<int, double> LeftOf;
			for (auto& Pair : Crafts)
			{
				Craft& F = Pair.second;
				// (one in the fight, one still flying in his place by the bombers, or one hit and going home: an easy kill)
				if (F.Side == C.Side || !F.bFighter || !Live(F) || !(F.Doing == Role::Fight || F.Doing == Role::Slot || F.Doing == Role::Home) || Had.count(F.Key)) continue;
				if (F.StragId >= 0) continue;
				const int Owed = DueOn(F.Group, G.Id);
				if (Owed <= 0) continue;
				// (one more than it has losses to show, so that the better placed may come in while the first is still far;
				// and past that one already close and before his nose, for whom it is a gift)
				const auto Tk = Taken.find(F.Group);
				const bool bFull = Tk != Taken.end() && Tk->second >= Owed + 1;
				const Vec Rel = F.P - C.P;
				const double D = Len(Rel);
				if (D > 5000.0) continue;
				auto Lf = LeftOf.find(F.Group);
				if (Lf == LeftOf.end()) Lf = LeftOf.emplace(F.Group, Left(F.Group)).first;
				// (the one his guns can soonest bear on: the turn on to it and the closing, as it is flying; a leader
				// before his wingman, whose work is his leader's tail; not one turning hard)
				// (he closes flat out on one flying steady, as the one he goes for will: DogDuel)
				const Vec Way = Rel * (1.0 / std::max(1.0, D));
				const double Steady = Clamp(0.8 * F.VMax, 95.0, F.VMax);
				const double Closing = std::max(20.0, C.VTop - Steady * Ahead(F.Hdg, F.Gam).Dot(Way));
				const double Turn = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Way), -1.0, 1.0));
				double Cost = std::max(0.0, D - 300.0) / Closing + Turn / 0.42;
				// (one above him he must climb to first, as his type climbs: a long way up is too long)
				const double Climb = F.P.Z - C.P.Z;
				if (Climb > 0.0) Cost += Climb / std::max(6.0, 0.7 * (C.ClimbRate > 0.0 ? C.ClimbRate : 15.0));
				if (C.Lead != 0 && C.Lead != C.Key) Cost += 3.0;
				if (Dodging(F)) Cost += 6.0;
				// (one already going in for a kill of his own is left to it, while there is another to have)
				if (F.Executes >= 0) Cost += 10.0;
				if (F.Step == Phase::Away) Cost += 2.0;
				if (Cost > Lf->second + 4.0) continue;
				if (bFull && Cost > 5.0) continue;
				if (Cost < Best) { Best = Cost; Pick = &F; }
			}
			if (!Pick) return false;
			V = Pick;
			C.Executes = V->Key;
			C.ExecSince = AirT;
			C.bCover = C.bReversed = C.bAnswer = false;
			C.Step = Phase::Chase;
			C.StepT = 0.0;
			C.Since = AirT;
			C.Plan = PlanNone;
			Chasers[V->Key].push_back(C.Key);
		}
		// Round into his six and a little above, flat out while he is far or off to his side; then the lead, a little
		// faster than he is, to his own range (no farther out than 240 m: the battle's loss waits for him); his sight on
		// and a burst.
		const Vec Tp = V->P, Tv = Velocity(*V);
		const Vec Rel = Tp - C.P;
		const double D = Len(Rel);
		const double Range = std::max(320.0, 450.0 - 250.0 * C.Skill);
		const Vec VNose = Ahead(V->Hdg, V->Gam);
		const double OffTail = Between(Norm(C.P - Tp), VNose * -1.0);
		Cmd K;
		if (D > 900.0 || (OffTail > 55.0 * Deg && D < 2000.0))
		{
			const double Back = Clamp(D * 0.6, 350.0, 800.0);
			K = Steer(C, Tp - VNose * Back + Up * 100.0 + Tv * 2.0, Tv, 0.05, 111.0, C.VTop);
			// (flat out from far; nearer, his throttle by the range, never to overshoot him as he swings in)
			K.Spd = D > 1500.0 ? C.VTop : Clamp(Len(Tv) + 30.0 + (D - 400.0) * 0.04, 111.0, C.VTop);
		}
		else if (D > Range * 1.3) K = Toward(C, Tp + Tv * (D / 800.0), Clamp(Len(Tv) + 30.0, 111.0, C.VTop));
		else
		{
			K = Toward(C, Tp + Tv * (D / 800.0), Clamp(Len(Tv) + (D > Range ? 15.0 : 8.0), 111.0, C.VTop));
			Bear(C, K);
		}
		if (D < 130.0) K.Spd = std::max(111.0, Len(Tv) - 10.0);
		K.Gam = Clamp(K.Gam, -1.0, 1.0);
		MindGround(C, K);
		Fly(C, K, Dt, true);
		FightSpeed(C);
		DogShoot(C, *V, G, Dt, Range);
		C.Step = Phase::Chase;
		C.Target = V->Key;
		C.Task = "attacking";
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The duel.

	Show::Craft* Show::DogPick(Craft& C, const Group& G, const Group& E, const Vec& Middle)
	{
		// As PickFoe, and as a pilot picks: one below him before one above (the height is his), one who has not seen him
		// before one who has.
		std::map<int, int> Claimed;
		std::set<int> Menacing;
		for (int Key_ : MembersOf(G.Id))
		{
			const Craft& O = Crafts.find(Key_)->second;
			if (O.Group != G.Id || O.Key == C.Key || !Live(O)) continue;
			if (O.Lead == O.Key && O.Target >= 0) Claimed[O.Target]++;
			if (O.Threat >= 0) Menacing.insert(O.Threat);
		}
		const bool bEscort = G.With >= 0 && Now.count(G.With);
		Craft* Best = nullptr;
		double BestCost = 1e18;
		for (auto& Pair : Crafts)
		{
			Craft& F = Pair.second;
			if (F.Side == C.Side || !F.bFighter || !Live(F) || F.Doing == Role::Ground) continue;
			const bool bOnBox = bEscort && F.Doing == Role::Fight && F.TargetGroup == G.With;
			const bool bOnUs = F.Doing == Role::Fight && F.TargetGroup == G.Id;
			if (F.Group != E.Id && !(bEscort && (bOnBox || bOnUs)) && !Menacing.count(F.Key)) continue;
			double Cost = Len(F.P - C.P);
			auto Cl = Claimed.find(F.Key);
			if (Cl != Claimed.end()) Cost += 700.0 * Cl->second;
			if (Menacing.count(F.Key)) Cost -= 900.0;
			if (F.Step == Phase::Away) Cost += 2500.0;
			const double Off = Len(F.P - Middle);
			if (bOnBox && Off < 2500.0) Cost -= 400.0;
			Cost += 2.0 * std::max(0.0, Off - 3500.0);
			Cost += 0.6 * Clamp(F.P.Z - C.P.Z, -900.0, 900.0);
			if (F.Target != C.Key && F.Threat != C.Key) Cost -= 300.0;
			if (Cost < BestCost) { BestCost = Cost; Best = &F; }
		}
		return Best;
	}

	void Show::DogPlanTo(Craft& C, int Plan)
	{
		if (C.Plan == Plan) return;
		C.Plan = Plan;
		C.PlanT = AirT;
		C.StepT = 0.0;
		C.YoYo = 0.0;
		switch (Plan)
		{
		case PlanSlash:
		case PlanHeadOn: C.Step = Phase::Run; Check.Passes++; break;
		case PlanAngles: C.Step = Phase::Turn; break;
		case PlanZoom: C.Step = Phase::Zoom; break;
		default: C.Step = Phase::Setup; break;
		}
	}

	void Show::DogThink(Craft& C, const Craft& T, const Group& G)
	{
		// He looks at the fight again, less often the greener he is: what he means to do now.
		C.Think = AirT + 0.5 + (1.0 - C.Skill) * 1.0 + DogRoll.U(0.0, 0.4);
		// (a pass, a zoom, a run away for speed, a head-on: each flown to its own end, DogEnergy)
		if (C.Plan == PlanSlash || C.Plan == PlanZoom || C.Plan == PlanExtend || C.Plan == PlanHeadOn) return;
		const Vec Rel = T.P - C.P;
		const double D = Len(Rel);
		const Vec Nose = Ahead(C.Hdg, C.Gam), TNose = Ahead(T.Hdg, T.Gam);
		const double Over = C.P.Z - T.P.Z;
		const double Spare = EnergyOf(C) - EnergyOf(T);
		int Style = StyleOf(C, G);
		// (a type that does either: up and down against one that turns the tighter, round with one that does not)
		if (Style == 3) Style = T.RMin < C.RMin * 0.95 ? 2 : 1;
		// (a green pilot turns and slows down, whatever he flies: one in two of them, by his number)
		if (Style == 2 && C.Skill < 0.3 && std::fmod(C.Key * 0.4142135623730951, 1.0) < 0.5) Style = 1;
		const bool bHeadOn = D < 2600.0 && D > 500.0 && Nose.Dot(Rel) > 0.85 * D && TNose.Dot(Rel * -1.0) > 0.85 * D;
		int Plan = C.Plan;
		if (bHeadOn) Plan = PlanHeadOn;
		else if (Style == 2)
		{
			// (down on him with the height; away for speed with him coming on with more; and never circling under one who
			// out-climbs him for a perch he cannot have: away for speed and up again, a perch held 15 s at most)
			const bool bOutClimbed = C.ClimbRate > 0.0 && T.ClimbRate > 1.1 * C.ClimbRate && Over < 0.0;
			if (Over > 250.0 && D < 2800.0 && D > 300.0) Plan = PlanSlash;
			else if (Spare < -400.0 && TNose.Dot(Rel * -1.0) > 0.7 * D && D < 1500.0) Plan = PlanExtend;
			else if ((C.Plan == PlanPerch && C.StepT > 15.0) || (bOutClimbed && C.Plan != PlanPerch)) Plan = PlanExtend;
			else Plan = PlanPerch;
		}
		else
		{
			if (Over > 400.0 && D < 2000.0 && C.Plan != PlanAngles) Plan = PlanSlash;
			else Plan = PlanAngles;
		}
		DogPlanTo(C, Plan);
	}

	bool Show::DogDuel(Craft& C, const Group& G, const Group& E, double Dt)
	{
		// The middle of the fight: between the two groups; for an escort, its bombers.
		Vec Middle = (Vec(E.X, E.Y, E.Alt) + Vec(G.X, G.Y, G.Alt)) * 0.5;
		int Anchor = G.Id;
		auto BIt = G.With >= 0 ? Now.find(G.With) : Now.end();
		if (BIt != Now.end())
		{
			Middle = Vec(BIt->second.X, BIt->second.Y, BIt->second.Alt);
			Anchor = G.With;
		}
		Craft* T = C.Target >= 0 ? Find(C.Target) : nullptr;
		if (T && (!Live(*T) || T->Side == C.Side || T->Doing == Role::Ground || !T->bFighter)) T = nullptr;
		// He keeps his man: a new one only when his is gone, or after half a minute or so, never in the middle of a pass.
		const bool bBusy = C.Burst > 0.0 || C.Plan == PlanSlash || C.Plan == PlanHeadOn || (T && Len(T->P - C.P) < 700.0);
		if (!T || (AirT > C.Repick && !bBusy))
		{
			Craft* Was = T;
			T = DogPick(C, G, E, Middle);
			C.Repick = AirT + DogRoll.U(25.0, 40.0);
			if (T != Was)
			{
				C.Plan = PlanNone;
				C.Think = 0.0;
				C.FoeSince = AirT;
				C.Step = Phase::None;
				C.StepT = 0.0;
			}
			C.Target = T ? T->Key : -1;
		}
		if (!T) return false;
		// One the battle has counted lost, an enemy coming in behind him to show it: his eyes on his own man, steady.
		if (DogMarked(C))
		{
			const Vec Rel = T->P - C.P;
			const double D = Len(Rel);
			Cmd K = Toward(C, T->P + Velocity(*T) * (D / 800.0), Clamp(0.8 * C.VMax, 95.0, C.VMax));
			Ease(C, K, 32.0 * Deg);
			K.Gam = Clamp(K.Gam, -0.2, 0.2);
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			DogShoot(C, *T, G, Dt);
			if (C.Step != Phase::Turn) { C.Step = Phase::Turn; C.StepT = 0.0; }
			C.Plan = PlanNone;
			C.Task = "attacking";
			return true;
		}
		// Drawn too far from the fight: back toward it, and another enemy.
		if (Len(C.P - Middle) > 5500.0 && C.Burst <= 0.0 && C.Plan != PlanSlash)
		{
			Cmd K = Steer(C, Middle, GroupVelocity(Anchor), 0.05, 111.0, C.VMax);
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "positioning";
			C.Repick = 0.0;
			C.Plan = PlanNone;
			return true;
		}
		if (AirT >= C.Think || C.Plan == PlanNone) DogThink(C, *T, G);
		if (C.Plan == PlanAngles)
		{
			if (C.Step != Phase::Turn) { C.Step = Phase::Turn; C.StepT = 0.0; }
			DogAngles(C, *T, G, Dt);
		}
		else DogEnergy(C, *T, G, Dt);
		return true;
	}

	void Show::DogAngles(Craft& C, Craft& T, const Group& G, double Dt)
	{
		// The turning fight, in three dimensions: cutting the corner while he is far, the lead as the range closes, his
		// own turn and climb followed as his speed allows; over the top of the turn when closing too fast to hold it
		// (the high yo-yo), down inside it when falling behind (the low).
		const Vec Tp = T.P, Tv = Velocity(T);
		const Vec Rel = Tp - C.P;
		const double Dist = Len(Rel);
		const double Off = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Norm(Rel)), -1.0, 1.0));
		const double Closing = (Velocity(C) - Tv).Dot(Norm(Rel));
		const double Open = std::max(DueOn(T.Group, C.Group) > 0 ? 320.0 : 0.0, 450.0 - 250.0 * C.Skill);
		const double Slow = 111.0;
		const Vec Aim = Tp + Tv * (Dist / 800.0);
		Cmd K;
		if (C.YoYo <= 0.0 && AirT > C.YoYoNext)
		{
			if (Dist < 450.0 && Closing > 25.0 && Off > 25.0 * Deg && C.Spd > C.VMin + 25.0)
			{
				C.YoYo = 2.4;
				C.YoYoWay = 1;
				C.YoYoNext = AirT + 7.0;
				Check.YoYos++;
			}
			else if (Dist > 550.0 && Dist < 1500.0 && Closing < 5.0 && Off < 45.0 * Deg && C.P.Z - GroundAt(C.P.X, C.P.Y) > 800.0)
			{
				C.YoYo = 2.4;
				C.YoYoWay = -1;
				C.YoYoNext = AirT + 8.0;
				Check.YoYos++;
			}
		}
		if (C.YoYo > 0.0)
		{
			C.YoYo -= Dt;
			K = Toward(C, Aim, C.YoYoWay > 0 ? C.VMax : C.VTop);
			if (C.YoYoWay > 0) K.Gam = std::min(std::min(std::max(K.Gam, C.Gam) + 0.35, 0.9), std::max(ClimbMost(C), 0.0));
			else K.Gam = std::max(std::min(K.Gam, C.Gam) - 0.35, -0.9);
		}
		else
		{
			if (Dist > 1000.0) K = Toward(C, Tp + Tv * (Dist / (C.VTop + 60.0)), C.VTop);
			else if (Dist > Open * 1.3) K = Toward(C, Aim, C.VTop);
			else K = Toward(C, Aim, Clamp(Len(Tv) + 12.0, Slow, C.VTop));
			if (Dist < 140.0) K.Spd = std::max(Slow, Len(Tv) - 8.0);
			K.Gam = Clamp(K.Gam, -1.2, 1.2);
			if (Dist < 1.6 * Open) Bear(C, K);
		}
		MindGround(C, K);
		Fly(C, K, Dt, true);
		FightSpeed(C);
		DogShoot(C, T, G, Dt);
		C.Task = "attacking";
	}

	void Show::DogEnergy(Craft& C, Craft& T, const Group& G, double Dt)
	{
		// The fight up and down: the bounce from above and out of the sun, one pass, the zoom back up on the speed of the
		// dive, and the perch over him again; away for speed when he has the better of it; and a head-on pass met.
		const Vec Tp = T.P, Tv = Velocity(T);
		const Vec Rel = Tp - C.P;
		const double Dist = Len(Rel);
		const Vec Nose = Ahead(C.Hdg, C.Gam);
		const double Open = std::max(DueOn(T.Group, C.Group) > 0 ? 320.0 : 0.0, 450.0 - 250.0 * C.Skill);
		Cmd K;
		switch (C.Plan)
		{
		case PlanSlash:
		{
			if (Dist > 1.6 * Open) K = Toward(C, Tp + Tv * (Dist / (C.Spd + 80.0)), C.VTop);
			else
			{
				K = Toward(C, Tp + Tv * (Dist / 800.0), C.VTop);
				Bear(C, K);
			}
			K.Gam = Clamp(K.Gam, -1.2, 1.0);
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			DogShoot(C, T, G, Dt);
			C.Task = "attacking";
			// Through: past him, too close, below him, he has turned inside it, or it has gone on too long. Up and away
			// on the speed of it, the way away from him.
			const bool bPast = C.StepT > 2.0 && Nose.Dot(Rel) < 0.0 && Dist < 700.0;
			const bool bInside = C.StepT > 2.5 && Dist < 400.0 && Nose.Dot(Rel) < 0.7 * Dist && C.Burst <= 0.0;
			if (bPast || bInside || Dist < 140.0 - 80.0 * C.Skill || C.StepT > 20.0 || C.P.Z < Tp.Z - 400.0)
			{
				C.Hand = Rel.Dot(RightOf(C.Hdg)) >= 0.0 ? -1.0 : 1.0;
				C.PlanZ = std::max(C.PlanZ, Tp.Z + 500.0);
				C.Burst = 0.0;
				DogPlanTo(C, PlanZoom);
			}
			break;
		}
		case PlanZoom:
		{
			K.Hdg = C.Hdg + C.Hand * 0.2;
			K.Gam = C.Spd > C.VMin + 45.0 ? 40.0 * Deg : C.Spd > C.VMin + 25.0 ? 15.0 * Deg : -5.0 * Deg;
			K.Spd = C.VTop;
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			C.Task = "breaking";
			if ((C.StepT > 3.0 && C.P.Z >= C.PlanZ) || C.Spd < C.VMin + 25.0 || C.StepT > 12.0)
			{
				C.PlanZ = C.P.Z;
				DogPlanTo(C, PlanPerch);
			}
			break;
		}
		case PlanExtend:
		{
			// Away from him in a shallow dive, flat out, for the speed to come back up with.
			K.Hdg = std::atan2(-Rel.X, -Rel.Y) + C.Hand * 0.3;
			K.Gam = -6.0 * Deg;
			K.Spd = C.VTop;
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			C.Task = "breaking";
			if (C.StepT > 10.0 || Dist > 2200.0)
			{
				C.PlanZ = Tp.Z + 600.0;
				DogPlanTo(C, PlanZoom);
			}
			break;
		}
		case PlanHeadOn:
		{
			// Nose to nose: a short burst from 650 m in to 250, then one over and one under him, and on through.
			const double Side = C.Key < T.Key ? 1.0 : -1.0;
			K = Toward(C, Tp + Tv * (Dist / 1600.0), C.VMax);
			if (Dist < 350.0) K.Gam += Side * 0.18;
			MindGround(C, K);
			Fly(C, K, Dt, true);
			FightSpeed(C);
			if (Dist > 250.0) Squeeze(C, T, G, 650.0, 6.0 * Deg, 0.5, 1.0, Dt);
			C.Task = "attacking";
			if ((C.StepT > 2.0 && Nose.Dot(Rel) < 0.0) || C.StepT > 15.0)
			{
				C.Hand = Rel.Dot(RightOf(C.Hdg)) >= 0.0 ? -1.0 : 1.0;
				const int Style = StyleOf(C, G);
				C.PlanZ = std::max(C.P.Z, Tp.Z) + 400.0;
				DogPlanTo(C, Style == 1 ? PlanAngles : PlanZoom);
			}
			break;
		}
		default:
		{
			// The perch: over him, behind him and up-sun of him, where he must look into the sun to see.
			if (C.Plan != PlanPerch) DogPlanTo(C, PlanPerch);
			const Vec SunFlat = Norm(Vec(SunWay.X, SunWay.Y, 0.0), Vec());
			Vec S = Tp - Ahead(T.Hdg) * 900.0 + Up * 650.0 + SunFlat * 600.0;
			S.Z = std::max(S.Z, GroundAt(S.X, S.Y) + 400.0);
			K = Steer(C, S, Tv, 0.06, 111.0, C.VMax);
			if (S.Z - C.P.Z > 60.0)
			{
				K.Spd = std::max(C.VMax, C.Spd);
				Ease(C, K, (C.Spd > 138.0 ? 45.0 : 60.0) * Deg);
			}
			MindGround(C, K);
			Fly(C, K, Dt, true);
			C.Task = "positioning";
			const double Over = C.P.Z - Tp.Z;
			const bool bUnseen = T.Target != C.Key && (T.Threat != C.Key || T.bBlind);
			if (Over > 250.0 && Dist < 2400.0 && (Len(S - C.P) < 700.0 || (bUnseen && Nose.Dot(Rel) > 0.3 * Dist) || C.StepT > 20.0)) DogPlanTo(C, PlanSlash);
			else if (Over > 450.0 && Dist < 1800.0 && Nose.Dot(Rel) > 0.4 * Dist) DogPlanTo(C, PlanSlash);
			break;
		}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Out of it.

	void Show::DogAway(Craft& C, const Group& G, double Dt)
	{
		// Hit, or his pair's guns empty: away toward home in a straight line and down, as such a pilot went home, his
		// wingman beside him; and, where the battle still counts him, holding far out (six km and more), a wide, slow
		// circle, never round and round within sight of the fight.
		C.Executes = -1;
		C.Plan = PlanNone;
		Vec Way = C.Side == 0 ? Vec(G.X - TargetX, G.Y - TargetY, 0.0) : Vec(TargetX - G.X, TargetY - G.Y, 0.0);
		Way = Norm(Way, Ahead(G.Face + Pi));
		const Vec Stand = Vec(G.X, G.Y, std::max(G.Alt - 900.0, GroundAt(G.X, G.Y) + 500.0)) + Way * 6500.0;
		const Vec Rel = Stand - C.P;
		Cmd K;
		if (Flat(Rel) > 1500.0)
		{
			K = Toward(C, Stand, 128.0);
			K.Gam = Clamp(K.Gam, -0.12, 0.05);
		}
		else
		{
			const double Wv = AirT * 2.0 * Pi / 90.0 + C.Weave;
			K = Steer(C, Stand + Vec(std::sin(Wv), std::cos(Wv), 0.0) * 1400.0, GroupVelocity(G.Id), 0.04, 100.0, 125.0);
		}
		Ease(C, K, 30.0 * Deg);
		MindGround(C, K);
		Fly(C, K, Dt, true);
	}
