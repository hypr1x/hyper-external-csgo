#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <iostream>
#include <array>
#include "vector.h"
#include "memory.h"

namespace offset
{
	// client
	constexpr ::std::ptrdiff_t dwLocalPlayer = 0xDEB99C;
	constexpr ::std::ptrdiff_t dwEntityList = 0x4E0102C;

	// engine
	constexpr ::std::ptrdiff_t dwClientState = 0x59F19C;
	constexpr ::std::ptrdiff_t dwClientState_ViewAngles = 0x4D90;
	constexpr ::std::ptrdiff_t dwClientState_GetLocalPlayer = 0x180;
	constexpr ::std::ptrdiff_t dwGlowObjectManager = 0x535BAD0;
	constexpr ::std::ptrdiff_t dwForceJump = 0x52BCD88;
	constexpr ::std::ptrdiff_t dwForceAttack = 0x322EE98;

	// entity
	constexpr ::std::ptrdiff_t m_dwBoneMatrix = 0x26A8;
	constexpr ::std::ptrdiff_t m_fFlags = 0x104;
	constexpr ::std::ptrdiff_t m_iGlowIndex = 0x10488;
	constexpr ::std::ptrdiff_t m_iShotsFired = 0x103E0;
	constexpr ::std::ptrdiff_t m_bDormant = 0xED;
	constexpr ::std::ptrdiff_t m_iTeamNum = 0xF4;
	constexpr ::std::ptrdiff_t m_lifeState = 0x25F;
	constexpr ::std::ptrdiff_t m_vecOrigin = 0x138;
	constexpr ::std::ptrdiff_t m_vecViewOffset = 0x108;
	constexpr ::std::ptrdiff_t m_aimPunchAngle = 0x303C; 
	constexpr ::std::ptrdiff_t m_iDefaultFOV = 0x333C;
	constexpr ::std::ptrdiff_t m_bSpottedByMask = 0x980;
	constexpr ::std::ptrdiff_t m_hActiveWeapon = 0x2F08;
	constexpr ::std::ptrdiff_t m_iHealth = 0x100;
	constexpr ::std::ptrdiff_t m_iItemDefinitionIndex = 0x2FBA;
	constexpr ::std::ptrdiff_t m_nFallbackPaintKit = 0x31D8;
	constexpr ::std::ptrdiff_t m_hMyWeapons = 0x2E08;
	constexpr ::std::ptrdiff_t m_iItemIDHigh = 0x2FD0;
	constexpr ::std::ptrdiff_t m_flFallbackWear = 0x31E0;
}

Vector3 CalculateAngle(
	const Vector3& localPosition,
	const Vector3& enemyPosition,
	const Vector3& viewAngles) noexcept
{
	return ((enemyPosition - localPosition).ToAngle() - viewAngles);
}


constexpr const int GetWeaponPaint(const short& itemDefinition) 
{
	switch (itemDefinition)
	{
	case 1: return 1090;
	case 4: return 38;
	case 7: return 490;
	case 9: return 344;
	case 61: return 653;
	default: return 0;
	}
}

int main() 
{
	const auto memory = Memory{ "csgo.exe" };

	// module addresses
	const auto client = memory.GetModuleAddress("client.dll");
	const auto engine = memory.GetModuleAddress("engine.dll");
	int fov = 90;

	while (true) 
	{
		const auto& localPlayer = memory.Read<std::uintptr_t>(client + offset::dwLocalPlayer);
		const auto& weapons = memory.Read<std::array<unsigned long,8>>(localPlayer + offset::m_hMyWeapons);

		for (const auto& handle : weapons)
		{
			const auto& weapon = memory.Read<std::uintptr_t>((client + offset::dwEntityList + (handle & 0xFFF) * 0x10) - 0x10);
			if (!weapon)
				continue;
			if (const auto paint = GetWeaponPaint(memory.Read<short>(weapon + offset::m_iItemDefinitionIndex)))
			{
				const bool shouldUpdate = memory.Read<std::int32_t>(weapon + offset::m_nFallbackPaintKit) != paint;

				memory.Write<std::int32_t>(weapon + offset::m_iItemIDHigh, -1);
				memory.Write<std::int32_t>(weapon + offset::m_nFallbackPaintKit, paint);
				memory.Write<float>(weapon + offset::m_flFallbackWear, 0.1f);

				if (shouldUpdate)
					memory.Write<std::int32_t>(memory.Read<std::uintptr_t>(engine + offset::dwClientState) + 0x174, -1);
			}
		}

		const auto localTeam = memory.Read<std::int32_t>(localPlayer + offset::m_iTeamNum);
		const auto flags = memory.Read<std::uintptr_t>(localPlayer + offset::m_fFlags);
		const auto localEyePosition = memory.Read<Vector3>(localPlayer + offset::m_vecOrigin) + memory.Read<Vector3>(localPlayer + offset::m_vecViewOffset);
		const auto clientState = memory.Read<std::uintptr_t>(engine + offset::dwClientState);
		const auto localPlayerId = memory.Read<std::int32_t>(clientState + offset::dwClientState_GetLocalPlayer);
		const auto viewAngles = memory.Read<Vector3>(clientState + offset::dwClientState_ViewAngles);
		const auto aimPunch = memory.Read<Vector3>(localPlayer + offset::m_aimPunchAngle) * 2;
		auto bestFov = 180.f;
		const auto gom = memory.Read<std::uintptr_t>(client + offset::dwGlowObjectManager);
		if (GetAsyncKeyState(VK_SPACE)) {
			(flags & (1 << 0)) ?
				memory.Write<uintptr_t>(client + offset::dwForceJump, 6) :
				memory.Write<uintptr_t>(client + offset::dwForceJump, 4);
		}
		if (GetAsyncKeyState(0x76) & 1) {
			fov = 90;
			memory.Write<int>(localPlayer + offset::m_iDefaultFOV, fov);
		}
		if (GetAsyncKeyState(0x77) & 1) {
			fov = fov - 1;
			memory.Write<int>(localPlayer + offset::m_iDefaultFOV, fov);
		}
		if (GetAsyncKeyState(0x78) & 1) {
			fov = fov + 1;
			memory.Write<int>(localPlayer + offset::m_iDefaultFOV, fov);
		}
		for (auto i = 1; i <= 32; ++i)
		{
			const auto player = memory.Read<std::uintptr_t>(client + offset::dwEntityList + i * 0x10);

			if (memory.Read<std::int32_t>(player + offset::m_lifeState))
				continue;

			const auto glowIndex = memory.Read<std::uintptr_t>(player + offset::m_iGlowIndex);

			memory.Write<float>(gom + (glowIndex * 0x38) + 0x8, 1.f);
			memory.Write<float>(gom + (glowIndex * 0x38) + 0xC, 1.f);
			memory.Write<float>(gom + (glowIndex * 0x38) + 0x10, 1.f);
			memory.Write<float>(gom + (glowIndex * 0x38) + 0x14, 1.f);
			memory.Write<bool>(gom + (glowIndex * 0x38) + 0x27, true);
			memory.Write<bool>(gom + (glowIndex * 0x38) + 0x28, true);

			if (memory.Read<std::int32_t>(player + offset::m_bSpottedByMask) & (1 << localPlayerId))
			{
				const auto boneMatrix = memory.Read<std::uintptr_t>(player + offset::m_dwBoneMatrix);
				const auto playerHeadPosition = Vector3{
					memory.Read<float>(boneMatrix + 0x30 * 8 + 0x0C),
					memory.Read<float>(boneMatrix + 0x30 * 8 + 0x1C),
					memory.Read<float>(boneMatrix + 0x30 * 8 + 0x2C)
				};

				const auto angle = CalculateAngle(
					localEyePosition,
					playerHeadPosition,
					viewAngles + aimPunch
				);
				memory.Write<Vector3>(clientState + offset::dwClientState_ViewAngles, viewAngles + angle);
			}
		}
	}
	return 0;
}