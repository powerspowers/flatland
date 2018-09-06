//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

#ifndef _COL_H
#define _COL_H

/*--------------------------------------------------------------

	Includes

--------------------------------------------------------------*/

#include "vec.h"
#include "maths.h"


/*--------------------------------------------------------------

	Defines

--------------------------------------------------------------*/

#define	MAX_COLLISION_POLYS			64

#define NON_INTERSECTING			false
#define INTERSECTING				true

#define XPOS (1<<0)
#define XNEG (1<<1)
#define YPOS (1<<2)
#define YNEG (1<<3)
#define ZPOS (1<<4)
#define ZNEG (1<<5)

#define	COL_MAJOR_X					0
#define	COL_MAJOR_Y					1
#define	COL_MAJOR_Z					2

#define CAND_PLANE_X (1<<2)
#define CAND_PLANE_Y (1<<1)
#define CAND_PLANE_Z (1<<0)

#define CP_PPP						(CAND_PLANE_X+CAND_PLANE_Y+CAND_PLANE_Z)
#define CP_PPN						(CAND_PLANE_X+CAND_PLANE_Y)
#define CP_PNP						(CAND_PLANE_X+CAND_PLANE_Z)
#define CP_PNN						(CAND_PLANE_X)
#define CP_NPP						(CAND_PLANE_Y+CAND_PLANE_Z)
#define CP_NPN						(CAND_PLANE_Y)
#define CP_NNP						(CAND_PLANE_Z)
#define CP_NNN						(0)

#define	COL_SHADOW_TOLERANCE		0.1f


/*--------------------------------------------------------------

	Macros

--------------------------------------------------------------*/

#define	LERP(r,a,b)					((r*(b-a))+a)


	//------ The edge test -------------------------------------

#define EDGE_TEST(c, min, max, boxpos, v0, v1)																		\
																												\
	float	r;																									\
																												\
	if (c & XNEG &&																								\
		FABS(boxPos.y - LERP((r = (min.x-v0->x) / (v1->x-v0->x)),v0->y,v1->y)) <= aaBox_p->maxDim.y &&			\
		FABS(boxPos.z - LERP(r,v0->z,v1->z)) <= aaBox_p->maxDim.z) return INTERSECTING;							\
	if (c & XPOS &&																								\
		FABS(boxPos.y - LERP((r = (max.x-v0->x) / (v1->x-v0->x)),v0->y,v1->y)) <= aaBox_p->maxDim.y &&			\
		FABS(boxPos.z - LERP(r,v0->z,v1->z)) <= aaBox_p->maxDim.z) return INTERSECTING;							\
	if (c & YNEG &&																								\
		FABS(boxPos.x - LERP((r = (min.y-v0->y) / (v1->y-v0->y)),v0->x,v1->x)) <= aaBox_p->maxDim.x &&			\
		FABS(boxPos.z - LERP(r,v0->z,v1->z)) <= aaBox_p->maxDim.z) return INTERSECTING;							\
	if (c & YPOS &&																								\
		FABS(boxPos.x - LERP((r = (max.y-v0->y) / (v1->y-v0->y)),v0->x,v1->x)) <= aaBox_p->maxDim.x &&			\
		FABS(boxPos.z - LERP(r,v0->z,v1->z)) <= aaBox_p->maxDim.z) return INTERSECTING;							\
	if (c & ZNEG &&																								\
		FABS(boxPos.x - LERP((r = (min.z-v0->z) / (v1->z-v0->z)),v0->x,v1->x)) <= aaBox_p->maxDim.x &&			\
		FABS(boxPos.y - LERP(r,v0->y,v1->y)) <= aaBox_p->maxDim.y) return INTERSECTING;			 				\
	if (c & ZPOS &&																								\
		FABS(boxPos.x - LERP((r = (max.z-v0->z) / (v1->z-v0->z)),v0->x,v1->x)) <= aaBox_p->maxDim.x &&			\
		FABS(boxPos.y - LERP(r,v0->y,v1->y)) <= aaBox_p->maxDim.y) return INTERSECTING;							



	//------ Some cross product macros -------------------------

#define XPX3(a,b1,b2)		( (a)->y * (b2)	- (b1) * (a)->z )
#define XPY3(a,b1,b2)		( (a)->x * (b2)	- (b1) * (a)->z )
#define XPZ3(a,b1,b2)		( (a)->x * (b2)	- (b1) * (a)->y )


	//------ 2D point inside tri containment tests -------------

#define CONTAINEDX3(e0, e1, e2, b1, b2, res)												\
				( ((res) = SIGN(XPX3((e0),(b1),(b2)))) == SIGN(XPX3((e2),((b1)+(e2)->y),	\
				((b2)+(e2)->z))) && (res) == SIGN(XPX3((e1),(b1)-(e0)->y,(b2)-(e0)->z)))

#define CONTAINEDY3(e0, e1, e2, b1, b2, res)												\
				( ((res) = SIGN(XPY3((e0),(b1),(b2)))) == SIGN(XPY3((e2),((b1)+(e2)->x),	\
				((b2)+(e2)->z))) && (res) == SIGN(XPY3((e1),(b1)-(e0)->x,(b2)-(e0)->z)))

#define CONTAINEDZ3(e0, e1, e2, b1, b2, res)												\
				( ((res) = SIGN(XPZ3((e0),(b1),(b2)))) == SIGN(XPZ3((e2),((b1)+(e2)->x),	\
				((b2)+(e2)->y))) && (res) == SIGN(XPZ3((e1),(b1)-(e0)->x,(b2)-(e0)->y)))


	//------ These test a ray against the various planes -------
	//------ of an axis aligned box ----------------------------

#define COL_AABOX_TEST_NEG_X_PLANE()	{													\
	float	p(centre_p->x);																	\
																							\
	if (ray_p->origin.x < (p-=box_p->maxDim.x))												\
	{																						\
		if (ray_p->end.x <= p)																\
			return NON_INTERSECTING;														\
																							\
		p = (p-ray_p->origin.x)*ray_p->invDir.x;											\
																							\
		if (p>1)																			\
			return NON_INTERSECTING;					   									\
																							\
		poi_p->y	= (ray_p->dir.y * p) + ray_p->origin.y;									\
		poi_p->z	= (ray_p->dir.z * p) + ray_p->origin.z;									\
																							\
		if (FABS(centre_p->y-poi_p->y) < box_p->maxDim.y &&									\
			FABS(centre_p->z-poi_p->z) < box_p->maxDim.z)									\
		{																					\
			poi_p->x = (ray_p->dir.x * p) + ray_p->origin.x;								\
			return INTERSECTING;															\
		} 																					\
	} }

				
#define COL_AABOX_TEST_NEG_Y_PLANE()	{													\
	float	p(centre_p->y);																	\
																							\
	if (ray_p->origin.y < (p-=box_p->maxDim.y))												\
	{																						\
		if (ray_p->end.y <= p) 																\
			return NON_INTERSECTING;														\
																							\
		p = (p-ray_p->origin.y)*ray_p->invDir.y;											\
		if (p>1) 																			\
			return NON_INTERSECTING; 														\
																							\
		poi_p->x = (ray_p->dir.x * p) + ray_p->origin.x;									\
		poi_p->z	= (ray_p->dir.z * p) + ray_p->origin.z;									\
																							\
		if (FABS(centre_p->x-poi_p->x) < box_p->maxDim.x &&									\
			FABS(centre_p->z-poi_p->z) < box_p->maxDim.z)									\
		{																					\
			poi_p->y	= (ray_p->dir.y * p) + ray_p->origin.y;								\
			return INTERSECTING;															\
		}																					\
	} }

		
#define COL_AABOX_TEST_NEG_Z_PLANE()	{													\
	float	p(centre_p->z);																	\
																							\
	if (ray_p->origin.z < (p-=box_p->maxDim.z))												\
	{																						\
		if (ray_p->end.z <= p) 																\
			return NON_INTERSECTING;														\
																							\
		p = (p-ray_p->origin.z)*ray_p->invDir.z;											\
		if (p>1) 																			\
			return NON_INTERSECTING;														\
																							\
		poi_p->x = (ray_p->dir.x * p) + ray_p->origin.x;									\
		poi_p->y	= (ray_p->dir.y * p) + ray_p->origin.y;									\
																							\
		if (FABS(centre_p->x-poi_p->x) < box_p->maxDim.x &&									\
			FABS(centre_p->y-poi_p->y) < box_p->maxDim.y)									\
		{																					\
			poi_p->z	= (ray_p->dir.z * p) + ray_p->origin.z;								\
			return INTERSECTING;															\
		}																					\
	} }


#define COL_AABOX_TEST_POS_X_PLANE()	{													\
	float	p(centre_p->x);																	\
																							\
	if (ray_p->origin.x > (p+=box_p->maxDim.x))												\
	{																						\
		if (ray_p->end.x >= p) 																\
			return NON_INTERSECTING;														\
																							\
		p = (ray_p->origin.x-p)*-ray_p->invDir.x;											\
		if (p>1) 																			\
			return NON_INTERSECTING;														\
																							\
		poi_p->y	= (ray_p->dir.y * p) + ray_p->origin.y;								   	\
		poi_p->z	= (ray_p->dir.z * p) + ray_p->origin.z;								   	\
																							\
		if (FABS(centre_p->y-poi_p->y) < box_p->maxDim.y &&									\
			FABS(centre_p->z-poi_p->z) < box_p->maxDim.z)								   	\
		{																					\
			poi_p->x = (ray_p->dir.x * p) + ray_p->origin.x;							   	\
			return INTERSECTING;															\
		}																					\
	} }

		
#define COL_AABOX_TEST_POS_Y_PLANE()	{													\
	float p(centre_p->y);																	\
																							\
	if (ray_p->origin.y > (p+=box_p->maxDim.y))												\
	{																						\
		if (ray_p->end.y >= p) 																\
			return NON_INTERSECTING; 														\
																							\
		p = (ray_p->origin.y-p)*-ray_p->invDir.y;										   	\
		if (p>1) 																			\
			return NON_INTERSECTING;														\
																							\
		poi_p->x = (ray_p->dir.x * p) + ray_p->origin.x;								   	\
		poi_p->z	= (ray_p->dir.z * p) + ray_p->origin.z;								   	\
																							\
		if (FABS(centre_p->x-poi_p->x) < box_p->maxDim.x &&									\
			FABS(centre_p->z-poi_p->z) < box_p->maxDim.z)								   	\
		{																					\
			poi_p->y	= (ray_p->dir.y * p) + ray_p->origin.y;							   	\
			return INTERSECTING;															\
		}																					\
	} }


#define COL_AABOX_TEST_POS_Z_PLANE()	{													\
	float	p(centre_p->z);																	\
																							\
	if (ray_p->origin.z > (p+=box_p->maxDim.z))												\
	{																						\
		if (ray_p->end.z >= p) 																\
			return NON_INTERSECTING; 														\
																							\
		p = (ray_p->origin.z-p)*-ray_p->invDir.z;											\
		if (p>1) 																			\
			return NON_INTERSECTING;														\
																							\
		poi_p->x = (ray_p->dir.x * p) + ray_p->origin.x;									\
		poi_p->y	= (ray_p->dir.y * p) + ray_p->origin.y;									\
																							\
		if (FABS(centre_p->x-poi_p->x) < box_p->maxDim.x &&									\
			FABS(centre_p->y-poi_p->y) < box_p->maxDim.y)									\
		{																					\
			poi_p->z	= (ray_p->dir.z * p) + ray_p->origin.z;								\
			return INTERSECTING;															\
		}																					\
	} }												


/*--------------------------------------------------------------

	Structures & Classes

--------------------------------------------------------------*/

struct	COL_POLY3
{
	int		vIdx[3];
	int		edgeIdx[3];

	int		normalMajorAxis;

	VEC3	min;
	VEC3	max;

	VEC3	normal;

	bool	double_sided;
};


struct	COL_MESH
{
	int			numPolys;
	int			numEdges;
	int			numVerts;

	VEC3		minBox;				// Bounding box
	VEC3		maxBox;

	VEC3		*v;					// vertices
	VEC3		*e;					// edges
	COL_POLY3	*p;					// polys
};


struct	COL_AABOX
{
	VEC3		maxDim;
	VEC3		offsToCentre;
};


struct	COL_RAY
{
	VEC3		origin;				  // The world position of the start of the ray
	VEC3		end;				  // origin + dir (for collision optimisation)
	VEC3		dir;				  // The ray direction & magnitude 
	VEC3		invDir;				  // The inverse of the rays direction (for collision optimisation)
}; 


/*--------------------------------------------------------------

	Prototypes

--------------------------------------------------------------*/

void		COL_checkCollisions(COL_MESH **colMeshes_pp, VEC3 *pos_p, int numMeshes,
								float *x, float *y, float *z,
								float oldX, float oldY, float oldZ,
								float *shadowHeight,
								float maxStepHeight,
								COL_AABOX *aaBox_p);

void		COL_init(void);
void		COL_exit(void);


/*--------------------------------------------------------------

	Externs

--------------------------------------------------------------*/


#endif
