//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

/*--------------------------------------------------------------

	Include files

--------------------------------------------------------------*/

#include <math.h>
#include "mat.h"
#include "maths.h"


/*--------------------------------------------------------------

	Initialise the matrix to identity

--------------------------------------------------------------*/

void	MAT_setIdentity(MAT33 *mat_p)
{
	mat_p->Xx = 1.0;
	mat_p->Xy = 0.0;
	mat_p->Xz = 0.0;

	mat_p->Yx = 0.0;
	mat_p->Yy = 1.0;
	mat_p->Yz = 0.0;

	mat_p->Zx = 0.0;
	mat_p->Zy = 0.0;
	mat_p->Zz = 1.0;
}


/*--------------------------------------------------------------

	Inversely multiply two matrices

--------------------------------------------------------------*/

void	MAT_mulInv(MAT33 *mat1_p, MAT33 *mat2_p, MAT33 *destMat_p)
{
	destMat_p->Xx = (mat1_p->Xx * mat2_p->Xx) + (mat1_p->Xy * mat2_p->Xy) + (mat1_p->Xz * mat2_p->Xz);
	destMat_p->Xy = (mat1_p->Xx * mat2_p->Yx) + (mat1_p->Xy * mat2_p->Yy) + (mat1_p->Xz * mat2_p->Yz);
	destMat_p->Xz = (mat1_p->Xx * mat2_p->Zx) + (mat1_p->Xy * mat2_p->Zy) + (mat1_p->Xz * mat2_p->Zz);

	destMat_p->Yx = (mat1_p->Yx * mat2_p->Xx) + (mat1_p->Yy * mat2_p->Xy) + (mat1_p->Yz * mat2_p->Xz);
	destMat_p->Yy = (mat1_p->Yx * mat2_p->Yx) + (mat1_p->Yy * mat2_p->Yy) + (mat1_p->Yz * mat2_p->Yz);
	destMat_p->Yz = (mat1_p->Yx * mat2_p->Zx) + (mat1_p->Yy * mat2_p->Zy) + (mat1_p->Yz * mat2_p->Zz);

	destMat_p->Zx = (mat1_p->Zx * mat2_p->Xx) + (mat1_p->Zy * mat2_p->Xy) + (mat1_p->Zz * mat2_p->Xz);
	destMat_p->Zy = (mat1_p->Zx * mat2_p->Yx) + (mat1_p->Zy * mat2_p->Yy) + (mat1_p->Zz * mat2_p->Yz);
	destMat_p->Zz = (mat1_p->Zx * mat2_p->Zx) + (mat1_p->Zy * mat2_p->Zy) + (mat1_p->Zz * mat2_p->Zz);
}


/*--------------------------------------------------------------

	Multiply two matrices together

--------------------------------------------------------------*/

void	MAT_mul(MAT33 *mat1_p, MAT33 *mat2_p, MAT33 *destMat_p)
{
	destMat_p->Xx = (mat1_p->Xx * mat2_p->Xx) + (mat1_p->Xy * mat2_p->Yx) + (mat1_p->Xz * mat2_p->Zx);
	destMat_p->Xy = (mat1_p->Xx * mat2_p->Xy) + (mat1_p->Xy * mat2_p->Yy) + (mat1_p->Xz * mat2_p->Zy);
	destMat_p->Xz = (mat1_p->Xx * mat2_p->Xz) + (mat1_p->Xy * mat2_p->Yz) + (mat1_p->Xz * mat2_p->Zz);
									    					    						 
	destMat_p->Yx = (mat1_p->Yx * mat2_p->Xx) + (mat1_p->Yy * mat2_p->Yx) + (mat1_p->Yz * mat2_p->Zx);
	destMat_p->Yy = (mat1_p->Yx * mat2_p->Xy) + (mat1_p->Yy * mat2_p->Yy) + (mat1_p->Yz * mat2_p->Zy);
	destMat_p->Yz = (mat1_p->Yx * mat2_p->Xz) + (mat1_p->Yy * mat2_p->Yz) + (mat1_p->Yz * mat2_p->Zz);
									    					    						 
	destMat_p->Zx = (mat1_p->Zx * mat2_p->Xx) + (mat1_p->Zy * mat2_p->Yx) + (mat1_p->Zz * mat2_p->Zx);
	destMat_p->Zy = (mat1_p->Zx * mat2_p->Xy) + (mat1_p->Zy * mat2_p->Yy) + (mat1_p->Zz * mat2_p->Zy);
	destMat_p->Zz = (mat1_p->Zx * mat2_p->Xz) + (mat1_p->Zy * mat2_p->Yz) + (mat1_p->Zz * mat2_p->Zz);
}


/*--------------------------------------------------------------

	Work out the matrix from the 3 angles (radians)

--------------------------------------------------------------*/

void	MAT_setRot(MAT33 *mat_p, float az, float el, float tw)
{
	float	cx, cy, cz,
			sx, sy, sz;


	cx = MATHS_cos(el);
	sx = MATHS_sin(el);

	cy = MATHS_cos(az);
	sy = MATHS_sin(az);

	cz = MATHS_cos(tw);
	sz = MATHS_sin(tw);

	mat_p->Xx = (cy * cz);
	mat_p->Xy = (cy * sz);
	mat_p->Xz = -sy;

	mat_p->Yx = (-sz*cx) + (sx*sy*cz);
	mat_p->Yy = (cx*cz) + (sx*sy*sz);
	mat_p->Yz = sx*cy;

	mat_p->Zx = (sx*sz) + (cx*sy*cz);
	mat_p->Zy = (-sx*cz) + (cx*sy*sz);
	mat_p->Zz = (cx*cy);
}


/*--------------------------------------------------------------

	Copy a matrix inversely

--------------------------------------------------------------*/

void	MAT_copyInv(MAT33 *mat_p, MAT33 *destMat_p)
{
	destMat_p->Xx = mat_p->Xx; destMat_p->Xy = mat_p->Yx; destMat_p->Xz = mat_p->Zx;
	destMat_p->Yx = mat_p->Xy; destMat_p->Yy = mat_p->Yy; destMat_p->Yz = mat_p->Zy;
	destMat_p->Zx = mat_p->Xz; destMat_p->Zy = mat_p->Yz; destMat_p->Zz = mat_p->Zz;
}

