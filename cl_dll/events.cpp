//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#include "hud.h"
#include "cl_util.h"

void Game_HookEvents( void );

/*
===================
EV_HookEvents

See if game specific code wants to hook any events.
===================
*/
void EV_HookEvents( void )
{
	Game_HookEvents();
}