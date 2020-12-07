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
#include "customentity.h"
#include "particle.h"
#include "weapons.h"
#include "decals.h"
#include "func_break.h"
#include "shake.h"
#include "entity_state.h"
#include "gamerules.h"
#include "player.h"
#include "grass.h"

//===============================================
// Particle Emitter - Standard Particle emitter,
// with PVS checking abilities. Very useful.
//===============================================
extern int gmsgParticles;

LINK_ENTITY_TO_CLASS( env_particle, CParticleEmitter );
LINK_ENTITY_TO_CLASS( env_particleemitter, CParticleEmitter );

TYPEDESCRIPTION	CParticleEmitter::m_SaveData[] = 
{
	DEFINE_FIELD( CParticleEmitter, m_iIDNumber, FIELD_INTEGER ),
	DEFINE_FIELD( CParticleEmitter, m_fParticleActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CParticleEmitter, m_fDeactivatedByPVS, FIELD_BOOLEAN ),
	DEFINE_FIELD( CParticleEmitter, m_fDeactivated, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CParticleEmitter, CPointEntity );

int CParticleEmitter::ms_iNextFreeKey = 1;

void CParticleEmitter::Spawn( void )
{
	if (FStringNull(pev->targetname))
		pev->spawnflags |= 1;

	m_iIDNumber = ms_iNextFreeKey;
	ms_iNextFreeKey++;

	pev->solid = SOLID_NOT;	// Remove model & collisions

	pev->nextthink = gpGlobals->time + 0.1;
	SetThink( ParticleThink );

}
void CParticleEmitter::ParticleThink( void )
{
	if ( !(pev->spawnflags & SF_EMITTER_STARTON) )
	{
		m_fParticleActive = FALSE; 
		m_fDeactivated = TRUE;
	}
	else
	{
		m_fParticleActive = TRUE; 
		m_fDeactivated = FALSE;
		SetThink( ParticleTurnOn );
		pev->nextthink = gpGlobals->time + 0.001;
	}
};
void CParticleEmitter::ParticleTurnOn( void )
{
	if ( m_fParticleActive )
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgParticles);//enviar a todos... qué importa??
			WRITE_SHORT(m_iIDNumber);
			WRITE_BYTE(0);
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);
			WRITE_COORD(pev->angles.x);
			WRITE_COORD(pev->angles.y);
			WRITE_COORD(pev->angles.z);
			WRITE_SHORT(0);
			WRITE_STRING(STRING(pev->message));
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgParticles);//enviar a todos... qué importa??
			WRITE_SHORT(m_iIDNumber);
			WRITE_BYTE(1);
		MESSAGE_END();
	}
	m_fNoUpdateNeeded = TRUE;

	SetThink( ParticleThinkContinous );
	pev->nextthink = gpGlobals->time + 0.5;
}
void CParticleEmitter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_fDeactivated )
	{
		m_fParticleActive = TRUE; 
		m_fDeactivated = FALSE;
		SetThink( ParticleTurnOn );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}
	else
	{
		m_fParticleActive = FALSE; 
		m_fDeactivated = TRUE;
		SetThink( ParticleTurnOn );
		pev->nextthink = gpGlobals->time + 0.001;
		return;
	}
};
void CParticleEmitter::ParticleThinkContinous( void )
{
	if ( !m_fNoUpdateNeeded && m_fParticleActive && !m_fDeactivated )
	{
		SetThink( ParticleTurnOn );
		pev->nextthink = gpGlobals->time + 0.001;
	}
	else
	{
		SetThink( ParticleThinkContinous );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	if ( !(pev->spawnflags & SF_EMITTER_NO_PVS_CHECK) )
	{
		if ( !m_fDeactivated )
		{
			if ( m_fParticleActive )
			{
				if ( FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
				{
					m_fParticleActive = FALSE;
					m_fDeactivatedByPVS = TRUE;
					SetThink( ParticleTurnOn );
					pev->nextthink = gpGlobals->time + 0.001;
				}
			}
			else
			{
				if ( m_fDeactivatedByPVS )
				{
					if ( !FNullEnt( FIND_CLIENT_IN_PVS( edict() ) ) )
					{
						m_fParticleActive = TRUE;
						m_fDeactivatedByPVS = FALSE;
						SetThink( ParticleTurnOn );
						pev->nextthink = gpGlobals->time + 0.001;
					}
				}
			}
		}
	}
}