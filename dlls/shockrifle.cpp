//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"
#include "customentity.h"
#include "animation.h"
#include "gamerules.h"
#include "shake.h"


enum shockrifle_e {
	SHOCK_IDLE1 = 0,
	SHOCK_FIRE,
	SHOCK_DRAW,
	SHOCK_HOLSTER,
	SHOCK_IDLE3
};


LINK_ENTITY_TO_CLASS( weapon_shockrifle, CShockRifle );

BOOL CShockRifle::IsUseable( void )
{
	return TRUE;
}

void CShockRifle::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SHOCKRIFLE;
	SET_MODEL(ENT(pev), "models/w_shock_rifle.mdl");
	pev->sequence		= 0;
	pev->animtime		= gpGlobals->time + 0.1;
	pev->framerate		= 1.0;

	m_iDefaultAmmo = SHOCK_DEFAULT_GIVE;
	m_iFirePhase = 0;
	
	FallInit();// get ready to fall down.
}


void CShockRifle::Precache( void )
{
	PRECACHE_MODEL("models/v_shock.mdl");
	PRECACHE_MODEL("models/w_shock_rifle.mdl");
	PRECACHE_MODEL("models/p_shock.mdl");

	PRECACHE_SOUND("weapons/shock_draw.wav");
	PRECACHE_SOUND("weapons/shock_fire.wav");
	PRECACHE_SOUND("weapons/shock_impact.wav");
	PRECACHE_SOUND("weapons/shock_recharge.wav");
	PRECACHE_SOUND("weapons/shock_discharge.wav");
	m_iBeam = PRECACHE_MODEL( "sprites/lgtning.spr" );

	m_usShockFire = PRECACHE_EVENT ( 1, "events/shockrifle.sc" );

	UTIL_PrecacheOther("shock_beam");
}

int CShockRifle::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "shock";
	p->iMaxAmmo1 = SHOCK_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 6;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_SHOCKRIFLE;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = SHOCKRIFLE_WEIGHT;

	return 1;
}

int CShockRifle::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{

#ifndef CLIENT_DLL
		if ( g_pGameRules->IsMultiplayer() )
		{
			// in multiplayer, all shockroaches come full. 
			pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = SHOCK_MAX_CARRY;
		}
#endif

		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}


BOOL CShockRifle::Deploy( )
{
	return DefaultDeploy( "models/v_shock.mdl", "models/p_shock.mdl", SHOCK_DRAW, "mp5" );
}

void CShockRifle::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( SHOCK_HOLSTER );

	if ( !m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] )
	{
		m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = 1;
	}
}


void CShockRifle::PrimaryAttack()
{
	Reload( );

	if 	(m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
		return;

	// Kill if fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		TraceResult	tr;
#ifndef CLIENT_DLL
		m_pPlayer->TakeDamage( VARS(eoNullEntity), VARS(eoNullEntity), 255, DMG_SHOCK );
#endif
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		return;
	}

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	flags = 0;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usShockFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, m_iBeam, 0, 0);	

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	anglesAim.x		= -anglesAim.x;

	Vector vecSrc	 = m_pPlayer->GetGunPosition( )+ gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

#ifndef CLIENT_DLL

	CBaseEntity *pShock = CBaseEntity::Create( "shock_beam", vecSrc, anglesAim, m_pPlayer->edict() );
	pShock->pev->velocity = gpGlobals->v_forward * 2000;
#endif

	m_flRechargeTime = gpGlobals->time + 0.5;
	
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;


	m_flNextPrimaryAttack = m_flNextPrimaryAttack + 0.25;

	if (m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
	{
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25;
	}

#ifndef CLIENT_DLL
	if ( g_pGameRules->IsMultiplayer() )
	{
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
	}
#endif

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
}


void CShockRifle::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= SHOCK_MAX_CARRY)
		return;

	while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < SHOCK_MAX_CARRY && m_flRechargeTime < gpGlobals->time)
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/shock_recharge.wav", 1.0, ATTN_NORM, 0, 100);
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
		#ifndef CLIENT_DLL
		if ( g_pGameRules->IsMultiplayer() )
		{
			m_flRechargeTime += 0.25;
		}
		else
		{
			m_flRechargeTime += 0.5;
		}
		#endif
	}

}


void CShockRifle::WeaponIdle( void )
{
	Reload( );

	if ( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 101.0 / 30.0;
		SendWeaponAnim( SHOCK_IDLE1, 1 );
	}
}