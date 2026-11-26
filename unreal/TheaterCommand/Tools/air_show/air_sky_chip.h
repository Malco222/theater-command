#pragma once

// The clouds' chip arithmetic (Source/TheaterCommand/Battle/AirSkyChip.inl) in C++, for the cloud test: the same text
// M_CloudTurned runs on the graphics chip, given the few HLSL types and functions it uses. Nothing else is added here,
// so that what the test checks is what the chip runs.

#include <algorithm>
#include <cmath>

namespace AirSkyChipCpp
{
	struct float3
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float3() = default;
		float3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
	};

	struct float4
	{
		float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
		float4() = default;
		float4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
	};

	inline float3 operator+(const float3& A, const float3& B) { return float3(A.x + B.x, A.y + B.y, A.z + B.z); }
	inline float3 operator-(const float3& A, const float3& B) { return float3(A.x - B.x, A.y - B.y, A.z - B.z); }
	inline float3 operator*(const float3& A, float S) { return float3(A.x * S, A.y * S, A.z * S); }
	inline float3 operator*(float S, const float3& A) { return float3(A.x * S, A.y * S, A.z * S); }
	inline float3 operator/(const float3& A, float S) { return float3(A.x / S, A.y / S, A.z / S); }
	inline float3 operator-(const float3& A) { return float3(-A.x, -A.y, -A.z); }
	inline float dot(const float3& A, const float3& B) { return A.x * B.x + A.y * B.y + A.z * B.z; }
	inline float3 cross(const float3& A, const float3& B) { return float3(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x); }
	inline float sqrt(float V) { return std::sqrt(V); }
	inline float length(const float3& A) { return std::sqrt(dot(A, A)); }
	inline float3 normalize(const float3& A) { return A / length(A); }
	inline float max(float A, float B) { return A > B ? A : B; }
	inline float min(float A, float B) { return A < B ? A : B; }
	inline float clamp(float V, float A, float B) { return min(max(V, A), B); }
	inline float cos(float V) { return std::cos(V); }
	inline float sin(float V) { return std::sin(V); }
	inline float atan2(float Y, float X) { return std::atan2(Y, X); }
	inline float floor(float V) { return std::floor(V); }

#include "Battle/AirSkyChip.inl"
}
