//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//=========================================================
// Freed vortigaunt in Raven City.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"rcallymonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"effects.h"
#include	"weapons.h"
#include	"soundent.h"

extern DLL_GLOBAL int		g_iSkillLevel;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		VORTIGAUNT_AE_CLAW					( 1 )
#define		VORTIGAUNT_AE_CLAWRAKE				( 2 )
#define		VORTIGAUNT_AE_ZAP_POWERUP			( 3 )
#define		VORTIGAUNT_AE_ZAP_SHOOT				( 4 )
#define		VORTIGAUNT_AE_ZAP_DONE				( 5 )
#define		VORTIGAUNT_AE_ZAP_CHARGE_START		( 6 )
#define		VORTIGAUNT_AE_ZAP_CHARGE_LOOP		( 7 )
#define		VORTIGAUNT_AE_ZAP_CHARGE_DONE		( 8 )

//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_VORT_NOFIRE	( bits_COND_SPECIAL4 )
//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_VORT_CHECK_FIRE = LAST_TALKMONSTER_TASK + 1,
};
//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_VORTIGAUNT_ESTABLISH_LINE_OF_FIRE = LAST_TALKMONSTER_SCHEDULE + 1,
};

#define		VORTIGAUNT_MAX_BEAMS	32

class CVortigaunt : public CRCAllyMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void AlertSound( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	void StartTask( Task_t *pTask );
	void CallForHelp( char *szClassname, float flDist, EHANDLE hEnemy, Vector &vecLocation );
	virtual int	ObjectCaps( void ) { return CRCAllyMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	void Killed( entvars_t *pevAttacker, int iGib );
	void DeclineFollowing( void );

	// Override these to set behavior
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );
	
	void TalkInit( void );

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void ClearBeams( );
	void ArmBeam( int side );
	void WackBeam( int side, CBaseEntity *pEntity );
	void ZapBeam( int side );
	void BeamGlow( void );

	int m_iBravery;

	CBeam *m_pBeam[VORTIGAUNT_MAX_BEAMS];

	int m_iBeams;
	float m_flNextAttack;

	BOOL FOkToSpeak( void );
	void JustSpoke( void );

	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;

	EHANDLE m_hDead;

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

	float	m_flPlayerDamage;// how much pain has the player inflicted on me?

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_vortigaunt_ally, CVortigaunt );

TYPEDESCRIPTION	CVortigaunt::m_SaveData[] = 
{
	DEFINE_FIELD( CVortigaunt, m_iBravery, FIELD_INTEGER ),
	DEFINE_ARRAY( CVortigaunt, m_pBeam, FIELD_CLASSPTR, VORTIGAUNT_MAX_BEAMS ),
	DEFINE_FIELD( CVortigaunt, m_iBeams, FIELD_INTEGER ),
	DEFINE_FIELD( CVortigaunt, m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( CVortigaunt, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CVortigaunt, m_hDead, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CVortigaunt, CRCAllyMonster );

const char *CVortigaunt::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CVortigaunt::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CVortigaunt :: FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if (gpGlobals->time <= CRCAllyMonster::g_talkWaitTime)
		return FALSE;

	if ( pev->spawnflags & SF_MONSTER_GAG )
	{
		if ( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			// no talking outside of combat if gagged.
			return FALSE;
		}
	}

	// if player is not in pvs, don't speak
//	if (FNullEnt(FIND_CLIENT_IN_PVS(edict())))
//		return FALSE;
	
	return TRUE;
}
//=========================================================
//=========================================================
void CVortigaunt :: JustSpoke( void )
{
	CRCAllyMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(1.5, 2.0);
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlVortigauntFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)100		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slVortigauntFollow[] =
{
	{
		tlVortigauntFollow,
		ARRAYSIZE ( tlVortigauntFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Vortigaunt Follow"
	},
};

Task_t	tlVortigauntFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slVortigauntFaceTarget[] =
{
	{
		tlVortigauntFaceTarget,
		ARRAYSIZE ( tlVortigauntFaceTarget ),
		bits_COND_CLIENT_PUSH	|
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Vortigaunt FaceTarget"
	},
};


Task_t	tlVortigauntIdleStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slVortigauntIdleStand[] =
{
	{ 
		tlVortigauntIdleStand,
		ARRAYSIZE ( tlVortigauntIdleStand ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL			|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|// sound flags - change these, and you'll break the talking code.
		//bits_SOUND_PLAYER		|
		//bits_SOUND_WORLD		|
		
		bits_SOUND_DANGER		|
		bits_SOUND_MEAT			|// scents
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t	tlVortigauntTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_COWER			},// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slVortigauntTakeCoverFromBestSound[] =
{
	{ 
		tlVortigauntTakeCoverFromBestSound,
		ARRAYSIZE ( tlVortigauntTakeCoverFromBestSound ), 
		0,
		0,
		"FriendlyGruntTakeCoverFromBestSound"
	},
};

// primary range attack
Task_t	tlVortigauntAttack1[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_VORT_CHECK_FIRE,		(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_VORT_CHECK_FIRE,		(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t	slVortigauntAttack1[] =
{
	{ 
		tlVortigauntAttack1,
		ARRAYSIZE ( tlVortigauntAttack1 ), 
		bits_COND_HEAR_SOUND |
		bits_COND_HEAVY_DAMAGE, 

		bits_SOUND_DANGER,
		"Vortigaunt Range Attack1"
	},
};

//=========================================================
// VortigauntCombatFace Schedule
//=========================================================
Task_t	tlVortigauntCombatFace1[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_WAIT,					(float)1.5					},
};

Schedule_t	slVortigauntCombatFace[] =
{
	{ 
		tlVortigauntCombatFace1,
		ARRAYSIZE ( tlVortigauntCombatFace1 ), 
		bits_COND_NEW_ENEMY				|
		bits_COND_ENEMY_DEAD			|
		bits_COND_CAN_RANGE_ATTACK1		|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Vortigaunt Combat Face"
	},
};

//=========================================================
// Vortigaunt Combat Fail
//=========================================================
Task_t	tlVortigauntCombatFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY,		(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slVortigauntCombatFail[] =
{
	{
		tlVortigauntCombatFail,
		ARRAYSIZE ( tlVortigauntCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Vortigaunt Combat Fail"
	},
};

//=========================================================
// Vortigaunt Fail
//=========================================================
Task_t	tlVortigauntFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slVortigauntFail[] =
{
	{
		tlVortigauntFail,
		ARRAYSIZE ( tlVortigauntFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"Vortigaunt Fail"
	},
};

//=========================================================
// Establish line of fire - move to a position that allows
// the vort to attack.
//=========================================================
Task_t tlVortigauntEstablishLineOfFire[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_TAKE_COVER_FROM_ENEMY	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0						},
	{ TASK_RUN_PATH,			(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slVortigauntEstablishLineOfFire[] =
{
	{ 
		tlVortigauntEstablishLineOfFire,
		ARRAYSIZE ( tlVortigauntEstablishLineOfFire ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"GruntEstablishLineOfFire"
	},
};

DEFINE_CUSTOM_SCHEDULES( CVortigaunt )
{
	slVortigauntFollow,
	slVortigauntFaceTarget,
	slVortigauntIdleStand,
	slVortigauntTakeCoverFromBestSound,
	slVortigauntAttack1,
	slVortigauntCombatFace,
	slVortigauntCombatFail,
	slVortigauntFail,
	slVortigauntEstablishLineOfFire,
};


IMPLEMENT_CUSTOM_SCHEDULES( CVortigaunt, CRCAllyMonster );

//=========================================================
// StartTask
//=========================================================
void CVortigaunt :: StartTask ( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_VORT_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_VORT_NOFIRE );
		}
		TaskComplete();
		break;

	default: 
		CRCAllyMonster :: StartTask( pTask );
		break;
	}

	ClearBeams( );
}


void CVortigaunt::Killed( entvars_t *pevAttacker, int iGib )
{
	ClearBeams( );
	CRCAllyMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CVortigaunt :: ISoundMask ( void) 
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CVortigaunt :: Classify ( void )
{
	return	CLASS_ALLY_VORTIGAUNT;
}

//=========================================================
// ALertSound - vortigaunt says "For Freedom!"
//=========================================================
void CVortigaunt :: AlertSound( void )
{
	if ( FOkToSpeak() )
	{
		if ( m_hEnemy != NULL )
		{
			PlaySentence( "VO_ATTACK", RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_NORM );
			JustSpoke();
			CallForHelp( "monster_vortigaunt_ally", 512, m_hEnemy, m_vecEnemyLKP );
			CallForHelp( "monster_human_grunt_ally", 512, m_hEnemy, m_vecEnemyLKP );
		}
	}
}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CVortigaunt :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_WALK:		
		ys = 50;	
		break;
	case ACT_RUN:		
		ys = 70;
		break;
	case ACT_IDLE:		
		ys = 50;
		break;
	default:
		ys = 90;
		break;
	}

	pev->yaw_speed = ys;
}
		
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CVortigaunt :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	ALERT( at_console, "event %d : %f\n", pEvent->event, pev->frame );
	switch( pEvent->event )
	{
		case VORTIGAUNT_AE_CLAW:
		{
			// SOUND HERE!
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.vortigauntDmgClaw, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
				}
				// Play a random attack hit sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
			else
			{
				// Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
		}
		break;

		case VORTIGAUNT_AE_CLAWRAKE:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.vortigauntDmgClawrake, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
			else
			{
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
		}
		break;

		case VORTIGAUNT_AE_ZAP_POWERUP:
		{

			pev->framerate = 1.5;

			UTIL_MakeAimVectors( pev->angles );

			if (m_iBeams == 0)
			{
				Vector vecSrc = pev->origin + gpGlobals->v_forward * 2;
				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
					WRITE_BYTE(TE_DLIGHT);
					WRITE_COORD(vecSrc.x);	// X
					WRITE_COORD(vecSrc.y);	// Y
					WRITE_COORD(vecSrc.z);	// Z
					WRITE_BYTE( 12 );		// radius * 0.1
					WRITE_BYTE( 255 );		// r
					WRITE_BYTE( 180 );		// g
					WRITE_BYTE( 96 );		// b
					WRITE_BYTE( 20 / pev->framerate );		// time * 10
					WRITE_BYTE( 0 );		// decay * 0.1
				MESSAGE_END( );

			}
			if (m_hDead != NULL)
			{
				WackBeam( -1, m_hDead );
				WackBeam( 1, m_hDead );
			}
			else
			{
				ArmBeam( -1 );
				ArmBeam( 1 );
				BeamGlow( );
			}

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "debris/zap4.wav", 1, ATTN_NORM, 0, 100 + m_iBeams * 10 );
			pev->skin = m_iBeams / 2;
		}
		break;

		case VORTIGAUNT_AE_ZAP_SHOOT:
		{
			ClearBeams( );

			if (m_hDead != NULL)
			{
				Vector vecDest = m_hDead->pev->origin + Vector( 0, 0, 38 );
				TraceResult trace;
				UTIL_TraceHull( vecDest, vecDest, dont_ignore_monsters, human_hull, m_hDead->edict(), &trace );

				if ( !trace.fStartSolid )
				{
					CBaseEntity *pNew = Create( "monster_vortigaunt_ally", m_hDead->pev->origin, m_hDead->pev->angles );
					CBaseMonster *pNewMonster = pNew->MyMonsterPointer( );
					pNew->pev->spawnflags |= 1;
					WackBeam( -1, pNew );
					WackBeam( 1, pNew );
					UTIL_Remove( m_hDead );
					EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "hassault/hw_shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 130, 160 ) );
					CBaseEntity *pEffect = Create( "test_effect", pNew->Center(), pev->angles );
					pEffect->Use( this, this, USE_ON, 1 );
					
					break;
				}
			}
			ClearMultiDamage();

			UTIL_MakeAimVectors( pev->angles );

			ZapBeam( -1 );
			ZapBeam( 1 );

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "hassault/hw_shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 130, 160 ) );
			ApplyMultiDamage(pev, pev);

			m_flNextAttack = gpGlobals->time + RANDOM_FLOAT( 0.5, 4.0 );
		}
		break;

		case VORTIGAUNT_AE_ZAP_DONE:
		{
			ClearBeams( );
		}
		break;

		default:
			CRCAllyMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// CheckRangeAttack1 - normal beam attack 
//=========================================================
BOOL CVortigaunt :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( m_flNextAttack > gpGlobals->time && !NoFriendlyFire() )
	{
		return FALSE;
	}
	return CRCAllyMonster::CheckRangeAttack1( flDot, flDist );
}

void CVortigaunt :: CallForHelp( char *szClassname, float flDist, EHANDLE hEnemy, Vector &vecLocation )
{
	if ( FStringNull( pev->netname ))
		return;

	CBaseEntity *pEntity = NULL;

	while ((pEntity = UTIL_FindEntityByString( pEntity, "netname", STRING( pev->netname ))) != NULL)
	{
		float d = (pev->origin - pEntity->pev->origin).Length();
		if (d < flDist)
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer( );
			if (pMonster)
			{
				pMonster->m_afMemory |= bits_MEMORY_PROVOKED;
				pMonster->PushEnemy( hEnemy, vecLocation );
			}
		}
	}
}

//=========================================================
// CheckRangeAttack2 - check bravery and try to resurect dead beret grunts
//=========================================================
BOOL CVortigaunt :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if (m_flNextAttack > gpGlobals->time)
	{
		return FALSE;
	}

	m_hDead = NULL;
	m_iBravery = 0;

	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityByClassname( pEntity, "monster_vortigaunt_ally" )))
	{
		TraceResult tr;

		UTIL_TraceLine( EyePosition( ), pEntity->EyePosition( ), ignore_monsters, ENT(pev), &tr );
		if (tr.flFraction == 1.0 || tr.pHit == pEntity->edict())
		{
			if (pEntity->pev->deadflag == DEAD_DEAD)
			{
				float d = (pev->origin - pEntity->pev->origin).Length();
				if (d < flDist)
				{
					m_hDead = pEntity;
					flDist = d;
				}
				m_iBravery--;
			}
			else
			{
				m_iBravery++;
			}
		}
	}

	if (m_hDead != NULL)
		return TRUE;
	else
		return FALSE;
}
//=========================================================
// Spawn
//=========================================================
void CVortigaunt :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/vortigaunt.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health			= gSkillData.vortigauntHealth;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	m_afCapability		= bits_CAP_SQUAD | bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK2 | bits_CAP_DOORS_GROUP;

	MonsterInit();
	SetUse( FollowerUse );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CVortigaunt :: Precache()
{
	PRECACHE_MODEL("models/vortigaunt.mdl");
	PRECACHE_MODEL("sprites/lgtning.spr");

	PRECACHE_SOUND("debris/zap1.wav");
	PRECACHE_SOUND("debris/zap4.wav");

	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("hassault/hw_shoot1.wav");

	PRECACHE_SOUND("zombie/claw_miss1.wav");
	PRECACHE_SOUND("zombie/claw_miss2.wav");

	PRECACHE_SOUND("zombie/claw_strike1.wav");
	PRECACHE_SOUND("zombie/claw_strike2.wav");
	PRECACHE_SOUND("zombie/claw_strike3.wav");

	PRECACHE_SOUND("vortigaunt/vo_pain1.wav");
	PRECACHE_SOUND("vortigaunt/vo_pain2.wav");
	PRECACHE_SOUND("vortigaunt/vo_pain3.wav");

	PRECACHE_SOUND("vortigaunt/vo_die1.wav");
	PRECACHE_SOUND("vortigaunt/vo_die2.wav");
	PRECACHE_SOUND("vortigaunt/vo_die3.wav");

	UTIL_PrecacheOther( "test_effect" );

	TalkInit();
	CRCAllyMonster::Precache();
}	

// Init talk data
void CVortigaunt :: TalkInit()
{
	
	CRCAllyMonster::TalkInit();

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER]  =	"VO_ANSWER";
	m_szGrp[TLK_QUESTION] =	"VO_QUESTION";
	m_szGrp[TLK_IDLE] =		"VO_IDLE";
	m_szGrp[TLK_STARE] =	"VO_STARE";
	m_szGrp[TLK_USE] =		"VO_OK";
	m_szGrp[TLK_UNUSE] =	"VO_WAIT";
	m_szGrp[TLK_STOP] =		"VO_STOP";

	m_szGrp[TLK_NOSHOOT] =	"VO_SCARED";
	m_szGrp[TLK_HELLO] =	"VO_HELLO";

	m_szGrp[TLK_PLHURT1] =	"!VO_CUREA";
	m_szGrp[TLK_PLHURT2] =	"!VO_CUREB"; 
	m_szGrp[TLK_PLHURT3] =	"!VO_CUREC";

	m_szGrp[TLK_SMELL] =	"VO_SMELL";
	
	m_szGrp[TLK_WOUND] =	"VO_WOUND";
	m_szGrp[TLK_MORTAL] =	"VO_MORTAL";

	m_voicePitch = 100;
}


static BOOL IsFacing( entvars_t *pevTest, const Vector &reference )
{
	Vector vecDir = (reference - pevTest->origin);
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	Vector forward, angle;
	angle = pevTest->v_angle;
	angle.x = 0;
	UTIL_MakeVectorsPrivate( angle, forward, NULL, NULL );
	// He's facing me, he meant it
	if ( DotProduct( forward, vecDir ) > 0.96 )	// +/- 15 degrees or so
	{
		return TRUE;
	}
	return FALSE;
}


int CVortigaunt :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// make sure friends talk about it if player hurts talkmonsters...
	int ret = CRCAllyMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	if ( m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT) )
	{
		m_flPlayerDamage += flDamage;

		// This is a heurstic to determine if the player intended to harm me
		// If I have an enemy, we can't establish intent (may just be crossfire)
		if ( m_hEnemy == NULL )
		{
			// If the player was facing directly at me, or I'm already suspicious, get mad
			if ( (m_afMemory & bits_MEMORY_SUSPICIOUS) )
			{
				// Alright, now I'm pissed!
				PlaySentence( "VO_MAD", 4, VOL_NORM, ATTN_NORM );

				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				// Hey, be careful with that
				PlaySentence( "VO_SHOT", 4, VOL_NORM, ATTN_NORM );
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
		{
			PlaySentence( "VO_SHOT", 4, VOL_NORM, ATTN_NORM );
		}
	}

	return ret;
}

	
//=========================================================
// PainSound
//=========================================================
void CVortigaunt :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "vortigaunt/vo_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "vortigaunt/vo_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "vortigaunt/vo_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CVortigaunt :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "vortigaunt/vo_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "vortigaunt/vo_die2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "vortigaunt/vo_die3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}


void CVortigaunt::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (bitsDamageType & DMG_SHOCK)
		return;

//	if (bitsDamageType & DMG_ENERGYBEAM)
//		return;

	CRCAllyMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CVortigaunt :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that vortigaunt will talk
		// when 'used' 
		psched = CRCAllyMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slVortigauntFaceTarget;	// override this for different target face behavior
		else
			return psched;

	case SCHED_TARGET_CHASE:
		return slVortigauntFollow;

	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slVortigauntTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// vortigaunt has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slVortigauntCombatFail[ 0 ];
			}
			return &slVortigauntFail[ 0 ];
		}
	case SCHED_VORTIGAUNT_ESTABLISH_LINE_OF_FIRE:
		{
			return &slVortigauntEstablishLineOfFire[ 0 ];
		}
		break;
	case SCHED_COMBAT_FACE:
		{
			return &slVortigauntCombatFace[ 0 ];
		}
	case SCHED_RANGE_ATTACK1:
		return slVortigauntAttack1;
	case SCHED_RANGE_ATTACK2:
		return slVortigauntAttack1;

	case SCHED_IDLE_STAND:
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CRCAllyMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
		{
			// just look straight ahead.
			return slVortigauntIdleStand;
		}
		else
			return psched;	
	}

	return CRCAllyMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CVortigaunt :: GetSchedule ( void )
{

	ClearBeams( );

	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		PlaySentence( "VO_KILL", 4, VOL_NORM, ATTN_NORM );
	}

	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
// dead enemy
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		{
			// call base class, all code to handle dead enemies is centralized there.
			return CBaseMonster :: GetSchedule();
		}

// new enemy
		if ( HasConditions(bits_COND_NEW_ENEMY) )
		{
			if ( InSquad() )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				if ( !IsLeader() )
				{
					return GetScheduleOfType ( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
		}
// can shoot
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				if ( InSquad() )
				{
					// if the enemy has eluded the squad and a squad member has just located the enemy
					// and the enemy does not see the squad member, issue a call to the squad to waste a 
					// little time and give the player a chance to turn.
					if ( MySquadLeader()->m_fEnemyEluded && !HasConditions ( bits_COND_ENEMY_FACING_ME ) )
					{
						MySquadLeader()->m_fEnemyEluded = FALSE;
					}
				}
				if ( OccupySlot ( bits_SLOTS_FGRUNT_ENGAGE ) )
				{
					// try to take an available ENGAGE slot
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				else
				{
					// hide!
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
		if (pev->health < 5 || m_iBravery < 0)
		{
			if (!HasConditions( bits_COND_CAN_MELEE_ATTACK1 ))
			{
				m_failSchedule = SCHED_CHASE_ENEMY;
				if (HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
				{
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				if ( HasConditions ( bits_COND_SEE_ENEMY ) && HasConditions ( bits_COND_ENEMY_FACING_ME ) )
				{
					ALERT( at_console, "exposed\n");
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
		}
		if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			return GetScheduleOfType ( SCHED_VORTIGAUNT_ESTABLISH_LINE_OF_FIRE );
		}
		break;

	case MONSTERSTATE_ALERT:	
	case MONSTERSTATE_IDLE:
		if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			// flinch if hurt
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		}

		if ( m_hEnemy == NULL && IsFollowing() )
		{
			if ( !m_hTargetEnt->IsAlive() )
			{
				// UNDONE: Comment about the recently dead player here?
				StopFollowing( FALSE );
				break;
			}
			else
			{
				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				{
					return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );
				}
				return GetScheduleOfType( SCHED_TARGET_FACE );
			}
		}

		if ( HasConditions( bits_COND_CLIENT_PUSH ) )
		{
			return GetScheduleOfType( SCHED_MOVE_AWAY );
		}
	}
	
	return CRCAllyMonster::GetSchedule();
}

MONSTERSTATE CVortigaunt :: GetIdealState ( void )
{
	return CRCAllyMonster::GetIdealState();
}

void CVortigaunt::DeclineFollowing( void )
{
	PlaySentence( "VO_POK", 2, VOL_NORM, ATTN_NORM );
}


//=========================================================
// ArmBeam - small beam from arm to nearby geometry
//=========================================================

void CVortigaunt :: ArmBeam( int side )
{
	TraceResult tr;
	float flDist = 1.0;
	
	if (m_iBeams >= VORTIGAUNT_MAX_BEAMS)
		return;

	UTIL_MakeAimVectors( pev->angles );
	Vector vecSrc = pev->origin + gpGlobals->v_up * 36 + gpGlobals->v_right * side * 16 + gpGlobals->v_forward * 32;

	for (int i = 0; i < 3; i++)
	{
		Vector vecAim = gpGlobals->v_right * side * RANDOM_FLOAT( 0, 1 ) + gpGlobals->v_up * RANDOM_FLOAT( -1, 1 );
		TraceResult tr1;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAim * 512, dont_ignore_monsters, ENT( pev ), &tr1);
		if (flDist > tr1.flFraction)
		{
			tr = tr1;
			flDist = tr.flFraction;
		}
	}

	// Couldn't find anything close enough
	if ( flDist == 1.0 )
		return;

	DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );

	m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );
	if (!m_pBeam[m_iBeams])
		return;

	m_pBeam[m_iBeams]->PointEntInit( tr.vecEndPos, entindex( ) );
	m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 1 );
	m_pBeam[m_iBeams]->SetColor( 96, 128, 16 );
	m_pBeam[m_iBeams]->SetBrightness( 64 );
	m_pBeam[m_iBeams]->SetNoise( 80 );
	m_iBeams++;
}


//=========================================================
// BeamGlow - brighten all beams
//=========================================================
void CVortigaunt :: BeamGlow( )
{
	int b = m_iBeams * 32;
	if (b > 255)
		b = 255;

	for (int i = 0; i < m_iBeams; i++)
	{
		if (m_pBeam[i]->GetBrightness() != 255) 
		{
			m_pBeam[i]->SetBrightness( b );
		}
	}
}


//=========================================================
// WackBeam - regenerate dead colleagues
//=========================================================
void CVortigaunt :: WackBeam( int side, CBaseEntity *pEntity )
{
	Vector vecDest;
	float flDist = 1.0;
	
	if (m_iBeams >= VORTIGAUNT_MAX_BEAMS)
		return;

	if (pEntity == NULL)
		return;

	m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );
	if (!m_pBeam[m_iBeams])
		return;

	m_pBeam[m_iBeams]->PointEntInit( pEntity->Center(), entindex( ) );
	m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 1 );
	m_pBeam[m_iBeams]->SetColor( 180, 255, 96 );
	m_pBeam[m_iBeams]->SetBrightness( 255 );
	m_pBeam[m_iBeams]->SetNoise( 80 );
	m_iBeams++;
}

//=========================================================
// ZapBeam - heavy damage directly forward
//=========================================================
void CVortigaunt :: ZapBeam( int side )
{
	Vector vecSrc, vecAim;
	TraceResult tr;
	CBaseEntity *pEntity;

	if (m_iBeams >= VORTIGAUNT_MAX_BEAMS)
		return;

	vecSrc = pev->origin + gpGlobals->v_up * 36;
	vecAim = ShootAtEnemy( vecSrc );
	float deflection = 0.01;
	vecAim = vecAim + side * gpGlobals->v_right * RANDOM_FLOAT( 0, deflection ) + gpGlobals->v_up * RANDOM_FLOAT( -deflection, deflection );
	UTIL_TraceLine ( vecSrc, vecSrc + vecAim * 1024, dont_ignore_monsters, ENT( pev ), &tr);

	m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 50 );
	if (!m_pBeam[m_iBeams])
		return;

	m_pBeam[m_iBeams]->PointEntInit( tr.vecEndPos, entindex( ) );
	m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 1 );
	m_pBeam[m_iBeams]->SetColor( 180, 255, 96 );
	m_pBeam[m_iBeams]->SetBrightness( 255 );
	m_pBeam[m_iBeams]->SetNoise( 20 );
	m_iBeams++;

	pEntity = CBaseEntity::Instance(tr.pHit);
	if (pEntity != NULL && pEntity->pev->takedamage)
	{
		pEntity->TraceAttack( pev, gSkillData.vortigauntDmgZap, vecAim, &tr, DMG_SHOCK );
	}
	UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );
}


//=========================================================
// ClearBeams - remove all beams
//=========================================================
void CVortigaunt :: ClearBeams( )
{
	for (int i = 0; i < VORTIGAUNT_MAX_BEAMS; i++)
	{
		if (m_pBeam[i])
		{
			UTIL_Remove( m_pBeam[i] );
			m_pBeam[i] = NULL;
		}
	}
	m_iBeams = 0;
	pev->skin = 0;

	STOP_SOUND( ENT(pev), CHAN_WEAPON, "debris/zap4.wav" );
}
