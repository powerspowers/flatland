//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

/********************************************************************

	DESCRIPTION :	Header file

	AUTHOR		:	Matt Wilkinson

********************************************************************/


#ifndef _COLLISION_H
#define _COLLISION_H

/*--------------------------------------------------------------

	Includes

--------------------------------------------------------------*/

#include "col.h"
struct block;
struct block_def;


/*--------------------------------------------------------------

	Defines

--------------------------------------------------------------*/


/*--------------------------------------------------------------

	Structures & Classes

--------------------------------------------------------------*/


/*--------------------------------------------------------------

	Prototypes

--------------------------------------------------------------*/

bool
COL_createBlockColMesh(block *block_ptr);

void 
COL_convertBlockToColMesh(block *block_ptr);

bool
COL_createSpriteColMesh(block *block_ptr);

void 
COL_convertSpriteToColMesh(COL_MESH *mesh_ptr, float minX, float minY, 
						   float minZ, float maxX, float maxY, float maxZ);

/*--------------------------------------------------------------

	Externs

--------------------------------------------------------------*/

#endif

