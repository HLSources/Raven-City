//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

// Spectator.h

class CBaseSpectator : public CBaseEntity 
{
public:
	void Spawn();
	void SpectatorConnect(void);
	void SpectatorDisconnect(void);
	void SpectatorThink(void);

private:
	void SpectatorImpulseCommand(void);
};