#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>

bool CBotSymona::GapAt(float ColX, float WalkY) const
{
	const float Half = CCharacterCore::PhysicalSize() / 2.0f;

	if(FreezeAt(vec2(ColX, WalkY)))
		return true;

	return !Collision()->CheckPoint(ColX - Half, WalkY + Half + 5.0f) &&
	       !Collision()->CheckPoint(ColX + Half, WalkY + Half + 5.0f);
}

bool CBotSymona::ScanGap(const CCharacterCore &Start, vec2 Goal, SGap *pOut) const
{
	static constexpr int MAX_COLS = 24;
	static const int s_aRowOff[17] = {0, -1, 1, -2, 2, -3, 3, -4, 4, -5, 5, 6, 8, 10, 12, 14, 16};

	const int Width = Collision()->GetWidth();
	const int Dir = Goal.x < Start.m_Pos.x ? -1 : 1;
	const int Col = std::clamp(round_to_int(Start.m_Pos.x) / 32, 0, Width - 1);
	const float WalkY = Start.m_Pos.y;

	int First = -1;
	for(int k = 1; k < MAX_COLS; k++)
	{
		const int c = Col + Dir * k;
		if(c < 0 || c >= Width)
			return false;
		const vec2 P = vec2(32.0f * c + 16.0f, WalkY);
		if(GapAt(P.x, WalkY))
		{
			First = c;
			break;
		}
		if(Collision()->TestBox(P, CCharacterCore::PhysicalSizeVec2()))
			return false;
	}
	if(First < 0)
		return false;

	int Last = First;
	for(int k = 1; k < MAX_COLS; k++)
	{
		const int c = First + Dir * k;
		if(c < 0 || c >= Width)
			return false;
		if(!GapAt(32.0f * c + 16.0f, WalkY))
			break;
		Last = c;
	}

	bool HaveLand = false;
	vec2 Land = vec2(0.0f, 0.0f);
	for(int k = 1; k <= MAX_COLS && !HaveLand; k++)
	{
		const int c = Last + Dir * k;
		if(c < 0 || c >= Width)
			break;
		for(int r = 0; r < 17; r++)
		{
			const vec2 P = vec2(32.0f * c + 16.0f, WalkY + 32.0f * s_aRowOff[r]);
			if(SafeStand(P))
			{
				Land = P;
				HaveLand = true;
				break;
			}
		}
	}
	if(!HaveLand)
		return false;

	const int Lo = std::min(First, Last);
	const int Hi = std::max(First, Last);
	int Rows = 1;
	for(int i = 1; i <= 6; i++)
	{
		bool Any = false;
		for(int c = Lo; c <= Hi && !Any; c++)
			Any = FreezeAt(vec2(32.0f * c + 16.0f, WalkY - 32.0f * i));
		if(!Any)
			break;
		Rows++;
	}

	pOut->m_Dir = Dir;
	pOut->m_FreezeRows = Rows;
	pOut->m_NearEdgeX = Dir > 0 ? 32.0f * First - 1.0f : 32.0f * (First + 1) + 1.0f;
	pOut->m_FarEdgeX = Dir > 0 ? 32.0f * (Last + 1) + 1.0f : 32.0f * Last - 1.0f;
	pOut->m_Width = absolute(pOut->m_FarEdgeX - pOut->m_NearEdgeX);
	pOut->m_LandPos = Land;
	return true;
}

bool CBotSymona::PlanCrossing(const CCharacterCore &Start, vec2 Goal, SAction *pOut, int *pLength) const
{
	SGap Gap;
	if(!ScanGap(Start, Goal, &Gap))
		return false;

	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	const float G = std::max(0.05f, (float)Tuning.m_Gravity);
	const float GroundJump = Tuning.m_GroundJumpImpulse;
	const float GroundSpeed = std::max(1.0f, (float)Tuning.m_GroundControlSpeed);
	const float GroundAccel = std::max(0.1f, (float)Tuning.m_GroundControlAccel);
	const float AirSpeed = Tuning.m_AirControlSpeed;

	const float Apex = GroundJump * GroundJump / (2.0f * G);
	const float Drop = Gap.m_LandPos.y - Start.m_Pos.y;
	if(-Drop > Apex - 16.0f)
		return false;

	const float Half = GroundJump + G * 0.5f;
	const float Disc = Half * Half + 2.0f * G * Drop;
	if(Disc < 0.0f)
		return false;
	const int AirTicks = std::clamp((int)std::ceil((Half + std::sqrt(Disc)) / G), 8, 200);

	const float WallTop = 17.0f + 32.0f * (float)(Gap.m_FreezeRows - 1);

	int RiseAbove = 1;
	while(RiseAbove < AirTicks && GroundJump * RiseAbove - G * RiseAbove * (RiseAbove - 1) * 0.5f < WallTop)
		RiseAbove++;

	int FallBelow = AirTicks;
	while(FallBelow > RiseAbove && GroundJump * FallBelow - G * FallBelow * (FallBelow - 1) * 0.5f < WallTop)
		FallBelow--;

	const int Window = std::max(1, FallBelow - RiseAbove);
	const float NeedSpeed = Gap.m_Width / (float)Window;
	const float TakeoffSpeed = std::max(AirSpeed, NeedSpeed);
	if(TakeoffSpeed > GroundSpeed + GroundAccel)
		return false;

	const float GroundNeed = std::max(0.0f, TakeoffSpeed - GroundAccel);
	const int RunTicks = std::clamp((int)std::ceil(GroundNeed / GroundAccel), 0, (int)std::ceil(GroundSpeed / GroundAccel));
	float RunDist = 0.0f;
	for(int j = 1; j <= RunTicks; j++)
		RunDist += std::min(GroundAccel * j, GroundSpeed);

	const float ClearHeight = 17.0f + 32.0f * (float)(Gap.m_FreezeRows - 1);
	int RiseTicks = 1;
	while(RiseTicks < 8 && GroundJump * RiseTicks - G * RiseTicks * (RiseTicks - 1) * 0.5f < ClearHeight)
		RiseTicks++;

	const float LeadDist = (float)RiseTicks * std::max(TakeoffSpeed, GroundSpeed);
	const float TakeoffX = Gap.m_NearEdgeX - Gap.m_Dir * LeadDist;
	const float BackUpX = TakeoffX - Gap.m_Dir * (RunDist + 8.0f);

	if(RunDist > 0.0f && !LaneClear(BackUpX, Gap.m_NearEdgeX, Start.m_Pos.y))
		return false;

	CCharacterCore Probe = Start;
	Probe.SetCoreWorld(nullptr, Collision(), nullptr);

	CNetObj_PlayerInput In;
	mem_zero(&In, sizeof(In));
	In.m_PlayerFlags = PLAYERFLAG_PLAYING;
	In.m_TargetX = 1;

	int BackTicks = 0;
	if(RunDist > 0.0f)
	{
		In.m_Direction = -Gap.m_Dir;
		while(BackTicks < 40 && Gap.m_Dir * (Probe.m_Pos.x - BackUpX) > 0.0f)
		{
			Probe.m_Input = In;
			Probe.Tick(true, false);
			Probe.Move();
			Probe.Quantize();
			BackTicks++;
		}
	}

	int AccelTicks = 0;
	In.m_Direction = Gap.m_Dir;
	while(AccelTicks < 40 && Gap.m_Dir * (Probe.m_Pos.x - TakeoffX) < 0.0f)
	{
		Probe.m_Input = In;
		Probe.Tick(true, false);
		Probe.Move();
		Probe.Quantize();
		AccelTicks++;
	}

	SAction Action;
	Action.m_PreDir = -Gap.m_Dir;
	Action.m_PreTicks = BackTicks;
	Action.m_Dir = Gap.m_Dir;
	Action.m_JumpAt = BackTicks + AccelTicks + 1;
	Action.m_AirJumpAt = -1;

	for(int Attempt = 0; Attempt < 2; Attempt++)
	{
		const int Horizon = Action.m_JumpAt + AirTicks + (Attempt == 0 ? 6 : 56);
		const SSimResult Res = Simulate(Start, Action, Gap.m_LandPos, Horizon);

		const bool Landed = !Res.m_Frozen && !Res.m_Airborne &&
				    Gap.m_Dir * (Res.m_EndPos.x - Gap.m_FarEdgeX) > 0.0f &&
				    absolute(Res.m_EndPos.y - Gap.m_LandPos.y) < 128.0f;
		if(Landed)
		{
			*pOut = Action;
			*pLength = Action.m_JumpAt + AirTicks + 4;
			return true;
		}
		Action.m_AirJumpAt = Action.m_JumpAt + 20;
	}
	return false;
}
