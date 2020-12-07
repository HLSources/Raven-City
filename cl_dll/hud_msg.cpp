//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//
//  hud_msg.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "rain.h"
#include "mp3.h"
#include "blur.h"
#include "particle_header.h"

#include <windows.h>
#include <gl\gl.h>

#define MAX_CLIENTS 32
extern rain_properties Rain;

vec3_t FogColor;
float g_iFogColor[3];
float g_iStartDist;
float g_iEndDist;

float m_iBlurActive;

extern BEAM *pBeam;
extern BEAM *pBeam2;

extern "C"
{
#include "pm_shared.h"
}

/// USER-DEFINED SERVER MESSAGE HANDLERS

int CHud :: MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf )
{
	ASSERT( iSize == 0 );

	g_iStartDist = 0.0; 
	g_iEndDist = 0.0;

	m_Lensflare.SunEnabled = FALSE;

	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while ( pList )
	{
		if ( pList->p )
			pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	return 1;
}

void CAM_ToFirstPerson(void);

void CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	CAM_ToFirstPerson();
}
void CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	g_iStartDist = 0.0; 
	g_iEndDist = 0.0;

	while (pList)
	{
		if ( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}

	m_iBlurActive = FALSE; // Reset this
	gHUD.m_iRoundtime = -1; // Reset timer
	m_Lensflare.SunEnabled = FALSE;
	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;

}


int CHud :: MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_Teamplay = READ_BYTE();

	return 1;
}


int CHud :: MsgFunc_Damage(const char *pszName, int iSize, void *pbuf )
{
	int		armor, blood;
	Vector	from;
	int		i;
	float	count;
	
	BEGIN_READ( pbuf, iSize );
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i=0 ; i<3 ; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iConcussionEffect = READ_BYTE();
	if (m_iConcussionEffect)
		this->m_StatusIcons.EnableIcon("dmg_concuss",255,160,0);
	else
		this->m_StatusIcons.DisableIcon("dmg_concuss");
	return 1;
}
int CHud :: MsgFunc_RainData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
		Rain.dripsPerSecond =	READ_SHORT();
		Rain.distFromPlayer =	READ_COORD();
		Rain.windX =			READ_COORD();
		Rain.windY =			READ_COORD();
		Rain.randX =			READ_COORD();
		Rain.randY =			READ_COORD();
		Rain.weatherMode =		READ_SHORT();
		Rain.globalHeight =		READ_COORD();
		
	return 1;
}
int CHud :: MsgFunc_PlayMP3( const char *pszName, int iSize, void *pbuf ) //AJH -Killar MP3
{
	BEGIN_READ( pbuf, iSize );

	gMP3.PlayMP3( READ_STRING(), NULL );

	return 1;
}
int CHud :: MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf )
{

	BEGIN_READ( pbuf, iSize );
	FogColor.x = TransformColor ( READ_SHORT() );
	FogColor.y = TransformColor ( READ_SHORT() );
	FogColor.z = TransformColor ( READ_SHORT() );
	g_iStartDist = READ_SHORT();
	g_iEndDist = READ_SHORT();

	return 1;
}
int CHud :: MsgFunc_MotionBlur( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iBlurActive = READ_BYTE();

	return 1;
}
int CHud :: MsgFunc_Materials(const char *pszName, int iSize, void *pbuf )
{
	const char *levelname = gEngfuncs.pfnGetLevelName();

	if ( strcmp( levelname, "" ) )
		PM_InitTextureTypes( levelname );

	return 1;
}