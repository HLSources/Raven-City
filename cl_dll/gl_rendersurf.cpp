//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

// Based on code from Paranoia, thanks to BUzer.
// I want to use VBO's to render the map, useful for saving on performance.


#include "hud.h"
#include "cl_util.h"
#include "windows.h"
#include "gl/gl.h"
#include "glext.h"

#include "const.h"
#include "com_model.h"
#include "studio.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
extern CGameStudioModelRenderer g_StudioRenderer;

int reloaded;

//================================
// ARRAYS AND VBOS MANAGEMENT
//
//================================

template <typename FuncType>
inline void LoadProcEXT( FuncType &pfn, const char* name )
{
	pfn = (FuncType)wglGetProcAddress( name );
}

#define LOAD_PROC_EXT(x) LoadProcEXT( x, #x );

PFNGLBINDBUFFERARBPROC			glBindBufferARB;
PFNGLGENBUFFERSARBPROC			glGenBuffersARB;
PFNGLBUFFERDATAARBPROC			glBufferDataARB;
PFNGLDELETEBUFFERSARBPROC		glDeleteBuffersARB;
PFNGLACTIVESTENCILFACEEXTPROC	glActiveStencilFaceEXT;
PFNGLACTIVETEXTUREARBPROC		glActiveTextureARB;
bool ARB_VBO_supported;

void InitSurfRender( void )
{
	ARB_VBO_supported = FALSE;
	if (g_StudioRenderer.StudioCheckExtension("GL_ARB_vertex_buffer_object"))
	{
		LOAD_PROC_EXT(glBindBufferARB);
		LOAD_PROC_EXT(glGenBuffersARB);
		LOAD_PROC_EXT(glBufferDataARB);
		LOAD_PROC_EXT(glDeleteBuffersARB);

		ARB_VBO_supported = true;
	}

	LOAD_PROC_EXT(glActiveStencilFaceEXT);
	LOAD_PROC_EXT(glActiveTextureARB);
}

#define OFFSET(type, variable) ((const void*)&(((type*)NULL)->variable))

struct BrushVertex
{
	vec3_t	pos;
};

struct BrushFace
{
	int start_vertex;
	vec3_t	normal;
	vec3_t	s_tangent;
	vec3_t	t_tangent;
};

GLuint	buffer = 0;
BrushVertex* buffer_data = NULL;
BrushFace* faces_extradata = NULL;

int use_vertex_array = 0; // 1 - VAR, 2 - VBO


void FreeBuffer()
{
	if (buffer)
	{
		glDeleteBuffersARB(1, &buffer);
		buffer = 0;
	}

	if (buffer_data)
	{
		delete[] buffer_data;
		buffer_data = NULL;
	}

	if (faces_extradata)
	{
		delete[] faces_extradata;
		faces_extradata = NULL;
	}
}


void GenerateVertexArray()
{
	// TODO: remember mapname!!
	FreeBuffer();

	// clear all previous errors
	GLenum err;
	do {err = glGetError();} while (err != GL_NO_ERROR);

	// calculate number of used faces and vertexes
	int numfaces = 0;
	int numverts = 0;
	model_t *world = gEngfuncs.GetEntityByIndex(0)->model;
	msurface_t* surfaces = world->surfaces;
	for (int i = 0; i < world->numsurfaces; i++)
	{
		if (!(surfaces[i].flags & (SURF_DRAWSKY|SURF_DRAWTURB)))
		{
			glpoly_t *p = surfaces[i].polys;
			if (p->numverts > 0)
			{
				numfaces++;
				numverts += p->numverts;
			}
		}
	}

	// create vertex array
	int curvert = 0;
	int curface = 0;
	buffer_data = new BrushVertex[numverts];
	faces_extradata = new BrushFace[numfaces];
	for (i = 0; i < world->numsurfaces; i++)
	{
		if (!(surfaces[i].flags & (SURF_DRAWSKY|SURF_DRAWTURB)))
		{
			glpoly_t *p = surfaces[i].polys;
			if (p->numverts > 0)
			{
				float *v = p->verts[0];

				// hack: pack extradata index in unused bits of poly->flags
				int packed = (curface << 16);
				packed |= (p->flags & 0xFFFF);
				p->flags = packed;

				BrushFace* ext = &faces_extradata[curface];
				ext->start_vertex = curvert;

				// store tangent space
				VectorCopy(surfaces[i].texinfo->vecs[0], ext->s_tangent);
				VectorCopy(surfaces[i].texinfo->vecs[1], ext->t_tangent);
				VectorNormalize(ext->s_tangent);
				VectorNormalize(ext->t_tangent);
				VectorCopy(surfaces[i].plane->normal, ext->normal);
				if (surfaces[i].flags & SURF_PLANEBACK)
				{
					VectorInverse(ext->normal);
				}

				for (int j=0; j<p->numverts; j++, v+= VERTEXSIZE)
				{
					buffer_data[curvert].pos[0] = v[0];
					buffer_data[curvert].pos[1] = v[1];
					buffer_data[curvert].pos[2] = v[2];
					curvert++;
				}
				curface++;
			}
		}
	}

	if (ARB_VBO_supported)
	{
		glGenBuffersARB(1, &buffer);
		if (buffer)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
			glBufferDataARB(GL_ARRAY_BUFFER_ARB, numverts*sizeof(BrushVertex), buffer_data, GL_STATIC_DRAW_ARB);

			if (glGetError() != GL_OUT_OF_MEMORY)
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0); // remove binding
			}
			else
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0); // remove binding
				glDeleteBuffersARB(1, &buffer);
				buffer = 0;
			}
		}
	}
}

void EnableVertexArray()
{
	if (!buffer_data)
		return;

	if (buffer)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);

		// initialize pointer for VBO
		glVertexPointer( 3, GL_FLOAT, sizeof(BrushVertex), OFFSET(BrushVertex, pos) );
		use_vertex_array = 2;
	}
	else
	{
		// initialize pointer for vertex array
		glVertexPointer( 3, GL_FLOAT, sizeof(BrushVertex), &buffer_data[0].pos );
		use_vertex_array = 1;
	}

	glEnableClientState(GL_VERTEX_ARRAY);	
}

void DisableVertexArray()
{
	if (!use_vertex_array)
		return;

	if (use_vertex_array == 2)
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
}

void InitForFrame ( void )
{
	if (reloaded)
	{
		GenerateVertexArray();
		reloaded = 0;
	}
}
//================================
// Draws poly from vertex array or VBO
//================================
void DrawPolyFromArray (glpoly_t *p)
{
	int facedataindex = (p->flags >> 16) & 0xFFFF;
	BrushFace* ext = &faces_extradata[facedataindex];
	glDrawArrays(GL_POLYGON, ext->start_vertex, p->numverts);	
}
