//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#include "particle_defs.h"

#define SF_EMITTER_STARTON 1
#define SF_EMITTER_NO_PVS_CHECK 2
class CParticleEmitter : public CPointEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT	ParticleThink( void );
	void EXPORT	ParticleTurnOn( void );
	void EXPORT	ParticleThinkContinous( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	static int	ms_iNextFreeKey;
	int		m_iIDNumber;

	BOOL	m_fParticleActive;
	BOOL	m_fNoUpdateNeeded;
	BOOL	m_fDeactivatedByPVS;
	BOOL	m_fDeactivated;

};