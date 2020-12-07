//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//=========================================================
// Maria - By Highlander.
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

#define		MARIA_CLIP_SIZE				6 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
//=========================================================
// Monster's Anim Events Go Here
//=========================================================
// first flag is maria dying for scripted sequences?
#define		MARIA_AE_SHOOT		( 3 )
#define		MARIA_AE_RELOAD		( 5 )
#define		MARIA_AE_SHELL		( 6 )// Drop a few casings

#define		GUN_GROUP	3
#define		GUN_DRAWN	0
#define		GUN_NONE	1

#define		MARIA_LIMP_HEALTH	20

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_MARIA_CHECK_FIRE = LAST_TALKMONSTER_TASK + 1,
};
//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_MARIA_NOFIRE	( bits_COND_SPECIAL4 )
//=========================================================
// monster-specific schedules
//=========================================================
enum
{
	SCHED_MARIA_COVER_AND_RELOAD = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_MARIA_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_MARIA_SWEEP,
	SCHED_MARIA_WAIT_FACE_ENEMY,
	SCHED_MARIA_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_MARIA_ELOF_FAIL,
	SCHED_MARIA_SUPPRESS,
};

class CMaria : public CProtagonistMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void Shoot( void );
	void AlertSound( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunAI( void );
	BOOL FCanCheckAttacks ( void );
	void CheckAmmo ( void );
	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	void PrescheduleThink ( void );
	virtual int	ObjectCaps( void ) { return CProtagonistMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	void SetActivity ( Activity NewActivity );
	void DeclineFollowing( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int IRelationship ( CBaseEntity *pTarget );

	// Override these to set behavior
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );
	
	BOOL FOkToSpeak( void );
	void TalkInit( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;
	int		m_cClipSize;

	Vector GetGunPosition( void );

	float	m_flShieldDamage; // The amount of damage that our shield had recieved.

	float	m_flShieldRechargeTime; // The amount of time it takes to recharge our shield.
	float	m_flShieldTime; // The time before the shield effect disappears.
	float	m_flRechargeStartTime;
	float	m_flRechargeSoundTime;

	BOOL	m_fShieldInActive;

	int		m_iBrassShell;

	// UNDONE: What is this for?  It isn't used?
	float	m_flPlayerDamage;// how much pain has the player inflicted on me?

	CUSTOM_SCHEDULES;
};
//=========================================================
//Overriden for Maria, since she hates Harrison like hell.
//=========================================================
int CMaria::IRelationship ( CBaseEntity *pTarget )
{
	if ( FClassnameIs( pTarget->pev, "monster_harrison" ) )
	{
		return R_NM;
	}

	return CProtagonistMonster::IRelationship( pTarget );
}
//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CMaria :: FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if (gpGlobals->time <= CProtagonistMonster::g_talkWaitTime)
		return FALSE;

	// if in the grip of a barnacle, don't speak
	if ( m_MonsterState == MONSTERSTATE_PRONE || m_IdealMonsterState == MONSTERSTATE_PRONE )
	{
		return FALSE;
	}

	// if not alive, certainly don't speak
	if ( pev->deadflag != DEAD_NO )
	{
		return FALSE;
	}

	if ( pev->spawnflags & SF_MONSTER_GAG )
	{
		if ( m_MonsterState != MONSTERSTATE_COMBAT )
		{
			// no talking outside of combat if gagged.
			return FALSE;
		}
	}
	
	return TRUE;
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

LINK_ENTITY_TO_CLASS( monster_maria, CMaria );
TYPEDESCRIPTION	CMaria::m_SaveData[] = 
{
	DEFINE_FIELD( CMaria, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CMaria, m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( CMaria, m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMaria, m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( CMaria, m_cClipSize, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CMaria, CProtagonistMonster );

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlMariaFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slMariaFollow[] =
{
	{
		tlMariaFollow,
		ARRAYSIZE ( tlMariaFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};

Task_t	tlMariaFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slMariaFaceTarget[] =
{
	{
		tlMariaFaceTarget,
		ARRAYSIZE ( tlMariaFaceTarget ),
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


Task_t	tlMariaIdleStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_PRT_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slMariaIdleStand[] =
{
	{ 
		tlMariaIdleStand,
		ARRAYSIZE ( tlMariaIdleStand ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL			|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|// sound flags - change these, and you'll break the talking code.
		
		bits_SOUND_DANGER		|
		bits_SOUND_MEAT			|// scents
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"IdleStand"
	},
};
//=========================================================
// Maria reload schedule
//=========================================================
Task_t	tlMariaHideReload[] =
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

Schedule_t slMariaHideReload[] = 
{
	{
		tlMariaHideReload,
		ARRAYSIZE ( tlMariaHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"MariaHideReload"
	}
};
//=========================================================
// Victory dance!
//=========================================================
Task_t	tlMariaVictoryDance[] =
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

Schedule_t	slMariaVictoryDance[] =
{
	{ 
		tlMariaVictoryDance,
		ARRAYSIZE ( tlMariaVictoryDance ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE,
		0,
		"MariaVictoryDance"
	},
};
//=========================================================
// MariaFail
//=========================================================
Task_t	tlMariaFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slMariaFail[] =
{
	{
		tlMariaFail,
		ARRAYSIZE ( tlMariaFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"Maria Fail"
	},
};

//=========================================================
// Maria Combat Fail
//=========================================================
Task_t	tlMariaCombatFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY,		(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slMariaCombatFail[] =
{
	{
		tlMariaCombatFail,
		ARRAYSIZE ( tlMariaCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Maria Combat Fail"
	},
};
//=========================================================
// Establish line of fire - move to a position that allows
// the Maria to attack.
//=========================================================
Task_t tlMariaEstablishLineOfFire[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_MARIA_ELOF_FAIL	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0						},
	{ TASK_RUN_PATH,			(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slMariaEstablishLineOfFire[] =
{
	{ 
		tlMariaEstablishLineOfFire,
		ARRAYSIZE ( tlMariaEstablishLineOfFire ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"MariaEstablishLineOfFire"
	},
};
//=========================================================
// MariaCombatFace Schedule
//=========================================================
Task_t	tlMariaCombatFace1[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_WAIT,					(float)1.5					},
	{ TASK_SET_SCHEDULE,			(float)SCHED_MARIA_SWEEP	},
};

Schedule_t	slMariaCombatFace[] =
{
	{ 
		tlMariaCombatFace1,
		ARRAYSIZE ( tlMariaCombatFace1 ), 
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
// empty or Maria gets hurt.
//=========================================================
Task_t	tlMariaSuppress[] =
{
	{ TASK_STOP_MOVING,			0							},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_MARIA_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_MARIA_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_MARIA_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_MARIA_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_MARIA_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
};

Schedule_t	slMariaSuppress[] =
{
	{ 
		tlMariaSuppress,
		ARRAYSIZE ( tlMariaSuppress ), 
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND		|
		bits_COND_MARIA_NOFIRE		|
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"Suppress"
	},
};
//=========================================================
// Maria wait in cover - we don't allow danger or the ability
// to attack to break a Maria's run to cover schedule, but
// when a Maria is in cover, we do want them to attack if they can.
//=========================================================
Task_t	tlMariaWaitInCover[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_WAIT_FACE_ENEMY,			(float)1					},
};

Schedule_t	slMariaWaitInCover[] =
{
	{ 
		tlMariaWaitInCover,
		ARRAYSIZE ( tlMariaWaitInCover ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2,

		bits_SOUND_DANGER,
		"MariaWaitInCover"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t	tlMariaTakeCover1[] =
{
	{ TASK_STOP_MOVING,				(float)0							},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_MARIA_TAKECOVER_FAILED	},
	{ TASK_WAIT,					(float)0.2							},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0							},
	{ TASK_RUN_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER			},
	{ TASK_SET_SCHEDULE,			(float)SCHED_MARIA_WAIT_FACE_ENEMY	},
};

Schedule_t	slMariaTakeCover[] =
{
	{ 
		tlMariaTakeCover1,
		ARRAYSIZE ( tlMariaTakeCover1 ), 
		0,
		0,
		"TakeCover"
	},
};
//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t	tlMariaTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_COWER			},// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slMariaTakeCoverFromBestSound[] =
{
	{ 
		tlMariaTakeCoverFromBestSound,
		ARRAYSIZE ( tlMariaTakeCoverFromBestSound ), 
		0,
		0,
		"MariaTakeCoverFromBestSound"
	},
};
//=========================================================
// Do a turning sweep of the area
//=========================================================
Task_t	tlMariaSweep[] =
{
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
};

Schedule_t	slMariaSweep[] =
{
	{ 
		tlMariaSweep,
		ARRAYSIZE ( tlMariaSweep ), 
		
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_WORLD		|// sound flags
		bits_SOUND_DANGER		|
		bits_SOUND_PLAYER,

		"Maria Sweep"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// Maria's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlMariaRangeAttack1[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_IDLE_ANGRY  },
	{ TASK_MARIA_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_MARIA_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_MARIA_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_MARIA_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
};

Schedule_t	slMariaRangeAttack1[] =
{
	{ 
		tlMariaRangeAttack1,
		ARRAYSIZE ( tlMariaRangeAttack1 ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED	|
		bits_COND_MARIA_NOFIRE		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"Range Attack1"
	},
};
DEFINE_CUSTOM_SCHEDULES( CMaria )
{
	slMariaFollow,
	slMariaFaceTarget,
	slMariaIdleStand,
	slMariaHideReload,
	slMariaVictoryDance,
	slMariaFail,
	slMariaCombatFail,
	slMariaEstablishLineOfFire,
	slMariaCombatFace,
	slMariaSuppress,
	slMariaWaitInCover,
	slMariaTakeCover,
	slMariaTakeCoverFromBestSound,
	slMariaSweep,
	slMariaRangeAttack1,
};

IMPLEMENT_CUSTOM_SCHEDULES( CMaria, CProtagonistMonster );

void CMaria :: StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_MARIA_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_MARIA_NOFIRE );
		}
		TaskComplete();
		break;
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// maria no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );

		CProtagonistMonster ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		pev->framerate = 2.00;
		m_IdealActivity = ACT_RELOAD;
		break;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CProtagonistMonster :: StartTask( pTask );
		if (pev->movetype == MOVETYPE_FLY)
		{
			m_IdealActivity = ACT_GLIDE;
		}
		break;

	default: 
		CProtagonistMonster :: StartTask( pTask );
		break;
	}
}

void CMaria :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:

		CProtagonistMonster::RunTask( pTask );
		break;
	default:
		CProtagonistMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CMaria :: PrescheduleThink ( void )
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
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CMaria :: ISoundMask ( void) 
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
int	CMaria :: Classify ( void )
{
		return	CLASS_PLAYER_ALLY;
}

//=========================================================
// ALertSound - Maria says "Freeze!"
//=========================================================
void CMaria :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( FOkToSpeak() )
		{
			PlaySentence( "MA_ATTACK", RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
		}
	}

}
//=========================================================
// FCanCheckAttacks - this is overridden for Maria
// because she can throw/shoot grenades when she can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
BOOL CMaria :: FCanCheckAttacks ( void )
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
// SetActivity 
//=========================================================
void CMaria :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RUN:
		if ( pev->health <= MARIA_LIMP_HEALTH )
		{
			// limp!
			iSequence = LookupActivity ( ACT_RUN_HURT );
		}
		else
		{
			iSequence = LookupActivity ( NewActivity );
		}
		break;
	case ACT_WALK:
		if ( pev->health <= MARIA_LIMP_HEALTH )
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
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CMaria :: SetYawSpeed ( void )
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
// GetGunPosition	return the end of the barrel
//=========================================================

Vector CMaria :: GetGunPosition( )
{
	Vector vecGun, vecAngles;
	GetAttachment( 1, vecGun, vecAngles );

	return vecGun;
}

//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CMaria :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() )
	{
		TraceResult	tr;

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
// Shoot - shoots one round from the pistol at
// the enemy Maria is facing.
//=========================================================
void CMaria :: Shoot ( void )
{
	Vector vecShootOrigin;

	UTIL_MakeVectors(pev->angles);
	vecShootOrigin = pev->origin + Vector( 0, 0, 55 );
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
	pev->effects = EF_MUZZLEFLASH;

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_1DEGREES, 1024, BULLET_PLAYER_357 );

	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/maria_python.wav", 1, ATTN_NORM );

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );

	WeaponFlash ( vecShootOrigin );

	// UNDONE: Reload?
	m_cAmmoLoaded--;// take away a bullet!
}
		
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CMaria :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case MARIA_AE_SHOOT:
		Shoot();
		break;

	case MARIA_AE_SHELL:
		for( int i = 0; i < 3; i++)
		{
			Vector vecGunOrigin = GetGunPosition();

			Vector	vecShellVelocity = gpGlobals->v_up * RANDOM_FLOAT(-75,-200);
			EjectBrass ( vecGunOrigin, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL); 
		}
		break;

	case MARIA_AE_RELOAD:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/maria_reload.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions(bits_COND_NO_AMMO_LOADED);
		break;

	default:
		CProtagonistMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Spawn
//=========================================================
void CMaria :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/maria.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= gSkillData.mariaHealth;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;
	m_cClipSize			= MARIA_CLIP_SIZE;
	m_cAmmoLoaded		= m_cClipSize;

	SetBodygroup( GUN_GROUP, GUN_DRAWN );

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_SQUAD;
	m_fEnemyEluded		= FALSE;

	MonsterInit();
	StartMonster();
	SetUse( FollowerUse );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CMaria :: Precache()
{
	PRECACHE_MODEL( "models/maria.mdl" );

	m_iBrassShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND( "weapons/maria_python.wav" );
	PRECACHE_SOUND( "weapons/maria_reload.wav" );

	PRECACHE_SOUND( "maria/ma_pain1.wav" );
	PRECACHE_SOUND( "maria/ma_pain2.wav" );
	PRECACHE_SOUND( "maria/ma_pain3.wav" );
	PRECACHE_SOUND( "maria/ma_pain4.wav" );
	PRECACHE_SOUND( "maria/ma_pain5.wav" );

	PRECACHE_SOUND( "common/maria_step01.wav" );// Maria step on concrete
	PRECACHE_SOUND( "common/maria_dirt01.wav" );// Maria step on dirt
	PRECACHE_SOUND( "common/maria_metal01.wav" );// Maria step on metal
	PRECACHE_SOUND( "common/maria_wood01.wav" );// Maria step on wood
	PRECACHE_SOUND( "common/maria_rug01.wav" );// Maria step on rug
	PRECACHE_SOUND( "common/maria_grate01.wav" );// Maria step on grate
	PRECACHE_SOUND( "common/maria_tile01.wav" );// Maria step on tile
	PRECACHE_SOUND( "common/maria_organic01.wav" );// Maria step on organic
	PRECACHE_SOUND( "common/maria_gravel01.wav" );// Maria step on organic

	PRECACHE_SOUND( "items/suitchargeok1.wav" );
	PRECACHE_SOUND( "items/suitcharge1.wav" );
	PRECACHE_SOUND( "debris/beamstart5.wav" );

	PRECACHE_SOUND( "maria/ma_die1.wav" );
	// every new maria must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();
	CProtagonistMonster::Precache();
}	

// Init talk data
void CMaria :: TalkInit()
{
	
	CProtagonistMonster::TalkInit();

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER]  =	"MA_ANSWER";
	m_szGrp[TLK_QUESTION] =	"MA_QUESTION";
	m_szGrp[TLK_IDLE] =		"MA_IDLE";
	m_szGrp[TLK_STARE] =		"MA_STARE";
	m_szGrp[TLK_USE] =		"MA_OK";
	m_szGrp[TLK_UNUSE] =	"MA_WAIT";
	m_szGrp[TLK_STOP] =		"MA_STOP";

	m_szGrp[TLK_NOSHOOT] =	"MA_SCARED";
	m_szGrp[TLK_HELLO] =	"MA_HELLO";

	m_szGrp[TLK_PLHURT1] =	"!MA_CUREA";
	m_szGrp[TLK_PLHURT2] =	"!MA_CUREB"; 
	m_szGrp[TLK_PLHURT3] =	"!MA_CUREC";

	m_szGrp[TLK_PHELLO] =	NULL;	//"BA_PHELLO";		// UNDONE
	m_szGrp[TLK_PIDLE] =	NULL;	//"BA_PIDLE";			// UNDONE
	m_szGrp[TLK_PQUESTION] = "MA_PQUEST";		// UNDONE

	m_szGrp[TLK_SMELL] =	"MA_SMELL";
	
	m_szGrp[TLK_WOUND] =	"MA_WOUND";
	m_szGrp[TLK_MORTAL] =	"MA_MORTAL";

	// get voice for head - just one maria voice for now
	m_voicePitch = 100;
}

void CMaria :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	pev->dmgtime = gpGlobals->time;
	TraceResult tr;
	m_flShieldDamage += flDamage/0.5;

	if ( pevAttacker->flags & FL_CLIENT)
		return;

	if ( ( m_flShieldDamage < gSkillData.mariaHealth / 3 ) && !m_fShieldInActive )
	{
		if ( m_flShieldTime != gpGlobals->time )
		{
			pev->renderfx = kRenderFxGlowShell;
			pev->rendercolor.x = 0; // R
			pev->rendercolor.y = 255; // G
			pev->rendercolor.z = 255; // B
			pev->renderamt = 15;

			m_flShieldTime = gpGlobals->time + 0.5; // Set the time.
			m_flRechargeStartTime = gpGlobals-> time + 5; // This is when we start recharging.
		}

		if ( pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0,10) < 1) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2) );
			pev->dmgtime = gpGlobals->time;
		}

		if ( RANDOM_LONG( 0, 1 ) == 0 )
		{
			Vector vecTracerDir = vecDir;

			vecTracerDir.x += RANDOM_FLOAT( -0.3, 0.3 );
			vecTracerDir.y += RANDOM_FLOAT( -0.3, 0.3 );
			vecTracerDir.z += RANDOM_FLOAT( -0.3, 0.3 );

			vecTracerDir = vecTracerDir * -512;

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
			WRITE_BYTE( TE_TRACER );
				WRITE_COORD( ptr->vecEndPos.x );
				WRITE_COORD( ptr->vecEndPos.y );
				WRITE_COORD( ptr->vecEndPos.z );

				WRITE_COORD( vecTracerDir.x );
				WRITE_COORD( vecTracerDir.y );
				WRITE_COORD( vecTracerDir.z );
			MESSAGE_END();
		}

		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));
		flDamage = 0;
	}
	else
	{
		if ( !m_fShieldInActive && !m_flShieldRechargeTime )
		{
			m_flShieldRechargeTime = gpGlobals->time + 30;
			m_fShieldInActive = TRUE;
		}

		if ( flDamage )
		{
			Vector vecBlood = (ptr->vecEndPos - pev->origin).Normalize( );

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
				WRITE_BYTE ( TE_GUNSHOT );
				WRITE_COORD( ptr->vecEndPos.x );
				WRITE_COORD( ptr->vecEndPos.y );
				WRITE_COORD( ptr->vecEndPos.z );
			MESSAGE_END();

			UTIL_BloodStream( ptr->vecEndPos, vecBlood, 6, 100);

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
		}
	}
	if ( m_fShieldInActive )
	{
		CProtagonistMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
	}
}


int CMaria :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	int ret;

	if ( pevAttacker->flags & FL_CLIENT)
		return 0;

	// make sure friends talk about it if player hurts talkmonsters...
	ret = CProtagonistMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);

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
			if ( (m_afMemory & bits_MEMORY_SUSPICIOUS) || IsFacing( pevAttacker, pev->origin ) )
			{
				// Alright, now I'm pissed!
				PlaySentence( "MA_MAD", 4, VOL_NORM, ATTN_NORM );

				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				// Hey, be careful with that
				PlaySentence( "MA_SHOT", 4, VOL_NORM, ATTN_NORM );
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
		{
			PlaySentence( "MA_SHOT", 4, VOL_NORM, ATTN_NORM );
		}
	}
	return ret;
}

	
//=========================================================
// PainSound
//=========================================================
void CMaria :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,4))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maria/ma_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maria/ma_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maria/ma_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maria/ma_pain4.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 4: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maria/ma_pain5.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CMaria :: DeathSound ( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "maria/ma_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch());
}

//=========================================================
// CheckAmmo - overridden for the Maria because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CMaria :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	{
		SetConditions(bits_COND_NO_AMMO_LOADED);
	}
}
void CMaria::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( GetBodygroup( GUN_GROUP ) != GUN_NONE )
	{// drop the gun!
		Vector vecGunPos;
		Vector vecGunAngles;

		SetBodygroup( GUN_GROUP , GUN_NONE );

		GetAttachment( 0, vecGunPos, vecGunAngles );
		
		CBaseEntity *pGun = DropItem( "weapon_9mmar", vecGunPos, vecGunAngles );
	}

	SetUse( NULL );	
	CProtagonistMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CMaria :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	case SCHED_TARGET_FACE:
		{
			// call base class default so that barney will talk
			// when 'used' 
			psched = CProtagonistMonster::GetScheduleOfType(Type);

			if (psched == slIdleStand)
				return slMariaFaceTarget;	// override this for different target face behavior
			else
				return psched;
		}
	case SCHED_MARIA_COVER_AND_RELOAD:
		{
			return &slMariaHideReload[ 0 ];
		}
	case SCHED_TARGET_CHASE:
		return slMariaFollow;

	case SCHED_IDLE_STAND:
	{
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CProtagonistMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
		{
			// just look straight ahead.
			return slMariaIdleStand;
		}
		else
			return psched;	
	}
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			return &slMariaTakeCover[ 0 ];
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slMariaTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_MARIA_TAKECOVER_FAILED:
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_PROTAGONIST_ENGAGE ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}
			else
			{
				return GetScheduleOfType ( SCHED_FAIL );
			}
		}
		break;
	case SCHED_MARIA_ELOF_FAIL:
		{
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}
	case SCHED_MARIA_ESTABLISH_LINE_OF_FIRE:
		{
			return &slMariaEstablishLineOfFire[ 0 ];
		}
	case SCHED_RANGE_ATTACK1:
		{
			return &slMariaRangeAttack1[ 0 ];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slMariaCombatFace[ 0 ];
		}
	case SCHED_MARIA_WAIT_FACE_ENEMY:
		{
			return &slMariaWaitInCover[ 0 ];
		}
	case SCHED_MARIA_SWEEP:
		{
			return &slMariaSweep[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( InSquad() )
			{
				if ( !IsLeader() )
				{
					return &slMariaFail[ 0 ];
				}
			}
			if ( IsFollowing() )
			{
				return &slMariaFail[ 0 ];
			}

			return &slMariaVictoryDance[ 0 ];
		}
	case SCHED_MARIA_SUPPRESS:
		{
			return &slMariaSuppress[ 0 ];
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// Maria has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slMariaCombatFail[ 0 ];
			}

			return &slMariaFail[ 0 ];
		}
		}
	return CProtagonistMonster::GetScheduleOfType( Type );
}

//=========================================================
// RunAI
//=========================================================
void CMaria :: RunAI( void )
{
	CProtagonistMonster :: RunAI();

	if ( m_fShieldInActive ) // Our shield needs recharging.
	{
		if ( pev->renderamt == 15 )
		{
			pev->renderfx = kRenderFxNone;
			pev->renderamt = 255;

			EMIT_SOUND_DYN(	ENT(pev), CHAN_WEAPON, "debris/beamstart5.wav", 1, ATTN_NORM, 0, PITCH_NORM );

		}
		if ( m_flShieldRechargeTime < gpGlobals->time )
		{
			m_fShieldInActive = FALSE;
			m_flShieldDamage = NULL;
			m_flShieldRechargeTime = NULL;
			m_flShieldTime = NULL;

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		}
	}

	if ( !m_fShieldInActive ) // Our shield effect has to get removed
	{
		if ( m_flShieldTime < gpGlobals->time )
		{
			m_flShieldTime = NULL;

			if ( pev->renderamt == 15 )
			{
				pev->renderfx = kRenderFxNone;
				pev->renderamt = 255;
			}
		}
	}
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CMaria :: GetSchedule ( void )
{
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
					SENTENCEG_PlayRndSz( ENT(pev), "MA_GREN", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
		}
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		PlaySentence( "MA_KILL", 4, VOL_NORM, ATTN_NORM );
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

			else if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				if ( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if ( !IsLeader() )
					{
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_MARIA_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_MARIA_ESTABLISH_LINE_OF_FIRE );
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
										SENTENCEG_PlayRndSz( ENT(pev), "MA_ZOMBIE", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
									else if ( FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
										SENTENCEG_PlayRndSz( ENT(pev), "MA_HEADCRAB", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
									else
										SENTENCEG_PlayRndSz( ENT(pev), "MA_ATTACK", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
								}
								else
								{
									SENTENCEG_PlayRndSz( ENT(pev), "MA_MAD", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
								}
							}
						}
						
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_MARIA_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_MARIA_ESTABLISH_LINE_OF_FIRE );
						}
					}
				}
			}
			// no ammo
			else if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			{
				PlaySentence( "MA_RELOAD", 2, VOL_NORM, ATTN_NORM );
				return GetScheduleOfType ( SCHED_MARIA_COVER_AND_RELOAD );
			}
			// damaged just a little
			else if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{

				int iPercent = RANDOM_LONG(0,99);

				if ( iPercent <= 90 && m_hEnemy != NULL )
				{
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( ENT(pev), "MA_COVER", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
					}
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
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
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
					return GetScheduleOfType( SCHED_MARIA_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					if (FOkToSpeak() && RANDOM_LONG(0,1))
					{
						SENTENCEG_PlayRndSz( ENT(pev), "MA_TAUNT", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
					}
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}
			if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_MARIA_ESTABLISH_LINE_OF_FIRE );
			}
		}
		break;

	case MONSTERSTATE_ALERT:	
	case MONSTERSTATE_IDLE:
		if ( m_hEnemy == NULL && IsFollowing() )
		{
			if ( !m_hTargetEnt->IsAlive() )
			{
				PlaySentence( "MA_PLDEAD", 2, VOL_NORM, ATTN_NORM );
				StopFollowing( FALSE );
				break;
			}
			else
			{
				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				{
					PlaySentence( "MA_MOVE", 2, VOL_NORM, ATTN_NORM );
					return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );
				}
				return GetScheduleOfType( SCHED_TARGET_FACE );
			}
		}

		if ( HasConditions( bits_COND_CLIENT_PUSH ) )
		{
			PlaySentence( "MA_MOVE", 2, VOL_NORM, ATTN_NORM );
			return GetScheduleOfType( SCHED_MOVE_AWAY );
		}

		// try to say something about smells
		TrySmellTalk();
		break;
	}
	
	return CProtagonistMonster::GetSchedule();
}

MONSTERSTATE CMaria :: GetIdealState ( void )
{
	return CProtagonistMonster::GetIdealState();
}

void CMaria::DeclineFollowing( void )
{
	PlaySentence( "MA_POK", 2, VOL_NORM, ATTN_NORM );
}