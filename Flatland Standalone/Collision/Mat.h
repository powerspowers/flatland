//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

#ifndef	_MAT_H
#define	_MAT_H

/*--------------------------------------------------------------

	Structures & Classes

--------------------------------------------------------------*/

struct	MAT33
{
	float		Xx;
	float		Xy;
	float		Xz;

	float		Yx;
	float		Yy;
	float		Yz;

	float		Zx;
	float		Zy;
	float		Zz;
};


/*--------------------------------------------------------------

	Prototypes

--------------------------------------------------------------*/

void	MAT_setIdentity(MAT33 *mat_p);
void	MAT_mulInv(MAT33 *mat1_p, MAT33 *mat2_p, MAT33 *destMat_p);
void	MAT_mul(MAT33 *mat1_p, MAT33 *mat2_p, MAT33 *destMat_p);
void	MAT_setRot(MAT33 *mat_p, float az, float el, float tw);
void	MAT_copyInv(MAT33 *mat_p, MAT33 *destMat_p);


/*--------------------------------------------------------------

	Externs

--------------------------------------------------------------*/


#endif