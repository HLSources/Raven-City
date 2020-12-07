//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

// studio_model.cpp
// routines for setting up to draw 3DStudio models

#include "windows.h"
#include "gl/gl.h"
#include "glext.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include <fstream> 
#include <iostream>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

using namespace std;
template <typename FuncType>

inline void LoadProcEXT( FuncType &pfn, const char* name )
{
	pfn = (FuncType)wglGetProcAddress( name );
}

#define LOAD_PROC_EXT(x) LoadProcEXT( x, #x );

class CArraysLocker
{
public:
	CArraysLocker( void )
	{
		const GLubyte *str = glGetString(GL_RENDERER);
		if (str)
		{
			// we're in opengl mode
			LOAD_PROC_EXT( glLockArraysEXT );
			LOAD_PROC_EXT( glUnlockArraysEXT );
		}
	}

	void Lock( GLsizei count )
	{
		glLockArraysEXT(0, count);
	}

	void Unlock( void )
	{
		glUnlockArraysEXT();
	}

private:
	PFNGLLOCKARRAYSEXTPROC		glLockArraysEXT;
	PFNGLUNLOCKARRAYSEXTPROC	glUnlockArraysEXT;
};

CArraysLocker	g_Lock;

vec3_t		vertexdata[MAXSTUDIOTRIANGLES * 5];
GLushort	indexdata[MAXSTUDIOTRIANGLES * 3 * 5];

int g_shadowpolycounter;
bool	g_bShadows;

void SetupBuffer( void )
{
	glVertexPointer( 3, GL_FLOAT, sizeof(vec3_t), vertexdata );
	glEnableClientState(GL_VERTEX_ARRAY);
	g_shadowpolycounter = 0;
	g_bShadows = true;
}

void ClearBuffer( void )
{
	if (!g_bShadows)
		return;

	glDisableClientState(GL_VERTEX_ARRAY);
}

extern cl_entity_t *m_pRenderEnts[1024];
extern int m_iNumRenderEnts;

cl_entity_t *m_pStencilEnts[1024];
int numStencilEnts;

float	m_psavedbonetransform[1024][ MAXSTUDIOBONES ][ 3 ][ 4 ];

bool facelight[MAXSTUDIOTRIANGLES];

/*
====================
Init

====================
*/
void CStudioModelRenderer::Init( void )
{
	StudioRenderingCheck();

	// Set up some variables shared with engine
	m_pCvarHiModels			= IEngineStudio.GetCvar( "cl_himodels" );
	m_pCvarDeveloper		= IEngineStudio.GetCvar( "developer" );
	m_pCvarDrawEntities		= IEngineStudio.GetCvar( "r_drawentities" );
	m_pChromeSprite			= IEngineStudio.GetChromeSprite();

	m_pCvarShadows			= CVAR_CREATE( "r_shadows", "0", FCVAR_ARCHIVE );
	m_pCvarShadowsDynamic	= CVAR_CREATE( "r_shadows_dynamic", "0", FCVAR_ARCHIVE );
	m_pCvarShadowsDebug		= CVAR_CREATE( "r_shadows_debug", "0", FCVAR_ARCHIVE );

	m_pCvarDrawModels		= CVAR_CREATE( "r_drawmodels", "1", FCVAR_ARCHIVE );

	IEngineStudio.GetModelCounters( &m_pStudioModelCount, &m_pModelsDrawn );

	// Get pointers to engine data structures
	m_pbonetransform		= (float (*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetBoneTransform();
	m_plighttransform		= (float (*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetLightTransform();
	m_paliastransform		= (float (*)[3][4])IEngineStudio.StudioGetAliasTransform();
	m_protationmatrix		= (float (*)[3][4])IEngineStudio.StudioGetRotationMatrix();
}

/*
====================
CStudioModelRenderer

====================
*/
CStudioModelRenderer::CStudioModelRenderer( void )
{
	m_fDoInterp				= 1;
	m_fGaitEstimation		= 1;
	m_pCurrentEntity		= NULL;
	m_pCvarHiModels			= NULL;
	m_pCvarDeveloper		= NULL;
	m_pCvarDrawEntities		= NULL;
	m_pChromeSprite			= NULL;
	m_pStudioModelCount		= NULL;
	m_pModelsDrawn			= NULL;
	m_protationmatrix		= NULL;
	m_paliastransform		= NULL;
	m_pbonetransform		= NULL;
	m_plighttransform		= NULL;
	m_pStudioHeader			= NULL;
	m_pBodyPart				= NULL;
	m_pSubModel				= NULL;
	m_pPlayerInfo			= NULL;
	m_pRenderModel			= NULL;
	m_pCvarShadows			= NULL;
	m_pCvarShadowsDynamic	= NULL;
	m_pCvarDrawModels		= NULL;
	m_hOpenGL32Dll			= NULL;
	m_pShadowHeader			= NULL;
}

/*
====================
~CStudioModelRenderer

====================
*/
CStudioModelRenderer::~CStudioModelRenderer( void )
{
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CStudioModelRenderer::StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen )
{
	int					i, j;
	float				value;
	mstudiobonecontroller_t *pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0/256.0) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0/256.0) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0) value = 0;
				if (value > 1.0) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
		}
		else
		{
			value = mouthopen / 64.0;
			if (value > 1.0) value = 1.0;				
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch(pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}


/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
{
	int					j, k;
	vec4_t				q1, q2;
	vec3_t				angle1, angle2;
	mstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j+3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k+1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if (pbone->bonecontroller[j+3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if (!VectorCompare( angle1, angle2 ))
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	int					j, k;
	mstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			/*
			if (i == 0 && j == 0)
				Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			*/
			
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
  				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k+1].value * (1.0 - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k+1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if ( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void CStudioModelRenderer::StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int			i;
	vec4_t		q3;
	float		s1;

	if (s < 0) s = 0;
	else if (s > 1.0) s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t *CStudioModelRenderer::StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t *paSequences;

	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);
	}

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if (paSequences == NULL)
	{
		paSequences = (cache_user_t *)IEngineStudio.Mem_Calloc( 16, sizeof( cache_user_t ) ); // UNDONE: leak!
		m_pSubModel->submodels = (dmodel_t *)paSequences;
	}

	if (!IEngineStudio.Cache_Check( (struct cache_user_s *)&(paSequences[pseqdesc->seqgroup])))
	{
		gEngfuncs.Con_DPrintf("loading %s\n", pseqgroup->name );
		IEngineStudio.LoadCacheFile( pseqgroup->name, (struct cache_user_s *)&paSequences[pseqdesc->seqgroup] );
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);
	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer::StudioSetUpTransform (int trivial_accept)
{
	int				i;
	vec3_t			angles;
	vec3_t			modelpos;

// tweek model origin	
	//for (i = 0; i < 3; i++)
	//	modelpos[i] = m_pCurrentEntity->origin[i];

	VectorCopy( m_pCurrentEntity->origin, modelpos );

// TODO: should really be stored with the entity instead of being reconstructed
// TODO: should use a look-up table
// TODO: could cache lazily, stored in the entity
	angles[ROLL] = m_pCurrentEntity->curstate.angles[ROLL];
	angles[PITCH] = m_pCurrentEntity->curstate.angles[PITCH];
	angles[YAW] = m_pCurrentEntity->curstate.angles[YAW];

	//Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], m_pCurrentEntity->index);
	//Con_DPrintf("movetype %d %d\n", m_pCurrentEntity->movetype, m_pCurrentEntity->aiment );
	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP) 
	{
		float			f = 0;
		float			d;

		mstudioseqdesc_t *pseqdesc; // acess to studio flags 
		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;


		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ( ( m_clTime < m_pCurrentEntity->curstate.animtime + 1.0f ) &&
			 ( m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime ) )
		{
			f = (m_clTime - m_pCurrentEntity->curstate.animtime) / (m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime);
			//Con_DPrintf("%4.2f %.2f %.2f\n", f, m_pCurrentEntity->curstate.animtime, m_clTime);
		}

		if (m_fDoInterp)
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		if (pseqdesc->motiontype & STUDIO_LX || m_pCurrentEntity->curstate.eflags & EFLAG_SLERP)//uncle misha
		{
			for (i = 0; i < 3; i++)
			{
				modelpos[i] += (m_pCurrentEntity->origin[i] - m_pCurrentEntity->latched.prevorigin[i]) * f;
			}
		}


		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		//if (f > -1.0 && f < 1.5) {}

//			Con_DPrintf("%.0f %.0f\n",m_pCurrentEntity->msg_angles[0][YAW], m_pCurrentEntity->msg_angles[1][YAW] );
		for (i = 0; i < 3; i++)
		{
			float ang1, ang2;

			ang1 = m_pCurrentEntity->angles[i];
			ang2 = m_pCurrentEntity->latched.prevangles[i];

			d = ang1 - ang2;
			if (d > 180)
			{
				d -= 360;
			}
			else if (d < -180)
			{	
				d += 360;
			}

			angles[i] += d * f;
		}
		//Con_DPrintf("%.3f \n", f );
	}
	else if ( m_pCurrentEntity->curstate.movetype != MOVETYPE_NONE ) 
	{
		VectorCopy( m_pCurrentEntity->angles, angles );
	}

	//Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
	//Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );

	angles[PITCH] = -angles[PITCH];
	AngleMatrix (angles, (*m_protationmatrix));

	if ( !IEngineStudio.IsHardware() )
	{
		static float viewmatrix[3][4];

		VectorCopy (m_vRight, viewmatrix[0]);
		VectorCopy (m_vUp, viewmatrix[1]);
		VectorInverse (viewmatrix[1]);
		VectorCopy (m_vNormal, viewmatrix[2]);

		(*m_protationmatrix)[0][3] = modelpos[0] - m_vRenderOrigin[0];
		(*m_protationmatrix)[1][3] = modelpos[1] - m_vRenderOrigin[1];
		(*m_protationmatrix)[2][3] = modelpos[2] - m_vRenderOrigin[2];

		ConcatTransforms (viewmatrix, (*m_protationmatrix), (*m_paliastransform));

		// do the scaling up of x and y to screen coordinates as part of the transform
		// for the unclipped case (it would mess up clipping in the clipped case).
		// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
		// correspondingly so the projected x and y come out right
		// FIXME: make this work for clipped case too?
		if (trivial_accept)
		{
			for (i=0 ; i<4 ; i++)
			{
				(*m_paliastransform)[0][i] *= m_fSoftwareXScale *
						(1.0 / (ZISCALE * 0x10000));
				(*m_paliastransform)[1][i] *= m_fSoftwareYScale *
						(1.0 / (ZISCALE * 0x10000));
				(*m_paliastransform)[2][i] *= 1.0 / (ZISCALE * 0x10000);

			}
		}
	}

	(*m_protationmatrix)[0][3] = modelpos[0];
	(*m_protationmatrix)[1][3] = modelpos[1];
	(*m_protationmatrix)[2][3] = modelpos[2];
}


/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer::StudioEstimateInterpolant( void )
{
	float dadt = 1.0;

	if ( m_fDoInterp && ( m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01 ) )
	{
		dadt = (m_clTime - m_pCurrentEntity->curstate.animtime) / 0.1;
		if (dadt > 2.0)
		{
			dadt = 2.0;
		}
	}
	return dadt;
}

/*
====================
StudioCalcRotations

====================
*/
void CStudioModelRenderer::StudioCalcRotations ( float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int					i;
	int					frame;
	mstudiobone_t		*pbone;

	float				s;
	float				adj[MAXSTUDIOCONTROLLERS];
	float				dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0;	// bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if ( f < -0.01 )
	{
		f = -0.01;
	}

	frame = (int)f;

	// Con_DPrintf("%d %.4f %.4f %.4f %.4f %d\n", m_pCurrentEntity->curstate.sequence, m_clTime, m_pCurrentEntity->animtime, m_pCurrentEntity->frame, f, frame );

	// Con_DPrintf( "%f %f %f\n", m_pCurrentEntity->angles[ROLL], m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->angles[YAW] );

	// Con_DPrintf("frame %d %d\n", frame1, frame2 );


	dadt = StudioEstimateInterpolant( );
	s = (f - frame);

	// add in programtic controllers
	pbone		= (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	StudioCalcBoneAdj( dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen );

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++) 
	{
		StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );

		StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
		// if (0 && i == 0)
		//	Con_DPrintf("%d %d %d %d\n", m_pCurrentEntity->curstate.sequence, frame, j, k );
	}

	if (pseqdesc->motiontype & STUDIO_X)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Y)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Z)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * m_pCurrentEntity->curstate.framerate;

	if (pseqdesc->motiontype & STUDIO_LX)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if (pseqdesc->motiontype & STUDIO_LY)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if (pseqdesc->motiontype & STUDIO_LZ)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}
/*
====================
Studio_FxTransform

====================
*/
void CStudioModelRenderer::StudioFxTransform( cl_entity_t *ent, float transform[3][4] )
{

	switch( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if ( gEngfuncs.pfnRandomLong(0,49) == 0 )
		{
			int axis = gEngfuncs.pfnRandomLong(0,1);
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			VectorScale( transform[axis], gEngfuncs.pfnRandomFloat(1,1.484), transform[axis] );
		}
		else if ( gEngfuncs.pfnRandomLong(0,49) == 0 )
		{
			float offset;
			int axis = gEngfuncs.pfnRandomLong(0,1);
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			offset = gEngfuncs.pfnRandomFloat(-10,10);
			transform[gEngfuncs.pfnRandomLong(0,2)][3] += offset;
		}
	break;
	case kRenderFxExplode:
		{
			float scale;

			scale = 1.0 + ( m_clTime - ent->curstate.animtime) * 10.0;
			if ( scale > 2 )	// Don't blow up more than 200%
				scale = 2;
			transform[0][1] *= scale;
			transform[1][1] *= scale;
			transform[2][1] *= scale;
		}
	break;

	}
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer::StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	double				dfdt, f;

	if ( m_fDoInterp )
	{
		if ( m_clTime < m_pCurrentEntity->curstate.animtime )
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (m_clTime - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;

		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
	}
 	
	f += dfdt;

	if (pseqdesc->flags & STUDIO_LOOPING) 
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		}
		if (f < 0) 
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else 
	{
		if (f >= pseqdesc->numframes - 1.001) 
		{
			f = pseqdesc->numframes - 1.001;
		}
		if (f < 0.0) 
		{
			f = 0.0;
		}
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer::StudioSetupBones ( void )
{
	int					i;
	double				f;

	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static float		pos[MAXSTUDIOBONES][3];
	static vec4_t		q[MAXSTUDIOBONES];
	float				bonematrix[3][4];

	static float		pos2[MAXSTUDIOBONES][3];
	static vec4_t		q2[MAXSTUDIOBONES];
	static float		pos3[MAXSTUDIOBONES][3];
	static vec4_t		q3[MAXSTUDIOBONES];
	static float		pos4[MAXSTUDIOBONES][3];
	static vec4_t		q4[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >=  m_pStudioHeader->numseq) 
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame( pseqdesc );

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim( m_pRenderModel, pseqdesc );
	StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if (pseqdesc->numblends > 1)
	{
		float				s;
		float				dadt;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		StudioSlerpBones( q, pos, q2, pos2, s );

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}
	
	if (m_fDoInterp &&
		m_pCurrentEntity->latched.sequencetime &&
		( m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime ) && 
		( m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float		pos1b[MAXSTUDIOBONES][3];
		static vec4_t		q1b[MAXSTUDIOBONES];
		float				s;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim( m_pRenderModel, pseqdesc );
		// clip prevframe
		StudioCalcRotations( pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
			StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
				StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0;
				StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0 - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2;
		StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		//Con_DPrintf("prevframe = %4.2f\n", f);
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if (m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0)
	{
		if (m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq) 
		{
			m_pPlayerInfo->gaitsequence = 0;
		}

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = StudioGetAnim( m_pRenderModel, pseqdesc );
		StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe );

		for (i = 0; i < m_pStudioHeader->numbones; i++)
		{
			if (strcmp( pbones[i].name, "Bip01 Spine") == 0)
				break;
			memcpy( pos[i], pos2[i], sizeof( pos[i] ));
			memcpy( q[i], q2[i], sizeof( q[i] ));
		}
	}


	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1) 
		{
			if ( IEngineStudio.IsHardware() )
			{
				ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

				// MatrixCopy should be faster...
				//ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
				MatrixCopy( (*m_pbonetransform)[i], (*m_plighttransform)[i] );
			}
			else
			{
				ConcatTransforms ((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
				ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
			}

			// Apply client-side effects to the transformation matrix
			StudioFxTransform( m_pCurrentEntity, (*m_pbonetransform)[i] );
		} 
		else 
		{
			ConcatTransforms ((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
			ConcatTransforms ((*m_plighttransform)[pbones[i].parent], bonematrix, (*m_plighttransform)[i]);
		}
	}
}


/*
====================
StudioSaveBones

====================
*/
void CStudioModelRenderer::StudioSaveBones( void )
{
	int		i;

	mstudiobone_t		*pbones;
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_nCachedBones = m_pStudioHeader->numbones;

	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		strcpy( m_nCachedBoneNames[i], pbones[i].name );
		MatrixCopy( (*m_pbonetransform)[i], m_rgCachedBoneTransform[i] );
		MatrixCopy( (*m_plighttransform)[i], m_rgCachedLightTransform[i] );
	}
}


/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer::StudioMergeBones ( model_t *m_pSubModel )
{
	int					i, j;
	double				f;
	int					do_hunt = true;

	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static float		pos[MAXSTUDIOBONES][3];
	float				bonematrix[3][4];
	static vec4_t		q[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >=  m_pStudioHeader->numseq) 
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame( pseqdesc );

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim( m_pSubModel, pseqdesc );
	StudioCalcRotations( pos, q, pseqdesc, panim, f );

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);


	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		for (j = 0; j < m_nCachedBones; j++)
		{
			if (stricmp(pbones[i].name, m_nCachedBoneNames[j]) == 0)
			{
				MatrixCopy( m_rgCachedBoneTransform[j], (*m_pbonetransform)[i] );
				MatrixCopy( m_rgCachedLightTransform[j], (*m_plighttransform)[i] );
				break;
			}
		}
		if (j >= m_nCachedBones)
		{
			QuaternionMatrix( q[i], bonematrix );

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if (pbones[i].parent == -1) 
			{
				if ( IEngineStudio.IsHardware() )
				{
					ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

					// MatrixCopy should be faster...
					//ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
					MatrixCopy( (*m_pbonetransform)[i], (*m_plighttransform)[i] );
				}
				else
				{
					ConcatTransforms ((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
					ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
				}

				// Apply client-side effects to the transformation matrix
				StudioFxTransform( m_pCurrentEntity, (*m_pbonetransform)[i] );
			} 
			else 
			{
				ConcatTransforms ((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
				ConcatTransforms ((*m_plighttransform)[pbones[i].parent], bonematrix, (*m_plighttransform)[i]);
			}
		}
	}
}
extern float g_iEndDist;
/*
====================
StudioDrawModel

====================
*/
int CStudioModelRenderer::StudioDrawModel( int flags )
{
	vec3_t tmp;
	alight_t lighting;
	vec3_t dir;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	if ( m_pCvarDrawEntities->value >= 1 && m_pCvarShadows->value >= 1
	&& m_pCurrentEntity->curstate.renderfx != kRenderFxNoShadow
	&& m_pCurrentEntity != gEngfuncs.GetViewModel() )
	{
		VectorSubtract(m_pCurrentEntity->origin, m_vRenderOrigin, tmp);
		if (DotProduct(tmp, tmp) < (700 * 700))
		{
			m_pStencilEnts[numStencilEnts] = m_pCurrentEntity;
			numStencilEnts++;
		}
	}

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
	IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );

	if (m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer)
	{
		entity_state_t deadplayer;

		int result;
		int save_interp;

		if (m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > gEngfuncs.GetMaxClients() )
			return 0;

		// get copy of player
		deadplayer = *(IEngineStudio.GetPlayerState( m_pCurrentEntity->curstate.renderamt - 1 )); //cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[m_pCurrentEntity->curstate.renderamt-1];

		// clear weapon, movement state
		deadplayer.number = m_pCurrentEntity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy( m_pCurrentEntity->curstate.angles, deadplayer.angles );
		VectorCopy( m_pCurrentEntity->curstate.origin, deadplayer.origin );

		save_interp = m_fDoInterp;
		m_fDoInterp = 0;
		
		// draw as though it were a player
		result = StudioDrawPlayer( flags, &deadplayer );
		
		m_fDoInterp = save_interp;
		return result;
	}

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (m_pRenderModel);
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	StudioSetUpTransform( 0 );

	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW)
	{
		StudioMergeBones( m_pRenderModel );
	}
	else
	{
		StudioSetupBones( );
	}
	StudioSaveBones( );

	memcpy( &m_psavedbonetransform[m_pCurrentEntity->index][0], &m_pbonetransform[0], sizeof( float )*12*m_pStudioHeader->numbones );

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!IEngineStudio.StudioCheckBBox ())
			return 0;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments( );
		IEngineStudio.StudioClientEvents( );
		// copy attachments into global entity array
		if ( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );

			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if (flags & STUDIO_RENDER)
	{
		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting );

		IEngineStudio.StudioEntityLight( &lighting );

		// model and frame independant
		IEngineStudio.StudioSetupLighting (&lighting);

		// get remap colors
		m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;

		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		StudioRenderModel( );
	}

	return 1;
}


/*
====================
StudioEstimateGait

====================
*/
void CStudioModelRenderer::StudioEstimateGait( entity_state_t *pplayer )
{
	float dt;
	vec3_t est_velocity;

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	if (dt == 0 || m_pPlayerInfo->renderframe == m_nFrameCount)
	{
		m_flGaitMovement = 0;
		return;
	}

	if ( m_fGaitEstimation )
	{
		VectorSubtract( m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin, est_velocity );
		VectorCopy( m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin );
		m_flGaitMovement = Length( est_velocity );
		if (dt <= 0 || m_flGaitMovement / dt < 5)
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, est_velocity );
		m_flGaitMovement = Length( est_velocity ) * dt;
	}

	if (est_velocity[1] == 0 && est_velocity[0] == 0)
	{
		float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_pPlayerInfo->gaityaw += flYawDiff;
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)(m_pPlayerInfo->gaityaw / 360) * 360;

		m_flGaitMovement = 0;
	}
	else
	{
		m_pPlayerInfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (m_pPlayerInfo->gaityaw > 180)
			m_pPlayerInfo->gaityaw = 180;
		if (m_pPlayerInfo->gaityaw < -180)
			m_pPlayerInfo->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void CStudioModelRenderer::StudioProcessGait( entity_state_t *pplayer )
{
	mstudioseqdesc_t	*pseqdesc;
	float dt;
	int iBlend;
	float flYaw;	 // view direction relative to movement

	if (m_pCurrentEntity->curstate.sequence >=  m_pStudioHeader->numseq) 
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	StudioPlayerBlend( pseqdesc, &iBlend, &m_pCurrentEntity->angles[PITCH] );

	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[0] = iBlend;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

	// Con_DPrintf("%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	StudioEstimateGait( pplayer );

	// Con_DPrintf("%f %f\n", m_pCurrentEntity->angles[YAW], m_pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
	m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
	m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
	m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
	if (m_pCurrentEntity->angles[YAW] < -0)
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

	if (pplayer->gaitsequence >= m_pStudioHeader->numseq) 
	{
		pplayer->gaitsequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
	{
		m_pPlayerInfo->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)(m_pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (m_pPlayerInfo->gaitframe < 0)
		m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
int CStudioModelRenderer::StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	alight_t lighting;
	vec3_t dir;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	if ( m_pCvarDrawEntities->value >= 1 && m_pCvarShadows->value >= 1 )
	{
		m_pStencilEnts[numStencilEnts] = m_pCurrentEntity;
		numStencilEnts++;
	}

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
	IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );

	// Con_DPrintf("DrawPlayer %d\n", m_pCurrentEntity->blending[0] );

	// Con_DPrintf("DrawPlayer %d %d (%d)\n", m_nFrameCount, pplayer->player_index, m_pCurrentEntity->curstate.sequence );

	// Con_DPrintf("Player %.2f %.2f %.2f\n", pplayer->velocity[0], pplayer->velocity[1], pplayer->velocity[2] );

	m_nPlayerIndex = pplayer->number - 1;

	if (m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients())
		return 0;

	m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );
	if (m_pRenderModel == NULL)
		return 0;

	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (m_pRenderModel);
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	if (pplayer->gaitsequence)
	{
		vec3_t orig_angles;
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

		VectorCopy( m_pCurrentEntity->angles, orig_angles );
	
		StudioProcessGait( pplayer );

		m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
		m_pPlayerInfo = NULL;

		StudioSetUpTransform( 0 );
		VectorCopy( orig_angles, m_pCurrentEntity->angles );
	}
	else
	{
		m_pCurrentEntity->curstate.controller[0] = 127;
		m_pCurrentEntity->curstate.controller[1] = 127;
		m_pCurrentEntity->curstate.controller[2] = 127;
		m_pCurrentEntity->curstate.controller[3] = 127;
		m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
		m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
		m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
		m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];
		
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		m_pPlayerInfo->gaitsequence = 0;

		StudioSetUpTransform( 0 );
	}

	m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
	StudioSetupBones( );
	StudioSaveBones( );

	memcpy( &m_psavedbonetransform[m_pCurrentEntity->index][0], &m_pbonetransform[0], sizeof( float )*12*m_pStudioHeader->numbones );

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!IEngineStudio.StudioCheckBBox ())
			return 0;

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	m_pPlayerInfo->renderframe = m_nFrameCount;

	m_pPlayerInfo = NULL;

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments( );
		IEngineStudio.StudioClientEvents( );
		// copy attachments into global entity array
		if ( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );

			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if (flags & STUDIO_RENDER)
	{
		if (m_pCvarHiModels->value && m_pRenderModel != m_pCurrentEntity->model  )
		{
			// show highest resolution multiplayer model
			m_pCurrentEntity->curstate.body = 255;
		}

		if (!(m_pCvarDeveloper->value == 0 && gEngfuncs.GetMaxClients() == 1 ) && ( m_pRenderModel == m_pCurrentEntity->model ) )
		{
			m_pCurrentEntity->curstate.body = 1; // force helmet
		}

		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting );

		IEngineStudio.StudioEntityLight( &lighting );

		// model and frame independant
		IEngineStudio.StudioSetupLighting (&lighting);

		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

		// get remap colors
		m_nTopColor = m_pPlayerInfo->topcolor;
		m_nBottomColor = m_pPlayerInfo->bottomcolor;
		if (m_nTopColor < 0)
			m_nTopColor = 0;
		if (m_nTopColor > 360)
			m_nTopColor = 360;
		if (m_nBottomColor < 0)
			m_nBottomColor = 0;
		if (m_nBottomColor > 360)
			m_nBottomColor = 360;

		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		StudioRenderModel( );
		m_pPlayerInfo = NULL;

		if (pplayer->weaponmodel)
		{
			cl_entity_t saveent = *m_pCurrentEntity;
			model_t *savedmdl = m_pRenderModel; // buz

			model_t *pweaponmodel = IEngineStudio.GetModelByIndex( pplayer->weaponmodel );
			m_pRenderModel = pweaponmodel; // buz

			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (pweaponmodel);
			IEngineStudio.StudioSetHeader( m_pStudioHeader );

			StudioMergeBones( pweaponmodel);

			IEngineStudio.StudioSetupLighting (&lighting);

			StudioRenderModel( );

			StudioCalcAttachments( );

			*m_pCurrentEntity = saveent;
			m_pRenderModel = savedmdl; // buz
		}
	}

	return 1;
}



/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioCalcAttachments( void )
{
	int i;
	mstudioattachment_t *pattachment;

	if ( m_pStudioHeader->numattachments > 4 )
	{
		gEngfuncs.Con_DPrintf( "Too many attachments on %s\n", m_pCurrentEntity->model->name );
		exit( -1 );
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		VectorTransform( pattachment[i].org, (*m_plighttransform)[pattachment[i].bone],  m_pCurrentEntity->attachment[i] );
	}
}
/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer::StudioRenderModel( void )
{
	IEngineStudio.SetChromeOrigin();
	IEngineStudio.SetForceFaceFlags( 0 );

	if ( m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell )
	{
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
		StudioRenderFinal( );
		
		if ( !IEngineStudio.IsHardware() )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		}

		IEngineStudio.SetForceFaceFlags( STUDIO_NF_CHROME );

		gEngfuncs.pTriAPI->SpriteTexture( m_pChromeSprite, 0 );
		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;

		StudioRenderFinal( );
		if ( !IEngineStudio.IsHardware() )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		}
	}
	else
	{
		StudioRenderFinal( );
	}
}
/*
====================
StudioRenderFinal

====================
*/
void CStudioModelRenderer::StudioRenderFinal( void )
{
	int i;
	int rendermode;

	rendermode = IEngineStudio.GetForceFaceFlags() ? kRenderTransAdd : m_pCurrentEntity->curstate.rendermode;
	IEngineStudio.SetupRenderer( rendermode );

	if (m_pCvarDrawEntities->value == 2)
	{
		IEngineStudio.StudioDrawBones();
	}
	else if (m_pCvarDrawEntities->value == 3)
	{
		IEngineStudio.StudioDrawHulls();
	}
	else
	{
		for (i=0 ; i < m_pStudioHeader->numbodyparts ; i++)
		{
			IEngineStudio.StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );

			if (m_fDoInterp)
			{
				// interpolation messes up bounding boxes.
				m_pCurrentEntity->trivial_accept = 0; 
			}

			IEngineStudio.GL_SetRenderMode( rendermode );

			if ( m_pCurrentEntity->curstate.renderfx != 81 && m_pCvarDrawModels->value >= 1 )
				IEngineStudio.StudioDrawPoints();
		}
	}
	if ( m_pCvarDrawEntities->value == 4 )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		IEngineStudio.StudioDrawHulls( );
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	}
	if (m_pCvarDrawEntities->value == 5)
	{
		IEngineStudio.StudioDrawAbsBBox( );
	}

	if ( m_pCurrentEntity->curstate.renderfx == 81 )
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;

	IEngineStudio.RestoreRenderer();
}
//====================================
// Z-Fail Stencil Shadows
// Code by BUzer and Highlander.
// All right reserved.
//====================================
/*
====================
BuildTriangles

====================
*/
void CStudioModelRenderer::StudioBuildTriangles(shadowsubmodel_t &submodel, mstudiomodel_t *src)
{
	submodel.numverts = src->numverts;
	vec3_t	*pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + src->vertindex);
	byte	*pvertbone = ((byte *)m_pStudioHeader + src->vertinfoindex);

	int numtris = 0;
	int i, n = 0;
	for ( i = 0; i < submodel.numverts; i++ )
	{
		submodel.boneindexes[i] = pvertbone[i];
		submodel.vertices[i] = pstudioverts[i];
	}

	// get number of triangles in all meshes

	mstudiomesh_t *pmeshes = (mstudiomesh_t *)((byte *)m_pStudioHeader + src->meshindex);
	for (i = 0; i < src->nummesh; i++) 
	{
		int j;
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmeshes[i].triindex);		
		while (j = *(ptricmds++))
		{
			if (j < 0) j *= -1;
			n += (j - 2);
			ptricmds += 4 * j;
		}
	}

	if (n == 0)  return;

	submodel.faces.reserve(n);

	for (i = 0; i < src->nummesh; i++) 
	{
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmeshes[i].triindex);

		int j;
		while (j = *(ptricmds++))
		{	
			if (j > 0) 
			{
				// convert triangle strip
				j -= 3;
				assert(j >= 0);

				short indices[3];
				indices[0] = ptricmds[0]; ptricmds += 4;
				indices[1] = ptricmds[0]; ptricmds += 4;
				indices[2] = ptricmds[0]; ptricmds += 4;
				submodel.faces.push_back( Face( indices[0], indices[1], indices[2] ) );

				bool reverse = false;
				for( ; j > 0; j--, ptricmds += 4)
				{
					indices[0] = indices[1];
					indices[1] = indices[2];
					indices[2] = ptricmds[0];

					if (!reverse)
						submodel.faces.push_back( Face( indices[2], indices[1], indices[0] ) );
					else
						submodel.faces.push_back( Face( indices[0], indices[1], indices[2] ) );

					reverse = !reverse;
				}
			}
			else
			{
				// convert triangle fan
				j = -j - 3;
				assert(j >= 0);

				short indices[3];
				indices[0] = ptricmds[0]; ptricmds += 4;
				indices[1] = ptricmds[0]; ptricmds += 4;
				indices[2] = ptricmds[0]; ptricmds += 4;
				submodel.faces.push_back( Face( indices[0], indices[1], indices[2] ) );

				for( ; j > 0; j--, ptricmds += 4)
				{
					indices[1] = indices[2];
					indices[2] = ptricmds[0];
					submodel.faces.push_back( Face( indices[0], indices[1], indices[2] ) );
				}
			}
		}
	}
}
/*
====================
SetupModelExtraData

====================
*/
void CStudioModelRenderer::StudioSetExtraData ( void )
{
	m_pShadowHeader = &m_pDataMap[m_pRenderModel->name];

	if ( m_pShadowHeader->submodels.size() > 0 )
		return;

	if ( StudioLoadData() )
		return;

	// generate extra data for this model
	if ( m_pCvarDeveloper->value != 0 )
		gEngfuncs.Con_Printf("Generating extra data for model %s\n", m_pRenderModel->name);
	
	// get number of submodels
	int i, n = 0;
	mstudiobodyparts_t *bp = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex);
	for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
		n += bp[i].nummodels;

	if (n == 0)
	{
		if ( m_pCvarDeveloper->value != 0 )
			gEngfuncs.Con_Printf("Error: model %s has 0 submodels\n", m_pRenderModel->name);
		
		return;
	}
	
	m_pShadowHeader->submodels.resize(n);

	// convert strips and fans to triangles, generate adjacency info
	n = 0;
	int trianglecounter = 0, edgecounter = 0;
	for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		mstudiomodel_t *sm = (mstudiomodel_t *)((byte *)m_pStudioHeader + bp[i].modelindex);
		for (int j = 0; j < bp[i].nummodels; j++)
		{
			if (n >= m_pShadowHeader->submodels.size()) 
			{
				if ( m_pCvarDeveloper->value != 0 )
					gEngfuncs.Con_Printf("Warning: in model %s submodel index %d is out of range\n", m_pRenderModel->name, n);
				
				return;
			}
			
			StudioBuildTriangles(m_pShadowHeader->submodels[n], &sm[j]);
			StudioBuildNeighbors(m_pShadowHeader->submodels[n]);
			StudioSpecialProcess(m_pShadowHeader->submodels[n]);

			trianglecounter += m_pShadowHeader->submodels[n].faces.size();
			
			n++;
		}
	}

	StudioWriteData();

	if ( m_pCvarDeveloper->value != 0 )
		gEngfuncs.Con_Printf("Done (%d polys)\n", trianglecounter);
}
/*
====================
BuildNeighbors

Get nearest neighbors for all the Faces.
====================
*/
void CStudioModelRenderer::StudioBuildNeighbors( shadowsubmodel_t &submodel )
{
	for (int i = 0; i < submodel.faces.size(); i++)
	{
		Face &triangle = submodel.faces[i];

		StudioGetNeighbors( submodel, triangle, i );
	}
}
void CStudioModelRenderer::StudioSpecialProcess(shadowsubmodel_t &submodel)
{
	int i;
	for (i = 0; i < submodel.faces.size(); i++)
	{
		Face &triangle = submodel.faces[i];
		triangle.vertexIndexes[0] *= 2;
		triangle.vertexIndexes[1] *= 2;
		triangle.vertexIndexes[2] *= 2;
	}
}

/*
====================
GetTriangleNeighbours

Go through each Face and compare vertices. The
Face that shares vertices with the triangle
at the edge is neighbor.
====================
*/
void CStudioModelRenderer::StudioGetNeighbors(shadowsubmodel_t &submodel, Face &triangle, int trianglenum)
{
	int i = 0;
	int j, k;
	int m_iVertA1, m_iVertA2, m_iVertB1, m_iVertB2;

	// Disgusting number, but I have to use it.
	triangle.neighborIndexes[0] = triangle.neighborIndexes[1] = triangle.neighborIndexes[2] = 65535;

	while (i < submodel.faces.size())
	{
		Face &neighbor = submodel.faces[i];
		if (i != trianglenum)
		{
			j = 0;
			while (j < 3)
			{
				m_iVertA1 = triangle.vertexIndexes[j];
				m_iVertA2 = triangle.vertexIndexes[(j+1)%3];

				k = 0;
				while (k < 3)
				{
					m_iVertB1 = neighbor.vertexIndexes[k];
					m_iVertB2 = neighbor.vertexIndexes[(k+1)%3];

					if ((m_iVertA1 == m_iVertB1 && m_iVertA2 == m_iVertB2) || (m_iVertA1 == m_iVertB2 && m_iVertA2 == m_iVertB1))
					{
						if ( triangle.neighborIndexes[j] == 65535 )
						{
							triangle.neighborIndexes[j] = i;
						}
						else
						{
							triangle.neighborIndexes[j] = -2;
						}
					}
					k++;
				}
				j++;
			}
		}
		i++;
	}

	j = 0;
	while (j < 3)
	{ //mark any -2 as -1 now since we're done

		if (triangle.neighborIndexes[j] == -2)
		{
			triangle.neighborIndexes[j] = 65535;
		}
		j++;
	}
}
/*
====================
StudioLoadData

Load data from the .dat file.
====================
*/
bool CStudioModelRenderer::StudioLoadData( void )
{
	int i;
	char szFile[256];

	std::string filename(m_pRenderModel->name);
	sprintf(szFile, "%s/%s.dat", gEngfuncs.pfnGetGameDirectory(), filename.substr(0, filename.rfind('.')).c_str() );

	std::ifstream fin(szFile, ios_base::in | ios_base::binary);

	if (fin.is_open())
	{
		int iSubmodelCount;
		fin.read((char*)&iSubmodelCount, sizeof(int));
		m_pShadowHeader->submodels.resize(iSubmodelCount);

		for ( i = 0; i < iSubmodelCount; i++ )
		{
			int iTriangleCount;
			Face m_fTriangles[MAXSTUDIOTRIANGLES];
			shadowsubmodel_t &m_pSubModel = m_pShadowHeader->submodels[i];

			fin.read((char *)&iTriangleCount, sizeof(int));
			fin.read((char *)&m_fTriangles, sizeof(Face)*iTriangleCount );

			m_pSubModel.faces.reserve( iTriangleCount );
			for ( int k = 0; k < iTriangleCount; k++ )
				m_pSubModel.faces.push_back( Face( m_fTriangles[k].vertexIndexes[0], m_fTriangles[k].vertexIndexes[1], m_fTriangles[k].vertexIndexes[2], m_fTriangles[k].neighborIndexes[0], m_fTriangles[k].neighborIndexes[1], m_fTriangles[k].neighborIndexes[2]) ); 

			fin.read((char *)&m_pSubModel.numverts, sizeof(int));
			fin.read((char *)&m_pSubModel.vertices, sizeof(vec3_t)*m_pSubModel.numverts);
			fin.read((char *)&m_pSubModel.boneindexes, sizeof(int)*m_pSubModel.numverts);
		}
		fin.close();
		return true;
	}
	return false;
}
/*
====================
StudioWriteData

Writes Shadow volume data into a file.
====================
*/
void CStudioModelRenderer::StudioWriteData( void )
{
	int i;
	char szFile[256];

	std::string filename(m_pRenderModel->name);
	sprintf(szFile, "%s/%s.dat", gEngfuncs.pfnGetGameDirectory(), filename.substr(0, filename.rfind('.')).c_str() );
	std::ofstream fout(szFile, ios_base::out | ios_base::binary | ios_base::trunc);

	if (fout.is_open())
	{
		int iSubmodelCount = m_pShadowHeader->submodels.size();
		fout.write((const char *)&iSubmodelCount, sizeof(int));

		for ( i = 0; i < m_pShadowHeader->submodels.size(); i++ )
		{
			shadowsubmodel_t &m_pSubModel = m_pShadowHeader->submodels[i];
			int iTriangleCount = m_pSubModel.faces.size();

			fout.write((const char *)&iTriangleCount, sizeof(int));
			fout.write((const char *)&m_pSubModel.faces[0], sizeof(Face)*iTriangleCount);

			fout.write((const char *)&m_pSubModel.numverts, sizeof(int));
			fout.write((const char *)&m_pSubModel.vertices, sizeof(vec3_t)*m_pSubModel.numverts);
			fout.write((const char *)&m_pSubModel.boneindexes, sizeof(int)*m_pSubModel.numverts);
		}
		fout.close();
	}

}
/*
====================
StudioSetupShadows

====================
*/
void CStudioModelRenderer::StudioSetupShadows ( void )
{
	StudioSetExtraData();

	if ( !m_pShadowHeader->submodels.size() )
		return;

	m_fShadowType = false;

	if ( m_pCvarShadowsDynamic->value <= 1 )
		StudioSwapLights();

	mstudiobodyparts_t *bp = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex);

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index = m_pCurrentEntity->curstate.body / bp[i].base;
		index = index % bp[i].nummodels;

		StudioDrawShadow( m_pShadowHeader->submodels[index + baseindex] );
		baseindex += bp[i].nummodels;
	}
}

/*
====================
SwapLights

Go through each light within the array, see if it's
closer than 512 units, see wich one of these lights
is the closest to our model, and select that as the
light position.
====================
*/
void CStudioModelRenderer::StudioSwapLights ( void )
{
	int m_iDist;
	vec3_t m_vEntOrigin;

	int m_iLightDistArray[1024];
	int m_iLightIDArray[1024];
	int m_iLightNum = 0;

	int m_iLastLowestDist = -1;
	int m_iLastLowestDistID = -1;

	if( m_iNumRenderEnts && m_pCvarShadowsDynamic->value >= 1 )
	{
		for( int i = 0; i < m_iNumRenderEnts; i++ )
		{	

			if ( m_pRenderEnts[i]->model->type == mod_brush )
				continue;

			m_vEntOrigin.x	= m_pCurrentEntity->curstate.origin.x;
			m_vEntOrigin.y	= m_pCurrentEntity->curstate.origin.y;
			m_vEntOrigin.z	= m_pCurrentEntity->curstate.origin.z + 128;
			m_iDist = (m_pRenderEnts[i]->curstate.origin - m_vEntOrigin).Length();

			if( m_iDist < 512 )
			{
				m_iLightDistArray[m_iLightNum] = m_iDist;
				m_iLightIDArray[m_iLightNum] = i;
				m_iLightNum++;
			}
		}
		if( m_iLightNum )
		{
			m_iLastLowestDist = -1;
			m_iLastLowestDistID = -1;

			for ( int j = 0; j < m_iLightNum; j++ )
			{
				if ( m_iLastLowestDist >= m_iLightDistArray[j] 
				|| m_iLastLowestDist == -1 
				&& m_iLastLowestDistID == -1  )
				{
					m_iLastLowestDist = m_iLightDistArray[j];
					m_iLastLowestDistID = m_iLightIDArray[j];
				}
			}

			m_vLightPosition = m_pRenderEnts[m_iLastLowestDistID]->curstate.origin;
			m_fShadowType = true;
			return;
		}
	}

	m_vShadowAngle[0] = 0.5;
	m_vShadowAngle[1] = 1;
	m_vShadowAngle[2] = 1.5;
	return;
}
/*
====================
DrawShadowVolume

====================
*/
void CStudioModelRenderer::StudioDrawShadow( shadowsubmodel_t &submodel )
{
	if ((submodel.faces.size() == 0) || (submodel.faces.size() > MAXSTUDIOTRIANGLES))
		return;

	vec3_t d;
	int i, j;
	int neighborIndex;

	if ( m_fShadowType == false )
	{
		VectorScale(m_vShadowAngle, INFINITY, d);
		for (i = 0, j = 0; i < submodel.numverts; i++, j+=2)
		{
			VectorTransform(submodel.vertices[i], (*m_pshadowbonetransform)[submodel.boneindexes[i]], vertexdata[j]);
			VectorSubtract(vertexdata[j], d, vertexdata[j+1]);
		}
	}
	else
	{
		for (i = 0, j = 0; i < submodel.numverts; i++, j+=2)
		{
			VectorTransform(submodel.vertices[i], (*m_pshadowbonetransform)[submodel.boneindexes[i]], vertexdata[j]);
			VectorScale( vertexdata[j]-m_vLightPosition, INFINITY, vertexdata[j+1]);
		}
	}

	for (int j = 0; j < submodel.faces.size(); j++)
	{
		facelight[j] = StudioFacingLight( submodel.faces[j] );
	}


	g_Lock.Lock(submodel.numverts * 2);

	int trianglecount = 0;
	GLushort *inddata = indexdata;

	for (int i = 0; i < submodel.faces.size(); i++)
	{
		Face &triangle = submodel.faces[i];

		if ( facelight[i] )
		{
			inddata[0] = triangle.vertexIndexes[0];
			inddata[1] = triangle.vertexIndexes[2];
			inddata[2] = triangle.vertexIndexes[1];
			inddata[3] = triangle.vertexIndexes[0] + 1;
			inddata[4] = triangle.vertexIndexes[1] + 1;
			inddata[5] = triangle.vertexIndexes[2] + 1;
			inddata += 6;
			trianglecount += 2;

			for ( int j = 0; j < 3; j++ )
			{
				neighborIndex = triangle.neighborIndexes[j];

				if ( neighborIndex == 65535 || !facelight[neighborIndex] )
				{
					inddata[0] = triangle.vertexIndexes[j];
					inddata[1] = triangle.vertexIndexes[( j+1 )%3];
				
					inddata[2] = inddata[0] + 1;
					inddata[3] = inddata[2];
					inddata[4] = inddata[1];
					inddata[5] = inddata[1] + 1;

					inddata += 6;

					trianglecount += 2;
				}
			}
		}
	}

	// We only need one pass.
	glDrawElements(GL_TRIANGLES, trianglecount * 3, GL_UNSIGNED_SHORT, indexdata);

	g_Lock.Unlock();

	g_shadowpolycounter += trianglecount * 2;
}
/*
====================
TriangleFacingLight

See if the given triangle is facing the light origin or vector.
====================
*/
bool CStudioModelRenderer :: StudioFacingLight( Face &triangle )
{
	vec_t *v1;
	vec_t *v2;
	vec_t *v3;
	float side;
	float planeEq[4];

	if ( m_fShadowType == true )
	{
		v1 = vertexdata[triangle.vertexIndexes[0]];
		v2 = vertexdata[triangle.vertexIndexes[1]];
		v3 = vertexdata[triangle.vertexIndexes[2]];

		planeEq[0] = v1[1]*(v2[2]-v3[2]) + v2[1]*(v3[2]-v1[2]) + v3[1]*(v1[2]-v2[2]);
		planeEq[1] = v1[2]*(v2[0]-v3[0]) + v2[2]*(v3[0]-v1[0]) + v3[2]*(v1[0]-v2[0]);
		planeEq[2] = v1[0]*(v2[1]-v3[1]) + v2[0]*(v3[1]-v1[1]) + v3[0]*(v1[1]-v2[1]);
		planeEq[3] = -( v1[0]*( v2[1]*v3[2] - v3[1]*v2[2] ) +
					v2[0]*(v3[1]*v1[2] - v1[1]*v3[2]) +
					v3[0]*(v1[1]*v2[2] - v2[1]*v1[2]) );

		side = planeEq[0]*m_vLightPosition[0]+
			planeEq[1]*m_vLightPosition[1]+
			planeEq[2]*m_vLightPosition[2]+
			planeEq[3];

		if ( side > 0 )
		{
			return 1;
		}

		return 0;
	}
	else
	{
		vec3_t v1, v2, norm;
		VectorSubtract(vertexdata[triangle.vertexIndexes[1]], vertexdata[triangle.vertexIndexes[0]], v1);
		VectorSubtract(vertexdata[triangle.vertexIndexes[2]], vertexdata[triangle.vertexIndexes[1]], v2);
		CrossProduct(v2, v1, norm);

		if ( DotProduct(norm, m_vShadowAngle) > 0 )
		{
			return 0;
		}

		return 1;
	}
}

void CStudioModelRenderer::StudioShadowForEntity( cl_entity_t *pEntity )
{
	m_pCurrentEntity = pEntity;

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (m_pRenderModel);
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	if (m_pStudioHeader->numbodyparts == 0)
		return;

	m_pshadowbonetransform = (float (*)[MAXSTUDIOBONES][3][4])m_psavedbonetransform[m_pCurrentEntity->index];

	StudioSetupShadows();
}
void CStudioModelRenderer::StudioRenderingCheck ( void )
{
	if (!IEngineStudio.IsHardware())
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: Attempting to run in Software mode!\n\nRaven City does not support the software renderer.\nChange to OpenGL mode\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		gEngfuncs.Con_Printf("VidInit: Error! Attempting to run in Software mode!\n");
		exit( -1 );
	}

	if ( IEngineStudio.IsHardware() == 2 )
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: Attempting to run in Direct3D mode!\n\nRaven City does not support DirectX.\nPlease change to OpenGL mode.\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		gEngfuncs.Con_Printf("VidInit: Error! Attempting to run in Direct3D mode!\n");
		exit( -1 );
	}

	m_hOpenGL32Dll = LoadLibrary("opengl32.dll");

	if (m_hOpenGL32Dll)
	{
		// check hacked opengl library
		if (!StudioCheckExtension("RAVEN_HACKS_V1"))
		{
			gEngfuncs.pfnClientCmd("escape\n");
			MessageBox(NULL, "VIDEO ERROR: Raven City's hacked OpenGL32.dll is missing!\n\nRaven City can't run without the library.\nCopy OpenGL32.dll from raven/sdk/ to the Half-Life folder.\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
			gEngfuncs.Con_Printf("VidInit: Error! Raven City's hacked opengl32.dll is missing!\n");

			FreeLibrary( m_hOpenGL32Dll );
			m_hOpenGL32Dll = NULL;
				
			exit( -1 );
		}
	}
}
BOOL CStudioModelRenderer::StudioCheckExtension(const char *ext)
{
	const char * extensions = (const char *)glGetString ( GL_EXTENSIONS );
	const char * start = extensions;
	const char * ptr;

	while ( ( ptr = strstr ( start, ext ) ) != NULL )
	{
		// we've found, ensure name is exactly ext
		const char * end = ptr + strlen ( ext );
		if ( isspace ( *end ) || *end == '\0' )
			return TRUE;

		start = end;
	}
	return FALSE;
}