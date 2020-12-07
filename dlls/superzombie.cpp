//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//
//=========================================================
//=========================================================
#define SZOMBIE_FLINCH_DELAY			4		// at most one flinch every n secs

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"animation.h"
#include	"squadmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"
#include	"customentity.h"
#include	"game.h"

#define		SZOMBIE_SHOTGUN_CLIP_SIZE				4 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
#define		SZOMBIE_M249_CLIP_SIZE				100 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		SZOMBIE_AE_GRENADE		( 2 )
#define		SZOMBIE_AE_SHOOT		( 3 )
#define		SZOMBIE_AE_BURST		( 4 )
#define		SZOMBIE_AE_RELOAD		( 5 )
#define		SZOMBIE_AE_STAB			( 6 )
#define		SZOMBIE_AE_FIRE			( 7 )
#define		SZOMBIE_AE_JUMP			( 8 )

#define		SZOMBIE_WEAPON_SHOTGUN	1
#define		SZOMBIE_WEAPON_M249		2
#define		SZOMBIE_WEAPON_BERETTA	3

#define	GUN_GROUP	1
#define GUN_SHOTGUN	0
#define GUN_M249	1
#define GUN_BERETTA	2
#define	GUN_HOLSTER	3

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_SZOMBIE_CHECK_FIRE = LAST_COMMON_TASK + 1,
	TASK_SZOMBIE_FALL_TO_GROUND, // falling and waiting to hit ground
};
//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_SZOMBIE_NOFIRE	( bits_COND_SPECIAL4 )
#define bits_MEMORY_SZOMBIE_BADJUMP	( bits_MEMORY_CUSTOM2 )
//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_SZOMBIE_COVER_AND_RELOAD = LAST_COMMON_SCHEDULE + 1,
	SCHED_SZOMBIE_ESTABLISH_LINE_OF_FIRE,// move to a location to set up an attack against the enemy. (usually when a friendly is in the way).
	SCHED_SZOMBIE_SWEEP,
	SCHED_SZOMBIE_WAIT_FACE_ENEMY,
	SCHED_SZOMBIE_TAKECOVER_FAILED,// special schedule type that forces analysis of conditions and picks the best possible schedule to recover from this type of failure.
	SCHED_SZOMBIE_ELOF_FAIL,
	SCHED_SZOMBIE_SUPPRESS,
	SCHED_SZOMBIE_JUMP,	// fly through the air
	SCHED_SZOMBIE_JUMP_ATTACK,	// fly through the air
	SCHED_SZOMBIE_JUMP_LAND, // hit and run away
};

#define		SZOMBIE_LIMP_HEALTH	75
#define		SZOMBIE_MAX_BEAMS		32
class CSuperZombie : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	void CheckAmmo ( void );
	void Shotgun( void );
	void M249( void );
	void Beretta( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void StartTask( Task_t *pTask );
	void PrescheduleThink ( void );
	BOOL FCanCheckAttacks ( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	BOOL CheckMeleeAttack2 ( float flDot, float flDist );
	void DeclineFollowing( void );
	void SetActivity ( Activity NewActivity );
	void RunTask ( Task_t *pTask );
	// Override these to set behavior
	CBaseEntity	*Kick( void );
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	int IRelationship ( CBaseEntity *pTarget );
	void DeathSound( void );
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );

	void Killed( entvars_t *pevAttacker, int iGib );
	int IgnoreConditions ( void );
	Vector GetGunPosition( void );

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float	m_flLinkToggle;// how much pain has the player inflicted on me?

	float m_flNextFlinch;

	float m_flNextJump;
	Vector m_vecJumpVelocity;
	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;
	int		m_iShotgunShell;
	int		m_iM249Shell;
	int		m_iM249Link;
	int		m_iBrassShell;
	int		m_iBodyGibs;
	int		m_iExplode;
	int		m_iSpriteTexture;
	int		m_cClipSize;
	int		m_iMetalGib;

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_superzombie, CSuperZombie );

TYPEDESCRIPTION	CSuperZombie::m_SaveData[] = 
{
	DEFINE_FIELD( CSuperZombie, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CSuperZombie, m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( CSuperZombie, m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSuperZombie, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CSuperZombie, m_iMetalGib, FIELD_INTEGER ),
	DEFINE_FIELD( CSuperZombie, m_flNextJump, FIELD_TIME ),
	DEFINE_FIELD( CSuperZombie, m_vecJumpVelocity, FIELD_VECTOR ),

};

IMPLEMENT_SAVERESTORE( CSuperZombie, CSquadMonster );
//=========================================================
//Overriden for SuperZombie, since he hates Maria the most.
//=========================================================
int CSuperZombie::IRelationship ( CBaseEntity *pTarget )
{
	if ( FClassnameIs( pTarget->pev, "monster_maria" ) )
	{
		return R_NM;
	}

	return CSquadMonster::IRelationship( pTarget );
}
int CSuperZombie::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ( m_hEnemy != NULL )
	{
		iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
	}


	return iIgnore;
	
}
//=========================================================
//=========================================================
CBaseEntity *CSuperZombie :: Kick( void )
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
// SuperZombie reload schedule
//=========================================================
Task_t	tlSuperZombieHideReload[] =
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

Schedule_t slSuperZombieHideReload[] = 
{
	{
		tlSuperZombieHideReload,
		ARRAYSIZE ( tlSuperZombieHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"SuperZombieHideReload"
	}
};
//=========================================================
// Victory dance!
//=========================================================
Task_t	tlSuperZombieVictoryDance[] =
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

Schedule_t	slSuperZombieVictoryDance[] =
{
	{ 
		tlSuperZombieVictoryDance,
		ARRAYSIZE ( tlSuperZombieVictoryDance ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE,
		0,
		"SuperZombieVictoryDance"
	},
};
//=========================================================
// SuperZombieFail
//=========================================================
Task_t	tlSuperZombieFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slSuperZombieFail[] =
{
	{
		tlSuperZombieFail,
		ARRAYSIZE ( tlSuperZombieFail ),
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 |
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"SuperZombie Fail"
	},
};

//=========================================================
// SuperZombie Combat Fail
//=========================================================
Task_t	tlSuperZombieCombatFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT_FACE_ENEMY,		(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slSuperZombieCombatFail[] =
{
	{
		tlSuperZombieCombatFail,
		ARRAYSIZE ( tlSuperZombieCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2,
		0,
		"SuperZombie Combat Fail"
	},
};
//=========================================================
// Establish line of fire - move to a position that allows
// the SuperZombie to attack.
//=========================================================
Task_t tlSuperZombieEstablishLineOfFire[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_SZOMBIE_ELOF_FAIL	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0						},
	{ TASK_RUN_PATH,			(float)0						},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0						},
};

Schedule_t slSuperZombieEstablishLineOfFire[] =
{
	{ 
		tlSuperZombieEstablishLineOfFire,
		ARRAYSIZE ( tlSuperZombieEstablishLineOfFire ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"SuperZombieEstablishLineOfFire"
	},
};
//=========================================================
// SuperZombieCombatFace Schedule
//=========================================================
Task_t	tlSuperZombieCombatFace1[] =
{
	{ TASK_STOP_MOVING,				0							},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_WAIT,					(float)1.5					},
	{ TASK_SET_SCHEDULE,			(float)SCHED_SZOMBIE_SWEEP	},
};

Schedule_t	slSuperZombieCombatFace[] =
{
	{ 
		tlSuperZombieCombatFace1,
		ARRAYSIZE ( tlSuperZombieCombatFace1 ), 
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
// empty or SuperZombie gets hurt.
//=========================================================
Task_t	tlSuperZombieSuppress[] =
{
	{ TASK_STOP_MOVING,			0							},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
	{ TASK_FACE_ENEMY,			(float)0					},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0					},
	{ TASK_RANGE_ATTACK1,		(float)0					},
};

Schedule_t	slSuperZombieSuppress[] =
{
	{ 
		tlSuperZombieSuppress,
		ARRAYSIZE ( tlSuperZombieSuppress ), 
		bits_COND_ENEMY_DEAD		|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND		|
		bits_COND_SZOMBIE_NOFIRE		|
		bits_COND_NO_AMMO_LOADED,

		bits_SOUND_DANGER,
		"Suppress"
	},
};
//=========================================================
// SuperZombie wait in cover - we don't allow danger or the ability
// to attack to break a Superzombie's run to cover schedule, but
// when a superzombie is in cover, we do want them to attack if they can.
//=========================================================
Task_t	tlSuperZombieWaitInCover[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_WAIT_FACE_ENEMY,			(float)1					},
};

Schedule_t	slSuperZombieWaitInCover[] =
{
	{ 
		tlSuperZombieWaitInCover,
		ARRAYSIZE ( tlSuperZombieWaitInCover ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_HEAR_SOUND		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2,

		bits_SOUND_DANGER,
		"SuperZombieWaitInCover"
	},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t	tlSuperZombieTakeCover1[] =
{
	{ TASK_STOP_MOVING,				(float)0							},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_SZOMBIE_TAKECOVER_FAILED	},
	{ TASK_WAIT,					(float)0.2							},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0							},
	{ TASK_RUN_PATH,				(float)0							},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0							},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER			},
	{ TASK_SET_SCHEDULE,			(float)SCHED_SZOMBIE_WAIT_FACE_ENEMY	},
};

Schedule_t	slSuperZombieTakeCover[] =
{
	{ 
		tlSuperZombieTakeCover1,
		ARRAYSIZE ( tlSuperZombieTakeCover1 ), 
		0,
		0,
		"TakeCover"
	},
};
//=========================================================
// hide from the loudest sound source (to run from grenade)
//=========================================================
Task_t	tlSuperZombieTakeCoverFromBestSound[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_COWER			},// duck and cover if cannot move from explosion
	{ TASK_STOP_MOVING,					(float)0					},
	{ TASK_FIND_COVER_FROM_BEST_SOUND,	(float)0					},
	{ TASK_RUN_PATH,					(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0					},
	{ TASK_REMEMBER,					(float)bits_MEMORY_INCOVER	},
	{ TASK_TURN_LEFT,					(float)179					},
};

Schedule_t	slSuperZombieTakeCoverFromBestSound[] =
{
	{ 
		tlSuperZombieTakeCoverFromBestSound,
		ARRAYSIZE ( tlSuperZombieTakeCoverFromBestSound ), 
		0,
		0,
		"SuperZombieTakeCoverFromBestSound"
	},
};
//=========================================================
// Do a turning sweep of the area
//=========================================================
Task_t	tlSuperZombieSweep[] =
{
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
	{ TASK_TURN_LEFT,			(float)179	},
	{ TASK_WAIT,				(float)1	},
};

Schedule_t	slSuperZombieSweep[] =
{
	{ 
		tlSuperZombieSweep,
		ARRAYSIZE ( tlSuperZombieSweep ), 
		
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_RANGE_ATTACK2	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_WORLD		|// sound flags
		bits_SOUND_DANGER		|
		bits_SOUND_PLAYER,

		"SuperZombie Sweep"
	},
};

//=========================================================
// primary range attack. Overriden because base class stops attacking when the enemy is occluded.
// SuperZombie's grenade toss requires the enemy be occluded.
//=========================================================
Task_t	tlSuperZombieRangeAttack1[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,(float)ACT_IDLE_ANGRY  },
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
	{ TASK_FACE_ENEMY,				(float)0		},
	{ TASK_SZOMBIE_CHECK_FIRE,	(float)0		},
	{ TASK_RANGE_ATTACK1,			(float)0		},
};

Schedule_t	slSuperZombieRangeAttack1[] =
{
	{ 
		tlSuperZombieRangeAttack1,
		ARRAYSIZE ( tlSuperZombieRangeAttack1 ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED	|
		bits_COND_SZOMBIE_NOFIRE		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER,
		"Range Attack1"
	},
};
//=========================================================
// Jumping Schedules
//=========================================================
Task_t	tlSuperZombieJump[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_HOP	},
	{ TASK_SET_SCHEDULE,		(float)SCHED_SZOMBIE_JUMP_ATTACK },
};

Schedule_t	slSuperZombieJump[] =
{
	{ 
		tlSuperZombieJump,
		ARRAYSIZE ( tlSuperZombieJump ), 
		0, 
		0, 
		"AssassinJump"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlSuperZombieJumpAttack[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_SZOMBIE_JUMP_LAND	},
	{ TASK_SZOMBIE_FALL_TO_GROUND, (float)0		},
};


Schedule_t	slSuperZombieJumpAttack[] =
{
	{ 
		tlSuperZombieJumpAttack,
		ARRAYSIZE ( tlSuperZombieJumpAttack ), 
		0, 
		0,
		"AssassinJumpAttack"
	},
};


//=========================================================
// repel 
//=========================================================
Task_t	tlSuperZombieJumpLand[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RANGE_ATTACK1	},
	{ TASK_SET_ACTIVITY,			(float)ACT_IDLE				},
	{ TASK_REMEMBER,				(float)bits_MEMORY_SZOMBIE_BADJUMP	},
	{ TASK_FIND_NODE_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_FORGET,					(float)bits_MEMORY_SZOMBIE_BADJUMP	},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RANGE_ATTACK1	},
};

Schedule_t	slSuperZombieJumpLand[] =
{
	{ 
		tlSuperZombieJumpLand,
		ARRAYSIZE ( tlSuperZombieJumpLand ), 
		0, 
		0,
		"AssassinJumpLand"
	},
};
DEFINE_CUSTOM_SCHEDULES( CSuperZombie )
{
	slSuperZombieHideReload,
	slSuperZombieVictoryDance,
	slSuperZombieFail,
	slSuperZombieCombatFail,
	slSuperZombieEstablishLineOfFire,
	slSuperZombieCombatFace,
	slSuperZombieSuppress,
	slSuperZombieWaitInCover,
	slSuperZombieTakeCover,
	slSuperZombieTakeCoverFromBestSound,
	slSuperZombieSweep,
	slSuperZombieRangeAttack1,
	slSuperZombieJumpAttack,
	slSuperZombieJumpLand,
	slSuperZombieEstablishLineOfFire,
};


IMPLEMENT_CUSTOM_SCHEDULES( CSuperZombie, CSquadMonster );

//=========================================================
// RunTask 
//=========================================================
void CSuperZombie :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_SZOMBIE_FALL_TO_GROUND:
		MakeIdealYaw( m_vecEnemyLKP );
		ChangeYaw( pev->yaw_speed );

		if (m_fSequenceFinished)
		{
			pev->sequence = LookupSequence( "fly" );

			ResetSequenceInfo( );
			SetYawSpeed();
		}
		if (pev->flags & FL_ONGROUND)
		{
			// ALERT( at_console, "on ground\n");
			TaskComplete( );
		}
		break;
	default: 
		CSquadMonster :: RunTask ( pTask );
		break;
	}
}

void CSuperZombie :: StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_SZOMBIE_CHECK_FIRE:
		if ( !NoFriendlyFire() )
		{
			SetConditions( bits_COND_SZOMBIE_NOFIRE );
		}
		TaskComplete();
		break;
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// superzombie no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CSquadMonster ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;

	default: 
		CSquadMonster :: StartTask( pTask );
		break;
	}
}
//=========================================================
// PrescheduleThink - this function runs after conditions
// are collected and before scheduling code is run.
//=========================================================
void CSuperZombie :: PrescheduleThink ( void )
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
// FCanCheckAttacks - this is overridden for SuperZombie
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
BOOL CSuperZombie :: FCanCheckAttacks ( void )
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
int CSuperZombie :: ISoundMask ( void) 
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
int	CSuperZombie :: Classify ( void )
{
		return	CLASS_ALIEN_MONSTER;
}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSuperZombie :: SetYawSpeed ( void )
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
void CSuperZombie :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity)
	{
	case ACT_RANGE_ATTACK1:
		// SuperZombie is either shooting standing or shooting crouched
		if ( pev->weapons == SZOMBIE_WEAPON_SHOTGUN )
		{
			iSequence = LookupSequence( "shotgun_shoot" );
		}
		else if ( pev->weapons == SZOMBIE_WEAPON_M249 )
		{
			// get crouching shoot
			iSequence = LookupSequence( "m249_shoot" );
		}
		else
		{
			iSequence = LookupSequence( "beretta_shoot" );
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
BOOL CSuperZombie :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() )
	{
		TraceResult	tr;

		if ( flDist <= 512 )
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
BOOL CSuperZombie :: CheckMeleeAttack1 ( float flDot, float flDist )
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
// CheckMeleeAttack1 - jump like crazy if the enemy gets too close. 
//=========================================================
BOOL CSuperZombie :: CheckMeleeAttack2 ( float flDot, float flDist )
{
	if ( m_flNextJump < gpGlobals->time && (flDist <= 128 || HasMemory( bits_MEMORY_SZOMBIE_BADJUMP )) && m_hEnemy != NULL )
	{
		TraceResult	tr;

		Vector vecDest = pev->origin + Vector( RANDOM_FLOAT( -64, 64), RANDOM_FLOAT( -64, 64 ), 160 );

		UTIL_TraceHull( pev->origin + Vector( 0, 0, 36 ), vecDest + Vector( 0, 0, 36 ), dont_ignore_monsters, human_hull, ENT(pev), &tr);

		if ( tr.fStartSolid || tr.flFraction < 1.0)
		{
			return FALSE;
		}

		float flGravity = g_psv_gravity->value;

		float time = sqrt( 160 / (0.5 * flGravity));
		float speed = flGravity * time / 160;
		m_vecJumpVelocity = (vecDest - pev->origin) * speed;

		return TRUE;
	}
	return FALSE;
}
//=========================================================
// Shoot
//=========================================================
void CSuperZombie :: Beretta ( void )
{
	if (m_hEnemy == NULL || !NoFriendlyFire() )
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/beretta_fire1.wav", 1, ATTN_NORM );

	UTIL_MakeVectors ( pev->angles );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL); 
	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_6DEGREES, 1024, BULLET_PLAYER_9MM ); // shoot +-5 degrees

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

//	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}
//=========================================================
// Shotgun - Superzombie' main weapon
//=========================================================
void CSuperZombie :: Shotgun ( void )
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
	EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iShotgunShell, TE_BOUNCE_SHOTSHELL); 
	FireBullets(gSkillData.szombieShotgunPellets, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0 ); // shoot +-7.5 degrees

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM );

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

//	m_cAmmoLoaded--;// take away a bullet!

	m_flNextAttack = gpGlobals->time + ( 35/27 );
}
//=========================================================
// Shoot
//=========================================================
void CSuperZombie :: M249 ( void )
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

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_9DEGREES, 2048, BULLET_PLAYER_556 ); // shoot +-5 degrees

	pev->effects |= EF_MUZZLEFLASH;
	
	WeaponFlash ( vecShootOrigin );

//	m_cAmmoLoaded--;// take away a bullet!

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CSuperZombie :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case SZOMBIE_AE_SHOOT:
		Shotgun();
		break;

	case SZOMBIE_AE_FIRE:
		Beretta();
		break;

	case SZOMBIE_AE_BURST:
		M249();
		break;

	case SZOMBIE_AE_STAB:
	{
		float iPitch = RANDOM_FLOAT( 90, 110 );
		CBaseEntity *pHurt = Kick();
		if ( pHurt )
		{
			UTIL_MakeAimVectors( pev->angles );

			// slit sound

			switch ( RANDOM_LONG( 0, 2 ) )
			{
			case 0:
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "szombie/spike_hit1.wav", 1, ATTN_NORM, 0, iPitch );	
				break;
			case 1:
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "szombie/spike_hit2.wav", 1, ATTN_NORM, 0, iPitch );	
				break;
			case 2:
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "szombie/spike_hit3.wav", 1, ATTN_NORM, 0, iPitch );	
				break;
			}

			// SOUND HERE!
			UTIL_MakeVectors( pev->angles );
			pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 100;
			pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 100;
			pHurt->TakeDamage( pev, pev, gSkillData.szombieDmgStab + 100, DMG_CLUB );
		}
		else
		{
			switch ( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "szombie/spike_miss1.wav", 1, ATTN_NORM, 0, iPitch );	
				break;
			case 1:
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "szombie/spike_miss2.wav", 1, ATTN_NORM, 0, iPitch );	
				break;
			}
		}
	}
	break;
	case SZOMBIE_AE_RELOAD:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions(bits_COND_NO_AMMO_LOADED);

	case SZOMBIE_AE_JUMP:
		{
			// ALERT( at_console, "jumping");
			UTIL_MakeAimVectors( pev->angles );
			pev->movetype = MOVETYPE_TOSS;
			pev->flags &= ~FL_ONGROUND;
			pev->velocity = m_vecJumpVelocity;
			m_flNextJump = gpGlobals->time + 3.0;
		}
		return;

	default:
		CSquadMonster::HandleAnimEvent( pEvent );
	}
}
//=========================================================
// GetGunPosition	return the end of the barrel
//=========================================================
Vector CSuperZombie :: GetGunPosition( )
{
		return pev->origin + Vector( 0, 0, 60 );
}
//=========================================================
// Spawn
//=========================================================
void CSuperZombie :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/superzombie.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= 0;
	pev->health			= gSkillData.szombieHealth;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	if ( pev->weapons == 0 )
	{
		pev->weapons = RANDOM_LONG(1,3);
	}

	if ( pev->body == -1 )
		pev->body = RANDOM_LONG(0,1);

	if ( pev->weapons == SZOMBIE_WEAPON_SHOTGUN )
	{
		SetBodygroup( GUN_GROUP, GUN_SHOTGUN );
		m_cClipSize	= SZOMBIE_SHOTGUN_CLIP_SIZE;
	}
	if ( pev->weapons == SZOMBIE_WEAPON_M249 )
	{
		SetBodygroup( GUN_GROUP, GUN_M249 );
		m_cClipSize	= SZOMBIE_M249_CLIP_SIZE;
	}
	if ( pev->weapons == SZOMBIE_WEAPON_BERETTA )
	{
		SetBodygroup( GUN_GROUP, GUN_BERETTA );
		m_cClipSize	= 7;
	}

	m_cAmmoLoaded		= m_cClipSize;

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_SQUAD | bits_CAP_MELEE_ATTACK1;
	m_fEnemyEluded		= FALSE;

	MonsterInit();
	StartMonster();
}
//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSuperZombie :: Precache()
{
	PRECACHE_MODEL("models/superzombie.mdl");

	PRECACHE_SOUND("weapons/sbarrel1.wav" );
	PRECACHE_SOUND("weapons/beretta_fire1.wav" );
	PRECACHE_SOUND("weapons/saw_fire1.wav" );
	PRECACHE_SOUND("weapons/saw_fire2.wav" );
	PRECACHE_SOUND("weapons/saw_fire3.wav" );

	PRECACHE_SOUND("szombie/sz_pain1.wav");
	PRECACHE_SOUND("szombie/sz_pain2.wav");
	PRECACHE_SOUND("szombie/sz_pain3.wav");
	PRECACHE_SOUND("szombie/sz_pain4.wav");

	PRECACHE_SOUND("szombie/sz_alert1.wav");
	PRECACHE_SOUND("szombie/sz_alert2.wav");
	PRECACHE_SOUND("szombie/sz_alert3.wav");

	PRECACHE_SOUND("szombie/sz_idle1.wav");
	PRECACHE_SOUND("szombie/sz_idle2.wav");
	PRECACHE_SOUND("szombie/sz_idle3.wav");
	PRECACHE_SOUND("szombie/sz_idle4.wav");

	PRECACHE_SOUND("szombie/spike_hit1.wav");
	PRECACHE_SOUND("szombie/spike_hit2.wav");
	PRECACHE_SOUND("szombie/spike_hit3.wav");

	PRECACHE_SOUND("szombie/spike_miss1.wav");
	PRECACHE_SOUND("szombie/spike_miss2.wav");

	PRECACHE_SOUND("szombie/run1.wav");
	PRECACHE_SOUND("szombie/run2.wav");

	m_iBrassShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell
	m_iShotgunShell = PRECACHE_MODEL ("models/shotgunshell.mdl");
	m_iM249Shell = PRECACHE_MODEL ("models/saw_shell.mdl");// saw shell
	m_iM249Link = PRECACHE_MODEL ("models/saw_link.mdl");// saw link

	CSquadMonster::Precache();
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
// CheckAmmo - overridden for the SZombie because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CSuperZombie :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	{
		SetConditions(bits_COND_NO_AMMO_LOADED);
	}
}
//=========================================================
// PainSound
//=========================================================
void CSuperZombie :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,3))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain3.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain4.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CSuperZombie :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,3))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain3.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_pain4.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
	}
}
void CSuperZombie :: AlertSound( void )
{
	switch (RANDOM_LONG(0,2))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_alert1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_alert2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_alert3.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
	}
}
void CSuperZombie :: IdleSound( void )
{
	switch (RANDOM_LONG(0,3))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_idle1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_idle2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_idle3.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
		case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "szombie/sz_idle4.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
	}
}
void CSuperZombie::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


void CSuperZombie::Killed( entvars_t *pevAttacker, int iGib )
{

	if ( GetBodygroup( GUN_GROUP ) != GUN_HOLSTER )
	{// drop the gun!
		Vector vecGunPos;
		Vector vecGunAngles;

		SetBodygroup( GUN_GROUP , GUN_HOLSTER );

		GetAttachment( 0, vecGunPos, vecGunAngles );

	if ( pev->weapons == SZOMBIE_WEAPON_SHOTGUN )
		CBaseEntity *pGun = DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
	else if ( pev->weapons == SZOMBIE_WEAPON_M249 )
		CBaseEntity *pGun = DropItem( "weapon_m249", vecGunPos, vecGunAngles );
	else
		CBaseEntity *pGun = DropItem( "weapon_beretta", vecGunPos, vecGunAngles );
	}

	SetUse( NULL );	
	CSquadMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CSuperZombie :: GetScheduleOfType ( int Type )
{
	switch( Type )
	{
	case SCHED_SZOMBIE_COVER_AND_RELOAD:
		{
			return &slSuperZombieHideReload[ 0 ];
		}
	case SCHED_TAKE_COVER_FROM_ENEMY:
		{
			return &slSuperZombieTakeCover[ 0 ];
		}
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		{
			return &slSuperZombieTakeCoverFromBestSound[ 0 ];
		}
	case SCHED_SZOMBIE_TAKECOVER_FAILED:
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
	case SCHED_SZOMBIE_ELOF_FAIL:
		{
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		}
	case SCHED_SZOMBIE_ESTABLISH_LINE_OF_FIRE:
		{
			return &slSuperZombieEstablishLineOfFire[ 0 ];
		}
	case SCHED_RANGE_ATTACK1:
		{
			return &slSuperZombieRangeAttack1[ 0 ];
		}
	case SCHED_COMBAT_FACE:
		{
			return &slSuperZombieCombatFace[ 0 ];
		}
	case SCHED_SZOMBIE_WAIT_FACE_ENEMY:
		{
			return &slSuperZombieWaitInCover[ 0 ];
		}
	case SCHED_SZOMBIE_SWEEP:
		{
			return &slSuperZombieSweep[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			if ( InSquad() )
			{
				if ( !IsLeader() )
				{
					return &slSuperZombieFail[ 0 ];
				}
			}
			return &slSuperZombieVictoryDance[ 0 ];
		}
	case SCHED_SZOMBIE_SUPPRESS:
		{
			return &slSuperZombieSuppress[ 0 ];
		}
	case SCHED_FAIL:
		{
			if ( m_hEnemy != NULL )
			{
				// SZombie has an enemy, so pick a different default fail schedule most likely to help recover.
				return &slSuperZombieCombatFail[ 0 ];
			}

			return &slSuperZombieFail[ 0 ];
		}
	case SCHED_MELEE_ATTACK2:
		if (pev->flags & FL_ONGROUND)
		{
			if (m_flNextJump > gpGlobals->time)
			{
				// can't jump yet, go ahead and fail
				return slSuperZombieFail;
			}
			else
			{
				return slSuperZombieJump;
			}
		}
		else
		{
			return slSuperZombieJumpAttack;
		}
		case SCHED_SZOMBIE_JUMP:
		case SCHED_SZOMBIE_JUMP_ATTACK:
			return slSuperZombieJumpAttack;
		case SCHED_SZOMBIE_JUMP_LAND:
			return slSuperZombieJumpLand;
	}
	return CSquadMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CSuperZombie :: GetSchedule ( void )
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
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
			}
		}
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

			// flying?
			if ( pev->movetype == MOVETYPE_TOSS)
			{
				if (pev->flags & FL_ONGROUND)
				{
					pev->movetype = MOVETYPE_STEP;
					return GetScheduleOfType ( SCHED_SZOMBIE_JUMP_LAND );
				}
				else
				{
					return GetScheduleOfType ( SCHED_SZOMBIE_JUMP );
				}
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
							return GetScheduleOfType ( SCHED_SZOMBIE_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_SZOMBIE_ESTABLISH_LINE_OF_FIRE );
						}
					}
					else 
					{
						
						if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
						{
							return GetScheduleOfType ( SCHED_SZOMBIE_SUPPRESS );
						}
						else
						{
							return GetScheduleOfType ( SCHED_SZOMBIE_ESTABLISH_LINE_OF_FIRE );
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
					return GetScheduleOfType ( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
			else if ( HasConditions ( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
			}
// can shoot
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) && !HasConditions ( bits_COND_NO_AMMO_LOADED ) && ( m_hEnemy->Classify() != CLASS_ALIEN_PREY ) ) // Headcrabs are a waste of ammo, zap them
			{
				if ( OccupySlot ( bits_SLOTS_HGRUNT_ENGAGE ) )
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
				if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				{
					return GetScheduleOfType( SCHED_SZOMBIE_ESTABLISH_LINE_OF_FIRE );
				}
				else
				{
					return GetScheduleOfType( SCHED_STANDOFF );
				}
			}
			// no ammo
			else if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) )
			{
				return GetScheduleOfType ( SCHED_SZOMBIE_COVER_AND_RELOAD );
			}
			else if ( HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ) )// Zap the guy if we are out of ammo, it's powerfull enough.
			{
				// zap the enemy
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
			if ( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_SZOMBIE_ESTABLISH_LINE_OF_FIRE );
			}
		}
		break;
		case MONSTERSTATE_IDLE:
		case MONSTERSTATE_ALERT:
			{
				if ( HasConditions ( bits_COND_HEAR_SOUND ))
				{
					CSound *pSound;
					pSound = PBestSound();

					ASSERT( pSound != NULL );
					if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
					{
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
					}
					if ( pSound && (pSound->m_iType & bits_SOUND_COMBAT) )
					{
						return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
					}
				}
			}
			break;
	}
	return CSquadMonster::GetSchedule();
}

MONSTERSTATE CSuperZombie :: GetIdealState ( void )
{
	return CSquadMonster::GetIdealState();
}