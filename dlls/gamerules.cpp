//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//
//=========================================================
// GameRules.cpp
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"teamplay_gamerules.h"
#include	"skill.h"
#include	"game.h"

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgMOTD;

int g_teamplay = 0;

//=========================================================
//=========================================================
BOOL CGameRules::CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry )
{
	int iAmmoIndex;

	if ( pszAmmoName )
	{
		iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );

		if ( iAmmoIndex > -1 )
		{
			if ( pPlayer->AmmoInventory( iAmmoIndex ) < iMaxCarry )
			{
				// player has room for more of this type of ammo
				return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
//=========================================================
edict_t *CGameRules :: GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );

	pPlayer->pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
	pPlayer->pev->v_angle  = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pentSpawnSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = TRUE;
	
	return pentSpawnSpot;
}

//=========================================================
//=========================================================
BOOL CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// only living players can have items
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( pWeapon->pszAmmo1() )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1() ) )
		{
			// we can't carry anymore ammo for this gun. We can only 
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->HasPlayerItem( pWeapon ) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->HasPlayerItem( pWeapon ) )
		{
			return FALSE;
		}
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return TRUE;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( void )
{
	int	iSkill;

	iSkill = (int)CVAR_GET_FLOAT("skill");
	g_iSkillLevel = iSkill;

	if ( iSkill < 1 )
	{
		iSkill = 1;
	}
	else if ( iSkill > 3 )
	{
		iSkill = 3; 
	}

	gSkillData.iSkillLevel = iSkill;

	ALERT ( at_console, "\nGAME SKILL LEVEL:%d\n",iSkill );

	//Agrunt		
	gSkillData.agruntHealth = GetSkillCvar( "sk_agrunt_health" );
	gSkillData.agruntDmgPunch = GetSkillCvar( "sk_agrunt_dmg_punch");

	// Apache 
	gSkillData.apacheHealth = GetSkillCvar( "sk_apache_health");

	// Barney
	gSkillData.barneyHealth = GetSkillCvar( "sk_barney_health");

	// Big Momma
	gSkillData.bigmommaHealthFactor = GetSkillCvar( "sk_bigmomma_health_factor" );
	gSkillData.bigmommaDmgSlash = GetSkillCvar( "sk_bigmomma_dmg_slash" );
	gSkillData.bigmommaDmgBlast = GetSkillCvar( "sk_bigmomma_dmg_blast" );
	gSkillData.bigmommaRadiusBlast = GetSkillCvar( "sk_bigmomma_radius_blast" );

	// Bullsquid
	gSkillData.bullsquidHealth = GetSkillCvar( "sk_bullsquid_health");
	gSkillData.bullsquidDmgBite = GetSkillCvar( "sk_bullsquid_dmg_bite");
	gSkillData.bullsquidDmgWhip = GetSkillCvar( "sk_bullsquid_dmg_whip");
	gSkillData.bullsquidDmgSpit = GetSkillCvar( "sk_bullsquid_dmg_spit");

	// Gargantua
	gSkillData.gargantuaHealth = GetSkillCvar( "sk_gargantua_health");
	gSkillData.gargantuaDmgSlash = GetSkillCvar( "sk_gargantua_dmg_slash");
	gSkillData.gargantuaDmgFire = GetSkillCvar( "sk_gargantua_dmg_fire");
	gSkillData.gargantuaDmgStomp = GetSkillCvar( "sk_gargantua_dmg_stomp");

	// Hassassin
	gSkillData.hassassinHealth = GetSkillCvar( "sk_hassassin_health");

	// Headcrab
	gSkillData.headcrabHealth = GetSkillCvar( "sk_headcrab_health");
	gSkillData.headcrabDmgBite = GetSkillCvar( "sk_headcrab_dmg_bite");

	// Hgrunt 
	gSkillData.hgruntHealth = GetSkillCvar( "sk_hgrunt_health");
	gSkillData.hgruntDmgKick = GetSkillCvar( "sk_hgrunt_kick");
	gSkillData.hgruntShotgunPellets = GetSkillCvar( "sk_hgrunt_pellets");
	gSkillData.hgruntGrenadeSpeed = GetSkillCvar( "sk_hgrunt_gspeed");

	// Houndeye
	gSkillData.houndeyeHealth = GetSkillCvar( "sk_houndeye_health");
	gSkillData.houndeyeDmgBlast = GetSkillCvar( "sk_houndeye_dmg_blast");

	// ISlave
	gSkillData.slaveHealth = GetSkillCvar( "sk_islave_health");
	gSkillData.slaveDmgClaw = GetSkillCvar( "sk_islave_dmg_claw");
	gSkillData.slaveDmgClawrake = GetSkillCvar( "sk_islave_dmg_clawrake");
	gSkillData.slaveDmgZap = GetSkillCvar( "sk_islave_dmg_zap");

	// Icthyosaur
	gSkillData.ichthyosaurHealth = GetSkillCvar( "sk_ichthyosaur_health");
	gSkillData.ichthyosaurDmgShake = GetSkillCvar( "sk_ichthyosaur_shake");

	// Leech
	gSkillData.leechHealth = GetSkillCvar( "sk_leech_health");

	gSkillData.leechDmgBite = GetSkillCvar( "sk_leech_dmg_bite");

	// Controller
	gSkillData.controllerHealth = GetSkillCvar( "sk_controller_health");
	gSkillData.controllerDmgZap = GetSkillCvar( "sk_controller_dmgzap");
	gSkillData.controllerSpeedBall = GetSkillCvar( "sk_controller_speedball");
	gSkillData.controllerDmgBall = GetSkillCvar( "sk_controller_dmgball");

	// Nihilanth
	gSkillData.nihilanthHealth = GetSkillCvar( "sk_nihilanth_health");
	gSkillData.nihilanthZap = GetSkillCvar( "sk_nihilanth_zap");

	// Scientist
	gSkillData.scientistHealth = GetSkillCvar( "sk_scientist_health");

	// Snark
	gSkillData.snarkHealth = GetSkillCvar( "sk_snark_health");
	gSkillData.snarkDmgBite = GetSkillCvar( "sk_snark_dmg_bite");
	gSkillData.snarkDmgPop = GetSkillCvar( "sk_snark_dmg_pop");

	// Zombie
	gSkillData.zombieHealth = GetSkillCvar( "sk_zombie_health");
	gSkillData.zombieDmgOneSlash = GetSkillCvar( "sk_zombie_dmg_one_slash");
	gSkillData.zombieDmgBothSlash = GetSkillCvar( "sk_zombie_dmg_both_slash");

	//Turret
	gSkillData.turretHealth = GetSkillCvar( "sk_turret_health");

	// MiniTurret
	gSkillData.miniturretHealth = GetSkillCvar( "sk_miniturret_health");
	
	// Sentry Turret
	gSkillData.sentryHealth = GetSkillCvar( "sk_sentry_health");

	//Raven City

	// ally grunt
	gSkillData.fgruntHealth = GetSkillCvar( "sk_hgrunt_ally_health");
	gSkillData.fgruntDmgKick = GetSkillCvar( "sk_hgrunt_ally_kick");
	gSkillData.fgruntShotgunPellets = GetSkillCvar( "sk_hgrunt_ally_pellets");
	gSkillData.fgruntGrenadeSpeed = GetSkillCvar( "sk_hgrunt_ally_gspeed");

	// ally grunt
	gSkillData.sogruntHealth = GetSkillCvar( "sk_spec_ops_health");
	gSkillData.sogruntDmgKick = GetSkillCvar( "sk_spec_ops_kick");
	gSkillData.sogruntShotgunPellets = GetSkillCvar( "sk_spec_ops_pellets");
	gSkillData.sogruntGrenadeSpeed = GetSkillCvar( "sk_spec_ops_gspeed");

	// spec ops grunt
	gSkillData.sogruntHealth = GetSkillCvar( "sk_spec_ops_health");
	gSkillData.sogruntDmgKick = GetSkillCvar( "sk_spec_ops_kick");
	gSkillData.sogruntShotgunPellets = GetSkillCvar( "sk_spec_ops_pellets");
	gSkillData.sogruntGrenadeSpeed = GetSkillCvar( "sk_spec_ops_gspeed");

	// harrison
	gSkillData.harrisonHealth = GetSkillCvar( "sk_harrison_health");
	gSkillData.harrisonDmgKick = GetSkillCvar( "sk_harrison_kick");

	// torch
	gSkillData.torchHealth = GetSkillCvar( "sk_torch_ally_health");
	gSkillData.torchDmgKick = GetSkillCvar( "sk_torch_ally_kick");

	// medic
	gSkillData.medicHealth = GetSkillCvar( "sk_medic_ally_health");
	gSkillData.medicDmgKick = GetSkillCvar( "sk_medic_ally_kick");
	gSkillData.medicHeal = GetSkillCvar( "sk_medic_ally_heal");
	// Otis
	gSkillData.OtisHealth = GetSkillCvar( "sk_otis_health");

	// Male Assassin
	gSkillData.massnHealth = GetSkillCvar( "sk_massassin_health");
	gSkillData.massnDmgKick = GetSkillCvar( "sk_massassin_kick");
	gSkillData.massnGrenadeSpeed = GetSkillCvar( "sk_massassin_gspeed");

	// vortigaunt
	gSkillData.vortigauntHealth = GetSkillCvar( "sk_vortigaunt_health");
	gSkillData.vortigauntDmgClaw = GetSkillCvar( "sk_vortigaunt_dmg_claw");
	gSkillData.vortigauntDmgClawrake = GetSkillCvar( "sk_vortigaunt_dmg_clawrake");
	gSkillData.vortigauntDmgZap = GetSkillCvar( "sk_vortigaunt_dmg_zap");

	// Zombie soldier
	gSkillData.zombiesoldierHealth = GetSkillCvar( "sk_zombie_soldier_health");
	gSkillData.zombiesoldierDmgOneSlash = GetSkillCvar( "sk_zombie_soldier_dmg_one_slash");
	gSkillData.zombiesoldierDmgBothSlash = GetSkillCvar( "sk_zombie_soldier_dmg_both_slash");

	// Zombie barney
	gSkillData.zombiebarneyHealth = GetSkillCvar( "sk_zombie_barney_health");
	gSkillData.zombiebarneyDmgOneSlash = GetSkillCvar( "sk_zombie_barney_dmg_one_slash");
	gSkillData.zombiebarneyDmgBothSlash = GetSkillCvar( "sk_zombie_barney_dmg_both_slash");

	// boris
	gSkillData.borisHealth = GetSkillCvar( "sk_boris_health");
	gSkillData.borisDmgKick = GetSkillCvar( "sk_boris_kick");
	gSkillData.borisShotgunPellets = GetSkillCvar( "sk_boris_pellets");
	gSkillData.borisDmgZap = GetSkillCvar( "sk_boris_zap");

	// maria
	gSkillData.mariaHealth = GetSkillCvar( "sk_maria_health");

	// gonome
	gSkillData.gonomeHealth = GetSkillCvar( "sk_gonome_health");
	gSkillData.gonomeDmgOneSlash = GetSkillCvar( "sk_gonome_dmg_one_slash");
	gSkillData.gonomeDmgGuts = GetSkillCvar( "sk_gonome_dmg_guts");
	gSkillData.gonomeDmgOneBite = GetSkillCvar( "sk_gonome_dmg_one_bite");

	// super zombie
	gSkillData.szombieHealth = GetSkillCvar( "sk_super_zombie_health");
	gSkillData.szombieDmgStab = GetSkillCvar( "sk_super_zombie_stab");
	gSkillData.szombieShotgunPellets = GetSkillCvar( "sk_super_zombie_pellets");

	// spec ops grunt
	gSkillData.gangsterHealth = GetSkillCvar( "sk_gangster_health");
	gSkillData.gangsterDmgKick = GetSkillCvar( "sk_gangster_kick");
	gSkillData.gangsterShotgunPellets = GetSkillCvar( "sk_gangster_pellets");
	gSkillData.gangsterGrenadeSpeed = GetSkillCvar( "sk_gangster_gspeed");

// PLAYER WEAPONS

	// Crowbar whack
	gSkillData.plrDmgCrowbar = GetSkillCvar( "sk_plr_crowbar");

	// Glock Round
	gSkillData.plrDmg9MM = GetSkillCvar( "sk_plr_9mm_bullet");

	// 357 Round
	gSkillData.plrDmg357 = GetSkillCvar( "sk_plr_357_bullet");

	// M4a1 Round
	gSkillData.plrDmgM4a1 = GetSkillCvar( "sk_plr_m4a1_bullet");

	// M203 grenade
	gSkillData.plrDmgM203Grenade = GetSkillCvar( "sk_plr_m4a1_grenade");

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = GetSkillCvar( "sk_plr_buckshot");

	// Crossbow
	gSkillData.plrDmgCrossbowClient = GetSkillCvar( "sk_plr_xbow_bolt_client");
	gSkillData.plrDmgCrossbowMonster = GetSkillCvar( "sk_plr_xbow_bolt_monster");

	// RPG
	gSkillData.plrDmgRPG = GetSkillCvar( "sk_plr_rpg");

	// Gauss gun
	gSkillData.plrDmgGauss = GetSkillCvar( "sk_plr_gauss");

	// Egon Gun
	gSkillData.plrDmgEgonNarrow = GetSkillCvar( "sk_plr_egon_narrow");
	gSkillData.plrDmgEgonWide = GetSkillCvar( "sk_plr_egon_wide");

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = GetSkillCvar( "sk_plr_hand_grenade");

	// Satchel Charge
	gSkillData.plrDmgSatchel = GetSkillCvar( "sk_plr_satchel");

	// Tripmine
	gSkillData.plrDmgTripmine = GetSkillCvar( "sk_plr_tripmine");

	// MONSTER WEAPONS
	gSkillData.monDmg12MM = GetSkillCvar( "sk_12mm_bullet");
	gSkillData.monDmgM4a1 = GetSkillCvar ("sk_m4a1_bullet" );
	gSkillData.monDmg9MM = GetSkillCvar( "sk_9mm_bullet");
	gSkillData.monDmg357 = GetSkillCvar( "sk_357_bullet");
	gSkillData.monDmgM249 = GetSkillCvar( "sk_556_bullet");
	gSkillData.monDmgM40a1 = GetSkillCvar( "sk_762_bullet");
	gSkillData.monDmgEagle = GetSkillCvar( "sk_eagle_bullet");
	gSkillData.monDmgBer92F = GetSkillCvar( "sk_beretta_bullet");

	// MONSTER HORNET
	gSkillData.monDmgHornet = GetSkillCvar( "sk_hornet_dmg");

	//Raven City

	// knife
	gSkillData.plrDmgKnife = GetSkillCvar( "sk_plr_knife");

	// Displacer
	gSkillData.plrDmgDisplacer = GetSkillCvar( "sk_plr_displacer_other");
	gSkillData.plrDisplacerRadius = GetSkillCvar( "sk_plr_displacer_radius");

	// Sporelauncher Spore
	gSkillData.plrDmgSpore = GetSkillCvar( "sk_plr_spore");

	// Shockrifle
	gSkillData.plrDmgShockm = GetSkillCvar( "sk_plr_shockroachm");
	gSkillData.plrDmgShocks = GetSkillCvar( "sk_plr_shockroachs");

	// pipe wrench
	gSkillData.plrDmgWrench = GetSkillCvar( "sk_plr_pipewrench");

	// m249
	gSkillData.plrDmgM249 = GetSkillCvar( "sk_plr_556_bullet");

	// Sniper Round
	gSkillData.plrDmgM40a1 = GetSkillCvar( "sk_plr_762_bullet");

	// eagle
	gSkillData.plrDmgEagle = GetSkillCvar( "sk_plr_eagle");

	// beretta
	gSkillData.plrDmgBer92F = GetSkillCvar( "sk_plr_beretta");

	// PLAYER HORNET
// Up to this point, player hornet damage and monster hornet damage were both using
// monDmgHornet to determine how much damage to do. In tuning the hivehand, we now need
// to separate player damage and monster hivehand damage. Since it's so late in the project, we've
// added plrDmgHornet to the SKILLDATA struct, but not to the engine CVar list, so it's inaccesible
// via SKILLS.CFG. Any player hivehand tuning must take place in the code. (sjb)
	gSkillData.plrDmgHornet = 7;


	// HEALTH/CHARGE
	gSkillData.suitchargerCapacity = GetSkillCvar( "sk_suitcharger" );
	gSkillData.batteryCapacity = GetSkillCvar( "sk_battery" );
	gSkillData.healthchargerCapacity = GetSkillCvar ( "sk_healthcharger" );
	gSkillData.healthkitCapacity = GetSkillCvar ( "sk_healthkit" );
	gSkillData.scientistHeal = GetSkillCvar ( "sk_scientist_heal" );

	// monster damage adj
	gSkillData.monHead = GetSkillCvar( "sk_monster_head" );
	gSkillData.monChest = GetSkillCvar( "sk_monster_chest" );
	gSkillData.monStomach = GetSkillCvar( "sk_monster_stomach" );
	gSkillData.monLeg = GetSkillCvar( "sk_monster_leg" );
	gSkillData.monArm = GetSkillCvar( "sk_monster_arm" );

	// player damage adj
	gSkillData.plrHead = GetSkillCvar( "sk_player_head" );
	gSkillData.plrChest = GetSkillCvar( "sk_player_chest" );
	gSkillData.plrStomach = GetSkillCvar( "sk_player_stomach" );
	gSkillData.plrLeg = GetSkillCvar( "sk_player_leg" );
	gSkillData.plrArm = GetSkillCvar( "sk_player_arm" );
}

//=========================================================
// instantiate the proper game rules object
//=========================================================

CGameRules *InstallGameRules( void )
{
	SERVER_COMMAND( "exec game.cfg\n" );
	SERVER_EXECUTE( );

	if ( !gpGlobals->deathmatch )
	{
		// generic half-life
		g_teamplay = 0;
		return new CHalfLifeRules;
	}
	else
	{
		if ( teamplay.value > 0 )
		{
			// teamplay

			g_teamplay = 1;
			return new CHalfLifeTeamplay;
		}
		if ((int)gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
		else
		{
			// vanilla deathmatch??
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
	}
}



