#pragma once

#include "Battle/AirLand.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

/**
 * (Release 2, the air battle's hitch: the Mac's timing legs of 26 November, build 172f19ae0.) The ground's pictures
 * under a raid with no stage (Battle/AirLand) take the pool 13.6 to 18.2 s to paint, and one draw-side freeze of about a
 * second fell inside that paint in every leg. What the scene needs to know about a paint's inputs and its kept pictures,
 * in plain C++ so that the native test (Tools/air_show/air_ground_keep_test.cpp) checks exactly what the game does:
 * - SamePlace: whether two asks of the painter read the same place, field by field (the row flyPaintEarly: a paint begun
 *   as the replay is queued is used only when the battle, once open, asks for the very same ground; else it is painted
 *   again, so the pictures are always the bytes today's paint makes).
 * - Key, Pack, Unpack, Remember (the row flyGroundKeep): the three painted pictures kept on disk under the project's
 *   Saved/AirGround, one file a ground, named by a key that hashes everything the painter reads (every field of the
 *   place, the book of town plans as text, each picture's frame, and PainterVersion). A file is taken only when its
 *   header, its key, its pictures' sizes, its length and the check of its texels all match; else the ground is painted.
 *   A file is written beside its place and then moved into it. The folder keeps the Keep grounds last used.
 *
 * The pictures are kept as the painter leaves them, less the alpha it always sets to 255 (Painter::PaintRows; ShadeRows
 * leaves it): Unpack gives it back as 255, so the texels the scene makes its pictures from are the same bytes.
 */
namespace AirGroundKeep
{
	/**
	 * The painter's own version, part of every key. RAISE IT WITH ANY CHANGE TO WHAT Battle/AirLand.cpp PAINTS (its
	 * fields, towns, colours, shading, frames): a kept ground of an older painter then no longer matches, and is painted
	 * again. (1: the painter as at 30899121, release 2.)
	 */
	inline constexpr uint32_t PainterVersion = 1;
	/** The kept file's own format (its header's layout), apart from the painter's version. */
	inline constexpr uint32_t FormatVersion = 1;
	/** How many grounds the folder keeps (the last used). */
	inline constexpr size_t Keep = 6;

	/** Every field of the place the painter reads, compared exactly. */
	inline bool SamePlace(const AirLand::Place& A, const AirLand::Place& B)
	{
		return A.AimX == B.AimX && A.AimY == B.AimY && A.NorthX == B.NorthX && A.NorthY == B.NorthY && A.Terrain == B.Terrain && A.Name == B.Name
			&& A.Month == B.Month && A.Seed == B.Seed && A.SunX == B.SunX && A.SunY == B.SunY && A.SunZ == B.SunZ && A.Land == B.Land && A.Rivers == B.Rivers;
	}

	/** FNV-1a, 64 bits, run on over bytes. */
	struct Hash
	{
		uint64_t H = 1469598103934665603ull;
		void Bytes(const void* Data, size_t Size)
		{
			const unsigned char* At = static_cast<const unsigned char*>(Data);
			for (size_t I = 0; I < Size; ++I)
			{
				H = (H ^ At[I]) * 1099511628211ull;
			}
		}
		void U64(uint64_t V)
		{
			unsigned char Le[8];
			for (int I = 0; I < 8; ++I)
			{
				Le[I] = static_cast<unsigned char>(V >> (8 * I));
			}
			Bytes(Le, 8);
		}
		void Num(double V)
		{
			uint64_t Bits = 0;
			std::memcpy(&Bits, &V, sizeof(Bits));
			U64(Bits);
		}
		void Text(const std::string& S)
		{
			U64(S.size());
			Bytes(S.data(), S.size());
		}
		void Lines(const std::vector<std::vector<AirLand::Pt>>& Rings)
		{
			U64(Rings.size());
			for (const std::vector<AirLand::Pt>& Ring : Rings)
			{
				U64(Ring.size());
				for (const AirLand::Pt& Point : Ring)
				{
					Num(Point.first);
					Num(Point.second);
				}
			}
		}
	};

	/** The key of a ground: everything the painter reads, the frames it paints and the painter's version. */
	inline uint64_t Key(const AirLand::Place& P, const std::string& Plans, const std::vector<AirLand::Frame>& Frames, uint32_t Version = PainterVersion)
	{
		Hash K;
		K.Text("theater-command air ground");
		K.U64(Version);
		K.Num(P.AimX);
		K.Num(P.AimY);
		K.Num(P.NorthX);
		K.Num(P.NorthY);
		K.Text(P.Terrain);
		K.Text(P.Name);
		K.U64(static_cast<uint64_t>(static_cast<int64_t>(P.Month)));
		K.U64(P.Seed);
		K.Num(P.SunX);
		K.Num(P.SunY);
		K.Num(P.SunZ);
		K.Lines(P.Land);
		K.Lines(P.Rivers);
		K.Text(Plans);
		K.U64(Frames.size());
		for (const AirLand::Frame& F : Frames)
		{
			K.Num(F.X0);
			K.Num(F.Y0);
			K.Num(F.Step);
			K.U64(static_cast<uint64_t>(static_cast<int64_t>(F.W)));
			K.U64(static_cast<uint64_t>(static_cast<int64_t>(F.H)));
		}
		return K.H;
	}

	/** A ground's file name in the folder: its key in sixteen hex digits. */
	inline std::string FileName(uint64_t Key)
	{
		char Name[32];
		std::snprintf(Name, sizeof(Name), "%016llx.ground", static_cast<unsigned long long>(Key));
		return Name;
	}

	namespace Detail
	{
		inline void Put32(std::vector<uint8_t>& Out, uint32_t V)
		{
			for (int I = 0; I < 4; ++I)
			{
				Out.push_back(static_cast<uint8_t>(V >> (8 * I)));
			}
		}
		inline void Put64(std::vector<uint8_t>& Out, uint64_t V)
		{
			for (int I = 0; I < 8; ++I)
			{
				Out.push_back(static_cast<uint8_t>(V >> (8 * I)));
			}
		}
		inline uint32_t Get32(const uint8_t* At)
		{
			uint32_t V = 0;
			for (int I = 0; I < 4; ++I)
			{
				V |= static_cast<uint32_t>(At[I]) << (8 * I);
			}
			return V;
		}
		inline uint64_t Get64(const uint8_t* At)
		{
			uint64_t V = 0;
			for (int I = 0; I < 8; ++I)
			{
				V |= static_cast<uint64_t>(At[I]) << (8 * I);
			}
			return V;
		}
		/** "TCAG", the format, the key, the count, then each picture's width and height. */
		inline size_t HeaderSize(size_t Count) { return 4 + 4 + 8 + 4 + Count * 8; }
	}

	/**
	 * The file of a ground: its header, each picture's red, green and blue texels in order (W x H x 3 bytes), and a
	 * check (FNV-1a of those texels). Rgba holds each picture's texels as the painter leaves them (W x H x 4); empty if
	 * any picture is not its frame's size.
	 */
	inline std::vector<uint8_t> Pack(uint64_t KeyOf, const std::vector<AirLand::Frame>& Frames, const std::vector<std::vector<uint8_t>>& Rgba)
	{
		std::vector<uint8_t> Out;
		if (Rgba.size() != Frames.size())
		{
			return Out;
		}
		size_t Texels = 0;
		for (size_t K = 0; K < Frames.size(); ++K)
		{
			const size_t N = static_cast<size_t>(Frames[K].W) * static_cast<size_t>(Frames[K].H);
			if (Frames[K].W <= 0 || Frames[K].H <= 0 || Rgba[K].size() != N * 4)
			{
				return Out;
			}
			Texels += N;
		}
		Out.reserve(Detail::HeaderSize(Frames.size()) + Texels * 3 + 8);
		Out.insert(Out.end(), { 'T', 'C', 'A', 'G' });
		Detail::Put32(Out, FormatVersion);
		Detail::Put64(Out, KeyOf);
		Detail::Put32(Out, static_cast<uint32_t>(Frames.size()));
		for (const AirLand::Frame& F : Frames)
		{
			Detail::Put32(Out, static_cast<uint32_t>(F.W));
			Detail::Put32(Out, static_cast<uint32_t>(F.H));
		}
		const size_t Start = Out.size();
		for (const std::vector<uint8_t>& Picture : Rgba)
		{
			for (size_t I = 0; I < Picture.size(); I += 4)
			{
				Out.push_back(Picture[I]);
				Out.push_back(Picture[I + 1]);
				Out.push_back(Picture[I + 2]);
			}
		}
		Hash Check;
		Check.Bytes(Out.data() + Start, Out.size() - Start);
		Detail::Put64(Out, Check.H);
		return Out;
	}

	/**
	 * A ground's file read back: true, with each picture's texels (W x H x 4, alpha 255), only when its header, its key,
	 * the count and sizes of its pictures (against Frames), its length and its check all match; else false, with why.
	 */
	inline bool Unpack(const uint8_t* Bytes, size_t Size, uint64_t KeyOf, const std::vector<AirLand::Frame>& Frames, std::vector<std::vector<uint8_t>>& Rgba, std::string& Why)
	{
		Rgba.clear();
		const size_t Head = Detail::HeaderSize(Frames.size());
		if (!Bytes || Size < Head + 8)
		{
			Why = "shorter than its header";
			return false;
		}
		if (std::memcmp(Bytes, "TCAG", 4) != 0 || Detail::Get32(Bytes + 4) != FormatVersion)
		{
			Why = "not a kept ground of this format";
			return false;
		}
		if (Detail::Get64(Bytes + 8) != KeyOf)
		{
			Why = "another ground's key";
			return false;
		}
		if (Detail::Get32(Bytes + 16) != Frames.size())
		{
			Why = "another count of pictures";
			return false;
		}
		size_t Texels = 0;
		for (size_t K = 0; K < Frames.size(); ++K)
		{
			if (Frames[K].W <= 0 || Frames[K].H <= 0 || Detail::Get32(Bytes + 20 + K * 8) != static_cast<uint32_t>(Frames[K].W)
				|| Detail::Get32(Bytes + 24 + K * 8) != static_cast<uint32_t>(Frames[K].H))
			{
				Why = "another picture size";
				return false;
			}
			Texels += static_cast<size_t>(Frames[K].W) * static_cast<size_t>(Frames[K].H);
		}
		if (Size != Head + Texels * 3 + 8)
		{
			Why = "the wrong length";
			return false;
		}
		Hash Check;
		Check.Bytes(Bytes + Head, Texels * 3);
		if (Check.H != Detail::Get64(Bytes + Head + Texels * 3))
		{
			Why = "its texels altered";
			return false;
		}
		const uint8_t* At = Bytes + Head;
		Rgba.resize(Frames.size());
		for (size_t K = 0; K < Frames.size(); ++K)
		{
			const size_t N = static_cast<size_t>(Frames[K].W) * static_cast<size_t>(Frames[K].H);
			std::vector<uint8_t>& Picture = Rgba[K];
			Picture.resize(N * 4);
			for (size_t I = 0; I < N; ++I)
			{
				Picture[I * 4] = At[0];
				Picture[I * 4 + 1] = At[1];
				Picture[I * 4 + 2] = At[2];
				Picture[I * 4 + 3] = 255;
				At += 3;
			}
		}
		return true;
	}

	/** Bytes written to a file whole (binary); false if it could not be. */
	inline bool WriteBytes(const std::string& Path, const std::vector<uint8_t>& Bytes)
	{
		std::ofstream File(Path, std::ios::binary | std::ios::trunc);
		if (!File)
		{
			return false;
		}
		File.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
		File.close();
		return static_cast<bool>(File);
	}

	/**
	 * The folder's order (the file names of the grounds kept, the last used first, one a line) with Name used now: Name
	 * first, then the others as they stood, Keep in all. Returns the new order; Drop has the names that fell off it.
	 */
	inline std::string Remember(const std::string& Order, const std::string& Name, std::vector<std::string>& Drop)
	{
		Drop.clear();
		std::vector<std::string> Names{ Name };
		size_t From = 0;
		while (From <= Order.size())
		{
			size_t To = Order.find('\n', From);
			if (To == std::string::npos)
			{
				To = Order.size();
			}
			std::string Line = Order.substr(From, To - From);
			while (!Line.empty() && (Line.back() == '\r' || Line.back() == ' '))
			{
				Line.pop_back();
			}
			if (!Line.empty() && Line != Name)
			{
				bool bSeen = false;
				for (const std::string& Had : Names)
				{
					bSeen = bSeen || Had == Line;
				}
				if (!bSeen)
				{
					Names.push_back(Line);
				}
			}
			From = To + 1;
		}
		std::string Out;
		for (size_t K = 0; K < Names.size(); ++K)
		{
			if (K < Keep)
			{
				Out += Names[K] + "\n";
			}
			else
			{
				Drop.push_back(Names[K]);
			}
		}
		return Out;
	}

	/** Whether a file name is one this folder's order may name (sixteen hex digits and ".ground"): nothing else is ever dropped. */
	inline bool IsGroundName(const std::string& Name)
	{
		if (Name.size() != 16 + 7 || Name.compare(16, 7, ".ground") != 0)
		{
			return false;
		}
		for (size_t I = 0; I < 16; ++I)
		{
			const char C = Name[I];
			if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f')))
			{
				return false;
			}
		}
		return true;
	}
}
