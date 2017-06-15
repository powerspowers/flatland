//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************


/*--------------------------------------------------------------

	Include files

--------------------------------------------------------------*/

#include <stdio.h>
#include <math.h>
#include "col.h"
#include "..\classes.h"
#include "..\main.h"
#include "..\parser.h"


/*--------------------------------------------------------------

	Private Variables

--------------------------------------------------------------*/

int			COL_polyCollidedWith[MAX_COLLISION_POLYS],
			COL_numCollisions;
COL_MESH	*COL_polysetCollidedWith[MAX_COLLISION_POLYS];
VEC3		*COL_collidedMeshOffs[MAX_COLLISION_POLYS],
			g_rayGoingDown = {0, -1, 0};


/*--------------------------------------------------------------

	Check if a Ray passes through a poly

--------------------------------------------------------------*/

bool	COL_isIntersecting2Way(VEC3 *rayOrigin_p, VEC3 *rayDir_p,
							   COL_MESH *colMesh_p, int polyNum)
{
	float		s, t,
				pz, py;
	bool		res;
	VEC3		*v0_p,
				*e0_p, *e1_p, *e2_p;
	COL_POLY3	*poly_p;
	

	//------ Point this at vertex 0 ----------------------------

	v0_p = &colMesh_p->v[colMesh_p->p[polyNum].vIdx[0]];


	//------ And this at the polygon in question ---------------

	poly_p = &colMesh_p->p[polyNum];


	//------ Perpendicular distance of start of the ray --------
	//------ to the plane --------------------------------------

	t = (v0_p->x-rayOrigin_p->x) * poly_p->normal.x + 
		(v0_p->y-rayOrigin_p->y) * poly_p->normal.y + 
		(v0_p->z-rayOrigin_p->z) * poly_p->normal.z;


	//------ Bin it if the start of the ray is behind the ------
	//------ plane ---------------------------------------------

	if (t > 0)
		return NON_INTERSECTING;


	s = VEC_dot(rayDir_p, &poly_p->normal);


	//------ Drop out if the ray travels away from the plane ----
	//------ normal, or the ray does not reach the plane --------

	if (t <= s || s == 0) 
		return NON_INTERSECTING;


	t /= s;


	//------ Get the edge pointers -----------------------------

	e0_p = &colMesh_p->e[poly_p->edgeIdx[0]];
	e1_p = &colMesh_p->e[poly_p->edgeIdx[1]];
	e2_p = &colMesh_p->e[poly_p->edgeIdx[2]];
		

	//------ And now, depending on which is the dominant -------
	//------ axis of the polygon, do a 2D containment test -----

	switch (poly_p->normalMajorAxis)
	{
	case COL_MAJOR_X:
		s = ((rayDir_p->y*t) + rayOrigin_p->y) - v0_p->y;
		pz = ((rayDir_p->z*t) + rayOrigin_p->z) - v0_p->z;

		return(CONTAINEDX3(e0_p, e1_p, e2_p, s, pz, res));
		break;

	case COL_MAJOR_Y:
		s = ((rayDir_p->x*t) + rayOrigin_p->x) - v0_p->x;
		pz = ((rayDir_p->z*t) + rayOrigin_p->z) - v0_p->z;
			
		return(CONTAINEDY3(e0_p, e1_p, e2_p, s, pz, res));
		break;

	case COL_MAJOR_Z:
		s = ((rayDir_p->x*t) + rayOrigin_p->x) - v0_p->x;
		py =((rayDir_p->y*t) + rayOrigin_p->y) - v0_p->y;
			
		return(CONTAINEDZ3(e0_p, e1_p, e2_p, s, py, res));
		break;
	}

	return(NON_INTERSECTING);
}


/*--------------------------------------------------------------

	Check if a ray passes through a poly, and if it does,
	compute the intersection co-ordinates (Point Of Intersection)

--------------------------------------------------------------*/

bool	
COL_isIntersectingPOI(VEC3 *rayOrigin_p, VEC3 *rayDir_p,
					  COL_MESH *colMesh_p, int polyNum, VEC3 *poi_p)
{
	float		s, t,
				pz, py;
	bool		res;
	VEC3		*v0_p,
				*e0_p, *e1_p, *e2_p;
	COL_POLY3	*poly_p;
	

	//------ This at the polygon is question -------------------

	poly_p = &colMesh_p->p[polyNum];


	//------ Check if the vector travels away from the poly ----
	//------ or if they are co-planar --------------------------

	s = VEC_dot(rayDir_p, &poly_p->normal);
	if (s >= 0)	
		return NON_INTERSECTING;


	//------ Point this at vertex 0 ----------------------------

	v0_p = &colMesh_p->v[colMesh_p->p[polyNum].vIdx[0]];


	//------ Perpendicular distance of start of the ray --------
	//------ to the plane --------------------------------------

	t = (v0_p->x-rayOrigin_p->x) * poly_p->normal.x + 
		(v0_p->y-rayOrigin_p->y) * poly_p->normal.y + 
		(v0_p->z-rayOrigin_p->z) * poly_p->normal.z;

	
	//------ Drop out if the ray travels away from the plane ----
	//------ normal, or the ray does not reach the plane --------

	if (t > 0 || t <= s) 
		return NON_INTERSECTING;


	t /= s;


	//------ Get the edge pointers -----------------------------

	e0_p = &colMesh_p->e[poly_p->edgeIdx[0]];
	e1_p = &colMesh_p->e[poly_p->edgeIdx[1]];
	e2_p = &colMesh_p->e[poly_p->edgeIdx[2]];
		

	//------ And now, depending on which is the dominant -------
	//------ axis of the polygon, do a 2D containment test -----

	switch (poly_p->normalMajorAxis)
	{
	case COL_MAJOR_X:
		poi_p->y = (rayDir_p->y*t) + rayOrigin_p->y;
		poi_p->z = (rayDir_p->z*t) + rayOrigin_p->z;

		s = poi_p->y - v0_p->y;
		pz = poi_p->z - v0_p->z;

		if (!(CONTAINEDX3(e0_p, e1_p, e2_p, s, pz, res)))
			return(NON_INTERSECTING);

		poi_p->x = (rayDir_p->x*t) + rayOrigin_p->x;
		return(INTERSECTING);
		break;

	case COL_MAJOR_Y:
		poi_p->x = (rayDir_p->x*t) + rayOrigin_p->x;
		poi_p->z = (rayDir_p->z*t) + rayOrigin_p->z;

		s = poi_p->x - v0_p->x;
		pz = poi_p->z - v0_p->z;
			
		if (!CONTAINEDY3(e0_p, e1_p, e2_p, s, pz, res))
			return(NON_INTERSECTING);

		poi_p->y = (rayDir_p->y*t) + rayOrigin_p->y;
		return(INTERSECTING);
		break;

	case COL_MAJOR_Z:
		poi_p->x = (rayDir_p->x*t) + rayOrigin_p->x;
		poi_p->y = (rayDir_p->y*t) + rayOrigin_p->y;

		s = poi_p->x - v0_p->x;
		py = poi_p->y - v0_p->y;
			
		if (!(CONTAINEDZ3(e0_p, e1_p, e2_p, s, py, res)))
			return(NON_INTERSECTING);

		poi_p->z = (rayDir_p->z*t) + rayOrigin_p->z;
		return(INTERSECTING);
		break;
	}

	return(NON_INTERSECTING);
}


/*--------------------------------------------------------------

	Check if a ray passes through a poly, and if it does,
	compute the intersection co-ordinates (Point Of Intersection)

	This version does not do the dot product - that is passed
	into the function instead.

--------------------------------------------------------------*/

bool	COL_isIntersectingPOI(VEC3 *rayOrigin_p, VEC3 *rayDir_p,
							  COL_MESH *colMesh_p, int polyNum, VEC3 *poi_p,
							  float s)
{
	float		t,
				pz, py;
	bool		res;
	VEC3		*v0_p,
				*e0_p, *e1_p, *e2_p;
	COL_POLY3	*poly_p;
	

	//------ This at the polygon in question -------------------

	poly_p = &colMesh_p->p[polyNum];


	//------ Point this at vertex 0 ----------------------------

	v0_p = &colMesh_p->v[colMesh_p->p[polyNum].vIdx[0]];


	//------ Perpendicular distance of start of the ray --------
	//------ to the plane --------------------------------------

	t = (v0_p->x-rayOrigin_p->x) * poly_p->normal.x + 
		(v0_p->y-rayOrigin_p->y) * poly_p->normal.y + 
		(v0_p->z-rayOrigin_p->z) * poly_p->normal.z;

	
	//------ Drop out if the ray travels away from the plane ----
	//------ normal, or the ray does not reach the plane --------

	if (t > 0 || t <= s || s == 0) 
		return NON_INTERSECTING;


	t /= s;


	//------ Get the edge pointers -----------------------------

	e0_p = &colMesh_p->e[poly_p->edgeIdx[0]];
	e1_p = &colMesh_p->e[poly_p->edgeIdx[1]];
	e2_p = &colMesh_p->e[poly_p->edgeIdx[2]];
		

	//------ And now, depending on which is the dominant -------
	//------ axis of the polygon, do a 2D containment test -----

	switch (poly_p->normalMajorAxis)
	{
	case COL_MAJOR_X:
		poi_p->y = (rayDir_p->y*t) + rayOrigin_p->y;
		poi_p->z = (rayDir_p->z*t) + rayOrigin_p->z;

		s = poi_p->y - v0_p->y;
		pz = poi_p->z - v0_p->z;

		if (!(CONTAINEDX3(e0_p, e1_p, e2_p, s, pz, res)))
			return(NON_INTERSECTING);

		poi_p->x = (rayDir_p->x*t) + rayOrigin_p->x;
		return(INTERSECTING);
		break;

	case COL_MAJOR_Y:
		poi_p->x = (rayDir_p->x*t) + rayOrigin_p->x;
		poi_p->z = (rayDir_p->z*t) + rayOrigin_p->z;

		s = poi_p->x - v0_p->x;
		pz = poi_p->z - v0_p->z;
			
		if (!CONTAINEDY3(e0_p, e1_p, e2_p, s, pz, res))
			return(NON_INTERSECTING);

		poi_p->y = (rayDir_p->y*t) + rayOrigin_p->y;
		return(INTERSECTING);
		break;

	case COL_MAJOR_Z:
		poi_p->x = (rayDir_p->x*t) + rayOrigin_p->x;
		poi_p->y = (rayDir_p->y*t) + rayOrigin_p->y;

		s = poi_p->x - v0_p->x;
		py = poi_p->y - v0_p->y;
			
		if (!(CONTAINEDZ3(e0_p, e1_p, e2_p, s, py, res)))
			return(NON_INTERSECTING);

		poi_p->z = (rayDir_p->z*t) + rayOrigin_p->z;
		return(INTERSECTING);
		break;
	}

	return(NON_INTERSECTING);
}


/*--------------------------------------------------------------

	See if an Axis-Aligned box is intersecting a polygon

--------------------------------------------------------------*/

bool
COL_isIntersecting(COL_MESH *colMesh_p, VEC3 *meshOffs_p, int polyNum,
				   COL_AABOX *aaBox_p, VEC3 *boxCentre_p)
{
	int			vC0(0), vC1(0), vC2(0),
				c;
	VEC3		dir,
				max, min,
				boxPos,
				*v0, *v1, *v2;
	COL_POLY3	*poly_p;


	//------ Point this at the poly in question ----------------

	poly_p = &colMesh_p->p[polyNum];


	//------ Check if the poly is nowhere near the box! --------

	if ((max.x = (boxPos.x = boxCentre_p->x - meshOffs_p->x) + 
		 aaBox_p->maxDim.x) < poly_p->min.x || 
		(min.x = boxPos.x - aaBox_p->maxDim.x) > poly_p->max.x ||
		(max.y = (boxPos.y = boxCentre_p->y - meshOffs_p->y) +
		 aaBox_p->maxDim.y) < poly_p->min.y ||
		(min.y = boxPos.y - aaBox_p->maxDim.y) > poly_p->max.y ||
		(max.z = (boxPos.z = boxCentre_p->z - meshOffs_p->z) + 
		 aaBox_p->maxDim.z) < poly_p->min.z ||
		(min.z = boxPos.z - aaBox_p->maxDim.z) > poly_p->max.z)							 
		return NON_INTERSECTING;		


	//------ Calculate the clip codes for each vertex ----------
	//------ See if vertex 0 is inside the box -----------------

 	v0 = &colMesh_p->v[poly_p->vIdx[0]];

	if (v0->x < min.x)
		vC0 = XNEG;
	else 
		if (v0->x > max.x)
			vC0 = XPOS;	

	if (v0->y < min.y)
		vC0 |= YNEG;
	else 
		if (v0->y > max.y)
			vC0 |= YPOS;

	if (v0->z < min.z)
		vC0 |= ZNEG;
	else 
		if (v0->z > max.z)
			vC0 |= ZPOS;	
		else 
			if (!vC0)
				return INTERSECTING; // Reject if point inside


	//------ Now do vertex 1 -----------------------------------

	v1 = &colMesh_p->v[poly_p->vIdx[1]];

	if (v1->x < min.x)
		vC1 = XNEG;
	else 
		if (v1->x > max.x)
			vC1 = XPOS;

	if (v1->y < min.y)
		vC1 |= YNEG;
	else 
		if (v1->y > max.y)
			vC1 |= YPOS;

	if (v1->z < min.z)		
		vC1 |= ZNEG;
	else 
		if (v1->z > max.z)
			vC1 |= ZPOS;	
		else 
			if (!vC1)
				return INTERSECTING; // Reject if point inside


	//------ And finally vertex 2 ------------------------------

	v2 = &colMesh_p->v[poly_p->vIdx[2]];

	if (v2->x < min.x)
		vC2 = XNEG;
	else 
		if (v2->x > max.x)
			vC2 = XPOS;

	if (v2->y < min.y)
		vC2 |= YNEG;
	else 
		if (v2->y > max.y)
			vC2 |= YPOS;

	if (v2->z < min.z)
		vC2 |= ZNEG;
	else 
		if (v2->z > max.z)
			vC2 |= ZPOS;	
		else 
			if (!vC2)
				return INTERSECTING; // Reject if point inside


	//------ Check the vertex codes and if all edges lie -------
	//------ outside of the same side of the box, then ---------
	//------ reject the poly -----------------------------------

	c = vC0 & vC1;
	if (c & vC2) 
		return NON_INTERSECTING;


	//------ Now see if a poly edge intersects the box ---------
	//------ any of the vertices being inside the box ----------

	if (c == 0)
	{
		c = vC0 | vC1;
		EDGE_TEST(c, min, max, boxpos, v0, v1);
	}

	if ((vC1 & vC2) == 0)
	{
		c = vC1 | vC2;
		EDGE_TEST(c, min, max, boxpos, v1, v2);
	}

	if ((vC2 & vC0) == 0)
	{
		c = vC2 | vC0;
		EDGE_TEST(c, min, max, boxpos, v2, v0);
	}	


	//------ Test corners --------------------------------------
	// Corner 1

 	VEC_sub(&max, &min, &dir);	

	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;

	swap(min.y, max.y, float);
	dir.y = -dir.y;
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;
	swap(min.y, max.y, float);
	dir.y = -dir.y;


	// Corner 2 - Flip X
	dir.x = -dir.x;
	swap(min.x, max.x, float);
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;

	swap(min.y, max.y, float);
	dir.y = -dir.y;
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;
	swap(min.y, max.y, float);
	dir.y = -dir.y;


	// Corner 3 - Flip Z
	dir.z = -dir.z;
	min.z = max.z;
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;

	swap(min.y, max.y, float);
	dir.y = -dir.y;
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;
	swap(min.y, max.y, float);
	dir.y = -dir.y;


	// Corner 4 - Flip X back
	dir.x = -dir.x;
	min.x = max.x;
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;

	swap(min.y, max.y, float);
	dir.y = -dir.y;
	if (COL_isIntersecting2Way(&min, &dir, colMesh_p, polyNum)) 
		return INTERSECTING;

	return NON_INTERSECTING;
}


/*--------------------------------------------------------------

	Check is a ray intersects an Axis Aligned box, and
	calculate it's Point Of Intersection

--------------------------------------------------------------*/

int		COL_rayAgainstAABoxPOI(COL_RAY *ray_p, COL_AABOX *box_p, VEC3 *centre_p,
							   VEC3 *poi_p)
{
	int		candidatePlanes;


	//------ Find which planes need to be checked against ------

	candidatePlanes = VEC3_SIGNS(ray_p->dir.x, ray_p->dir.y, ray_p->dir.z);


	//------ And now check the relevant planes -----------------

	switch(candidatePlanes)
	{
	case CP_NNN:
		COL_AABOX_TEST_NEG_X_PLANE(); 
		COL_AABOX_TEST_NEG_Y_PLANE();
		COL_AABOX_TEST_NEG_Z_PLANE(); 
		return NON_INTERSECTING; 
		break;

	case CP_NNP:
		COL_AABOX_TEST_NEG_X_PLANE();
		COL_AABOX_TEST_NEG_Y_PLANE();
		COL_AABOX_TEST_POS_Z_PLANE();
		return NON_INTERSECTING; 
		break;

	case CP_NPN:
		COL_AABOX_TEST_NEG_X_PLANE();
		COL_AABOX_TEST_POS_Y_PLANE();
		COL_AABOX_TEST_NEG_Z_PLANE();
		return NON_INTERSECTING; 
		break;

	case CP_NPP:
		COL_AABOX_TEST_NEG_X_PLANE();
		COL_AABOX_TEST_POS_Y_PLANE();
		COL_AABOX_TEST_POS_Z_PLANE();
		return NON_INTERSECTING;
		break;

	case CP_PNN:
		COL_AABOX_TEST_POS_X_PLANE();
		COL_AABOX_TEST_NEG_Y_PLANE();
		COL_AABOX_TEST_NEG_Z_PLANE();
		return NON_INTERSECTING;
		break;

	case CP_PNP:
		COL_AABOX_TEST_POS_X_PLANE();
		COL_AABOX_TEST_NEG_Y_PLANE();
		COL_AABOX_TEST_POS_Z_PLANE();
		return NON_INTERSECTING;
		break;

	case CP_PPN:
		COL_AABOX_TEST_POS_X_PLANE();
		COL_AABOX_TEST_POS_Y_PLANE();
		COL_AABOX_TEST_NEG_Z_PLANE();
		return NON_INTERSECTING;
		break;

	case CP_PPP:
		COL_AABOX_TEST_POS_X_PLANE();
		COL_AABOX_TEST_POS_Y_PLANE();
		COL_AABOX_TEST_POS_Z_PLANE();
		return NON_INTERSECTING; 
		break;
	}

	return NON_INTERSECTING;
}


/*--------------------------------------------------------------

	Check an AA box against a mesh's bounding box

--------------------------------------------------------------*/

bool	COL_checkAABoxAgainstMesh(COL_MESH *colMesh_p, VEC3 *meshOffs_p,
								  COL_AABOX *aaBox_p, VEC3 *boxCentre_p)
{
	if ( ((boxCentre_p->x-aaBox_p->maxDim.x) > 
		  (colMesh_p->maxBox.x+meshOffs_p->x)) ||
		 ((boxCentre_p->x+aaBox_p->maxDim.x) < 
		  (colMesh_p->minBox.x+meshOffs_p->x)) ||
		 ((boxCentre_p->y-aaBox_p->maxDim.y) > 
		  (colMesh_p->maxBox.y+meshOffs_p->y)) ||
		 ((boxCentre_p->y+aaBox_p->maxDim.y) < 
		  (colMesh_p->minBox.y+meshOffs_p->y)) ||
		 ((boxCentre_p->z-aaBox_p->maxDim.z) >
		  (colMesh_p->maxBox.z+meshOffs_p->z)) ||
		 ((boxCentre_p->z+aaBox_p->maxDim.z) <
		  (colMesh_p->minBox.z+meshOffs_p->z)) )
		 return NON_INTERSECTING;

	return INTERSECTING;
}


/*--------------------------------------------------------------

	Check whether a Axis Aligned bounding box has collided 
	with a shape structure. Return TRUE if it has

--------------------------------------------------------------*/

bool	COL_checkMeshCollision(COL_MESH *colMesh_p, VEC3 *meshOffs_p,
								float x, float y, float z,
								float oldX, float oldY, float oldZ,
								COL_AABOX *aaBox_p, VEC3 *boxCentre_p)
{
	bool	collided;
	int		i;
	VEC3	movementVec;

   	movementVec.x = x - oldX;
	movementVec.y = y - oldY;
	movementVec.z = z - oldZ;
	VEC_normalise(&movementVec);

	collided = false;
	for (i=0; i<colMesh_p->numPolys; i++)
	{
		//------ Check the movement vector against the ---------
		//------ normal of the poly...if it's single-sided and -
		//------ facing away from us, ignore it ----------------

		if (colMesh_p->p[i].double_sided ||
			VEC_dot(&colMesh_p->p[i].normal, &movementVec) < 0)
		{
			if (COL_isIntersecting(colMesh_p, meshOffs_p, i, aaBox_p,
				boxCentre_p))
			{
				collided = true;
				if (COL_numCollisions != MAX_COLLISION_POLYS) {
					COL_polyCollidedWith[COL_numCollisions] = i;
					COL_polysetCollidedWith[COL_numCollisions] = colMesh_p;
					COL_collidedMeshOffs[COL_numCollisions] = meshOffs_p;
					COL_numCollisions++;
				}
			}
		}
	}
	return(collided);
}


/*--------------------------------------------------------------

	Check whether a Axis Aligned bounding box has collided 
	with the polys that have already been collided with.
	Return TRUE if it has.

--------------------------------------------------------------*/

bool	
COL_checkCollidedPolys(float x, float y, float z,
					   float oldX, float oldY, float oldZ,
					   COL_AABOX *aaBox_p, VEC3 *boxCentre_p)
{
	int		i;
	VEC3	movementVec;


   	movementVec.x = x - oldX;
	movementVec.y = y - oldY;
	movementVec.z = z - oldZ;
	VEC_normalise(&movementVec);

	for (i=0; i<COL_numCollisions; i++)
	{
		//------ Check the movement vector against the ---------
		//------ normal of the poly... if it's facing ----------
		//------ away from us, ignore it -----------------------

		if (COL_isIntersecting(COL_polysetCollidedWith[i], 
			COL_collidedMeshOffs[i], COL_polyCollidedWith[i], aaBox_p, 
			boxCentre_p))
		{
			return(true);
		}
	}

	return(false);
}


/*--------------------------------------------------------------

	Return the sum of all normals of the polys that have
	been collided with.

	The Y component of the normal is zeroed out because
	we are only interested in a 2D reaction - the up and down
	movement comes from gravity checks.

--------------------------------------------------------------*/

void	COL_getReactionNormal(VEC3 *res_p)
{
	int		i;
	float	oneOverNumCol;


	VEC_set(res_p, 0, 0, 0);

	if (COL_numCollisions == 0)
		return;


	for (i=0; i<COL_numCollisions; i++)
	{
		VEC_add(&COL_polysetCollidedWith[i]->p[COL_polyCollidedWith[i]].normal,
			res_p, res_p);
	}

	oneOverNumCol = (float)1/COL_numCollisions;
	res_p->x *= oneOverNumCol;
	res_p->y = 0.0;
	res_p->z *= oneOverNumCol;

	VEC_normalise(res_p);
}


/*--------------------------------------------------------------

	Return the normal of one of the collided polys

	The Y component of the normal is zeroed out because
	we are only interested in a 2D reaction - the up and down
	movement comes from gravity checks.

--------------------------------------------------------------*/

void	COL_getCollidedNormal(int collidedNum, VEC3 *res_p)
{
	int			i;
	COL_MESH	*colMesh_p;

	colMesh_p = COL_polysetCollidedWith[collidedNum];
	i = COL_polyCollidedWith[collidedNum];

	res_p->x = colMesh_p->p[i].normal.x;
	res_p->y = 0.0;
	res_p->z = colMesh_p->p[i].normal.z;

	VEC_normalise(res_p);
}	


/*--------------------------------------------------------------

	Check a ray going straight down to the bottom of the 
	world (y co-ordinate of 0) against a collision polygon

--------------------------------------------------------------*/

bool
COL_rayAgainstPoly(VEC3 *startRay_p, COL_MESH *colMesh_p,
				   VEC3 *pos_p, int polyNum, VEC3 *poi_p)
{
	COL_POLY3	*poly_p;
	VEC3		newRay;


	poly_p = &colMesh_p->p[polyNum];

	if ((startRay_p->x < (poly_p->min.x + pos_p->x)) || 
		(startRay_p->x > (poly_p->max.x + pos_p->x)) ||
		(startRay_p->z < (poly_p->min.z + pos_p->z)) || 
		(startRay_p->z > (poly_p->max.z + pos_p->z)) ||
		(startRay_p->y < (poly_p->min.y + pos_p->y)))
		return(NON_INTERSECTING);

  	g_rayGoingDown.y = -(startRay_p->y+0.5f);

	newRay.x = startRay_p->x - pos_p->x;
	newRay.y = startRay_p->y - pos_p->y;
	newRay.z = startRay_p->z - pos_p->z;

	return(COL_isIntersectingPOI(&newRay, &g_rayGoingDown, colMesh_p, polyNum,
		poi_p)); 
}


/*--------------------------------------------------------------

	Check a polygon against an AA box, and return the point
	of intersection.

--------------------------------------------------------------*/

float		COL_aaboxAndRaysAgainstPoly(VEC3 *rays_p, COL_MESH *colMesh_p,
									 VEC3 *pos_p, int polyNum, VEC3 *poi_p,
									 COL_AABOX *aaBox_p, VEC3 *centre_p,
									 float y, float maxStepHeight)
{
	int			i, j;
	float		s, nearestY, stepHeight,
				boxMaxX, boxMaxY, boxMaxZ,
				boxMinX, boxMinZ;
	COL_POLY3	*poly_p;
	VEC3		newRay, *startRay_p;
	COL_RAY		ray;


	//------ Get the pointer to the poly -----------------------

	poly_p = &colMesh_p->p[polyNum];


	nearestY = -1;


	//------ Check if the vector travels away from the poly ----
	//------ or if they are co-planar. -------------------------

  	g_rayGoingDown.y = -(rays_p[0].y+0.5f);
	s = VEC_dot(&g_rayGoingDown, &poly_p->normal);
	if (!poly_p->double_sided && s >= 0)	
		return(-1);


	//------ See if the AA box is anywhere near the poly -------

	if (((boxMaxX = centre_p->x+aaBox_p->maxDim.x) < (poly_p->min.x + pos_p->x)) || 
		((boxMinX = centre_p->x-aaBox_p->maxDim.x) > (poly_p->max.x + pos_p->x)) || 
		((boxMaxZ = centre_p->z+aaBox_p->maxDim.z) < (poly_p->min.z + pos_p->z)) || 
		((boxMinZ = centre_p->z-aaBox_p->maxDim.z) > (poly_p->max.z + pos_p->z)) || 
		((boxMaxY = centre_p->y+aaBox_p->maxDim.y) < (poly_p->min.y + pos_p->y)))
		return(-1);
		

	//------ Now check the rays --------------------------------

  	for (i=0; i<4; i++)
	{
		startRay_p = (VEC3 *)&rays_p[i];


		if ((startRay_p->x < (poly_p->min.x + pos_p->x)) || 
			(startRay_p->x > (poly_p->max.x + pos_p->x)) ||
			(startRay_p->z < (poly_p->min.z + pos_p->z)) || 
			(startRay_p->z > (poly_p->max.z + pos_p->z)) ||
			(startRay_p->y < (poly_p->min.y + pos_p->y)))
		{
		}
		else
		{
			newRay.x = startRay_p->x - pos_p->x;
			newRay.y = startRay_p->y - pos_p->y;
			newRay.z = startRay_p->z - pos_p->z;

			if (COL_isIntersectingPOI(&newRay, &g_rayGoingDown, colMesh_p,
				polyNum, poi_p, s))
			{
				poi_p->y += pos_p->y;

				stepHeight = poi_p->y - y;
				if (stepHeight < maxStepHeight && poi_p->y > nearestY)
					nearestY = poi_p->y;
			}
		}
	}


	//------ Now see if we're colliding at a point the rays ----
	//------ didn't detect -------------------------------------

	i = 0;
	while (i<3)
	{
	   	ray.origin.x = colMesh_p->v[poly_p->vIdx[i]].x + pos_p->x;
		ray.origin.y = colMesh_p->v[poly_p->vIdx[i]].y + pos_p->y;
		ray.origin.z = colMesh_p->v[poly_p->vIdx[i]].z + pos_p->z;

		j = i+1;
		if (j > 2)
			j = 0;

		//------ If the origin of the ray is inside the box, ---
		//------ then we need to swap the ends to make sure ----
		//------ the test will work properly -------------------

		if ((boxMaxX > ray.origin.x) && 
			(boxMinX < ray.origin.x) && 
			(boxMaxZ > ray.origin.z) && 
			(boxMinZ < ray.origin.z) && 
			(boxMaxY > ray.origin.y))
		{
			ray.end.x = ray.origin.x;
			ray.end.y = ray.origin.y;
			ray.end.z = ray.origin.z;

			ray.origin.x = colMesh_p->v[poly_p->vIdx[j]].x + pos_p->x;
			ray.origin.y = colMesh_p->v[poly_p->vIdx[j]].y + pos_p->y;
			ray.origin.z = colMesh_p->v[poly_p->vIdx[j]].z + pos_p->z;
		}
		else
		{
			ray.end.x = colMesh_p->v[poly_p->vIdx[j]].x + pos_p->x;
			ray.end.y = colMesh_p->v[poly_p->vIdx[j]].y + pos_p->y;
			ray.end.z = colMesh_p->v[poly_p->vIdx[j]].z + pos_p->z;
		}


		VEC_sub(&ray.end, &ray.origin, &ray.dir);

		ray.invDir.x = 1.0f/ray.dir.x;
		ray.invDir.y = 1.0f/ray.dir.y;
		ray.invDir.z = 1.0f/ray.dir.z;


		if(COL_rayAgainstAABoxPOI(&ray, aaBox_p, centre_p, poi_p))
		{
			stepHeight = poi_p->y - y;
			if (stepHeight < maxStepHeight && poi_p->y > nearestY)
				nearestY = poi_p->y;
		}

		i++;
	}

	return(nearestY);
}


/*--------------------------------------------------------------

	Find the highest shadow point beneath the top of the 
	Axis Aligned box that isn't higher than the maximum
	step height.

--------------------------------------------------------------*/

float	COL_getShadowHeight(COL_MESH **colMeshes_pp, VEC3 *pos_p, int numMeshes,
							float x, float y, float z,                       
							COL_AABOX *aaBox_p, float maxStepHeight)
{
	int			i, j;
	VEC3		ray[4],
				poi,
				centreDownBox;
	float		nearestY, highestY;
	COL_MESH	*colMesh_p;
	COL_AABOX	downBox;

	//------ southwest top corner -----------------------------------

	ray[0].x = x - aaBox_p->maxDim.x;
	ray[0].y = y + aaBox_p->maxDim.y;
	ray[0].z = z - aaBox_p->maxDim.z;

	//------ southeast top corner ----------------------------------

	ray[1].x = x + aaBox_p->maxDim.x;
	ray[1].y = ray[0].y;
	ray[1].z = ray[0].z;

	//------ northwest top corner --------------------------------

	ray[2].x = x - aaBox_p->maxDim.x;
	ray[2].y = ray[0].y;
	ray[2].z = z + aaBox_p->maxDim.z;

	//------ northeast top corner ----------------------------------

	ray[3].x = x + aaBox_p->maxDim.x;
	ray[3].y = ray[0].y;
	ray[3].z = ray[2].z;


	VEC_add(&ray[0], &aaBox_p->offsToCentre, &ray[0]);
	VEC_add(&ray[1], &aaBox_p->offsToCentre, &ray[1]);
	VEC_add(&ray[2], &aaBox_p->offsToCentre, &ray[2]);
	VEC_add(&ray[3], &aaBox_p->offsToCentre, &ray[3]);

	// Create an AA box that extends one whole block below the player's
	// collision box, so that we can find a shadow height that is no more
	// than one block below the player.

	downBox.maxDim.x = aaBox_p->maxDim.x;
	downBox.maxDim.y = aaBox_p->maxDim.y + UNITS_PER_HALF_BLOCK;
	downBox.maxDim.z = aaBox_p->maxDim.z;

	downBox.offsToCentre.x = aaBox_p->offsToCentre.x;
	downBox.offsToCentre.y = -(UNITS_PER_BLOCK - downBox.maxDim.y); 
	downBox.offsToCentre.z = aaBox_p->offsToCentre.z;

	VEC_set(&centreDownBox, x, y, z);
	VEC_add(&centreDownBox, &downBox.offsToCentre, &centreDownBox);

	// Make the default shadow height be one block below the player's
	// collision box.

	nearestY = y - UNITS_PER_BLOCK;

 	for (j=0; j<numMeshes; j++)
	{
		colMesh_p = colMeshes_pp[j];

		if (COL_checkAABoxAgainstMesh(colMesh_p, &pos_p[j], &downBox,
			&centreDownBox))
		{
			for (i=0; i<colMesh_p->numPolys; i++)
			{
				highestY = COL_aaboxAndRaysAgainstPoly((VEC3 *)&ray, colMesh_p,
					&pos_p[j], i, &poi, &downBox, &centreDownBox, 
					y, maxStepHeight);

				if (highestY > nearestY)
					nearestY = highestY;
			}
		}
	}

	return(nearestY + COL_SHADOW_TOLERANCE);
}


/*--------------------------------------------------------------

	Check collision against multiple collision meshes,
	calculate a reaction, and return the new position

--------------------------------------------------------------*/

void	COL_checkCollisions(COL_MESH **colMeshes_pp, VEC3 *pos_p, int numMeshes,
							float *x, float *y, float *z,
							float oldX, float oldY, float oldZ,
							float *shadowHeight,
							float maxStepHeight,
							COL_AABOX *aaBox_p)
{
	int			i, 
				colliding, collided,
				noMoreChecks;
	VEC3		centreOfBox,
				reactionVec, movementVec;
	float		newX, newY, newZ,
				shadowY,
				len, mod;

	// Use a negative value for the default shadow height, so we can detect
	// when it wasn't set.
 
	*shadowHeight = -1.0;

	//------ Reset the count that 'checkMeshCollision' ---------
	//------ updates -------------------------------------------

	COL_numCollisions = 0;


	//------ Find the shadow height of the new position --------

	shadowY = COL_getShadowHeight(colMeshes_pp, pos_p, numMeshes, *x, *y, *z,
		aaBox_p, maxStepHeight);

	// If the shadow height is above the current player position, step up.
	// XXX -- Should we do both step up or fall down here, so we can fall
	// through stuff?

	if (FGT(shadowY, *y))
		*y = shadowY;

	//------ Set the centre of the axis-aligned box ------------

	VEC_set(&centreOfBox, *x, *y, *z);
	VEC_add(&centreOfBox, &aaBox_p->offsToCentre, &centreOfBox);

	//------ Loop through every collision mesh to check --------

	for (i=0; i<numMeshes; i++)
	{
		if (COL_checkAABoxAgainstMesh(colMeshes_pp[i], &pos_p[i], aaBox_p,
			&centreOfBox))
			COL_checkMeshCollision(colMeshes_pp[i], &pos_p[i], *x, *y, *z,
				oldX, oldY, oldZ, aaBox_p, &centreOfBox);
	}


	//------ If there were some collisions ---------------------

	if (COL_numCollisions > 0)
	{ 
		//------ We have to put the Y co-ordinate back to the --
		//------ previous position because the new Y position --
		//------ obviously hasn't worked! ----------------------

		*y = oldY;

		noMoreChecks = 0;

		movementVec.x = *x - oldX;
		movementVec.y = *y - oldY;
		movementVec.z = *z - oldZ;
		len = VEC_length(&movementVec);
		VEC_normalise(&movementVec);


		//------ If there's only one collision, then slide -----
		//------ against that normal ---------------------------
						   
		if (len > 0)					// Double check for safety reasons!
		{
			if (COL_numCollisions == 1)
			{
				COL_getCollidedNormal(0, &reactionVec);
				if ((fabs(reactionVec.x) > 0.001f) || 
					(fabs(reactionVec.z) > 0.001f))
				{
					VEC_normalise(&reactionVec);
					len *= -VEC_dot(&reactionVec, &movementVec);

					newX = *x + reactionVec.x * len;
					newY = *y + reactionVec.y * len;
					newZ = *z + reactionVec.z * len;
				} 
				else 
				{
					newX = oldX;
					newY = oldY;
					newZ = oldZ;
				}
			}
			else
			{
				//------ Try each collided poly normal as a --------
				//------ reaction, and see if that works -----------

				i = 0;
				colliding = 1;
				while ((i < COL_numCollisions) && (colliding))
				{
					COL_getCollidedNormal(i, &reactionVec);

					if ((fabs(reactionVec.x) > 0.001f) || 
						(fabs(reactionVec.z) > 0.001f))
					{
						VEC_normalise(&reactionVec);
						mod = len * -VEC_dot(&reactionVec, &movementVec);

						newX = *x + reactionVec.x * mod;
						newY = *y + reactionVec.y * mod;
						newZ = *z + reactionVec.z * mod;

						VEC_set(&centreOfBox, newX, newY, newZ);
						VEC_add(&centreOfBox, &aaBox_p->offsToCentre, 
							&centreOfBox);

						colliding = COL_checkCollidedPolys(newX, newY, newZ,
							oldX, oldY, oldZ, aaBox_p, &centreOfBox);
					} 
					else 
					{
						newX = oldX;
						newY = oldY;
						newZ = oldZ;
					}
				
					i++;
				}


				if (colliding)
				{
					//------ Nope, so now try moving along the -----
					//------ combined normals ----------------------

					COL_getReactionNormal(&reactionVec);
					mod = VEC_length(&reactionVec);

					if (mod > 0)
					{
						VEC_normalise(&reactionVec);
						len *= -VEC_dot(&reactionVec, &movementVec);

						newX = *x + reactionVec.x * len;
						newY = *y + reactionVec.y * len;
						newZ = *z + reactionVec.z * len;

						VEC_set(&centreOfBox, newX, newY, newZ);
						VEC_add(&centreOfBox, &aaBox_p->offsToCentre, 
							&centreOfBox);

						// See if we're out of collision. If not, give up.

						if (COL_checkCollidedPolys(newX, newY, newZ,
							oldX, oldY, oldZ, aaBox_p, &centreOfBox) ) 
						{
							newX = oldX;
							newY = oldY;
							newZ = oldZ;

							noMoreChecks = 1;
						}
					} 
					else 
					{
						newX = oldX;
						newY = oldY;
						newZ = oldZ;

					 	noMoreChecks = 1;
					}
				}
			}
		} 
		else
		{
			newX = oldX;
			newY = oldY;
			newZ = oldZ;

	   	 	noMoreChecks = 1;
		}

		*x = newX;
		*y = newY;
		*z = newZ;


		//------ Sanity check ----------------------------------

		if (!noMoreChecks)
		{
			i = 0;
			collided = 0;
			COL_numCollisions = 0;

			//------ Find the shadow height of the new ---------
			//------ position ----------------------------------

			shadowY = COL_getShadowHeight(colMeshes_pp, pos_p, numMeshes, 
				*x, *y, *z, aaBox_p, maxStepHeight);

			// If the shadow height is above the current player position, step
			// up.

			if (FGT(shadowY, *y))
				*y = shadowY;

			VEC_set(&centreOfBox, *x, *y, *z);
			VEC_add(&centreOfBox, &aaBox_p->offsToCentre, &centreOfBox);

			while ((i < numMeshes) && (!collided))
			{
				if (COL_checkAABoxAgainstMesh(colMeshes_pp[i], &pos_p[i],
					aaBox_p, &centreOfBox))
					collided = COL_checkMeshCollision(colMeshes_pp[i],
						&pos_p[i], *x, *y, *z, oldX, oldY, oldZ, aaBox_p,
						&centreOfBox);
				i++;
			}

			if (collided)
			{
				*x = oldX;
				*y = oldY;
				*z = oldZ;
			}
			else
			{
				*shadowHeight = shadowY;
			}
		}
	}
	else
	{
		//------ No collision, so the first calculated ---------
		//------ shadow position is fine -----------------------

		*shadowHeight = shadowY;
	}
}


/*--------------------------------------------------------------

	Initialise the collision

--------------------------------------------------------------*/

void	COL_init(void)
{
	MATHS_init();
}


/*--------------------------------------------------------------

	Close down the collision system

--------------------------------------------------------------*/

void	COL_exit(void)
{
	MATHS_exit();
}