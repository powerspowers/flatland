//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#ifndef _MATHS_H
#define	_MATHS_H

/*--------------------------------------------------------------

	Defines

--------------------------------------------------------------*/

#define	COSTAB_ENTRIES		65536
#define	SINTAB_ENTRIES		65536
#define	COSTAB_MASK			0xffff
#define	SINTAB_MASK			0xffff

#define	TWOPI				(float)6.283185307
#define	TWOPI_D				(double)6.283185307

#define	COSTAB_SCALE		(float)(COSTAB_ENTRIES/TWOPI)
#define	SINTAB_SCALE		(float)(SINTAB_ENTRIES/TWOPI)

#define NUM_SQRT_ENTRIES	65536
#define HALF_SQRT_ENTRIES	(NUM_SQRT_ENTRIES / 2)


/*--------------------------------------------------------------

	Macros

--------------------------------------------------------------*/

#define SIGN(a)				((a)>0.0 ? (true) : (false))
#define BNEG(a)				((a)>0.0 ? (false) : (true))


 //------ Swaps two variables of the same type -----------------

#define swap( a, b, type ) { type temp(a); a=b; b=temp; }


//------ Returns the maximum of two values ---------------------

#define MAX2( a, b ) (((a) > (b)) ? (a) : (b))

//------ Returns the maximum of three values -------------------

#define MAX3(a,b,c) (MAX2(a,MAX2(b,c)))


/*--------------------------------------------------------------

	Structures & Classes

--------------------------------------------------------------*/

typedef union _uMATHS_FLOAT_INT
{

	float			f;
	unsigned int	i;

} uMATHS_FLOAT_INT;


/*--------------------------------------------------------------

	Prototypes

--------------------------------------------------------------*/

void	MATHS_init(void);
void	MATHS_exit(void);
float	MATHS_cos(float num);
float	MATHS_sin(float num);
float	MATHS_sqrt(float num);


/*--------------------------------------------------------------

	Externs

--------------------------------------------------------------*/


#endif