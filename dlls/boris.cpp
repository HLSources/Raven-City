//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//
//=========================================================
// Abilities : 
// - Extremely powerful shotgun.
// - Squad automatic weapon.
// - High energy kick. - Used if the enemy gets too close
// - Energy beam attack. - Used when out of ammo, or against headcrabs
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"animation.h"
#include	"protagonist.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"
#include	"customentity.h"

#define		BORIS_SHOTGUN_CLIP_SIZE				4 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define		BORIS_M249_CLIP_SIZE				100 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		BORIS_AE_GRENADE	( 2 )
#define		BORIS_AE_SHOOT		( 3 )
#define		BORIS_AE_BURST		( 4 )
#define		BORIS_AE_RELOAD		( 5 )
#define		BORIS_AE_KICK		( 6 )
#define		BORIS_AE_ZAPCHARGE	( 7 )
#define		BORIS_AE_ZAPSHOOT	( 8 )
#define		BORIS_AE_ZAPDONE	( 9 )

#define		BORIS_WEAPON_SHOTGUN	1
#define		BORIS_WEAPON_M249		2

#define	GUN_GROUP	2
#define	GUN_HOLSTER	0
#define GUN_SHOTGUN	1
#define GUN_M249	2

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_BORIS_CHECK_FIRE = LAST_TALKMONSTER_TASK + 1,
};
//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_BORIS_NOFIRE	( bits_COND_SPECIAL4 )
//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_BORIS_COVER_AND_RELOAD = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_BORIS_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_BORIS_SWEEP,
	SCHED_BORIS_WAIT_FACE_ENEMY,
	SCHED_BORIS_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_BORIS_ELOF_FAIL,
	SCHED_BORIS_SUPPRESS,
};

#define		BORIS_LIMP_HEALTH	75
#define		BORIS_MAX_BEAMS		32
class CBoris : public CProtagonistMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void CheckAmmo ( void );
	void Shotgun( void );
	void M249( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void KeyValue( KeyValueData *pkvd );
	void StartTask( Task_t *pTask );
	void PrescheduleThink ( void );
	BOOL FCanCheckAttacks ( void );
	virtual int	ObjectCaps( void ) { return CProtagonistMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	void DeclineFollowing( void );
	void SetActivity ( Activity NewActivity );
	// Override these to set behavior
	CBaseEntity	*Kick( void );
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );


	int IRelationship ( CBaseEntity *pTarget );
	void DeathSound( void );
	void PainSound( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	Vector GetGunPosition( void );
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

	CBeam *m_pBeam[BORIS_MAX_BEAMS];

	float	m_flLinkToggle;// how much pain has the player inflicted on me?

	int m_iBeams;
	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;
	BOOL	m_fFastBoris;
	int		m_iShotgunShell;
	int		m_iM249Shell;
	int		m_iM249Link;
	int		m_iBodyGibs;
	int		m_iExplode;
	int		m_iSpriteTexture;
	int		m_cClipSize;
	int		m_iMetalGib;

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_boris, CBoris );

TYPEDESCRIPTION	CBoris::m_SaveData[] = 
{
	DEFINE_ARRAY( CBoris, m_pBeam, FIELD_CLASSPTR, BORIS_MAX_BEAMS ),
	DEFINE_FIELD( CBoris, m_iBeams, FIELD_INTEGER ),
	DEFINE_FIELD( CBoris, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CBoris, m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( CBoris, m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBoris, m_fFastBoris, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBoris, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CBoris, m_iMetalGib, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBoris, CProtagonistMonster );

//=========================================================
// KeyValue
//
// !!! netname entvar field is used in squadmonster for groupname!!!
//=========================================================
void CBoris :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "fastboris"))
	{
		m_fFastBoris = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseMonster::KeyValue( pkvd );
	}
}

//=========================================================
//=========================================================
int CBoris::IRelationship ( CBaseEntity *pTarget )
{
	if ( FClassnameIs( pTarget->pev, "monster_harrison" ) )
	{
		return R_NM;
	}

	return CProtagonistMonster::IRelationship( pTarget );
}
//=========================================================
//=========================================================
CBaseEntity *CBoris :: Kick( void )
{
	TraceResult tr;
	// blast circle
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER ); // This acts like 1000 joules of energy were released.
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z);
		WRITE_COORD( pev->origin.x);
		WRITE_COORD( pev->origin.y);
		WRITE_COORD( pev->origin.z + 200 ); // reach damage radius over .2 seconds
		WRITE_SHORT( m_iSpriteTexture );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 10 ); // life
		WRITE_BYTE( 64 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 24 );   // r, g, b
		WRITE_BYTE( 121 );   // r, g, b
		WRITE_BYTE( 239 );   // r, g, b
		WRITE_BYTE( 128 ); // brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_TELEPORT );		// This makes a Quake 1 teleport effect.This acts like the source of the cylinder and disk.
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_LAVASPLASH );		// This makes a Quake 1 lavasplash, becausecause this is what the cylinder leaves behind
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_LAVASPLASH );		// This makes a Quake 1 lavasplash, because this is what the cylinder leaves behind
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
	MESSAGE_END();
	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 70);
	UTIL_ScreenShake( pev->origin, 12.0, 100.0, 2.0, 1000 );
	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}
	return NULL;
}
//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlBorisFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slBorisFollow[] =
{
	{
		tlBorisFollow,
		ARRAYSIZE ( tlBorisFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};
//=========================================================
// Boris reload schedule
//=========================================================
Task_t	tlBorisHideReload[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RELOAD			},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_RELOAD			},
};

Schedule_t slBorisHideReload[] = 
{
	{
		tlBorisHideReload,
		ARRAYSIZE ( tlBorisHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"BorisHideReload"
	}
};
//=========================================================
// Victory dance!
//=========================================================
Task_t	tlBorisVictoryDance[] =
{
	{ TASK_STOP_MOVING,						(float)0					},
	{ TASK_FACE_ENEMY,						(float)0					},
	{ TASK_WAIT,							(float)1.5					},
	{ TASK_GET_PATH_TO_ENEMY_CORPSE,		(float)0					},
	{ TASK_WALK_PATH,						(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,				(float)0					},
	{ TASK_FACE_ENEMY,						(float)0					},
	{ TASK_PLAY_SEQUENCE,					(float)ACT_VICTORY_DANCE	},
};

Schedule_t	slBorisVictoryDance[] =
{
	{ 
		tlBorisVictoryDance,
		ARRAYSIZE ( tlBorisVictoryDance ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE,
		0,
		"BorisVictoryDance"
	},
};

Task_t	tlBorisFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slBorisFaceTarget[] =
{
	{
		tlBorisFaceTarget,
		ARRAYSIZE ( tlBorisFaceTarget ),
		bits_COND_CLIENT_PUSH	|
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};


Task_t	tlBorisIdleStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_PRT_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slBorisIdleStand[] =
{
	{ 
		tlBorisIdleStand,
		ARRAYSIZE ( tlBorisIdleStand ), 
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
// BorisFail
//=========================================================
Task_t	tlBorisFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slBorisFail[] =
{
	{
		tlBorisFail,
		ARRAYSIZE ( tlBorisFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"Boris Fail"
	},
};

//=========================================================
// Boris Combat Fail
//=========================================================
Task_t	tlBorisCombatFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY,		(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slBorisCombatFail[] =
{
	{
		tlBorisCombatFail,
		ARRAYSIZE ( tlBorisCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Boris Combat Fail"
	},
};
//=========================================================
// Establish line of fire - move to a position that allows
// the Boris to attack.
//=========================================================
Task_t tlBorisEstablishLineOfFire[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_BORIS_ELOF_FAIL	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0						},
	{ TASK_RUN_PATH,			(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slBorisEstablishLineOfFire[] =
{
	{ 
		tlBorisEstablishLineOfFire,
		ARRAYSIZE ( tlBorisEstablishLineOfFire ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"BorisEstablishLineOfFire"
	},
};
//=========================================================
// BorisCombatFace Schedule
//=========================================================
Task_t	tlBorisCombatFace1[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_WAIT,					(float)1.5					},
	{ TASK_SET_SCHEDULE,			(float)SCHED_BORIS_SWEEP	},
};

Schedule_t	slBorisCombatFace[] =
{
	{ 
		tlBorisCombatFace1,
		ARRAYSIZE ( tlBorisCombatFace1 ), 
		bits_COND_NEW_ENEMY				|
		bits_COND_ENEMY_DEAD			|
		bits_COND_CAN_RANGE_ATTACK1		|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Combat Face"
	},
};
//=========================================================
// Suppressing fire - don't stop shooting until the clip is
// empty or Boris gets hurt.
//=========================================================
Task_t	tlBorisSuppress[] =
{
	{ TASK_STOP_MOVING,			0							},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_BORIS_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_BORIS_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_BORIS_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_BORIS_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_BORIS_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
};

Schedule_t	slBorisSuppress[] =
{
	{ 
		tlBorisSuppress,
		ARRAYSIZE ( tlBorisSuppress ), 
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND		|
		bits_COND_BORIS_NOFIRE		|
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"Suppress"
	},
};
//=========================================================
// Boris wait in cover - we don't allow danger or the ability
// to attack to break a Boris's run to cover schedule, but
// when a Boris is in cover, we do want them to attack if they can.
//=========================================================
Task_t	tlBorisWaitInCover[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_WAIT_FACE_ENEMY,			(float)1					},
};

Schedule_t	slBorisWaitInCover[] =
{
	{ 
		tlBorisWaitInCover,
		ARRAYSIZE ( tlBorisWaitInCover ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2,

		bits_SOUND_DANGER,
		"BorisWaitInCover"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t	tlBorisTakeCover1[] =
{
	{ TASK_STOP_MOVING,				(float)0							},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_BORIS_TAKECOVER_FAILED	},
	{ TASK_WAIT,					(float)0.2							},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0							},
	{ TASK_RUN_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER			},
	{ TASK_SET_SCHEDULE,			(float)SCHED_BORIS_WAIT_FACE_ENEMY	},
};

Schedule_t	slBorisTakeCover[] =
{
	{ 
		tlBorisTakeCover1,
		ARRAYSIZE ( tlBorisTakeCover1 ), 
		0,
		0,
		"TakeCover"
	},
};
//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t	tlBorisTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_COWER			},// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slBorisTakeCoverFromBestSound[] =
{
	{ 
		tlBorisTakeCoverFromBestSound,
		ARRAYSIZE ( tlBorisTakeCoverFromBestSound ), 
		0,
		0,
		"BorisTakeCoverFromBestSound"
	},
};
//=========================================================
// Do a turning sweep of the area
//=========================================================
Task_t	tlBorisSweep[] =
{
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
};

Schedule_t	slBorisSweep[] =
{
	{ 
		tlBorisSweep,
		ARRAYSIZE ( tlBorisSweep ), 
		
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_WORLD		|// sound flags
		bits_SOUND_DANGER		|
		bits_SOUND_PLAYER,

		"Boris Sweep"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// Boris's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlBorisRangeAttack1[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_IDLE_ANGRY  },
	{ TASK_BORIS_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_BORIS_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_BORIS_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_BORIS_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
};

Schedule_t	slBorisRangeAttack1[] =
{
	{ 
		tlBorisRangeAttack1,
		ARRAYSIZE ( tlBorisRangeAttack1 ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED	|
		bits_COND_BORIS_NOFIRE		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"Range Attack1"
	},
};
DEFINE_CUSTOM_SCHEDULES( CBoris )
{
	slBorisFollow,
	slBorisFaceTarget,
	slBorisIdleStand,
	slBorisHideReload,
	slBorisVictoryDance,
	slBorisFail,
	slBorisCombatFail,
	slBorisEstablishLineOfFire,
	slBorisCombatFace,
	slBorisSuppress,
	slBorisWaitInCover,
	slBorisTakeCover,
	slBorisTakeCoverFromBestSound,
	slBorisSweep,
	slBorisRangeAttack1,
};


IMPLEMENT_CUSTOM_SCHEDULES( CBoris, CProtagonistMonster );

void CBoris :: StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_BORIS_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_BORIS_NOFIRE );
		}
		TaskComplete();
		break;
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// boris no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CProtagonistMonster ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;

	default: 
		CProtagonistMonster :: StartTask( pTask );
		break;
	}
}
//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CBoris :: PrescheduleThink ( void )
{
	if ( InSquad() && m_hEnemy != NULL )
	{
		if ( HasConditions ( bits_COND_SEE_ENEMY ) )
		{
			// update the squad's last enemy sighting time.
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
		}
		else
		{
			if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
			{
				// been a while since we've seen the enemy
				MySquadLeader()->m_fEnemyEluded = TRUE;
			}
		}
	}
	CBaseMonster :: PrescheduleThink();
}
//=========================================================
// FCanCheckAttacks - this is overridden for human Boris
// because they can throw/shoot grenades when they can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
BOOL CBoris :: FCanCheckAttacks ( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CBoris :: ISoundMask ( void) 
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
int	CBoris :: Classify ( void )
{
	if ( m_fFastBoris )
		return CLASS_NONE;
	else
		return	CLASS_PLAYER_ALLY;
}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBoris :: SetYawSpeed ( void )
{
	int ys;

	ys = 0;

	switch ( m_Activity )
	{
	case ACT_IDLE:		
		ys = 70;
		break;
	case ACT_WALK:
		ys = 70;
		break;
	case ACT_RUN:
		ys = 90;
		break;
	default:
		ys = 70;
		break;
	}

	pev->yaw_speed = ys;
}
//=========================================================
// SetActivity 
//=========================================================
void CBoris :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// Boris is either shooting standing or shooting crouched
		if (FBitSet( pev->weapons, BORIS_WEAPON_SHOTGUN))
		{
			iSequence = LookupSequence( "shoot_shotgun" );
		}
		else
		{
			// get crouching shoot
			iSequence = LookupSequence( "shoot_m249" );
		}
		break;
	case ACT_RUN:
		if ( m_fFastBoris )
		{
			iSequence = LookupSequence( "city07_run" );
		}
		else
		{
			if ( pev->health <= BORIS_LIMP_HEALTH )
			{
				// limp!
				iSequence = LookupActivity ( ACT_RUN_HURT );
			}
			else
			{
				iSequence = LookupActivity ( NewActivity );
			}
		}
		break;
	case ACT_WALK:
		if ( pev->health <= BORIS_LIMP_HEALTH )
		{
			// limp!
			iSequence = LookupActivity ( ACT_WALK_HURT );
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	case ACT_IDLE:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
		{
			NewActivity = ACT_IDLE_ANGRY;
		}
		iSequence = LookupActivity ( NewActivity );
		break;
	default:
		iSequence = LookupActivity ( NewActivity );
		break;
	}
	
	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}
//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CBoris :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() )
	{
		TraceResult	tr;

		if ( !m_hEnemy->IsPlayer() && flDist <= 64 )
		{
			// kick nonclients, but don't shoot at them.
			return FALSE;
		}

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, ENT(pev), &tr);

		if ( tr.flFraction == 1.0 )
		{
			return TRUE;
		}
	}

	return FALSE;
}

//=========================================================
// CheckRangeAttack2 - beam attack 
//=========================================================
BOOL CBoris :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() )
	{
		TraceResult	tr;

		if ( !m_hEnemy->IsPlayer() && flDist <= 64 )
		{
			// kick nonclients, but don't shoot at them.
			return FALSE;
		}

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, ENT(pev), &tr);

		if ( tr.flFraction == 1.0 )
		{
			return TRUE;
		}
	}

	return FALSE;
}
//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CBoris :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	CBaseMonster *pEnemy;

	if ( m_hEnemy != NULL )
	{
		pEnemy = m_hEnemy->MyMonsterPointer();

		if ( !pEnemy )
		{
			return FALSE;
		}
	}

	if ( flDist <= 64 && flDot >= 0.7	&& 
		 pEnemy->Classify() != CLASS_ALIEN_BIOWEAPON &&
		 pEnemy->Classify() != CLASS_PLAYER_BIOWEAPON )
	{
		return TRUE;
	}
	return FALSE;
}
//=========================================================
// Shotgun - Boris' main weapon
//=========================================================
void CBoris :: Shotgun ( void )
{
	if (m_hEnemy == NULL)
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);

	for ( int i = 0; i < 2; i++ )
		EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iShotgunShell, TE_BOUNCE_SHOTSHELL); 


	FireBullets(gSkillData.borisShotgunPellets, vecShootOrigin, vecShootDir, VECTOR_CONE_6DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0 ); // shoot +-7.5 degrees

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/dbarrel1.wav", 1, ATTN_NORM );

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

	m_cAmmoLoaded--;// take away a bullet!

	m_flNextAttack = gpGlobals->time + ( 35/27 );
}
//=========================================================
// Shoot
//=========================================================
void CBoris :: M249 ( void )
{
	if ( m_hEnemy == NULL )
	{
		return;
	}

	switch ( RANDOM_LONG(0,2) )
	{
		case 0: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/saw_fire1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/saw_fire2.wav", 1, ATTN_NORM ); break;
		case 2: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/saw_fire3.wav", 1, ATTN_NORM ); break;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);

	m_flLinkToggle = !m_flLinkToggle;

	if (!m_flLinkToggle)
		EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iM249Shell, TE_BOUNCE_SHELL);
	else
		EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iM249Link, TE_BOUNCE_SHELL);

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_3DEGREES, 2048, BULLET_PLAYER_556 ); // shoot +-5 degrees

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CBoris :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case BORIS_AE_SHOOT:
		Shotgun();
		break;

	case BORIS_AE_BURST:
		M249();
		break;

	case BORIS_AE_KICK:
	{
		CBaseEntity *pHurt = Kick();
		if ( pHurt )
		{
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
					WRITE_BYTE( 24 );		// r
					WRITE_BYTE( 121 );		// g
					WRITE_BYTE( 239 );		// b
					WRITE_BYTE( 20 / pev->framerate );		// time * 10
					WRITE_BYTE( 0 );		// decay * 0.1
				MESSAGE_END( );

			}

			ArmBeam( -1 );
			ArmBeam( 1 );
			ArmBeam( -1 );
			ArmBeam( 1 );
			ArmBeam( -1 );
			ArmBeam( 1 );
			BeamGlow( );
			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "garg/gar_stomp1.wav", 1, ATTN_NORM );
			// SOUND HERE!
			UTIL_MakeVectors( pev->angles );
			pHurt->pev->punchangle.x = 50;
			pHurt->pev->punchangle.y = 50;
			pHurt->pev->punchangle.z = 50;
			pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 500 + gpGlobals->v_up * 500;
			pHurt->TakeDamage( pev, pev, gSkillData.borisDmgKick, DMG_SHOCK );
		}
	}
	break;
	case BORIS_AE_RELOAD:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions(bits_COND_NO_AMMO_LOADED);
	break;
		case BORIS_AE_ZAPCHARGE:
		{
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
					WRITE_BYTE( 24 );		// r
					WRITE_BYTE( 121 );		// g
					WRITE_BYTE( 239 );		// b
					WRITE_BYTE( 20 / pev->framerate );		// time * 10
					WRITE_BYTE( 0 );		// decay * 0.1
				MESSAGE_END( );

			}

			ArmBeam( -1 );
			ArmBeam( 1 );
			ArmBeam( -1 );
			ArmBeam( 1 );
			ArmBeam( -1 );
			ArmBeam( 1 );
			BeamGlow( );

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "debris/beamstart15.wav", 1, ATTN_NORM, 0, 100 + (m_iBeams /3) * 10 );
		}
		break;

		case BORIS_AE_ZAPSHOOT:
		{
			ClearBeams( );

			ClearMultiDamage();

			UTIL_MakeAimVectors( pev->angles );

			ZapBeam( -1 );
			ZapBeam( 1 );

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "debris/beamstart4.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 130, 160 ) );
			ApplyMultiDamage(pev, pev);

			m_flNextAttack = gpGlobals->time + RANDOM_FLOAT( 0.5, 4.0 );
		}
		break;

		case BORIS_AE_ZAPDONE:
		{
			ClearBeams( );
		}
		break;
	default:
		CProtagonistMonster::HandleAnimEvent( pEvent );
	}
}
//=========================================================
// GetGunPosition	return the end of the barrel
//=========================================================
Vector CBoris :: GetGunPosition( )
{
		return pev->origin + Vector( 0, 0, 60 );
}
//=========================================================
// Spawn
//=========================================================
void CBoris :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/boris.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= 0;
	pev->health			= gSkillData.borisHealth;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	if ( pev->weapons == 0 )
	{
		pev->weapons = RANDOM_LONG(1,2);
	}

	if ( pev->weapons == BORIS_WEAPON_SHOTGUN )
	{
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_cClipSize	= BORIS_SHOTGUN_CLIP_SIZE;
	}
	if ( pev->weapons == BORIS_WEAPON_M249 )
	{
		SetBodygroup( GUN_GROUP, GUN_M249 );
		m_cClipSize	= BORIS_M249_CLIP_SIZE;
	}

	m_cAmmoLoaded		= m_cClipSize;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_SQUAD | bits_CAP_MELEE_ATTACK1;
	m_fEnemyEluded		= FALSE;

	MonsterInit();
	StartMonster();
	SetUse( FollowerUse );
}
//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBoris :: Precache()
{
	PRECACHE_MODEL("models/boris.mdl");

	PRECACHE_SOUND("weapons/dbarrel1.wav" );

	PRECACHE_SOUND("boris/bo_pain1.wav");
	PRECACHE_SOUND("boris/bo_pain2.wav");
	PRECACHE_SOUND("boris/bo_pain3.wav");

	PRECACHE_SOUND("boris/bo_die1.wav");
	PRECACHE_SOUND("boris/bo_die2.wav");
	PRECACHE_SOUND("boris/bo_die3.wav");

	PRECACHE_SOUND("player/pl_jump1.wav");

	PRECACHE_SOUND( "debris/beamstart15.wav" );
	PRECACHE_SOUND( "hgrunt/gr_reload1.wav" );
	PRECACHE_SOUND( "weapons/xbow_hit1.wav" );
	PRECACHE_SOUND( "zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event
	PRECACHE_SOUND( "garg/gar_stomp1.wav" );

	PRECACHE_MODEL( "models/metalplategibs_green.mdl" );

	PRECACHE_MODEL("sprites/lgtning.spr");

	PRECACHE_SOUND("debris/zap4.wav");
	PRECACHE_SOUND("debris/zap1.wav");
	PRECACHE_SOUND("debris/beamstart4.wav");

	PRECACHE_SOUND("weapons/scock1.wav");

	PRECACHE_SOUND("weapons/saw_fire1.wav" );
	PRECACHE_SOUND("weapons/saw_fire2.wav" );
	PRECACHE_SOUND("weapons/saw_fire3.wav" );

	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("hassault/hw_shoot1.wav");

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );
	m_iMetalGib = PRECACHE_MODEL( "models/metalplategibs_green.mdl" );

	PRECACHE_MODEL("sprites/lgtning.spr");

	m_iExplode	= PRECACHE_MODEL( "sprites/fexplo.spr" );
	m_iBodyGibs = PRECACHE_MODEL( "models/metalplategibs_green.mdl" );
	m_iShotgunShell = PRECACHE_MODEL ("models/shotgunshell.mdl");
	m_iM249Shell = PRECACHE_MODEL ("models/saw_shell.mdl");// saw shell
	m_iM249Link = PRECACHE_MODEL ("models/saw_link.mdl");// saw link
	// every new boris must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();
	CProtagonistMonster::Precache();
}	

// Init talk data
void CBoris :: TalkInit()
{
	
	CProtagonistMonster::TalkInit();

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER]  =	"BO_ANSWER";
	m_szGrp[TLK_QUESTION] =	"BO_QUESTION";
	m_szGrp[TLK_IDLE] =		"BO_IDLE";
	m_szGrp[TLK_STARE] =		"BO_STARE";
	m_szGrp[TLK_USE] =		"BO_OK";
	m_szGrp[TLK_UNUSE] =	"BO_WAIT";
	m_szGrp[TLK_STOP] =		"BO_STOP";

	m_szGrp[TLK_NOSHOOT] =	"BO_SCARED";
	m_szGrp[TLK_HELLO] =	"BO_HELLO";

	m_szGrp[TLK_PLHURT1] =	"!BO_CUREA";
	m_szGrp[TLK_PLHURT2] =	"!BO_CUREB"; 
	m_szGrp[TLK_PLHURT3] =	"!BO_CUREC";

	m_szGrp[TLK_PHELLO] =	NULL;	//"BO_PHELLO";		// UNDONE
	m_szGrp[TLK_PIDLE] =	NULL;	//"BO_PIDLE";			// UNDONE
	m_szGrp[TLK_PQUESTION] = "BO_PQUEST";		// UNDONE

	m_szGrp[TLK_SMELL] =	"BO_SMELL";
	
	m_szGrp[TLK_WOUND] =	"BO_WOUND";
	m_szGrp[TLK_MORTAL] =	"BO_MORTAL";

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


int CBoris :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if ( m_fFastBoris )
		flDamage = 0;

	if ( pevAttacker->flags & FL_CLIENT)
		return 0;

	Forget( bits_MEMORY_INCOVER );
	// make sure friends talk about it if player hurts talkmonsters...
	int ret = CProtagonistMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	if ( m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT) )
	{

		// This is a heurstic to determine if the player intended to harm me
		// If I have an enemy, we can't establish intent (may just be crossfire)
		if ( m_hEnemy == NULL )
		{
			// If the player was facing directly at me, or I'm already suspicious, get mad
			if ( (m_afMemory & bits_MEMORY_SUSPICIOUS) || IsFacing( pevAttacker, pev->origin ) )
			{
				// Alright, now I'm pissed!
				PlaySentence( "BO_MAD", 4, VOL_NORM, ATTN_NORM );

				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				// Hey, be careful with that
				PlaySentence( "BO_SHOT", 4, VOL_NORM, ATTN_NORM );
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
		{
			PlaySentence( "BO_SHOT", 4, VOL_NORM, ATTN_NORM );
		}
	}

	return ret;
}
//=========================================================
// CheckAmmo - overridden for the Boris because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CBoris :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	{
		SetConditions(bits_COND_NO_AMMO_LOADED);
	}
}
//=========================================================
// PainSound
//=========================================================
void CBoris :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,2))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "boris/bo_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "boris/bo_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "boris/bo_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CBoris :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,2))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "boris/bo_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "boris/bo_die2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "boris/bo_die3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}


void CBoris::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( pevAttacker->flags & FL_CLIENT)
		return;

	UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
	pev->dmgtime = gpGlobals->time;

	TraceResult tr;

	Vector vecBlood = (ptr->vecEndPos - pev->origin).Normalize( );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
		WRITE_BYTE ( TE_GUNSHOT );
		WRITE_COORD( ptr->vecEndPos.x );
		WRITE_COORD( ptr->vecEndPos.y );
		WRITE_COORD( ptr->vecEndPos.z );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
		WRITE_BYTE( TE_STREAK_SPLASH );
		WRITE_COORD( ptr->vecEndPos.x );		// origin
		WRITE_COORD( ptr->vecEndPos.y );
		WRITE_COORD( ptr->vecEndPos.z );
		WRITE_COORD( ptr->vecPlaneNormal.x );	// direction
		WRITE_COORD( ptr->vecPlaneNormal.y );
		WRITE_COORD( ptr->vecPlaneNormal.z );
		WRITE_BYTE( 0 );	// Streak color 6
		WRITE_SHORT( 3 );	// count
		WRITE_SHORT( 25 );
		WRITE_SHORT( 50 );	// Random velocity modifier
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));
	UTIL_BloodStream( ptr->vecEndPos, vecBlood, 6, 100);
	CProtagonistMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


void CBoris::Killed( entvars_t *pevAttacker, int iGib )
{
	ClearBeams( );

	if ( GetBodygroup( GUN_GROUP ) != GUN_HOLSTER )
	{// drop the gun!
		Vector vecGunPos;
		Vector vecGunAngles;

		SetBodygroup( GUN_GROUP , GUN_HOLSTER );

		GetAttachment( 0, vecGunPos, vecGunAngles );

	if ( pev->weapons == BORIS_WEAPON_SHOTGUN )
		CBaseEntity *pGun = DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
	}

	SetUse( NULL );	
	CProtagonistMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CBoris :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	case SCHED_TARGET_FACE:
		{
			// call base class default so that Boris will talk
			// when 'used' 
			psched = CProtagonistMonster::GetScheduleOfType(Type);

			if (psched == slIdleStand)
				return slBorisFaceTarget;	// override this for different target face behavior
			else
				return psched;
		}
	case SCHED_BORIS_COVER_AND_RELOAD:
		{
			return &slBorisHideReload[ 0 ];
		}
	case SCHED_TARGET_CHASE:
		return slBorisFollow;

	case SCHED_IDLE_STAND:
	{
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CProtagonistMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
		{
			// just look straight ahead.
			return slBorisIdleStand;
		}
		else
			return psched;	
	}
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			return &slBorisTakeCover[ 0 ];
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slBorisTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_BORIS_TAKECOVER_FAILED:
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}
			else
			{
				return GetScheduleOfType ( SCHED_FAIL );
			}
		}
		break;
	case SCHED_BORIS_ELOF_FAIL:
		{
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}
	case SCHED_BORIS_ESTABLISH_LINE_OF_FIRE:
		{
			return &slBorisEstablishLineOfFire[ 0 ];
		}
	case SCHED_RANGE_ATTACK1:
		{
			return &slBorisRangeAttack1[ 0 ];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slBorisCombatFace[ 0 ];
		}
	case SCHED_BORIS_WAIT_FACE_ENEMY:
		{
			return &slBorisWaitInCover[ 0 ];
		}
	case SCHED_BORIS_SWEEP:
		{
			return &slBorisSweep[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( InSquad() )
			{
				if ( !IsLeader() )
				{
					return &slBorisFail[ 0 ];
				}
			}
			if ( IsFollowing() )
			{
				return &slBorisFail[ 0 ];
			}

			return &slBorisVictoryDance[ 0 ];
		}
	case SCHED_BORIS_SUPPRESS:
		{
			return &slBorisSuppress[ 0 ];
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// Boris has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slBorisCombatFail[ 0 ];
			}

			return &slBorisFail[ 0 ];
		}
		}
	return CProtagonistMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CBoris :: GetSchedule ( void )
{
	ClearBeams( );
	// Maria places HIGH priority on running away from danger sounds.
	if ( HasConditions(bits_COND_HEAR_SOUND) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound)
		{
			if (pSound->m_iType & bits_SOUND_DANGER)
			{
				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz( ENT(pev), "BO_GREN", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
		}
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		PlaySentence( "BO_KILL", 4, VOL_NORM, ATTN_NORM );
	}

	switch( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				if ( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if ( !IsLeader() )
					{
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_BORIS_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_BORIS_ESTABLISH_LINE_OF_FIRE );
						}
					}
					else 
					{
						if (FOkToSpeak())// && RANDOM_LONG(0,1))
						{
							if (m_hEnemy != NULL)
							{
								if ( !m_hEnemy->IsPlayer() ) 
								{
									if ( FClassnameIs( m_hEnemy->pev, "monster_zombie" ) || FClassnameIs( m_hEnemy->pev, "monster_zombie_barney" ) || FClassnameIs( m_hEnemy->pev, "monster_zombie_soldier" ) )
										SENTENCEG_PlayRndSz( ENT(pev), "BO_ZOMBIE", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
									else if ( FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
										SENTENCEG_PlayRndSz( ENT(pev), "BO_HEADCRAB", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
									else
										SENTENCEG_PlayRndSz( ENT(pev), "BO_ATTACK", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
								}
								else
								{
									SENTENCEG_PlayRndSz( ENT(pev), "BO_MAD", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
								}
							}
						}
						
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_BORIS_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_BORIS_ESTABLISH_LINE_OF_FIRE );
						}
					}
				}
			}
// damaged just a little
			else if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{

				int iPercent = RANDOM_LONG(0,99);

				if ( iPercent <= 90 && m_hEnemy != NULL )
				{
					if (FOkToSpeak()) // && RANDOM_LONG(0,1))
					{
						SENTENCEG_PlayRndSz( ENT(pev), "BO_COVER", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
					}
					// only try to take cover if we actually have an enemy!

					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				else
				{
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
				}
			}
			else if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
			}
// can shoot
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) && !HasConditions ( bits_COND_NO_AMMO_LOADED ) && ( m_hEnemy->Classify() != CLASS_ALIEN_PREY ) ) // Headcrabs are a waste of ammo, zap them
			{
				if ( OccupySlot ( bits_SLOTS_PROTAGONIST_ENGAGE ) )
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
// can't see enemy
			else if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
			{
				if ( OccupySlot( bits_SLOTS_PROTAGONIST_ENGAGE ) )
				{
					return GetScheduleOfType( SCHED_BORIS_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					if (FOkToSpeak() && RANDOM_LONG(0,1))
					{
						SENTENCEG_PlayRndSz( ENT(pev), "BO_TAUNT", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
					}
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}
			// no ammo
			else if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				PlaySentence( "BO_RELOAD", 2, VOL_NORM, ATTN_NORM );
				return GetScheduleOfType ( SCHED_BORIS_COVER_AND_RELOAD );
			}
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) )// Zap the guy if we are out of ammo, it's powerfull enough.
			{
				// zap the enemy
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
			if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_BORIS_ESTABLISH_LINE_OF_FIRE );
			}
		}
		break;

	case MONSTERSTATE_ALERT:	
	case MONSTERSTATE_IDLE:
		if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
		{
			return GetScheduleOfType ( SCHED_RELOAD );
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

		// try to say something about smells
		TrySmellTalk();
		break;
	}
	
	return CProtagonistMonster::GetSchedule();
}

MONSTERSTATE CBoris :: GetIdealState ( void )
{
	return CProtagonistMonster::GetIdealState();
}



void CBoris::DeclineFollowing( void )
{
	PlaySentence( "BO_POK", 2, VOL_NORM, ATTN_NORM );
}
//=========================================================
// ArmBeam - small beam from arm to nearby geometry
//=========================================================

void CBoris :: ArmBeam( int side )
{
	TraceResult tr;
	float flDist = 1.0;
	
	if (m_iBeams >= BORIS_MAX_BEAMS)
		return;

	UTIL_MakeAimVectors( pev->angles );
	Vector vecSrc = pev->origin + gpGlobals->v_up * 36 + gpGlobals->v_right * side * 16 + gpGlobals->v_forward * 32;

	for (int i = 0; i < 3; i++)
	{
		Vector vecAim = gpGlobals->v_right * side * RANDOM_FLOAT( 0, 1 ) + gpGlobals->v_up * RANDOM_FLOAT( -1, 1 );
		TraceResult tr1;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAim * 2048, dont_ignore_monsters, ENT( pev ), &tr1);
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
	m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 3 );
	m_pBeam[m_iBeams]->SetColor( 24, 121, 239 );
	m_pBeam[m_iBeams]->SetBrightness( 64 );
	m_pBeam[m_iBeams]->SetNoise( 80 );
	m_iBeams++;
}


//=========================================================
// BeamGlow - brighten all beams
//=========================================================
void CBoris :: BeamGlow( )
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
// ZapBeam - heavy damage directly forward
//=========================================================
void CBoris :: ZapBeam( int side )
{
	Vector vecSrc, vecAim;
	TraceResult tr;
	CBaseEntity *pEntity;

	if (m_iBeams >= BORIS_MAX_BEAMS)
		return;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
		WRITE_BYTE( TE_STREAK_SPLASH );
		WRITE_COORD( tr.vecEndPos.x );		// origin
		WRITE_COORD( tr.vecEndPos.y );
		WRITE_COORD( tr.vecEndPos.z );
		WRITE_COORD( tr.vecPlaneNormal.x );	// direction
		WRITE_COORD( tr.vecPlaneNormal.y );
		WRITE_COORD( tr.vecPlaneNormal.z );
		WRITE_BYTE( 3 );	// Streak color 6
		WRITE_SHORT( 120 );	// count
		WRITE_SHORT( 25 );
		WRITE_SHORT( 50 );	// Random velocity modifier
	MESSAGE_END();

	vecSrc = pev->origin + gpGlobals->v_up * 36;
	vecAim = ShootAtEnemy( vecSrc );
	float deflection = 0.01;
	vecAim = vecAim + side * gpGlobals->v_right * RANDOM_FLOAT( 0, deflection ) + gpGlobals->v_up * RANDOM_FLOAT( -deflection, deflection );
	UTIL_TraceLine ( vecSrc, vecSrc + vecAim * 1024, dont_ignore_monsters, ENT( pev ), &tr);

	m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 50 );
	if (!m_pBeam[m_iBeams])
		return;

	m_pBeam[m_iBeams]->PointEntInit( tr.vecEndPos, entindex( ) );
	m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 3 );
	m_pBeam[m_iBeams]->SetColor( 24, 121, 239 );
	m_pBeam[m_iBeams]->SetBrightness( 255 );
	m_pBeam[m_iBeams]->SetNoise( 20 );
	m_iBeams++;

	pEntity = CBaseEntity::Instance(tr.pHit);
	if (pEntity != NULL && pEntity->pev->takedamage)
	{
		pEntity->TraceAttack( pev, gSkillData.borisDmgZap, vecAim, &tr, DMG_ENERGYBEAM );
	}
	UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );
}
//=========================================================
// ClearBeams - remove all beams
//=========================================================
void CBoris :: ClearBeams( )
{
	for (int i = 0; i < BORIS_MAX_BEAMS; i++)
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