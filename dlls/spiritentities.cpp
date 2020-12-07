//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"

//==================================================================
//LRC- Xen monsters' warp-in effect, for those too lazy to build it. :)
//==================================================================
class CEnvWarpBall : public CBaseEntity
{
public:
	void	Precache( void );
	void	Spawn( void ) { Precache(); }
	void	Think( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( env_warpball, CEnvWarpBall );

void CEnvWarpBall::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_MODEL( "sprites/Fexplo1.spr" );
	PRECACHE_MODEL( "sprites/XFlare1.spr" );
	PRECACHE_SOUND( "debris/beamstart2.wav" );
	PRECACHE_SOUND( "debris/beamstart7.wav" );
}

void CEnvWarpBall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam *pBeam;
	while (iDrawn<pev->frags && iTimes<(pev->frags * 3)) // try to draw <frags> beams, but give up after 3x<frags> tries.
	{
		vecDest = pev->health * (Vector(RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1)).Normalize());
		UTIL_TraceLine( pev->origin, pev->origin + vecDest, ignore_monsters, NULL, &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate("sprites/lgtning.spr",200);
			pBeam->PointsInit( pev->origin, tr.vecEndPos );
			pBeam->SetColor( 197, 243, 169 );
			pBeam->SetNoise( 65 );
			pBeam->SetBrightness( 150 );
			pBeam->SetWidth( 18 );
			pBeam->SetScrollRate( 35 );
			pBeam->SetThink(&CBeam:: SUB_Remove );
			pBeam->pev->nextthink = gpGlobals->time + 1;
		}
		iTimes++;
	}
	EMIT_SOUND( edict(), CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(pev->origin.x);	// X
		WRITE_COORD(pev->origin.y);	// Y
		WRITE_COORD(pev->origin.z);	// Z
		WRITE_BYTE( 20 );		// radius * 0.1
		WRITE_BYTE( 197 );		// r
		WRITE_BYTE( 243 );		// g
		WRITE_BYTE( 169 );		// b
		WRITE_BYTE( 30 );		// time * 10
		WRITE_BYTE( 5 );		// decay * 0.1
	MESSAGE_END( );

	CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", pev->origin, TRUE );
	pSpr->AnimateAndDie( 10 );
	pSpr->SetTransparency(kRenderGlow,  77, 210, 130,  255, kRenderFxNoDissipation);

	pSpr = CSprite::SpriteCreate( "sprites/XFlare1.spr", pev->origin, TRUE );
	pSpr->AnimateAndDie( 10 );
	pSpr->SetTransparency(kRenderGlow,  184, 250, 214,  255, kRenderFxNoDissipation);

	SetThink( Think );
	pev->nextthink = gpGlobals->time + 0.5;
}

void CEnvWarpBall::Think( void )
{
	EMIT_SOUND( edict(), CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM );
}
