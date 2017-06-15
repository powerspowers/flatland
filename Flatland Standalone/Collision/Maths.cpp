//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************


/*--------------------------------------------------------------

	Include files

--------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "maths.h"
#include "../Classes.h"
#include "../Memory.h"

/*--------------------------------------------------------------

	Private Variables

--------------------------------------------------------------*/

float				*cosTab = NULL,
					*sinTab = NULL,
					MATHS_gSqrtTable[NUM_SQRT_ENTRIES];
uMATHS_FLOAT_INT	MATHS_gFloatInt;


/*--------------------------------------------------------------

	Initialise the maths tables, etc.

--------------------------------------------------------------*/

void	MATHS_init(void)
{
	int		i;
	double	num;

	NEWARRAY(cosTab, float, COSTAB_ENTRIES);
	NEWARRAY(sinTab, float, SINTAB_ENTRIES);

	//------ Now set up the tables -----------------------------

	num = 0;
	for (i=0; i<COSTAB_ENTRIES; i++)
	{
		cosTab[i] = (float)cos(num);
		num += (double)(TWOPI_D / COSTAB_ENTRIES);
	}

	num = 0;
	for (i=0; i<SINTAB_ENTRIES; i++)
	{
		sinTab[i] = (float)sin(num);
		num += (double)(TWOPI_D / SINTAB_ENTRIES);
	}

	//------ Set up the SQRT table -----------------------------

	for (i = 0; i < NUM_SQRT_ENTRIES; i++)
	{
		MATHS_gFloatInt.i = ((i - HALF_SQRT_ENTRIES) << 13) + 0x40000000;
		MATHS_gSqrtTable[(i - HALF_SQRT_ENTRIES) & 0xffff] = (float)(sqrt(MATHS_gFloatInt.f));
	}
}


/*--------------------------------------------------------------

	Close down the maths system

--------------------------------------------------------------*/

void	MATHS_exit(void)
{
	DELBASEARRAY(cosTab, float, COSTAB_ENTRIES);
	DELBASEARRAY(sinTab, float, SINTAB_ENTRIES);
}


/*--------------------------------------------------------------

	Return the Cosine of an angle in radians

--------------------------------------------------------------*/

float	MATHS_cos(float num)
{
	int		idx;

	idx = (int)((float)num*COSTAB_SCALE);
	idx &= COSTAB_MASK;
	return (cosTab[idx]);
}


/*--------------------------------------------------------------

	Return the Sine of an angle in radians

--------------------------------------------------------------*/

float	MATHS_sin(float num)
{
	int		idx;

	idx = (int)((float)num*SINTAB_SCALE);
	idx &= SINTAB_MASK;
	return (sinTab[idx]);
}


/*--------------------------------------------------------------

	An extremely fast table-based squareroot function

	This table is not a general purpose log table sqroot. It 
 	is biased for LOW exponent numbers upto 1 billion.
 	
--------------------------------------------------------------*/

float	MATHS_sqrt(float num)
{
	MATHS_gFloatInt.f = num;
	return(MATHS_gSqrtTable[(MATHS_gFloatInt.i >> 13) & 0xffff]);
}


