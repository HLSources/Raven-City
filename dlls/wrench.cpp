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
#include "gamerules.h"


#define	WRENCH_BODYHIT_VOLUME 128
#define	WRENCH_WALLHIT_VOLUME 2048

LINK_ENTITY_TO_CLASS( weapon_pipewrench, CWrench );

float CWrench::GetFullChargeTime( void )
{
	return 10;
}

enum wrench_e {
	WRENCH_IDLE1 = 0,
	WRENCH_IDLE2,
	WRENCH_IDLE3,
	WRENCH_DRAW,
	WRENCH_HOLSTER,
	WRENCH_ATTACK1HIT,
	WRENCH_ATTACK1MISS,
	WRENCH_ATTACK2HIT,
	WRENCH_ATTACK2MISS,
	WRENCH_ATTACK3HIT,
	WRENCH_ATTACK3MISS,
	WRENCH_BIGWIND,
	WRENCH_BIGHIT,
	WRENCH_BIGMISS,
	WRENCH_BIGLOOP,
};


void CWrench::Spawn( )
{
	Precache( );
	m_iId = WEAPON_PIPEWRENCH;
	SET_MODEL(ENT(pev), "models/w_pipe_wrench.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}


void CWrench::Precache( void )
{
	PRECACHE_MODEL("models/v_pipe_wrench.mdl");
	PRECACHE_MODEL("models/w_pipe_wrench.mdl");
	PRECACHE_MODEL("models/p_pipe_wrench.mdl");
	PRECACHE_SOUND("weapons/pwrench_miss1.wav");
	PRECACHE_SOUND("weapons/pwrench_miss2.wav");
	PRECACHE_SOUND("weapons/pwrench_hitbod1.wav");
	PRECACHE_SOUND("weapons/pwrench_hitbod2.wav");
	PRECACHE_SOUND("weapons/pwrench_hitbod3.wav");
	PRECACHE_SOUND("weapons/pwrench_big_miss.wav");
	PRECACHE_SOUND("weapons/pwrench_big_hitbod1.wav");
	PRECACHE_SOUND("weapons/pwrench_big_hitbod2.wav");
	PRECACHE_SOUND("weapons/pwrench_hit1.wav");
	PRECACHE_SOUND("weapons/pwrench_hit2.wav");

	m_usWrench= PRECACHE_EVENT ( 1, "events/wrench.sc" );
	m_usWrenchLarge= PRECACHE_EVENT ( 1, "events/wrench2.sc" );
}

int CWrench::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iId = WEAPON_PIPEWRENCH;
	p->iWeight = PIPEWRENCH_WEIGHT;
	return 1;
}



BOOL CWrench::Deploy( )
{
	return DefaultDeploy( "models/v_pipe_wrench.mdl", "models/p_pipe_wrench.mdl", WRENCH_DRAW, "crowbar" );
}

void CWrench::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	SendWeaponAnim( WRENCH_HOLSTER );
}


void FindHullIntersection_Wrench( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


void CWrench::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}
void CWrench::SecondaryAttack()
{
	if ( m_fInAttack == 0 )
	{
		m_fPrimaryFire = FALSE;

		SendWeaponAnim( WRENCH_BIGWIND );
		m_fInAttack = 1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
		m_pPlayer->m_flStartCharge = gpGlobals->time;
		m_pPlayer->m_flAmmoStartCharge = UTIL_WeaponTimeBase() + GetFullChargeTime();
	}
	else if (m_fInAttack == 1)
	{
		if (m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
		{
			SendWeaponAnim( WRENCH_BIGLOOP );
			m_fInAttack = 2;
		}
	}
	else
	{
		if ( UTIL_WeaponTimeBase() >= m_pPlayer->m_flAmmoStartCharge )
		{
			// don't eat any more ammo after gun is fully charged.
			m_pPlayer->m_flNextAmmoBurn = 85;
		}
	}
}
void CWrench::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CWrench::SwingAgain( void )
{
	Swing( 0 );
}
//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CWrench::StartLargeSwing( void )
{
	float flDamage;

	if ( gpGlobals->time - m_pPlayer->m_flStartCharge > GetFullChargeTime() )
	{
			flDamage = 200;
	}
	else
	{
			flDamage = 200 * (( gpGlobals->time - m_pPlayer->m_flStartCharge) / GetFullChargeTime() );
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.75;
	LargeSwing( flDamage );
}
int CWrench::LargeSwing( float flDamage )
{
	int fDidHit = FALSE;
	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection_Wrench( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usWrenchLarge, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0,0, 0 );
	if ( tr.flFraction >= 1.0 )
	{
			// miss
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}
	else
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if ( FClassnameIs( pEntity->pev, "monster_maria" ) 
			|| FClassnameIs( pEntity->pev, "monster_boris" ) )
		{
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
			return 0;
		}

		SendWeaponAnim( WRENCH_BIGHIT );
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		fDidHit = TRUE;
		ClearMultiDamage( );

		// first swing does full damage
		pEntity->TraceAttack(m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB ); 

		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,1) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_big_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_big_hitbod2.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = WRENCH_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return TRUE;
				else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		if (fHitWorld)
		{
			//float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			//if ( g_pGameRules->IsMultiplayer() )
			//{
			//	// override the volume here, cause we don't play texture sounds in multiplayer, 
			//	// and fvolbar is going to be 0 from the above call.
			float fvolbar;
			fvolbar = 1;
			//}

			// also play wrench strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * WRENCH_WALLHIT_VOLUME;
#endif
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;

		SetThink( Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 1.0;

		
	}
	return fDidHit;
}
int CWrench::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection_Wrench( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usWrench, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0.0, 0, 0.0 );


	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.75;
			
			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if ( FClassnameIs( pEntity->pev, "monster_maria" ) 
			|| FClassnameIs( pEntity->pev, "monster_boris" ) )
		{
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
			return 0;
		}

		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( WRENCH_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( WRENCH_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( WRENCH_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		fDidHit = TRUE;

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 2 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgWrench, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgWrench / 2, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,1) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_hitbod2.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = WRENCH_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
				{
					  m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
					  return TRUE;
				}
				else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			//float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			//if ( g_pGameRules->IsMultiplayer() )
			//{
			//	// override the volume here, cause we don't play texture sounds in multiplayer, 
			//	// and fvolbar is going to be 0 from the above call.


			float fvolbar;
			fvolbar = 1;
			//}

			// also play knife strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/pwrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * WRENCH_WALLHIT_VOLUME;
#endif
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		
		SetThink( Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;

		
	}
	return fDidHit;
}
void CWrench::WeaponIdle( void )
{

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;
		if (m_fInAttack != 0)
		{
			StartLargeSwing();
			m_fInAttack = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
		}
		else
		{
			int iAnim;
			float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

			if (flRand <= 0.3 + 0 * 0.75)
			{
				iAnim = WRENCH_IDLE3;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 91.0 / 30.0;
			}
			else if (flRand <= 0.6 + 0 * 0.875)
			{
				iAnim = WRENCH_IDLE1;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 61.0 / 30.0;
			}
			else
			{
				iAnim = WRENCH_IDLE2;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 91.0 / 30.0;
			}

			SendWeaponAnim( iAnim, 1 );
		}
}