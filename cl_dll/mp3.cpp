//========= Copyright � 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//mp3 support added by Killar

#include "hud.h"
#include "cl_util.h"
#include "mp3.h"

int CMP3::Initialize()
{

	char fmodlib[256];
	
	m_iIsPlaying = 0;
	sprintf( fmodlib, "%s/cl_dlls/mp3dec.dll", gEngfuncs.pfnGetGameDirectory());
	// replace forward slashes with backslashes
	for( int i=0; i < 256; i++ )
		if( fmodlib[i] == '/' ) fmodlib[i] = '\\';
	
	m_hFMod = LoadLibrary( fmodlib );

	if( m_hFMod != NULL )
	{
		// fill in the function pointers
		(FARPROC&)VER = 	GetProcAddress(m_hFMod, "_FSOUND_GetVersion@0");
		(FARPROC&)SCL = 	GetProcAddress(m_hFMod, "_FSOUND_Stream_Close@4");
		(FARPROC&)SOP = 	GetProcAddress(m_hFMod, "_FSOUND_SetOutput@4");
		(FARPROC&)SBS = 	GetProcAddress(m_hFMod, "_FSOUND_SetBufferSize@4");
		(FARPROC&)SDRV = 	GetProcAddress(m_hFMod, "_FSOUND_SetDriver@4");
		(FARPROC&)INIT = 	GetProcAddress(m_hFMod, "_FSOUND_Init@12");
		(FARPROC&)SOF = 	GetProcAddress(m_hFMod, "_FSOUND_Stream_OpenFile@12");
		(FARPROC&)SO = 		GetProcAddress(m_hFMod, "_FSOUND_Stream_Open@16");//AJH Use new version of fmod
		(FARPROC&)SPLAY = 	GetProcAddress(m_hFMod, "_FSOUND_Stream_Play@8");
		(FARPROC&)CLOSE = 	GetProcAddress(m_hFMod, "_FSOUND_Close@0");
		
		if( !(SCL && SOP && SBS && SDRV && INIT && (SOF||SO) && SPLAY && CLOSE) )
		{
			FreeLibrary( m_hFMod );
			gEngfuncs.Con_Printf("Fatal Error: FMOD functions couldn't be loaded!\n");
			return 0;
		}
	}

	else
	{
		gEngfuncs.Con_Printf("Fatal Error: FMOD library couldn't be loaded!\n");
		return 0;
	}
	gEngfuncs.Con_Printf("mp3dec.dll loaded succesfully!\n");
	return 1;
}

int CMP3::Shutdown()
{
	if( m_hFMod )
	{
		CLOSE();

		FreeLibrary( m_hFMod );
		m_hFMod = NULL;
		m_iIsPlaying = 0;
		return 1;
	}
	else
		return 0;
}

int CMP3::StopMP3( void )
{
	SCL( m_Stream );
	m_iIsPlaying = 0;
	return 1;
}

int CMP3::PlayMP3( const char *pszSong, BOOL m_fIsLooped )
{
	if( m_iIsPlaying )
	{
		// sound system is already initialized
		SCL( m_Stream );
	} 
	else
	{
		SOP( FSOUND_OUTPUT_DSOUND );
		SBS( 200 );
		SDRV( 0 );
		INIT( 44100, 1, 0 ); // we need just one channel, multiple mp3s at a time would be, erm, strange...	
	}//AJH not for really cool effects, say walking past cars in a street playing different tunes

	char song[256];

	sprintf( song, "%s/media/%s", gEngfuncs.pfnGetGameDirectory(), pszSong);

	gEngfuncs.Con_Printf("Using fmod.dll version %f\n",VER());
	if( SOF )
	{													
		m_Stream = SOF( song, FSOUND_NORMAL /*| FSOUND_LOOP_NORMAL*/, 1 );//G-Cont. kill loop sound
	}
	else if (SO)
	{
		m_Stream = SO( song, FSOUND_NORMAL /*| FSOUND_LOOP_NORMAL*/, 0 ,0);//G-Cont. kill loop sound
	}
	if(m_Stream)
	{
		SPLAY( 0, m_Stream );
		m_iIsPlaying = 1;
		return 1;
	}
	else
	{
		m_iIsPlaying = 0;
		gEngfuncs.Con_Printf("Error: Could not load %s\n",song);
		return 0;
	}
}