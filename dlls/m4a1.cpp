//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//


#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum m4a1_e
{
	M4A1_LONGIDLE = 0,
	M4A1_IDLE1,
	M4A1_LAUNCH,
	M4A1_RELOAD,
	M4A1_DEPLOY,
	M4A1_EMPTY,
	M4A1_FIRE1,
	M4A1_FIRE2,
	M4A1_FIRE3,
};



LINK_ENTITY_TO_CLASS( weapon_m4a1, CM4a1 );


//=========================================================
//=========================================================
int CM4a1::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

void CM4a1::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_m4a1"); // hack to allow for old names
	Precache( );
	SET_MODEL(ENT(pev), "models/w_m4a1.mdl");
	m_iId = WEAPON_M4A1;

	m_iDefaultAmmo = M4A1_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CM4a1::Precache( void )
{
	PRECACHE_MODEL("models/v_m4a1.mdl");
	PRECACHE_MODEL("models/w_m4a1.mdl");
	PRECACHE_MODEL("models/p_m4a1.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shellTE_MODEL

	PRECACHE_MODEL("models/grenade.mdl");	// grenade

	PRECACHE_MODEL("models/w_m4clip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND("weapons/m4_catch.wav");
	PRECACHE_SOUND("weapons/m4_magin.wav");
	PRECACHE_SOUND("weapons/m4_magout.wav");

	PRECACHE_SOUND ("weapons/m4_fire1.wav");// H to the K
	PRECACHE_SOUND ("weapons/m4_fire2.wav");// H to the K

	PRECACHE_SOUND( "weapons/glauncher.wav" );
	PRECACHE_SOUND( "weapons/glauncher2.wav" );

	PRECACHE_SOUND ("weapons/357_cock1.wav");

	m_usM4a1 = PRECACHE_EVENT( 1, "events/m4a1.sc" );
	m_usM4a12 = PRECACHE_EVENT( 1, "events/m4a12.sc" );
}

int CM4a1::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "m4a1";
	p->iMaxAmmo1 = _M4A1_MAX_CARRY;
	p->pszAmmo2 = "ARgrenades";
	p->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
	p->iMaxClip = M4A1_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_M4A1;
	p->iWeight = M4A1_WEIGHT;

	return 1;
}

int CM4a1::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CM4a1::Deploy( )
{
	return DefaultDeploy( "models/v_m4a1.mdl", "models/p_m4a1.mdl", M4A1_DEPLOY, "mp5" );
}


void CM4a1::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;


	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir;

#ifdef CLIENT_DLL
	if ( !bIsMultiplayer() )
#else
	if ( !g_pGameRules->IsMultiplayer() )
#endif
	{
		// optimized multiplayer. Widened to make it easier to hit a moving player
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_6DEGREES, 8192, BULLET_PLAYER_M4A1, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
	else
	{
		// single player spread
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_3DEGREES, 8192, BULLET_PLAYER_M4A1, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}

  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usM4a1, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.09;

	if ( m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 7.0f / 10.0f;
}



void CM4a1::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] == 0)
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;
			
	m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType]--;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

 	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// we don't add in player velocity anymore.
	CGrenade::ShootContact( m_pPlayer->pev, 
							m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16, 
							gpGlobals->v_forward * 800 );

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usM4a12 );
	
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 34.0f / 30.0f;

	if (!m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
}

void CM4a1::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == M4A1_MAX_CLIP)
		return;

	DefaultReload( M4A1_MAX_CLIP, M4A1_RELOAD, 2.0 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0;
}


void CM4a1::WeaponIdle( void )
{
	if (m_flTimeWeaponIdle <  UTIL_WeaponTimeBase() )
	{
		ResetEmptySound( );
		m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		int iAnim;
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:	
			iAnim = M4A1_LONGIDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 41.0f / 8.0f;
			break;
		
		default:
		case 1:
			iAnim = M4A1_IDLE1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 111.0f / 35.0f;
			break;
		}

		SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
	}
}


class CM4a1AmmoClip : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_m4clip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_m4clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_M4A1CLIP_GIVE, "m4a1", _M4A1_MAX_CARRY) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
LINK_ENTITY_TO_CLASS( ammo_m4a1clip, CM4a1AmmoClip );
LINK_ENTITY_TO_CLASS( ammo_m4a1, CM4a1AmmoClip );



class CM4a1Chainammo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_CHAINBOX_GIVE, "m4a1", _M4A1_MAX_CARRY) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
LINK_ENTITY_TO_CLASS( ammo_m4a1box, CM4a1Chainammo );


class CM4a1AmmoGrenade : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_ARgrenade.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_ARgrenade.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_M203BOX_GIVE, "ARgrenades", M203_GRENADE_MAX_CARRY ) != -1);

		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
LINK_ENTITY_TO_CLASS( ammo_m4a1grenades, CM4a1AmmoGrenade );
LINK_ENTITY_TO_CLASS( ammo_ARgrenades, CM4a1AmmoGrenade );