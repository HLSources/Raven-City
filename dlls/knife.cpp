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
#ifndef CLIENT_DLL
#include "particle_defs.h"
#endif
#define	KNIFE_BODYHIT_VOLUME 128
#define	KNIFE_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_knife, CKnife );

enum knife_e {
	KNIFE_IDLE1 = 0,
	KNIFE_DRAW,
	KNIFE_HOLSTER,
	KNIFE_SLASHHIT,
	KNIFE_SLASHMISS,
	KNIFE_CHARGE,
	KNIFE_STAB,
};


void CKnife::Spawn( )
{
	Precache( );
	m_iId = WEAPON_KNIFE;
	SET_MODEL(ENT(pev), "models/w_knife.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}


void CKnife::Precache( void )
{
	PRECACHE_MODEL("models/v_knife.mdl");
	PRECACHE_MODEL("models/w_knife.mdl");
	PRECACHE_MODEL("models/p_knife.mdl");

	PRECACHE_SOUND("weapons/knife_hit1.wav");
	PRECACHE_SOUND("weapons/knife_hit2.wav");
	PRECACHE_SOUND("weapons/knife_hit3.wav");
	PRECACHE_SOUND("weapons/knife_wall1.wav");
	PRECACHE_SOUND("weapons/knife_wall2.wav");
	PRECACHE_SOUND("weapons/knife_wall3.wav");
	PRECACHE_SOUND("weapons/knife_swipe1.wav");
	PRECACHE_SOUND("weapons/knife_swipe2.wav");

	m_usKnife= PRECACHE_EVENT ( 1, "events/knife.sc" );
}

int CKnife::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 3;
	p->iId = WEAPON_KNIFE;
	p->iWeight = KNIFE_WEIGHT;
	return 1;
}



BOOL CKnife::Deploy( )
{
	return DefaultDeploy( "models/v_knife.mdl", "models/p_knife.mdl", KNIFE_DRAW, "crowbar" );
}

void CKnife::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	SendWeaponAnim( KNIFE_HOLSTER );
}


void FindHullIntersection_knife( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
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


void CKnife::PrimaryAttack()
{
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usKnife, 
	0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
	0.0, 0, 0.0 );

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	SetThink( Hit );
	pev->nextthink = gpGlobals->time + 0.15;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
}
void CKnife::Hit( void )
{
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
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection_knife( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );

			vecEnd = tr.vecEndPos;
		}
	}
#endif


	if ( tr.flFraction >= 1.0 )
	{
	}
	else
	{
		
#ifndef CLIENT_DLL

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if ( FClassnameIs( pEntity->pev, "monster_maria" ) 
			|| FClassnameIs( pEntity->pev, "monster_boris" ) )
			return;

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgKnife, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgKnife / 2, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				switch (RANDOM_LONG(0,2))
				{
					case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/knife_hit1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
					case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/knife_hit2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
					case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/knife_hit3.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
				}

				m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;
				if ( pEntity->IsAlive() )
				{
					  flVol = 0.1;
				}

				fHitWorld = FALSE;
			}
		}
		if (fHitWorld)
		{
			float fvolbar;
			fvolbar = 1;

			switch (RANDOM_LONG(0,2))
			{
				case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/knife_wall1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
				case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/knife_wall2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
				case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/knife_wall3.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
			}

			PlayTextureSound( pEntity ,tr ,vecSrc, vecEnd );

			UTIL_Sparks( tr.vecEndPos + gpGlobals->v_right - 5 );
			UTIL_Sparks( tr.vecEndPos );
			UTIL_Sparks( tr.vecEndPos + gpGlobals->v_right + 5 );

			DecalGunshot( &tr, BULLET_PLAYER_KNIFE );
		}
		m_pPlayer->m_iWeaponVolume = flVol * KNIFE_WALLHIT_VOLUME;
#endif
	}
}
extern int gmsgParticles;
void CKnife::PlayTextureSound( CBaseEntity *pEntity, TraceResult tr, Vector vecSrc, Vector vecEnd )
{
#ifndef CLIENT_DLL
	char chTextureType;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];
	bool b_CanMakeParticles;
	chTextureType = 0;

	if ( CVAR_GET_FLOAT("r_particle" ) != 0 )		
		b_CanMakeParticles = TRUE;
	else
		b_CanMakeParticles = FALSE;

	vecSrc.CopyToArray(rgfl1);
	vecEnd.CopyToArray(rgfl2);

	if (pEntity)
		pTextureName = TRACE_TEXTURE( ENT(pEntity->pev), rgfl1, rgfl2 );
	else
		pTextureName = TRACE_TEXTURE( ENT(0), rgfl1, rgfl2 );
		
	if ( pTextureName )
	{
		if (*pTextureName == '-' || *pTextureName == '+')
			pTextureName += 2;

		if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			pTextureName++;

		strcpy(szbuffer, pTextureName);
		szbuffer[CBTEXTURENAMEMAX - 1] = 0;
		chTextureType = TEXTURETYPE_Find(szbuffer);	
	}

	if ( pEntity && pEntity->pev->rendermode == kRenderTransTexture || chTextureType == CHAR_TEX_GLASS )
	{
		switch ( RANDOM_LONG(0,3) )
		{
			case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/glass/glass_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
			case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/glass/glass_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
			case 2: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/glass/glass_impact_bullet3.wav", 1, ATTN_NORM, 0, 100); break;
			case 3: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/glass/glass_impact_bullet4.wav", 1, ATTN_NORM, 0, 100); break;
		}
	}
	else
	{
		if (chTextureType == CHAR_TEX_METAL)		
		{	
			UTIL_Ricochet( tr.vecEndPos, 0.5 );

			if (RANDOM_LONG( 0, 99 ) < 40)
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 500 );//chispas

			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 9, 5, 5, 100 );//puntos
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 20 );//chispas

			switch ( RANDOM_LONG(0,1) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/metal_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/metal_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
			}

			b_CanMakeParticles = FALSE;
		}
		else if (chTextureType == CHAR_TEX_VENT)		
		{
					
			UTIL_Ricochet( tr.vecEndPos, 0.5 );

			if (RANDOM_LONG( 0, 99 ) < 40)
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 500 );//chispas

			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 9, 5, 5, 100 );//puntos
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 20 );//chispas
		
			switch ( RANDOM_LONG(0,3) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/vent_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/vent_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
				case 2: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/vent_impact_bullet3.wav", 1, ATTN_NORM, 0, 100); break;
				case 3: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/vent_impact_bullet4.wav", 1, ATTN_NORM, 0, 100); break;
			}

			b_CanMakeParticles = FALSE;
		}
		else if (chTextureType == CHAR_TEX_COMPUTER)		
		{
					
			UTIL_Ricochet( tr.vecEndPos, 0.5 );

			if (RANDOM_LONG( 0, 99 ) < 40)
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 500 );//chispas

			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 9, 5, 5, 100 );//puntos
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 20 );//chispas

			UTIL_Sparks( tr.vecEndPos );
						
			switch ( RANDOM_LONG(0,1) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "buttons/spark5.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "buttons/spark6.wav", 1, ATTN_NORM, 0, 100); break;
			}

			b_CanMakeParticles = FALSE;
		}
		else if (chTextureType == CHAR_TEX_GRATE)		
		{
					
			UTIL_Ricochet( tr.vecEndPos, 0.5 );

			if (RANDOM_LONG( 0, 99 ) < 40)
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 500 );//chispas

			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 9, 5, 5, 100 );//puntos
			UTIL_WhiteSparks( tr.vecEndPos, tr.vecPlaneNormal, 0, 5, 500, 20 );//chispas

			UTIL_Sparks( tr.vecEndPos );
				
			switch ( RANDOM_LONG(0,1) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/metal_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/metal/metal_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
			}

			b_CanMakeParticles = FALSE;
		}
		else if (chTextureType == CHAR_TEX_DIRT || chTextureType == CHAR_TEX_SNOW || chTextureType == CHAR_TEX_GRAVEL || chTextureType == CHAR_TEX_RUG )		
		{
			switch ( RANDOM_LONG(0,3) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/sand/sand_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/sand/sand_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
				case 2: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/sand/sand_impact_bullet3.wav", 1, ATTN_NORM, 0, 100); break;
				case 3: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/sand/sand_impact_bullet4.wav", 1, ATTN_NORM, 0, 100); break;
			}
		}
		else if (chTextureType == CHAR_TEX_CONCRETE || chTextureType == CHAR_TEX_CONCRETE && pEntity && pEntity->pev->rendermode != kRenderTransAlpha )				
		{
			switch ( RANDOM_LONG(0,3) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/concrete/concrete_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/concrete/concrete_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
				case 2: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/concrete/concrete_impact_bullet3.wav", 1, ATTN_NORM, 0, 100); break;
				case 3: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/concrete/concrete_impact_bullet4.wav", 1, ATTN_NORM, 0, 100); break;
			}
		}
		else if (chTextureType == CHAR_TEX_TILE )		
		{
			switch ( RANDOM_LONG(0,3) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/tile/tile_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/tile/tile_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
				case 2: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/tile/tile_impact_bullet3.wav", 1, ATTN_NORM, 0, 100); break;
				case 3: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/tile/tile_impact_bullet4.wav", 1, ATTN_NORM, 0, 100); break;
			}
		}
		else if (chTextureType == CHAR_TEX_WOOD )		
		{
			switch ( RANDOM_LONG(0,4) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/wood/wood_impact_bullet1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/wood/wood_impact_bullet2.wav", 1, ATTN_NORM, 0, 100); break;
				case 2: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/wood/wood_impact_bullet3.wav", 1, ATTN_NORM, 0, 100); break;
				case 3: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/wood/wood_impact_bullet4.wav", 1, ATTN_NORM, 0, 100); break;
				case 4: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "impact/wood/wood_impact_bullet5.wav", 1, ATTN_NORM, 0, 100); break;
			}
		}
		else if (chTextureType == CHAR_TEX_ORGANIC )		
		{
			switch ( RANDOM_LONG(0,1) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "weapons/bullet_hit1.wav", 1, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), tr.vecEndPos, "weapons/bullet_hit2.wav", 1, ATTN_NORM, 0, 100); break;
			}
		}
	}

	if ( pEntity && pEntity->pev->rendermode == kRenderTransAlpha )
		b_CanMakeParticles = FALSE;

	if ( b_CanMakeParticles )
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgParticles);
			WRITE_SHORT(0);
			WRITE_BYTE(0);
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );

			if ( pEntity && pEntity->pev->rendermode == kRenderTransTexture )
			{
				WRITE_SHORT(iDefaultHitGlass);
			}
			else
			{
				if (chTextureType == CHAR_TEX_RUG)		
					WRITE_SHORT(iDefaultHitGreen);
				else if (chTextureType == CHAR_TEX_DIRT)		
					WRITE_SHORT(iDefaultHitDirt);
				else if (chTextureType == CHAR_TEX_GRAVEL)		
					WRITE_SHORT(iDefaultHitGravel);
				else if (chTextureType == CHAR_TEX_ORGANIC)		
					WRITE_SHORT(iDefaultHitSlime);
				else if (chTextureType == CHAR_TEX_WOOD)
					WRITE_SHORT(iDefaultHitWood1);
				else if (chTextureType == CHAR_TEX_SNOW)
					WRITE_SHORT(iDefaultHitSnow);
				else if (chTextureType == CHAR_TEX_TILE)
					WRITE_SHORT(iDefaultHitTile);
				else if (chTextureType == CHAR_TEX_GLASS)
					WRITE_SHORT(iDefaultHitGlass);
				else
					WRITE_SHORT(iDefaultWallSmoke);
			}
		MESSAGE_END();
	}
#endif
}

void CKnife::WeaponIdle( void )
{

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

		iAnim = KNIFE_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 151.0 / 15;

		SendWeaponAnim( iAnim, 1 );

}