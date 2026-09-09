#ifndef GAME_CLIENT_COMPONENTS_BOTSYMONA_BOTSYMONA_H
#define GAME_CLIENT_COMPONENTS_BOTSYMONA_BOTSYMONA_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/http.h>
#include <engine/shared/http.h>

#include <game/client/component.h>
#include <game/gamecore.h>

#include <memory>
#include <vector>

struct CNetObj_PlayerInput;

class CBotSymona : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnMessage(int MsgType, void *pRawMsg) override;

	bool OnDummyInput(CNetObj_PlayerInput *pInput);

	void JoinServer();
	void LeaveServer();
	bool BotOnServer() const;
	bool CanJoin(const char **ppReason) const;

	int BotClientId() const;
	int FindOwner() const;

	const char *StatusText() const { return m_aStatus; }
	const char *ConnectionText() const;
	const char *AiStatusText() const { return m_aAiStatus; }
	int CountVisiblePlayers() const;

	enum ERole
	{
		ROLE_NONE = 0,
		ROLE_ADMIN,
		ROLE_OWNER,
	};

	enum EList
	{
		LIST_ADMIN = 0,
		LIST_TEAM,
		LIST_WAR,
		LIST_TROLL,
		NUM_LISTS,
	};

	void LoadRoles();
	void SaveRoles();
	const char *RolesPath() const { return m_aRolesPath; }
	int ListCount(int List) const;
	const char *ListEntry(int List, int Index) const;

private:
	enum EState
	{
		S_FOLLOW = 0,
		S_GOTO,
		S_HOOK,
		S_PULL,
		S_HAMMER,
		S_UNREACH,
		S_CLIMB,
		S_LAUNCH,
		S_SELF,
		S_ROAM,
	};

	enum ESubject
	{
		SUBJ_NONE = 0,
		SUBJ_RESCUE,
		SUBJ_FOLLOW,
	};

	struct SLaunch
	{
		vec2 m_Grab;
		vec2 m_Land;
		int m_Hold;
		int m_Dir;
	};

	struct CBotName
	{
		char m_aName[MAX_NAME_LENGTH];
	};

	struct SLimits
	{
		float m_HookLength;
		float m_HookReach;
		float m_HookMinPull;
		float m_HammerMax;
		int m_HookTravelTicks;
	};

	struct SRejected
	{
		vec2 m_Pos;
		int m_Tick;
	};

	struct SAction
	{
		int m_PreDir = 0;
		int m_PreTicks = 0;
		int m_Dir = 0;
		int m_JumpAt = -1;
		int m_AirJumpAt = -1;
		int m_HookFrom = -1;
		int m_HookUntil = -1;
		vec2 m_Aim = vec2(1.0f, 0.0f);
	};

	struct SPlan
	{
		int m_PreDir;
		int m_PreUntil;
		int m_Dir;
		int m_JumpAt;
		int m_AirJumpAt;
	};

	struct SSimResult
	{
		bool m_Frozen;
		int m_FreezeTick;
		vec2 m_EndPos;
		float m_MinGoalDist;
		float m_EndGoalDist;
		int m_MinGoalTick;
		int m_BrushTicks;
		bool m_Airborne;
	};

	struct SGap
	{
		int m_Dir;
		int m_FreezeRows;
		float m_NearEdgeX;
		float m_FarEdgeX;
		float m_Width;
		vec2 m_LandPos;
	};

	int BotConn() const;
	int PredTickNow() const;
	const void *BotSnapItem(int Type, int Id) const;
	SLimits Limits() const;
	int PredictHorizon() const;
	float FollowDist() const;

	const struct CNetObj_Character *SnapChar(int ClientId) const;
	bool SweepFreeze(vec2 From, vec2 To) const;
	bool CoreFromSnap(int ClientId, CCharacterCore *pOut) const;
	bool PredCore(int ClientId, CCharacterCore *pOut) const;
	SSimResult Simulate(const CCharacterCore &Start, const SAction &Action, vec2 Goal, int Ticks) const;
	float ScoreAction(const SSimResult &Res, const SAction &Action, float StartDist) const;
	bool SearchDirect(const CCharacterCore &Start, vec2 Goal, float StartDist, SAction *pOut, float *pScore) const;
	bool ScanGap(const CCharacterCore &Start, vec2 Goal, SGap *pOut) const;
	bool LaneClear(float FromX, float ToX, float WalkY) const;
	bool PlanCrossing(const CCharacterCore &Start, vec2 Goal, SAction *pOut, int *pLength) const;
	void Steer(const CCharacterCore &Start, vec2 Goal, int PredTick, CNetObj_PlayerInput *pInput);
	void DropPlan();

	enum EHookOutcome
	{
		HK_PENDING = 0,
		HK_FLYING,
		HK_OWNER,
		HK_TERRAIN,
		HK_MISS,
	};

	bool SafeStand(vec2 Pos) const;
	bool SafeGoalNear(vec2 OwnerPos, vec2 BotPos, vec2 *pOut) const;
	bool HammerCanHit(vec2 From, vec2 Target) const;
	bool PullSafe(vec2 From, vec2 OwnerPos) const;
	bool HookAimPoint(vec2 From, vec2 OwnerPos, int BotId, int OwnerId, vec2 *pAim) const;
	int FirstTeeOnRay(vec2 From, vec2 To, int SelfId) const;
	int BlockerAhead(vec2 From, vec2 To, int SelfId) const;
	bool HookPathToOwner(vec2 From, vec2 OwnerPos, int BotId, int OwnerId, vec2 *pAim = nullptr) const;
	int HookOutcome(int PredTick, int HookState, int HookedPlayer, int OwnerId) const;
	bool FindAnchor(vec2 OwnerPos, vec2 BotPos, float Reach, int PredTick, vec2 *pOut) const;
	bool AnchorReachable(vec2 BotPos, vec2 P) const;
	bool CorridorClear(vec2 From, vec2 To) const;
	bool IsRejected(vec2 P, int PredTick) const;
	void Reject(vec2 P, int PredTick);
	static float WalkTicks(float Dist);
	float AnchorEta(vec2 P, vec2 OwnerPos, vec2 BotPos, const SLimits &L) const;
	bool HookableAt(vec2 Pos) const;
	bool HookProbe(vec2 From, vec2 Dir, vec2 *pGrab) const;
	bool GapAt(float ColX, float WalkY) const;
	bool PlanClimb(const CCharacterCore &Start, vec2 OwnerPos, int PredTick, vec2 *pGrab, vec2 *pLand, int *pHold, bool *pAirJump) const;
	bool PlanLaunch(const CCharacterCore &Start, vec2 Goal, int PredTick, SLaunch *pOut) const;
	bool FreezeAhead(vec2 Pos, vec2 Vel, vec2 Pull, int Ticks) const;
	bool TeeState(int ClientId, vec2 *pPos, vec2 *pVel) const;
	bool IsFrozen(int ClientId, bool *pDeep) const;
	int FreezeTicksLeft(int ClientId) const;
	bool FreezeAtIndex(int Index) const;
	bool FreezeAt(vec2 Pos) const;
	bool SwitchActive(int Index) const;

	int RoleOf(const char *pName) const;
	int RoleOfId(int ClientId) const;
	int FindSubject(int *pMode) const;
	bool PickRoamGoal(vec2 BotPos, int PredTick, vec2 *pOut) const;

	bool BotCore(CCharacterCore *pOut) const;
	bool HasWeapon(int Weapon) const;
	int WeaponFireDelay(int Weapon) const;
	bool WeaponReady(int Weapon, int PredTick) const;
	bool ShotLineClear(vec2 From, vec2 To, int SelfId, int TargetId, float TeeRadius) const;
	bool BeamAim(vec2 BotPos, vec2 TargetPos, int SelfId, int TargetId, int Weapon, vec2 *pAim) const;
	bool LaserAim(vec2 BotPos, vec2 TargetPos, int SelfId, int TargetId, vec2 *pAim) const;
	bool DragFrees(vec2 TargetPos, int TargetId, vec2 PullOrigin) const;
	bool ShotgunDragAim(vec2 BotPos, vec2 TargetPos, int SelfId, int TargetId, vec2 *pAim) const;
	bool AimAndFire(CNetObj_PlayerInput *pInput, vec2 BotPos, vec2 TargetPos, int Weapon, int PredTick);

	bool InList(int List, const char *pName) const;
	bool ListAdd(int List, const char *pName);
	bool ListRemove(int List, const char *pName);

	// PVP / war
	bool TryPvP(int PredTick, int BotId, vec2 BotPos, vec2 BotVel, CNetObj_PlayerInput *pInput);
	int FindWarEnemy(vec2 *pPos, vec2 *pVel) const;
	bool WarHandle(int PredTick, int BotId, vec2 BotPos, vec2 BotVel, int EnemyId, vec2 EnemyPos, vec2 EnemyVel, CNetObj_PlayerInput *pInput);
	bool WarCounterHook(vec2 BotPos, int BotId, int EnemyId, vec2 EnemyPos, CNetObj_PlayerInput *pInput);
	bool WarKillFrozen(int PredTick, int BotId, int EnemyId, vec2 BotPos, vec2 EnemyPos, CNetObj_PlayerInput *pInput);
	int PickWarWeapon(float Dist) const;
	int WarLead(int Weapon, float Dist) const;
	bool WarShot(CNetObj_PlayerInput *pInput, int Weapon, vec2 BotPos, int PredTick, vec2 TargetPos, int BotId, int EnemyId);

	// chat commands
	bool HandleChatCommand(int SenderId, int Team, const char *pText);
	bool InfoCommand(int Team);
	bool TeamCommand(int Team, const char *pArg, bool Add);
	bool FollowCommand(int Team, const char *pArg, bool Do);
	bool WarCommand(int Team, const char *pArg);
	bool UnWarCommand(int Team, const char *pArg);
	void BotSay(int Team, const char *pFormat, ...);
	int FindPlayerByName(const char *pName) const;

	// AI chat
	bool TryAiMention(int SenderId, int Team, const char *pText);
	void PollAi();
	void SendAiRequest(bool Warmup = false);
	void SendWarmupRequest();
	void SetAiStatus(const char *pFormat, ...);
	static const char *AiJsonEscape(char *pOut, int OutSize, const char *pIn);

	void WriteDefaultRoles();
	void SetStatus(const char *pFormat, ...);
	void RestoreDummyName();
	bool ReleaseInput(CNetObj_PlayerInput *pInput);
	bool StopDriving();

	static void TrimName(char *pDst, int Size, const char *pSrc);

	static void ConBotJoin(IConsole::IResult *pResult, void *pUserData);
	static void ConBotLeave(IConsole::IResult *pResult, void *pUserData);

	char m_aStatus[128] = "выключен";
	char m_aAiStatus[128] = "ИИ выключен";
	std::shared_ptr<CHttpRequest> m_pAiRequest;
	char m_aAiReplyName[MAX_NAME_LENGTH] = "";
	int m_AiReplyTeam = 0;
	char m_aAiQuestion[256] = "";
	int64_t m_NextAiReply = 0;
	int64_t m_NextAiWarmup = 0;
	bool m_AiWarmed = false;
	bool m_AiWarming = false;
	bool m_AiQueued = false;
	char m_aRolesPath[64] = "botroles.txt";
	struct SGrab
	{
		float m_Rank;
		vec2 m_Pos;
	};

	struct SCandidate
	{
		float m_Score;
		vec2 m_Pos;
	};

	std::vector<CBotName> m_avLists[NUM_LISTS];
	mutable std::vector<SCandidate> m_vAnchorCandidates;
	mutable std::vector<SGrab> m_vLaunchGrabs;

	int m_PvpEnemyId = -1;
	int m_PvpSwitchTick = -1;
	vec2 m_PvpStrafe = vec2(0.0f, 1.0f);
	int m_PvpStrafeTick = -1;
	int m_NoKillShotUntil = -1;
	char m_aPvpTarget[MAX_NAME_LENGTH] = "";
	int m_PvpTargetTick = -1;

	int m_State = S_FOLLOW;
	int m_CommitUntil = -1;
	int m_PlanTick = -1;
	vec2 m_Anchor = vec2(0.0f, 0.0f);
	bool m_AnchorValid = false;
	SRejected m_aRejected[8] = {};
	int m_RejectedNext = 0;
	int m_NoHookUntil = -1;
	int m_NextHookPlanTick = -1;
	int m_EscapeUntil = -1;
	vec2 m_EscapeGoal = vec2(0.0f, 0.0f);
	vec2 m_ClimbGrab = vec2(0.0f, 0.0f);
	vec2 m_ClimbLand = vec2(0.0f, 0.0f);
	int m_ClimbStart = -1;
	float m_ClimbBestY = 0.0f;
	int m_ClimbGainTick = -1;
	int m_ClimbHold = 0;
	bool m_ClimbAirJump = false;
	SLaunch m_Launch = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), 0, 0};
	int m_LaunchStart = -1;
	float m_LaunchStartDist = 0.0f;
	int m_HookFiredTick = -1;
	int m_HookRetries = 0;
	int m_GrabbedTick = -1;
	int m_ReleaseUntil = -1;
	float m_LastPullDist = 0.0f;
	int m_LastProgressTick = -1;
	vec2 m_LastPos = vec2(0.0f, 0.0f);
	int m_StuckTicks = 0;

	SPlan m_Plan = {0, -1, 0, -1, -1};
	int m_PlanUntil = -1;
	int m_PlanCommitUntil = -1;
	vec2 m_PlanGoal = vec2(0.0f, 0.0f);
	bool m_PlanSafe = false;
	bool m_PlanCross = false;
	bool m_UsingPrediction = false;
	bool m_FollowMoving = false;
	int m_ProgressTick = -1;
	float m_BestGoalDist = 0.0f;
	float m_JumpBias = 0.0f;

	int m_LastHammerTick = -1;
	int m_LastShotTick = -1;
	bool m_LastHammerHit = false;
	int m_NextBeamTick = -1;
	int m_BeamUntil = -1;
	int m_BeamWeapon = -1;
	vec2 m_BeamAim = vec2(1.0f, 0.0f);
	vec2 m_BeamFrom = vec2(0.0f, 0.0f);
	vec2 m_BeamTarget = vec2(0.0f, 0.0f);
	int m_NoHammerUntil = -1;
	int m_HammerEnterTick = -1;
	int m_LastDebugTick = -1;
	bool m_JumpHeld = false;
	int m_JumpPressTick = -1;
	vec2 m_AimPoint = vec2(1.0f, 0.0f);
	int m_FollowUntilTick = -1;
	int m_FollowId = -1;
	int m_SubjectMode = SUBJ_NONE;
	vec2 m_HomePos = vec2(0.0f, 0.0f);
	bool m_HomeSet = false;
	vec2 m_RoamGoal = vec2(0.0f, 0.0f);
	bool m_RoamValid = false;
	int m_RoamUntil = -1;
	mutable int m_OwnerId = -1;
	int m_LastOwnerId = -2;
	int m_OwnerStateWas = -1;
	bool m_Paused = false;
	bool m_Driving = false;
	bool m_ReturnCamera = false;
	bool m_DummyNameSaved = false;
	char m_aSavedDummyName[64] = "";
	int64_t m_NextTeamJoin = 0;
	int64_t m_NextJoinTry = 0;
	int64_t m_NextDebugLog = 0;
};

#endif
