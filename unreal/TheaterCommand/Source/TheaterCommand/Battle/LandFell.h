#pragma once

// Where the ground battle fell over. The general's walk on the Mac fell over twice at the same moment of the setup step
// (22 November, both runs: a read of address 0 about 10 s into the battle), and the crash report could not name the
// game's own functions: on the Mac they are hidden from the report. So the scene says, as it goes, which part of the
// frame it is in (Phase: a literal and a number, kept in a ring of the last 64), and once Arm is called a crash first
// writes those last parts, the faulting address and the stack (each frame as its library and offset, for `atos`) to a
// file and to the error stream; then the game's own crash report runs as before.
//
// Engine-free: built into the game and into Tools/land_live/land_fell_test.cpp, which crashes on purpose to check it.
namespace LandFell
{
	/** The part of the frame the scene is in now. Where must live the whole game (a string literal). */
	void Phase(const char* Where, int Detail = -1);

	/** On from now: a crash writes where it fell to File (made afresh) and to the error stream before the game's own
	 * crash report; and the ring of parts is kept as it goes in RingFile, a file mapped into memory, so it is there
	 * however the game ends (each line numbered: sort them). False where the platform has neither (nothing made). */
	bool Arm(const char* File, const char* RingFile);

	/** Off again: the crash report as it was, the file closed. */
	void Disarm();

	/** Whether Arm is on. */
	bool Armed();
}
