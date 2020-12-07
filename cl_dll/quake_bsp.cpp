//===============================
// function for working with bsp levels, based on code from QW
//===============================

// manages elight rendering

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
#include "quake_bsp.h"

#include "ref_params.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
extern CGameStudioModelRenderer g_StudioRenderer;

void DrawPolyFromArray (glpoly_t *p);
void DisableVertexArray ( void );
void EnableVertexArray ( void );
void SetupBuffer ( void );
void ClearBuffer ( void );

extern bool g_bShadows;
model_t *g_pworld;
int		g_visframe;
int		g_framecount;

extern cl_entity_t *m_pRenderEnts[1024];
extern int	m_iNumRenderEnts;

extern cl_entity_t *m_pStencilEnts[1024];
extern int numStencilEnts;

extern PFNGLACTIVESTENCILFACEEXTPROC	glActiveStencilFaceEXT;
extern PFNGLACTIVETEXTUREARBPROC		glActiveTextureARB;

extern int use_vertex_array;

// 0  - success
// -1 - failed
int RecursiveLightPoint (mnode_t *node, const vec3_t &start, const vec3_t &end, vec3_t &outcolor)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	color24		*lightmap; // buz

	if (node->contents < 0)
		return -1;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end, outcolor);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = RecursiveLightPoint (node->children[side], start, mid, outcolor);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node
	model_t* world = gEngfuncs.GetEntityByIndex(0)->model; // buz

	surf = world->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{
			lightmap += dt * ((surf->extents[0]>>4)+1) + ds;
			
			// buz
			outcolor.x = lightmap->r;
			outcolor.y = lightmap->g;
			outcolor.z = lightmap->b;

		}		
		return r;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end, outcolor);
}

int R_LightPoint (vec3_t &p, vec3_t &outcolor)
{
	vec3_t		end;

	model_t* world = gEngfuncs.GetEntityByIndex(0)->model; // buz
	
	if (!world->lightdata)
	{
		outcolor[0] = outcolor[1] = outcolor[2] = 255; // buz
		return 255;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 8192;
	
	return RecursiveLightPoint (world->nodes, p, end, outcolor);
}






#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

// ========================================================
extern vec3_t render_origin;

mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t *node;
	float d;
	mplane_t *plane;

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}
void KillSky( void )
{
	model_t *world = gEngfuncs.GetEntityByIndex(0)->model;
	msurface_t* surfaces = world->surfaces;
	for (int i = 0; i < world->numsurfaces; i++)
	{
		if (surfaces[i].flags & SURF_DRAWSKY)
		{
			glpoly_t *p = surfaces[i].polys;
			p->numverts = -p->numverts;
		}
	}
}
/*
==============================

Stencil shadowing.

==============================
*/
/*
=================
R_DrawBrushModel
=================
*/
vec3_t vec_to_eyes;

void R_RotateForEntity (cl_entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);
}

int IsEntityMoved(cl_entity_t *e)
{
	if (e->angles[0] || e->angles[1] || e->angles[2] ||
		e->origin[0] || e->origin[1] || e->origin[2] ) // skybox models reques separate pass
		return TRUE;
	else
		return FALSE;
}

int IsEntityTransparent(cl_entity_t *e)
{
	if (e->curstate.rendermode == kRenderNormal)
		return FALSE;
	else
		return TRUE;
}

void RecursiveDrawWorld (mnode_t *node)
{
	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->visframe != g_visframe)
		return;
	
	if (node->contents < 0)
		return;	// faces already marked by engine

	if (gHUD.viewFrustum.R_CullBox (node->minmaxs, node->minmaxs+3))
		return;

	// recurse down the children, Order doesn't matter
	RecursiveDrawWorld (node->children[0]);
	RecursiveDrawWorld (node->children[1]);

	// draw stuff
	int c = node->numsurfaces;
	if (c)
	{
		msurface_t	*surf = g_pworld->surfaces + node->firstsurface;

		for ( ; c ; c--, surf++)
		{
			if (surf->visframe != g_framecount)
				continue;

			if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER))
				continue;

			glpoly_t *p = surf->polys;

			if ( !use_vertex_array )
			{
				float *v = p->verts[0];

				glBegin (GL_POLYGON);			
				for (int i = 0; i < p->numverts; i++, v+= VERTEXSIZE)
				{
					glVertex3fv (v);
				}
				glEnd ();
			}
			else
			{
				DrawPolyFromArray( p );
			}
		}
	}
}


void R_DrawBrushModel (cl_entity_t *e)
{
	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	int			rotated;
	vec3_t		trans;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (gHUD.viewFrustum.R_CullBox (mins, maxs))
		return;

	VectorSubtract (g_StudioRenderer.m_vRenderOrigin, e->origin, vec_to_eyes);

	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (vec_to_eyes, temp);
		AngleVectors (e->angles, forward, right, up);
		vec_to_eyes[0] = DotProduct (temp, forward);
		vec_to_eyes[1] = -DotProduct (temp, right);
		vec_to_eyes[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug

	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		pplane = psurf->plane;
		dot = DotProduct (vec_to_eyes, pplane->normal) - pplane->dist;

		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			psurf->visframe = g_framecount;
			DrawPolyFromArray(psurf->polys);
		}
	}
}
//================================
// GL_StencilShadowing -    Original code by BUzer, modified by Highlander.
//
// The main idea of this function is that the shadow volumes are rendered
// seperately from the model rendering pipeline, this we can render them
// after bmodels have rendered, allowing us to shadow those too while
// not having to render a gray screen square, thus preventing self shadowing.
// 
// This algorythm only needs one pass for stencil shadow volumes, thus saving
// on peformance. 
//================================
void GL_StencilShadowing ( void )
{
	if( g_StudioRenderer.m_pCvarShadows->value == 0 || !numStencilEnts )
		return;

	glPushAttrib( GL_ALL_ATTRIB_BITS );

	glActiveTextureARB( GL_TEXTURE1_ARB );
	glDisable(GL_TEXTURE_2D);
	glActiveTextureARB( GL_TEXTURE0_ARB );
	glDisable(GL_TEXTURE_2D);

	glDepthMask(GL_FALSE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Disable the near and far clip planes, for z-fail.
	glEnable(GL_DEPTH_CLAMP_NV);
	glDisable(GL_CULL_FACE);

	glEnable(GL_STENCIL_TEST);
	glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

	//Incrementing the stencil buffer.
	glActiveStencilFaceEXT(GL_FRONT);
	glStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

	//Decrement the stencil buffer.
	glActiveStencilFaceEXT(GL_BACK);
	glStencilOp(GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

	SetupBuffer();
	for ( int i = 0; i < numStencilEnts; i++ )
		g_StudioRenderer.StudioShadowForEntity( m_pStencilEnts[i] );
	ClearBuffer();

	glDisable(GL_DEPTH_CLAMP_NV);

	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f (0.005, 0.005, 0.005, 0.5f);

	glStencilFunc( GL_NOTEQUAL, 0, 0xFFFFFFFF );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

	glPushMatrix(); //save the current view matrix

	if ( g_StudioRenderer.m_pCvarShadowsDebug->value >= 1 )
	{
		glLoadIdentity();

		glRotatef(-90,  1, 0, 0);
		glRotatef(90,  0, 0, 1);
		glBegin(GL_QUADS);
			glVertex3f (10, 100, 100);
			glVertex3f (10, -100, 100);
			glVertex3f (10, -100, -100);
			glVertex3f (10, 100, -100);
		glEnd();
	}
	else
	{
		// get current visframe number
		g_pworld = gEngfuncs.GetEntityByIndex(0)->model;
		mleaf_t *leaf = Mod_PointInLeaf ( g_StudioRenderer.m_vRenderOrigin, g_pworld );
		g_visframe = leaf->visframe;

		// get current frame number
		g_framecount = g_StudioRenderer.m_nFrameCount;

		EnableVertexArray();

		// draw world
		RecursiveDrawWorld( g_pworld->nodes );

		for ( int i = 0; i < m_iNumRenderEnts; i++ )
		{
			if ( m_pRenderEnts[i]->model->type == mod_brush )
				R_DrawBrushModel( m_pRenderEnts[i] );
		}

		DisableVertexArray();
	}

	glPopMatrix(); //so we aren't stuck with the identity matrix
	glPopAttrib(); //pop back our previous enable/etc. state so we don't have to bother switching it all back
}