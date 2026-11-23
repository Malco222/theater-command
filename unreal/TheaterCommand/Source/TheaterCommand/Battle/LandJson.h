#pragma once

#include <string>
#include <vector>

/**
 * A small JSON reader for the ground battle's plain C++ (the recording, the vehicle and soldier models). It is the air
 * scene's (AirFeed::Json) in shape, kept apart so that the ground battle's cores and their cloud tests build without
 * the air scene's flight core; the air's own is left as it is.
 *
 * Numbers are read with a hand-rolled reader, not strtod over a copied string: a battle model is four and a half
 * megabytes, most of it numbers, and the air's way (a std::string a number) took a second and a half on one.
 */
namespace LandJson
{
	struct Json
	{
		enum class Type { Null, Bool, Number, String, Array, Object };
		Type Kind = Type::Null;
		bool Flag = false;
		double Value = 0.0;
		std::string Text;
		std::vector<Json> Items;
		/** An object's fields: names and values side by side, in the order written. */
		std::vector<std::string> Keys;
		std::vector<Json> Values;

		/** The field by name, or a shared null. */
		const Json& operator[](const char* Key) const;
		const Json& operator[](const std::string& Key) const { return (*this)[Key.c_str()]; }
		/** The item by index, or a shared null. */
		const Json& At(size_t Index) const;
		size_t Count() const { return Kind == Type::Array ? Items.size() : Kind == Type::Object ? Values.size() : 0; }
		bool IsNull() const { return Kind == Type::Null; }
		bool IsNumber() const { return Kind == Type::Number; }
		bool IsString() const { return Kind == Type::String; }
		bool IsArray() const { return Kind == Type::Array; }
		bool IsObject() const { return Kind == Type::Object; }
		bool Has(const char* Key) const { return !(*this)[Key].IsNull(); }
		double Num(double Default = 0.0) const { return Kind == Type::Number ? Value : Kind == Type::Bool ? (Flag ? 1.0 : 0.0) : Default; }
		int Int(int Default = 0) const;
		bool Truth(bool Default = false) const { return Kind == Type::Bool ? Flag : Kind == Type::Number ? Value != 0.0 : Default; }
		const std::string& Str() const;
		std::string Str(const char* Default) const { return Kind == Type::String ? Text : std::string(Default); }
	};

	/** Reads JSON text; false (and why, with where) on a fault. */
	bool Parse(const std::string& Source, Json& OutValue, std::string& OutError);
	/** Reads a whole file as text; false if it cannot be opened. */
	bool ReadFile(const std::string& Path, std::string& OutText);
	/** An array of numbers as doubles (anything else in it read as 0). */
	std::vector<double> Numbers(const Json& Array);
}
