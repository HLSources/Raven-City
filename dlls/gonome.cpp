//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//
//=========================================================
// Gonome
//=========================================================

// UNDONE: Don't flinch every time you get hit

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"decals.h"
#include	"animation.h"
int			iGonomeGutSprite;

//=========================================================
// Gonome Guts
//=========================================================
class CGonomeGuts : public CBaseEntity
{
public:
	void Spawn( void );

	static void Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	void Touch( CBaseEntity *pOther );
	void EXPORT Animate( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int  m_maxFrame;
};

LINK_ENTITY_TO_CLASS( gonome_guts, CGonomeGuts );

TYPEDESCRIPTION	CGonomeGuts::m_SaveData[] = 
{
	DEFINE_FIELD( CGonomeGuts, m_maxFrame, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CGonomeGuts, CBaseEntity );

void CGonomeGuts:: Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "gonome_guts" );
	
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAlpha;
	pev->renderamt = 255;

	SET_MODEL(ENT(pev), "sprites/bigspit.spr");
	pev->frame = 0;
	pev->scale = 0.5;

	UTIL_SetSize( pev, Vector( 0, 0, 0), Vector(0, 0, 0) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
}

void CGonomeGuts::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->frame++ )
	{
		if ( pev->frame > m_maxFrame )
		{
			pev->frame = 0;
		}
	}
}

void CGonomeGuts::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGonomeGuts *pGuts = GetClassPtr( (CGonomeGuts *)NULL );
	pGuts->Spawn();
	
	UTIL_SetOrigin( pGuts->pev, vecStart );
	pGuts->pev->velocity = vecVelocity;
	pGuts->pev->owner = ENT(pevOwner);

	pGuts->SetThink ( Animate );
	pGuts->pev->nextthink = gpGlobals->time + 0.1;
}

void CGonomeGuts :: Touch ( CBaseEntity *pOther )
{
	TraceResult tr;
	int		iPitch;

	// splat sound
	iPitch = RANDOM_FLOAT( 90, 110 );

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_acid1.wav", 1, ATTN_NORM, 0, iPitch );	

	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_spithit1.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_spithit2.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	}

	if ( !pOther->pev->takedamage )
	{

		// make a splat on the wall
		UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT( pev ), &tr );
		UTIL_DecalTrace(&tr, DECAL_SPIT1 + RANDOM_LONG(0,1));

		// make some flecks
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
			WRITE_BYTE( TE_SPRITE_SPRAY );
			WRITE_COORD( tr.vecEndPos.x);	// pos
			WRITE_COORD( tr.vecEndPos.y);	
			WRITE_COORD( tr.vecEndPos.z);	
			WRITE_COORD( tr.vecPlaneNormal.x);	// dir
			WRITE_COORD( tr.vecPlaneNormal.y);	
			WRITE_COORD( tr.vecPlaneNormal.z);	
			WRITE_SHORT( iGonomeGutSprite );	// model
			WRITE_BYTE ( 5 );			// count
			WRITE_BYTE ( 30 );			// speed
			WRITE_BYTE ( 80 );			// noise ( client will divide by 100 )
		MESSAGE_END();
	}
	else
	{
		pOther->TakeDamage ( pev, pev, gSkillData.gonomeDmgGuts, DMG_GENERIC );
	}

	SetThink ( SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	GONOME_AE_ATTACK_RIGHT		1
#define	GONOME_AE_ATTACK_LEFT		2
#define	GONOME_AE_GRAB_SALIVA		3
#define	GONOME_AE_THROW_SALIVA		4
#define	GONOME_AE_ATTACK_MOUTH01	19
#define	GONOME_AE_ATTACK_MOUTH02	20
#define	GONOME_AE_ATTACK_MOUTH03	21
#define	GONOME_AE_ATTACK_MOUTH04	22

#define GONOME_FLINCH_DELAY			2		// at most one flinch every n secs

class CGonome : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );
	void SetActivity ( Activity NewActivity );

	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );

	float m_flNextFlinch;

	void PainSound( void );
	void IdleSound( void );
	void HitSound( void );
	void MissSound( void );

	float m_flNextThrowTime;// last time the bullsquid used the spit attack.

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_gonome, CGonome );

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CGonome :: Classify ( void )
{
	return	CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetActivity 
//=========================================================
void CGonome :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	Vector target;

	if ( m_hEnemy != NULL )
	{
		target = m_hEnemy->pev->origin - pev->origin;
	}
	switch ( NewActivity)
	{
	case ACT_MELEE_ATTACK1:
		if ( target.Length() < 100 )
		{
			iSequence = LookupSequence( "attack2" );
		}
		else
		{
			iSequence = LookupSequence( "attack1" );
		}
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
void CGonome :: SetYawSpeed ( void )
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}
//=========================================================
// CheckMeleeAttack2 - bullsquid is a big guy, so has a longer
// melee range than most monsters. This is the bite attack.
// this attack will not be performed if the tailwhip attack
// is valid.
//=========================================================
BOOL CGonome :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 85 && flDot >= 0.7 && !HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )		// The player & bullsquid can be as much as their bboxes 
	{										// apart (48 * sqrt(3)) and he can still attack (85 is a little more than 48*sqrt(3))
		return TRUE;
	}
	return FALSE;
}  
//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CGonome :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( IsMoving() && flDist >= 512 )
	{
		// gonome will far too far behind if he stops running to throw at this distance from the enemy.
		return FALSE;
	}

	if ( flDist > 64 && flDist <= 784 && flDot >= 0.5 && gpGlobals->time >= m_flNextThrowTime )
	{
		if ( m_hEnemy != NULL )
		{
			if ( fabs( pev->origin.z - m_hEnemy->pev->origin.z ) > 256 )
			{
				// don't try to throw at someone up really high or down really low.
				return FALSE;
			}
		}

		if ( IsMoving() )
		{
			// don't spit again for a long time, resume chasing enemy.
			m_flNextThrowTime = gpGlobals->time + 5;
		}
		else
		{
			// not moving, so spit again pretty soon.
			m_flNextThrowTime = gpGlobals->time + 0.5;
		}

		return TRUE;
	}

	return FALSE;
}
int CGonome :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3;
	}

	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CGonome :: PainSound( void )
{
	switch ( RANDOM_LONG(0,3) )
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_pain1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_pain2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_pain3.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 3: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_pain4.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	}
}

void CGonome :: IdleSound( void )
{
	switch ( RANDOM_LONG(0,2) )
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_idle1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_idle2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "gonome/gonome_idle3.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	}
}

void CGonome :: HitSound( void )
{
	switch ( RANDOM_LONG(0,1) )
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "zombie/claw_strike1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "zombie/claw_strike1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "zombie/claw_strike3.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	}
}

void CGonome :: MissSound( void )
{
	switch ( RANDOM_LONG(0,1) )
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "zombie/claw_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "zombie/claw_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
	}
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGonome :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case GONOME_AE_ATTACK_RIGHT:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.gonomeDmgOneSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
				}
				// Play a random attack hit sound
				HitSound();
			}
			else // Play a random attack miss sound
				MissSound();
		}
		break;

		case GONOME_AE_ATTACK_LEFT:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.gonomeDmgOneSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100;
				}
				HitSound();
			}
			else
				MissSound();
		}
		break;

		case GONOME_AE_ATTACK_MOUTH01:
		case GONOME_AE_ATTACK_MOUTH02:
		case GONOME_AE_ATTACK_MOUTH03:
		case GONOME_AE_ATTACK_MOUTH04:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.gonomeDmgOneBite, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.x = 10;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
				}
				HitSound();
			}
			else
				MissSound();
		}
		break;

		case GONOME_AE_THROW_SALIVA:
		{
			Vector	vecGutOffset;
			Vector	vecGutDir;

			UTIL_MakeVectors ( pev->angles );

			vecGutOffset = ( gpGlobals->v_right * 8 + gpGlobals->v_forward * 37 + gpGlobals->v_up * 23 );		
			vecGutOffset = ( pev->origin + vecGutOffset );
			vecGutDir = ( ( m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs ) - vecGutOffset ).Normalize();

			vecGutDir.x += RANDOM_FLOAT( -0.05, 0.05 );
			vecGutDir.y += RANDOM_FLOAT( -0.05, 0.05 );
			vecGutDir.z += RANDOM_FLOAT( -0.05, 0 );

			// spew the spittle temporary ents.
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecGutOffset );
				WRITE_BYTE( TE_SPRITE_SPRAY );
				WRITE_COORD( vecGutOffset.x);	// pos
				WRITE_COORD( vecGutOffset.y);	
				WRITE_COORD( vecGutOffset.z);	
				WRITE_COORD( vecGutDir.x);	// dir
				WRITE_COORD( vecGutDir.y);	
				WRITE_COORD( vecGutDir.z);	
				WRITE_SHORT( iGonomeGutSprite );	// model
				WRITE_BYTE ( 15 );			// count
				WRITE_BYTE ( 210 );			// speed
				WRITE_BYTE ( 25 );			// noise ( client will divide by 100 )
			MESSAGE_END();

			CGonomeGuts::Shoot( pev, vecGutOffset, vecGutDir * 900 );
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CGonome :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/gonome.mdl");
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health			= gSkillData.gonomeHealth;
	pev->view_ofs		= VEC_VIEW;// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;
	m_flNextThrowTime	= gpGlobals->time;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGonome :: Precache()
{

	UTIL_PrecacheOther( "gonome_guts" );

	iGonomeGutSprite = PRECACHE_MODEL("sprites/bigspit.spr");// client side spittle.

	PRECACHE_MODEL("models/gonome.mdl");

	PRECACHE_SOUND("gonome/gonome_death2.wav");
	PRECACHE_SOUND("gonome/gonome_death3.wav");
	PRECACHE_SOUND("gonome/gonome_death4.wav");

	PRECACHE_SOUND("gonome/gonome_pain1.wav");
	PRECACHE_SOUND("gonome/gonome_pain2.wav");
	PRECACHE_SOUND("gonome/gonome_pain3.wav");
	PRECACHE_SOUND("gonome/gonome_pain4.wav");

	PRECACHE_SOUND("gonome/gonome_melee1.wav");
	PRECACHE_SOUND("gonome/gonome_melee2.wav");

	PRECACHE_SOUND("gonome/gonome_eat.wav");

	PRECACHE_SOUND("gonome/gonome_run.wav");

	PRECACHE_SOUND("gonome/gonome_jumpattack.wav");

	PRECACHE_SOUND("gonome/gonome_idle1.wav");
	PRECACHE_SOUND("gonome/gonome_idle2.wav");
	PRECACHE_SOUND("gonome/gonome_idle3.wav");
}	

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

int CGonome::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ((m_Activity == ACT_MELEE_ATTACK1) || (m_Activity == ACT_MELEE_ATTACK1))
	{
#if 0
		if (pev->health < 20)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
		else
#endif			
		if (m_flNextFlinch >= gpGlobals->time)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
	}

	if ((m_Activity == ACT_SMALL_FLINCH) || (m_Activity == ACT_BIG_FLINCH))
	{
		if (m_flNextFlinch < gpGlobals->time)
			m_flNextFlinch = gpGlobals->time + GONOME_FLINCH_DELAY;
	}

	return iIgnore;
	
}