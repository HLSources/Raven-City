//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#include "hud.h"
#include "cl_util.h"
#include "windows.h"
#include "gl/gl.h"
#include "glext.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI
#include "util.h"
#include "rain.h"
#include "const.h"
#include "r_efx.h"
#include "studio.h"
#include "pm_defs.h"
#include "event_api.h"
#include "com_model.h"
#include "event_args.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "custom_alloc.h"
#include "particle_header.h"
#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

void GL_StencilShadowing ( void );

extern cvar_t* g_EnableParticle;
extern vec3_t FogColor;
extern int g_iWaterLevel;

extern float g_iFogColor[4]; 
extern float g_iStartDist; 
extern float g_iEndDist; 

extern engine_studio_api_t IEngineStudio;
extern CGameStudioModelRenderer g_StudioRenderer;

#define DLLEXPORT __declspec( dllexport )
extern "C"
{
	void DLLEXPORT HUD_DrawNormalTriangles( void );
	void DLLEXPORT HUD_DrawTransparentTriangles( void );
};
#include "r_studioint.h"

//-----------------------------------------------------

void SetPoint( float x, float y, float z, float (*matrix)[4])
{
	vec3_t point, result;
	point[0] = x;
	point[1] = y;
	point[2] = z;

	VectorTransformTriAPI(point, matrix, result);

	gEngfuncs.pTriAPI->Vertex3f(result[0], result[1], result[2]);
}

#if defined( TEST_IT )
/*
=================
Draw_Triangles

Example routine.  Draws a sprite offset from the player origin.
=================
*/
void Draw_Triangles( void )
{
	cl_entity_t *player;
	vec3_t org;

	// Load it up with some bogus data
	player = gEngfuncs.GetLocalPlayer();
	if ( !player )
		return;

	org = player->origin;

	org.x += 50;
	org.y += 50;

	if (gHUD.m_hsprCursor == 0)
	{
		char sz[256];
		sprintf( sz, "sprites/cursor.spr" );
		gHUD.m_hsprCursor = SPR_Load( sz );
	}

	if ( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( gHUD.m_hsprCursor ), 0 ))
	{
		return;
	}
	
	// Create a triangle, sigh
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	// Overload p->color with index into tracer palette, p->packedColor with brightness
	gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, 1.0 );
	// UNDONE: This gouraud shading causes tracers to disappear on some cards (permedia2)
	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( org.x, org.y, org.z );

	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( org.x, org.y + 50, org.z );

	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( org.x + 50, org.y + 50, org.z );

	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
	gEngfuncs.pTriAPI->Vertex3f( org.x + 50, org.y, org.z );

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
}

#endif

/*
=================================
DrawRain
=================================
*/
extern MemBlock<cl_drip>	g_dripsArray;
extern MemBlock<cl_rainfx>	g_fxArray;
extern rain_properties Rain;

void DrawRain( void )
{
	if (g_dripsArray.IsClear())
		return; // no drips to draw

	HSPRITE hsprTexture;
	const model_s *pTexture;
	float visibleHeight = Rain.globalHeight - SNOWFADEDIST;

	if (Rain.weatherMode == 0 || Rain.weatherMode == 3)
		hsprTexture = LoadSprite( "sprites/hi_rain.spr" ); // load rain sprite
	else 
		hsprTexture = LoadSprite( "sprites/snowflake.spr" ); // load snow sprite

	if (!hsprTexture)
	{
		gEngfuncs.Con_Printf("ERROR: hi_rain.spr or snowflake.spr could not be loaded!\n");
		return;
	}

	// usual triapi stuff
	pTexture = gEngfuncs.GetSpritePointer( hsprTexture );
	gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)pTexture, 0 );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	
	// go through drips list
	g_dripsArray.StartPass();
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	cl_drip* Drip = g_dripsArray.GetCurrent();

	if ( Rain.weatherMode == 0 )
	{ // draw rain
		while (Drip != NULL)
		{
					
			Vector2D toPlayer; 
			toPlayer.x = player->origin[0] - Drip->origin[0];
			toPlayer.y = player->origin[1] - Drip->origin[1];
			toPlayer = toPlayer.Normalize();
	
			toPlayer.x *= DRIP_SPRITE_HALFWIDTH;
			toPlayer.y *= DRIP_SPRITE_HALFWIDTH;

			float shiftX = (Drip->xDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;
			float shiftY = (Drip->yDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;

		// --- draw triangle --------------------------
			gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, Drip->alpha );
			gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				gEngfuncs.pTriAPI->Vertex3f( Drip->origin[0]-toPlayer.y - shiftX, Drip->origin[1]+toPlayer.x - shiftY,
					Drip->origin[2] + DRIP_SPRITE_HALFHEIGHT );

				gEngfuncs.pTriAPI->TexCoord2f( 0.5, 1 );
				gEngfuncs.pTriAPI->Vertex3f( Drip->origin[0] + shiftX, Drip->origin[1] + shiftY,
					Drip->origin[2]-DRIP_SPRITE_HALFHEIGHT );

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				gEngfuncs.pTriAPI->Vertex3f( Drip->origin[0]+toPlayer.y - shiftX, Drip->origin[1]-toPlayer.x - shiftY,
					Drip->origin[2]+DRIP_SPRITE_HALFHEIGHT);
	
			gEngfuncs.pTriAPI->End();
		// --- draw triangle end ----------------------

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}
	}
	else if ( Rain.weatherMode == 2 ) // draw fog particles
	{
		vec3_t normal;
		gEngfuncs.GetViewAngles((float*)normal);
	
		float matrix[3][4];
		AngleMatrixTriAPI (normal, matrix);	// calc view matrix

		while (Drip != NULL)
		{

			matrix[0][3] = Drip->origin[0]; // write origin to matrix
			matrix[1][3] = Drip->origin[1];
			matrix[2][3] = Drip->origin[2];

			// apply start fading effect
			float alpha = (Drip->origin[2] <= visibleHeight) ? Drip->alpha :
				((Rain.globalHeight - Drip->origin[2]) / (float)SNOWFADEDIST) * Drip->alpha;
					
			float flSize = FOG_SPRITE_HALFSIZE;

			// --- draw triangles start ----------------------
			gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, alpha );
			gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				SetPoint(0, flSize ,flSize, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint(0, flSize ,-flSize, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint(0, -flSize ,flSize, matrix);

				// Repeats for triangles.

				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint(0, flSize ,-flSize, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				SetPoint(0, -flSize ,-flSize, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint(0, -flSize ,flSize, matrix);

			gEngfuncs.pTriAPI->End();
			// --- draw triangles end ----------------------

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}
	}
	else if ( Rain.weatherMode == 3 )
	{ // draw rain
		while (Drip != NULL)
		{
			Vector2D toPlayer; 
			toPlayer.x = player->origin[0] - Drip->origin[0];
			toPlayer.y = player->origin[1] - Drip->origin[1];
			toPlayer = toPlayer.Normalize();
	
			toPlayer.x *= DRIP_LONG_SPRITE_HALFWIDTH;
			toPlayer.y *= DRIP_LONG_SPRITE_HALFWIDTH;

			float shiftX = (Drip->xDelta / DRIPSPEED) * DRIP_LONG_SPRITE_HALFHEIGHT;
			float shiftY = (Drip->yDelta / DRIPSPEED) * DRIP_LONG_SPRITE_HALFHEIGHT;

		// --- draw triangle --------------------------
			gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, Drip->alpha );
			gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				gEngfuncs.pTriAPI->Vertex3f( Drip->origin[0]-toPlayer.y - shiftX, Drip->origin[1]+toPlayer.x - shiftY,
					Drip->origin[2] + DRIP_LONG_SPRITE_HALFHEIGHT );

				gEngfuncs.pTriAPI->TexCoord2f( 0.5, 1 );
				gEngfuncs.pTriAPI->Vertex3f( Drip->origin[0] + shiftX, Drip->origin[1] + shiftY,
					Drip->origin[2]-DRIP_LONG_SPRITE_HALFHEIGHT );

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				gEngfuncs.pTriAPI->Vertex3f( Drip->origin[0]+toPlayer.y - shiftX, Drip->origin[1]-toPlayer.x - shiftY,
					Drip->origin[2]+DRIP_LONG_SPRITE_HALFHEIGHT);
	
			gEngfuncs.pTriAPI->End();
		// --- draw triangle end ----------------------

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}
	}
	else
	{ // draw snow
		vec3_t normal;
		gEngfuncs.GetViewAngles((float*)normal);
	
		float matrix[3][4];
		AngleMatrixTriAPI (normal, matrix);	// calc view matrix

		while (Drip != NULL)
		{

			matrix[0][3] = Drip->origin[0]; // write origin to matrix
			matrix[1][3] = Drip->origin[1];
			matrix[2][3] = Drip->origin[2];

			// apply start fading effect
			float alpha = (Drip->origin[2] <= visibleHeight) ? Drip->alpha :
				((Rain.globalHeight - Drip->origin[2]) / (float)SNOWFADEDIST) * Drip->alpha;
					
		// --- draw triangles start --------------------------
			gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, alpha );
			gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				SetPoint(0, SNOW_SPRITE_HALFSIZE ,SNOW_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint(0, SNOW_SPRITE_HALFSIZE ,-SNOW_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint(0, -SNOW_SPRITE_HALFSIZE ,SNOW_SPRITE_HALFSIZE, matrix);

				// Repeats for triangles.
				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint(0, SNOW_SPRITE_HALFSIZE ,-SNOW_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				SetPoint(0, -SNOW_SPRITE_HALFSIZE ,-SNOW_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint(0, -SNOW_SPRITE_HALFSIZE ,SNOW_SPRITE_HALFSIZE, matrix);

			gEngfuncs.pTriAPI->End();

		// --- draw triangles end ----------------------

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}
	}
}
/*
=================================
DrawFXObjects
=================================
*/
void DrawFXObjects( void )
{
	if (g_fxArray.IsClear())
		return; // no objects to draw

	float curtime = gEngfuncs.GetClientTime();

	g_fxArray.StartPass();
	cl_rainfx* curFX = g_fxArray.GetCurrent();

	// usual triapi stuff
	HSPRITE hsprTexture;
	const model_s *pTexture;

	if ( Rain.weatherMode != 3 )
		hsprTexture = LoadSprite( "sprites/waterring.spr" ); // load water ring sprite
	else
		hsprTexture = LoadSprite( "sprites/wsplash3.spr" ); // load water ring sprite

	pTexture = gEngfuncs.GetSpritePointer( hsprTexture );

	int frame = (int)(curFX->birthTime * 15) % SPR_Frames(hsprTexture);

	gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)pTexture, frame );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	
	while (curFX != NULL)
	{
		if ( Rain.weatherMode != 3 )
		{
			// fadeout
			float alpha = ((curFX->birthTime + curFX->life - curtime) / curFX->life) * curFX->alpha;
			float size = (curtime - curFX->birthTime) * MAXRINGHALFSIZE;

			// --- draw triangles --------------------------
				gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, alpha );
				gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );

					gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
					gEngfuncs.pTriAPI->Vertex3f(curFX->origin[0] - size, curFX->origin[1] - size, curFX->origin[2]);

					gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
					gEngfuncs.pTriAPI->Vertex3f(curFX->origin[0] - size, curFX->origin[1] + size, curFX->origin[2]);

					gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
					gEngfuncs.pTriAPI->Vertex3f(curFX->origin[0] + size, curFX->origin[1] + size, curFX->origin[2]);

					// Repeats for triangles.
					gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
					gEngfuncs.pTriAPI->Vertex3f(curFX->origin[0] - size, curFX->origin[1] - size, curFX->origin[2]);

					gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
					gEngfuncs.pTriAPI->Vertex3f(curFX->origin[0] + size, curFX->origin[1] - size, curFX->origin[2]);

					gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
					gEngfuncs.pTriAPI->Vertex3f(curFX->origin[0] + size, curFX->origin[1] + size, curFX->origin[2]);

				gEngfuncs.pTriAPI->End();

			// --- draw triangles end ----------------------

			g_fxArray.MoveNext();
			curFX = g_fxArray.GetCurrent();
		}
		else
		{
			vec3_t normal;
			gEngfuncs.GetViewAngles((float*)normal);
		
			float matrix[3][4];
			AngleMatrixTriAPI (normal, matrix);	// calc view matrix

			matrix[0][3] = curFX->origin[0]; // write origin to matrix
			matrix[1][3] = curFX->origin[1];
			matrix[2][3] = curFX->origin[2];

			// apply start fading effect
			float alpha = ((curFX->birthTime + curFX->life - curtime) / curFX->life) * curFX->alpha;

			// --- draw triangles start --------------------------
			gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, alpha );
			gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				SetPoint(0, SPLASH_SPRITE_HALFSIZE ,SPLASH_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint(0, SPLASH_SPRITE_HALFSIZE ,-SPLASH_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint(0, -SPLASH_SPRITE_HALFSIZE ,SPLASH_SPRITE_HALFSIZE, matrix);

				// Repeats for triangles.
				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint(0, SPLASH_SPRITE_HALFSIZE ,-SPLASH_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				SetPoint(0, -SPLASH_SPRITE_HALFSIZE ,-SPLASH_SPRITE_HALFSIZE, matrix);

				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint(0, -SPLASH_SPRITE_HALFSIZE ,SPLASH_SPRITE_HALFSIZE, matrix);

			gEngfuncs.pTriAPI->End();
			// --- draw triangles end ----------------------

			g_fxArray.MoveNext();
			curFX = g_fxArray.GetCurrent();
		}
	}
}

void BlackFog ( bool GL = true )
{
	static float fColorBlack[3] = {0,0,0};
	bool bFog = g_iStartDist > 0 && g_iEndDist > 0;

	if (bFog)
	{
		if ( !GL || gHUD.SteamFog->value >= 1 )
		{
			gEngfuncs.pTriAPI->Fog ( fColorBlack, g_iStartDist, g_iEndDist, bFog );
		}
		else
		{
			glEnable(GL_FOG);
			glFogi (GL_FOG_MODE, GL_LINEAR);
			glFogfv (GL_FOG_COLOR, fColorBlack);
			glFogf (GL_FOG_DENSITY, 1.0f);
			glHint (GL_FOG_HINT, GL_NICEST);
			glFogf (GL_FOG_START, g_iStartDist);
			glFogf (GL_FOG_END, g_iEndDist);
		}
	}
	else
	{
		if ( !GL || gHUD.SteamFog->value >= 1 )
		{
			gEngfuncs.pTriAPI->Fog ( fColorBlack, g_iStartDist, g_iEndDist, bFog );
		}
		else
		{
			glDisable( GL_FOG );
		}
	}
}

void RenderFog ( void ) 
{
	float g_iFogColor[4] = { FogColor.x, FogColor.y, FogColor.z, 1.0 };
	bool bFog = g_iStartDist > 0 && g_iEndDist > 0;
	if ( bFog )
	{
		if ( IEngineStudio.IsHardware() == 2 )
		{
			gEngfuncs.pTriAPI->Fog ( g_iFogColor, g_iStartDist, g_iEndDist, bFog );
		}
		else if( IEngineStudio.IsHardware() == 1 )
		{
			glEnable(GL_FOG);
			glFogi (GL_FOG_MODE, GL_LINEAR);
			glFogfv (GL_FOG_COLOR, g_iFogColor);
			glFogf (GL_FOG_DENSITY, 1.0f);
			glHint (GL_FOG_HINT, GL_NICEST);
			glFogf (GL_FOG_START, g_iStartDist);
			glFogf (GL_FOG_END, g_iEndDist);
		}
	}
	else
	{
		if ( IEngineStudio.IsHardware() == 2 )
		{
			gEngfuncs.pTriAPI->Fog ( g_iFogColor, g_iStartDist, g_iEndDist, bFog );
		}
		else if( IEngineStudio.IsHardware() == 1 )
		{
			glDisable(GL_FOG);
		}
	}
}

void DisableFog ( void )
{
	glDisable(GL_FOG);
}
/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles( void )
{
	RenderFog();

	GL_StencilShadowing();

	gHUD.m_Spectator.DrawOverview();

#if defined( TEST_IT )
	Draw_Triangles();
#endif
}
/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
class CException;
void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
	DisableFog();
	//========== Particle Engine ==========//
	if( g_EnableParticle->value >= 1) //If enabled, render the particles
	{
		try 
		{
			pParticleManager->UpdateSystems();
		} 
		catch( CException *e ) 
		{
			e;
			e = NULL;
			gEngfuncs.Con_Printf("There was a serious error within the particle engine. Particles will return on map change\n");
			delete pParticleManager;
			pParticleManager = NULL;
		}
	}
	//========== Particle Engine ==========//
	DisableFog();

	//This is past the point we need to worry about switching fog types.
	BlackFog();// Start drawing our black fog. 

	ProcessFXObjects();
	ProcessRain();
	DrawRain();
	DrawFXObjects();

	DisableFog();
	BlackFog( false );

#if defined( TEST_IT )
	Draw_Triangles();
#endif
}