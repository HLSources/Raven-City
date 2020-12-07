//========= Copyright � 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

#if !defined ( STUDIOMODELRENDERER_H )
#define STUDIOMODELRENDERER_H
#if defined( _WIN32 )
#pragma once
#endif

// buz start

// disable "identifier was truncated to '255' characters in the browser information" messages
#pragma warning( disable: 4786 )
#pragma warning( disable: 4018 )

#include "windows.h"
#include "gl/gl.h"
#include "dlight.h"
#include <assert.h>

#include <vector>
#include <map>
#include <string>

#define CONPRINT gEngfuncs.Con_Printf

// some precomputed data about model, for shadow volumes optimization
struct Face
{
	Face() {}
	Face(GLushort v0, GLushort v1, GLushort v2)
	{
		vertexIndexes[0] = v0;
		vertexIndexes[1] = v1;
		vertexIndexes[2] = v2;
	}
	Face(GLushort v0, GLushort v1, GLushort v2,
		GLushort v3, GLushort v4, GLushort v5)
	{
		vertexIndexes[0] = v0;
		vertexIndexes[1] = v1;
		vertexIndexes[2] = v2;

		neighborIndexes[0] = v3;
		neighborIndexes[1] = v4;
		neighborIndexes[2] = v5;
	}
	int vertexIndexes[3];
	int neighborIndexes[3];
};

struct shadowsubmodel_t
{
	std::vector<Face> faces;

	int numverts;
	int boneindexes[MAXSTUDIOVERTS];
	vec3_t vertices[MAXSTUDIOVERTS];
};
struct shadowhdr_t
{
	std::vector<shadowsubmodel_t> submodels;
};
typedef std::map < std::string, shadowhdr_t > datamap_t;
// buz end

/*
====================
CStudioModelRenderer

====================
*/
class CStudioModelRenderer
{
public:
	// Construction/Destruction
	CStudioModelRenderer( void );
	virtual ~CStudioModelRenderer( void );

	// Initialization
	virtual void Init( void );

public:  
	// Public InterFaces
	virtual int StudioDrawModel ( int flags );
	virtual int StudioDrawPlayer ( int flags, struct entity_state_s *pplayer );

public:
	// Local interFaces
	//

	// Look up animation data for sequence
	virtual mstudioanim_t *StudioGetAnim ( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc );

	// Interpolate model position and angles and set up matrices
	virtual void StudioSetUpTransform (int trivial_accept);

	// Set up model bone positions
	virtual void StudioSetupBones ( void );	

	// Find final attachment points
	virtual void StudioCalcAttachments ( void );
	
	// Save bone matrices and names
	virtual void StudioSaveBones( void );

	// Merge cached bones with current bones for model
	virtual void StudioMergeBones ( model_t *m_pSubModel );

	// Determine interpolation fraction
	virtual float StudioEstimateInterpolant( void );

	// Determine current frame for rendering
	virtual float StudioEstimateFrame ( mstudioseqdesc_t *pseqdesc );

	// Apply special effects to transform matrix
	virtual void StudioFxTransform( cl_entity_t *ent, float transform[3][4] );

	// Spherical interpolation of bones
	virtual void StudioSlerpBones ( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s );

	// Compute bone adjustments ( bone controllers )
	virtual void StudioCalcBoneAdj ( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen );

	// Get bone quaternions
	virtual void StudioCalcBoneQuaterion ( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q );

	// Get bone positions
	virtual void StudioCalcBonePosition ( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos );

	// Compute rotations
	virtual void StudioCalcRotations ( float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f );

	// Send bones and verts to renderer
	virtual void StudioRenderModel ( void );

	// Finalize rendering
	virtual void StudioRenderFinal ( void );

	// Player specific data
	// Determine pitch and blending amounts for players
	virtual void StudioPlayerBlend ( mstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch );

	// Estimate gait frame for player
	virtual void StudioEstimateGait ( entity_state_t *pplayer );

	// Process movement of player
	virtual void StudioProcessGait ( entity_state_t *pplayer );

public:

	// Client clock
	double			m_clTime;				
	// Old Client clock
	double			m_clOldTime;			

	// Do interpolation?
	int				m_fDoInterp;			
	// Do gait estimation?
	int				m_fGaitEstimation;		

	// Current render frame #
	int				m_nFrameCount;

	// Cvars that studio model code needs to reference
	//
	// Use high quality models?
	cvar_t			*m_pCvarHiModels;	
	// Developer debug output desired?
	cvar_t			*m_pCvarDeveloper;
	// Draw entities bone hit boxes, etc?
	cvar_t			*m_pCvarDrawEntities;

	// Draw model meshes? Debug feature.
	cvar_t			*m_pCvarDrawModels;

	// The entity which we are currently rendering.
	cl_entity_t		*m_pCurrentEntity;

	// The model for the entity being rendered
	model_t			*m_pRenderModel;

	// Player info for current player, if drawing a player
	player_info_t	*m_pPlayerInfo;

	// The index of the player being drawn
	int				m_nPlayerIndex;

	// The player's gait movement
	float			m_flGaitMovement;

	// Pointer to header block for studio model data
	studiohdr_t		*m_pStudioHeader;
	
	// Pointers to current body part and submodel
	mstudiobodyparts_t *m_pBodyPart;
	mstudiomodel_t	*m_pSubModel;

	// Palette substition for top and bottom of model
	int				m_nTopColor;			
	int				m_nBottomColor;

	//
	// Sprite model used for drawing studio model chrome
	model_t			*m_pChromeSprite;

	// Caching
	// Number of bones in bone cache
	int				m_nCachedBones; 
	// Names of cached bones
	char			m_nCachedBoneNames[ MAXSTUDIOBONES ][ 32 ];
	// Cached bone & light transformation matrices
	float			m_rgCachedBoneTransform [ MAXSTUDIOBONES ][ 3 ][ 4 ];
	float			m_rgCachedLightTransform[ MAXSTUDIOBONES ][ 3 ][ 4 ];

	// Software renderer scale factors
	float			m_fSoftwareXScale, m_fSoftwareYScale;

	// Current view vectors and render origin
	float			m_vUp[ 3 ];
	float			m_vRight[ 3 ];
	float			m_vNormal[ 3 ];

	float			m_vRenderOrigin[ 3 ];
	
	// Model render counters ( from engine )
	int				*m_pStudioModelCount;
	int				*m_pModelsDrawn;

	// Matrices
	// Model to world transformation
	float			(*m_protationmatrix)[ 3 ][ 4 ];	
	// Model to view transformation
	float			(*m_paliastransform)[ 3 ][ 4 ];	

	// Concatenated bone and light transforms
	float			(*m_pbonetransform) [ MAXSTUDIOBONES ][ 3 ][ 4 ];
	float			(*m_plighttransform)[ MAXSTUDIOBONES ][ 3 ][ 4 ];

public:
	// Is the selected rendermode supported?
	virtual void	StudioRenderingCheck( void );
	// Check the given extension;
	virtual BOOL	StudioCheckExtension( const char *ext );

	// The hacked opengl32 library.
	HMODULE			m_hOpenGL32Dll;

public:
	// Shadow rendering calls.
	virtual void	StudioShadowForEntity( cl_entity_t *pEntity );

	// Prepare data for processing.
	virtual void	StudioSetExtraData ( void );
	virtual void	StudioBuildTriangles ( shadowsubmodel_t &submodel, mstudiomodel_t *src );

	// Main rendering line.
	virtual void	StudioSetupShadows ( void );
	virtual void	StudioDrawShadow ( shadowsubmodel_t &submodel );

	// Process data for shadow volumes.
	virtual void	StudioBuildNeighbors( shadowsubmodel_t &submodel ); 
	virtual void	StudioGetNeighbors( shadowsubmodel_t &submodel, Face &triangle, int trianglenum );
	virtual void	StudioSpecialProcess( shadowsubmodel_t &submodel );

	// Is the given triangle facing the light angle or origin?
	virtual bool	StudioFacingLight( Face &triangle );

	// Shadow data writing functions.
	virtual void	StudioWriteData( void );
	virtual bool	StudioLoadData( void );

	// Find the nearest light.
	virtual void	StudioSwapLights( void );

	// Data for shadow volume rendering.
	datamap_t		m_pDataMap;
	shadowhdr_t		*m_pShadowHeader;

	// Light detection related code.
	vec3_t			m_vShadowAngle;
	// Position of the nearest light.
	vec3_t			m_vLightPosition;
	
	// Are we trying to render angle or light origin offset shadows?
	bool			m_fShadowType;

	// Are we allowed to render the stencil shadows?
	cvar_t			*m_pCvarShadows;
	// Should we try to get light angles or an origin?
	cvar_t			*m_pCvarShadowsDynamic;
	// Debug version.
	cvar_t			*m_pCvarShadowsDebug;

	// Saved bone transformation for shadow rendering, saves a lot on fps.
	float			(*m_pshadowbonetransform) [ MAXSTUDIOBONES ][ 3 ][ 4 ];
};

#endif // STUDIOMODELRENDERER_H