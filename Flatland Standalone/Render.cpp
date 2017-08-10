//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "Classes.h"
#include "Fileio.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Spans.h"
#include "Utils.h"

// Frustum test results.

#define OUTSIDE_FRUSTUM		0
#define INTERSECTS_FRUSTUM	1
#define INSIDE_FRUSTUM		2

// List of solid colour spans, transparent transformed polygons and colour transformed polygons.

static span *colour_span_list;
static tpolygon *transparent_tpolygon_list;
static tpolygon *colour_tpolygon_list;

// The current absolute and relative camera position.

static vertex camera_position;
static int camera_column, camera_row, camera_level;
static vertex relative_camera_position;
static vector camera_direction;

// The visiblity of the current block.

static int curr_block_visibility;

// The current square and block being rendered, it's type, and whether it's
// movable or not.

static square *curr_square_ptr;
static block *curr_block_ptr;
static int curr_block_type;
static bool curr_block_movable;
static vertex block_centre;

// The current block translation.

static vertex block_translation;

// The current block's transformed vertex list.

static int max_block_vertices;
static vertex *block_tvertex_list;

// The current polygon's vertex colour list and front face visible flag.

static int max_polygon_vertices;
static RGBcolour *vertex_colour_list;
static bool front_face_visible;

// Variables used to keep track of currently rendered polygon.

static int spoints;						// Number of spoints.
static spoint *main_spoint_list;		// Main screen point list.
static spoint *temp_spoint_list;		// Temporary screen point list.
static spoint *first_spoint_ptr;		// Pointer to first screen point.
static spoint *last_spoint_ptr;			// Pointer to last screen point.
static spoint *top_spoint_ptr;			// Pointer to topmost screen point.
static spoint *bottom_spoint_ptr;		// Pointer to bottommost screen point.
static spoint *left_spoint1_ptr;		// Endpoints of current left edge.
static spoint *left_spoint2_ptr;	
static spoint *right_spoint1_ptr;		// Endpoints of current right edge.
static spoint *right_spoint2_ptr;
static edge left_edge, right_edge;		// Current left and right edge.
static edge left_slope, right_slope;	// Current left and right slope.
static float half_texel_u;				// Normalised size of half a texel.
static float half_texel_v;

// Texture coordinate scaling list.

static float uv_scale_list[IMAGE_SIZES] = {
	1.0f, 1.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f
};

// One over texture dimensions list.

static float one_on_dimensions_list[IMAGE_SIZES] = {
	1.0f / 1024.0f, 
	1.0f / 512.0f, 
	1.0f / 256.0f, 
	1.0f / 128.0f, 
	1.0f / 64.0f, 
	1.0f / 32.0f, 
	1.0f / 16.0f, 
	1.0f / 8.0f,
	1.0f / 4.0f,
	1.0f / 2.0f
};

// View bounding box.

static vertex min_view, max_view;

// Visible popup list, last popup in list, and currently selected popup.

static popup *visible_popup_list;
static popup *last_visible_popup_ptr;
static popup *curr_popup_ptr;

// Flag indicating whether a polygon or popup selection has been found.

static bool found_selection;

// Current screen coordinates and size of orb.

static float curr_orb_x, curr_orb_y;
static float curr_orb_width, curr_orb_height;

//------------------------------------------------------------------------------
// Initialise the renderer.
//------------------------------------------------------------------------------

void
init_renderer(void)
{
	block_tvertex_list = NULL;
	vertex_colour_list = NULL;
	temp_spoint_list = NULL;
}

//------------------------------------------------------------------------------
// Set up the renderer.
//------------------------------------------------------------------------------

void
set_up_renderer(void)
{
	blockset *blockset_ptr;
	block_def *block_def_ptr;
	int polygon_no;
	polygon_def *polygon_def_ptr;

	// Step through all block definitions, and remember the most vertices
	// seen in a block and polygon.  The minimum is four (required for the
	// ground block).

	max_block_vertices = 4;
	max_polygon_vertices = 4;
	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		block_def_ptr = blockset_ptr->block_def_list;
		while (block_def_ptr != NULL) {
			if (block_def_ptr->vertices > max_block_vertices)
				max_block_vertices = block_def_ptr->vertices;
			for (polygon_no = 0; polygon_no < block_def_ptr->polygons;
				polygon_no++) {
				polygon_def_ptr = &block_def_ptr->polygon_def_list[polygon_no];
				if (polygon_def_ptr->vertices > max_polygon_vertices)
					max_polygon_vertices = polygon_def_ptr->vertices;
			}
			block_def_ptr = block_def_ptr->next_block_def_ptr;
		}
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	block_def_ptr = custom_blockset_ptr->block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->vertices > max_block_vertices)
			max_block_vertices = block_def_ptr->vertices;
		for (polygon_no = 0; polygon_no < block_def_ptr->polygons; 
			polygon_no++) {
			polygon_def_ptr = &block_def_ptr->polygon_def_list[polygon_no];
			if (polygon_def_ptr->vertices > max_polygon_vertices)
				max_polygon_vertices = polygon_def_ptr->vertices;
		}
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}

	// Initialise the free transformed vertex and polygon lists.

	init_free_tvertex_list();
	init_free_tpolygon_list();

	// Initialise the screen polygon list.  The maximum number of screen points
	// per polygon must be the maximum number of polygon vertices + 5; this
	// caters for polygons clipped to the viewing plane and the four screen
	// edges.

	init_screen_polygon_list(max_polygon_vertices + 5);

	// Create the transformed vertex list.

	NEWARRAY(block_tvertex_list, vertex, max_block_vertices);
	if (block_tvertex_list == NULL)
		memory_error("block transformed vertex list");

	// Create the vertex colour list.

	NEWARRAY(vertex_colour_list, RGBcolour, max_polygon_vertices);
	if (vertex_colour_list == NULL)
		memory_error("vertex colour list");

	// Create the temp screen point list.

	NEWARRAY(temp_spoint_list, spoint, max_polygon_vertices + 5);
	if (temp_spoint_list == NULL)
		memory_error("screen point list");
}

//------------------------------------------------------------------------------
// Clean up the renderer.
//------------------------------------------------------------------------------

void
clean_up_renderer(void)
{
	delete_screen_polygon_list();
	if (block_tvertex_list != NULL) {
		DELARRAY(block_tvertex_list, vertex, max_block_vertices);
		block_tvertex_list = NULL;
	}
	if (vertex_colour_list != NULL) {
		DELARRAY(vertex_colour_list, RGBcolour, max_polygon_vertices);
		vertex_colour_list = NULL;
	}
	if (temp_spoint_list != NULL) {
		DELARRAY(temp_spoint_list, spoint, max_polygon_vertices + 5);
		temp_spoint_list = NULL;
	}
}

//------------------------------------------------------------------------------
// Translate a vertex by the player position.
//------------------------------------------------------------------------------

void
translate_vertex(vertex *old_vertex_ptr, vertex *new_vertex_ptr)
{
	// Translate the old vertex by the inverse of the player viewpoint.

	new_vertex_ptr->x = old_vertex_ptr->x - player_viewpoint.position.x;
	new_vertex_ptr->y = old_vertex_ptr->y - player_viewpoint.position.y;
	new_vertex_ptr->z = old_vertex_ptr->z - player_viewpoint.position.z;
}

//------------------------------------------------------------------------------
// Rotate a vertex by the player orientation.
//------------------------------------------------------------------------------

void
rotate_vertex(vertex *old_vertex_ptr, vertex *new_vertex_ptr)
{
	float rz;

	// Rotate the vertex around the Y axis by the inverse of the player turn 
	// angle.

	new_vertex_ptr->x = old_vertex_ptr->x * cosf(player_viewpoint.inv_turn_angle_radians) + 
		old_vertex_ptr->z * sinf(player_viewpoint.inv_turn_angle_radians);
	rz = old_vertex_ptr->z * cosf(player_viewpoint.inv_turn_angle_radians) - 
		old_vertex_ptr->x * sinf(player_viewpoint.inv_turn_angle_radians);

	// Rotate the vertex around the X axis by the inverse of the player look
	// angle.

	new_vertex_ptr->y = old_vertex_ptr->y * cosf(player_viewpoint.inv_look_angle_radians) - 
		rz * sinf(player_viewpoint.inv_look_angle_radians);
	new_vertex_ptr->z = rz * cosf(player_viewpoint.inv_look_angle_radians) + 
		old_vertex_ptr->y * sinf(player_viewpoint.inv_look_angle_radians);
}

//------------------------------------------------------------------------------
// Transform a vertex by the player position and orientation, and then adjust
// by the current camera offset.
//------------------------------------------------------------------------------

void
transform_vertex(vertex *old_vertex_ptr, vertex *new_vertex_ptr)
{
	float tx, ty, tz;
	float rx, ry, rz1, rz2;

	// Translate the old vertex by the inverse of the player viewpoint.

	tx = old_vertex_ptr->x - player_viewpoint.position.x;
	ty = old_vertex_ptr->y - player_viewpoint.position.y;
	tz = old_vertex_ptr->z - player_viewpoint.position.z;

	// Rotate the vertex around the Y axis by the inverse of the player turn angle.

	rx = tx * cosf(player_viewpoint.inv_turn_angle_radians) + tz * sinf(player_viewpoint.inv_turn_angle_radians);
	rz1 = tz * cosf(player_viewpoint.inv_turn_angle_radians) - tx * sinf(player_viewpoint.inv_turn_angle_radians);

	// Rotate the vertex around the X axis by the inverse of the player look angle.

	ry = ty * cosf(player_viewpoint.inv_look_angle_radians)- rz1 * sinf(player_viewpoint.inv_look_angle_radians);
	rz2 = rz1 * cosf(player_viewpoint.inv_look_angle_radians) + ty * sinf(player_viewpoint.inv_look_angle_radians);

	// Translate the vertex by the camera offset.

	new_vertex_ptr->x = rx - player_camera_offset.dx;
	new_vertex_ptr->y = ry - player_camera_offset.dy;
	new_vertex_ptr->z = rz2 - player_camera_offset.dz;
}

//------------------------------------------------------------------------------
// Transform a vector by the player orientation.
//------------------------------------------------------------------------------

void
transform_vector(vector *old_vector_ptr, vector *new_vector_ptr)
{
	float rdz;

	// Rotate the vector around the Y axis by the inverse of the player turn
	// angle.

	new_vector_ptr->dx = old_vector_ptr->dx * cosf(player_viewpoint.inv_turn_angle_radians) + 
		old_vector_ptr->dz * sinf(player_viewpoint.inv_turn_angle_radians);
	rdz = old_vector_ptr->dz * cosf(player_viewpoint.inv_turn_angle_radians) - 
		old_vector_ptr->dx * sinf(player_viewpoint.inv_turn_angle_radians);

	// Rotate the vertex around the X axis by the inverse of the player look
	// angle.

	new_vector_ptr->dy = old_vector_ptr->dy * cosf(player_viewpoint.inv_look_angle_radians) - 
		rdz * sinf(player_viewpoint.inv_look_angle_radians);
	new_vector_ptr->dz = rdz * cosf(player_viewpoint.inv_look_angle_radians) + 
		old_vector_ptr->dy * sinf(player_viewpoint.inv_look_angle_radians);
}

//------------------------------------------------------------------------------
// Determine if the given polygon is outside, inside or intersecting the
// frustum.
//------------------------------------------------------------------------------

static int
compare_polygon_against_frustum(polygon *polygon_ptr)
{
	int vertices, vertex_index;
	int planes, plane_index;
	vector *normal_vector_ptr;
	float plane_offset;
	polygon_def *polygon_def_ptr;
	vertex *vertex_list, *vertex_ptr;
	vertex_def *vertex_def_ptr;

	// For each frustum plane, determine how many polygon vertices are on the 
	// inside.  If none are, the polygon is outside of the frustum.  Otherwise
	// it is intersecting or inside the frustum.

	vertex_list = block_tvertex_list;
	polygon_def_ptr = polygon_ptr->polygon_def_ptr;
	planes = 0;
	for (plane_index = 0; plane_index < FRUSTUM_PLANES; plane_index++) {
		normal_vector_ptr = &frustum_normal_vector_list[plane_index];
		plane_offset = frustum_plane_offset_list[plane_index];
		vertices = 0;
		for (vertex_index = 0; vertex_index < polygon_def_ptr->vertices;
			vertex_index++) {
			vertex_def_ptr = &polygon_def_ptr->vertex_def_list[vertex_index];
			vertex_ptr = &vertex_list[vertex_def_ptr->vertex_no];
			if (FLE(normal_vector_ptr->dx * vertex_ptr->x + 
				normal_vector_ptr->dy * vertex_ptr->y +
				normal_vector_ptr->dz * vertex_ptr->z +
				plane_offset, 0.0f))
				vertices++;
		}
		if (vertices == 0)
			return(OUTSIDE_FRUSTUM);
		if (vertices == polygon_def_ptr->vertices)
			planes++;
	}
	return(planes == 6 ? INSIDE_FRUSTUM : INTERSECTS_FRUSTUM);
}

//------------------------------------------------------------------------------
// Determine if the current screen polygon is outside, inside or intersecting
// the window.
//------------------------------------------------------------------------------

static int
compare_spoints_against_window(void)
{
	int edges;
	int spoint_index, spoints_inside;
	spoint *spoint_ptr;

	edges = 0;

	spoints_inside = 0;
	for (spoint_index = 0; spoint_index < spoints; spoint_index++) {
		spoint_ptr = &main_spoint_list[spoint_index];
		if (spoint_ptr->sx >= 0)
			spoints_inside++;
	}
	if (spoints_inside == 0)
		return(OUTSIDE_FRUSTUM);
	if (spoints_inside == spoints)
		edges++;

	spoints_inside = 0;
	for (spoint_index = 0; spoint_index < spoints; spoint_index++) {
		spoint_ptr = &main_spoint_list[spoint_index];
		if (spoint_ptr->sx < window_width)
			spoints_inside++;
	}
	if (spoints_inside == 0)
		return(OUTSIDE_FRUSTUM);
	if (spoints_inside == spoints)
		edges++;

	spoints_inside = 0;
	for (spoint_index = 0; spoint_index < spoints; spoint_index++) {
		spoint_ptr = &main_spoint_list[spoint_index];
		if (spoint_ptr->sy >= 0)
			spoints_inside++;
	}
	if (spoints_inside == 0)
		return(OUTSIDE_FRUSTUM);
	if (spoints_inside == spoints)
		edges++;

	spoints_inside = 0;
	for (spoint_index = 0; spoint_index < spoints; spoint_index++) {
		spoint_ptr = &main_spoint_list[spoint_index];
		if (spoint_ptr->sy < window_height)
			spoints_inside++;
	}
	if (spoints_inside == 0)
		return(OUTSIDE_FRUSTUM);
	if (spoints_inside == spoints)
		edges++;

	return(edges == 4 ? INSIDE_FRUSTUM : INTERSECTS_FRUSTUM);
}

//------------------------------------------------------------------------------
// Determine whether a polygon is visible, and if so, which face is visible.
//------------------------------------------------------------------------------

static bool
polygon_visible(polygon *polygon_ptr, float angle)
{
	polygon_def *polygon_def_ptr;
	vertex *vertex_list;
	vertex *vertex_ptr;
	part *part_ptr;
	texture *texture_ptr;
	vector normal_vector;
	float dot_product;

	// If the polygon is not active, it's invisible.

	if (!polygon_ptr->active)
		return(false);

	// If the polygon has zero faces, it's invisible.

	polygon_def_ptr = polygon_ptr->polygon_def_ptr;
	part_ptr = polygon_def_ptr->part_ptr;
	if (part_ptr->faces == 0)
		return(false);

	// Calculate the dot product between the view vector, which is the vector
	// from the origin to any transformed vertex of the polygon, and the
	// normal vector rotated to match the viewer and polygon orientation.
	// If the result is negative, we're looking at the front face of the
	// polygon, otherwise we're looking at the back.

	if (FEQ(angle, 0.0f))
		transform_vector(&polygon_ptr->normal_vector, &normal_vector);
	else {
		normal_vector = polygon_ptr->normal_vector;
		normal_vector.rotate_y(angle - player_viewpoint.turn_angle);
		normal_vector.rotate_x(-player_viewpoint.look_angle);
	}
	vertex_list = block_tvertex_list;
	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
	vertex_ptr = VERTEX_PTR(0);
	dot_product = vertex_ptr->x * normal_vector.dx + 
		vertex_ptr->y * normal_vector.dy + vertex_ptr->z * normal_vector.dz;
	front_face_visible = FLT(dot_product, 0.0f);

	// If the polygon is two-sided, translucent (hardware acceleration only),
	// or transparent, it is visible regardless of which face is being viewed.
	
	texture_ptr = part_ptr->texture_ptr;
	if (part_ptr->faces == 2 || 
		(hardware_acceleration && part_ptr->alpha < 1.0f) ||
		(texture_ptr && texture_ptr->transparent))
		return(true);

	// If the polygon is one-sided or opaque, then it's visible if we're
	// looking at the front face.

	return(front_face_visible);
}

//------------------------------------------------------------------------------
// Determine the bounding box of a polygon by stepping through it's screen
// point list.  We return pointers to the top, bottom, left and right screen
// points.
//------------------------------------------------------------------------------

static void
get_polygon_bounding_box(spoint *&first_spoint_ptr, spoint *&last_spoint_ptr,
						 spoint *&top_spoint_ptr, spoint *&bottom_spoint_ptr,
						 spoint *&left_spoint_ptr, spoint *&right_spoint_ptr)
{
	spoint *spoint_ptr;

	// Initialise the pointer to the first and last screen point.

	first_spoint_ptr = main_spoint_list;
	last_spoint_ptr = &main_spoint_list[spoints - 1];	

	// Initialise all screen point pointers to the first screen point.

	spoint_ptr = first_spoint_ptr;
	top_spoint_ptr = first_spoint_ptr;
	bottom_spoint_ptr = first_spoint_ptr;
	left_spoint_ptr = first_spoint_ptr;
	right_spoint_ptr = first_spoint_ptr;
	
	// Now step through the remaining screen points to see which are the one's
	// we really want.

	for (spoint_ptr++; spoint_ptr <= last_spoint_ptr; spoint_ptr++) {
		if (spoint_ptr->sy < top_spoint_ptr->sy)
			top_spoint_ptr = spoint_ptr;
		if (spoint_ptr->sy > bottom_spoint_ptr->sy)
			bottom_spoint_ptr = spoint_ptr;
		if (spoint_ptr->sx < left_spoint_ptr->sx)
			left_spoint_ptr = spoint_ptr;
		if (spoint_ptr->sx > right_spoint_ptr->sx)
			right_spoint_ptr = spoint_ptr;
	}
}

//------------------------------------------------------------------------------
// Clip a transformed 3D line to the viewing plane at z = 1.
//------------------------------------------------------------------------------

static void
clip_3D_line(vertex *tvertex1_ptr, vertex_def *vertex1_def_ptr,
			 RGBcolour *vertex1_colour_ptr, vertex *tvertex2_ptr, 
			 vertex_def *vertex2_def_ptr, RGBcolour *vertex2_colour_ptr,
		     vertex *clipped_tvertex_ptr, vertex_def *clipped_vertex_def_ptr,
			 RGBcolour *clipped_vertex_colour_ptr)
{
	float fraction;
	
	// Find the fraction of the line that is behind the viewing plane.
	
	fraction = (1.0f - tvertex1_ptr->z) / (tvertex2_ptr->z - tvertex1_ptr->z);

	// Compute the vertex intersection.

	clipped_tvertex_ptr->x = tvertex1_ptr->x +
		fraction * (tvertex2_ptr->x - tvertex1_ptr->x);
	clipped_tvertex_ptr->y = tvertex1_ptr->y +
		fraction * (tvertex2_ptr->y - tvertex1_ptr->y);
	clipped_tvertex_ptr->z = 1.0;

	// Compute the texture coordinate intersection.
	
	clipped_vertex_def_ptr->u = vertex1_def_ptr->u + 
		fraction * (vertex2_def_ptr->u - vertex1_def_ptr->u);
	clipped_vertex_def_ptr->v = vertex1_def_ptr->v + 
		fraction * (vertex2_def_ptr->v - vertex1_def_ptr->v);

	// Compute the colour intersection.

	clipped_vertex_colour_ptr->red = vertex1_colour_ptr->red +
		fraction * (vertex2_colour_ptr->red - vertex1_colour_ptr->red);
	clipped_vertex_colour_ptr->green = vertex1_colour_ptr->green +
		fraction * (vertex2_colour_ptr->green - vertex1_colour_ptr->green);
	clipped_vertex_colour_ptr->blue = vertex1_colour_ptr->blue +
		fraction * (vertex2_colour_ptr->blue - vertex1_colour_ptr->blue);
}

//------------------------------------------------------------------------------
// Project a transformed vertex into screen space, and add the resulting screen
// point to the polygon's screen point list.  Also compute 1/tz for that screen
// point.
//------------------------------------------------------------------------------

static spoint *
add_spoint_to_list(vertex *tvertex_ptr, vertex_def *vertex_def_ptr, 
				   RGBcolour *vertex_colour_ptr)
{
	spoint *spoint_ptr;
	float one_on_tz, px, py;
	float u, v;

	// Get a pointer to the next available screen point.

	spoint_ptr = &main_spoint_list[spoints++];

	// Project vertex into screen space.

	one_on_tz = 1.0f / tvertex_ptr->z;
	px = tvertex_ptr->x * horz_scaling_factor * one_on_tz;
	py = tvertex_ptr->y * vert_scaling_factor * one_on_tz;
	spoint_ptr->sx = half_window_width + (px * half_window_width);
	spoint_ptr->sy = half_window_height - (py * half_window_height);

	// Save 1/tz.

	spoint_ptr->one_on_tz = one_on_tz;

	// If or v are 0 or 1, move them in by half a texel to prevent inaccurate
	// texture wrapping.

	u = vertex_def_ptr->u;
	if (FEQ(u, 0.0f))
		u += half_texel_u;
	else if (FEQ(u, 1.0f))
		u -= half_texel_u;
	v = vertex_def_ptr->v;
	if (FEQ(v, 0.0f))
		v += half_texel_v;
	else if (FEQ(v, 1.0f))
		v -= half_texel_v;

	// Compute u/tz and v/tz.

	spoint_ptr->u_on_tz = u * one_on_tz;
	spoint_ptr->v_on_tz = v * one_on_tz;

	// Save the normalised lit colour.

	spoint_ptr->colour = *vertex_colour_ptr;
	return(spoint_ptr);
}

//------------------------------------------------------------------------------
// Project a polygon from 3D space to 2D space without clipping.
//------------------------------------------------------------------------------

static void
project_3D_polygon(polygon_def *polygon_def_ptr, pixmap *pixmap_ptr)
{
	int vertices;
	int vertex_no;
	vertex_def *vertex_def_ptr;
	vertex *tvertex_ptr;
	RGBcolour *vertex_colour_ptr;
	spoint *spoint_ptr;

	// Get the required information from the polygon.

	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
	vertices = polygon_def_ptr->vertices;

	// Step through the transformed vertices of the polygon, and project and
	// add them to the screen point list

	spoints = 0;
	for (vertex_no = 0; vertex_no < vertices; vertex_no++) {

		vertex_def_ptr = &vertex_def_list[vertex_no];
		tvertex_ptr = &block_tvertex_list[vertex_def_ptr->vertex_no];
		vertex_colour_ptr = &vertex_colour_list[vertex_no];
		spoint_ptr = add_spoint_to_list(tvertex_ptr, vertex_def_ptr, 
			vertex_colour_ptr);

		// Adjust the screen point if it's off the window.  This is just a
		// sanity check for small math errors.

		if (spoint_ptr->sx < 0)
			spoint_ptr->sx = 0;
		else if (spoint_ptr->sx >= window_width)
			spoint_ptr->sx = (float)(window_width - 1);
		if (spoint_ptr->sy < 0)
			spoint_ptr->sy = 0;
		else if (spoint_ptr->sy >= window_height)
			spoint_ptr->sy = (float)(window_height - 1);
	}
}

//------------------------------------------------------------------------------
// Clip a transformed polygon against the viewing plane at z = 1, and store the
// projected screen points in the polygon definition's screen point list.
//------------------------------------------------------------------------------

static void
clip_and_project_3D_polygon(polygon_def *polygon_def_ptr, pixmap *pixmap_ptr)
{
	int vertices;
	int vertex1_no, vertex2_no;

	// Get the required information from the polygon.

	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
	vertices = polygon_def_ptr->vertices;

	// Step through the transformed vertices of the polygon, checking the status
	// of each edge compared against the viewing plane, and output the new
	// set of vertices.

	spoints = 0;
	vertex1_no = vertices - 1;
	for (vertex2_no = 0; vertex2_no < vertices; vertex2_no++) {
		vertex_def *vertex1_def_ptr, *vertex2_def_ptr, clipped_vertex_def;
		vertex *tvertex1_ptr, *tvertex2_ptr, clipped_tvertex;
		RGBcolour *vertex1_colour_ptr, *vertex2_colour_ptr, 
			clipped_vertex_colour;

		// Get the vertex definition pointers for the current edge, and then
		// get the vertex pointers and brightness values.

		vertex1_def_ptr = &vertex_def_list[vertex1_no];
		vertex2_def_ptr = &vertex_def_list[vertex2_no];
		tvertex1_ptr = &block_tvertex_list[vertex1_def_ptr->vertex_no];
		tvertex2_ptr = &block_tvertex_list[vertex2_def_ptr->vertex_no];
		vertex1_colour_ptr = &vertex_colour_list[vertex1_no];
		vertex2_colour_ptr = &vertex_colour_list[vertex2_no];

		// Compare the edge against the viewing plane at z = 1 and add
		// zero, one or two vertices to the global vertex list.

		if (tvertex1_ptr->z >= 1.0) {

			// If the edge is entirely in front of the viewing plane, add the
			// end point to the screen point list.

			if (tvertex2_ptr->z >= 1.0)
				add_spoint_to_list(tvertex2_ptr, vertex2_def_ptr, 
					vertex2_colour_ptr); 

			// If the edge is crossing over to the back of the viewing plane,
			// add the intersection point to the screen point list.

			else {
				clip_3D_line(tvertex1_ptr, vertex1_def_ptr, vertex1_colour_ptr,
					tvertex2_ptr, vertex2_def_ptr, vertex2_colour_ptr,
					&clipped_tvertex, &clipped_vertex_def, 
					&clipped_vertex_colour);
				add_spoint_to_list(&clipped_tvertex, &clipped_vertex_def,
					&clipped_vertex_colour);
			}			
		} else {

			// If the edge is crossing over to the front of the viewing plane,
			// add the intersection point and the edge's end point to the screen
			// point list.

			if (tvertex2_ptr->z >= 1.0) {
				clip_3D_line(tvertex1_ptr, vertex1_def_ptr, vertex1_colour_ptr,
					tvertex2_ptr, vertex2_def_ptr, vertex2_colour_ptr,
					&clipped_tvertex, &clipped_vertex_def, 
					&clipped_vertex_colour);
				add_spoint_to_list(&clipped_tvertex, &clipped_vertex_def,
					&clipped_vertex_colour);
				add_spoint_to_list(tvertex2_ptr, vertex2_def_ptr,
					vertex2_colour_ptr);
			}
		}
		
		// Move onto the next edge.
	
		vertex1_no = vertex2_no;
	}
}

//------------------------------------------------------------------------------
// Scale u/tz and v/tz for all screen points.
//------------------------------------------------------------------------------

static void
scale_tvertex_texture_interpolants(pixmap *pixmap_ptr, tvertex *tvertex_list, int texture_style)
{
	float u_scale, v_scale;

	// Scale u and v in each screen point by the ratio of the pixmap size to the cached image size.
	// If the pixmap is tiled, then scale by the ratio of 256 pixels to the cached image size for pixmaps
	// smaller than 256 pixels, otherwise just use a 1:1 ratio; if there is no pixmap, don't scale at all.

	if (pixmap_ptr == NULL) {
		u_scale = 1.0f;
		v_scale = 1.0f;
	} else {
		if (texture_style == TILED_TEXTURE) {
			u_scale = uv_scale_list[pixmap_ptr->size_index];
			v_scale = u_scale;
		} else {
			float one_on_dimensions = one_on_dimensions_list[pixmap_ptr->size_index];
			u_scale = (float)pixmap_ptr->width * one_on_dimensions;
			v_scale = (float)pixmap_ptr->height * one_on_dimensions;
		}
	}

	// Perform the actual scaling.

	tvertex *tvertex_ptr = tvertex_list;
	while (tvertex_ptr) {
		tvertex_ptr->u *= u_scale;
		tvertex_ptr->v *= v_scale;
		tvertex_ptr = tvertex_ptr->next_tvertex_ptr;
	}
}

//------------------------------------------------------------------------------
// Scale u/tz and v/tz for all screen points.
//------------------------------------------------------------------------------

static void
scale_spoint_texture_interpolants(pixmap *pixmap_ptr, int texture_style)
{
	float u_scale, v_scale;
	
	// Scale u/tz and v/tz in each screen point by the size of the pixmap.
	// If there is no pixmap or it is tiled, then use 256x256 for any pixmap
	// smaller than that, otherwise use the cached image size.

	if (pixmap_ptr == NULL) {
		u_scale = 256.0f;
		v_scale = 256.0f;
	} else if (texture_style == TILED_TEXTURE) {
		int image_dimensions = image_dimensions_list[pixmap_ptr->size_index];
		u_scale = image_dimensions < 256 ? 256.0f : image_dimensions;
		v_scale = u_scale;
	} else {
		u_scale = (float)pixmap_ptr->width;
		v_scale = (float)pixmap_ptr->height;
	}

	// Perform the actual scaling.

	for (int index = 0; index < spoints; index++) {
		spoint *spoint_ptr = &main_spoint_list[index];
		spoint_ptr->u_on_tz *= u_scale;
		spoint_ptr->v_on_tz *= v_scale;
	}
}

//------------------------------------------------------------------------------
// Clip a 2D line to a vertical screen edge.
//------------------------------------------------------------------------------

static void
clip_2D_line_to_x(spoint *spoint1_ptr, spoint *spoint2_ptr, float sx,
				  spoint *clipped_spoint_ptr)
{
	float fraction;
	
	// Find the fraction of the line that is to the left of the screen edge.
	
	fraction = (sx - spoint1_ptr->sx) / (spoint2_ptr->sx - spoint1_ptr->sx);

	// Compute the vertex intersection.

	clipped_spoint_ptr->sx = sx;
	clipped_spoint_ptr->sy = spoint1_ptr->sy +
		fraction * (spoint2_ptr->sy - spoint1_ptr->sy);

	// Compute the 1/tz, u/tz and v/tz intersections.

	clipped_spoint_ptr->one_on_tz = spoint1_ptr->one_on_tz + 
		fraction * (spoint2_ptr->one_on_tz - spoint1_ptr->one_on_tz);
	clipped_spoint_ptr->u_on_tz = spoint1_ptr->u_on_tz + 
		fraction * (spoint2_ptr->u_on_tz - spoint1_ptr->u_on_tz);
	clipped_spoint_ptr->v_on_tz = spoint1_ptr->v_on_tz +
		fraction * (spoint2_ptr->v_on_tz - spoint1_ptr->v_on_tz);

	// Compute the colour intersection.

	clipped_spoint_ptr->colour.red = spoint1_ptr->colour.red + fraction * 
		(spoint2_ptr->colour.red - spoint1_ptr->colour.red);
	clipped_spoint_ptr->colour.green = spoint1_ptr->colour.green + fraction *
		(spoint2_ptr->colour.green - spoint1_ptr->colour.green);
	clipped_spoint_ptr->colour.blue = spoint1_ptr->colour.blue + fraction * 
		(spoint2_ptr->colour.blue - spoint1_ptr->colour.blue);
}

//------------------------------------------------------------------------------
// Clip a 2D line to a horizontal screen edge.
//------------------------------------------------------------------------------

static void
clip_2D_line_to_y(spoint *spoint1_ptr, spoint *spoint2_ptr, float sy,
				  spoint *clipped_spoint_ptr)
{
	float fraction;

	// Find the fraction of the line that is above the screen edge.
	
	fraction = (sy - spoint1_ptr->sy) / (spoint2_ptr->sy - spoint1_ptr->sy);

	// Compute the vertex intersection.

	clipped_spoint_ptr->sx = spoint1_ptr->sx +
		fraction * (spoint2_ptr->sx - spoint1_ptr->sx);
	clipped_spoint_ptr->sy = sy;

	// Compute the 1/tz, u/tz and v/tz intersections.

	clipped_spoint_ptr->one_on_tz = spoint1_ptr->one_on_tz + 
		fraction * (spoint2_ptr->one_on_tz - spoint1_ptr->one_on_tz);
	clipped_spoint_ptr->u_on_tz = spoint1_ptr->u_on_tz + 
		fraction * (spoint2_ptr->u_on_tz - spoint1_ptr->u_on_tz);
	clipped_spoint_ptr->v_on_tz = spoint1_ptr->v_on_tz +
		fraction * (spoint2_ptr->v_on_tz - spoint1_ptr->v_on_tz);

	// Compute the colour intersections.

	clipped_spoint_ptr->colour.red = spoint1_ptr->colour.red + fraction * 
		(spoint2_ptr->colour.red - spoint1_ptr->colour.red);
	clipped_spoint_ptr->colour.green = spoint1_ptr->colour.green + fraction *
		(spoint2_ptr->colour.green - spoint1_ptr->colour.green);
	clipped_spoint_ptr->colour.blue = spoint1_ptr->colour.blue + fraction * 
		(spoint2_ptr->colour.blue - spoint1_ptr->colour.blue);
}

//------------------------------------------------------------------------------
// Clip a projected polygon against the display screen.
//------------------------------------------------------------------------------

static void
clip_2D_polygon(void)
{
	int old_spoints, new_spoints;
	int spoint1_no, spoint2_no;
	float left_sx, right_sx, top_sy, bottom_sy;

	// Set up the clipping edges of the display.

	left_sx = 0.0;
	right_sx = (float)window_width;
	top_sy = 0.0;
	bottom_sy = (float)window_height;

	// Clip the projected polygon against the left display edge.

	old_spoints = spoints;
	new_spoints = 0;
	spoint1_no = old_spoints - 1;
	for (spoint2_no = 0; spoint2_no < old_spoints; spoint2_no++) {
		spoint *spoint1_ptr, *spoint2_ptr;

		// Get pointers to the screen points for this edge.

		spoint1_ptr = &main_spoint_list[spoint1_no];
		spoint2_ptr = &main_spoint_list[spoint2_no];

		// Compare the polygon edge against the left display edge and add zero,
		// one or two screen points to the temporary screen point list.

		if (spoint1_ptr->sx >= left_sx) {
			if (spoint2_ptr->sx >= left_sx)
				temp_spoint_list[new_spoints++] = *spoint2_ptr;
			else
				clip_2D_line_to_x(spoint1_ptr, spoint2_ptr, left_sx, 
					&temp_spoint_list[new_spoints++]);			
		} else {
			if (spoint2_ptr->sx >= left_sx) {
				clip_2D_line_to_x(spoint1_ptr, spoint2_ptr, left_sx,
					&temp_spoint_list[new_spoints++]);
				temp_spoint_list[new_spoints++] = *spoint2_ptr;
			}
		}
		
		// Move onto the next edge.
	
		spoint1_no = spoint2_no;
	}

	// Clip the projected polygon against the right display edge.

	old_spoints = new_spoints;
	new_spoints = 0;
	spoint1_no = old_spoints - 1;
	for (spoint2_no = 0; spoint2_no < old_spoints; spoint2_no++) {
		spoint *spoint1_ptr, *spoint2_ptr;

		// Get pointers to the screen points for this edge.

		spoint1_ptr = &temp_spoint_list[spoint1_no];
		spoint2_ptr = &temp_spoint_list[spoint2_no];

		// Compare the polygon edge against the right display edge and add zero,
		// one or two screen points to the main screen point list.

		if (spoint1_ptr->sx <= right_sx) {
			if (spoint2_ptr->sx <= right_sx)
				main_spoint_list[new_spoints++] = *spoint2_ptr;
			else
				clip_2D_line_to_x(spoint1_ptr, spoint2_ptr, right_sx, 
					&main_spoint_list[new_spoints++]);			
		} else {
			if (spoint2_ptr->sx <= right_sx) {
				clip_2D_line_to_x(spoint1_ptr, spoint2_ptr, right_sx,
					&main_spoint_list[new_spoints++]);
				main_spoint_list[new_spoints++] = *spoint2_ptr;
			}
		}
		
		// Move onto the next edge.
	
		spoint1_no = spoint2_no;
	}

	// Clip the projected polygon against the top screen edge.

	old_spoints = new_spoints;
	new_spoints = 0;
	spoint1_no = old_spoints - 1;
	for (spoint2_no = 0; spoint2_no < old_spoints; spoint2_no++) {
		spoint *spoint1_ptr, *spoint2_ptr;

		// Get pointers to the screen points for this edge.

		spoint1_ptr = &main_spoint_list[spoint1_no];
		spoint2_ptr = &main_spoint_list[spoint2_no];

		// Compare the polygon edge against the top display edge add zero, one or
		// two screen points to the temporary screen point list.

		if (spoint1_ptr->sy >= top_sy) {
			if (spoint2_ptr->sy >= top_sy)
				temp_spoint_list[new_spoints++] = *spoint2_ptr;
			else
				clip_2D_line_to_y(spoint1_ptr, spoint2_ptr, top_sy,
					&temp_spoint_list[new_spoints++]);
		} else {
			if (spoint2_ptr->sy >= top_sy) {
				clip_2D_line_to_y(spoint1_ptr, spoint2_ptr, top_sy,
					&temp_spoint_list[new_spoints++]);
				temp_spoint_list[new_spoints++] = *spoint2_ptr;
			}
		}
		
		// Move onto the next edge.
	
		spoint1_no = spoint2_no;
	}

	// Clip the projected polygon against the bottom screen edge.

	old_spoints = new_spoints;
	new_spoints = 0;
	spoint1_no = old_spoints - 1;
	for (spoint2_no = 0; spoint2_no < old_spoints; spoint2_no++) {
		spoint *spoint1_ptr, *spoint2_ptr;

		// Get pointers to the screen points for this edge.

		spoint1_ptr = &temp_spoint_list[spoint1_no];
		spoint2_ptr = &temp_spoint_list[spoint2_no];

		// Compare the polygon edge against the bottom display edge and add zero,
		// one or two screen points to the main screen point list.

		if (spoint1_ptr->sy <= bottom_sy) {
			if (spoint2_ptr->sy <= bottom_sy)
				main_spoint_list[new_spoints++] = *spoint2_ptr;
			else
				clip_2D_line_to_y(spoint1_ptr, spoint2_ptr, bottom_sy,
					&main_spoint_list[new_spoints++]);
		} else {
			if (spoint2_ptr->sy <= bottom_sy) {
				clip_2D_line_to_y(spoint1_ptr, spoint2_ptr, bottom_sy,
					&main_spoint_list[new_spoints++]);
				main_spoint_list[new_spoints++] = *spoint2_ptr;
			}
		}
		
		// Move onto the next edge.
	
		spoint1_no = spoint2_no;
	}

	// Set the final number of screen points.

	spoints = new_spoints;
}

//------------------------------------------------------------------------------
// Determine if the mouse is currently pointing at the given screen polygon.
//------------------------------------------------------------------------------

static bool
check_for_polygon_selection(void)
{
	int spoint1_no, spoint2_no;

	// Step through the screen point list, and determine whether the mouse
	// position is inside the polygon.

	spoint1_no = spoints - 1;
	for (spoint2_no = 0; spoint2_no < spoints; spoint2_no++) {
		spoint *spoint1_ptr, *spoint2_ptr;
		float relationship;
		
		// Get pointers to the screen points for this edge.

		spoint1_ptr = &main_spoint_list[spoint1_no];
		spoint2_ptr = &main_spoint_list[spoint2_no];

		// Compute the relationship between the mouse position and the edge.

		relationship = 
			(spoint2_ptr->sy - spoint1_ptr->sy) * (mouse_x - spoint1_ptr->sx) -
			(spoint2_ptr->sx - spoint1_ptr->sx) * (mouse_y - spoint1_ptr->sy);

		// If the mouse position is behind this edge, it is outside of the
		// polygon and hence it isn't selected.  If the polygon is double-sided 
		// and we're looking at the back face, the edge test must be reversed.

		if (front_face_visible) {
			if (FGE(relationship, 0.0f))
				return(false);
		} else {
			if (FLE(relationship, 0.0f))
				return(false);
		}

		// Move onto the next edge.
	
		spoint1_no = spoint2_no;
	}

	// The polygon was selected.

	return(true);
}

//------------------------------------------------------------------------------
// Select the next left edge by moving in an anti-clockwise direction through
// the spoint list.
//------------------------------------------------------------------------------

static void
next_left_edge(void)
{
	left_spoint1_ptr = left_spoint2_ptr;
	if (left_spoint2_ptr == first_spoint_ptr)
		left_spoint2_ptr = last_spoint_ptr;
	else
		left_spoint2_ptr--;
}

//------------------------------------------------------------------------------
// Select the next right edge by moving in a clockwise direction through the
// spoint list.
//------------------------------------------------------------------------------

static void
next_right_edge(void)
{
	right_spoint1_ptr = right_spoint2_ptr;
	if (right_spoint2_ptr == last_spoint_ptr)
		right_spoint2_ptr = first_spoint_ptr;
	else
		right_spoint2_ptr++;
}

//------------------------------------------------------------------------------
// Compute the slopes for the values to be interpolated down a polygon edge.
//------------------------------------------------------------------------------

static bool
compute_slopes(spoint *spoint1_ptr, spoint *spoint2_ptr, float scan_sy,
			   edge *edge_ptr, edge *slope_ptr)
{
	float delta_sy;

	// Determine the height of the edge, and if it's zero reject it.

	delta_sy = spoint2_ptr->sy - spoint1_ptr->sy;
	if (delta_sy == 0.0)
		return(false);

	// Compute the slopes of the interpolants.

	slope_ptr->sx = (spoint2_ptr->sx - spoint1_ptr->sx) / delta_sy;
	slope_ptr->one_on_tz = (spoint2_ptr->one_on_tz - spoint1_ptr->one_on_tz) /
		delta_sy;
	slope_ptr->u_on_tz = (spoint2_ptr->u_on_tz - spoint1_ptr->u_on_tz) /
		delta_sy;
	slope_ptr->v_on_tz = (spoint2_ptr->v_on_tz - spoint1_ptr->v_on_tz) /
		delta_sy;

	// Determine the initial values for the interpolants.  If the top screen Y
	// coordinate is above the Y coordinate of the scan line, we need to adjust
	// the initial interpolants so that they represent the values at the scan
	// line.

	edge_ptr->sx = spoint1_ptr->sx;
	edge_ptr->one_on_tz = spoint1_ptr->one_on_tz;
	edge_ptr->u_on_tz = spoint1_ptr->u_on_tz;
	edge_ptr->v_on_tz = spoint1_ptr->v_on_tz;
	delta_sy = scan_sy - spoint1_ptr->sy;
	if (delta_sy > 0.0) {
		edge_ptr->sx += slope_ptr->sx * delta_sy;
		edge_ptr->one_on_tz += slope_ptr->one_on_tz * delta_sy;
		edge_ptr->u_on_tz += slope_ptr->u_on_tz * delta_sy;
		edge_ptr->v_on_tz += slope_ptr->v_on_tz * delta_sy;
	}
	return(true);
}

//------------------------------------------------------------------------------
// Get the next left or right edge, and compute the slopes for the values to
// interpolate down that edge.  A boolean flag is returned to indicate if the
// bottom of the polygon was reached.
//------------------------------------------------------------------------------

static bool
prepare_next_left_edge(float scan_sy)
{
	// Step through left edges until we either reach the bottom of the polygon,
	// or find one that isn't above the requested scan line. 
	
	do {
		if (left_spoint2_ptr == bottom_spoint_ptr)
			return(false);
		next_left_edge();
	} while (left_spoint2_ptr->sy < scan_sy || 
		!compute_slopes(left_spoint1_ptr, left_spoint2_ptr, scan_sy,
			&left_edge, &left_slope));
	return(true);
}

static bool
prepare_next_right_edge(float scan_sy)
{
	// Step through right edges until we either reach the bottom of the polygon,
	// or find one that isn't above the requested scan line.

	do {
		if (right_spoint2_ptr == bottom_spoint_ptr)
			return(false);
		next_right_edge();
	} while (right_spoint2_ptr->sy < scan_sy || 
		!compute_slopes(right_spoint1_ptr, right_spoint2_ptr, scan_sy,
			&right_edge, &right_slope));
	return(true);
}

//------------------------------------------------------------------------------
// Add a transformed vertex to a transformed polygon.
//------------------------------------------------------------------------------

static void
add_tvertex_to_tpolygon(tpolygon *tpolygon_ptr, int tvertex_index, polygon_def *polygon_def_ptr, tvertex *&last_tvertex_ptr)
{
	vertex_def *vertex_def_ptr = &polygon_def_ptr->vertex_def_list[tvertex_index];
	vertex *tvertex_ptr = &block_tvertex_list[vertex_def_ptr->vertex_no];
	tvertex *new_tvertex_ptr = new_tvertex();
	new_tvertex_ptr->x = tvertex_ptr->x;
	new_tvertex_ptr->y = tvertex_ptr->y;
	new_tvertex_ptr->z = tvertex_ptr->z;
	new_tvertex_ptr->u = vertex_def_ptr->u;
	new_tvertex_ptr->v = vertex_def_ptr->v;
	new_tvertex_ptr->colour = vertex_colour_list[tvertex_index];
	new_tvertex_ptr->next_tvertex_ptr = NULL;
	if (last_tvertex_ptr) {
		last_tvertex_ptr->next_tvertex_ptr = new_tvertex_ptr;
	} else {
		tpolygon_ptr->tvertex_list = new_tvertex_ptr;
	}
	last_tvertex_ptr = new_tvertex_ptr;
}

//------------------------------------------------------------------------------
// Create a transformed polygon suitable for handing over to the hardware
// renderer.
//------------------------------------------------------------------------------

static tpolygon *
create_transformed_polygon(polygon_def *polygon_def_ptr, pixmap *pixmap_ptr, part *part_ptr)
{
	// Get a new transformed polygon object, and initialize everything but the transformed vertex list.

	tpolygon *tpolygon_ptr = new_tpolygon();
	tpolygon_ptr->pixmap_ptr = pixmap_ptr;
	tpolygon_ptr->alpha = part_ptr->alpha;
	tpolygon_ptr->tvertex_list = NULL;
	tpolygon_ptr->tvertices = polygon_def_ptr->vertices;
	tpolygon_ptr->next_tpolygon_ptr = NULL;

	// Step through this polygon by alternating between vertices taken from stepping forward and backward through the vertex list.
	// This ensures we end up with a triangle strip suitable for rendering.

	int i = 1;
	int j = tpolygon_ptr->tvertices - 1;
	bool front = true;
	tvertex *last_tvertex_ptr = NULL;
	add_tvertex_to_tpolygon(tpolygon_ptr, 0, polygon_def_ptr, last_tvertex_ptr);
	while (i <= j) {
		if (front) {
			add_tvertex_to_tpolygon(tpolygon_ptr, i, polygon_def_ptr, last_tvertex_ptr);
			i++;
		} else {
			add_tvertex_to_tpolygon(tpolygon_ptr, j, polygon_def_ptr, last_tvertex_ptr);
			j--;
		}
		front = !front;
	}
	return tpolygon_ptr;
}

//------------------------------------------------------------------------------
// Render a polygon.
//------------------------------------------------------------------------------

static void
render_polygon(polygon *polygon_ptr, float turn_angle)
{
	int polygon_visibility;
	bool polygon_selected;
	polygon_def *polygon_def_ptr;
	spolygon *spolygon_ptr;
	vertex polygon_centroid;
	vector normal_vector;
	float brightness;
	int brightness_index;
	RGBcolour colour;
	pixel colour_pixel;
	part *part_ptr;
	texture *texture_ptr;
	pixmap *pixmap_ptr;
	float sy, end_sy;
	vertex centre(UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK);

	// If the polygon is trivially invisible (inactive, zero faces or facing
	// away from the camera), then don't render it.
	
	if (!polygon_visible(polygon_ptr, turn_angle))
		return;

	// If the current block is inside the frustum, the polygon is also inside
	// the frustum.  Otherwise, determine the polygon's visibility, and if it's
	// outside the frustum don't render it.
	
	if (curr_block_visibility == INSIDE_FRUSTUM)
		polygon_visibility = INSIDE_FRUSTUM;
	else if ((polygon_visibility = compare_polygon_against_frustum(polygon_ptr)) == OUTSIDE_FRUSTUM)
		return;

	// Get a pointer to the part and texture.
	
	polygon_def_ptr = polygon_ptr->polygon_def_ptr;
	part_ptr = polygon_def_ptr->part_ptr;
	texture_ptr = part_ptr->texture_ptr;

	// Rotate the polygon's normal vector by the turn angle, then reverse it if the back of the polygon is visible.

	normal_vector = polygon_ptr->normal_vector;
	normal_vector.rotate_y(turn_angle);
	if (!front_face_visible)
		normal_vector = -normal_vector;

	// If the texture exists, get a pointer to the current pixmap, and compute the size of half a texel in normalised
	// texture units for this pixmap. If there is no pixmap, assume a pixmap size of 256x256.

	if (texture_ptr != NULL) {
		if (curr_block_type == MULTIFACETED_SPRITE) {
			pixmap_ptr = &texture_ptr->pixmap_list[curr_block_ptr->pixmap_index];
		} else if (texture_ptr->loops) {
			pixmap_ptr = texture_ptr->get_curr_pixmap_ptr(curr_time_ms - start_time_ms);
		} else {
			pixmap_ptr = texture_ptr->get_curr_pixmap_ptr(curr_time_ms - curr_block_ptr->start_time_ms);
		}
		half_texel_u = 0.5f / pixmap_ptr->width;
		half_texel_v = 0.5f / pixmap_ptr->height;
	} else {
		pixmap_ptr = NULL;
		half_texel_u = 1.953125e-3;
		half_texel_v = 1.953125e-3;
	}

	// If hardware acceleration is enabled...

	if (hardware_acceleration) {
		int vertex_no;
		RGBcolour *vertex_colour_ptr;
		vertex polygon_vertex;
		tpolygon *tpolygon_ptr;

		// Compute the normalised lit colour for all polygon vertices, after rotating them by the turn angle.
		// If the turn angle is zero, we skip that step to save time.

		PREPARE_VERTEX_LIST(curr_block_ptr);
		PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
		if (FEQ(turn_angle, 0.0f)) {
			for (vertex_no = 0; vertex_no < polygon_def_ptr->vertices; vertex_no++) {
				polygon_vertex = *VERTEX_PTR(vertex_no) + curr_block_ptr->translation;
				vertex_colour_ptr = &vertex_colour_list[vertex_no];
				compute_vertex_colour(curr_block_ptr->active_light_list, &polygon_vertex, &normal_vector, vertex_colour_ptr);
				vertex_colour_ptr->normalise();
			}
		} else {
			for (vertex_no = 0; vertex_no < polygon_def_ptr->vertices; vertex_no++) {
				polygon_vertex = *VERTEX_PTR(vertex_no);
				polygon_vertex -= centre;
				polygon_vertex.rotate_y(turn_angle);
				polygon_vertex += block_centre;
				vertex_colour_ptr = &vertex_colour_list[vertex_no];
				compute_vertex_colour(curr_block_ptr->active_light_list, &polygon_vertex, &normal_vector, vertex_colour_ptr);
				vertex_colour_ptr->normalise();
			}
		}

		// If this polygon has no pixmap, blend it's colour with the vertex colours.

		if (pixmap_ptr == NULL)
			for (vertex_no = 0; vertex_no < polygon_def_ptr->vertices; vertex_no++)
				vertex_colour_list[vertex_no].blend(part_ptr->normalised_colour);

		// Create the transformed polygon.

		tpolygon_ptr = create_transformed_polygon(polygon_def_ptr, pixmap_ptr, part_ptr);

		// Scale the texture interpolants in the main screen point list.

		scale_tvertex_texture_interpolants(pixmap_ptr, tpolygon_ptr->tvertex_list, part_ptr->texture_style);

		// Add the transformed polygon to a list in the pixmap if it has a texture, a special transparent list if it's
		// translucent or transparent, or a special colour list if it has no texture.

		if (pixmap_ptr != NULL) {
			if (part_ptr->alpha < 1.0 || pixmap_ptr->transparent_index >= 0) {
				tpolygon_ptr->next_tpolygon_ptr = transparent_tpolygon_list;
				transparent_tpolygon_list = tpolygon_ptr;
			} else {
				tpolygon_ptr->next_tpolygon_ptr = pixmap_ptr->tpolygon_list;
				pixmap_ptr->tpolygon_list = tpolygon_ptr;
			}
		} else {
			tpolygon_ptr->next_tpolygon_ptr = colour_tpolygon_list;
			colour_tpolygon_list = tpolygon_ptr;
		}

		// Determine whether the polygon has been selected by the mouse.

		polygon_selected = !found_selection && mouse_intersects_with_polygon((float)mouse_x, (float)mouse_y, &camera_direction, tpolygon_ptr);
	} 
	
	// If hardware acceleration is not enabled... 

	else {
		spoint *left_spoint_ptr, *right_spoint_ptr;

		// Compute the brightness at the polygon centroid, and compute the colour pixel.

		polygon_centroid = polygon_ptr->centroid + block_translation;
		brightness = compute_vertex_brightness(curr_block_ptr->active_light_list,
			&polygon_centroid, &normal_vector);
		brightness_index = get_brightness_index(brightness);
		colour = part_ptr->colour;
		colour.adjust_brightness(brightness);
		colour_pixel = RGB_to_display_pixel(colour);

		// Get a pointer to the next available screen polygon, and make it's screen
		// point list be the main screen point list.

		spolygon_ptr = get_next_screen_polygon();
		main_spoint_list = spolygon_ptr->spoint_list;

		// If the polygon intersects the frustum, clip the polygon against the 
		// viewing plane before projecting it onto the 2D screen, then clip it
		// against the window.  If there are no screen points left after this
		// process, the polygon was off-screen and does not need to be rendered.
			
		if (polygon_visibility == INTERSECTS_FRUSTUM) {
			clip_and_project_3D_polygon(polygon_def_ptr, pixmap_ptr);
			if (spoints > 0)
				clip_2D_polygon();
			if (spoints == 0)
				return;
		} 
	
		// If the polygon is completely inside the frustum, just project it onto
		// the 2D screen without any clipping.

		else
			project_3D_polygon(polygon_def_ptr, pixmap_ptr);

		// Remember the number of screen points.

		spolygon_ptr->spoints = spoints;

		// Scale the texture interpolants in the main screen point list.

		scale_spoint_texture_interpolants(pixmap_ptr, part_ptr->texture_style);

		// Set pixmap pointer and alpha in the screen polygon.

		spolygon_ptr->pixmap_ptr = pixmap_ptr;
		spolygon_ptr->alpha = part_ptr->alpha;

		// Obtain pointers to the screen points representing the bounding box of
		// the polygon, as well as the last screen point in the list.

		get_polygon_bounding_box(first_spoint_ptr, last_spoint_ptr, 
			top_spoint_ptr, bottom_spoint_ptr, left_spoint_ptr, 
			right_spoint_ptr);

		// The ceiling of the top display y coordinate becomes the initial
		// display y coordinate, and the ceiling of the bottom screen y
		// coordinate becomes the last screen y coordinate + 1.  If these
		// are one and the same, then there is nothing to render.

		sy = FCEIL(top_spoint_ptr->sy);
		end_sy = FCEIL(bottom_spoint_ptr->sy);
		if (sy == end_sy)
			return;	

		// Prepare the first left and right edge for rendering.

		left_spoint2_ptr = top_spoint_ptr;
		prepare_next_left_edge(sy);
		right_spoint2_ptr = top_spoint_ptr;
		prepare_next_right_edge(sy);

		// Add the polygon to the span buffer row by row, recomputing the
		// slopes of the interpolants as as we pass the next left and right
		// vertex, until we've reached the bottom vertex or the bottom of
		// the display.

		while ((int)sy < window_height) {

			// Add this span to the span buffer.

			if (curr_block_movable)
				add_movable_span((int)sy, &left_edge, &right_edge, pixmap_ptr,
					colour_pixel, brightness_index);
			else 
				add_span((int)sy, &left_edge, &right_edge, pixmap_ptr, 
					colour_pixel, brightness_index, false);

			// Move to next row.

			sy += 1.0;
			if (sy < left_spoint2_ptr->sy) {
				left_edge.sx += left_slope.sx;
				left_edge.one_on_tz += left_slope.one_on_tz;
				left_edge.u_on_tz += left_slope.u_on_tz;
				left_edge.v_on_tz += left_slope.v_on_tz;
			} else if (!prepare_next_left_edge(sy))
				break;
			if (sy < right_spoint2_ptr->sy) {
				right_edge.sx += right_slope.sx;
				right_edge.one_on_tz += right_slope.one_on_tz;
				right_edge.u_on_tz += right_slope.u_on_tz;
				right_edge.v_on_tz += right_slope.v_on_tz;
			} else if (!prepare_next_right_edge(sy))
				break;
		}

		// Determine whether this polygon is selected.

		polygon_selected = !found_selection && check_for_polygon_selection();
	}

	// Check whether this polygon is selected by the mouse, and if so 
	// remember it as the currently selected block, popup block and square,
	// and exit.  However, if the polygon has a transparent texture or is
	// translucent, and it doesn't have an exit or any mouse-based triggers,
	// then ignore it.

	if (!found_selection && polygon_selected &&
		(((!hardware_acceleration || part_ptr->alpha == 1.0f) &&
		(texture_ptr == NULL || !texture_ptr->transparent)) ||
			((curr_block_ptr != NULL &&
			((curr_block_ptr->trigger_flags & MOUSE_TRIGGERS) ||
				(curr_block_ptr->popup_trigger_flags & MOUSE_TRIGGERS) ||
				(curr_block_ptr->exit_ptr != NULL &&
				(curr_block_ptr->exit_ptr->trigger_flags & MOUSE_TRIGGERS)))) ||
					(curr_square_ptr != NULL &&
				((curr_square_ptr->trigger_flags & MOUSE_TRIGGERS) ||
						(curr_square_ptr->popup_trigger_flags & MOUSE_TRIGGERS) ||
					(curr_square_ptr->exit_ptr != NULL &&
					(curr_square_ptr->exit_ptr->trigger_flags & MOUSE_TRIGGERS))))))) {
		found_selection = true;
		curr_selected_block_ptr = curr_block_ptr;
		curr_selected_square_ptr = curr_square_ptr;
		curr_popup_block_ptr = curr_block_ptr;
		curr_popup_square_ptr = curr_square_ptr;
		if (curr_square_ptr != NULL && curr_square_ptr->exit_ptr != NULL)
			curr_selected_exit_ptr = curr_square_ptr->exit_ptr;
		else if (curr_block_ptr != NULL)
			curr_selected_exit_ptr = curr_block_ptr->exit_ptr;
#ifdef _DEBUG
		curr_selected_polygon_no = 
			(polygon_ptr - curr_block_ptr->polygon_list) + 1;
		curr_selected_block_def_ptr = curr_block_ptr->block_def_ptr;
#endif
		//*mp* from the polygon set the part pointer
		curr_selected_part_ptr = polygon_ptr->polygon_def_ptr->part_ptr;
	}
}

//------------------------------------------------------------------------------
// Determine whether the camera is in front or behind a polygon by substituting
// the camera's relative position into the polygon's plane equation.
//------------------------------------------------------------------------------

static bool
camera_in_front(polygon *polygon_ptr)
{
	vector *normal_vector_ptr = &polygon_ptr->normal_vector;
	float relationship = normal_vector_ptr->dx * relative_camera_position.x +
		normal_vector_ptr->dy * relative_camera_position.y +
		normal_vector_ptr->dz * relative_camera_position.z + 
		polygon_ptr->plane_offset;
	return (FGE(relationship, 0.0f));
}

//------------------------------------------------------------------------------
// Render the polygons in a block by traversing the block's BSP tree.  The
// return value indicates if at least one polygon in the block was rendered.
//------------------------------------------------------------------------------

static void
render_polygons_in_block(BSP_node *BSP_node_ptr)
{
	int polygon_no;
	polygon *polygon_ptr;

	// Depending on whether the camera is in front or behind the polygon,
	// traverse the tree in front-node-back or back-node-front order.

	polygon_no = BSP_node_ptr->polygon_no;
	polygon_ptr = &curr_block_ptr->polygon_list[polygon_no];
	if (camera_in_front(polygon_ptr)) {
		if (BSP_node_ptr->front_node_ptr != NULL)
			render_polygons_in_block(BSP_node_ptr->front_node_ptr);
		render_polygon(polygon_ptr, 0.0f);
		if (BSP_node_ptr->rear_node_ptr != NULL)
			render_polygons_in_block(BSP_node_ptr->rear_node_ptr);
	} else {
		if (BSP_node_ptr->rear_node_ptr != NULL)
			render_polygons_in_block(BSP_node_ptr->rear_node_ptr);
		render_polygon(polygon_ptr, 0.0f);
		if (BSP_node_ptr->front_node_ptr != NULL)
			render_polygons_in_block(BSP_node_ptr->front_node_ptr);
	}
}

//------------------------------------------------------------------------------
// Determine if the block at the given translation is outside, inside or
// intersecting the frustum.
//------------------------------------------------------------------------------

static int
compare_block_against_frustum(block *block_ptr)
{
	COL_MESH *col_mesh_ptr;
	float min_x, min_y, min_z, max_x, max_y, max_z;
	vertex bbox[8], tbbox[8];
	int corners, planes;
	vector *normal_vector_ptr;
	float plane_offset;

	// Calculate the block's bounding box.  If the block has no collision mesh,
	// then simply assume it's always visible.

	col_mesh_ptr = block_ptr->col_mesh_ptr;
	if (col_mesh_ptr == NULL)
		return(INSIDE_FRUSTUM);
	min_x = col_mesh_ptr->minBox.x + block_ptr->translation.x;
	min_y = col_mesh_ptr->minBox.y + block_ptr->translation.y;
	min_z = col_mesh_ptr->minBox.z + block_ptr->translation.z;
	max_x = col_mesh_ptr->maxBox.x + block_ptr->translation.x;
	max_y = col_mesh_ptr->maxBox.y + block_ptr->translation.y;
	max_z = col_mesh_ptr->maxBox.z + block_ptr->translation.z;

	// Create the corner vertices of the bounding box, and transform it.

	bbox[0].x = min_x;
	bbox[0].y = min_y;
	bbox[0].z = min_z;
	transform_vertex(&bbox[0], &tbbox[0]);

	bbox[1].x = max_x;
	bbox[1].y = min_y;
	bbox[1].z = min_z;
	transform_vertex(&bbox[1], &tbbox[1]);

	bbox[2].x = min_x;
	bbox[2].y = max_y;
	bbox[2].z = min_z;
	transform_vertex(&bbox[2], &tbbox[2]);

	bbox[3].x = max_x;
	bbox[3].y = max_y;
	bbox[3].z = min_z;
	transform_vertex(&bbox[3], &tbbox[3]);

	bbox[4].x = min_x;
	bbox[4].y = min_y;
	bbox[4].z = max_z;
	transform_vertex(&bbox[4], &tbbox[4]);

	bbox[5].x = max_x;
	bbox[5].y = min_y;
	bbox[5].z = max_z;
	transform_vertex(&bbox[5], &tbbox[5]);

	bbox[6].x = min_x;
	bbox[6].y = max_y;
	bbox[6].z = max_z;
	transform_vertex(&bbox[6], &tbbox[6]);

	bbox[7].x = max_x;
	bbox[7].y = max_y;
	bbox[7].z = max_z;
	transform_vertex(&bbox[7], &tbbox[7]);

	// For each frustum plane, count the number of bounding box corners which
	// are on the inside.  If none are, the block is outside the frustum 
	// completely.  Otherwise the block is inside or intersects the frustum.
 
	planes = 0;
	for(int plane_index = 0; plane_index < FRUSTUM_PLANES; plane_index++) {
		normal_vector_ptr = &frustum_normal_vector_list[plane_index];
		plane_offset = frustum_plane_offset_list[plane_index];
		corners = 0;
		for (int vertex_index = 0; vertex_index < 8; vertex_index++)
			if (FLE(normal_vector_ptr->dx * tbbox[vertex_index].x + 
				normal_vector_ptr->dy * tbbox[vertex_index].y + 
				normal_vector_ptr->dz * tbbox[vertex_index].z + plane_offset,
				0.0f))
			corners++;
		if (corners == 0)
			return(OUTSIDE_FRUSTUM);

		// If all 8 corners are on the inside of the plane, increment the plane
		// count.

		if (corners == 8)
			planes++;
	}

	// If the block was inside of all 6 planes, then it's inside the frustum,
	// otherwise it intersects.

	return(planes == 6 ? INSIDE_FRUSTUM : INTERSECTS_FRUSTUM);
}

//------------------------------------------------------------------------------
// Render a square as a wireframe cube.
//------------------------------------------------------------------------------

static void
render_wireframe_square(int column, int row, int level)
{
	float min_x, min_y, min_z, max_x, max_y, max_z;
	vertex bbox[8], tbbox[8], vertices[24];

	// Calculate this square's bounding box.

	min_x = column * UNITS_PER_BLOCK;
	min_y = level * UNITS_PER_BLOCK;
	min_z = (world_ptr->rows - row - 1) * UNITS_PER_BLOCK;
	max_x = min_x + UNITS_PER_BLOCK;
	max_y = min_y + UNITS_PER_BLOCK;
	max_z = min_z + UNITS_PER_BLOCK;

	// Create the corner vertices of the bounding box, and transform them.

	bbox[0].x = min_x;
	bbox[0].y = min_y;
	bbox[0].z = min_z;
	transform_vertex(&bbox[0], &tbbox[0]);

	bbox[1].x = max_x;
	bbox[1].y = min_y;
	bbox[1].z = min_z;
	transform_vertex(&bbox[1], &tbbox[1]);

	bbox[2].x = max_x;
	bbox[2].y = max_y;
	bbox[2].z = min_z;
	transform_vertex(&bbox[2], &tbbox[2]);

	bbox[3].x = min_x;
	bbox[3].y = max_y;
	bbox[3].z = min_z;
	transform_vertex(&bbox[3], &tbbox[3]);

	bbox[4].x = min_x;
	bbox[4].y = min_y;
	bbox[4].z = max_z;
	transform_vertex(&bbox[4], &tbbox[4]);

	bbox[5].x = max_x;
	bbox[5].y = min_y;
	bbox[5].z = max_z;
	transform_vertex(&bbox[5], &tbbox[5]);

	bbox[6].x = max_x;
	bbox[6].y = max_y;
	bbox[6].z = max_z;
	transform_vertex(&bbox[6], &tbbox[6]);

	bbox[7].x = min_x;
	bbox[7].y = max_y;
	bbox[7].z = max_z;
	transform_vertex(&bbox[7], &tbbox[7]);

	// Now create the 24 vertices used to draw each line of the cube.

	vertices[0] = tbbox[0]; vertices[1] = tbbox[1];
	vertices[2] = tbbox[1]; vertices[3] = tbbox[2];
	vertices[4] = tbbox[2]; vertices[5] = tbbox[3];
	vertices[6] = tbbox[3]; vertices[7] = tbbox[0];

	vertices[8] = tbbox[4]; vertices[9] = tbbox[5];
	vertices[10] = tbbox[5]; vertices[11] = tbbox[6];
	vertices[12] = tbbox[6]; vertices[13] = tbbox[7];
	vertices[14] = tbbox[7]; vertices[15] = tbbox[4];

	vertices[16] = tbbox[0]; vertices[17] = tbbox[4];
	vertices[18] = tbbox[1]; vertices[19] = tbbox[5];
	vertices[20] = tbbox[2]; vertices[21] = tbbox[6];
	vertices[22] = tbbox[3]; vertices[23] = tbbox[7];

	// Draw the lines.

	if (hardware_acceleration) {
		RGBcolour white;
		white.set_RGB(1.0f, 1.0f, 1.0f, 1.0f);
		hardware_render_lines(vertices, 24, white);
	}
}

//------------------------------------------------------------------------------
// Render a block on the given square (which may be NULL if the block is
// movable).
//------------------------------------------------------------------------------

static void
render_block(square *square_ptr, block *block_ptr, bool movable)
{
	block_def *block_def_ptr;
	vertex temp_vertex;
	vertex centre(UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK, 
		UNITS_PER_HALF_BLOCK);

	// If the block is outside the frustum, ignore it.

	if ((curr_block_visibility = compare_block_against_frustum(block_ptr)) ==
		OUTSIDE_FRUSTUM)
		return;

	// Remember the square and block pointers, and whether the block is movable.

	curr_square_ptr = square_ptr;
	curr_block_ptr = block_ptr;
	curr_block_movable = movable;

	// Get a pointer to the block definition and remember it's type.

	block_def_ptr = curr_block_ptr->block_def_ptr;
	curr_block_type = block_def_ptr->type;

	// Save the block translation in a global variable, and compute the
	// camera position relative to this.

	block_translation = curr_block_ptr->translation;
	relative_camera_position = camera_position;
	relative_camera_position -= block_translation;

	// Determine the centre of the block.

	block_centre.x = block_translation.x + UNITS_PER_HALF_BLOCK;
	block_centre.y = block_translation.y + UNITS_PER_HALF_BLOCK;
	block_centre.z = block_translation.z + UNITS_PER_HALF_BLOCK;

	// Locate the active lights closest to the centre of the block. It would be
	// more accurate to do this for every polygon, but that increases 
	// processing time enormously.

	if (block_ptr->set_active_lights) {
		set_active_lights(block_ptr->active_light_list, &block_centre);
		block_ptr->set_active_lights = false;
	}

	// If the block is a sprite...

	if (curr_block_type & SPRITE_BLOCK) {
		polygon *polygon_ptr;
		vector line_of_sight;
		float sprite_angle;
		
		// Get a pointer to the sprite polygon.

		polygon_ptr = curr_block_ptr->polygon_list;

		// Compute the angle that the sprite is facing based upon it's type.

		switch (curr_block_type) {
		case MULTIFACETED_SPRITE:
		case FACING_SPRITE:

			// The sprite angle is the player turn angle rotated by 180 degrees.

			sprite_angle = pos_adjust_angle(player_viewpoint.turn_angle +
				180.0f);
			break;
		
		case REVOLVING_SPRITE:

			// The sprite angle must be updated according to the elapsed time
			// and the rotational speed for a revolving sprite.  If delta angle
			// is greater than 45 degrees, clamp it so that the direction of
			// motion is obvious.

			if (block_def_ptr->degrees_per_ms > 0) {
				float delta_angle = clamp_angle((curr_time_ms - 
					curr_block_ptr->last_time_ms) * 
					block_def_ptr->degrees_per_ms, -45.0f, 45.0f);
				curr_block_ptr->sprite_angle = 
					pos_adjust_angle(curr_block_ptr->sprite_angle + delta_angle);
				curr_block_ptr->last_time_ms = curr_time_ms;
			}
			sprite_angle = (float)curr_block_ptr->sprite_angle;
			break;

		case ANGLED_SPRITE:

			// The sprite angle remains constant for an angled sprite.

			sprite_angle = curr_block_ptr->sprite_angle;
		}

		// If this is a multifaceted sprite and it has a texture, compute the
		// mipmap index based upon a line of sight from the player viewpoint
		// to the sprite centre.

		if (curr_block_type == MULTIFACETED_SPRITE) {
			polygon_def *polygon_def_ptr = polygon_ptr->polygon_def_ptr;
			part *part_ptr = polygon_def_ptr->part_ptr;
			texture *texture_ptr = part_ptr->texture_ptr;
			if (texture_ptr != NULL) {
				float angle, angle_range;

				// Generate a vector from the centre of the block to the 
				// player's position. 

				line_of_sight = player_viewpoint.position - block_centre; 
				line_of_sight.normalise();
			
				// Compute the angle of this vector in the X-Z plane.
				
				if (FEQ(line_of_sight.dx, 0.0f)) {
					if (FGT(line_of_sight.dz, 0.0f))
						angle = 0.0f;
					else
						angle = 180.0f;
				} else {
					angle = (float)DEG(atan(line_of_sight.dz / 
						line_of_sight.dx));
					if (FGT(line_of_sight.dx, 0.0f))
						angle = 90.0f - angle;
					else
						angle = 270.0f - angle;
				}

				// Calculate the pixmap index based upon this angle.

				angle_range = 360.0f / texture_ptr->pixmaps;
				curr_block_ptr->pixmap_index = (int)(angle / angle_range);
			}
		}

		// Rotate each vertex around the block's centre by the sprite angle,
		// then transform each vertex by the player position and 
		// orientation, storing them in a global list.

		for (int vertex_no = 0; vertex_no < curr_block_ptr->vertices; 
			vertex_no++) {
			temp_vertex = curr_block_ptr->vertex_list[vertex_no];
			temp_vertex -= centre;
			temp_vertex.rotate_y(sprite_angle);
			temp_vertex += block_centre;
			transform_vertex(&temp_vertex, &block_tvertex_list[vertex_no]);
		}

		// Render the sprite polygon.

		render_polygon(polygon_ptr, sprite_angle);
	}
	
	// If the block is not a sprite...
	
	else {

		// Transform the vertices by the block's position, then the player's
		// position, turn angle and look angle, storing them in a global list.

		for (int vertex_no = 0; vertex_no < curr_block_ptr->vertices; 
			vertex_no++) {
			temp_vertex = curr_block_ptr->vertex_list[vertex_no] +
				curr_block_ptr->translation;
			transform_vertex(&temp_vertex, &block_tvertex_list[vertex_no]);
		}

		// If the block is not movable and has a BSP tree, traverse it to 
		// render the polygons in front-to-back order.  Otherwise render the 
		// active polygons in order of appearance (NOTE: if the block is not
		// marked as movable, or has transparent or translucent polygons, this
		// may result in gross sorting errors).  However, we want to be able to
		// manipulate the vertices of movable blocks, and that will mess up any
		// BSP tree.

		if (!block_def_ptr->movable && block_def_ptr->BSP_tree != NULL)
			render_polygons_in_block(block_def_ptr->BSP_tree);
		else {
			int polygon_no;
			polygon *polygon_ptr;
			
			for (polygon_no = 0; polygon_no < curr_block_ptr->polygons;
				polygon_no++) {
				polygon_ptr = &curr_block_ptr->polygon_list[polygon_no];
				render_polygon(polygon_ptr, 0.0f);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Render the block on the given square.
//------------------------------------------------------------------------------

static void
render_block_on_square(int column, int row, int level)
{
	square *square_ptr;
	block *block_ptr;

	// Get a pointer to the square and it's block.  If the location is invalid
	// or there is no block on this square, there is nothing to render.

	if ((square_ptr = world_ptr->get_square_ptr(column, row, level)) == NULL ||
		(block_ptr = square_ptr->block_ptr) == NULL)
		return;

	// Now render the block, which is not movable.

	render_block(square_ptr, block_ptr, false);
}

//------------------------------------------------------------------------------
// Render the specified range of blocks on the map in an implicit BSP order.
//------------------------------------------------------------------------------

static void
render_blocks_on_map(int min_column, int min_row, int min_level,
					 int max_column, int max_row, int max_level)
{
	int column, row, level;

	// Get the map position the camera is in.

	camera_position.get_map_position(&camera_column, &camera_row, &camera_level);

	// Traverse the blocks in an implicit BSP order: first the columns to the
	// right and left of the camera, then the rows to the south and north of
	// the camera, then the levels above and below the camera.  The camera block
	// is rendered first.

	for (column = camera_column; column <= max_column; column++) {
		for (row = camera_row; row <= max_row; row++) {
			for (level = camera_level; level <= max_level; level++)
				render_block_on_square(column, row, level);
			for (level = camera_level - 1; level >= min_level; level--)
				render_block_on_square(column, row, level);
		}
		for (row = camera_row - 1; row >= min_row; row--) {
			for (level = camera_level; level <= max_level; level++)
				render_block_on_square(column, row, level);
			for (level = camera_level - 1; level >= min_level; level--)
				render_block_on_square(column, row, level);
		}
	}
	for (column = camera_column - 1; column >= min_column; column--) {
		for (row = camera_row; row <= max_row; row++) {
			for (level = camera_level; level <= max_level; level++)
				render_block_on_square(column, row, level);
			for (level = camera_level - 1; level >= min_level; level--)
				render_block_on_square(column, row, level);
		}
		for (row = camera_row - 1; row >= min_row; row--) {
			for (level = camera_level; level <= max_level; level++)
				render_block_on_square(column, row, level);
			for (level = camera_level - 1; level >= min_level; level--)
				render_block_on_square(column, row, level);
		}
	}
}

//------------------------------------------------------------------------------
// Render the player block.
//------------------------------------------------------------------------------

static void
render_player_block(void)
{
	vertex centre(UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK,
		UNITS_PER_HALF_BLOCK);

	// Determine the translation and centre of the block.  We need to compensate
	// for the fact that the player block needs to be centered on the player
	// position in the X and Z direction, and placed at ground level in the Y
	// direction.

	block_translation.x = player_viewpoint.position.x - UNITS_PER_HALF_BLOCK;
	block_translation.y = player_viewpoint.position.y - player_dimensions.y;
	block_translation.z = player_viewpoint.position.z - UNITS_PER_HALF_BLOCK;
	block_centre = block_translation + centre;

	// Set the active lights for the player block.

	if (player_block_ptr->set_active_lights) {
		set_active_lights(player_block_ptr->active_light_list, 
			&player_viewpoint.position);
		player_block_ptr->set_active_lights = false;
	}
	
	// Render the player block.

	player_block_ptr->translation = block_translation;
	render_block(NULL, player_block_ptr, true);
}

//------------------------------------------------------------------------------
// Compute the view bounding box.
//------------------------------------------------------------------------------

static void
compute_view_bounding_box(void)
{
	int index;
	vertex frustum_tvertex_list[FRUSTUM_VERTICES];
	vertex frustum_vertex;
	
	// Transform the frustum vertices from view space to world space.

	for (index = 0; index < FRUSTUM_VERTICES; index++) {
		frustum_vertex = frustum_vertex_list[index];
		frustum_vertex += player_camera_offset;
		frustum_vertex.rotate_x(player_viewpoint.look_angle);
		frustum_vertex.rotate_y(player_viewpoint.turn_angle);
		frustum_vertex += player_viewpoint.position;
		frustum_tvertex_list[index] = frustum_vertex;
	}

	// Initialise the minimum and maximum view coordinates to the first frustum
	// vertex.
 
	min_view = frustum_tvertex_list[0];
	max_view = frustum_tvertex_list[0];

	// Step through the remaining frustum vertices and remember the minimum and
	// maximum coordinates.

	for (index = 1; index < FRUSTUM_VERTICES; index++) {
		frustum_vertex = frustum_tvertex_list[index];
		if (frustum_vertex.x < min_view.x)
			min_view.x = frustum_vertex.x;
		else if (frustum_vertex.x > max_view.x)
			max_view.x = frustum_vertex.x;
		if (frustum_vertex.y < min_view.y)
			min_view.y = frustum_vertex.y;
		else if (frustum_vertex.y > max_view.y)
			max_view.y = frustum_vertex.y;
		if (frustum_vertex.z < min_view.z)
			min_view.z = frustum_vertex.z;
		else if (frustum_vertex.z > max_view.z)
			max_view.z = frustum_vertex.z;
	}
}

//------------------------------------------------------------------------------
// Render the blocks on the map that intersect with the view
//------------------------------------------------------------------------------

static void
render_map(void)
{
	int min_column, min_row, min_level;
	int max_column, max_row, max_level;

	// Turn the coordinates into minimum and maximum map positions, then render
	// the blocks in this range.

	min_view.get_map_position(&min_column, &max_row, &min_level);
	max_view.get_map_position(&max_column, &min_row, &max_level);
	render_blocks_on_map(min_column, min_row, min_level, 
		max_column, max_row, max_level);
}

//------------------------------------------------------------------------------
// Render all movable blocks.
//------------------------------------------------------------------------------

static void
render_movable_blocks(void)
{
	block *block_ptr = movable_block_list;
	while (block_ptr != NULL) {
		render_block(NULL, block_ptr, true);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Render a popup texture.
//------------------------------------------------------------------------------

static void
render_popup_texture(popup *popup_ptr, pixmap *pixmap_ptr)
{
	// If hardware acceleration is enabled, render the popup as a 2D polygon.

	if (hardware_acceleration) {
		RGBcolour dummy_colour;
		float one_on_dimensions = one_on_dimensions_list[pixmap_ptr->size_index];
		float u = (float)pixmap_ptr->width * one_on_dimensions;
		float v = (float)pixmap_ptr->height * one_on_dimensions;
		hardware_render_2D_polygon(pixmap_ptr, dummy_colour, get_brightness(popup_ptr->brightness_index),
			(float)popup_ptr->sx, (float)popup_ptr->sy, (float)pixmap_ptr->width, (float)pixmap_ptr->height, 0.0f, 0.0f, u, v);
	}

	// If not using hardware acceleration, add the popup image to the span
	// buffer one row at a time.

	else {
		int width, height;
		int left_offset, top_offset;
		edge left_edge, right_edge;

		// If the screen x or y coordinates are negative, then we clamp 
		// them at zero and adjust the image offsets and size to match.

		if (popup_ptr->sx < 0) {
			left_offset = -popup_ptr->sx;
			width = pixmap_ptr->width - left_offset;
			popup_ptr->sx = 0;
		} else {
			left_offset = 0;
			width = pixmap_ptr->width;
		}	
		if (popup_ptr->sy < 0) {
			top_offset = -popup_ptr->sy;
			height = pixmap_ptr->height - top_offset;
			popup_ptr->sy = 0;
		} else {
			top_offset = 0;
			height = pixmap_ptr->height;
		}

		// If the popup overlaps the right or bottom edge, clip it.

		if (popup_ptr->sx + width > window_width)
			width = window_width - popup_ptr->sx;
		if (popup_ptr->sy + height > window_height)
			height = window_height - popup_ptr->sy;

		// Initialise the fields of the popup's left and right edge
		// that do not change.

		left_edge.sx = (float)popup_ptr->sx;
		left_edge.one_on_tz = 1.0;
		left_edge.u_on_tz = (float)left_offset;
		right_edge.sx = (float)(popup_ptr->sx + width);
		right_edge.one_on_tz = 1.0;
		right_edge.u_on_tz = (float)(left_offset + width);

		// Fill each span buffer row with a popup span.

		for (int row = 0; row < height; row++) {

			// Set up the left and right edge of the popup for this row,
			// then add the popup span to the span buffer.

			left_edge.v_on_tz = (float)(top_offset + row);
			right_edge.v_on_tz = (float)(top_offset + row);
			add_span(popup_ptr->sy + row, &left_edge, &right_edge, 
				pixmap_ptr, 0, popup_ptr->brightness_index, true);
		}
	}
}

//------------------------------------------------------------------------------
// Add all visible popups from the given popup list to the visible popup list.
// If hardware acceleration is enabled, this list needs to be in back to front
// order; otherwise it needs to be in front to back order.
//------------------------------------------------------------------------------

static void
add_visible_popups_in_popup_list(popup *popup_list, vertex *translation_ptr)
{
	popup *popup_ptr;
	vertex popup_position;
	bool visible_by_proximity;
	texture *bg_texture_ptr, *fg_texture_ptr;
	int popup_width, popup_height;
	bool popup_displayable;

	// Step through each popup in the given popup list, determining whether
	// the player moved into into the trigger radius of those with a proximity
	// trigger, and adding those that are current visible by proximity or 
	// rollover to the visible popup list.

	popup_ptr = popup_list;
	while (popup_ptr != NULL) {

		// Determine the size of the popup.  If this is not possible because
		// the popup has neither a background nor foreground texture, skip it.

		bg_texture_ptr = popup_ptr->bg_texture_ptr;
		fg_texture_ptr = popup_ptr->fg_texture_ptr;
		if (bg_texture_ptr != NULL) {
			popup_width = bg_texture_ptr->width;
			popup_height = bg_texture_ptr->height;
		} else if (fg_texture_ptr != NULL) {
			popup_width = fg_texture_ptr->width;
			popup_height = fg_texture_ptr->height;
		} else {
			popup_ptr = popup_ptr->next_popup_ptr;
			continue;
		}

		// Determine whether or not this popup is displayable.

		if ((bg_texture_ptr == NULL || bg_texture_ptr->pixmap_list != NULL) &&
			(fg_texture_ptr == NULL || fg_texture_ptr->pixmap_list != NULL))
			popup_displayable = true;
		else
			popup_displayable = false;

		// If this popup is displayable, check whether it's visible by 
		// proximity.

		visible_by_proximity = false;
		if (popup_displayable) {

			// If this popup is always visible, treat it as currently visible
			// by proximity.

			if (popup_ptr->always_visible ||
				(popup_ptr->trigger_flags & EVERYWHERE) != 0)
				visible_by_proximity = true;

			// Otherwise, if this popup has the proximity trigger set, check
			// whether the player viewpoint is within the radius of the popup.

			else if ((popup_ptr->trigger_flags & PROXIMITY) != 0) {
				vector distance;
				float distance_squared;

				// Determine the distance squared from the popup's position to
				// the player's position.  If the translation vertex is
				// specified, we must factor this into the equation.

				if (translation_ptr != NULL)
					popup_position = *translation_ptr + popup_ptr->position;
				else
					popup_position = popup_ptr->position;
				distance = popup_position - player_viewpoint.position;
				distance_squared = distance.dx * distance.dx + 
					distance.dy * distance.dy + distance.dz * distance.dz;

				// If the player is on the same level as the popup, and the
				// distance squared is within the trigger radius squared, then
				// make the popup visible by proximity.

				if (player_viewpoint.position.get_map_level() ==
					popup_position.get_map_level() &&
					distance_squared <= popup_ptr->radius_squared) {
					visible_by_proximity = true;
				}
			}
		}

		// If the popup was not previously visible and has become visible by
		// proximity, set the time that this popup was made visible, and if the
		// window alignment is by mouse then set the position of the popup.

		if (popup_ptr->visible_flags == 0 && visible_by_proximity) {
			popup_ptr->start_time_ms = curr_time_ms;
			if (popup_ptr->window_alignment == MOUSE) {
				popup_ptr->sx = mouse_x - popup_width / 2;
				popup_ptr->sy = mouse_y - popup_height / 2;
			}
		}

		// Set or reset the popup's visible by proximity flag.

		if (visible_by_proximity)
			popup_ptr->visible_flags |= PROXIMITY;
		else
			popup_ptr->visible_flags &= ~PROXIMITY;

		// If the popup is visible by proximity or rollover, and it is
		// displayable, then add it to the visible popup list...

		if (popup_ptr->visible_flags != 0 && popup_displayable) {

			// Set the pointer to the current background pixmap if there is a
			// background texture.  Non-looping textures begin animating from
			// the time the popup becomes visible.

			if (bg_texture_ptr != NULL && bg_texture_ptr->pixmap_list != NULL) {
				if (bg_texture_ptr->loops)
					popup_ptr->bg_pixmap_ptr = 
						bg_texture_ptr->get_curr_pixmap_ptr(curr_time_ms - 
						start_time_ms);
				else
					popup_ptr->bg_pixmap_ptr = 
						bg_texture_ptr->get_curr_pixmap_ptr(curr_time_ms - 
						popup_ptr->start_time_ms);
			}

			// Compute the screen x and y coordinates of the popup according
			// to the window alignment mode and the size of the popup.
			// These coordinates may be negative.
			
			switch (popup_ptr->window_alignment) {
			case TOP_LEFT:
			case LEFT:
			case BOTTOM_LEFT:
				popup_ptr->sx = 0;
				break;
			case TOP:
			case CENTRE:
			case BOTTOM:
				popup_ptr->sx = (window_width - popup_width) / 2;
				break;
			case TOP_RIGHT:
			case RIGHT:
			case BOTTOM_RIGHT:
				popup_ptr->sx = window_width - popup_width;
			}
			switch (popup_ptr->window_alignment) {
			case TOP_LEFT:
			case TOP:
			case TOP_RIGHT:
				popup_ptr->sy = 0;
				break;
			case LEFT:
			case CENTRE:
			case RIGHT:
				popup_ptr->sy = (window_height - popup_height) / 2;
				break;
			case BOTTOM_LEFT:
			case BOTTOM:
			case BOTTOM_RIGHT:
				popup_ptr->sy = window_height - popup_height;
			}
			
			// If a mouse selection hasn't been found yet, check whether
			// this popup has been selected.  If the popup has no imagemap
			// and is transparent, ignore it.

			if (!found_selection && 
				(popup_ptr->imagemap_ptr || 
				 (popup_ptr->bg_texture_ptr && 
				 !popup_ptr->bg_texture_ptr->transparent) ||
				 !popup_ptr->transparent_background) &&
				check_for_popup_selection(popup_ptr, popup_width, 
				popup_height)) {
				found_selection = true;
				curr_popup_ptr = popup_ptr;
			}

			// If hardware acceleration is enabled, add this popup to the tail
			// of the popup list, otherwise add it to the head.

			if (hardware_acceleration) {
				if (last_visible_popup_ptr != NULL)
					last_visible_popup_ptr->next_visible_popup_ptr = popup_ptr;
				else
					visible_popup_list = popup_ptr;
				popup_ptr->next_visible_popup_ptr = NULL;
				last_visible_popup_ptr = popup_ptr;
			} else {
				popup_ptr->next_visible_popup_ptr = visible_popup_list;
				visible_popup_list = popup_ptr;
			}
		}

		// Move onto the next popup in the list.

		popup_ptr = popup_ptr->next_popup_ptr;
	}
}

//------------------------------------------------------------------------------
// Add all visible popups from the given block list to the visible popup list.
//------------------------------------------------------------------------------

static void
add_visible_popups_in_block_list(block *block_list)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->popup_list != NULL)
			add_visible_popups_in_popup_list(block_ptr->popup_list,
				&block_ptr->translation);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Render the orb.
//------------------------------------------------------------------------------

static void
render_orb(void)
{
	pixmap *pixmap_ptr;
	direction relative_orb_direction;
	direction delta_direction;
	float one_on_dimensions;
	float width_scale, height_scale;
	float left_offset, top_offset;
	edge left_edge, right_edge;
	RGBcolour dummy_colour;

	// If there is no orb texture loaded, then there is nothing to render.

	if (orb_texture_ptr == NULL)
		return;

	// Get a pointer to the orb pixmap.

	pixmap_ptr = orb_texture_ptr->get_curr_pixmap_ptr(curr_time_ms - 
		start_time_ms);

	// Adjust the X angle of the orb direction so that it's between -180 and 
	// 179, and make sure the Y angle is between 0 and 359.

	relative_orb_direction.angle_x = neg_adjust_angle(orb_direction.angle_x);
	relative_orb_direction.angle_y = pos_adjust_angle(orb_direction.angle_y);

	// If the X angle is less than -90 or greater than 90, then the orb is
	// now over the opposite side of the sky, so switch the Y angle and
	// adjust the X angle approapiately.

	if (FLT(relative_orb_direction.angle_x, -90.0f)) {
		relative_orb_direction.angle_x = -180.0f - 
			relative_orb_direction.angle_x;
		relative_orb_direction.angle_y = 
			pos_adjust_angle(relative_orb_direction.angle_y + 180.0f);
	} else if (FGT(relative_orb_direction.angle_x, 90.0f)) {
		relative_orb_direction.angle_x = 180.0f - 
			relative_orb_direction.angle_x;
		relative_orb_direction.angle_y = 
			pos_adjust_angle(relative_orb_direction.angle_y + 180.0f);
	}

	// Adjust the orb direction so that it's relative to a player at direction
	// (0,0).

	relative_orb_direction.angle_x = pos_adjust_angle(
		pos_adjust_angle(relative_orb_direction.angle_x) -
		pos_adjust_angle(-player_viewpoint.look_angle));
	relative_orb_direction.angle_y = pos_adjust_angle(
		pos_adjust_angle(relative_orb_direction.angle_y) -
		pos_adjust_angle(player_viewpoint.turn_angle));

	// If the relative orb direction is greater than 180.0 along either axis,
	// compute the opposite (negative) angle.

	if (FGT(relative_orb_direction.angle_x, 180.0f))
		relative_orb_direction.angle_x -= 360.0f;
	if (FGT(relative_orb_direction.angle_y, 180.0f))
		relative_orb_direction.angle_y -= 360.0f;

	// Now calculate the 2D position of the orb in the window.

	curr_orb_x = half_window_width + relative_orb_direction.angle_y * 
		horz_pixels_per_degree - half_orb_width;
	curr_orb_y = half_window_height - relative_orb_direction.angle_x * 
		vert_pixels_per_degree - half_orb_height;

	// If the orb is complete outside of the window, don't render it.

	if (FLT(curr_orb_x + orb_width, 0.0f) || FGE(curr_orb_x, window_width) ||
		FLT(curr_orb_y + orb_height, 0.0f) || FGE(curr_orb_y, window_height))
		return;

	// If the screen x or y coordinates are negative, then we clamp 
	// them at zero and adjust the image offsets and size to match.

	if (curr_orb_x < 0.0f) {
		left_offset = -curr_orb_x;
		curr_orb_width = orb_width - left_offset;
		curr_orb_x = 0.0f;
	} else {
		left_offset = 0.0f;
		curr_orb_width = orb_width;
	}
	if (curr_orb_y < 0.0f) {
		top_offset = -curr_orb_y;
		curr_orb_height = orb_height - top_offset;
		curr_orb_y = 0.0f;
	} else {
		top_offset = 0.0f;
		curr_orb_height = orb_height;
	}

	// If the texture overlaps the right or bottom edge, clip it.

	if (curr_orb_x + curr_orb_width > (float)window_width)
		curr_orb_width = (float)window_width - curr_orb_x;
	if (curr_orb_y + curr_orb_height > (float)window_height)
		curr_orb_height = (float)window_height - curr_orb_y;

	// Calculating the scaling factor for the size of the orb.

	width_scale = (float)pixmap_ptr->width / orb_width;
	height_scale = (float)pixmap_ptr->height / orb_height;

	// If using hardware acceleration, render the orb as a 2D polygon.

	if (hardware_acceleration) {
		one_on_dimensions = one_on_dimensions_list[pixmap_ptr->size_index];
		width_scale = (float)pixmap_ptr->width * one_on_dimensions;
		height_scale = (float)pixmap_ptr->height * one_on_dimensions;
		hardware_render_2D_polygon(pixmap_ptr, dummy_colour, orb_brightness,
			curr_orb_x, curr_orb_y, curr_orb_width, curr_orb_height,
			left_offset / orb_width * width_scale, 
			top_offset / orb_height * height_scale, 
			(left_offset + curr_orb_width) / orb_width * width_scale, 
			(top_offset + curr_orb_height) / orb_height * height_scale);
	}

	// If not using hardware acceleration, render the orb as a 2D scaled
	// texture in software...

	else {

		// Initialise the fields of the left and right edge that do not change.

		left_edge.sx = curr_orb_x;
		left_edge.one_on_tz = 1.0f;
		left_edge.u_on_tz = left_offset * width_scale;
		right_edge.sx = curr_orb_x + curr_orb_width;
		right_edge.one_on_tz = 1.0f;
		right_edge.u_on_tz = (left_offset + curr_orb_width) * width_scale;

		// Fill each span buffer row with an image span.

		for (int row = 0; row < curr_orb_height; row++) {
			left_edge.v_on_tz = (top_offset + row) * height_scale;
			right_edge.v_on_tz = (top_offset + row) * height_scale;
			add_span((int)(curr_orb_y + row), &left_edge, &right_edge, 
				pixmap_ptr, 0, orb_brightness_index, false);
		}
	}
}

//------------------------------------------------------------------------------
// Render the screen polygons that use the given texture, using hardware
// acceleration.
//------------------------------------------------------------------------------

static void
render_textured_polygons(texture *texture_ptr)
{
	// Step through each pixmap in this texture...

	for (int pixmap_no = 0; pixmap_no < texture_ptr->pixmaps; pixmap_no++) {
		pixmap *pixmap_ptr = &texture_ptr->pixmap_list[pixmap_no];

		// Render all the transformed polygons in the pixmap via hardware, removing them from the pixmap as we go.

		tpolygon *tpolygon_ptr = pixmap_ptr->tpolygon_list;
		while (tpolygon_ptr != NULL) {
			hardware_render_polygon(tpolygon_ptr);
			tpolygon_ptr = del_tpolygon(tpolygon_ptr);
		}
		pixmap_ptr->tpolygon_list = NULL;
	}
}

//------------------------------------------------------------------------------
// Render the spans that use the given texture to a 16-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_textured_spans16(texture *texture_ptr)
{
	int pixmap_no;
	pixmap *pixmap_ptr;
	int index;
	span *span_ptr;

	// Step through each pixmap in this texture...

	for (pixmap_no = 0; pixmap_no < texture_ptr->pixmaps; pixmap_no++) {
		pixmap_ptr = &texture_ptr->pixmap_list[pixmap_no];

		// Step through each span list in this pixmap, and render them as 
		// linear or opaque depending on whether it belongs to a popup or
		// not.

		for (index = 0; index < BRIGHTNESS_LEVELS; index++) {
			span_ptr = pixmap_ptr->span_lists[index];
			while (span_ptr != NULL) {
				if (span_ptr->is_popup)
					render_popup_span16(span_ptr);
				else
					render_transparent_span16(span_ptr);
				span_ptr = del_span(span_ptr);
			}
			pixmap_ptr->span_lists[index] = NULL;
		}
	}
}

//------------------------------------------------------------------------------
// Render the spans that use the given texture to a 24-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_textured_spans24(texture *texture_ptr)
{
	int pixmap_no;
	pixmap *pixmap_ptr;
	int index;
	span *span_ptr;

	// Step through each pixmap in this texture...

	for (pixmap_no = 0; pixmap_no < texture_ptr->pixmaps; pixmap_no++) {
		pixmap_ptr = &texture_ptr->pixmap_list[pixmap_no];

		// Step through each span list in this pixmap, and render them as 
		// linear or opaque depending on whether it belongs to a popup or
		// not.

		for (index = 0; index < BRIGHTNESS_LEVELS; index++) {
			span_ptr = pixmap_ptr->span_lists[index];
			while (span_ptr != NULL) {
				if (span_ptr->is_popup)
					render_popup_span24(span_ptr);
				else
					render_transparent_span24(span_ptr);
				span_ptr = del_span(span_ptr);
			}
			pixmap_ptr->span_lists[index] = NULL;
		}
	}
}

//------------------------------------------------------------------------------
// Render the spans that use the given texture to a 32-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_textured_spans32(texture *texture_ptr)
{
	int pixmap_no;
	pixmap *pixmap_ptr;
	int index;
	span *span_ptr;

	// Step through each pixmap in this texture...

	for (pixmap_no = 0; pixmap_no < texture_ptr->pixmaps; pixmap_no++) {
		pixmap_ptr = &texture_ptr->pixmap_list[pixmap_no];

		// Step through each span list in this pixmap, and render them as 
		// linear or opaque depending on whether it belongs to a popup or
		// not.

		for (index = 0; index < BRIGHTNESS_LEVELS; index++) {
			span_ptr = pixmap_ptr->span_lists[index];
			while (span_ptr != NULL) {
				if (span_ptr->is_popup)
					render_popup_span32(span_ptr);
				else
					render_transparent_span32(span_ptr);
				span_ptr = del_span(span_ptr);
			}
			pixmap_ptr->span_lists[index] = NULL;
		}
	}
}

//------------------------------------------------------------------------------
// Render the screen polygons or spans for this frame.
//------------------------------------------------------------------------------

static void
render_textured_polygons_or_spans(void)
{
	blockset *blockset_ptr;
	texture *texture_ptr;
#ifdef STREAMING_MEDIA
	video_texture *scaled_video_texture_ptr;
#endif

	// Handle hardware accelerated rendering...

	if (hardware_acceleration) {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			texture_ptr = blockset_ptr->first_texture_ptr;
			while (texture_ptr != NULL) {
				render_textured_polygons(texture_ptr);
				texture_ptr = texture_ptr->next_texture_ptr;
			}
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
		texture_ptr = custom_blockset_ptr->first_texture_ptr;
		while (texture_ptr != NULL) {
			render_textured_polygons(texture_ptr);
			texture_ptr = texture_ptr->next_texture_ptr;
		}

#ifdef STREAMING_MEDIA

		if (unscaled_video_texture_ptr != NULL)
			render_textured_polygons(unscaled_video_texture_ptr);
		scaled_video_texture_ptr = scaled_video_texture_list;
		while (scaled_video_texture_ptr != NULL) {
			render_textured_polygons(scaled_video_texture_ptr->texture_ptr);
			scaled_video_texture_ptr = 
				scaled_video_texture_ptr->next_video_texture_ptr;
		}
#endif

	} 
	
	// Handle software rendering to a 16-bit frame buffer...

	else if (display_depth <= 16) {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			texture_ptr = blockset_ptr->first_texture_ptr;
			while (texture_ptr != NULL) {
				render_textured_spans16(texture_ptr);
				texture_ptr = texture_ptr->next_texture_ptr;
			}
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
		texture_ptr = custom_blockset_ptr->first_texture_ptr;
		while (texture_ptr != NULL) {
			render_textured_spans16(texture_ptr);
			texture_ptr = texture_ptr->next_texture_ptr;
		}

#ifdef STREAMING_MEDIA

		if (unscaled_video_texture_ptr != NULL)
			render_textured_spans16(unscaled_video_texture_ptr);
		scaled_video_texture_ptr = scaled_video_texture_list;
		while (scaled_video_texture_ptr != NULL) {
			render_textured_spans16(scaled_video_texture_ptr->texture_ptr);
			scaled_video_texture_ptr = 
				scaled_video_texture_ptr->next_video_texture_ptr;
		}

#endif

	} 
	
	// Handle software rendering to a 24-bit frame buffer...

	else if (display_depth == 24) {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			texture_ptr = blockset_ptr->first_texture_ptr;
			while (texture_ptr != NULL) {
				render_textured_spans24(texture_ptr);
				texture_ptr = texture_ptr->next_texture_ptr;
			}
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
		texture_ptr = custom_blockset_ptr->first_texture_ptr;
		while (texture_ptr != NULL) {
			render_textured_spans24(texture_ptr);
			texture_ptr = texture_ptr->next_texture_ptr;
		}

#ifdef STREAMING_MEDIA

		if (unscaled_video_texture_ptr != NULL)
			render_textured_spans24(unscaled_video_texture_ptr);
		scaled_video_texture_ptr = scaled_video_texture_list;
		while (scaled_video_texture_ptr != NULL) {
			render_textured_spans24(scaled_video_texture_ptr->texture_ptr);
			scaled_video_texture_ptr = 
				scaled_video_texture_ptr->next_video_texture_ptr;
		}

#endif

	}  
	
	// Handle software rendering to a 32-bit frame buffer...

	else {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			texture_ptr = blockset_ptr->first_texture_ptr;
			while (texture_ptr != NULL) {
				render_textured_spans32(texture_ptr);
				texture_ptr = texture_ptr->next_texture_ptr;
			}
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
		texture_ptr = custom_blockset_ptr->first_texture_ptr;
		while (texture_ptr != NULL) {
			render_textured_spans32(texture_ptr);
			texture_ptr = texture_ptr->next_texture_ptr;
		}

#ifdef STREAMING_MEDIA

		if (unscaled_video_texture_ptr != NULL)
			render_textured_spans32(unscaled_video_texture_ptr);
		scaled_video_texture_ptr = scaled_video_texture_list;
		while (scaled_video_texture_ptr != NULL) {
			render_textured_spans32(scaled_video_texture_ptr->texture_ptr);
			scaled_video_texture_ptr = 
				scaled_video_texture_ptr->next_video_texture_ptr;
		}

#endif

	}
}

//------------------------------------------------------------------------------
// Render the screen polygons/spans for this frame that have a solid colour.
//------------------------------------------------------------------------------

static void
render_colour_polygons_or_spans(void)
{
	// If using hardware acceleration, render the transformed polygons in the solid colour transformed polygon list via hardware,
	// removing them from the list in the process.

	if (hardware_acceleration) {
		tpolygon *tpolygon_ptr = colour_tpolygon_list;
		while (tpolygon_ptr != NULL) {
			hardware_render_polygon(tpolygon_ptr);
			tpolygon_ptr = del_tpolygon(tpolygon_ptr);
		}
		colour_tpolygon_list = NULL;
	}

	// If not using OpenGL or hardware acceleration, render the solid colour
	// spans in software.

	else if (display_depth <= 16) {
		span *span_ptr = colour_span_list;
		while (span_ptr != NULL) {
			render_colour_span16(span_ptr);
			span_ptr = del_span(span_ptr);
		}
	} else if (display_depth == 24) {
		span *span_ptr = colour_span_list;
		while (span_ptr != NULL) {
			render_colour_span24(span_ptr);
			span_ptr = del_span(span_ptr);
		}
	} else {
		span *span_ptr = colour_span_list;
		while (span_ptr != NULL) {
			render_colour_span32(span_ptr);
			span_ptr = del_span(span_ptr);
		}
	}
}

//------------------------------------------------------------------------------
// Render the screen polygons/spans for this frame that are transparent.
//------------------------------------------------------------------------------

static void
render_transparent_polygons_or_spans(void)
{
	int row;
	span_row *span_row_ptr;
	span *span_ptr;

	// If using hardware acceleration, render the transformed polygons in the transparent transformed polygon list in back to front order,
	// via hardware, removing them from the list in the process.

	if (hardware_acceleration) {
		tpolygon *tpolygon_ptr = transparent_tpolygon_list;
		while (tpolygon_ptr != NULL) {
			hardware_render_polygon(tpolygon_ptr);
			tpolygon_ptr = del_tpolygon(tpolygon_ptr);
		}
		transparent_tpolygon_list = NULL;
	}

	// If not using hardware acceleration, render the transparent spans from
	// each span buffer row in back to front order, removing them as we go.
	// Spans that have a texture of unlimited size are only used by popups, and
	// are rendered as linear texture-mapped spans.

	else if (display_depth <= 16) {
		for (row = 0; row < window_height; row++) {
			span_row_ptr = (*span_buffer_ptr)[row];
			span_ptr = span_row_ptr->transparent_span_list;
			while (span_ptr != NULL) {
				if (span_ptr->is_popup)
					render_popup_span16(span_ptr);
				else
					render_transparent_span16(span_ptr);
				span_ptr = del_span(span_ptr);
			}
		}
	} else if (display_depth == 24) {
		for (row = 0; row < window_height; row++) {
			span_row_ptr = (*span_buffer_ptr)[row];
			span_ptr = span_row_ptr->transparent_span_list;
			while (span_ptr != NULL) {
				if (span_ptr->is_popup)
					render_popup_span24(span_ptr);
				else
					render_transparent_span24(span_ptr);
				span_ptr = del_span(span_ptr);
			}
		}
	} else {
		for (row = 0; row < window_height; row++) {
			span_row_ptr = (*span_buffer_ptr)[row];
			span_ptr = span_row_ptr->transparent_span_list;
			while (span_ptr != NULL) {
				if (span_ptr->is_popup)
					render_popup_span32(span_ptr);
				else
					render_transparent_span32(span_ptr);
				span_ptr = del_span(span_ptr);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Make all popups in the list with rollover triggers visible.
//------------------------------------------------------------------------------

static void
show_rollover_popups(popup *popup_list)
{
	popup *popup_ptr;
	texture *bg_texture_ptr, *fg_texture_ptr;
	int popup_width, popup_height;

	// Step through the list of popups...

	popup_ptr = popup_list;
	while (popup_ptr != NULL) {
		if ((popup_ptr->trigger_flags & ROLLOVER) != 0) {

			// If the popup was not previously visible, set the time that this
			// popup was made visible, and if the window alignment is by mouse
			// then set the position of the popup.

			if (popup_ptr->visible_flags == 0) {
				popup_ptr->start_time_ms = curr_time_ms;
				if (popup_ptr->window_alignment == MOUSE) {
					bg_texture_ptr = popup_ptr->bg_texture_ptr;
					fg_texture_ptr = popup_ptr->fg_texture_ptr;
					if (bg_texture_ptr != NULL) {
						popup_width = bg_texture_ptr->width;
						popup_height = bg_texture_ptr->height;
					} else {
						popup_width = fg_texture_ptr->width;
						popup_height = fg_texture_ptr->height;
					}
					popup_ptr->sx = mouse_x - popup_width / 2;
					popup_ptr->sy = mouse_y - popup_height / 2;
				}
			}

			// Update the popup's visible flags.

			popup_ptr->visible_flags |= ROLLOVER;
		}

		// Move onto the next popup in the list.

		popup_ptr = popup_ptr->next_square_popup_ptr;
	}
}

//------------------------------------------------------------------------------
// Make all popups in the list with rollover triggers invisible.
//------------------------------------------------------------------------------

static void
hide_rollover_popups(popup *popup_list)
{
	popup *popup_ptr = popup_list;
	while (popup_ptr != NULL) {
		if ((popup_ptr->trigger_flags & ROLLOVER) != 0)
			popup_ptr->visible_flags &= ~ROLLOVER;
		popup_ptr = popup_ptr->next_square_popup_ptr;
	}
}

//------------------------------------------------------------------------------
// Render the entire frame.
//------------------------------------------------------------------------------

void
render_frame(void)
{
	block *prev_popup_block_ptr;
	square *prev_popup_square_ptr;
	pixmap *sky_pixmap_ptr;
	float sky_start_u, sky_start_v, sky_end_u, sky_end_v;
	texture *texture_ptr;
	pixmap *pixmap_ptr;
	int row;

	// If hardware acceleration is enabled clear the frame buffer, 
	// otherwise lock the frame buffer.

	if (hardware_acceleration)
		clear_frame_buffer();
	else
		lock_frame_buffer(frame_buffer_ptr, frame_buffer_width);

	// Reset the span and transformed polygon lists.

	colour_span_list = NULL;
	transparent_tpolygon_list = NULL;
	colour_tpolygon_list = NULL;
	reset_screen_polygon_list();

#ifdef _DEBUG

	// Reset the number of the currently selected polygon.

	curr_selected_polygon_no = 0;

#endif

	// Remember the previous block and square selected, then reset the current
	// block and square selected.

	prev_selected_block_ptr = curr_selected_block_ptr;
	prev_selected_square_ptr = curr_selected_square_ptr;
	curr_selected_block_ptr = NULL;
	curr_selected_square_ptr = NULL;

	// *mp* Remember the previous part and reset
	prev_selected_part_ptr = curr_selected_part_ptr;
	curr_selected_part_ptr = NULL;

	// Remember the previous exit selected, and reset the current exit
	// selected.

	prev_selected_exit_ptr = curr_selected_exit_ptr;
	curr_selected_exit_ptr = NULL;

	// Remember the previous area selected and it's square and block, and reset
	// the current area selected and it's square and block.

	prev_selected_area_ptr = curr_selected_area_ptr;
	prev_area_block_ptr = curr_area_block_ptr;
	prev_area_square_ptr = curr_area_square_ptr;
	curr_selected_area_ptr = NULL;
	curr_area_block_ptr = NULL;
	curr_area_square_ptr = NULL;

	// Remember the previous popup block and square, and reset the current 
	// popup block and square.

	prev_popup_block_ptr = curr_popup_block_ptr;
	prev_popup_square_ptr = curr_popup_square_ptr;
	curr_popup_block_ptr = NULL;
	curr_popup_square_ptr = NULL;

	// Reset the selection flag and current popup.

	found_selection = false;
	curr_popup_ptr = NULL;

	// Initialise the visible popup list and the last visible popup pointer.

	visible_popup_list = NULL;
	last_visible_popup_ptr = NULL;

	// Create the visible popup list.

	add_visible_popups_in_popup_list(global_popup_list, NULL);
	add_visible_popups_in_block_list(movable_block_list);
	add_visible_popups_in_block_list(fixed_block_list);

	// Get the sky pixmap, if there is one.

	if (sky_texture_ptr != NULL)
		sky_pixmap_ptr = sky_texture_ptr->get_curr_pixmap_ptr(curr_time_ms - start_time_ms);
	else
		sky_pixmap_ptr = NULL;

	// Compute the top-left and bottom-right texture coordinates for the sky,
	// such that a single sky texture map occupies a 15x15 degree area.

	sky_start_u = (float)((int)player_viewpoint.turn_angle % 15) / 15.0f;
	sky_start_v = (float)((int)player_viewpoint.look_angle % 15) / 15.0f;
	sky_end_u = sky_start_u + horz_field_of_view / 15.0f;
	sky_end_v = sky_start_v + vert_field_of_view / 15.0f;

	// Compute the position of the camera in world space.

	camera_position.x = player_camera_offset.dx;
	camera_position.y = player_camera_offset.dy;
	camera_position.z = player_camera_offset.dz;
	camera_position.rotate_x(player_viewpoint.look_angle);
	camera_position.rotate_y(player_viewpoint.turn_angle);
	camera_position += player_viewpoint.position;

	// Compute the direction of the camera in view space.

	float view_mouse_x = (mouse_x - half_window_width) / half_window_width * half_viewport_width;
	float view_mouse_y = (half_window_height - mouse_y) / half_window_height * half_viewport_height;
	camera_direction = vector(view_mouse_x, view_mouse_y, 1.0f);
	camera_direction.normalise();

	// If using hardware acceleration...
	
	if (hardware_acceleration) {

		// Render the sky and orb polygon.  If fog is enabled, render the sky using the fog colour instead,
		// and don't show the orb.

		if (global_fog_enabled) {
			hardware_render_2D_polygon(NULL, global_fog.colour, 1.0f,
				0.0f, 0.0f, (float)window_width, (float)window_height, 
				sky_start_u, sky_start_v, sky_end_u, sky_end_v);
		} else {
			hardware_render_2D_polygon(sky_pixmap_ptr, sky_colour, sky_brightness,
				0.0f, 0.0f, (float)window_width, (float)window_height, 
				sky_start_u, sky_start_v, sky_end_u, sky_end_v);
			render_orb();
		}
	}

	// If not using hardware acceleration...
	
	else {

		// Clear the span buffer.

		for (row = 0; row < window_height; row++) {
			span_row *span_row_ptr = (*span_buffer_ptr)[row];
			span_row_ptr->opaque_span_list = NULL;
			span_row_ptr->transparent_span_list = NULL;
		}

		// Render the visible popups in front to back order.

		popup *popup_ptr = visible_popup_list;
		while (popup_ptr != NULL) {
			texture_ptr = popup_ptr->fg_texture_ptr;
			if (texture_ptr != NULL)
				render_popup_texture(popup_ptr, &texture_ptr->pixmap_list[0]);
			texture_ptr = popup_ptr->bg_texture_ptr;
			if (texture_ptr != NULL)
				render_popup_texture(popup_ptr, popup_ptr->bg_pixmap_ptr);
			popup_ptr = popup_ptr->next_visible_popup_ptr;
		}
	}

	// Render the blocks on the map that insect with the view frustum.

	compute_view_bounding_box();
	render_map();

	// If not using hardware acceleration...

	if (!hardware_acceleration) {
		float sky_delta_v;
		edge sky_left_edge, sky_right_edge;

		// Render the orb.

		render_orb();

		// Scale the sky texture coordinates if there is a sky pixmap.

		if (sky_pixmap_ptr != NULL) {
			sky_start_u *= sky_pixmap_ptr->width;
			sky_start_v *= sky_pixmap_ptr->height;
			sky_end_u *= sky_pixmap_ptr->width;
			sky_end_v *= sky_pixmap_ptr->height;
		}

		// Initialise the fields of the sky's left and right edge.  Notice that
		// the 1/tz values are 0.0 rather than a really small value, since that
		// causes inaccuracies in u/tz and v/tz; the span rendering functions
		// will use a 1/tz value of 1.0 instead when rendering the sky spans.

		sky_left_edge.sx = 0.0f;
		sky_left_edge.one_on_tz = 0.0f;
		sky_left_edge.u_on_tz = sky_start_u;
		sky_left_edge.v_on_tz = sky_start_v; 
		sky_right_edge.sx = (float)window_width;
		sky_right_edge.one_on_tz = 0.0f;
		sky_right_edge.u_on_tz = sky_end_u;
		sky_right_edge.v_on_tz = sky_start_v;

		// Fill each span buffer row with a sky span.

		sky_delta_v = (sky_end_v - sky_start_v) / window_height;
		for (row = 0; row < window_height; row++) {

			// Add the sky span to the span buffer.

			add_span(row, &sky_left_edge, &sky_right_edge, sky_pixmap_ptr,
				sky_colour_pixel, sky_brightness_index, false);

			// Adjust v/tz for the next row.

			sky_left_edge.v_on_tz += sky_delta_v; 
			sky_right_edge.v_on_tz += sky_delta_v;
		}
	}

	// Render all movable blocks.
	
	render_movable_blocks();

	// If there is a player block, render it last.

	if (player_block_ptr != NULL)
		render_player_block();

	// If not using hardware acceleration, step through all opaque spans in the
	// span buffer, and add each to the span list of the pixmap associated with
	// that span.  Solid colour spans go in their own list.

	if (!hardware_acceleration) {
		for (row = 0; row < window_height; row++) {
			span_row *span_row_ptr = (*span_buffer_ptr)[row];
			span *span_ptr = span_row_ptr->opaque_span_list;
			while (span_ptr != NULL) {
				int brightness_index;
				span *next_span_ptr = span_ptr->next_span_ptr;
				pixmap_ptr = span_ptr->pixmap_ptr;
				brightness_index = span_ptr->brightness_index;
				if (pixmap_ptr != NULL) {
					span **span_list_ptr = 
						&pixmap_ptr->span_lists[brightness_index];
					span_ptr->next_span_ptr = *span_list_ptr;
					*span_list_ptr = span_ptr;
				} else {
					span_ptr->next_span_ptr = colour_span_list;
					colour_span_list = span_ptr;
				}
				span_ptr = next_span_ptr;
			}
		}
	}

	// Step through each pixmap in each texture, rendering any polygons/spans
	// listed in these pixmaps.

	render_textured_polygons_or_spans();

	// Render the colour polygons/spans, followed by the transparent 
	// polygons/spans.

	render_colour_polygons_or_spans();
	render_transparent_polygons_or_spans();

	// If build mode is active, render the selected square as a wireframe cube.

	if (build_mode.get()) {
		render_wireframe_square(0, 0, 0);
	}

	// If hardware acceleration is enabled, render the visible popup list in
	// back to front order.

	if (hardware_acceleration) {
		popup *popup_ptr = visible_popup_list;
		while (popup_ptr != NULL) {
			texture_ptr = popup_ptr->bg_texture_ptr;
			if (texture_ptr != NULL)
				render_popup_texture(popup_ptr, popup_ptr->bg_pixmap_ptr);
			texture_ptr = popup_ptr->fg_texture_ptr;
			if (texture_ptr != NULL)
				render_popup_texture(popup_ptr, &texture_ptr->pixmap_list[0]);
			popup_ptr = popup_ptr->next_visible_popup_ptr;
		}
	}

	// If hardware acceleration is not enabled, unlock the frame buffer.

	else
		unlock_frame_buffer();

	// If the viewpoint has changed since the last frame, reset the current
	// popup block and square, and if there was a previous popup block and/or 
	// square, make all popups on that block and/or square with a rollover 
	// trigger invisible.

	if (viewpoint_has_changed) {
		curr_popup_block_ptr = NULL;
		curr_popup_square_ptr = NULL;
		if (prev_popup_block_ptr != NULL)
			hide_rollover_popups(prev_popup_block_ptr->popup_list);
		if (prev_popup_square_ptr != NULL)
			hide_rollover_popups(prev_popup_square_ptr->popup_list);
	}

	// If a popup is currently selected that is visible due to a rollover
	// trigger, pretend the current popup block and square is the same as the
	// previous popup block square in order to keep all rollover popups visible.

	else if (curr_popup_ptr && (curr_popup_ptr->visible_flags & ROLLOVER)) {
		curr_popup_block_ptr = prev_popup_block_ptr;
		curr_popup_square_ptr = prev_popup_square_ptr;
	} 
	
	// If the viewpoint hasn't changed and there is no popup selected with
	// a rollover trigger...

	else {

		// If the current popup block is different to the previous popup 
		// block, hide all rollover popup's on the previous block (if there
		// was one), and show all rollover popup's on the current block
		// (if there is one).
	
		if (curr_popup_block_ptr != prev_popup_block_ptr) {
			if (prev_popup_block_ptr != NULL)
				hide_rollover_popups(prev_popup_block_ptr->popup_list);
			if (curr_popup_block_ptr != NULL)
				show_rollover_popups(curr_popup_block_ptr->popup_list);
		}

		// If the current popup square is different to the previous popup 
		// square, hide all rollover popup's on the previous square (if there
		// was one), and show all rollover popup's on the current square
		// (if there is one).
	
		if (curr_popup_square_ptr != prev_popup_square_ptr) {
			if (prev_popup_square_ptr != NULL)
				hide_rollover_popups(prev_popup_square_ptr->popup_list);
			if (curr_popup_square_ptr != NULL)
				show_rollover_popups(curr_popup_square_ptr->popup_list);
		}
	}

	// If a mouse selection hasn't been found yet, check whether the orb has
	// been selected.  If the orb has no exit, ignore it.

	if (!found_selection && orb_exit_ptr && mouse_x >= curr_orb_x &&
		mouse_x < curr_orb_x + curr_orb_width && mouse_y >= curr_orb_y &&
		mouse_y < curr_orb_y + curr_orb_height) {
		found_selection = true;
		curr_selected_exit_ptr = orb_exit_ptr;
	}
}