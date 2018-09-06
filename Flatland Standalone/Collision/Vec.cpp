//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

/*--------------------------------------------------------------

	Include files

--------------------------------------------------------------*/

#include "vec.h"
#include "maths.h"


/*--------------------------------------------------------------

	Set the values of a vector

--------------------------------------------------------------*/

void	VEC_set(VEC3 *vec_p, float x, float y, float z)
{
	vec_p->x = x;
	vec_p->y = y;
	vec_p->z = z;
}


/*--------------------------------------------------------------

	Subtract two vectors

--------------------------------------------------------------*/

void	VEC_sub(VEC3 *vec1, VEC3 *vec2, VEC3 *vec3)
{
	vec3->x = vec1->x-vec2->x;
	vec3->y = vec1->y-vec2->y;
	vec3->z = vec1->z-vec2->z;
}


/*--------------------------------------------------------------

	Add two vectors

--------------------------------------------------------------*/

void	VEC_add(VEC3 *vec1, VEC3 *vec2, VEC3 *vec3)
{
	vec3->x = vec1->x+vec2->x;
	vec3->y = vec1->y+vec2->y;
	vec3->z = vec1->z+vec2->z;
}


/*--------------------------------------------------------------

	Multiply a vector by a matrix

--------------------------------------------------------------*/

void	VEC_mulMat(VEC3 *vec_p, MAT33 *mat_p, VEC3 *destVec_p)
{
	destVec_p->x = (vec_p->x * mat_p->Xx) + (vec_p->y * mat_p->Yx) + (vec_p->z * mat_p->Zx);
	destVec_p->y = (vec_p->x * mat_p->Xy) + (vec_p->y * mat_p->Yy) + (vec_p->z * mat_p->Zy);
	destVec_p->z = (vec_p->x * mat_p->Xz) + (vec_p->y * mat_p->Yz) + (vec_p->z * mat_p->Zz);
}


/*--------------------------------------------------------------

	Inversely multiply a vector by a matrix

--------------------------------------------------------------*/

void 	VEC_mulMatInv(VEC3 *vec_p, MAT33 *mat_p, VEC3 *destVec_p)
{
	destVec_p->x = (vec_p->x * mat_p->Xx) + (vec_p->y * mat_p->Xy) + (vec_p->z * mat_p->Xz);
	destVec_p->y = (vec_p->x * mat_p->Yx) + (vec_p->y * mat_p->Yy) + (vec_p->z * mat_p->Yz);
	destVec_p->z = (vec_p->x * mat_p->Zx) + (vec_p->y * mat_p->Zy) + (vec_p->z * mat_p->Zz);
}


/*--------------------------------------------------------------

	Return the dot product of two vectors

--------------------------------------------------------------*/

float	VEC_dot(VEC3 *vec1_p, VEC3 *vec2_p)
{
	return ((vec1_p->x*vec2_p->x) + (vec1_p->y*vec2_p->y) + (vec1_p->z*vec2_p->z));
}


/*--------------------------------------------------------------

	Normalise the vector

--------------------------------------------------------------*/

void	VEC_normalise(VEC3 *vec_p)
{
	float	norm;


	norm = (float)MATHS_sqrt(vec_p->x*vec_p->x + vec_p->y*vec_p->y + vec_p->z*vec_p->z);

	vec_p->x = vec_p->x / norm;
	vec_p->y = vec_p->y / norm;
	vec_p->z = vec_p->z / norm;
}


/*--------------------------------------------------------------

	Return the length of the vector

--------------------------------------------------------------*/

float	VEC_length(VEC3 *vec_p)
{
	return((float)MATHS_sqrt(vec_p->x*vec_p->x + vec_p->y*vec_p->y + vec_p->z*vec_p->z));
}