//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//=========================================================
// skill.h - skill level concerns
//=========================================================

struct skilldata_t
{

	int iSkillLevel; // game skill level

// Monster Health & Damage
	float	agruntHealth;
	float agruntDmgPunch;

	float apacheHealth;
	
	float barneyHealth;

	float bigmommaHealthFactor;		// Multiply each node's health by this
	float bigmommaDmgSlash;			// melee attack damage
	float bigmommaDmgBlast;			// mortar attack damage
	float bigmommaRadiusBlast;		// mortar attack radius

	float bullsquidHealth;
	float bullsquidDmgBite;
	float bullsquidDmgWhip;
	float bullsquidDmgSpit;

	float gargantuaHealth;
	float gargantuaDmgSlash;
	float gargantuaDmgFire;
	float gargantuaDmgStomp;

	float hassassinHealth;

	float headcrabHealth;
	float headcrabDmgBite;

	float hgruntHealth;
	float hgruntDmgKick;
	float hgruntShotgunPellets;
	float hgruntGrenadeSpeed;

	float houndeyeHealth;
	float houndeyeDmgBlast;

	float slaveHealth;
	float slaveDmgClaw;
	float slaveDmgClawrake;
	float slaveDmgZap;

	float ichthyosaurHealth;
	float ichthyosaurDmgShake;

	float leechHealth;
	float leechDmgBite;

	float controllerHealth;
	float controllerDmgZap;
	float controllerSpeedBall;
	float controllerDmgBall;

	float nihilanthHealth;
	float nihilanthZap;

	float scientistHealth;

	float snarkHealth;
	float snarkDmgBite;
	float snarkDmgPop;

	float zombieHealth;
	float zombieDmgOneSlash;
	float zombieDmgBothSlash;

	float turretHealth;
	float miniturretHealth;
	float sentryHealth;

	// Raven City

	float fgruntHealth;
	float fgruntDmgKick;
	float fgruntShotgunPellets;
	float fgruntGrenadeSpeed;

	float OtisHealth;

	float harrisonHealth;
	float harrisonDmgKick;

	float borisHealth;
	float borisDmgKick;
	float borisShotgunPellets;
	float borisDmgZap;

	float sogruntHealth;
	float sogruntDmgKick;
	float sogruntShotgunPellets;
	float sogruntGrenadeSpeed;

	float massnHealth;
	float massnDmgKick;
	float massnGrenadeSpeed;

	float torchHealth;
	float torchDmgKick;

	float medicHealth;
	float medicDmgKick;
	float medicHeal;

	float vortigauntHealth;
	float vortigauntDmgClaw;
	float vortigauntDmgClawrake;
	float vortigauntDmgZap;

	float zombiesoldierHealth;
	float zombiesoldierDmgOneSlash;
	float zombiesoldierDmgBothSlash;

	float zombiebarneyHealth;
	float zombiebarneyDmgOneSlash;
	float zombiebarneyDmgBothSlash;

	float gonomeHealth;
	float gonomeDmgOneSlash;
	float gonomeDmgGuts;
	float gonomeDmgOneBite;

	float szombieHealth;
	float szombieDmgStab;
	float szombieShotgunPellets;

	float mariaHealth;

	float gangsterHealth;
	float gangsterDmgKick;
	float gangsterShotgunPellets;
	float gangsterGrenadeSpeed;

// Player Weapons
	float plrDmgCrowbar;
	float plrDmg9MM;
	float plrDmg357;
	float plrDmgM4a1;
	float plrDmgM203Grenade;
	float plrDmgBuckshot;
	float plrDmgCrossbowClient;
	float plrDmgCrossbowMonster;
	float plrDmgRPG;
	float plrDmgGauss;
	float plrDmgEgonNarrow;
	float plrDmgEgonWide;
	float plrDmgHornet;
	float plrDmgHandGrenade;
	float plrDmgSatchel;
	float plrDmgTripmine;
	// Raven City
	float plrDmgEagle;
	float plrDmgM249;
	float plrDmgKnife;
	float plrDmgM40a1;
	float plrDmgBer92F;
	float plrDmgDisplacer;
	float plrDisplacerRadius;
	float plrDmgSpore;
	float plrDmgShocks;
	float plrDmgShockm;
	float plrDmgWrench;
// weapons shared by monsters
	float monDmg9MM;
	float monDmgM4a1;
	float monDmg12MM;
	float monDmgHornet;
	float monDmgShock;
	float monDmgEagle;
	float monDmgM249;
	float monDmgM40a1;
	float monDmgBer92F;
	float monDmg357;


// health/suit charge
	float suitchargerCapacity;
	float batteryCapacity;
	float healthchargerCapacity;
	float healthkitCapacity;
	float scientistHeal;

// monster damage adj
	float monHead;
	float monChest;
	float monStomach;
	float monLeg;
	float monArm;

// player damage adj
	float plrHead;
	float plrChest;
	float plrStomach;
	float plrLeg;
	float plrArm;
};

extern	DLL_GLOBAL	skilldata_t	gSkillData;
float GetSkillCvar( char *pName );

extern DLL_GLOBAL int		g_iSkillLevel;

#define SKILL_EASY		1
#define SKILL_MEDIUM	2
#define SKILL_HARD		3
