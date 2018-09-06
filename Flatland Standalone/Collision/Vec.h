//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

#ifndef	_VEC_H
#define	_VEC_H

/*--------------------------------------------------------------

	Includes

--------------------------------------------------------------*/

#include "mat.h"

/*--------------------------------------------------------------

	Macros

--------------------------------------------------------------*/

	//------ This converts the signs of the vector elements ----
	//------ into a numerical value ----------------------------

#define VEC3_SIGNS(vecX, vecY, vecZ) ((BNEG(vecX)<<2) + (BNEG(vecY)<<1) + BNEG(vecZ));


/*--------------------------------------------------------------

	Structures & Classes

--------------------------------------------------------------*/

struct	VEC3
{
	float		x;
	float		y;
	float		z;
};


/*--------------------------------------------------------------

	Prototypes

--------------------------------------------------------------*/

void	VEC_set(VEC3 *vec_p, float x, float y, float z);
void	VEC_sub(VEC3 *vec1, VEC3 *vec2, VEC3 *vec3);
void	VEC_add(VEC3 *vec1, VEC3 *vec2, VEC3 *vec3);
void	VEC_mulMat(VEC3 *vec_p, MAT33 *mat_p, VEC3 *destVec_p);
void 	VEC_mulMatInv(VEC3 *vec_p, MAT33 *mat_p, VEC3 *destVec_p);
float	VEC_dot(VEC3 *vec1_p, VEC3 *vec2_p);
void	VEC_normalise(VEC3 *vec_p);
float	VEC_length(VEC3 *vec_p);


/*--------------------------------------------------------------

	Externs

--------------------------------------------------------------*/


#endif
