//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

#include <stdlib.h>
#include <stdio.h>

#include "collision.h"
#include "..\Classes.h"
#include "..\Main.h"
#include "..\Parser.h"
#include "..\Memory.h"

//-----------------------------------------------------------------------------
// Create a collision mesh of the given size.
//-----------------------------------------------------------------------------

static bool
COL_createColMesh(block *block_ptr, int vertices, int edges, int triangles)
{
	byte *temp_ptr;
	COL_MESH *col_mesh_ptr;
	int col_mesh_size;

	// Compute the size of the collision mesh structure.

	col_mesh_size = sizeof(COL_MESH) + vertices * sizeof(VEC3) + 
		edges * sizeof(VEC3) + triangles * sizeof(COL_POLY3);

	// Allocate the collision mesh structure.

	NEWARRAY(temp_ptr, colmeshbyte, col_mesh_size);
	if (temp_ptr == NULL)
		return(false);
	col_mesh_ptr = (COL_MESH *)temp_ptr;

	// Set up the collision mesh pointers.

	temp_ptr += sizeof(COL_MESH);
	col_mesh_ptr->v = (VEC3 *)temp_ptr;
	temp_ptr += sizeof(VEC3) * vertices;
	col_mesh_ptr->e = (VEC3 *)temp_ptr;
	temp_ptr += sizeof(VEC3) * edges;
	col_mesh_ptr->p = (COL_POLY3 *)temp_ptr;

	// Set up the list sizes.

	col_mesh_ptr->numVerts = vertices;
	col_mesh_ptr->numEdges = edges;
	col_mesh_ptr->numPolys = triangles;

	// Store the collision mesh pointer and size in the block.

	block_ptr->col_mesh_ptr = col_mesh_ptr;
	block_ptr->col_mesh_size = col_mesh_size;
	return(true);
}

//-----------------------------------------------------------------------------
//	Create a collision mesh for a block.
//-----------------------------------------------------------------------------

bool
COL_createBlockColMesh(block *block_ptr)
{
	block_def *block_def_ptr;
	int	index, triangles;
	polygon_def *polygon_def_ptr;
	part *part_ptr;

	// Determine the number of triangles, vertices and edges, then create
	// the collision mesh.

	triangles = 0;
	block_def_ptr = block_ptr->block_def_ptr;
	for (index = 0; index < block_def_ptr->polygons; index++) {
		polygon_def_ptr = &block_def_ptr->polygon_def_list[index];
		part_ptr = polygon_def_ptr->part_ptr;
		if (part_ptr->solid)
			triangles += polygon_def_ptr->vertices - 2;
	}
	return(COL_createColMesh(block_ptr, block_def_ptr->vertices, 
		triangles * 3, triangles));
}

//-----------------------------------------------------------------------------
//	Convert a block into a collision mesh
//-----------------------------------------------------------------------------

void
COL_convertBlockToColMesh(block *block_ptr)
{
	COL_MESH	*mesh_ptr;
	block_def	*block_def_ptr;
	int			i, j, k, n,
				v[3],
				numVerts,
				numEdges, thisEdge,
				numTris;
	VEC3		meshMin, meshMax;
	COL_POLY3	*poly_ptr;
	polygon		*polygon_ptr;
	polygon_def	*polygon_def_ptr;
	part		*part_ptr;


	//------ Initialise the min/max box ----------------------------

	VEC_set(&meshMin, 999999.0f, 999999.0f, 999999.0f);
	VEC_set(&meshMax, -999999.0f, -999999.0f, -999999.0f);

	
	//------ Get the mesh data -------------------------------------

	mesh_ptr = block_ptr->col_mesh_ptr;
	numVerts = mesh_ptr->numVerts;
	numEdges = mesh_ptr->numEdges;
	numTris = mesh_ptr->numPolys;


	// Compute the edge list for the solid polygons.

	thisEdge = 0;
	block_def_ptr = block_ptr->block_def_ptr;
	for (i = 0; i < block_def_ptr->polygons; i++) {
		polygon_def_ptr = &block_def_ptr->polygon_def_list[i];
		part_ptr = polygon_def_ptr->part_ptr;
		if (!part_ptr->solid)
			continue;

		for (n = 0; n < polygon_def_ptr->vertices - 2; n++) {
			v[0] = polygon_def_ptr->vertex_def_list[0].vertex_no;
			v[1] = polygon_def_ptr->vertex_def_list[n+1].vertex_no;
			v[2] = polygon_def_ptr->vertex_def_list[n+2].vertex_no;

			for (j = 0; j < 3; j++) {
				mesh_ptr->e[thisEdge].x = 
					block_ptr->vertex_list[v[(j + 1) % 3]].x -
					block_ptr->vertex_list[v[j]].x;
				mesh_ptr->e[thisEdge].y = 
					block_ptr->vertex_list[v[(j + 1) % 3]].y -
					block_ptr->vertex_list[v[j]].y;
				mesh_ptr->e[thisEdge].z = 
					block_ptr->vertex_list[v[(j + 1) % 3]].z -
					block_ptr->vertex_list[v[j]].z;
				thisEdge++;
			}
		}
	}


	//------ Copy the vertex data ------------------------------

	for (i = 0; i < block_def_ptr->vertices; i++) {
		mesh_ptr->v[i].x = block_ptr->vertex_list[i].x;
		mesh_ptr->v[i].y = block_ptr->vertex_list[i].y;
		mesh_ptr->v[i].z = block_ptr->vertex_list[i].z;
	}

	//------ Create the polygon data ---------------------------

	i = 0;
	thisEdge = 0;
	for (k = 0; k < block_def_ptr->polygons; k++) {
		bool double_sided;

		// Get a pointer to this polygon and it's part.  If this polygon is
		// not solid, ignore it.

		polygon_ptr = &block_ptr->polygon_list[k];
		polygon_def_ptr = polygon_ptr->polygon_def_ptr;
		part_ptr = polygon_def_ptr->part_ptr;
		if (!part_ptr->solid)
			continue;

		// Determine whether or not the polygon is double-sided.

		double_sided = block_def_ptr->part_list[polygon_def_ptr->part_no].faces 
			!= 1;

		//------ If this poly has more than three vertices -----
		//------ then we need to tesselate it into tri's -------

		v[0] = polygon_def_ptr->vertex_def_list[0].vertex_no;

		for (n = 0; n < polygon_def_ptr->vertices - 2; n++) {
			poly_ptr = &mesh_ptr->p[i];

			// Set the polygon's double-sided flag.

			poly_ptr->double_sided = double_sided;

			poly_ptr->normal.x = polygon_ptr->normal_vector.dx;
			poly_ptr->normal.y = polygon_ptr->normal_vector.dy;
			poly_ptr->normal.z = polygon_ptr->normal_vector.dz;

			VEC_normalise(&poly_ptr->normal);

			v[1] = polygon_def_ptr->vertex_def_list[n + 1].vertex_no;
			v[2] = polygon_def_ptr->vertex_def_list[n + 2].vertex_no;

			for (j = 0; j < 3; j++) {
				poly_ptr->vIdx[j] = v[j];
				poly_ptr->edgeIdx[j] = thisEdge;
				thisEdge++;

				if (j == 0) {
					poly_ptr->min.x = block_ptr->vertex_list[v[0]].x;
					poly_ptr->min.y = block_ptr->vertex_list[v[0]].y;
					poly_ptr->min.z = block_ptr->vertex_list[v[0]].z;

					poly_ptr->max.x = block_ptr->vertex_list[v[0]].x;
					poly_ptr->max.y = block_ptr->vertex_list[v[0]].y;
					poly_ptr->max.z = block_ptr->vertex_list[v[0]].z;
				} else {
					if (block_ptr->vertex_list[v[j]].x < poly_ptr->min.x)
						poly_ptr->min.x = block_ptr->vertex_list[v[j]].x;
					if (block_ptr->vertex_list[v[j]].x > poly_ptr->max.x)
						poly_ptr->max.x = block_ptr->vertex_list[v[j]].x;

					if (block_ptr->vertex_list[v[j]].y < poly_ptr->min.y)
						poly_ptr->min.y = block_ptr->vertex_list[v[j]].y;
					if (block_ptr->vertex_list[v[j]].y > poly_ptr->max.y)
						poly_ptr->max.y = block_ptr->vertex_list[v[j]].y;

					if (block_ptr->vertex_list[v[j]].z < poly_ptr->min.z)
						poly_ptr->min.z = block_ptr->vertex_list[v[j]].z;
					if (block_ptr->vertex_list[v[j]].z > poly_ptr->max.z)
						poly_ptr->max.z = block_ptr->vertex_list[v[j]].z;
				}
			}

			//------ Check the mesh bounding box ---------------

			if (poly_ptr->min.x < meshMin.x)
				meshMin.x = poly_ptr->min.x;
			if (poly_ptr->min.y < meshMin.y)
				meshMin.y = poly_ptr->min.y;
			if (poly_ptr->min.z < meshMin.z)
				meshMin.z = poly_ptr->min.z;

			if (poly_ptr->max.x > meshMax.x)
				meshMax.x = poly_ptr->max.x;
			if (poly_ptr->max.y > meshMax.y)
				meshMax.y = poly_ptr->max.y;
			if (poly_ptr->max.z > meshMax.z)
				meshMax.z = poly_ptr->max.z;

			//------ Determine the major axis of the face ------
			//------ normal ------------------------------------
		
			if (FABS(poly_ptr->normal.x) == MAX3(FABS(poly_ptr->normal.x),
				FABS(poly_ptr->normal.y), FABS(poly_ptr->normal.z))) 
				poly_ptr->normalMajorAxis = COL_MAJOR_X;
			else if (FABS(poly_ptr->normal.y) == MAX3(FABS(poly_ptr->normal.x),
				FABS(poly_ptr->normal.y), FABS(poly_ptr->normal.z)))
				poly_ptr->normalMajorAxis = COL_MAJOR_Y;
			else if (FABS(poly_ptr->normal.z) == MAX3(FABS(poly_ptr->normal.x),
				FABS(poly_ptr->normal.y), FABS(poly_ptr->normal.z))) 
				poly_ptr->normalMajorAxis = COL_MAJOR_Z;

			i++;
		}
	}


	//------ Set the mesh bounding box -------------------------

	mesh_ptr->minBox.x = meshMin.x;
	mesh_ptr->minBox.y = meshMin.y;
	mesh_ptr->minBox.z = meshMin.z;

	mesh_ptr->maxBox.x = meshMax.x;
	mesh_ptr->maxBox.y = meshMax.y;
	mesh_ptr->maxBox.z = meshMax.z;
}

//-----------------------------------------------------------------------------
//	Create a collision mesh for a sprite.
//-----------------------------------------------------------------------------

bool
COL_createSpriteColMesh(block *block_ptr)
{
	return(COL_createColMesh(block_ptr, 8, 36, 12)); 
}

//-----------------------------------------------------------------------------
// Create a box collison mesh (for sprites).
//-----------------------------------------------------------------------------

void 
COL_convertSpriteToColMesh(COL_MESH *mesh_ptr, float minX, float minY, 
						   float minZ, float maxX, float maxY, float maxZ)
{
	int			i, j,
				numVerts,
				numEdges, thisEdge,
				numTris,
				p[12][3];
	VEC3		meshMin, meshMax,
				v[8],
				pn[12];
	COL_POLY3	*poly_ptr;


	//------ Initialise the min/max box ------------------------

	VEC_set(&meshMin, 999999.0f, 999999.0f, 999999.0f);
	VEC_set(&meshMax, -999999.0f, -999999.0f, -999999.0f);


	//------ Set the number of triangles, vertices and edges ---

	numTris = 12;
	numVerts = 8;
	numEdges = numTris * 3;

	//------ Set the 8 vertices --------------------------------

	v[0].x = minX;
	v[0].y = maxY;
	v[0].z = maxZ;

	v[1].x = maxX;
	v[1].y = maxY;
	v[1].z = maxZ;

	v[2].x = maxX;
	v[2].y = maxY;
	v[2].z = minZ;

	v[3].x = minX;
	v[3].y = maxY;
	v[3].z = minZ;

	v[4].x = minX;
	v[4].y = minY;
	v[4].z = maxZ;

	v[5].x = maxX;
	v[5].y = minY;
	v[5].z = maxZ;

	v[6].x = maxX;
	v[6].y = minY;
	v[6].z = minZ;

	v[7].x = minX;
	v[7].y = minY;
	v[7].z = minZ;


	//------ And the vertex indexes ----------------------------

	p[0][0] = 0;			// Top
	p[0][1] = 1;
	p[0][2] = 2;

	p[1][0] = 0;
	p[1][1] = 2;
	p[1][2] = 3;

	p[2][0] = 5;			// Bottom
	p[2][1] = 4;
	p[2][2] = 7;

	p[3][0] = 5;
	p[3][1] = 7;
	p[3][2] = 6;

	p[4][0] = 0;			// L side
	p[4][1] = 3;
	p[4][2] = 7;

	p[5][0] = 0;
	p[5][1] = 7;
	p[5][2] = 4;

	p[6][0] = 2;			// R side
	p[6][1] = 1;
	p[6][2] = 5;

	p[7][0] = 2;
	p[7][1] = 5;
	p[7][2] = 6;

	p[8][0] = 3;			// Front
	p[8][1] = 2;
	p[8][2] = 6;

	p[9][0] = 3;
	p[9][1] = 6;
	p[9][2] = 7;

	p[10][0] = 1;			// Back
	p[10][1] = 0;
	p[10][2] = 4;

	p[11][0] = 1;
	p[11][1] = 4;
	p[11][2] = 5;


	//------ And the normals -----------------------------------

	VEC_set(&pn[0], 0.0f, 1.0f, 0.0f);
	VEC_set(&pn[1], 0.0f, 1.0f, 0.0f);

	VEC_set(&pn[2], 0.0f, -1.0f, 0.0f);
	VEC_set(&pn[3], 0.0f, -1.0f, 0.0f);

	VEC_set(&pn[4], -1.0f, 0.0f, 0.0f);
	VEC_set(&pn[5], -1.0f, 0.0f, 0.0f);

	VEC_set(&pn[6], 1.0f, 0.0f, 0.0f);
	VEC_set(&pn[7], 1.0f, 0.0f, 0.0f);

	VEC_set(&pn[8], 0.0f, 0.0f, -1.0f);
	VEC_set(&pn[9], 0.0f, 0.0f, -1.0f);

	VEC_set(&pn[10], 0.0f, 0.0f, 1.0f);
	VEC_set(&pn[11], 0.0f, 0.0f, 1.0f);



	//------ Compute the edge list --------------------------

	thisEdge = 0;
	for (i = 0; i < numTris; i++) {
		for (j=0; j<3; j++) {
			mesh_ptr->e[thisEdge].x = (float)v[p[i][(j+1)%3]].x -
				(float)v[p[i][j]].x;
			mesh_ptr->e[thisEdge].y = (float)v[p[i][(j+1)%3]].y -
				(float)v[p[i][j]].y;
			mesh_ptr->e[thisEdge].z = (float)v[p[i][(j+1)%3]].z -
				(float)v[p[i][j]].z;
			thisEdge++;
		}
	}


	//------ Copy the vertex data ------------------------------

	for (i = 0; i < 8; i++) {
		mesh_ptr->v[i].x = (float)v[i].x;
		mesh_ptr->v[i].y = (float)v[i].y;
		mesh_ptr->v[i].z = (float)v[i].z;
	}

	//------ Create the polygon data ---------------------------

	thisEdge = 0;
	for (i = 0; i < numTris; i++) {
		//------ If this poly has more than three vertices -----
		//------ then we need to tesselate it into tri's -------

		poly_ptr = &mesh_ptr->p[i];

		poly_ptr->double_sided = false;

		poly_ptr->normal.x = (float)pn[i].x;
		poly_ptr->normal.y = (float)pn[i].y;
		poly_ptr->normal.z = (float)pn[i].z;


		for (j = 0; j < 3; j++) {
			poly_ptr->vIdx[j] = p[i][j];
			poly_ptr->edgeIdx[j] = thisEdge;
			thisEdge++;

			if (j == 0) {
				poly_ptr->min.x = (float)v[p[i][0]].x;
				poly_ptr->min.y = (float)v[p[i][0]].y;
				poly_ptr->min.z = (float)v[p[i][0]].z;

				poly_ptr->max.x = (float)v[p[i][0]].x;
				poly_ptr->max.y = (float)v[p[i][0]].y;
				poly_ptr->max.z = (float)v[p[i][0]].z;
			} else {
				if (v[p[i][j]].x < poly_ptr->min.x)
					poly_ptr->min.x = (float)v[p[i][j]].x;
				if (v[p[i][j]].x > poly_ptr->max.x)
					poly_ptr->max.x = (float)v[p[i][j]].x;

				if (v[p[i][j]].y < poly_ptr->min.y)
					poly_ptr->min.y = (float)v[p[i][j]].y;
				if (v[p[i][j]].y > poly_ptr->max.y)
					poly_ptr->max.y = (float)v[p[i][j]].y;

				if (v[p[i][j]].z < poly_ptr->min.z)
					poly_ptr->min.z = (float)v[p[i][j]].z;
				if (v[p[i][j]].z > poly_ptr->max.z)
					poly_ptr->max.z = (float)v[p[i][j]].z;
			}
		}


		//------ Check the mesh bounding box ---------------

		if (poly_ptr->min.x < meshMin.x)
			meshMin.x = poly_ptr->min.x;
		if (poly_ptr->min.y < meshMin.y)
			meshMin.y = poly_ptr->min.y;
		if (poly_ptr->min.z < meshMin.z)
			meshMin.z = poly_ptr->min.z;

		if (poly_ptr->max.x > meshMax.x)
			meshMax.x = poly_ptr->max.x;
		if (poly_ptr->max.y > meshMax.y)
			meshMax.y = poly_ptr->max.y;
		if (poly_ptr->max.z > meshMax.z)
			meshMax.z = poly_ptr->max.z;


		//------ Determine the major axis of the face ------
		//------ normal ------------------------------------
		
		if (FABS(poly_ptr->normal.x) == MAX3(FABS(poly_ptr->normal.x),
			FABS(poly_ptr->normal.y), FABS(poly_ptr->normal.z))) 
			poly_ptr->normalMajorAxis = COL_MAJOR_X;
		else if (FABS(poly_ptr->normal.y) == MAX3(FABS(poly_ptr->normal.x),
			FABS(poly_ptr->normal.y), FABS(poly_ptr->normal.z)))
			poly_ptr->normalMajorAxis = COL_MAJOR_Y;
		else if (FABS(poly_ptr->normal.z) == MAX3(FABS(poly_ptr->normal.x),
			FABS(poly_ptr->normal.y), FABS(poly_ptr->normal.z))) 
			poly_ptr->normalMajorAxis = COL_MAJOR_Z;
	}


	//------ Set the mesh bounding box -------------------------

	mesh_ptr->minBox.x = meshMin.x;
	mesh_ptr->minBox.y = meshMin.y;
	mesh_ptr->minBox.z = meshMin.z;

	mesh_ptr->maxBox.x = meshMax.x;
	mesh_ptr->maxBox.y = meshMax.y;
	mesh_ptr->maxBox.z = meshMax.z;
}