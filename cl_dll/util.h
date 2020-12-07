//========= Copyright © 2004-2008, Raven City Team, All rights reserved. ============//
//																					 //
// Purpose:																			 //
//																					 //
// $NoKeywords: $																	 //
//===================================================================================//

//  Util.h 
// Mainly Fog related API's and stuff, not much more.

// Misc C-runtime library headers
#include "STDIO.H"
#include "STDLIB.H"
#include "MATH.H"


float TransformColor ( float color );
void	AngleMatrixTriAPI (const float *angles, float (*matrix)[4] );
void	VectorTransformTriAPI (const float *in1, float in2[3][4], float *out);
void	VectorAngles( const float *forward, float *angles );