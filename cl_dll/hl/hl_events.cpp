//========= Copyright � 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#include "../hud.h"
#include "../cl_util.h"
#include "event_api.h"

extern "C"
{
// HLDM
void EV_FireGlock1( struct event_args_s *args  );
void EV_FireGlock2( struct event_args_s *args  );
void EV_FireShotGunSingle( struct event_args_s *args  );
void EV_FireShotGunDouble( struct event_args_s *args  );
void EV_ShotGunShell( struct event_args_s *args  );
void EV_FireM4a1( struct event_args_s *args  );
void EV_FireM4a12( struct event_args_s *args  );
void EV_FirePython( struct event_args_s *args  );
void EV_FireGauss( struct event_args_s *args  );
void EV_SpinGauss( struct event_args_s *args  );
void EV_Crowbar( struct event_args_s *args );
void EV_FireCrossbow( struct event_args_s *args );
void EV_FireCrossbow2( struct event_args_s *args );
void EV_FireRpg( struct event_args_s *args );
void EV_EgonFire( struct event_args_s *args );
void EV_EgonStop( struct event_args_s *args );
void EV_HornetGunFire( struct event_args_s *args );
void EV_TripmineFire( struct event_args_s *args );
void EV_SnarkFire( struct event_args_s *args );
//Raven City weapons
void EV_FireEagle( struct event_args_s *args  );
void EV_Knife( struct event_args_s *args );
void EV_FireM249( struct event_args_s *args  );
void EV_FireM40a1( struct event_args_s *args  );
void EV_SpinDisplacer( struct event_args_s *args  );
void EV_FireDisplacer( struct event_args_s *args  );
void EV_SporeLauncherRocket( struct event_args_s *args  );
void EV_SporeLauncherGrenade( struct event_args_s *args  );
void EV_FireShock( struct event_args_s *args  );
void EV_WrenchNormal( struct event_args_s *args  );
void EV_WrenchLarge( struct event_args_s *args  );
void EV_FireBeretta( struct event_args_s *args  );
void EV_TrainPitchAdjust( struct event_args_s *args );
}

/*
======================
Game_HookEvents

Associate script file name with callback functions.  Callback's must be extern "C" so
 the engine doesn't get confused about name mangling stuff.  Note that the format is
 always the same.  Of course, a clever mod team could actually embed parameters, behavior
 into the actual .sc files and create a .sc file parser and hook their functionality through
 that.. i.e., a scripting system.

That was what we were going to do, but we ran out of time...oh well.
======================
*/
void Game_HookEvents( void )
{
	gEngfuncs.pfnHookEvent( "events/glock1.sc",					EV_FireGlock1 );
	gEngfuncs.pfnHookEvent( "events/glock2.sc",					EV_FireGlock2 );
	gEngfuncs.pfnHookEvent( "events/shotgun1.sc",				EV_FireShotGunSingle );
	gEngfuncs.pfnHookEvent( "events/shotgun2.sc",				EV_FireShotGunDouble );
	gEngfuncs.pfnHookEvent( "events/scock.sc",					EV_ShotGunShell );
	gEngfuncs.pfnHookEvent( "events/m4a1.sc",					EV_FireM4a1 );
	gEngfuncs.pfnHookEvent( "events/m4a12.sc",					EV_FireM4a12 );
	gEngfuncs.pfnHookEvent( "events/python.sc",					EV_FirePython );
	gEngfuncs.pfnHookEvent( "events/gauss.sc",					EV_FireGauss );
	gEngfuncs.pfnHookEvent( "events/gaussspin.sc",				EV_SpinGauss );
	gEngfuncs.pfnHookEvent( "events/train.sc",					EV_TrainPitchAdjust );
	gEngfuncs.pfnHookEvent( "events/crowbar.sc",				EV_Crowbar );
	gEngfuncs.pfnHookEvent( "events/crossbow1.sc",				EV_FireCrossbow );
	gEngfuncs.pfnHookEvent( "events/crossbow2.sc",				EV_FireCrossbow2 );
	gEngfuncs.pfnHookEvent( "events/rpg.sc",					EV_FireRpg );
	gEngfuncs.pfnHookEvent( "events/egon_fire.sc",				EV_EgonFire );
	gEngfuncs.pfnHookEvent( "events/egon_stop.sc",				EV_EgonStop );
	gEngfuncs.pfnHookEvent( "events/firehornet.sc",				EV_HornetGunFire );
	gEngfuncs.pfnHookEvent( "events/tripfire.sc",				EV_TripmineFire );
	gEngfuncs.pfnHookEvent( "events/snarkfire.sc",				EV_SnarkFire );
	//Raven City weapons
	gEngfuncs.pfnHookEvent( "events/eagle.sc",					EV_FireEagle );
	gEngfuncs.pfnHookEvent( "events/knife.sc",				    EV_Knife );
	gEngfuncs.pfnHookEvent( "events/m249.sc",					EV_FireM249 );
	gEngfuncs.pfnHookEvent( "events/m40a1.sc",					EV_FireM40a1 );
	gEngfuncs.pfnHookEvent( "events/spindisplacer.sc",			EV_SpinDisplacer );
	gEngfuncs.pfnHookEvent( "events/firedisplacer.sc",			EV_FireDisplacer );
	gEngfuncs.pfnHookEvent( "events/sporerocket.sc",			EV_SporeLauncherRocket );
	gEngfuncs.pfnHookEvent( "events/sporegrenade.sc",			EV_SporeLauncherGrenade );
	gEngfuncs.pfnHookEvent( "events/shockrifle.sc",				EV_FireShock );
	gEngfuncs.pfnHookEvent( "events/wrench.sc",					EV_WrenchNormal );
	gEngfuncs.pfnHookEvent( "events/wrench2.sc",				EV_WrenchLarge );
	gEngfuncs.pfnHookEvent( "events/beretta.sc",				EV_FireBeretta );
}
