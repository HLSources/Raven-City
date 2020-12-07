//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//=========================================================
// Special Ops grunt, a major enemy of the player and the
// remaining H.E.C.U.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"animation.h"
#include	"rcallymonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"
#include	"customentity.h"

//=========================================================
//
//=========================================================
#define	SO_GRUNT_CLIP_SIZE					32 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define SO_GRUNT_LIMP_HEALTH				20

#define SO_GRUNT_M4A1					( 1 << 0)
#define SO_GRUNT_HANDGRENADE			( 1 << 1)
#define SO_GRUNT_GRENADELAUNCHER		( 1 << 2)
#define SO_GRUNT_SHOTGUN				( 1 << 3)
#define SO_GRUNT_M249					( 1 << 4)

// Torso group for weapons
#define	SO_TORSO_GROUP					2
#define SO_TORSO_DEFAULT				0
#define SO_TORSO_M249					1
#define SO_TORSO_FLAT					2
#define SO_TORSO_SHOTGUN				3
// Weapon group
#define SO_GUN_GROUP					3
#define SO_GUN_MP5						0
#define SO_GUN_SHOTGUN					1
#define SO_GUN_SAW						2
#define SO_GUN_NONE					3

//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_SO_GRUNT_NOFIRE	( bits_COND_SPECIAL1 )
//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_SO_GRUNT_FACE_TOSS_DIR = LAST_TALKMONSTER_TASK + 1,
	TASK_SO_GRUNT_CHECK_FIRE,
};
//=========================================================
// monster heads
//=========================================================

// Head group
#define SO_HEAD_GROUP					1
#define SO_HEAD_MASK					0
#define SO_HEAD_BERET					1
#define SO_HEAD_SHOTGUN					2
#define SO_HEAD_SAW						3
#define SO_HEAD_SAW_BLACK				4
#define SO_HEAD_MP						5
#define SO_HEAD_MAJOR					6
#define SO_HEAD_BERET_BLACK				7

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		SPEC_OPS_AE_RELOAD			( 2 )
#define		SPEC_OPS_AE_KICK			( 3 )
#define		SPEC_OPS_AE_BURST1			( 4 )
#define		SPEC_OPS_AE_BURST2			( 5 ) 
#define		SPEC_OPS_AE_BURST3			( 6 ) 
#define		SPEC_OPS_AE_GREN_TOSS		( 7 )
#define		SPEC_OPS_AE_GREN_LAUNCH		( 8 )
#define		SPEC_OPS_AE_GREN_DROP		( 9 )
#define		SPEC_OPS_AE_CAUGHT_ENEMY	( 10) // grunt established sight with an enemy (player only) that had previously eluded the squad.
#define		SPEC_OPS_AE_DROP_GUN		( 11) // grunt (probably dead) is dropping his mp5.
//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_SPEC_OPS_SUPPRESS = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_SPEC_OPS_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_SPEC_OPS_COVER_AND_RELOAD,
	SCHED_SPEC_OPS_SWEEP,
	SCHED_SPEC_OPS_FOUND_ENEMY,
	SCHED_SPEC_OPS_REPEL,
	SCHED_SPEC_OPS_REPEL_ATTACK,
	SCHED_SPEC_OPS_REPEL_LAND,
	SCHED_SPEC_OPS_WAIT_FACE_ENEMY,
	SCHED_SPEC_OPS_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_SPEC_OPS_ELOF_FAIL,
};
class CSOGrunt : public CRCAllyMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void CheckAmmo ( void );
	void SetActivity ( Activity NewActivity );
	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	void KeyValue( KeyValueData *pkvd );
	BOOL FCanCheckAttacks ( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	void DeclineFollowing( void );
	void PrescheduleThink ( void );
	Vector GetGunPosition( void );
	void Shoot ( void );
	void Shotgun ( void );
	void M249 ( void );
	// Override these to set behavior
	CBaseEntity	*Kick( void );
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );
	void GibMonster( void );
	void TalkInit( void );

	void Killed( entvars_t *pevAttacker, int iGib );

	BOOL FOkToSpeak( void );
	void JustSpoke( void );

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	
	int IRelationship ( CBaseEntity *pTarget );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// UNDONE: What is this for?  It isn't used?
	float	m_flPlayerDamage;// how much pain has the player inflicted on me?

	// checking the feasibility of a grenade toss is kind of costly, so we do it every couple of seconds,
	// not every server frame.
	float m_flNextGrenadeCheck;
	float m_flNextPainTime;
	float m_flLastEnemySightTime;

	float	m_flLinkToggle;// how much pain has the player inflicted on me?

	Vector	m_vecTossVelocity;

	BOOL	m_fThrowGrenade;
	BOOL	m_fStanding;
	BOOL	m_fFirstEncounter;// only put on the handsign show in the squad's first encounter.
	int		m_cClipSize;

	int		m_iBrassShell;
	int		m_iShotgunShell;

	int		m_iSentence;
	int		m_iHead;

	int		m_iM249Shell;
	int		m_iM249Link;

	static const char *PSOGruntSentences[];

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_specops_grunt, CSOGrunt );

TYPEDESCRIPTION	CSOGrunt::m_SaveData[] = 
{
	DEFINE_FIELD( CSOGrunt, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( CSOGrunt, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( CSOGrunt, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CSOGrunt, m_fThrowGrenade, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSOGrunt, m_fStanding, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSOGrunt, m_fFirstEncounter, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSOGrunt, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CSOGrunt, m_iHead, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSOGrunt, CRCAllyMonster );
const char *CSOGrunt::PSOGruntSentences[] = 
{
	"FG_GREN", // grenade scared grunt
	"FG_ALERT", // sees player
	"FG_MONSTER", // sees monster
	"FG_COVER", // running to cover
	"FG_THROW", // about to throw grenade
	"FG_CHARGE",  // running out to get the enemy
	"FG_TAUNT", // say rude things
};

enum
{
	SO_GRUNT_SENT_NONE = -1,
	SO_GRUNT_SENT_GREN = 0,
	SO_GRUNT_SENT_ALERT,
	SO_GRUNT_SENT_MONSTER,
	SO_GRUNT_SENT_COVER,
	SO_GRUNT_SENT_THROW,
	SO_GRUNT_SENT_CHARGE,
	SO_GRUNT_SENT_TAUNT,
} SO_GRUNT_SENTENCE_TYPES;

//=========================================================
// KeyValue
//
// !!! netname entvar field is used in squadmonster for groupname!!!
//=========================================================
void CSOGrunt :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "head"))
	{
		m_iHead = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseMonster::KeyValue( pkvd );
	}
}

void CSOGrunt::Killed( entvars_t *pevAttacker, int iGib )
{
	CBaseMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// someone else is talking - don't speak
//=========================================================
BOOL CSOGrunt :: FOkToSpeak( void )
{
// if someone else is talking, don't speak
	if (gpGlobals->time <= CRCAllyMonster::g_talkWaitTime)
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
//=========================================================
//=========================================================
void CSOGrunt :: JustSpoke( void )
{
	CRCAllyMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(1.5, 2.0);
	m_iSentence = SO_GRUNT_SENT_NONE;
}
//=========================================================
// IRelationship - overridden because Male Assassins are 
// Human Grunt's nemesis.
//=========================================================
int CSOGrunt::IRelationship ( CBaseEntity *pTarget )
{
	if ( FClassnameIs( pTarget->pev, "monster_human_grunt_ally" ) )
	{
		return R_NM;
	}

	return CRCAllyMonster::IRelationship( pTarget );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlSoGruntFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slSoGruntFollow[] =
{
	{
		tlSoGruntFollow,
		ARRAYSIZE ( tlSoGruntFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};
Task_t	tlSoGruntFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slSoGruntFaceTarget[] =
{
	{
		tlSoGruntFaceTarget,
		ARRAYSIZE ( tlSoGruntFaceTarget ),
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


Task_t	tlSoGruntIdleStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slSoGruntIdleStand[] =
{
	{ 
		tlSoGruntIdleStand,
		ARRAYSIZE ( tlSoGruntIdleStand ), 
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
// GruntFail
//=========================================================
Task_t	tlSoGruntFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slSoGruntFail[] =
{
	{
		tlSoGruntFail,
		ARRAYSIZE ( tlSoGruntFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"Grunt Fail"
	},
};

//=========================================================
// Grunt Combat Fail
//=========================================================
Task_t	tlSoGruntCombatFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY,		(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slSoGruntCombatFail[] =
{
	{
		tlSoGruntCombatFail,
		ARRAYSIZE ( tlSoGruntCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"Grunt Combat Fail"
	},
};

//=========================================================
// Victory dance!
//=========================================================
Task_t	tlSoGruntVictoryDance[] =
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

Schedule_t	slSoGruntVictoryDance[] =
{
	{ 
		tlSoGruntVictoryDance,
		ARRAYSIZE ( tlSoGruntVictoryDance ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE,
		0,
		"GruntVictoryDance"
	},
};

//=========================================================
// Establish line of fire - move to a position that allows
// the grunt to attack.
//=========================================================
Task_t tlSoGruntEstablishLineOfFire[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_SPEC_OPS_ELOF_FAIL	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0						},
	{ TASK_RUN_PATH,			(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slSoGruntEstablishLineOfFire[] =
{
	{ 
		tlSoGruntEstablishLineOfFire,
		ARRAYSIZE ( tlSoGruntEstablishLineOfFire ),
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

//=========================================================
// GruntFoundEnemy - grunt established sight with an enemy
// that was hiding from the squad.
//=========================================================
Task_t	tlSoGruntFoundEnemy[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_SIGNAL1			},
};

Schedule_t	slSoGruntFoundEnemy[] =
{
	{ 
		tlSoGruntFoundEnemy,
		ARRAYSIZE ( tlSoGruntFoundEnemy ), 
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"GruntFoundEnemy"
	},
};

//=========================================================
// GruntCombatFace Schedule
//=========================================================
Task_t	tlSoGruntCombatFace1[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_WAIT,					(float)1.5					},
	{ TASK_SET_SCHEDULE,			(float)SCHED_SPEC_OPS_SWEEP	},
};

Schedule_t	slSoGruntCombatFace[] =
{
	{ 
		tlSoGruntCombatFace1,
		ARRAYSIZE ( tlSoGruntCombatFace1 ), 
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
// empty or grunt gets hurt.
//=========================================================
Task_t	tlSoGruntSignalSuppress[] =
{
	{ TASK_STOP_MOVING,					0						},
	{ TASK_FACE_IDEAL,					(float)0				},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,	(float)ACT_SIGNAL2		},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_SO_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_SO_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_SO_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_SO_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
	{ TASK_FACE_ENEMY,					(float)0				},
	{ TASK_SO_GRUNT_CHECK_FIRE,			(float)0				},
	{ TASK_RANGE_ATTACK1,				(float)0				},
};

Schedule_t	slSoGruntSignalSuppress[] =
{
	{ 
		tlSoGruntSignalSuppress,
		ARRAYSIZE ( tlSoGruntSignalSuppress ), 
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND		|
		bits_COND_SO_GRUNT_NOFIRE		|
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"SignalSuppress"
	},
};

Task_t	tlSoGruntSuppress[] =
{
	{ TASK_STOP_MOVING,			0							},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
};

Schedule_t	slSoGruntSuppress[] =
{
	{ 
		tlSoGruntSuppress,
		ARRAYSIZE ( tlSoGruntSuppress ), 
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND		|
		bits_COND_SO_GRUNT_NOFIRE		|
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"Suppress"
	},
};


//=========================================================
// grunt wait in cover - we don't allow danger or the ability
// to attack to break a grunt's run to cover schedule, but
// when a grunt is in cover, we do want them to attack if they can.
//=========================================================
Task_t	tlSoGruntWaitInCover[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_WAIT_FACE_ENEMY,			(float)1					},
};

Schedule_t	slSoGruntWaitInCover[] =
{
	{ 
		tlSoGruntWaitInCover,
		ARRAYSIZE ( tlSoGruntWaitInCover ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2,

		bits_SOUND_DANGER,
		"GruntWaitInCover"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t	tlSoGruntTakeCover1[] =
{
	{ TASK_STOP_MOVING,				(float)0							},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_SPEC_OPS_TAKECOVER_FAILED	},
	{ TASK_WAIT,					(float)0.2							},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0							},
	{ TASK_RUN_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER			},
	{ TASK_SET_SCHEDULE,			(float)SCHED_SPEC_OPS_WAIT_FACE_ENEMY	},
};

Schedule_t	slSoGruntTakeCover[] =
{
	{ 
		tlSoGruntTakeCover1,
		ARRAYSIZE ( tlSoGruntTakeCover1 ), 
		0,
		0,
		"TakeCover"
	},
};

//=========================================================
// drop grenade then run to cover.
//=========================================================
Task_t	tlSoGruntGrenadeCover1[] =
{
	{ TASK_STOP_MOVING,						(float)0							},
	{ TASK_FIND_COVER_FROM_ENEMY,			(float)99							},
	{ TASK_FIND_FAR_NODE_COVER_FROM_ENEMY,	(float)384							},
	{ TASK_PLAY_SEQUENCE,					(float)ACT_SPECIAL_ATTACK1			},
	{ TASK_CLEAR_MOVE_WAIT,					(float)0							},
	{ TASK_RUN_PATH,						(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,				(float)0							},
	{ TASK_SET_SCHEDULE,					(float)SCHED_SPEC_OPS_WAIT_FACE_ENEMY	},
};

Schedule_t	slSoGruntGrenadeCover[] =
{
	{ 
		tlSoGruntGrenadeCover1,
		ARRAYSIZE ( tlSoGruntGrenadeCover1 ), 
		0,
		0,
		"GrenadeCover"
	},
};


//=========================================================
// drop grenade then run to cover.
//=========================================================
Task_t	tlSoGruntTossGrenadeCover1[] =
{
	{ TASK_FACE_ENEMY,						(float)0							},
	{ TASK_RANGE_ATTACK2, 					(float)0							},
	{ TASK_SET_SCHEDULE,					(float)SCHED_TAKE_COVER_FROM_ENEMY	},
};

Schedule_t	slSoGruntTossGrenadeCover[] =
{
	{ 
		tlSoGruntTossGrenadeCover1,
		ARRAYSIZE ( tlSoGruntTossGrenadeCover1 ), 
		0,
		0,
		"TossGrenadeCover"
	},
};

//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t	tlSoGruntTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_COWER			},// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slSoGruntTakeCoverFromBestSound[] =
{
	{ 
		tlSoGruntTakeCoverFromBestSound,
		ARRAYSIZE ( tlSoGruntTakeCoverFromBestSound ), 
		0,
		0,
		"GruntTakeCoverFromBestSound"
	},
};

//=========================================================
// Grunt reload schedule
//=========================================================
Task_t	tlSoGruntHideReload[] =
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

Schedule_t slSoGruntHideReload[] = 
{
	{
		tlSoGruntHideReload,
		ARRAYSIZE ( tlSoGruntHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"GruntHideReload"
	}
};

//=========================================================
// Do a turning sweep of the area
//=========================================================
Task_t	tlSoGruntSweep[] =
{
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
};

Schedule_t	slSoGruntSweep[] =
{
	{ 
		tlSoGruntSweep,
		ARRAYSIZE ( tlSoGruntSweep ), 
		
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_WORLD		|// sound flags
		bits_SOUND_DANGER		|
		bits_SOUND_PLAYER,

		"Grunt Sweep"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlSoGruntRangeAttack1A[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,		(float)ACT_CROUCH },
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t	slSoGruntRangeAttack1A[] =
{
	{ 
		tlSoGruntRangeAttack1A,
		ARRAYSIZE ( tlSoGruntRangeAttack1A ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_HEAR_SOUND		|
		bits_COND_SO_GRUNT_NOFIRE		|
		bits_COND_NO_AMMO_LOADED,
		
		bits_SOUND_DANGER,
		"Range Attack1A"
	},
};


//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlSoGruntRangeAttack1B[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_IDLE_ANGRY  },
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_SO_GRUNT_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t	slSoGruntRangeAttack1B[] =
{
	{ 
		tlSoGruntRangeAttack1B,
		ARRAYSIZE ( tlSoGruntRangeAttack1B ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED	|
		bits_COND_SO_GRUNT_NOFIRE		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"Range Attack1B"
	},
};

//=========================================================
// secondary range attack. Overriden because base class stops attacking when the enemy is occluded.
// grunt's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlSoGruntRangeAttack2[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SO_GRUNT_FACE_TOSS_DIR,		(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_RANGE_ATTACK2	},
	{ TASK_SET_SCHEDULE,			(float)SCHED_SPEC_OPS_WAIT_FACE_ENEMY	},// don't run immediately after throwing grenade.
};

Schedule_t	slSoGruntRangeAttack2[] =
{
	{ 
		tlSoGruntRangeAttack2,
		ARRAYSIZE ( tlSoGruntRangeAttack2 ), 
		0,
		0,
		"RangeAttack2"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlSoGruntRepel[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_GLIDE 	},
};

Schedule_t	slSoGruntRepel[] =
{
	{ 
		tlSoGruntRepel,
		ARRAYSIZE ( tlSoGruntRepel ), 
		bits_COND_SEE_ENEMY			|
		bits_COND_NEW_ENEMY			|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER			|
		bits_SOUND_COMBAT			|
		bits_SOUND_PLAYER, 
		"Repel"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlSoGruntRepelAttack[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_FLY 	},
};

Schedule_t	slSoGruntRepelAttack[] =
{
	{ 
		tlSoGruntRepelAttack,
		ARRAYSIZE ( tlSoGruntRepelAttack ), 
		bits_COND_ENEMY_OCCLUDED,
		0,
		"Repel Attack"
	},
};

//=========================================================
// repel land
//=========================================================
Task_t	tlSoGruntRepelLand[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_LAND	},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_RUN_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t	slSoGruntRepelLand[] =
{
	{ 
		tlSoGruntRepelLand,
		ARRAYSIZE ( tlSoGruntRepelLand ), 
		bits_COND_SEE_ENEMY			|
		bits_COND_NEW_ENEMY			|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER			|
		bits_SOUND_COMBAT			|
		bits_SOUND_PLAYER, 
		"Repel Land"
	},
};


DEFINE_CUSTOM_SCHEDULES( CSOGrunt )
{
	slSoGruntFollow,
	slSoGruntFaceTarget,
	slSoGruntIdleStand,
	slSoGruntFail,
	slSoGruntCombatFail,
	slSoGruntVictoryDance,
	slSoGruntEstablishLineOfFire,
	slSoGruntFoundEnemy,
	slSoGruntCombatFace,
	slSoGruntSignalSuppress,
	slSoGruntSuppress,
	slSoGruntWaitInCover,
	slSoGruntTakeCover,
	slSoGruntGrenadeCover,
	slSoGruntTossGrenadeCover,
	slSoGruntTakeCoverFromBestSound,
	slSoGruntHideReload,
	slSoGruntSweep,
	slSoGruntRangeAttack1A,
	slSoGruntRangeAttack1B,
	slSoGruntRangeAttack2,
	slSoGruntRepel,
	slSoGruntRepelAttack,
	slSoGruntRepelLand,
};


IMPLEMENT_CUSTOM_SCHEDULES( CSOGrunt, CRCAllyMonster );

void CSOGrunt :: StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_SO_GRUNT_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_SO_GRUNT_NOFIRE );
		}
		TaskComplete();
		break;

	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// grunt no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CRCAllyMonster ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;

	case TASK_SO_GRUNT_FACE_TOSS_DIR:
		break;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CRCAllyMonster :: StartTask( pTask );
		if (pev->movetype == MOVETYPE_FLY)
		{
			m_IdealActivity = ACT_GLIDE;
		}
		break;

	default: 
		CRCAllyMonster :: StartTask( pTask );
		break;
	}
}

void CSOGrunt :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{

	case TASK_SO_GRUNT_FACE_TOSS_DIR:
		{
			// project a point along the toss vector and turn to face that point.
			MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
			ChangeYaw( pev->yaw_speed );

			if ( FacingIdeal() )
			{
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			}
			break;
		}
	default:
		{
			CRCAllyMonster :: RunTask( pTask );
			break;
		}
	}
}
//=========================================================
// GibMonster - make gun fly through the air.
//=========================================================
void CSOGrunt :: GibMonster ( void )
{
	Vector	vecGunPos;
	Vector	vecGunAngles;

	if ( GetBodygroup( 3 ) != 3 )
	{// throw a gun if the grunt has one
		GetAttachment( 0, vecGunPos, vecGunAngles );
		
		CBaseEntity *pGun;
		if (FBitSet( pev->weapons, SO_GRUNT_SHOTGUN ))
		{
			pGun = DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
		}
		else if (FBitSet( pev->weapons, SO_GRUNT_M4A1 ))
		{
			pGun = DropItem( "weapon_m4a1", vecGunPos, vecGunAngles );
		}
		else
		{
			pGun = DropItem( "weapon_m249", vecGunPos, vecGunAngles );
		}
		if ( pGun )
		{
			pGun->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));
			pGun->pev->avelocity = Vector ( 0, RANDOM_FLOAT( 200, 400 ), 0 );
		}
	
		if (FBitSet( pev->weapons, SO_GRUNT_GRENADELAUNCHER ))
		{
			pGun = DropItem( "ammo_ARgrenades", vecGunPos, vecGunAngles );
			if ( pGun )
			{
				pGun->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));
				pGun->pev->avelocity = Vector ( 0, RANDOM_FLOAT( 200, 400 ), 0 );
			}
		}
	}

	CBaseMonster :: GibMonster();
}
//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CSOGrunt :: ISoundMask ( void) 
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
// CheckAmmo - overridden for the grunt because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CSOGrunt :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	{
		SetConditions(bits_COND_NO_AMMO_LOADED);
	}
}
//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CSOGrunt :: Classify ( void )
{
	return	CLASS_HUMAN_MILITARY;
}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSOGrunt :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:	
		ys = 150;		
		break;
	case ACT_RUN:	
		ys = 150;	
		break;
	case ACT_WALK:	
		ys = 180;		
		break;
	case ACT_RANGE_ATTACK1:	
		ys = 120;	
		break;
	case ACT_RANGE_ATTACK2:	
		ys = 120;	
		break;
	case ACT_MELEE_ATTACK1:	
		ys = 120;	
		break;
	case ACT_MELEE_ATTACK2:	
		ys = 120;	
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:	
		ys = 180;
		break;
	case ACT_GLIDE:
	case ACT_FLY:
		ys = 30;
		break;
	default:
		ys = 90;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CSOGrunt :: PrescheduleThink ( void )
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
// FCanCheckAttacks - this is overridden for human grunts
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
BOOL CSOGrunt :: FCanCheckAttacks ( void )
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
// CheckMeleeAttack1
//=========================================================
BOOL CSOGrunt :: CheckMeleeAttack1 ( float flDot, float flDist )
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
// CheckRangeAttack1 - overridden for HGrunt, cause 
// FCanCheckAttacks() doesn't disqualify all attacks based
// on whether or not the enemy is occluded because unlike
// the base class, the HGrunt can attack when the enemy is
// occluded (throw grenade over wall, etc). We must 
// disqualify the machine gun attack if the enemy is occluded.
//=========================================================
BOOL CSOGrunt :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() && ( GetBodygroup( 3 ) != 3 ) )
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
// CheckRangeAttack2 - this checks the Grunt's grenade
// attack. 
//=========================================================
BOOL CSOGrunt :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if (! FBitSet(pev->weapons, (SO_GRUNT_HANDGRENADE | SO_GRUNT_GRENADELAUNCHER)) || FBitSet(pev->weapons, ( SO_GRUNT_M249 )) )
	{
		return FALSE;
	}
	
	// if the grunt isn't moving, it's ok to check.
	if ( m_flGroundSpeed != 0 )
	{
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

	// assume things haven't changed too much since last time
	if (gpGlobals->time < m_flNextGrenadeCheck )
	{
		return m_fThrowGrenade;
	}

	if ( !FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND ) && m_hEnemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->absmax.z  )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}
	
	Vector vecTarget;

	if (FBitSet( pev->weapons, SO_GRUNT_HANDGRENADE))
	{
		// find feet
		if (RANDOM_LONG(0,1))
		{
			// magically know where they are
			vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
		}
		else
		{
			// toss it to where you last saw them
			vecTarget = m_vecEnemyLKP;
		}
		// vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
		// estimate position
		// vecTarget = vecTarget + m_hEnemy->pev->velocity * 2;
	}
	else
	{
		// find target
		// vecTarget = m_hEnemy->BodyTarget( pev->origin );
		vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
		// estimate position
		if (HasConditions( bits_COND_SEE_ENEMY))
			vecTarget = vecTarget + ((vecTarget - pev->origin).Length() / gSkillData.sogruntGrenadeSpeed) * m_hEnemy->pev->velocity;
	}

	// are any of my squad members near the intended grenade impact area?
	if ( InSquad() )
	{
		if (SquadMemberInRange( vecTarget, 256 ))
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
			m_fThrowGrenade = FALSE;
			return m_fThrowGrenade;	//AJH need this or it is overridden later.
		}
	}
	
	if ( ( vecTarget - pev->origin ).Length2D() <= 256 )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		m_fThrowGrenade = FALSE;
		return m_fThrowGrenade;
	}

		
	if (FBitSet( pev->weapons, SO_GRUNT_HANDGRENADE))
	{
		Vector vecToss = VecCheckToss( pev, GetGunPosition(), vecTarget, 0.5 );

		if ( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}
	else
	{
		Vector vecToss = VecCheckThrow( pev, GetGunPosition(), vecTarget, gSkillData.sogruntGrenadeSpeed, 0.5 );

		if ( vecToss != g_vecZero )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 0.3; // 1/3 second.
		}
		else
		{
			// don't throw
			m_fThrowGrenade = FALSE;
			// don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
		}
	}

	

	return m_fThrowGrenade;
}
//=========================================================
//=========================================================
CBaseEntity *CSOGrunt :: Kick( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->angles );
	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 70);

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );
	
	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		return pEntity;
	}

	return NULL;
}

//=========================================================
// GetGunPosition	return the end of the barrel
//=========================================================

Vector CSOGrunt :: GetGunPosition( )
{
	if (m_fStanding )
	{
		return pev->origin + Vector( 0, 0, 60 );
	}
	else
	{
		return pev->origin + Vector( 0, 0, 48 );
	}
}

//=========================================================
// Shoot
//=========================================================
void CSOGrunt :: Shoot ( void )
{
	if (m_hEnemy == NULL )
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	switch ( RANDOM_LONG(0,1) )
	{
		case 0: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/m4_fire1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/m4_fire2.wav", 1, ATTN_NORM ); break;
	}

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL); 
	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_4DEGREES, 2048, BULLET_MONSTER_M4A1 ); // shoot +-5 degrees

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}

//=========================================================
// Shoot
//=========================================================
void CSOGrunt :: Shotgun ( void )
{
	if (m_hEnemy == NULL)
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iShotgunShell, TE_BOUNCE_SHOTSHELL); 
	FireBullets(gSkillData.sogruntShotgunPellets, vecShootOrigin, vecShootDir, VECTOR_CONE_9DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0 ); // shoot +-7.5 degrees

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}
//=========================================================
// Shoot
//=========================================================
void CSOGrunt :: M249 ( void )
{
	if (m_hEnemy == NULL )
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

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_3DEGREES, 2048, BULLET_MONSTER_556 ); // shoot +-5 degrees

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CSOGrunt :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch( pEvent->event )
	{
		case SPEC_OPS_AE_DROP_GUN:
			{
			Vector	vecGunPos;
			Vector	vecGunAngles;

			GetAttachment( 0, vecGunPos, vecGunAngles );

			// switch to body group with no gun.
			SetBodygroup( SO_GUN_GROUP, SO_GUN_NONE );

			// now spawn a gun.
			if (FBitSet( pev->weapons, SO_GRUNT_SHOTGUN ))
			{
				 DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
			}
			else if (FBitSet( pev->weapons, SO_GRUNT_M4A1 ))
			{
				 DropItem( "weapon_m4a1", vecGunPos, vecGunAngles );
			}
			else
			{
				 DropItem( "weapon_m249", vecGunPos, vecGunAngles );
			}
			if (FBitSet( pev->weapons, SO_GRUNT_GRENADELAUNCHER ))
			{
				DropItem( "ammo_ARgrenades", BodyTarget( pev->origin ), vecGunAngles );
			}

			}
			break;

		case SPEC_OPS_AE_RELOAD:
			if (FBitSet( pev->weapons, SO_GRUNT_M4A1 | SO_GRUNT_SHOTGUN ))
			{
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
			}
			else
			{
				EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/saw_reload2.wav", 1, ATTN_NORM );
			}
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions(bits_COND_NO_AMMO_LOADED);
			break;

		case SPEC_OPS_AE_GREN_TOSS:
		{
			UTIL_MakeVectors( pev->angles );
			// CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 34 + Vector (0, 0, 32), m_vecTossVelocity, 3.5 );
			CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5 );

			m_fThrowGrenade = FALSE;
			m_flNextGrenadeCheck = gpGlobals->time + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
			// !!!LATER - when in a group, only try to throw grenade if ordered.
		}
		break;

		case SPEC_OPS_AE_GREN_LAUNCH:
		{
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM);
			CGrenade::ShootContact( pev, GetGunPosition(), m_vecTossVelocity );
			m_fThrowGrenade = FALSE;
			if (g_iSkillLevel == SKILL_EASY)
				m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 );// wait a random amount of time before shooting again
			else
				m_flNextGrenadeCheck = gpGlobals->time + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
		}
		break;

		case SPEC_OPS_AE_GREN_DROP:
		{
			UTIL_MakeVectors( pev->angles );
			CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
		}
		break;

		case SPEC_OPS_AE_BURST1:
		{
			if ( FBitSet( pev->weapons, SO_GRUNT_M4A1 ))
			{
				Shoot();
			}
			else if ( FBitSet( pev->weapons, SO_GRUNT_SHOTGUN ))
			{
				Shotgun( );

				EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM );
			}
			else
			{
				M249( );
			}
		
			CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );
		}
		break;

		case SPEC_OPS_AE_BURST2:
		case SPEC_OPS_AE_BURST3:
			if ( FBitSet( pev->weapons, SO_GRUNT_M4A1 ))
				Shoot();
			else
				M249();
			break;

		case SPEC_OPS_AE_KICK:
		{
			CBaseEntity *pHurt = Kick();

			if ( pHurt )
			{
				// SOUND HERE!
				UTIL_MakeVectors( pev->angles );
				pHurt->pev->punchangle.x = 15;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
				pHurt->TakeDamage( pev, pev, gSkillData.sogruntDmgKick, DMG_CLUB );
			}
		}
		break;

		case SPEC_OPS_AE_CAUGHT_ENEMY:
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz(ENT(pev), "FG_ALERT", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
				JustSpoke();
			}

		}

		default:
			CRCAllyMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CSOGrunt :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/hgrunt_specops.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= gSkillData.sogruntHealth;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime	= gpGlobals->time;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded		= FALSE;
	m_fFirstEncounter	= TRUE;// this is true when the grunt spawns, because he hasn't encountered an enemy yet.
	m_flLinkToggle		= 0;

	m_HackedGunPos = Vector ( 0, 0, 55 );

	if ( m_iHead == -1 )
		m_iHead = 0;

	if ( pev->weapons == 0 || pev->weapons == -1 )
	{
		pev->weapons = SO_GRUNT_M4A1;
	}
	if (FBitSet( pev->weapons, SO_GRUNT_SHOTGUN ))
	{
		SetBodygroup( SO_GUN_GROUP, SO_GUN_SHOTGUN );
		SetBodygroup( SO_TORSO_GROUP, SO_TORSO_SHOTGUN );
		m_cClipSize		= 8;
	}
	if (FBitSet( pev->weapons, SO_GRUNT_M4A1 ))
	{
		SetBodygroup( SO_GUN_GROUP, SO_GUN_MP5 );
		m_cClipSize		= SO_GRUNT_CLIP_SIZE;
	}
	if (FBitSet( pev->weapons, SO_GRUNT_M249 ))
	{
		SetBodygroup( SO_GUN_GROUP, SO_GUN_SAW );
		SetBodygroup( SO_TORSO_GROUP, SO_TORSO_M249 );
		m_cClipSize		= SO_GRUNT_CLIP_SIZE;
	}

	if ( m_iHead == 0 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_MASK );
	if ( m_iHead == 1 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_BERET );
	if ( m_iHead == 2 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_SHOTGUN );
	if ( m_iHead == 3 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_SAW );
	if ( m_iHead == 4 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_SAW_BLACK );
	if ( m_iHead == 5 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_MP );
	if ( m_iHead == 6 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_MAJOR );
	if ( m_iHead == 7 )
		SetBodygroup( SO_HEAD_GROUP, SO_HEAD_BERET_BLACK );

	m_cAmmoLoaded		= m_cClipSize;

//	m_fHostile = TRUE;

	MonsterInit();
	StartMonster();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSOGrunt :: Precache()
{
	PRECACHE_MODEL("models/hgrunt_specops.mdl");

	PRECACHE_SOUND("weapons/saw_fire1.wav" );
	PRECACHE_SOUND("weapons/saw_fire2.wav" );
	PRECACHE_SOUND("weapons/saw_fire3.wav" );

	PRECACHE_SOUND( "weapons/m4_fire1.wav" );
	PRECACHE_SOUND( "weapons/m4_fire2.wav" );

	PRECACHE_SOUND("fgrunt/gr_pain1.wav");
	PRECACHE_SOUND("fgrunt/gr_pain2.wav");
	PRECACHE_SOUND("fgrunt/gr_pain3.wav");
	PRECACHE_SOUND("fgrunt/gr_pain4.wav");
	PRECACHE_SOUND("fgrunt/gr_pain5.wav");
	PRECACHE_SOUND("fgrunt/gr_pain6.wav");

	PRECACHE_SOUND("fgrunt/death1.wav");
	PRECACHE_SOUND("fgrunt/death2.wav");
	PRECACHE_SOUND("fgrunt/death3.wav");
	PRECACHE_SOUND("fgrunt/death4.wav");
	PRECACHE_SOUND("fgrunt/death5.wav");
	PRECACHE_SOUND("fgrunt/death6.wav");

	PRECACHE_SOUND("hgrunt/gr_reload1.wav");

	PRECACHE_SOUND("weapons/saw_reload2.wav");

	PRECACHE_SOUND("weapons/glauncher.wav");

	PRECACHE_SOUND("weapons/sbarrel1.wav");

	PRECACHE_SOUND("zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	m_iBrassShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell
	m_iShotgunShell = PRECACHE_MODEL ("models/shotgunshell.mdl");// shotgun shell
	m_iM249Shell = PRECACHE_MODEL ("models/saw_shell.mdl");// saw shell
	m_iM249Link = PRECACHE_MODEL ("models/saw_link.mdl");// saw link

	TalkInit();
	CRCAllyMonster::Precache();
}	

// Init talk data
void CSOGrunt :: TalkInit()
{
	
	CRCAllyMonster::TalkInit();

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER]  =	"FG_ANSWER";
	m_szGrp[TLK_QUESTION] =	"FG_QUESTION";
	m_szGrp[TLK_IDLE] =		"FG_IDLE";
	m_szGrp[TLK_STARE] =		"FG_STARE";
	m_szGrp[TLK_USE] =		"FG_OK";
	m_szGrp[TLK_UNUSE] =	"FG_WAIT";
	m_szGrp[TLK_STOP] =		"FG_STOP";

	m_szGrp[TLK_NOSHOOT] =	"FG_SCARED";
	m_szGrp[TLK_HELLO] =	"FG_HELLO";

	m_szGrp[TLK_PLHURT1] =	"FG_CURE";
	m_szGrp[TLK_PLHURT2] =	"FG_CURE"; 
	m_szGrp[TLK_PLHURT3] =	"FG_CURE";

	m_szGrp[TLK_SMELL] =	"FG_SMELL";
	
	m_szGrp[TLK_WOUND] =	"FG_WOUND";
	m_szGrp[TLK_MORTAL] =	"FG_MORTAL";


	if ( m_iHead == 0 )
		m_voicePitch = 100;
	if ( m_iHead == 1 )
		m_voicePitch = 100;
	if ( m_iHead == 2 )
		m_voicePitch = 90;
	if ( m_iHead == 3 )
		m_voicePitch = 100;
	if ( m_iHead == 4 )
		m_voicePitch = 90;
	if ( m_iHead == 5 )
		m_voicePitch = 100;
	if ( m_iHead == 6 )
		m_voicePitch = 100;
	if ( m_iHead == 7 )
		m_voicePitch = 90;
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

	
//=========================================================
// PainSound
//=========================================================
void CSOGrunt :: PainSound ( void )
{
	if ( gpGlobals->time > m_flNextPainTime )
	{
		switch ( RANDOM_LONG(0,5) )
		{
			case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/gr_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
			case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/gr_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
			case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/gr_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
			case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/gr_pain4.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
			case 4: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/gr_pain5.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
			case 5: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/gr_pain6.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		}
		m_flNextPainTime = gpGlobals->time + 1;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CSOGrunt :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,5))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/death1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/death2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/death3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/death4.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 4: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/death5.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 5: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "fgrunt/death6.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}
//=========================================================
// TraceAttack - make sure we're not taking it in the helmet
//=========================================================
void CSOGrunt :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	// check for helmet shot
	if (ptr->iHitgroup == 11)
	{
		// make sure we're wearing one
		if (GetBodygroup( 1 ) == 0 && (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB)))
		{
			// absorb damage
			flDamage -= 20;
			if (flDamage <= 0)
			{
				UTIL_Ricochet( ptr->vecEndPos, 1.0 );
				flDamage = 0.01;
			}
		}
		// it's head shot anyways
		ptr->iHitgroup = HITGROUP_HEAD;
	}
	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}
//=========================================================
// TakeDamage - overridden for the grunt because the grunt
// needs to forget that he is in cover if he's hurt. (Obviously
// not in a safe place anymore).
//=========================================================
int CSOGrunt :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Forget( bits_MEMORY_INCOVER );

	return CBaseMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CSOGrunt :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		{
			// call base class default so that barney will talk
			// when 'used' 
			psched = CRCAllyMonster::GetScheduleOfType(Type);

			if (psched == slIdleStand)
				return slSoGruntFaceTarget;	// override this for different target face behavior
			else
				return psched;
		}
	case SCHED_TARGET_CHASE:
		{
			return slSoGruntFollow;
		}
	case SCHED_IDLE_STAND:
		{
			psched = CRCAllyMonster::GetScheduleOfType(Type);

			if (psched == slIdleStand)
			{
				// just look straight ahead.
				return slSoGruntIdleStand;
			}
			else
				return psched;	
		}
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			return &slSoGruntTakeCover[ 0 ];
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slSoGruntTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_SPEC_OPS_TAKECOVER_FAILED:
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_FGRUNT_ENGAGE ) )
			{
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			}
			else
			{
				return GetScheduleOfType ( SCHED_FAIL );
			}
		}
		break;
	case SCHED_SPEC_OPS_ELOF_FAIL:
		{
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}
		break;
	case SCHED_SPEC_OPS_ESTABLISH_LINE_OF_FIRE:
		{
			return &slSoGruntEstablishLineOfFire[ 0 ];
		}
		break;
	case SCHED_RANGE_ATTACK1:
		{
			// randomly stand or crouch
			if (RANDOM_LONG(0,9) == 0)
				m_fStanding = RANDOM_LONG(0,1);
		 
			if (m_fStanding)
				return &slSoGruntRangeAttack1B[ 0 ];
			else
				return &slSoGruntRangeAttack1A[ 0 ];
		}
	case SCHED_RANGE_ATTACK2:
		{
			return &slSoGruntRangeAttack2[ 0 ];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slSoGruntCombatFace[ 0 ];
		}
	case SCHED_SPEC_OPS_WAIT_FACE_ENEMY:
		{
			return &slSoGruntWaitInCover[ 0 ];
		}
	case SCHED_SPEC_OPS_SWEEP:
		{
			return &slSoGruntSweep[ 0 ];
		}
	case SCHED_SPEC_OPS_COVER_AND_RELOAD:
		{
			return &slSoGruntHideReload[ 0 ];
		}
	case SCHED_SPEC_OPS_FOUND_ENEMY:
		{
			return &slSoGruntFoundEnemy[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( InSquad() )
			{
				if ( !IsLeader() )
				{
					return &slSoGruntFail[ 0 ];
				}
			}

			return &slSoGruntVictoryDance[ 0 ];
		}
	case SCHED_SPEC_OPS_SUPPRESS:
		{
			if ( m_fFirstEncounter )
			{
				m_fFirstEncounter = FALSE;// after first encounter, leader won't issue handsigns anymore when he has a new enemy
				return &slSoGruntSignalSuppress[ 0 ];
			}
			else
			{
				return &slSoGruntSuppress[ 0 ];
			}
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// grunt has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slSoGruntCombatFail[ 0 ];
			}

			return &slSoGruntFail[ 0 ];
		}
	case SCHED_SPEC_OPS_REPEL:
		{
			if (pev->velocity.z > -128)
				pev->velocity.z -= 32;
			return &slSoGruntRepel[ 0 ];
		}
	case SCHED_SPEC_OPS_REPEL_ATTACK:
		{
			if (pev->velocity.z > -128)
				pev->velocity.z -= 32;
			return &slSoGruntRepelAttack[ 0 ];
		}
	case SCHED_SPEC_OPS_REPEL_LAND:
		{
			return &slSoGruntRepelLand[ 0 ];
		}
	default:
		{
			return CRCAllyMonster :: GetScheduleOfType ( Type );
		}
	}
}
//=========================================================
// SetActivity 
//=========================================================
void CSOGrunt :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// grunt is either shooting standing or shooting crouched
		if (FBitSet( pev->weapons, SO_GRUNT_M4A1))
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_mp5" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_mp5" );
			}
		}
		else if (FBitSet( pev->weapons, SO_GRUNT_SHOTGUN))
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_shotgun" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_shotgun" );
			}
		}
		else
		{
			if ( m_fStanding )
			{
				// get aimable sequence
				iSequence = LookupSequence( "standing_saw" );
			}
			else
			{
				// get crouching shoot
				iSequence = LookupSequence( "crouching_saw" );
			}
		}
		break;
	case ACT_RANGE_ATTACK2:
		// grunt is going to a secondary long range attack. This may be a thrown 
		// grenade or fired grenade, we must determine which and pick proper sequence
		if ( pev->weapons & SO_GRUNT_HANDGRENADE )
		{
			// get toss anim
			iSequence = LookupSequence( "throwgrenade" );
		}
		else
		{
			// get launch anim
			iSequence = LookupSequence( "launchgrenade" );
		}
		break;
	case ACT_RUN:
		if ( pev->health <= SO_GRUNT_LIMP_HEALTH )
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
		if ( pev->health <= SO_GRUNT_LIMP_HEALTH )
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
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CSOGrunt :: GetSchedule ( void )
{
	// grunts place HIGH priority on running away from danger sounds.
	if ( HasConditions(bits_COND_HEAR_SOUND) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound)
		{
			if (pSound->m_iType & bits_SOUND_DANGER)
			{
				// dangerous sound nearby!
				
				//!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
				// and the grunt should find cover from the blast
				// good place for "SHIT!" or some other colorful verbal indicator of dismay.
				// It's not safe to play a verbal order here "Scatter", etc cause 
				// this may only affect a single individual in a squad. 
				
				if (FOkToSpeak())
				{
					SENTENCEG_PlayRndSz( ENT(pev), "FG_GREN", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
			/*
			if (!HasConditions( bits_COND_SEE_ENEMY ) && ( pSound->m_iType & (bits_SOUND_PLAYER | bits_SOUND_COMBAT) ))
			{
				MakeIdealYaw( pSound->m_vecOrigin );
			}
			*/
		}
	}
	// flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling. 
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if (pev->flags & FL_ONGROUND)
		{
			// just landed
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType ( SCHED_SPEC_OPS_REPEL_LAND );
		}
		else
		{
			// repel down a rope, 
			if ( m_MonsterState == MONSTERSTATE_COMBAT )
				return GetScheduleOfType ( SCHED_SPEC_OPS_REPEL_ATTACK );
			else
				return GetScheduleOfType ( SCHED_SPEC_OPS_REPEL );
		}
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		PlaySentence( "FG_KILL", 4, VOL_NORM, ATTN_NORM );
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

// new enemy
			if ( HasConditions(bits_COND_NEW_ENEMY) )
			{
				if ( InSquad() )
				{
					MySquadLeader()->m_fEnemyEluded = FALSE;

					if ( !IsLeader() )
					{
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_SPEC_OPS_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_SPEC_OPS_ESTABLISH_LINE_OF_FIRE );
						}
					}
					else 
					{
						//!!!KELLY - the leader of a squad of grunts has just seen the player or a 
						// monster and has made it the squad's enemy. You
						// can check pev->flags for FL_CLIENT to determine whether this is the player
						// or a monster. He's going to immediately start
						// firing, though. If you'd like, we can make an alternate "first sight" 
						// schedule where the leader plays a handsign anim
						// that gives us enough time to hear a short sentence or spoken command
						// before he starts pluggin away.
						if (FOkToSpeak())// && RANDOM_LONG(0,1))
						{
							if ((m_hEnemy != NULL) &&
									(m_hEnemy->Classify() != CLASS_PLAYER_ALLY) && 
									(m_hEnemy->Classify() != CLASS_HUMAN_MILITARY) && 
									(m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE) && 
									(m_hEnemy->Classify() != CLASS_MACHINE))
								// monster
								SENTENCEG_PlayRndSz( ENT(pev), "FG_ALERT", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
							else
								// player
								SENTENCEG_PlayRndSz( ENT(pev), "FG_ATTACK", VOL_NORM, ATTN_NORM, 0, m_voicePitch);

							JustSpoke();
						}
						
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_SPEC_OPS_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_SPEC_OPS_ESTABLISH_LINE_OF_FIRE );
						}
					}
				}
			}
// no ammo
			else if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				return GetScheduleOfType ( SCHED_SPEC_OPS_COVER_AND_RELOAD );
			}
			
// damaged just a little
			else if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
			{
				// if hurt:
				// 90% chance of taking cover
				// 10% chance of flinch.

				int iPercent = RANDOM_LONG(0,99);

				if ( iPercent <= 90 && m_hEnemy != NULL )
				{
					if (FOkToSpeak()) // && RANDOM_LONG(0,1))
					{
						//SENTENCEG_PlayRndSz( ENT(pev), "HG_COVER", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
						m_iSentence = SO_GRUNT_SENT_COVER;
						//JustSpoke();
					}
					// only try to take cover if we actually have an enemy!

					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				else
				{
					return GetScheduleOfType( SCHED_SMALL_FLINCH );
				}
			}
// can kick
			else if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
			}
// can grenade launch

			else if ( FBitSet( pev->weapons, SO_GRUNT_GRENADELAUNCHER) && HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_FGRUNT_GRENADE ) )
			{
				// shoot a grenade if you can
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
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
						return GetScheduleOfType ( SCHED_SPEC_OPS_FOUND_ENEMY );
					}
				}
				if ( OccupySlot ( bits_SLOTS_FGRUNT_ENGAGE ) )
				{
					// try to take an available ENGAGE slot
					return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
				}
				else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_FGRUNT_GRENADE ) )
				{
					// throw a grenade if can and no engage slots are available
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
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
				if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_FGRUNT_GRENADE ) )
				{
					//!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
					if (FOkToSpeak())
					{
						SENTENCEG_PlayRndSz( ENT(pev), "FG_THROW", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
				}
				else if ( OccupySlot( bits_SLOTS_FGRUNT_ENGAGE ) )
				{
					return GetScheduleOfType( SCHED_SPEC_OPS_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					//!!!KELLY - grunt is going to stay put for a couple seconds to see if
					// the enemy wanders back out into the open, or approaches the
					// grunt's covered position. Good place for a taunt, I guess?
					if (FOkToSpeak() && RANDOM_LONG(0,1))
					{
						SENTENCEG_PlayRndSz( ENT(pev), "FG_TAUNT", VOL_NORM, ATTN_NORM, 0, m_voicePitch);
						JustSpoke();
					}
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}
			
			if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_SPEC_OPS_ESTABLISH_LINE_OF_FIRE );
			}
		}
		break;
	case MONSTERSTATE_ALERT:	
	case MONSTERSTATE_IDLE:

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
	
	return CRCAllyMonster :: GetSchedule();
}
MONSTERSTATE CSOGrunt :: GetIdealState ( void )
{
	return CRCAllyMonster::GetIdealState();
}
void CSOGrunt::DeclineFollowing( void )
{
	PlaySentence( "FG_STOP", 2, VOL_NORM, ATTN_NORM );
}
//=========================================================
// CSOGruntRepel - when triggered, spawns a
// repelling down a line.
//=========================================================

class CSOGruntRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( monster_specops_repel, CSOGruntRepel );

void CSOGruntRepel::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;

	SetUse( RepelUse );
}

void CSOGruntRepel::Precache( void )
{
	UTIL_PrecacheOther( "monster_specops_grunt" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/rope.spr" );
}

void CSOGruntRepel::RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096.0), dont_ignore_monsters, ENT(pev), &tr);

	CBaseEntity *pEntity = Create( "monster_specops_grunt", pev->origin, pev->angles );
	CBaseMonster *pGrunt = pEntity->MyMonsterPointer( );
	pGrunt->pev->movetype = MOVETYPE_FLY;
	pGrunt->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
	pGrunt->SetActivity( ACT_GLIDE );
	pGrunt->m_vecLastPosition = tr.vecEndPos;

	CBeam *pBeam = CBeam::BeamCreate( "sprites/rope.spr", 10 );
	pBeam->PointEntInit( pev->origin + Vector(0,0,112), pGrunt->entindex() );
	pBeam->SetFlags( BEAM_FSOLID );
	pBeam->SetColor( 255, 255, 255 );
	pBeam->SetThink( SUB_Remove );
	pBeam->pev->nextthink = gpGlobals->time + -4096.0 * tr.flFraction / pGrunt->pev->velocity.z + 0.5;

	UTIL_Remove( this );
}