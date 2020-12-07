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

enum beretta_e {
	BERETTA_IDLE1 = 0,
	BERETTA_IDLE2,
	BERETTA_IDLE3,
	BERETTA_SHOOT,
	BERETTA_SHOOT_EMPTY,
	BERETTA_RELOAD,
	BERETTA_RELOAD_NOT_EMPTY,
	BERETTA_DRAW,
	BERETTA_HOLSTER,
	BERETTA_ADD_SILENCER
};

LINK_ENTITY_TO_CLASS( weapon_beretta, CBeretta );


void CBeretta::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_beretta"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_BERETTA;
	SET_MODEL(ENT(pev), "models/w_beretta.mdl");

	m_iDefaultAmmo = BERETTA_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CBeretta::Precache( void )
{
	PRECACHE_MODEL("models/v_beretta.mdl");
	PRECACHE_MODEL("models/w_beretta.mdl");
	PRECACHE_MODEL("models/p_beretta.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell_92f.mdl");// brass shell

	PRECACHE_SOUND("weapons/ber_magin.wav");
	PRECACHE_SOUND("weapons/ber_magout.wav");

	PRECACHE_SOUND("weapons/ber_slide.wav");
	PRECACHE_SOUND("weapons/ber_slideforward.wav");

	PRECACHE_SOUND ("weapons/beretta_fire1.wav");

	m_usFireBeretta = PRECACHE_EVENT( 1, "events/beretta.sc" );
}

int CBeretta::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "beretta";
	p->iMaxAmmo1 = BERETTA_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = BERETTA_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_BERETTA;
	p->iWeight = BERETTA_WEIGHT;

	return 1;
}

BOOL CBeretta::Deploy( )
{
	return DefaultDeploy( "models/v_beretta.mdl", "models/p_beretta.mdl", BERETTA_DRAW, "onehanded", /*UseDecrement() ? 1 : 0*/ 0 );
}

void CBeretta::PrimaryAttack( void )
{
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
		}

		return;
	}

	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	float flSpread = 0.01;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming;
	vecAiming = gpGlobals->v_forward;

	Vector vecDir;

	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpread, flSpread, flSpread ), 8192, BULLET_PLAYER_BERETTA, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	m_flNextPrimaryAttack = 0.30;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireBeretta, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	// HEV suit - indicate out of ammo condition
	m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 31.0f / 30.0f;
}

void CBeretta::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == BERETTA_MAX_CLIP)
		return;

	if (m_iClip == 0)
	{
		DefaultReload( 17, BERETTA_RELOAD, 1.5, 0, TRUE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 76.0f / 35.0f;
	}
	else
	{
		DefaultReload( 17, BERETTA_RELOAD_NOT_EMPTY, 1.5, 0, TRUE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 66.0f / 35.0f;
	}

}



void CBeretta::WeaponIdle( void )
{
	if (m_flTimeWeaponIdle <  UTIL_WeaponTimeBase() )
	{
		ResetEmptySound( );
		m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

		// only idle if the slide isn't back
		if (m_iClip != 0)
		{
			int iAnim;
			float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

			if (flRand <= 0.3 + 0 * 0.75)
			{
				iAnim = BERETTA_IDLE3;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
			}
			else if (flRand <= 0.6 + 0 * 0.875)
			{
				iAnim = BERETTA_IDLE1;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
			}
			else
			{
				iAnim = BERETTA_IDLE2;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
			}
			SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
		}
	}
}

class CBerettaAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_berclip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_berclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_BER92F_GIVE, "beretta", BERETTA_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_beretta, CBerettaAmmo );


