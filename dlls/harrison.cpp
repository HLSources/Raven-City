//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//
//=========================================================
// Harrison - By Highlander.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"talkmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"
#define		HARRISON_CLIP_SIZE				50 // how many bullets in a clip? - NOTE: 3 round burst sound, so keep as 3 * x!
//=========================================================
// Monster's Anim Events Go Here
//=========================================================
// first flag is harrison dying for scripted sequences?
#define		HARRISON_AE_DRAW		( 2 )
#define		HARRISON_AE_SHOOT		( 3 )
#define		HARRISON_AE_HOLSTER		( 4 )
#define		HARRISON_AE_RELOAD		( 5 )
#define		HARRISON_AE_KICK		( 6 )

#define	GUN_GROUP	1
#define	GUN_HOLSTER	0
#define GUN_DRAWN	1

enum
{
	SCHED_HARRISON_COVER_AND_RELOAD = LAST_TALKMONSTER_SCHEDULE + 1,
};

class CHarrison : public CTalkMonster
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
	void KeyValue( KeyValueData *pkvd );
	void CheckAmmo ( void );
	void RunTask( Task_t *pTask );
	void StartTask( Task_t *pTask );
	virtual int	ObjectCaps( void ) { return CTalkMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	CBaseEntity	*Kick( void );
	void DeclineFollowing( void );

	// Override these to set behavior
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule ( void );
	MONSTERSTATE GetIdealState ( void );

	void DeathSound( void );
	void PainSound( void );
	
	void TalkInit( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float	m_painTime;
	float	m_checkAttackTime;
	BOOL	m_lastAttackCheck;
	int		m_cClipSize;
	BOOL	m_fFriendly;
	// UNDONE: What is this for?  It isn't used?
	float	m_flPlayerDamage;// how much pain has the player inflicted on me?

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_harrison, CHarrison );

//=========================================================
// KeyValue
//
// !!! netname entvar field is used in squadmonster for groupname!!!
//=========================================================
void CHarrison :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "friendly"))
	{
		m_fFriendly = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseMonster::KeyValue( pkvd );
	}
}
TYPEDESCRIPTION	CHarrison::m_SaveData[] = 
{
	DEFINE_FIELD( CHarrison, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CHarrison, m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( CHarrison, m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHarrison, m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( CHarrison, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CHarrison, m_fFriendly, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CHarrison, CTalkMonster );

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlHarrisonFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slHarrisonFollow[] =
{
	{
		tlHarrisonFollow,
		ARRAYSIZE ( tlHarrisonFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};

Task_t	tlHarrisonFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slHarrisonFaceTarget[] =
{
	{
		tlHarrisonFaceTarget,
		ARRAYSIZE ( tlHarrisonFaceTarget ),
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


Task_t	tlHarrisonIdleStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slHarrisonIdleStand[] =
{
	{ 
		tlHarrisonIdleStand,
		ARRAYSIZE ( tlHarrisonIdleStand ), 
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
// Harrison reload schedule
//=========================================================
Task_t	tlHarrisonHideReload[] =
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

Schedule_t slHarrisonHideReload[] = 
{
	{
		tlHarrisonHideReload,
		ARRAYSIZE ( tlHarrisonHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"HarrisonHideReload"
	}
};
//=========================================================
// Victory dance!
//=========================================================
Task_t	tlHarrisonVictoryDance[] =
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

Schedule_t	slHarrisonVictoryDance[] =
{
	{ 
		tlHarrisonVictoryDance,
		ARRAYSIZE ( tlHarrisonVictoryDance ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE,
		0,
		"HarrisonVictoryDance"
	},
};
DEFINE_CUSTOM_SCHEDULES( CHarrison )
{
	slHarrisonFollow,
	slHarrisonFaceTarget,
	slHarrisonIdleStand,
	slHarrisonHideReload,
	slHarrisonVictoryDance,
};


IMPLEMENT_CUSTOM_SCHEDULES( CHarrison, CTalkMonster );

void CHarrison :: StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_WALK_PATH:
	case TASK_RUN_PATH:
		// harrison no longer assumes he is covered if he moves
		Forget( bits_MEMORY_INCOVER );
		CTalkMonster ::StartTask( pTask );
		break;

	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CTalkMonster :: StartTask( pTask );
		if (pev->movetype == MOVETYPE_FLY)
		{
			m_IdealActivity = ACT_GLIDE;
		}
		break;

	default: 
		CTalkMonster :: StartTask( pTask );
		break;
	}
}

void CHarrison :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:

		pev->framerate = 0.30;

		CTalkMonster::RunTask( pTask );
		break;
	default:
		CTalkMonster::RunTask( pTask );
		break;
	}
}




//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int CHarrison :: ISoundMask ( void) 
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
int	CHarrison :: Classify ( void )
{
	if ( m_fFriendly )
		return	CLASS_PLAYER_ALLY;
	else
		return	CLASS_HUMAN_MILITARY;
}

//=========================================================
// ALertSound - Harrison says "Freeze!"
//=========================================================
void CHarrison :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( FOkToSpeak() )
		{
			PlaySentence( "HA_ATTACK", RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
		}
	}

}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CHarrison :: SetYawSpeed ( void )
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
// CheckRangeAttack1
//=========================================================
BOOL CHarrison :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 1024 && flDot >= 0.5 )
	{
		if ( !m_hEnemy->IsPlayer() && flDist <= 64 )
		{
			// kick nonclients, but don't shoot at them.
			return FALSE;
		}
		if ( gpGlobals->time > m_checkAttackTime )
		{
			TraceResult tr;
			
			Vector shootOrigin = pev->origin + Vector( 0, 0, 55 );
			CBaseEntity *pEnemy = m_hEnemy;
			Vector shootTarget = ( (pEnemy->BodyTarget( shootOrigin ) - pEnemy->pev->origin) + m_vecEnemyLKP );
			UTIL_TraceLine( shootOrigin, shootTarget, dont_ignore_monsters, ENT(pev), &tr );
			m_checkAttackTime = gpGlobals->time + 1;
			if ( tr.flFraction == 1.0 || (tr.pHit != NULL && CBaseEntity::Instance(tr.pHit) == pEnemy) )
				m_lastAttackCheck = TRUE;
			else
				m_lastAttackCheck = FALSE;
			m_checkAttackTime = gpGlobals->time + 1.5;
		}
		return m_lastAttackCheck;
	}
	return FALSE;
}
//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CHarrison :: CheckMeleeAttack1 ( float flDot, float flDist )
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
//=========================================================
CBaseEntity *CHarrison :: Kick( void )
{
	TraceResult tr;
	//// blast circle
	//MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
	//	WRITE_BYTE( TE_BEAMTORUS ); // This acts like 1000 joules of energy were released.
	//	WRITE_COORD( pev->origin.x);
	//	WRITE_COORD( pev->origin.y);
	//	WRITE_COORD( pev->origin.z);
	//	WRITE_COORD( pev->origin.x);
	//	WRITE_COORD( pev->origin.y);
	//	WRITE_COORD( pev->origin.z + 200 ); // reach damage radius over .2 seconds
	//	WRITE_SHORT( m_iSpriteTexture );
	//	WRITE_BYTE( 0 ); // startframe
	//	WRITE_BYTE( 0 ); // framerate
	//	WRITE_BYTE( 10 ); // life
	//	WRITE_BYTE( 64 );  // width
	//	WRITE_BYTE( 0 );   // noise
	//	WRITE_BYTE( 24 );   // r, g, b
	//	WRITE_BYTE( 121 );   // r, g, b
	//	WRITE_BYTE( 239 );   // r, g, b
	//	WRITE_BYTE( 128 ); // brightness
	//	WRITE_BYTE( 0 );		// speed
	//MESSAGE_END();
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
// Shoot - shoots one round from the pistol at
// the enemy harrison is facing.
//=========================================================
void CHarrison :: Shoot ( void )
{
	Vector vecShootOrigin;

	UTIL_MakeVectors(pev->angles);
	vecShootOrigin = pev->origin + Vector( 0, 0, 55 );
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
	pev->effects = EF_MUZZLEFLASH;

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_8DEGREES, 1024, BULLET_PLAYER_M4A1 );

	switch ( RANDOM_LONG(0,1) )
	{
		case 0: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/m4_fire1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/m4_fire2.wav", 1, ATTN_NORM ); break;
	}

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
void CHarrison :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case HARRISON_AE_SHOOT:
		Shoot();
		break;

	case HARRISON_AE_KICK:
	{
		CBaseEntity *pHurt = Kick();

		if ( pHurt )
		{
			// SOUND HERE!
			UTIL_MakeVectors( pev->angles );
			pHurt->pev->punchangle.x = 15;
			pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
			pHurt->TakeDamage( pev, pev, gSkillData.harrisonDmgKick, DMG_CLUB );
		}
	}
	break;

	case HARRISON_AE_RELOAD:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions(bits_COND_NO_AMMO_LOADED);
		break;

	default:
		CTalkMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Spawn
//=========================================================
void CHarrison :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/harrison.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= gSkillData.harrisonHealth;
	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;
	m_cClipSize			= HARRISON_CLIP_SIZE;
	m_cAmmoLoaded		= m_cClipSize;

	SetBodygroup( GUN_GROUP, GUN_DRAWN );

	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	MonsterInit();
	if ( m_fFriendly )
		SetUse( FollowerUse );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CHarrison :: Precache()
{
	PRECACHE_MODEL("models/harrison.mdl");

	PRECACHE_SOUND( "weapons/m4_fire1.wav" );
	PRECACHE_SOUND( "weapons/m4_fire2.wav" );

	PRECACHE_SOUND("harrison/ha_pain1.wav");
	PRECACHE_SOUND("harrison/ha_pain2.wav");
	PRECACHE_SOUND("harrison/ha_pain3.wav");

	PRECACHE_SOUND("harrison/ha_die1.wav");
	
	PRECACHE_SOUND( "hgrunt/gr_reload1.wav" );

	// every new harrison must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();
	CTalkMonster::Precache();
}	

// Init talk data
void CHarrison :: TalkInit()
{
	
	CTalkMonster::TalkInit();

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER]  =	"HA_ANSWER";
	m_szGrp[TLK_QUESTION] =	"HA_QUESTION";
	m_szGrp[TLK_IDLE] =		"HA_IDLE";
	m_szGrp[TLK_STARE] =		"HA_STARE";
	m_szGrp[TLK_USE] =		"HA_OK";
	m_szGrp[TLK_UNUSE] =	"HA_WAIT";
	m_szGrp[TLK_STOP] =		"HA_STOP";

	m_szGrp[TLK_NOSHOOT] =	"HA_SCARED";
	m_szGrp[TLK_HELLO] =	"HA_HELLO";

	m_szGrp[TLK_PLHURT1] =	"!HA_CUREA";
	m_szGrp[TLK_PLHURT2] =	"!HA_CUREB"; 
	m_szGrp[TLK_PLHURT3] =	"!HA_CUREC";

	m_szGrp[TLK_PHELLO] =	NULL;	//"BA_PHELLO";		// UNDONE
	m_szGrp[TLK_PIDLE] =	NULL;	//"BA_PIDLE";			// UNDONE
	m_szGrp[TLK_PQUESTION] = "HA_PQUEST";		// UNDONE

	m_szGrp[TLK_SMELL] =	"HA_SMELL";
	
	m_szGrp[TLK_WOUND] =	"HA_WOUND";
	m_szGrp[TLK_MORTAL] =	"HA_MORTAL";

	// get voice for head - just one harrison voice for now
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


int CHarrison :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	int ret;
	if ( m_fFriendly )
	{
		flDamage = 0;
		// make sure friends talk about it if player hurts talkmonsters...
		ret = CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	}
	else
	{
		ret = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	}
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	if ( m_fFriendly )
	{
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
					PlaySentence( "HA_MAD", 4, VOL_NORM, ATTN_NORM );

					Remember( bits_MEMORY_PROVOKED );
					StopFollowing( TRUE );
				}
				else
				{
					// Hey, be careful with that
					PlaySentence( "HA_SHOT", 4, VOL_NORM, ATTN_NORM );
					Remember( bits_MEMORY_SUSPICIOUS );
				}
			}
			else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
			{
				PlaySentence( "HA_SHOT", 4, VOL_NORM, ATTN_NORM );
			}
		}
	}
	return ret;
}

	
//=========================================================
// PainSound
//=========================================================
void CHarrison :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "harrison/ha_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "harrison/ha_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "harrison/ha_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CHarrison :: DeathSound ( void )
{

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "harrison/ha_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch());
}

//=========================================================
// CheckAmmo - overridden for the grunt because he actually
// uses ammo! (base class doesn't)
//=========================================================
void CHarrison :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
	{
		SetConditions(bits_COND_NO_AMMO_LOADED);
	}
}
void CHarrison::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( GetBodygroup( GUN_GROUP ) != GUN_HOLSTER )
	{// drop the gun!
		Vector vecGunPos;
		Vector vecGunAngles;

		SetBodygroup( GUN_GROUP , GUN_HOLSTER );

		GetAttachment( 0, vecGunPos, vecGunAngles );
		
		CBaseEntity *pGun = DropItem( "weapon_m4a1", vecGunPos, vecGunAngles );
	}

	SetUse( NULL );	
	CTalkMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* CHarrison :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{

	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that harrison will talk
		// when 'used' 
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slHarrisonFaceTarget;	// override this for different target face behavior
		else
			return psched;

	case SCHED_HARRISON_COVER_AND_RELOAD:
		{
			return &slHarrisonHideReload[ 0 ];
		}
	case SCHED_VICTORY_DANCE:
		{
			return &slHarrisonVictoryDance[ 0 ];
		}
	case SCHED_TARGET_CHASE:
		return slHarrisonFollow;

	case SCHED_IDLE_STAND:
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
		{
			// just look straight ahead.
			return slHarrisonIdleStand;
		}
		else
			return psched;	
	}

	return CTalkMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CHarrison :: GetSchedule ( void )
{
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
		PlaySentence( "HA_KILL", 4, VOL_NORM, ATTN_NORM );
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

			// always act surprized with a new enemy
			if ( HasConditions( bits_COND_NEW_ENEMY ) && HasConditions( bits_COND_LIGHT_DAMAGE) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );

			// no ammo
			if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
			{
				//!!!KELLY - this individual just realized he's out of bullet ammo. 
				// He's going to try to find cover to run to and reload, but rarely, if 
				// none is available, he'll drop and reload in the open here. 
				return GetScheduleOfType ( SCHED_HARRISON_COVER_AND_RELOAD );
			}

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
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

		// try to say something about smells
		TrySmellTalk();
		break;
	}
	
	return CTalkMonster::GetSchedule();
}

MONSTERSTATE CHarrison :: GetIdealState ( void )
{
	return CTalkMonster::GetIdealState();
}



void CHarrison::DeclineFollowing( void )
{
	PlaySentence( "HA_POK", 2, VOL_NORM, ATTN_NORM );
}