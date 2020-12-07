//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

/*
====== rain.h ========================================================
*/
#ifndef __RAIN_H__
#define __RAIN_H__

#define DRIPSPEED	900		// speed of raindrips (pixel per secs)
#define SNOWSPEED	200		// speed of snowflakes
#define SNOWFADEDIST	80
#define FOGSPEED 100

#define MAXDRIPS	2000	// max raindrops
#define MAXFX		3000	// max effects

#define DRIP_SPRITE_HALFHEIGHT	12
#define DRIP_SPRITE_HALFWIDTH	4

#define DRIP_LONG_SPRITE_HALFHEIGHT	46
#define DRIP_LONG_SPRITE_HALFWIDTH	8

#define SNOW_SPRITE_HALFSIZE	1.5
#define FOG_SPRITE_HALFSIZE		256
#define SPLASH_SPRITE_HALFSIZE	5
// radius water rings
#define MAXRINGHALFSIZE		25	

typedef struct
{
	int			dripsPerSecond;
	float		distFromPlayer;
	float		windX, windY;
	float		randX, randY;
	int			weatherMode;	// 0 - snow, 1 - rain
	float		globalHeight;
} rain_properties;


typedef struct cl_drip
{
	float		birthTime;
	float		minHeight;	// капля будет уничтожена на этой высоте.
	vec3_t		origin;
	float		alpha;

	float		xDelta;		// side speed
	float		yDelta;
	int			landInWater;
	
} cl_drip_t;

typedef struct cl_rainfx
{
	float		birthTime;
	float		life;
	vec3_t		origin;
	float		alpha;
} cl_rainfx_t;

 
void ProcessRain( void );
void ProcessFXObjects( void );
void ResetRain( void );
void InitRain( void );

#endif