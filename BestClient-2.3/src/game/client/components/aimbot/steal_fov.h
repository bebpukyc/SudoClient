#ifndef GAME_CLIENT_COMPONENTS_STEAL_FOV_H
#define GAME_CLIENT_COMPONENTS_STEAL_FOV_H

#include <engine/shared/config.h>

// steal: the hook is a separate mechanic from the weapons in DDNet. While the
// hook key is held the aimbot uses the "Hook" FOV; otherwise it uses the FOV
// of the weapon that is currently equipped (the pistol is WEAPON_GUN).
static inline int GetStealAimbotFov(int Weapon, bool Hooking)
{
	if(Hooking)
		return g_Config.m_AaFovHook;
	switch(Weapon)
	{
	case WEAPON_HAMMER: return g_Config.m_AaFovHammer;
	case WEAPON_GUN: return g_Config.m_AaFovPistol;
	case WEAPON_GRENADE: return g_Config.m_AaFovGrenade;
	case WEAPON_SHOTGUN: return g_Config.m_AaFovShotgun;
	case WEAPON_LASER: return g_Config.m_AaFovLaser;
	}
	return g_Config.m_AaFov;
}

// steal: whether the aimbot is enabled for the current situation. The hook has
// its own toggle; each weapon has its own toggle as well.
static inline bool GetStealAimbotEnabled(int Weapon, bool Hooking)
{
	if(Hooking)
		return g_Config.m_AaWeaponHook != 0;
	switch(Weapon)
	{
	case WEAPON_HAMMER: return g_Config.m_AaWeaponHammer != 0;
	case WEAPON_GUN: return g_Config.m_AaWeaponPistol != 0;
	case WEAPON_GRENADE: return g_Config.m_AaWeaponGrenade != 0;
	case WEAPON_SHOTGUN: return g_Config.m_AaWeaponShotgun != 0;
	case WEAPON_LASER: return g_Config.m_AaWeaponLaser != 0;
	}
	return true;
}

// steal: whether the aimbot should avoid moving the mouse in this situation.
static inline bool GetStealAimbotSilent(int Weapon, bool Hooking)
{
	if(Hooking)
		return g_Config.m_AaSilentHook != 0;
	switch(Weapon)
	{
	case WEAPON_HAMMER: return g_Config.m_AaSilentHammer != 0;
	case WEAPON_GUN: return g_Config.m_AaSilentPistol != 0;
	case WEAPON_GRENADE: return g_Config.m_AaSilentGrenade != 0;
	case WEAPON_SHOTGUN: return g_Config.m_AaSilentShotgun != 0;
	case WEAPON_LASER: return g_Config.m_AaSilentLaser != 0;
	}
	return g_Config.m_AaSilent != 0;
}

#endif
