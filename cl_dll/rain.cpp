//=========================================
// rain.cpp
// written by BUzer
//=========================================


#include <memory.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "cdll_int.h"
#include "pm_defs.h"
#include "event_api.h"

#include "rain.h"
#include "custom_alloc.h"

void WaterLandingEffect(cl_drip *drip);


rain_properties     Rain;

MemBlock<cl_drip>	g_dripsArray( MAXDRIPS );
MemBlock<cl_rainfx>	g_fxArray( MAXFX );

double rain_curtime;    // current time
double rain_oldtime;    // last time we have updated drips
double rain_timedelta;  // difference between old time and current time
double rain_nextspawntime;  // when the next drip should be spawned

int dripcounter = 0;
int fxcounter = 0;

/*
=================================
ProcessRain
Перемещает существующие объекты, удаляет их при надобности,
и, если дождь включен, создает новые.

Должна вызываться каждый кадр.
=================================
*/
void ProcessRain( void )
{
	rain_oldtime = rain_curtime; // save old time
	rain_curtime = gEngfuncs.GetClientTime();
	rain_timedelta = rain_curtime - rain_oldtime;

	// first frame
	if (rain_oldtime == 0)
	{
		// fix first frame bug with nextspawntime
		rain_nextspawntime = gEngfuncs.GetClientTime();
		return;
	}

	if (Rain.dripsPerSecond == 0 && g_dripsArray.IsClear())
	{
		// keep nextspawntime correct
		rain_nextspawntime = rain_curtime;
		return;
	}

	if (rain_timedelta == 0)
		return; // not in pause

	double timeBetweenDrips = 1 / (double)Rain.dripsPerSecond;

	if (!g_dripsArray.StartPass())
	{
		Rain.dripsPerSecond = 0; // disable rain
		gEngfuncs.Con_Printf( "Rain error: failed to allocate memory block!\n");
		return;
	}

	cl_drip* curDrip = g_dripsArray.GetCurrent();	
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();

	// хранение отладочной информации
	float debug_lifetime = 0;
	int debug_howmany = 0;
	int debug_attempted = 0;
	int debug_dropped = 0;

	while (curDrip != NULL) // go through list
	{
	//	nextDrip = curDrip->p_Next; // save pointer to next drip

		if ( Rain.weatherMode == 0 || Rain.weatherMode == 3 )
			curDrip->origin.z -= rain_timedelta * DRIPSPEED; // rain
		if ( Rain.weatherMode == 2 )
			curDrip->origin.z -= rain_timedelta * FOGSPEED; // rain
		else
			curDrip->origin.z -= rain_timedelta * SNOWSPEED; // snow

		curDrip->origin.x += rain_timedelta * curDrip->xDelta;
		curDrip->origin.y += rain_timedelta * curDrip->yDelta;
		
		// remove drip if its origin lower than minHeight
		if (curDrip->origin.z < curDrip->minHeight) 
		{
			if (curDrip->landInWater && Rain.weatherMode == 0 || Rain.weatherMode == 3 )
			{
				WaterLandingEffect(curDrip); // create water rings
			}
			if (gHUD.RainInfo->value)
			{
				debug_lifetime += (rain_curtime - curDrip->birthTime);
				debug_howmany++;
			}
			
			g_dripsArray.DeleteCurrent();
					
			dripcounter--;
		}
		else
			g_dripsArray.MoveNext();

		curDrip = g_dripsArray.GetCurrent();
	}

	int maxDelta; // maximum height randomize distance
	float falltime;
	if (Rain.weatherMode == 0 || Rain.weatherMode == 3)
	{
		maxDelta = DRIPSPEED * rain_timedelta; // for rain
		falltime = (Rain.globalHeight + 4096) / DRIPSPEED;
	}
	if (Rain.weatherMode == 2)
	{
		maxDelta = FOGSPEED * rain_timedelta; // for rain
		falltime = (Rain.globalHeight + 4096) / FOGSPEED;
	}
	else
	{
		maxDelta = SNOWSPEED * rain_timedelta; // for snow
		falltime = (Rain.globalHeight + 4096) / SNOWSPEED;
	}

	while (rain_nextspawntime < rain_curtime)
	{
		rain_nextspawntime += timeBetweenDrips;		
		if (gHUD.RainInfo->value)
			debug_attempted++;
				
		if (dripcounter < MAXDRIPS) // check for overflow
		{
			float deathHeight;
			vec3_t vecStart, vecEnd;

			vecStart[0] = gEngfuncs.pfnRandomFloat(player->origin.x - Rain.distFromPlayer, player->origin.x + Rain.distFromPlayer);
			vecStart[1] = gEngfuncs.pfnRandomFloat(player->origin.y - Rain.distFromPlayer, player->origin.y + Rain.distFromPlayer);
			vecStart[2] = Rain.globalHeight;

			float xDelta = Rain.windX + gEngfuncs.pfnRandomFloat(Rain.randX * -1, Rain.randX);
			float yDelta = Rain.windY + gEngfuncs.pfnRandomFloat(Rain.randY * -1, Rain.randY);

			// find a point at bottom of map
			vecEnd[0] = vecStart[0] + (falltime * xDelta);
			vecEnd[1] = vecStart[1] + (falltime * yDelta);
			vecEnd[2] = vecStart[2] - 4096;

			pmtrace_t pmtrace;
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, vecEnd, PM_STUDIO_IGNORE, -1, &pmtrace );

			if (pmtrace.startsolid)
			{
				if (gHUD.RainInfo->value)
					debug_dropped++;

				continue; // drip cannot be placed
			}
			
			// falling to water?
			int contents = gEngfuncs.PM_PointContents( pmtrace.endpos, NULL );
			if (contents == CONTENTS_WATER)
			{
				int waterEntity = gEngfuncs.PM_WaterEntity( pmtrace.endpos );
				if ( waterEntity > 0 )
				{
					cl_entity_t *pwater = gEngfuncs.GetEntityByIndex( waterEntity );
					if ( pwater && ( pwater->model != NULL ) )
					{
						deathHeight = pwater->curstate.maxs[2];
					}
					else
					{
						continue;
					}
				}
				else
				{
					continue;
				}
			}
			else
			{
				deathHeight = pmtrace.endpos[2];
			}

			// just in case..
			if (deathHeight > vecStart[2])
			{
				continue;
			}


			cl_drip *newClDrip = g_dripsArray.Allocate();
			if (!newClDrip)
			{
				Rain.dripsPerSecond = 0; // disable rain
				return;
			}
			
			vecStart[2] -= gEngfuncs.pfnRandomFloat(0, maxDelta); // randomize a bit
			
			if ( Rain.weatherMode == 2 )
				newClDrip->alpha = gEngfuncs.pfnRandomFloat(0.05, 0.02);
			else
				newClDrip->alpha = gEngfuncs.pfnRandomFloat(0.12, 0.2);

			VectorCopy(vecStart, newClDrip->origin);
			
			newClDrip->xDelta = xDelta;
			newClDrip->yDelta = yDelta;
	
			newClDrip->birthTime = rain_curtime; // store time when it was spawned
			newClDrip->minHeight = deathHeight;

			if (contents == CONTENTS_WATER)
				newClDrip->landInWater = 1;
			else
				newClDrip->landInWater = 0;

			dripcounter++;
		}
		else
		{
			return;
		}
	}

	if (gHUD.RainInfo->value) // print debug info
	{
		gEngfuncs.Con_Printf( "Rain info: Drips exist: %i\n", dripcounter );
		gEngfuncs.Con_Printf( "Rain info: FX's exist: %i\n", fxcounter );
		gEngfuncs.Con_Printf( "Rain info: Attempted/Dropped: %i, %i\n", debug_attempted, debug_dropped);
		if (debug_howmany)
		{
			float ave = debug_lifetime / (float)debug_howmany;
			gEngfuncs.Con_Printf( "Rain info: Average drip life time: %f\n", ave);
		}
		else
			gEngfuncs.Con_Printf( "Rain info: Average drip life time: --\n");
	}
	return;
}

/*
=================================
WaterLandingEffect
создает круг на водной поверхности
=================================
*/
void WaterLandingEffect(cl_drip *drip)
{
	if (fxcounter >= MAXFX)
	{
		gEngfuncs.Con_Printf( "Rain error: FX limit overflow!\n" );
		return;
	}	
	
	cl_rainfx *newFX = g_fxArray.Allocate();
	if (!newFX)
	{
		gEngfuncs.Con_Printf( "Rain error: failed to allocate FX object!\n");
		return;
	}
			
	newFX->alpha = gEngfuncs.pfnRandomFloat(0.6, 0.9);

	VectorCopy(drip->origin, newFX->origin);
	if ( Rain.weatherMode == 3 )
		newFX->origin[2] = drip->minHeight + SPLASH_SPRITE_HALFSIZE; // correct position
	else
		newFX->origin[2] = drip->minHeight; // correct position

	newFX->birthTime = gEngfuncs.GetClientTime();
	newFX->life = gEngfuncs.pfnRandomFloat(0.7, 1);
	fxcounter++;
}

/*
=================================
ProcessFXObjects
удаляет FX объекты, у которых вышел срок жизни

Каждый кадр вызывается перед ProcessRain
=================================
*/
void ProcessFXObjects( void )
{
	float curtime = gEngfuncs.GetClientTime();
	
	if (!g_fxArray.StartPass())
	{
		Rain.dripsPerSecond = 0; // disable rain
		gEngfuncs.Con_Printf( "Rain error: failed to allocate memory block for fx objects!\n");
		return;
	}

	cl_rainfx* curFX = g_fxArray.GetCurrent();

	while (curFX != NULL) // go through FX objects list
	{
		// delete current?
		if ((curFX->birthTime + curFX->life) < curtime)
		{
			g_fxArray.DeleteCurrent();
			fxcounter--;
		}
		else
			g_fxArray.MoveNext();

		curFX = g_fxArray.GetCurrent();
	}
}

/*
=================================
ResetRain
очищает память, удаляя все объекты.
=================================
*/
void ResetRain( void )
{
	g_dripsArray.Clear();
	g_fxArray.Clear();
	
	dripcounter = 0;
	fxcounter = 0;

	InitRain();
	return;
}

/*
=================================
InitRain
Инициализирует все переменные нулевыми значениями.
=================================
*/
void InitRain( void )
{
	Rain.dripsPerSecond = 0;
	Rain.distFromPlayer = 0;
	Rain.windX = 0;
	Rain.windY = 0;
	Rain.randX = 0;
	Rain.randY = 0;
	Rain.weatherMode = 0;
	Rain.globalHeight = 0;
	
	rain_oldtime = 0;
	rain_curtime = 0;
	rain_nextspawntime = 0;

	return;
}

