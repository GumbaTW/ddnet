/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_FAST_INPUT_H
#define GAME_CLIENT_FAST_INPUT_H

#include <base/vmath.h>
#include <game/gamecore.h>

#include <cmath>

namespace FastInput
{
inline int ExtraTicks(float Offset)
{
	return Offset > 0.0f ? (int)std::ceil(Offset) : 0;
}

inline void ApplyOffset(float Offset, int &Tick, float &Intra)
{
	if(Offset <= 0.0f)
		return;
	const int WholeTicks = (int)Offset;
	const float CombinedIntra = Intra + (Offset - WholeTicks);
	const int Carry = (int)CombinedIntra;
	Tick += WholeTicks + Carry;
	Intra = CombinedIntra - Carry;
}

inline bool IsLocalClient(int ClientId, int LocalId, int DummyId)
{
	return ClientId >= 0 && (ClientId == LocalId || ClientId == DummyId);
}

inline bool IsValidSample(int Tick, int MinimumTick, int MaximumTick, const int (&aPredTicks)[200])
{
	return Tick > 0 && Tick - 1 >= MinimumTick && Tick <= MaximumTick &&
	       aPredTicks[(Tick - 1) % 200] == Tick - 1 && aPredTicks[Tick % 200] == Tick;
}

inline CNetObj_PlayerInput PrepareInput(CNetObj_PlayerInput Input, const CNetObj_PlayerInput &Previous, const CNetObj_PlayerInput &LastSent, vec2 MousePos, vec2 ActionPos, bool Active, bool SubTickAiming)
{
	// The inactive tee already has its own stored target; the active mouse
	// position and its press-time aim lock must not replace that target.
	if(!Active)
		return Input;

	const bool UnsentHook = Input.m_Hook && !LastSent.m_Hook;
	const bool UnsentFire = CountInput(LastSent.m_Fire, Input.m_Fire).m_Presses > 0;
	if(SubTickAiming && (UnsentHook || UnsentFire))
	{
		if(ActionPos != vec2(0, 0))
			MousePos = ActionPos;
		else if(Input.m_Hook == Previous.m_Hook && Input.m_Fire == Previous.m_Fire)
			MousePos = vec2(Previous.m_TargetX, Previous.m_TargetY);
	}
	Input.m_TargetX = (int)MousePos.x;
	Input.m_TargetY = (int)MousePos.y;
	if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		Input.m_TargetX = 1;
	return Input;
}

inline bool HasChanged(const CNetObj_PlayerInput &Previous, const CNetObj_PlayerInput &Input, const CNetObj_PlayerInput &LastSent)
{
	const bool AimChanged = Previous.m_TargetX != Input.m_TargetX || Previous.m_TargetY != Input.m_TargetY;
	const bool AimAffectsPhysics = Input.m_Hook || (Input.m_Fire & 1) || CountInput(LastSent.m_Fire, Input.m_Fire).m_Presses > 0;
	return Previous.m_Direction != Input.m_Direction || Previous.m_Hook != Input.m_Hook ||
	       Previous.m_Fire != Input.m_Fire || Previous.m_Jump != Input.m_Jump ||
	       Previous.m_NextWeapon != Input.m_NextWeapon || Previous.m_PrevWeapon != Input.m_PrevWeapon ||
	       Previous.m_WantedWeapon != Input.m_WantedWeapon || Previous.m_PlayerFlags != Input.m_PlayerFlags ||
	       (AimChanged && AimAffectsPhysics);
}
} // namespace FastInput

#endif
