//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//
// pm_shared.h
//
#if !defined( PM_SHAREDH )
#define PM_SHAREDH
#pragma once

void PM_InitTextureTypes( const char *levelname );
void PM_Init( struct playermove_s *ppmove );
void PM_Move ( struct playermove_s *ppmove, int server );
char PM_FindTextureType( char *name );


// Spectator Movement modes (stored in pev->iuser1, so the physics code can get at them)
#define OBS_NONE				0
#define OBS_CHASE_LOCKED		1
#define OBS_CHASE_FREE			2
#define OBS_ROAMING				3		
#define OBS_IN_EYE				4
#define OBS_MAP_FREE			5
#define OBS_MAP_CHASE			6

#endif