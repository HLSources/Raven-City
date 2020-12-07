//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

// manages light rendering

#include "windows.h"
#include "gl/gl.h"
#include "glext.h"

#include "hud.h"
#include "cl_util.h"
#include "r_efx.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include "ref_params.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

extern int IsEntityTransparent(cl_entity_t *e);
extern int IsEntityMoved(cl_entity_t *e);

cl_entity_t *m_pRenderEnts[1024];
int		m_iNumRenderEnts;
int		m_iDlightIndex;
/*
====================
Update the list of lights
each frame.

This func is needed by the studio
renderer to get a list of light
entities. It was merged with that
for optimization purposes.
====================
*/
void UpdateLightEntities ( void )
{
	if( gEngfuncs.pfnGetCvarFloat( "r_dynamic" ) <= 0 
		&& gEngfuncs.pfnGetCvarFloat( "r_shadows" ) <= 0 
		|| gEngfuncs.pfnGetCvarFloat( "r_dynamic" ) <= 0 
		&& gEngfuncs.pfnGetCvarFloat( "r_shadows_dynamic" ) <= 0)
		return;

	m_iNumRenderEnts = 0;

	cl_entity_t *m_pPlayer = gEngfuncs.GetLocalPlayer();

	if (!m_pPlayer)
		return;

	cl_entity_t *viewent = gEngfuncs.GetViewModel();

	int m_iCurMsg = m_pPlayer->curstate.messagenum;

	for ( int i = 1; i < 1024; i++ )
	{
		cl_entity_t *m_pEntity = gEngfuncs.GetEntityByIndex( i );

		if (!m_pEntity)
			break;

		if (!m_pEntity->model)
			continue;

		if (m_pEntity->curstate.messagenum != m_iCurMsg)
			continue;

		if (m_pEntity->curstate.effects & EF_NODRAW)
			continue;

		if (m_pEntity == viewent)
			continue;

		if ( m_pEntity->curstate.renderfx == 69 
			|| m_pEntity->curstate.renderfx == 70 
			|| m_pEntity->curstate.renderfx == 71
			|| m_pEntity->model->type == mod_brush 
			&& !IsEntityTransparent(m_pEntity)
			&& !IsEntityMoved(m_pEntity))
		{
			m_pRenderEnts[m_iNumRenderEnts] = m_pEntity;
			m_iNumRenderEnts++;
		}
	}
}
/*
====================
Render this entity light.
====================
*/
void RenderForEntityLight ( cl_entity_t *pLightEntity )
{
	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocElight( pLightEntity->curstate.number );

	dl->origin = pLightEntity->curstate.origin;
	dl->color = pLightEntity->curstate.rendercolor;
	dl->radius = pLightEntity->curstate.renderamt;

	dl->die = gEngfuncs.GetClientTime() + 0.1;
}
/*
====================
Render this dynamic light.
====================
*/
void RenderForDynamicLight ( cl_entity_t *pLightEntity )
{
	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( m_iDlightIndex );

	dl->origin = pLightEntity->curstate.origin;
	dl->color = pLightEntity->curstate.rendercolor;
	dl->radius = pLightEntity->curstate.renderamt;

	dl->die = gEngfuncs.GetClientTime() + 0.1;

	m_iDlightIndex++;
}
/*
====================
Render all of the lights.
====================
*/
void UpdateLightSystem( struct ref_params_s *pparams )
{
	if( gEngfuncs.pfnGetCvarFloat( "r_dynamic" ) <= 0)
		return;

	if ( !m_iNumRenderEnts )
		return;

	if ( pparams->paused )
		return;

	m_iDlightIndex = 0;

	for ( int i = 0; i < m_iNumRenderEnts; i++ )
	{
		if ( m_pRenderEnts[i]->curstate.renderfx == 69 )
			RenderForEntityLight( m_pRenderEnts[i] );
		else if ( m_pRenderEnts[i]->curstate.renderfx == 70 )
			RenderForDynamicLight( m_pRenderEnts[i] );
	}
}