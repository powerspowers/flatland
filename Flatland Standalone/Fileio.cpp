//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <direct.h>
#include <io.h>
#include "Classes.h"
#include "Image.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Render.h"
#include "Spans.h"
#include "Tags.h"
#include "Utils.h"
#include "SimKin.h"

// Flag indicating if strict XML compliance is being used to parse spot.

static bool spot_XML_compliance;

// Flags indicating whether we've seen certain tags already when loading a new
// spot, blockset or block file.

static bool got_action_exit_tag;
static bool got_ambient_light_tag;
static bool got_ambient_sound_tag;
static bool got_base_tag;
static bool got_blockset_tag;
static bool got_body_tag;
static bool got_BSP_tree_tag;
static bool got_client_tag;
static bool got_debug_tag;
static bool got_exit_tag;
static bool got_head_tag;
static bool got_location_param;
static bool got_map_tag;
static bool got_frame_list;
static bool got_loop_list;
static bool got_param_tag;
static bool got_part_list;
static bool got_player_tag;
static bool got_player_size;
static bool got_player_camera;
static bool got_server_tag;
static bool got_sprite_part_tag;
#ifdef STREAMING_MEDIA
static bool got_stream_tag;
#endif
static bool got_title_tag;
static bool got_vertex_list;

// Last level of map that was defined.

static int last_level;

// List of vertex definition entries, parts and polygons in the current block
// definition.

static int vertices;
static vertex_def_entry *vertex_def_entry_list;
static int polygons;
static polygon_def *polygon_def_list;
static int parts;
static part *part_list;

// Global SimKin script.

static string global_script;

//==============================================================================
// Functions to parse BLOCK tags.
//==============================================================================

//------------------------------------------------------------------------------
// Update a part's common fields.
//------------------------------------------------------------------------------

static void
update_common_part_fields(part *part_ptr, texture *texture_ptr, 
						  RGBcolour colour, float translucency,
						  int texture_style)
{
	// If the texture or stream parameter was given, set either the current or
	// custom texture pointer.  If the colour parameter was given but the
	// texture parameter was not, set the texture pointers to NULL so that the
	// colour takes precedence.

#ifdef STREAMING_MEDIA

	if (parsed_attribute[PART_TEXTURE] || parsed_attribute[PART_STREAM]) {
		if (texture_ptr != NULL && texture_ptr->blockset_ptr == NULL) {
			part_ptr->custom_texture_ptr = texture_ptr;
			part_ptr->texture_ptr = NULL;
		} else
			part_ptr->texture_ptr = texture_ptr;
	} else if (parsed_attribute[PART_COLOUR]) {
		part_ptr->texture_ptr = NULL;
		part_ptr->custom_texture_ptr = NULL;
	}

#else

	if (parsed_attribute[PART_TEXTURE]) {
		if (texture_ptr != NULL && texture_ptr->blockset_ptr == NULL) {
			part_ptr->custom_texture_ptr = texture_ptr;
			part_ptr->texture_ptr = NULL;
		} else
			part_ptr->texture_ptr = texture_ptr;
	} else if (parsed_attribute[PART_COLOUR]) {
		part_ptr->texture_ptr = NULL;
		part_ptr->custom_texture_ptr = NULL;
	}

#endif

	// Update remaining fields, if they have been specified.

	if (parsed_attribute[PART_COLOUR]) {
		part_ptr->colour_set = true;
		part_ptr->colour = colour;
		colour.normalise();
		part_ptr->normalised_colour = colour;
	}
	if (parsed_attribute[PART_TRANSLUCENCY])
		part_ptr->alpha = 1.0f - translucency;
	if (parsed_attribute[PART_STYLE])
		part_ptr->texture_style = texture_style;
}

//------------------------------------------------------------------------------
// Parse the vertex list.
//------------------------------------------------------------------------------

static void
parse_vertex_list(block_def *block_def_ptr)
{
	int vertices;
	vertex_entry *vertex_entry_list;
	vertex_entry *vertex_entry_ptr, *next_vertex_entry_ptr;
	int tag_token;
	int index;

	// If the vertices tag has already been seen, this is an error.

	if (got_vertex_list)
		error("Duplicate vertex list");
	got_vertex_list = true;

	// The new block format has the number of vertices up front so its faster.

	if (parsed_attribute[VERTICES_SIZE]) {
		
		// Do the parsing in a try block, so that we can trap errors...

		if (vertices_size < 0)
			error("Vertices SIZE attribute cannot be negative");
		vertices = vertices_size;
		block_def_ptr->create_vertex_list(vertices);
		index = 0;
		try { 

			// Read each vertex definition and store it in the vertex entry list,
			// until all vertex definitions have been read.

			start_parsing_nested_tags();
			while (parse_next_nested_tag(TOKEN_VERTICES, vertices_tag_list, false,
				&tag_token)) {

				// Verify that the vertex reference is in sequence.

				index++;
				if (vertex_ref != index)
					error("Vertex reference number %d is out of sequence",
						vertex_ref);

				// Create a new vertex entry, and add it to the head of the
				// vertex entry list.

				block_def_ptr->vertex_list[index-1].x = vertex_coords.x;
				block_def_ptr->vertex_list[index-1].y = vertex_coords.y;
				block_def_ptr->vertex_list[index-1].z = vertex_coords.z;

			}
			stop_parsing_nested_tags();
		}

		// Upon an error, delete the vertex entry list, before throwing the error.

		catch (char *message) {
			DELARRAY(block_def_ptr->vertex_list, vertex, vertices);
			throw (char *)message;
		}
	} else {

		// Do the parsing in a try block, so that we can trap errors...

		vertices = 0;
		vertex_entry_list = NULL;
		try { 

			// Read each vertex definition and store it in the vertex entry list,
			// until all vertex definitions have been read.

			start_parsing_nested_tags();
			while (parse_next_nested_tag(TOKEN_VERTICES, vertices_tag_list, false,
				&tag_token)) {

				// Verify that the vertex reference is in sequence.

				vertices++;
				if (vertex_ref != vertices)
					error("Vertex reference number %d is out of sequence",
						vertex_ref);

				// Create a new vertex entry, and add it to the head of the
				// vertex entry list.

				NEW(vertex_entry_ptr, vertex_entry);
				if (vertex_entry_ptr == NULL)
					memory_error("vertex entry");
				vertex_entry_ptr->next_vertex_entry_ptr = vertex_entry_list;
				vertex_entry_list = vertex_entry_ptr;

				// Initialise the vertex entry.

				vertex_entry_ptr->x = vertex_coords.x;
				vertex_entry_ptr->y = vertex_coords.y;
				vertex_entry_ptr->z = vertex_coords.z;
			}
			stop_parsing_nested_tags();
		}

		// Upon an error, delete the vertex entry list, before throwing the error.

		catch (char *message) {
			while (vertex_entry_list != NULL) {
				next_vertex_entry_ptr = vertex_entry_list->next_vertex_entry_ptr;
				DEL(vertex_entry_list, vertex_entry);
				vertex_entry_list = next_vertex_entry_ptr;
			}
			throw (char *)message;
		}

		// Copy the vertex entry list to the block definition's vertex list, 
		// deleting it along the way.  This is done in reverse order because the
		// vertex entry list is in reverse order.

		block_def_ptr->create_vertex_list(vertices);
		vertex_entry_ptr = vertex_entry_list;
		for (index = vertices - 1; index >= 0; index--) {
			block_def_ptr->vertex_list[index].x = vertex_entry_ptr->x;
			block_def_ptr->vertex_list[index].y = vertex_entry_ptr->y;
			block_def_ptr->vertex_list[index].z = vertex_entry_ptr->z;
			next_vertex_entry_ptr = vertex_entry_ptr->next_vertex_entry_ptr;
			DEL(vertex_entry_ptr, vertex_entry);
			vertex_entry_ptr = next_vertex_entry_ptr;
		}
	}
}

//------------------------------------------------------------------------------
// Parse the vertex reference list.
//------------------------------------------------------------------------------

static void
parse_vertex_ref_list(int block_vertices)
{
	vertex_def_entry *last_vertex_def_entry_ptr;

	// Parse each vertex reference number in the string.

	start_parsing_value(TOKEN_POLYGON, TOKEN_VERTICES, polygon_vertices, true);
	last_vertex_def_entry_ptr = NULL;
	do {
		int ref_no;
		vertex_def_entry *vertex_def_entry_ptr;

		// If the reference number cannot be parsed or is out of range,
		// this is an error.

		parse_integer_in_value(&ref_no);
		check_int_range(ref_no, 1, block_vertices);

		// Create a new vertex definition entry and initialise it's vertex
		// number and next vertex definition entry pointer.

		NEW(vertex_def_entry_ptr, vertex_def_entry);
		if (vertex_def_entry_ptr == NULL)
			memory_error("vertex definition entry");
		vertex_def_entry_ptr->vertex_no = ref_no - 1;
		vertex_def_entry_ptr->next_vertex_def_entry_ptr = NULL;

		// Add the vertex definition entry to the end of the vertex 
		// definition entry list.

		if (last_vertex_def_entry_ptr != NULL)
			last_vertex_def_entry_ptr->next_vertex_def_entry_ptr = 
				vertex_def_entry_ptr;
		else
			vertex_def_entry_list = vertex_def_entry_ptr;
		last_vertex_def_entry_ptr = vertex_def_entry_ptr;
		vertices++;
	} while (token_in_value_is(",", false));

	// If the remainder of the string is not empty or whitespace, or the
	// vertex reference list has less than 3 elements, this is an error.

	stop_parsing_value(true);
	if (vertices < 3)
		error("Polygon has less than 3 vertices");
}

//------------------------------------------------------------------------------
// Parse the frame list.
//------------------------------------------------------------------------------

static void
parse_frame_list(block_def *block_def_ptr)
{
	int tag_token;
	int index;

	// Do the parsing in a try block, so that we can trap errors...

	if (frames_size < 0)
		error("Frames SIZE attribute cannot be negative");

	NEW(block_def_ptr->animation,animation_def);
	if (block_def_ptr->animation == NULL)
		error ("animation_def");
	block_def_ptr->animated = true;

	block_def_ptr->animation->loops = 0;
	block_def_ptr->animation->loops_list = NULL;

	block_def_ptr->create_frames_list(frames_size);
	block_def_ptr->animation->original_frames = true;

	index = 0;

	try { 

		// Read each frame definition and store it in the frame entry list,
		// until all frame definitions have been read.

		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_FRAMES, frames_tag_list, false,
			&tag_token)) {

			// Verify that the frame reference is in sequence.

			index++;
			if (frame_ref != index)
				error("Frame reference number %d is out of sequence",
					frame_ref);

			// Set the frame angle
			block_def_ptr->animation->angles[index-1] = 0.0f;

			// Read each frame section (vertices only right now)

			start_parsing_nested_tags();
			while (parse_next_nested_tag(TOKEN_FRAME, frame_tag_list, false, &tag_token)) {
				switch (tag_token) {
					case TOKEN_VERTICES:
						got_vertex_list = false;
						parse_vertex_list(block_def_ptr);
						block_def_ptr->animation->frame_list[index-1].vertex_list = block_def_ptr->vertex_list;
						block_def_ptr->vertex_list = NULL;
						block_def_ptr->animation->vertices[index-1] = block_def_ptr->vertices;
						block_def_ptr->vertices = 0;
						break;
				}

			}
			stop_parsing_nested_tags();

			// Set the block def to the first vertex list and vertices counter

			block_def_ptr->vertices = block_def_ptr->animation->vertices[0];
			block_def_ptr->vertex_list = block_def_ptr->animation->frame_list[0].vertex_list;
		}
		stop_parsing_nested_tags();
	}

	// Upon an error, delete the frame entry list, before throwing the error.

	catch (char *message) {
		DELARRAY(block_def_ptr->animation->vertices, int, frames_size);
		for (index=0; index < frames_size; index++)
			DELARRAY(block_def_ptr->animation->frame_list[index].vertex_list, vertex, block_def_ptr->animation->vertices[index]);
		DELARRAY(block_def_ptr->animation->frame_list, frame_def, frames_size);
		DELARRAY(block_def_ptr->animation->angles, float, frames_size);
		DEL(block_def_ptr->animation,animation_def);
		throw (char *)message;
	}
}


//------------------------------------------------------------------------------
// Parse the loop list.
//------------------------------------------------------------------------------

static void
parse_loop_list(block_def *block_def_ptr)
{
	int tag_token;
	int index;

	// Do the parsing in a try block, so that we can trap errors...

	if (loops_size < 0)
		error("Frames SIZE attribute cannot be negative");

	block_def_ptr->create_loops_list(loops_size);

	index = 0;

	try { 

		// Read each loop definition and store it in the loop entry list,
		// until all loop definitions have been read.

		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_LOOPS, loops_tag_list, false,
			&tag_token)) {

			// Verify that the loop reference is in sequence.

			index++;
			if (loop_ref != index)
				error("Loop reference number %d is out of sequence",
					loop_ref);

			// Check the range of the frames is correct
			if (loop_range.min < 1)
				error("Loop min frames number %d cannot be less than zero",
					loop_range.min);
			if (loop_range.max > block_def_ptr->animation->frames)
				error("Loop max frames number %d cannot be more than %d",
					loop_range.max, block_def_ptr->animation->frames);

			block_def_ptr->animation->loops_list[index-1].min = loop_range.min - 1;
			block_def_ptr->animation->loops_list[index-1].max = loop_range.max - 1;
		}
		stop_parsing_nested_tags();
	}

	// Upon an error, delete the loop entry list, before throwing the error.

	catch (char *message) {
		DELARRAY(block_def_ptr->animation->loops_list, intrange, loops_size);
		throw (char *)message;
	}
}

//------------------------------------------------------------------------------
// Parse the texture coordinates list.
//------------------------------------------------------------------------------

static void
parse_texcoords_list(void)
{
	vertex_def_entry *vertex_def_entry_ptr;

	// Parse the list of texture coordinates.  These coordinates are assumed
	// to be for "stretched" texture style; the texture coordinates for 
	// "tiled" or "scaled" texture style can be computed on the fly.

	start_parsing_value(TOKEN_POLYGON, TOKEN_TEXCOORDS, polygon_texcoords, true);
	vertex_def_entry_ptr = vertex_def_entry_list;
	do {
		
		// If we've run out of vertex definition entries, or we can't parse
		// the texture coordinates, this is an error.

		if (vertex_def_entry_ptr == NULL)
			error("There are too many texture coordinates");
		token_in_value_is("(", true);
		parse_float_in_value(&vertex_def_entry_ptr->u);
		token_in_value_is(",", true);
		parse_float_in_value(&vertex_def_entry_ptr->v);
		token_in_value_is(")", true);

		// Move onto the next vertex definition.

		vertex_def_entry_ptr = 
			vertex_def_entry_ptr->next_vertex_def_entry_ptr;
	} while (token_in_value_is(",", false));

	// If we haven't come to the end of the vertex definition entry list,
	// or the remainder of the string is not empty or whitespace, this is
	// an error.

	if (vertex_def_entry_ptr != NULL)
		error("There are too few texture coordinates");
	stop_parsing_value(true);
}

//------------------------------------------------------------------------------
// Compute the texture coordinates for a polygon.
//------------------------------------------------------------------------------

static void
compute_texture_coordinates(polygon_def *polygon_def_ptr, 
							int texture_style, int projection,
							block_def *block_def_ptr)
{
	// Compute the texture coordinates for this polygon.  The texture
	// coordinates for the "stretched" texture style are already computed
	// and merely need to be recovered.

	if (texture_style == STRETCHED_TEXTURE)
		for (int index = 0; index < polygon_def_ptr->vertices; index++) {
			vertex_def *vertex_def_ptr = 
				&polygon_def_ptr->vertex_def_list[index];
			vertex_def_ptr->u = vertex_def_ptr->orig_u;
			vertex_def_ptr->v = vertex_def_ptr->orig_v;
		}
	else
		polygon_def_ptr->project_texture(block_def_ptr->vertex_list, 
			projection);
}

//------------------------------------------------------------------------------
// Rotate the texture coordinates for a polygon.
//------------------------------------------------------------------------------

static void
rotate_texture_coordinates(polygon_def *polygon_def_ptr, float texture_angle)
{
	float texture_angle_radians = RAD(texture_angle);
	for (int vertex_no = 0; vertex_no < polygon_def_ptr->vertices; vertex_no++) {
		vertex_def *vertex_def_ptr = &polygon_def_ptr->vertex_def_list[vertex_no];
		float tu = vertex_def_ptr->u - 0.5f;
		float tv = vertex_def_ptr->v - 0.5f;
		float ru = tu * cosf(texture_angle_radians) + tv * sinf(texture_angle_radians);
		float rv = tv * cosf(texture_angle_radians) - tu * sinf(texture_angle_radians);
		vertex_def_ptr->u = ru + 0.5f;
		vertex_def_ptr->v = rv + 0.5f;
	}
}

//------------------------------------------------------------------------------
// Parse the polygon tag.
//------------------------------------------------------------------------------

static void
parse_polygon_tag(block_def *block_def_ptr, part *part_ptr)
{
	int index;
	polygon_def *polygon_def_ptr;
	vertex_def_entry *vertex_def_entry_ptr;
	vertex_def_entry *next_vertex_def_entry_ptr;

	// Make sure the polygon reference number was in sequence.

	if (polygon_ref != polygons + 1)
		error("Polygon reference number %d is out of sequence", polygon_ref);

	// Create a new polygon definition, and add it to the head of the polygon 
	// definition list.

	NEW(polygon_def_ptr, polygon_def);
	if (polygon_def_ptr == NULL)
		memory_error("polygon entry");
	polygon_def_ptr->next_polygon_def_ptr = polygon_def_list;
	polygon_def_list = polygon_def_ptr;
	polygons++;

	// Store the front and rear polygon references in the polygon, if they
	// were given.

	if (parsed_attribute[POLYGON_FRONT])
		polygon_def_ptr->front_polygon_ref = polygon_front;
	if (parsed_attribute[POLYGON_REAR])
		polygon_def_ptr->rear_polygon_ref = polygon_rear;

	// Parse the vertex reference list and texture coordinates list, creating a
	// vertex definition entry list in the process.  If this fails, delete the 
	// vertex definition entry list before throwing the error.

	vertices = 0;
	vertex_def_entry_list = NULL;
	try {
		parse_vertex_ref_list(block_def_ptr->vertices);
		parse_texcoords_list();
	}
	catch (char *error_str) {
		while (vertex_def_entry_list != NULL) {
			next_vertex_def_entry_ptr = 
				vertex_def_entry_list->next_vertex_def_entry_ptr;
			DEL(vertex_def_entry_list, vertex_def_entry);
			vertex_def_entry_list = next_vertex_def_entry_ptr;
		}
		throw (char *)error_str;
	}

	// Copy the vertex definition entry list to the polygon's vertex definition
	// list, deleting it in the process.

	if (!polygon_def_ptr->create_vertex_def_list(vertices))
		memory_error("polygon vertex definition list");
	vertex_def_entry_ptr = vertex_def_entry_list;
	for (index = 0; index < vertices; index++) {
		vertex_def *vertex_def_ptr = &polygon_def_ptr->vertex_def_list[index];
		vertex_def_ptr->vertex_no = vertex_def_entry_ptr->vertex_no;
		vertex_def_ptr->u = vertex_def_entry_ptr->u;
		vertex_def_ptr->v = vertex_def_entry_ptr->v;
		vertex_def_ptr->orig_u = vertex_def_entry_ptr->u;
		vertex_def_ptr->orig_v = vertex_def_entry_ptr->v;
		next_vertex_def_entry_ptr = 
			vertex_def_entry_ptr->next_vertex_def_entry_ptr;
		DEL(vertex_def_entry_ptr, vertex_def_entry);
		vertex_def_entry_ptr = next_vertex_def_entry_ptr;
	}

	// Set the part number, then compute the centroid, normal vector and plane
	// offset.

	polygon_def_ptr->part_no = parts - 1;

	// Compute and rotate the texture coordinates.

	compute_texture_coordinates(polygon_def_ptr, part_ptr->texture_style, NONE,
		block_def_ptr);
	rotate_texture_coordinates(polygon_def_ptr, part_ptr->texture_angle);
}

//------------------------------------------------------------------------------
// Parse a part tag in a structural block.
//------------------------------------------------------------------------------

static void
parse_part_tag(blockset *blockset_ptr, block_def *block_def_ptr)
{
	part *part_ptr;
	texture *texture_ptr = NULL;
	int tag_token;

	// Verify the part name hasn't been used in this block already.

	part_ptr = part_list;
	while (part_ptr != NULL) {
		if (!_stricmp(part_name, part_ptr->name))
			error("Duplicate part name \"%s\"", part_name);
		part_ptr = part_ptr->next_part_ptr;
	}

	// Create a new part with default attributes, and add it to the head of 
	// the part list.

	NEW(part_ptr, part);
	if (part_ptr == NULL)
		memory_error("part");
	part_ptr->next_part_ptr = part_list;
	part_list = part_ptr;
	parts++;

	// Set the part name.

	part_ptr->name = part_name;

	// initialize the trigger flags.

	part_ptr->trigger_flags = NULL;

	// If the texture parameter was present, load or retrieve the texture.

	if (parsed_attribute[PART_TEXTURE]) 
		texture_ptr = load_texture(blockset_ptr, part_texture, true);

	// Initialise the part's common fields.
	
	update_common_part_fields(part_ptr, texture_ptr, part_colour,
		part_translucency, part_style);
	
	// Initialise the fields permitted in a structural block part tag.

	if (parsed_attribute[PART_FACES]) {
		if (part_faces < 0 || part_faces > 2)
			error("A part must have 0, 1 or 2 faces per polygon");
		part_ptr->faces = part_faces;
	}
	if (parsed_attribute[PART_ANGLE])
		part_ptr->texture_angle = part_angle;

    // See if there is a SOLID tag.

	if (parsed_attribute[PART_SOLID])
		part_ptr->solid = part_solid;
	
	// Parse each polygon tag.

	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_PART, part_tag_list, false, &tag_token))
		parse_polygon_tag(block_def_ptr, part_ptr);
	stop_parsing_nested_tags();
}

//------------------------------------------------------------------------------
// Parse the structural part list.
//------------------------------------------------------------------------------

static void
parse_part_list(blockset *blockset_ptr, block_def *block_def_ptr)
{
	part *part_ptr, *next_part_ptr;
	polygon_def *polygon_def_ptr, *next_polygon_def_ptr;
	int tag_token, index;
 
	// If the parts tag has already been seen, this is an error.

	if (got_part_list)
		error("Duplicate <I>parts</I> tag");
	got_part_list = true;

	// Read each part definition and store it in the part list, until all
	// parts have been read.

	parts = 0;
	part_list = NULL;
	polygons = 0;
	polygon_def_list = NULL;
	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_PARTS, parts_tag_list, false, 
		&tag_token))
		parse_part_tag(blockset_ptr, block_def_ptr);
	stop_parsing_nested_tags();

	// Copy the part list into the block definition.  This is done in reverse
	// order because the part list is in reverse order.

	block_def_ptr->create_part_list(parts);
	part_ptr = part_list;
	for (index = parts - 1; index >= 0; index--) {
		block_def_ptr->part_list[index] = *part_ptr;
		block_def_ptr->part_list[index].number = index;
		next_part_ptr = part_ptr->next_part_ptr;
		DEL(part_ptr, part);
		part_ptr = next_part_ptr;
	}

	// Copy the polygon list into the block definition, and set the part pointer
	// for each polygon.  This is done in reverse order because the polygon list
	// is in reverse order.

	block_def_ptr->create_polygon_def_list(polygons);
	polygon_def_ptr = polygon_def_list;
	for (index = polygons - 1; index >= 0; index--) {
		polygon_def *new_polygon_def_ptr = 
			&block_def_ptr->polygon_def_list[index];
		*new_polygon_def_ptr = *polygon_def_ptr;
		new_polygon_def_ptr->part_ptr = 
			&block_def_ptr->part_list[new_polygon_def_ptr->part_no];
		next_polygon_def_ptr = polygon_def_ptr->next_polygon_def_ptr;
		DEL(polygon_def_ptr, polygon_def);
		polygon_def_ptr = next_polygon_def_ptr;
	}
}

//------------------------------------------------------------------------------
// Parse the sprite part tag.
//------------------------------------------------------------------------------

static void
parse_sprite_part_tag(blockset *blockset_ptr, block_def *block_def_ptr)
{
	part *part_ptr;
	texture *texture_ptr;

	// If the sprite part tag has already been seen, return without doing
	// anything.

	if (got_sprite_part_tag)
		return;
	got_sprite_part_tag = true;

	// Create a single part and initialise it's name and the number of faces
	// per polygon.

	block_def_ptr->create_part_list(1);
	part_ptr = block_def_ptr->part_list;
	part_ptr->name = part_name;
	part_ptr->faces = 2;

	// If the texture parameter was given, load or retrieve the texture with the
	// given name.

	if (parsed_attribute[PART_TEXTURE])
		texture_ptr = load_texture(blockset_ptr, part_texture, true);

	// Initialise the part's common fields.
	
	update_common_part_fields(part_ptr, texture_ptr, part_colour, 
		part_translucency, part_style);
		
	// Create the vertex and polygon lists needed to define the sprite.

	create_sprite_polygon(block_def_ptr, part_ptr);
}

//==============================================================================
// Functions to parse BLOCK and certain BODY tags.
//==============================================================================

//------------------------------------------------------------------------------
// Create a BSP node for the given polygon reference.  A zero polygon reference
// indicates the node does not exist, so NULL is returned.  Otherwise a new BSP
// node is created and this function called recursively to obtain the front and
// rear BSP nodes that branch off from this one.
//------------------------------------------------------------------------------

static BSP_node *
create_BSP_node(int polygon_ref, block_def *block_def_ptr)
{
	polygon_def *polygon_def_ptr;
	BSP_node *BSP_node_ptr;

	// If the polygon reference is zero, return a NULL pointer.

	if (polygon_ref == 0)
		return(NULL);

	// Make sure the polygon reference is in range, then get a pointer to the
	// referenced polygon.

	if (!check_int_range(polygon_ref, 1, block_def_ptr->polygons))
		error("Polygon reference number %d is out of range", polygon_ref);
	polygon_def_ptr = &block_def_ptr->polygon_def_list[polygon_ref - 1];

	// Create a new BSP node and initialise it.  This involves recursively
	// calling this function to obtain the BSP node for the polygon in front
	// of and behind this one.

	NEW(BSP_node_ptr, BSP_node);
	if (BSP_node_ptr == NULL)
		memory_error("BSP node");
	BSP_node_ptr->polygon_no = polygon_ref - 1;
	BSP_node_ptr->front_node_ptr = 
		create_BSP_node(polygon_def_ptr->front_polygon_ref, block_def_ptr);
	BSP_node_ptr->rear_node_ptr = 
		create_BSP_node(polygon_def_ptr->rear_polygon_ref, block_def_ptr);

	// Return this BSP node.

	return(BSP_node_ptr);
}

//------------------------------------------------------------------------------
// Parse the BSP tree tag.
//------------------------------------------------------------------------------

void
parse_BSP_tree_tag(block_def *block_def_ptr)
{
	// If we've seen the BSP tree tag before, return without doing anything.

	if (got_BSP_tree_tag)
		return;
	got_BSP_tree_tag = true;

	// Store the root polygon reference number in the block definition.

	block_def_ptr->root_polygon_ref = BSP_tree_root;
}

//------------------------------------------------------------------------------
// Parse an exit tag.
//------------------------------------------------------------------------------

void
parse_exit_tag(hyperlink *&exit_ptr)
{
	// If there is no exit object, create one with default settings.

	if (exit_ptr == NULL) {
		NEW(exit_ptr, hyperlink);
		if (exit_ptr == NULL) {
			memory_warning("exit");
			return;
		}
	}

	// Initialise the exit with whatever parameter were parsed.

	exit_ptr->URL = exit_href;
	if (parsed_attribute[EXIT_IS_SPOT])
		exit_ptr->is_spot_URL = true;
	if (parsed_attribute[EXIT_TARGET])
		exit_ptr->target = exit_target;
	if (parsed_attribute[EXIT_TRIGGER])
		exit_ptr->trigger_flags = exit_trigger;
	if (parsed_attribute[EXIT_TEXT])
		exit_ptr->label = exit_text;
}

//------------------------------------------------------------------------------
// Parse the structural param tag.
//------------------------------------------------------------------------------

void
parse_param_tag(block_def *block_def_ptr)
{
	// If the param tag has already been seen, this is an error.

	if (got_param_tag) {
		warning("Duplicate <I>param</I> tag");
		return;
	}
	got_param_tag = true;

	// If the orient parameter was given, set the block orientation.

	if (parsed_attribute[PARAM_ORIENTATION])
		block_def_ptr->block_orientation = param_orientation;

	// If the origin parameter was given, set the block origin.

	if (parsed_attribute[PARAM_ORIGIN])
		block_def_ptr->block_origin = param_origin;

	// If the solid parameter was given, set the solid flag in the block
	// definition.

	if (parsed_attribute[PARAM_SOLID])
		block_def_ptr->solid = param_solid;

	// If the movable parameter was given, set the movable flag in the block
	// definition.

	if (parsed_attribute[PARAM_MOVABLE])
		block_def_ptr->movable = param_movable;

	// If the rotate parameter is given rotate the vertices.

	if (parsed_attribute[PARAM_ROTATE]) {
		block_def_ptr->rotate_x(param_rotate.x);
		block_def_ptr->rotate_y(param_rotate.y);
		block_def_ptr->rotate_z(param_rotate.z);
	}

	// If the scale parameters are given scale the vertices.

	if (parsed_attribute[PARAM_SCALE]) {
		int n;
		for (n=0;n < block_def_ptr->vertices;n++) {
			if(param_scale.x != 0.0) 
				block_def_ptr->vertex_list[n].x *= param_scale.x;
			if(param_scale.y != 0.0) 
				block_def_ptr->vertex_list[n].y *= param_scale.y;
			if(param_scale.z != 0.0) 
				block_def_ptr->vertex_list[n].z *= param_scale.z;
		}
		if(param_scale.x != 0.0) 
			block_def_ptr->block_origin.x *= param_scale.x;
		if(param_scale.y != 0.0) 
			block_def_ptr->block_origin.y *= param_scale.y;
		if(param_scale.z != 0.0) 
			block_def_ptr->block_origin.z *= param_scale.z;

	}

	// If the position parameter is given set the rel position.

	if (parsed_attribute[PARAM_POSITION]) {
		block_def_ptr->position = param_position;
	}
}

//------------------------------------------------------------------------------
// Parse the sprite param tag.
//------------------------------------------------------------------------------

void
parse_sprite_param_tag(block_def *block_def_ptr)
{
	// If the param tag has already been seen, this is an error.

	if (got_param_tag) {
		warning("Duplicate <I>param</I> tag");
		return;
	}
	got_param_tag = true;

	// If the angle parameter was given, set the sprite angle in the block
	// definition.

	if (parsed_attribute[SPRITE_ANGLE])
		block_def_ptr->sprite_angle = sprite_angle;

	// If the speed parameter was given, set the rotational speed (degrees per
	// millisecond) in the block definition.

	if (parsed_attribute[SPRITE_SPEED])
		block_def_ptr->degrees_per_ms = sprite_speed * 360.0f / 1000.0f;

	// If the align parameter was given, set the sprite alignment.

	if (parsed_attribute[SPRITE_ALIGNMENT])
		block_def_ptr->sprite_alignment = sprite_alignment;

	// If the solid parameter was given, set the sprite's solid flag.

	if (parsed_attribute[SPRITE_SOLID])
		block_def_ptr->solid = sprite_solid;

	// If the movable parameter was given, set the sprite's movable flag.

	if (parsed_attribute[SPRITE_MOVABLE])
		block_def_ptr->movable = sprite_movable;

	// If the size parameter was given, set the sprite's size.

	if (parsed_attribute[SPRITE_SIZE])
		block_def_ptr->sprite_size = sprite_size;
}

//------------------------------------------------------------------------------
// Parse a point light tag, and add or update a point light in the given list.
//------------------------------------------------------------------------------

static light *
parse_point_light_tag(light *&light_list, light *&last_light_ptr, bool allow_replacement)
{
	string light_name;
	light *light_ptr;

	// If the name parameter was specified, set the light name, otherwise use
	// an empty name.

	if (parsed_attribute[POINT_LIGHT_NAME])
		light_name = point_light_name;

	// If allow_replacement is TRUE, check whether a light of that name already
	// exists in the list, and if so use it.
	//
	// NOTE: This means that there can only be one light with no name.

	if (allow_replacement)
		light_ptr = find_light(light_list, light_name);

	// If allow_replacement is FALSE, and the light name is not empty, then
	// verify no light with that name exists.
	//
	// NOTE: This means there can be multiple lights with no name.

	else {
		if (strlen(light_name) > 0 && 
			find_light(light_list, light_name) != NULL) {
			warning("Duplicate light with name \"%s\"", light_name);
			return(NULL);
		}
		light_ptr = NULL;
	}

	// If no light has been found, create a new light with default settings,
	// set it's name, then add it to the end of the list.

	if (light_ptr == NULL) {
		NEW(light_ptr, light);
		if (light_ptr == NULL) {
			memory_warning("point light");
			return(NULL);
		}
		light_ptr->name = light_name;
		light_ptr->next_light_ptr = NULL;
		if (last_light_ptr) {
			last_light_ptr->next_light_ptr = light_ptr;
		} else {
			light_list = light_ptr;
		}
		last_light_ptr = light_ptr;
	}

	// If the style parameter was specified, set the light style, otherwise have
	// it default to 'static'.

	if (parsed_attribute[POINT_LIGHT_STYLE])
		light_ptr->style = point_light_style;
	else
		light_ptr->style = STATIC_POINT_LIGHT;

	// If the position parameter was specified, set the position of the light
	// relative to the block.

	if (parsed_attribute[POINT_LIGHT_POSITION])
		light_ptr->position = point_light_position;

	// If the brightness parameter was specified, set the intensity range of the
	// light.

	if (parsed_attribute[POINT_LIGHT_BRIGHTNESS])
		light_ptr->set_intensity_range(point_light_brightness);

	// If the radius parameter was specified, set the radius of the light
	// source.

	if (parsed_attribute[POINT_LIGHT_RADIUS])
		light_ptr->set_radius(point_light_radius);

	// If the speed parameter was specified, convert the speed in cycles per
	// second to percentage points per second.

	if (parsed_attribute[POINT_LIGHT_SPEED]) {
		if (point_light_speed < 0.0f)
			warning("Light rotational speed must be greater than zero; using "
				"default speed instead");
		else
			light_ptr->set_intensity_speed(point_light_speed);
	}

	// If the flood parameter was specified, set the flood light flag.

	if (parsed_attribute[POINT_LIGHT_FLOOD])
		light_ptr->flood = point_light_flood;

	// If the colour parameter was specified, set the colour of the light.

	if (parsed_attribute[POINT_LIGHT_COLOUR])
		light_ptr->colour = point_light_colour;

	// Return a pointer to the light.

	return(light_ptr);
}

//------------------------------------------------------------------------------
// Parse a popup tag and add a popup to the given list.
//------------------------------------------------------------------------------

static popup *
parse_popup_tag(popup *&popup_list, popup *&last_popup_ptr, bool allow_replacement)
{
	popup *popup_ptr;

	// If the popup has no texture, stream, colour or text, then return without
	// doing anything.

#ifdef STREAMING_MEDIA
		
	if (!parsed_attribute[POPUP_TEXTURE] && !parsed_attribute[POPUP_COLOUR] && 
		!parsed_attribute[POPUP_TEXT] 
		&& !parsed_attribute[POPUP_STREAM]) 
		return(NULL);

#else

	if (!parsed_attribute[POPUP_TEXTURE] && !parsed_attribute[POPUP_COLOUR] && 
		!parsed_attribute[POPUP_TEXT]) 
		return(NULL);

#endif

	// If the name parameter was not specified, initialise it to an empty name.

	if (!parsed_attribute[POPUP_NAME])
		popup_name = "";

	// If allow_replacement is TRUE, check whether a popup of that name already
	// exists in the list, and if so use it.
	//
	// NOTE: This means that there can only be one popup with no name.

	if (allow_replacement)
		popup_ptr = find_popup(popup_list, popup_name);

	// If allow_replacement is FALSE, and the popup name is not empty, then
	// verify no popup with that name exists.
	//
	// NOTE: This means there can be multiple popup with no name.

	else {
		if (strlen(popup_name) > 0 && 
			find_popup(popup_list, popup_name) != NULL) {
			warning("duplicate popup with name \"%s\"", popup_name);
			return(NULL);
		}
		popup_ptr = NULL;
	}

	// If no popup has been found, create a new popup with default settings,
	// set it's name, then add it to the end of the list.

	if (popup_ptr == NULL) {
		NEW(popup_ptr, popup);
		if (popup_ptr == NULL) {
			memory_warning("popup");
			return(NULL);
		}
		popup_ptr->name = popup_name;
		if (last_popup_ptr != NULL)
			last_popup_ptr->next_popup_ptr = popup_ptr;
		else
			popup_list = popup_ptr;
		last_popup_ptr = popup_ptr;
	}

	// If the colour or text parameter was given, set a flag indicating that
	// the foreground texture must be created when the popup is initialised,
	// and create the foreground texture object if it doesn't yet exist.

	if (parsed_attribute[POPUP_COLOUR] || parsed_attribute[POPUP_TEXT]) {
		popup_ptr->create_foreground = true;
		if (popup_ptr->fg_texture_ptr == NULL) {
			NEW(popup_ptr->fg_texture_ptr, texture);
			if (popup_ptr->fg_texture_ptr == NULL)
				memory_error("popup texture");
		}
	} else
		popup_ptr->create_foreground = false;

	// Initialise the background texture if the texture or stream parameter
	// was given.

	if (parsed_attribute[POPUP_TEXTURE]) {
		popup_ptr->bg_texture_URL = popup_texture;
		popup_ptr->bg_texture_ptr = load_texture(custom_blockset_ptr,
			popup_texture, true);
	}

#ifdef STREAMING_MEDIA

	else if (parsed_attribute[POPUP_STREAM])
		popup_ptr->bg_texture_ptr =
			create_video_texture(popup_stream, NULL, true);

#endif

	// Initialise the colour if the colour parameter was given, otherwise
	// make the background transparent.

	if (parsed_attribute[POPUP_COLOUR]) {
		popup_ptr->colour = popup_colour;
		popup_ptr->transparent_background = false;
	} else
		popup_ptr->transparent_background = true;

	// If the size parameter was given, set the popup size (only used if there
	// is no background texture.

	if (parsed_attribute[POPUP_SIZE]) {
		popup_ptr->width = popup_size.width;
		popup_ptr->height = popup_size.height;
	}

	// If the imagemap parameter was given, get a pointer to the imagemap of
	// that name, if it exists.

	if (parsed_attribute[POPUP_IMAGEMAP]) {
		popup_ptr->imagemap_name = popup_imagemap;
		if ((popup_ptr->imagemap_ptr = find_imagemap(popup_imagemap)) == NULL)
			warning("There is no imagemap with name \"%s\"", popup_imagemap);
	}

	// Initialise the popup with whatever additional optional parameters were 
	// parsed.

	if (parsed_attribute[POPUP_PLACEMENT])
		popup_ptr->window_alignment = popup_placement;
	if (parsed_attribute[POPUP_RADIUS])
		popup_ptr->radius_squared = popup_radius * popup_radius;
	if (parsed_attribute[POPUP_BRIGHTNESS])
		popup_ptr->brightness = popup_brightness;
	if (parsed_attribute[POPUP_TEXT])
		popup_ptr->text = popup_text;
	if (parsed_attribute[POPUP_TEXTCOLOUR])
		popup_ptr->text_colour = popup_textcolour;
	if (parsed_attribute[POPUP_TEXTALIGN])
		popup_ptr->text_alignment = popup_textalign;
	if (parsed_attribute[POPUP_TRIGGER])
		popup_ptr->trigger_flags = popup_trigger;

	// If the brightness parameter was present, compute the brightness index
	// and adjust the popup and text colour.

	if (parsed_attribute[POPUP_BRIGHTNESS]) {
		popup_ptr->brightness_index = get_brightness_index(popup_brightness);
		popup_ptr->colour.adjust_brightness(popup_brightness);
		popup_ptr->text_colour.adjust_brightness(popup_brightness);
	}

	// Return a pointer to the popup.

	return(popup_ptr);
}

//------------------------------------------------------------------------------
// Parse a sound tag and add a sound to the given list.
//------------------------------------------------------------------------------

static sound *
parse_sound_tag(sound *&sound_list, sound *&last_sound_ptr, bool allow_replacement,
				blockset *blockset_ptr)
{
	sound *sound_ptr;
	wave *wave_ptr;

	// If the name parameter was not specified, initialise it to an empty name.

	if (!parsed_attribute[SOUND_NAME])
		sound_name = "";

	// If allow_replacement is TRUE, check whether a sound of that name already
	// exists in the list, and if so use it.  If it doesn't have a wave assigned
	// to it and the file parameter is missing, this is an error.
	//
	// NOTE: This means that there can only be one sound with no name.

	if (allow_replacement) {
		sound_ptr = find_sound(sound_list, sound_name);
		if (sound_ptr != NULL && sound_ptr->wave_ptr == NULL &&
			!parsed_attribute[SOUND_FILE]) {
			warning("sound with name \"%s\" does not have a wave file assigned, "
				"and there is no <I>file</I> attribute present", sound_name);
			return(NULL);
		}
	}

	// If allow_replacement is FALSE, and the sound name is not empty, then
	// verify no sound with that name exists.
	//
	// NOTE: This means there can be multiple sounds with no name.

	else {
		if (strlen(sound_name) > 0 && 
			find_sound(sound_list, sound_name) != NULL) {
			warning("duplicate sound with name \"%s\"", sound_name);
			return(NULL);
		}
		sound_ptr = NULL;
	}

	// If the file parameter was given, try to load the wave file.  If this
	// fails, display a warning and return.

	if (parsed_attribute[SOUND_FILE]) {
		if ((wave_ptr = load_wave(blockset_ptr, sound_file)) == NULL) {
			warning("Unable to download wave file from %s", sound_file);
			return(NULL);
		}
	}
 
	// If no sound has been found, create a new sound with default settings,
	// set it's name, then add it to the list.

	if (sound_ptr == NULL) {
		NEW(sound_ptr, sound);
		if (sound_ptr == NULL) {
			memory_warning("sound");
			return(NULL);
		}
		sound_ptr->name = sound_name;
		sound_ptr->next_sound_ptr = sound_list;
		sound_list = sound_ptr;
	}

	// If the file parameter was given, assign the wave to the sound.

	if (parsed_attribute[SOUND_FILE]) {
		sound_ptr->URL = sound_file;
		sound_ptr->wave_ptr = wave_ptr;
	}
	
	// If the volume parameter was given, set the sound volume.

	if (parsed_attribute[SOUND_VOLUME])
		sound_ptr->volume = sound_volume;

	// If the playback parameter was given, set the playback mode of the sound.

	if (parsed_attribute[SOUND_PLAYBACK])
		sound_ptr->playback_mode = sound_playback;

	// If the delay parameter was given, set the delay range of the sound.

	if (parsed_attribute[SOUND_DELAY])
		sound_ptr->delay_range = sound_delay;

	// If the flood parameter was given, set the flood flag.

	if (parsed_attribute[SOUND_FLOOD])
		sound_ptr->flood = sound_flood;

	// If the rolloff parameter was given, set the sound rolloff factor.

	if (parsed_attribute[SOUND_ROLLOFF]) {
		if (sound_rolloff < 0.0f)
			warning("Sound rolloff must be greater than zero; using default rolloff of 1.0");
		else
			sound_ptr->rolloff = sound_rolloff;
	}

	// If the radius parameter was given, and this is a flood sound or a
	// non-flood sound with a "once" or "single" playback mode, set the sound
	// radius.

	if (parsed_attribute[SOUND_RADIUS] && (sound_ptr->flood || 
		sound_ptr->playback_mode == ONE_PLAY ||
		sound_ptr->playback_mode == SINGLE_PLAY))
		sound_ptr->radius = sound_radius;

	// Return a pointer to the sound.

	return(sound_ptr);
}

//------------------------------------------------------------------------------
// Parse a spot light tag, and add a spot light to the given list.
//------------------------------------------------------------------------------

static light *
parse_spot_light_tag(light *&light_list,  light *&last_light_ptr, bool allow_replacement)
{
	string light_name;
	light *light_ptr;

	// If the name parameter was specified, set the light name, otherwise use
	// an empty name.

	if (parsed_attribute[SPOT_LIGHT_NAME])
		light_name = spot_light_name;

	// If allow_replacement is TRUE, check whether a light of that name already
	// exists in the list, and if so use it.
	//
	// NOTE: This means that there can only be one light with no name.

	if (allow_replacement)
		light_ptr = find_light(light_list, light_name);

	// If allow_replacement is FALSE, and the light name is not empty, then
	// verify no light with that name exists.
	//
	// NOTE: This means there an be multiple lights with no name.

	else {
		if (strlen(light_name) > 0 && 
			find_light(light_list, light_name) != NULL) {
			warning("duplicate light with name \"%s\"", light_name);
			return(NULL);
		}
		light_ptr = NULL;
	}

	// If no light has been found, create a new light with default settings,
	// set it's name, then add it to the list.

	if (light_ptr == NULL) {
		NEW(light_ptr, light);
		if (light_ptr == NULL) {
			memory_warning("point light");
			return(NULL);
		}
		light_ptr->name = light_name;
		light_ptr->next_light_ptr = NULL;
		if (last_light_ptr) {
			last_light_ptr->next_light_ptr = light_ptr;
		} else {
			light_list = light_ptr;
		}
		last_light_ptr = light_ptr;	
	}

	// If the style parameter was specified, set the light style, otherwise have
	// it default to 'static'.

	if (parsed_attribute[SPOT_LIGHT_STYLE])
		light_ptr->style = spot_light_style;
	else
		light_ptr->style = STATIC_SPOT_LIGHT;

	// If the position parameter was specified, set the position of the light
	// relative to the block.

	if (parsed_attribute[SPOT_LIGHT_POSITION])
		light_ptr->position = spot_light_position;

	// If the brightness parameter was specified, set the brightness of the
	// light.

	if (parsed_attribute[SPOT_LIGHT_BRIGHTNESS])
		light_ptr->set_intensity(spot_light_brightness);

	// If the radius parameter was specified, set the radius of the light
	// source.

	if (parsed_attribute[SPOT_LIGHT_RADIUS])
		light_ptr->set_radius(spot_light_radius);

	// If the direction parameter was specified, set the direction range of the
	// light, and initialise the current direction.

	if (parsed_attribute[SPOT_LIGHT_DIRECTION])
		light_ptr->set_dir_range(spot_light_direction);

	// If the velocity parameter was specified, set the light rotational speed
	// in degrees per second for the Y and X axes.

	if (parsed_attribute[SPOT_LIGHT_SPEED]) {
		if (spot_light_speed < 0.0f)
			warning("Light rotational speed must be greater than zero; using "
				"default speed instead");
		else
			light_ptr->set_dir_speed(spot_light_speed);
	}

	// If the cone parameter was specified, set the cone angle of the light.
	// The angle given in the cone parameter is assumed to be for generating a
	// diameter, not a radius.

	if (parsed_attribute[SPOT_LIGHT_CONE])
		light_ptr->set_cone_angle(spot_light_cone / 2.0f);

	// If the flood parameter was specified, set the flood light flag.

	if (parsed_attribute[SPOT_LIGHT_FLOOD])
		light_ptr->flood = spot_light_flood;

	// If the colour parameter was specified, set the colour of the light.

	if (parsed_attribute[SPOT_LIGHT_COLOUR])
		light_ptr->colour = spot_light_colour;

	// Return a pointer to the light.

	return(light_ptr);
}

//==============================================================================
// Functions to parse STYLE and certain HEAD tags.
//==============================================================================

//------------------------------------------------------------------------------
// Parse the BLOCK file.
//------------------------------------------------------------------------------

static void
parse_block_file(blockset *blockset_ptr, block_def *block_def_ptr)
{
	int tag_token;

	// If the block is a sprite...

	if (block_def_ptr->type & SPRITE_BLOCK) {
	
		// Reset the flags indicating which tags we've seen.

		got_exit_tag = false;
		got_param_tag = false;
		got_sprite_part_tag = false;

		// Parse all tags that may be present in a sprite block definition,
		// until the closing block tag is reached. 

		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_BLOCK, sprite_block_tag_list, false,
			&tag_token))
			switch (tag_token) {
			case TOKEN_EXIT:
				if (!got_exit_tag) {
					got_exit_tag = true;
					parse_exit_tag(block_def_ptr->exit_ptr);
				}
				break;
			case TOKEN_PARAM:
				parse_sprite_param_tag(block_def_ptr);
				break;
			case TOKEN_PART:
				parse_sprite_part_tag(blockset_ptr, block_def_ptr);
				break;
			case TOKEN_POINT_LIGHT:
				parse_point_light_tag(block_def_ptr->light_list, block_def_ptr->last_light_ptr, true);
				break;
			case TOKEN_SOUND:
				parse_sound_tag(block_def_ptr->sound_list, block_def_ptr->last_sound_ptr, true, blockset_ptr);
				break;
			case TOKEN_SPOT_LIGHT:
				parse_spot_light_tag(block_def_ptr->light_list, block_def_ptr->last_light_ptr, true);
			}
		stop_parsing_nested_tags();

		// Make sure the sprite part tag was present.

		if (!got_sprite_part_tag)
			error("Missing part tag in sprite block definition");
	}

	// If the block is a structural block...

	else {

		// Reset the flags indicating which tags we've seen.

		got_BSP_tree_tag = false;
		got_exit_tag = false;
		got_param_tag = false;
		got_frame_list = false;
		got_loop_list = false;
		got_part_list = false;
		got_vertex_list = false;

		// Parse all tags that may be present in a standard block definition,
		// until the closing block tag is reached.  Note that the vertices tag
		// must appear before the parts tag.

		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_BLOCK, block_tag_list, false,
			&tag_token)) 
			switch (tag_token) {
			case TOKEN_FRAMES:
				if (!got_frame_list) {
					got_frame_list = true;
					parse_frame_list(block_def_ptr);
				}
				break;
			case TOKEN_LOOPS:
				if (!got_loop_list) {
					got_loop_list = true;
					parse_loop_list(block_def_ptr);
				}
				break;
			case TOKEN_BSP_TREE:
				parse_BSP_tree_tag(block_def_ptr);
				break;
			case TOKEN_EXIT:
				if (!got_exit_tag) {
					got_exit_tag = true;
					parse_exit_tag(block_def_ptr->exit_ptr);
				}
				break;
			case TOKEN_PARAM:
				parse_param_tag(block_def_ptr);
				break;
			case TOKEN_PARTS:
				if (!got_vertex_list)
					error("Vertex list must appear before part list");
				parse_part_list(blockset_ptr, block_def_ptr);
				break;
			case TOKEN_POINT_LIGHT:
				parse_point_light_tag(block_def_ptr->light_list, block_def_ptr->last_light_ptr, true);
				break;
			case TOKEN_SOUND:
				parse_sound_tag(block_def_ptr->sound_list, block_def_ptr->last_sound_ptr, true, blockset_ptr);
				break;
			case TOKEN_SPOT_LIGHT:
				parse_spot_light_tag(block_def_ptr->light_list, block_def_ptr->last_light_ptr, true);
				break;
			case TOKEN_VERTICES:
				parse_vertex_list(block_def_ptr);
			}
		stop_parsing_nested_tags();

		// Make sure the vertex list and part list were seen.

		if (!got_vertex_list)
			error("Vertex list was missing in block file");
		if (!got_part_list)
			error("Part list was missing in block file");

		// Create the BSP tree for this block definition.

		block_def_ptr->BSP_tree = 
			create_BSP_node(block_def_ptr->root_polygon_ref, block_def_ptr);
	}
}

//------------------------------------------------------------------------------
// Parse a block tag.
//------------------------------------------------------------------------------

static void
parse_block_tag(blockset *blockset_ptr)
{
	block_def *new_block_def_ptr;
	string file_path;

	// If the new block symbol is a duplicate, then ignore this block.

	if (blockset_ptr->get_block_def(style_block_symbol) != NULL) {
		warning("Block defined with a duplicate symbol");
		return;
	}

	// Push the block file, which is expected to reside in the style zip 
	// archive.  If it can't be found, this is an error.

	file_path = "blocks/";
	file_path += style_block_file;
	if (!push_zip_file(file_path, true))
		error("Unable to open block file %s in the %s blockset", 
			style_block_file, blockset_ptr->name);

	// Create a new block definition.  If we're out of memory, ignore this
	// block.

	NEW(new_block_def_ptr, block_def);
	if (new_block_def_ptr == NULL) {
		pop_file();
		memory_warning("block definition");
		return;
	}

	// Set the block definition's symbol and double symbol (and name if present).

	new_block_def_ptr->single_symbol = style_block_symbol;
	if (parsed_attribute[STYLE_BLOCK_DOUBLE])
		new_block_def_ptr->double_symbol = style_block_double;
	else
		new_block_def_ptr->double_symbol = style_block_symbol;
	if (parsed_attribute[STYLE_BLOCK_NAME])
		new_block_def_ptr->name = style_block_name;

	// Parse the block file.  We do this in a try block so that we can delete
	// the block definition before throwing the error again.

	try {

		// Parse the start of the block file.

		parse_start_of_document(TOKEN_BLOCK, block_attr_list, BLOCK_ATTRIBUTES);

		// The block name is only set if it wasn't already defined in the style 
		// file.  The type and entrance attributes are optional.

		if (strlen(new_block_def_ptr->name) == 0)
			new_block_def_ptr->name = block_name;
		if (parsed_attribute[BLOCK_TYPE])
			new_block_def_ptr->type = block_type;
		if (parsed_attribute[BLOCK_ENTRANCE])
			new_block_def_ptr->allow_entrance = block_entrance;

		// Parse the rest of the block file.

		parse_rest_of_document(true);
		parse_block_file(blockset_ptr, new_block_def_ptr);
		pop_file();
	}
	catch (char *message) {
		DEL(new_block_def_ptr, block_def);
		throw message;
	}

	// If the block name is a duplicate then this is an error, otherwise add
	// it to the blockset.

	if (blockset_ptr->get_block_def(new_block_def_ptr->name) != NULL) {
		DEL(new_block_def_ptr, block_def);
		error("Block defined with a duplicate name");
	}
	blockset_ptr->add_block_def(new_block_def_ptr);
}

//------------------------------------------------------------------------------
// Parse the ground tag.
//------------------------------------------------------------------------------

void
parse_ground_tag(blockset *blockset_ptr)
{
	// If this blockset already has a ground defined, do nothing.

	if (blockset_ptr->ground_defined)
		return;
	blockset_ptr->ground_defined = true;

	// Initialise the ground parameters that were given.

	if (parsed_attribute[GROUND_TEXTURE]) {
		blockset_ptr->ground_texture_URL = ground_texture;
		blockset_ptr->ground_texture_ptr = load_texture(blockset_ptr, ground_texture, true);
	}

#ifdef STREAMING_MEDIA

	else if (parsed_attribute[GROUND_STREAM])
		blockset_ptr->ground_texture_ptr =
			create_video_texture(ground_stream, NULL, false);

#endif

	if (parsed_attribute[GROUND_COLOUR]) {
		blockset_ptr->ground_colour_set = true;
		blockset_ptr->ground_colour = ground_colour;
	}
}

//------------------------------------------------------------------------------
// Parse the orb tag.
//------------------------------------------------------------------------------

static void
parse_orb_tag(blockset *blockset_ptr) 
{
	// If this blockset already has a orb defined, do nothing.

	if (blockset_ptr->orb_defined)
		return;
	blockset_ptr->orb_defined = true;

	// Initialise the orb parameters that were given.

	if (parsed_attribute[ORB_TEXTURE]) {
		blockset_ptr->orb_texture_URL = orb_texture;
		blockset_ptr->orb_texture_ptr = load_texture(blockset_ptr, orb_texture, true);
	}

#ifdef STREAMING_MEDIA

	else if (parsed_attribute[ORB_STREAM])
		blockset_ptr->orb_texture_ptr =
			create_video_texture(orb_stream, NULL, false);

#endif

	if (parsed_attribute[ORB_POSITION]) {
		blockset_ptr->orb_direction_set = true;
		blockset_ptr->orb_direction = orb_position;
	}
	if (parsed_attribute[ORB_BRIGHTNESS]) {
		blockset_ptr->orb_brightness_set = true;
		blockset_ptr->orb_brightness = orb_intensity;
	}
	if (parsed_attribute[ORB_COLOUR]) {
		blockset_ptr->orb_colour_set = true;
		blockset_ptr->orb_colour = orb_colour;
	}

	// If the href parameter was given, create a hyperlink for the orb.

	if (parsed_attribute[ORB_HREF]) {
		hyperlink *exit_ptr;

		NEW(exit_ptr, hyperlink);
		if (exit_ptr == NULL) {
			memory_warning("orb exit");
			return;
		}
		exit_ptr->URL = orb_href;
		if (parsed_attribute[ORB_IS_SPOT])
			exit_ptr->is_spot_URL = orb_is_spot;
		if (parsed_attribute[ORB_TARGET])
			exit_ptr->target = orb_target;
		if (parsed_attribute[ORB_TEXT])
			exit_ptr->label = orb_text;
		blockset_ptr->orb_exit_ptr = exit_ptr;
	}
}

//------------------------------------------------------------------------------
// Parse the placeholder tag.
//------------------------------------------------------------------------------

void
parse_placeholder_tag(blockset *blockset_ptr)
{
	texture *texture_ptr;

	// If this blockset already has a placeholder texture defined, do nothing.

	if (blockset_ptr->placeholder_texture_ptr != NULL)
		return;

	// Download the placeholder texture now, even if it's a custom texture.
	// If the download fails, we won't use a placeholder texture.

	if ((texture_ptr = load_texture(blockset_ptr, placeholder_texture, false)) == NULL || 
		(texture_ptr->blockset_ptr == NULL && (!download_URL(texture_ptr->URL, NULL, false) ||
		 !load_image(texture_ptr->URL, curr_file_path, texture_ptr)))) {
		warning("Unable to download placeholder texture from %s", placeholder_texture);
		if (texture_ptr != NULL)
			DEL(texture_ptr, texture);
		return;
	}

	// Store the pointer to the placeholder texture in the blockset.

	blockset_ptr->placeholder_texture_URL = placeholder_texture;
	blockset_ptr->placeholder_texture_ptr = texture_ptr;
}

//------------------------------------------------------------------------------
// Parse the sky tag.
//------------------------------------------------------------------------------

void
parse_sky_tag(blockset *blockset_ptr)
{
	// If this blockset already has a sky defined, do nothing.

	if (blockset_ptr->sky_defined)
		return;
	blockset_ptr->sky_defined = true;

	// Initialise the sky parameters that were given.

	if (parsed_attribute[SKY_TEXTURE]) {
		blockset_ptr->sky_texture_URL = sky_texture;
		blockset_ptr->sky_texture_ptr = load_texture(blockset_ptr, sky_texture, true);
	}

#ifdef STREAMING_MEDIA

	else if (parsed_attribute[SKY_STREAM])
		blockset_ptr->sky_texture_ptr = 
			create_video_texture(sky_stream, NULL, false);

#endif

	if (parsed_attribute[SKY_COLOUR]) {
		blockset_ptr->sky_colour_set = true;
		blockset_ptr->sky_colour = sky_color;
	}
	if (parsed_attribute[SKY_BRIGHTNESS]) {
		blockset_ptr->sky_brightness_set = true;
		blockset_ptr->sky_brightness = sky_intensity;
	}
}

//==============================================================================
// Functions to parse HEAD tags.
//==============================================================================

//------------------------------------------------------------------------------
// Parse the ambient light tag.
//------------------------------------------------------------------------------

static void
parse_ambient_light_tag(void)
{
	// If we've seen an ambient light tag already, this is an error.

	if (got_ambient_light_tag) {
		warning("Duplicate <I>ambient_light</I> tag");
		return;
	}
	got_ambient_light_tag = true;

	// If the ambient colour was not specified, choose white as the default.

	if (!parsed_attribute[AMBIENT_LIGHT_COLOUR])
		ambient_light_colour.set_RGB(255, 255, 255);

	// Set the ambient light.

	set_ambient_light(ambient_light_brightness, ambient_light_colour);
}

//------------------------------------------------------------------------------
// Parse the ambient sound tag.
//------------------------------------------------------------------------------

static void
parse_ambient_sound_tag(void)
{
	wave *wave_ptr;

	// If we've seen an ambient sound tag already, this is an error.

	if (got_ambient_sound_tag) {
		warning("Duplicate <I>ambient_sound</I> tag");
		return;
	}
	got_ambient_sound_tag = true;

	// Load the wave from the given wave file.  If this fails, generate a 
	// warning and return.

	if ((wave_ptr = load_wave(custom_blockset_ptr, ambient_sound_file)) == 
		NULL) {
		warning("Unable to download wave from URL %s", ambient_sound_file);
		return;
	}

	// Create the ambient sound with default settings.  If this fails, generate
	// a warning and return, otherwise set it's ambient flag and wave pointer.

	NEW(ambient_sound_ptr, sound);
	if (ambient_sound_ptr == NULL) {
		memory_warning("ambient sound");
		return;
	}
	ambient_sound_ptr->ambient = true;
	ambient_sound_ptr->URL = ambient_sound_file;
	ambient_sound_ptr->wave_ptr = wave_ptr;

	// If the volume attribute was given, set the sound volume.

	if (parsed_attribute[AMBIENT_SOUND_VOLUME])
		ambient_sound_ptr->volume = ambient_sound_volume;

	// If the playback attribute was given, set the playback mode of the sound.

	if (parsed_attribute[AMBIENT_SOUND_PLAYBACK])
		ambient_sound_ptr->playback_mode = ambient_sound_playback;

	// If the delay attribute was given, set the delay range of the sound.

	if (parsed_attribute[AMBIENT_SOUND_DELAY])
		ambient_sound_ptr->delay_range = ambient_sound_delay;
}

//------------------------------------------------------------------------------
// Parse the BASE tag.
//------------------------------------------------------------------------------

static void
parse_base_tag(void)
{
	int length;

	// If we've seen a base tag already, this is an error.

	if (got_base_tag) {
		warning("Duplicate <I>base</I> tag");
		return;
	}
	got_base_tag = true;

	// Set the spot URL directory.

	spot_URL_dir = base_href;
	if ((length = strlen(spot_URL_dir) - 1) >= 0 && 
		spot_URL_dir[length] != '/' && spot_URL_dir[length] != '\\')
		spot_URL_dir += "/";
}

//------------------------------------------------------------------------------
// Parse the blockset, creating a blockset object and returning a pointer to it.
//------------------------------------------------------------------------------

static blockset *
parse_blockset(char *blockset_URL)
{
	char *file_name_ptr, *ext_ptr;
	string blockset_name, style_file_name;
	blockset *blockset_ptr;
	int tag_token;

	// Extract the blockset file name, and remove the ".bset" extension.
	// This is stored as the blockset name.

	file_name_ptr = strrchr(blockset_URL, '/');
	blockset_name = file_name_ptr + 1;
	ext_ptr = strrchr(blockset_name, '.');
	blockset_name.truncate(ext_ptr - (char *)blockset_name);

	// Set the title to reflect we're trying to load a blockset.

	set_title("Loading %s blockset", blockset_name);

	// Open the blockset.

	if (!open_blockset(blockset_URL, blockset_name))
		error("Unable to open the %s blockset with URL %s", 
			blockset_name, blockset_URL);

	// Add a ".style" extension to the blockset name, and open the file in
	// the blockset that has this name.

	style_file_name = blockset_name;
	style_file_name += ".style";
	if (!push_zip_file(style_file_name, true))
		error("Unable to open %s from the %s blockset", style_file_name,
			blockset_name);

	// Create the blockset object, and initialise it's URL and name.

	NEW(blockset_ptr, blockset);
	if (blockset_ptr == NULL)
		memory_error("blockset");
	blockset_ptr->URL = blockset_URL;
	blockset_ptr->name = blockset_name;

	// Parse the style file.  This is done in a try block so that we
	// can delete the blockset on an error.

	try {
		parse_start_of_document(TOKEN_STYLE, style_attr_list, STYLE_ATTRIBUTES);
		parse_rest_of_document(true);
		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_STYLE, style_tag_list, false,
			&tag_token)) 
			switch (tag_token) {
			case TOKEN_BLOCK:
				parse_block_tag(blockset_ptr);
				break;
			case TOKEN_GROUND:
				parse_ground_tag(blockset_ptr);
				break;
			case TOKEN_ORB:
				parse_orb_tag(blockset_ptr);
				break;
			case TOKEN_PLACEHOLDER:
				parse_placeholder_tag(blockset_ptr);
				break;
			case TOKEN_SKY:
				parse_sky_tag(blockset_ptr);
			}
		stop_parsing_nested_tags();

		// Stop parsing the style file, close it and the blockset, and return a
		// pointer to it.

		pop_file();
		close_zip_archive();
		return(blockset_ptr);
	}

	// Delete the blockset, close the zip archive, and throw the error again.

	catch (char *message) {
		DEL(blockset_ptr, blockset);
		close_zip_archive();
		throw message;
	}
}

//------------------------------------------------------------------------------
// Parse the blockset tag.
//------------------------------------------------------------------------------
		
static void
parse_blockset_tag(void)
{
	char *ext_ptr;
	blockset *blockset_ptr;

	// Verify the blockset URL points to a ".bset" file.

	ext_ptr = strrchr(blockset_href, '.');
	if (ext_ptr == NULL || _stricmp(ext_ptr, ".bset")) {
		warning("URL %s is not a blockset", blockset_href);
		return;
	}

	// Indicate that a valid blockset tag was seen.

	got_blockset_tag = true;

	// If the blockset has already been loaded, there is no need to reload it.

	if (blockset_list_ptr->find_blockset(blockset_href))
		return;

	// If the blockset exists in the old blockset list, move it to the new
	// blockset list.

	if (old_blockset_list_ptr && (blockset_ptr = 
		old_blockset_list_ptr->remove_blockset(blockset_href)) != NULL) {
		blockset_list_ptr->add_blockset(blockset_ptr);
		return;
	}

	// Load and parse the blockset, then add it to the blockset list.

	blockset_ptr = parse_blockset(blockset_href);
	blockset_list_ptr->add_blockset(blockset_ptr);
}

//------------------------------------------------------------------------------
// Parse the debug tag.
//------------------------------------------------------------------------------

static void
parse_debug_tag(void)
{
	// If we've seen a debug tag already, this is an error.

	if (got_debug_tag) {
		warning("Duplicate <I>debug</I> tag");
		return;
	}
	got_debug_tag = true;

	// If the warning attribute was given, and it is TRUE, then show both
	// errors and warnings.  Otherwise only show errors.

	if (parsed_attribute[DEBUG_WARNINGS] && debug_warnings)
		spot_debug_level = SHOW_ERRORS_AND_WARNINGS;
	else
		spot_debug_level = SHOW_ERRORS_ONLY; 
}

//------------------------------------------------------------------------------
// Parse a meta tag.
//------------------------------------------------------------------------------

static void
parse_meta_tag(void)
{
	metadata *metadata_ptr;

	// Create a new metadata object, and add it to the end of the metadata list.

	NEW(metadata_ptr, metadata);
	if (metadata_ptr == NULL) {
		memory_warning("metadata");
		return;
	}
	metadata_ptr->name = meta_name;
	metadata_ptr->content = meta_content;
	metadata_ptr->next_metadata_ptr = NULL;
	if (last_metadata_ptr != NULL) {
		last_metadata_ptr->next_metadata_ptr = metadata_ptr;
	} else {
		first_metadata_ptr = metadata_ptr;
	}
	last_metadata_ptr = metadata_ptr;
}

//------------------------------------------------------------------------------
// Parse a fog tag.
//------------------------------------------------------------------------------

static void
parse_fog_tag(fog *fog_ptr)
{
	// If we've seen a fog tag already, this is an error.

	if (global_fog_enabled) {
		warning("Duplicate <I>fog</I> tag");
		return;
	}
	global_fog_enabled = true;

	// Enable global fog with the specified colour, start and end radii.

	if (parsed_attribute[FOG_STYLE])
		fog_ptr->style = fog_style;
	else
		fog_ptr->style = LINEAR_FOG;
	if (parsed_attribute[FOG_COLOUR])
		fog_ptr->colour = fog_colour;
	else
		fog_ptr->colour.set_RGB(255, 255, 255);
	fog_ptr->colour.normalise();
	if (parsed_attribute[FOG_START])
		fog_ptr->start_radius = fog_start;
	else
		fog_ptr->start_radius = 0.0f;
	if (parsed_attribute[FOG_END])
		fog_ptr->end_radius = fog_end;
	else
		fog_ptr->end_radius = 0.0f;
	if (parsed_attribute[FOG_DENSITY])
		fog_ptr->density = fog_density;
	else
		fog_ptr->density = 1.0f;
}

//------------------------------------------------------------------------------
// Parse the map tag to obtain the dimensions and scale of the map.
//------------------------------------------------------------------------------

static void
parse_map_tag(void)
{
	mapcoords size;

	// If we've seen a map tag already, this is an error.

	if (got_map_tag) {
		warning("Duplicate <I>map</I> tag");
		return;
	}
	got_map_tag = true;

	// Store the map dimensions in the world object. We add one level so that
	// there is always a level of empty space above the last defined level.

	world_ptr->columns = map_dimensions.column;
	world_ptr->rows = map_dimensions.row;
	world_ptr->levels = map_dimensions.level + 1;

	// If the style attribute was given, set the map style.

	if (parsed_attribute[MAP_STYLE])
		world_ptr->map_style = map_style;
}

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Parse the stream tag.
//------------------------------------------------------------------------------

static void
parse_stream_tag(void)
{
	// If we've seen a stream tag already, or neither RP or WMP attributes were
	// given, this is an error.

	if (got_stream_tag)
		warning("Duplicate <I>stream</I> tag");
	got_stream_tag = true;
	if (!parsed_attribute[STREAM_RP] && !parsed_attribute[STREAM_WMP])
		diagnose("At least one of the <I>rp</I> or <I>wmp</I> attributes is "
			"required");
	stream_set = true;

	// The name attribute is mandatory.

	name_of_stream = stream_name;

	// One of the rp and wmp attributes is optional.

	if (parsed_attribute[STREAM_RP])
		rp_stream_URL = create_stream_URL(stream_rp);
	if (parsed_attribute[STREAM_WMP])
		wmp_stream_URL = create_stream_URL(stream_wmp);
}

#endif

//------------------------------------------------------------------------------
// Parse the title tag, and set the spot title.
//------------------------------------------------------------------------------

static void
parse_title_tag(void)
{
	// If we've seen a title tag already, this is an error.

	if (got_title_tag) {
		warning("Duplicate <I>title</I> tag");
		return;
	}
	got_title_tag = true;

	// Set the spot title.

	spot_title = title_name;
}

//==============================================================================
// Functions to parse ACTION tags.
//==============================================================================

//------------------------------------------------------------------------------
// Parse an action exit tag.
//------------------------------------------------------------------------------

static action *
parse_action_exit_tag(void)
{
	action *action_ptr;

	// If we've already seen an exit tag in this action tag, this is an error.

	if (got_action_exit_tag) {
		warning("Duplicate <I>exit</I> tag");
		return(NULL);
	}
	got_action_exit_tag = true;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = EXIT_ACTION;
	action_ptr->next_action_ptr = NULL;
	
	// Initialise the action with whatever parameter were parsed.

	action_ptr->exit_URL = exit_href;
	action_ptr->is_spot_URL = parsed_attribute[EXIT_IS_SPOT];
	if (parsed_attribute[EXIT_TARGET])
		action_ptr->exit_target = exit_target;

	// Return a pointer to the action.

	return(action_ptr);
}


//------------------------------------------------------------------------------
// Parse an action ripple tag.
//------------------------------------------------------------------------------

static action *
parse_action_ripple_tag(block_def *block_def_ptr)
{
	action *action_ptr;
	int n;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = RIPPLE_ACTION;
	action_ptr->next_action_ptr = NULL;

	// Set the style value
	if (parsed_attribute[RIPPLE_STYLE])
		action_ptr->style = ripple_style;
	else
		action_ptr->style = RAIN_RIPPLE;

	// Set the force values.

	if (parsed_attribute[RIPPLE_FORCE]){
		action_ptr->force = ripple_force / TEXELS_PER_UNIT;
	} else {
		action_ptr->force = 15.0 / TEXELS_PER_UNIT;
	}

	// Set the droprate value.

	if (parsed_attribute[RIPPLE_DROPRATE]){
		action_ptr->droprate = ripple_droprate;
	} else {
		action_ptr->droprate = (float)0.25;
	}

	// Set the damp value.

	if (parsed_attribute[RIPPLE_DAMP]){
		action_ptr->damp = ripple_damp;
	} else {
		action_ptr->damp = (float)0.85;
	}

	// Make two arrays to handle the timestep of forces for rippling.

	NEWARRAY(action_ptr->prev_step_ptr, float, block_def_ptr->vertices);
	if (action_ptr->prev_step_ptr == NULL) {
		memory_warning("ripple");
		return(NULL);
	}

	NEWARRAY(action_ptr->curr_step_ptr, float, block_def_ptr->vertices);
	if (action_ptr->curr_step_ptr == NULL) {
		memory_warning("ripple");
		return(NULL);
	}

	for (n = 0; n < block_def_ptr->vertices; n++) {
		action_ptr->prev_step_ptr[n] = 0.0;
		action_ptr->curr_step_ptr[n] = 0.0;
	}
	action_ptr->vertices = block_def_ptr->vertices;

	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse an action spin tag.
//------------------------------------------------------------------------------

static action *
parse_action_spin_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = SPIN_ACTION;
	action_ptr->next_action_ptr = NULL;

	// Set the rotation values.

	if (parsed_attribute[SPIN_ANGLES]) {
		action_ptr->spin_angles.x = spin_angles.x;
		action_ptr->spin_angles.y = spin_angles.y;
		action_ptr->spin_angles.z = spin_angles.z;
	} 
	else {
		action_ptr->spin_angles.x = 0.0f;
		action_ptr->spin_angles.y = 0.0f;
		action_ptr->spin_angles.z = 0.0f;
	}

	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse an action orbit tag.
//------------------------------------------------------------------------------

static action *
parse_action_orbit_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = ORBIT_ACTION;
	action_ptr->next_action_ptr = NULL;

	// Set the rotation values.

	if (parsed_attribute[ORBIT_DISTANCE]) {
		action_ptr->spin_angles.x = orbit_distance.x;
		action_ptr->spin_angles.y = orbit_distance.y;
		action_ptr->spin_angles.z = orbit_distance.z;
	} else {
		action_ptr->spin_angles.x = 0.0f;
		action_ptr->spin_angles.y = 0.0f;
		action_ptr->spin_angles.z = 0.0f;
	}

	// Set the source block symbol or location.

	if (parsed_attribute[ORBIT_SOURCE]) {
		action_ptr->source = orbit_source;
	} else {
		action_ptr->source.is_symbol = false;
	}

	// Set the speed value.

	if (parsed_attribute[ORBIT_SPEED]){
		action_ptr->speed = orbit_speed;
	} else {
		action_ptr->speed = 40;
	}

	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse an action move tag.
//------------------------------------------------------------------------------

static action *
parse_action_move_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = MOVE_ACTION;
	action_ptr->next_action_ptr = NULL;

	if (parsed_attribute[MOVE_PATTERN]) {
		action_ptr->exit_URL = move_pattern; // store the move instructions inside the exit string
		action_ptr->charindex = action_ptr->exit_URL.text;
	}
	else 
		action_ptr->charindex = NULL;

	action_ptr->totalx = action_ptr->totaly = action_ptr->totalz = 0;
	action_ptr->speedx = action_ptr->speedy = action_ptr->speedz = 0;
	action_ptr->temp = 0;

	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse an action replace tag.
//------------------------------------------------------------------------------

static action *
parse_action_replace_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = REPLACE_ACTION;
	action_ptr->next_action_ptr = NULL;

	// Set the source block symbol or location.

	action_ptr->source = replace_source;

	// If the target parameter was given, set the target block location,
	// otherwise make the target location be the trigger location.

	if (parsed_attribute[REPLACE_TARGET]) {
		action_ptr->target_is_trigger = false;
		action_ptr->target = replace_target;
	} else
		action_ptr->target_is_trigger = true;
	
	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse a setframe action tag.
//------------------------------------------------------------------------------

static action *
parse_action_setframe_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = SETFRAME_ACTION;
	action_ptr->next_action_ptr = NULL;

	// If the frame parameter was given set it

	if (parsed_attribute[SETFRAME_NUMBER]) {
		action_ptr->rel_number = setframe_number;
	} else {
		action_ptr->rel_number.value = 1;
		action_ptr->rel_number.relative_value = true;
	}
	
	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse animate action tag.
//------------------------------------------------------------------------------

static action *
parse_action_animate_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = ANIMATE_ACTION;
	action_ptr->next_action_ptr = NULL;

	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse stop action tags.
//------------------------------------------------------------------------------

static action *
parse_action_stop_tag(int type)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = type;
	action_ptr->next_action_ptr = NULL;

	// Return a pointer to the action.

	return(action_ptr);
}

//------------------------------------------------------------------------------
// Parse set loop action tag.
//------------------------------------------------------------------------------

static action *
parse_action_setloop_tag(void)
{
	action *action_ptr;

	// Create an action object, and initialise it's type and next action
	// pointer.

	NEW(action_ptr, action);
	if (action_ptr == NULL) {
		memory_warning("action");
		return(NULL);
	}
	action_ptr->type = SETLOOP_ACTION;
	action_ptr->next_action_ptr = NULL;

	// If the loop number parameter was given set it

	if (parsed_attribute[SETLOOP_NUMBER]) {
		action_ptr->rel_number = setloop_number;
	} else {
		action_ptr->rel_number.value = 1;
		action_ptr->rel_number.relative_value = true;
	}
	
	// Return a pointer to the action.

	return(action_ptr);
}

//==============================================================================
// Functions to parse ACTION tags.
//==============================================================================

//------------------------------------------------------------------------------
// Parse rectangle coordinates. 
//------------------------------------------------------------------------------

static bool
parse_rect_coords(int tag_token, char *rect_coords, rect *rect_ptr)
{
	start_parsing_value(tag_token, TOKEN_COORDS, rect_coords, false);
	if (!parse_integer_in_value(&rect_ptr->x1) || 
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&rect_ptr->y1) || 
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&rect_ptr->x2) || 
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&rect_ptr->y2) || 
		!stop_parsing_value(true))
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Parse circle coordinates. 
//------------------------------------------------------------------------------

static bool
parse_circle_coords(int tag_token, char *circle_coords, circle *circle_ptr)
{
	start_parsing_value(tag_token, TOKEN_COORDS, circle_coords, false);
	if (!parse_integer_in_value(&circle_ptr->x) || 
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&circle_ptr->y) || 
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&circle_ptr->r) || 
		!stop_parsing_value(true))
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Parse an imagemap action tag.
//------------------------------------------------------------------------------

static void
parse_imagemap_action_tag(imagemap *imagemap_ptr)
{
	trigger *trigger_ptr;
	action *action_ptr, *last_action_ptr, *action_list;
	rect rect_shape;
	circle circle_shape;
	int tag_token;

	// Create a trigger.  If this fails, skip the action tag.

	if ((trigger_ptr = new_trigger()) == NULL) {
		memory_warning("trigger");
		return;
	}

	// If the trigger parameter was given, set the trigger flag.

	if (parsed_attribute[ACTION_TRIGGER])
		trigger_ptr->trigger_flag = action_trigger;

	// If the text parameter was given, set the trigger label.

	if (parsed_attribute[ACTION_TEXT])
		trigger_ptr->label = action_text;

	// Initialise the action list.

	action_list = NULL;
	last_action_ptr = NULL;

	// We haven't seen an exit tag yet.

	got_action_exit_tag = false;

	// Parse all nested action tags.  This is done in a try block so that we
	// can delete the trigger on an error.

	try {
		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_ACTION, action_tag_list, false,
			&tag_token)) {

			// Parse the replace or exit tag.

			switch (tag_token) {
			case TOKEN_EXIT:
				action_ptr = parse_action_exit_tag();
				break;
			case TOKEN_REPLACE:
				action_ptr = parse_action_replace_tag();
				break;
			}

			// Add the action to the end of the action list, if it was parsed.

			if (action_ptr != NULL) {
				if (last_action_ptr != NULL)
					last_action_ptr->next_action_ptr = action_ptr;
				else
					action_list = action_ptr;
				last_action_ptr = action_ptr;
			}
		}
		stop_parsing_nested_tags();
	}
	catch (char *message) {
		DEL(trigger_ptr, trigger);
		throw message;
	}

	// Store pointer to action list in trigger.

	trigger_ptr->action_list = action_list;
	trigger_ptr->part_index = ALL_PARTS;

	// Parse the coords parameter based upon the area shape, then add the
	// area with the trigger to the imagemap.

	switch (action_shape) {
	case RECT_SHAPE:
		if (parse_rect_coords(TOKEN_ACTION, action_coords, &rect_shape))
			imagemap_ptr->add_area(&rect_shape, trigger_ptr);
		break;
	case CIRCLE_SHAPE:
		if (parse_circle_coords(TOKEN_ACTION, action_coords, &circle_shape))
			imagemap_ptr->add_area(&circle_shape, trigger_ptr);
	}
}

//------------------------------------------------------------------------------
// Parse an imagemap area tag.
//------------------------------------------------------------------------------

static void
parse_imagemap_area_tag(imagemap *imagemap_ptr)
{
	rect rect_shape;
	circle circle_shape;
	hyperlink *exit_ptr;

	// Create and initialise the exit object.  If this fails, ignore this area
	// tag.

	NEW(exit_ptr, hyperlink);
	if (exit_ptr == NULL)
		return;
	exit_ptr->URL = area_href;
	if (parsed_attribute[AREA_IS_SPOT])
		exit_ptr->is_spot_URL = true;
	if (parsed_attribute[AREA_TARGET])
		exit_ptr->target = area_target;
	if (parsed_attribute[AREA_TEXT])
		exit_ptr->label = area_text;

	// Parse the coords parameter based upon the area shape, then add the
	// area to the imagemap.

	switch (area_shape) {
	case RECT_SHAPE:
		if (parse_rect_coords(TOKEN_AREA, area_coords, &rect_shape))
			imagemap_ptr->add_area(&rect_shape, exit_ptr);
		break;
	case CIRCLE_SHAPE:
		if (parse_circle_coords(TOKEN_AREA, area_coords, &circle_shape))
			imagemap_ptr->add_area(&circle_shape, exit_ptr);
	}
}

//------------------------------------------------------------------------------
// Parse an imagemap script tag.
//------------------------------------------------------------------------------

static void
parse_imagemap_script_tag(imagemap *imagemap_ptr)
{
	trigger *trigger_ptr;
	rect rect_shape;
	circle circle_shape;
	string script;

	// Create a trigger.  If this fails, skip the script tag.

	if ((trigger_ptr = new_trigger()) == NULL) {
		memory_warning("trigger");
		return;
	}

	// If the trigger parameter was given, set the trigger flag.  Only
	// "roll on", "roll off" and "click on" triggers are permitted; if any
	// other trigger is given, pretend the trigger parameter wasn't seen.

	if (parsed_attribute[ACTION_TRIGGER]) {
		if (action_trigger != ROLL_ON && action_trigger != ROLL_OFF &&
			action_trigger != CLICK_ON)
			warning("Expected 'roll on', 'roll off' or 'click on' as the "
				"value for the <I>trigger</I> attribute; using default value "
				"for this attribute instead");
		else
			trigger_ptr->trigger_flag = action_trigger;
	}

	// If the text parameter was given, set the trigger label.

	if (parsed_attribute[ACTION_TEXT])
		trigger_ptr->label = action_text;

	// Parse the nested text inside the script tag as a script.

	script = nested_text_to_string(TOKEN_SCRIPT);
	if ((trigger_ptr->script_def_ptr = create_script_def(script)) == NULL) {
		memory_warning("trigger script");
		delete trigger_ptr;
		return;
	}
	trigger_ptr->part_index = ALL_PARTS;

	// Parse the coords parameter based upon the area shape, then add the
	// area with the trigger to the imagemap.

	switch (action_shape) {
	case RECT_SHAPE:
		if (parse_rect_coords(TOKEN_SCRIPT, action_coords, &rect_shape))
			imagemap_ptr->add_area(&rect_shape, trigger_ptr);
		break;
	case CIRCLE_SHAPE:
		if (parse_circle_coords(TOKEN_SCRIPT, action_coords, &circle_shape))
			imagemap_ptr->add_area(&circle_shape, trigger_ptr);
	}
}

//==============================================================================
// Functions to parse the BODY tags.
//==============================================================================

//------------------------------------------------------------------------------
// Update the texture coordinates for all polygons in the given part by the
// given texture angle.
//------------------------------------------------------------------------------

static void
update_textures_in_part(block_def *block_def_ptr, part *part_ptr,
	bool recompute_tex_coords)
{
	// Step through each polygon that belongs to this part, and update it's
	// texture coordinates.

	for (int polygon_no = 0; polygon_no < block_def_ptr->polygons; 
		polygon_no++) {
		polygon_def *polygon_def_ptr = 
			&block_def_ptr->polygon_def_list[polygon_no];
		if (polygon_def_ptr->part_ptr == part_ptr) {
			if (recompute_tex_coords)
				compute_texture_coordinates(polygon_def_ptr, 
					part_ptr->texture_style, part_ptr->projection, 
					block_def_ptr);
			rotate_texture_coordinates(polygon_def_ptr, 
				part_ptr->texture_angle);
		}
	}
}

//------------------------------------------------------------------------------
// Parse a part tag inside a create tag.
//------------------------------------------------------------------------------

static void
parse_create_part_tag(block_def *block_def_ptr)
{
	texture *texture_ptr = NULL;
	int part_no;
	part *part_ptr;

	// If the texture parameter was given, then load the texture.

	if (parsed_attribute[PART_TEXTURE])
		texture_ptr = load_texture(custom_blockset_ptr, part_texture, true);

#ifdef STREAMING_MEDIA

	// Otherwise if the stream parameter was given, then create a video texture.
	// If the rect parameter was also given, create a video texture for that
	// rectangle only.

	else if (parsed_attribute[PART_STREAM]) {
		if (parsed_attribute[PART_RECT])
			texture_ptr = create_video_texture(part_stream, &part_rect, false);
		else
			texture_ptr = create_video_texture(part_stream, NULL, false);
	}

#endif

	// If the faces parameter was given, verify that it's within range; if not,
	// ignore it.

	if (parsed_attribute[PART_FACES] && (part_faces < 0 || part_faces > 2)) {
		warning("A part must have 0, 1 or 2 faces per polygon; using "
			"default value instead");
		parsed_attribute[PART_FACES] = false;
	}
	 
	// If the part name expression is "*", we modify all parts in the block
	// definition.
	
	if (*part_name == '*') {
		for (part_no = 0; part_no < block_def_ptr->parts; part_no++) {
			part_ptr = &block_def_ptr->part_list[part_no];

			// Update the part fields.

			update_common_part_fields(part_ptr, texture_ptr, part_colour, 
				part_translucency, part_style);
			if (parsed_attribute[PART_FACES])
				part_ptr->faces = part_faces;
			if (parsed_attribute[PART_ANGLE])
				part_ptr->texture_angle = part_angle;
			if (parsed_attribute[PART_PROJECTION])
				part_ptr->projection = part_projection;
			if (parsed_attribute[PART_SOLID])
				part_ptr->solid = part_solid;

			// Update the textures in the part.

			update_textures_in_part(block_def_ptr, part_ptr, 
				parsed_attribute[PART_STYLE] || 
				parsed_attribute[PART_PROJECTION]);
		}
	}

	// Otherwise modify all matching groups in the block definition.  Undefined
	// part names are ignored.

	else {
		char *curr_part_name;

		// Step through the list of part names.

		curr_part_name = strtok(part_name, ",");
		while (curr_part_name) {

			// Search for this name in the block definition's part list.

			for (part_no = 0; part_no < block_def_ptr->parts; part_no++) {
				part_ptr = &block_def_ptr->part_list[part_no];
				if (!_stricmp(curr_part_name, part_ptr->name))
					break;
			}

			// If not found, generate a warning message.

			if (part_no == block_def_ptr->parts)
				warning("There is no part with name \"%s\"", curr_part_name);

			// Otherwise update the part fields and textures in the part.

			else {
				update_common_part_fields(part_ptr, texture_ptr, part_colour, 
					part_translucency, part_style);
				if (parsed_attribute[PART_FACES])
					part_ptr->faces = part_faces;
				if (parsed_attribute[PART_ANGLE])
					part_ptr->texture_angle = part_angle;
				if (parsed_attribute[PART_PROJECTION])
					part_ptr->projection = part_projection;
				if (parsed_attribute[PART_SOLID])
					part_ptr->solid = part_solid;
				update_textures_in_part(block_def_ptr, part_ptr, 
					parsed_attribute[PART_STYLE] || 
					parsed_attribute[PART_PROJECTION]);
			}
			curr_part_name = strtok(NULL, ",");
		}
	}
}

//------------------------------------------------------------------------------
// Parse an action tag.
//------------------------------------------------------------------------------

static trigger *
parse_action_tag(block_def *block_def_ptr, bool need_location_param)
{
	trigger *trigger_ptr;
	action *action_ptr, *last_action_ptr, *action_list;
	int tag_token;
	int part_no;

	// If the LOCATION parameter is missing and it is required, this is an 
	// error.

	got_location_param = parsed_attribute[ACTION_LOCATION];
	if (!got_location_param && need_location_param)
		error("Stand-alone <I>action</I> tag must have a <I>location</I> "
			"attribute");

	// If the trigger is "key up/down/hold", make sure the key code parameter
	// has also been given.

	if (parsed_attribute[ACTION_TRIGGER] && !parsed_attribute[ACTION_KEY] &&
		(action_trigger & KEY_TRIGGERS) != 0)
		error("Expected the <I>key</I> attribute in the <I>action</I> tag");

	// Create a trigger.  If this fails, skip over the rest of the action tag.

	if ((trigger_ptr = new_trigger()) == NULL) {
		memory_warning("trigger");
		return(NULL);
	}

	// Initialize the part ptr to all parts which is the default.

	if (!need_location_param) {
		trigger_ptr->part_name = "*";
		trigger_ptr->part_index = ALL_PARTS;
	}

	// If the trigger parameter was given, set the trigger flag.

	if (parsed_attribute[ACTION_TRIGGER]) 
		trigger_ptr->trigger_flag = action_trigger;

	// If the radius parameter was given, set the trigger radius, otherwise
	// use a default radius of one block.

	if (parsed_attribute[ACTION_RADIUS]) {
		trigger_ptr->radius = action_radius;
	} else {
		trigger_ptr->radius = UNITS_PER_BLOCK;
	}
	trigger_ptr->radius_squared = trigger_ptr->radius * trigger_ptr->radius;

	// If the delay parameter was given, set the delay range of the trigger.

	if (parsed_attribute[ACTION_DELAY])
		trigger_ptr->delay_range = action_delay;
	else {
		trigger_ptr->delay_range.min_delay_ms = 1000;
		trigger_ptr->delay_range.delay_range_ms = 0;
	}

	// If the text parameter as given, set the label of the trigger.

	if (parsed_attribute[ACTION_TEXT])
		trigger_ptr->label = action_text;

	// If the target parameter was given, set the target location.

	if (parsed_attribute[ACTION_TARGET])
		trigger_ptr->target = action_target;

	// If the key parameter was gievn, set the key code.

	if (parsed_attribute[ACTION_KEY])
		trigger_ptr->key_code = action_key;

	// If the part parameter was given, set the part name.

	if (parsed_attribute[ACTION_PART_NAME] && !need_location_param) {
		trigger_ptr->part_name = action_part_name;
		if (!_stricmp(action_part_name, "*")) 
			trigger_ptr->part_index = ALL_PARTS;
		else {
		  
			// Search for this name in the block definition's part list.
			
			for (part_no = 0; part_no < block_def_ptr->parts; part_no++) {
				if (!_stricmp(action_part_name, block_def_ptr->part_list[part_no].name)) 
					break;
			}

			// If not found, generate a warning message.

			if (part_no == block_def_ptr->parts) {
				warning("There is no part with name \"%s\"", (char *)action_part_name);
				trigger_ptr->part_index = ALL_PARTS;
			}

			// Otherwise hold the part number in the trigger.

			else {
				trigger_ptr->part_index = part_no;
			}
		}
	}

	// Initialise the action list.

	action_list = NULL;
	last_action_ptr = NULL;

	// We haven't seen an exit tag yet.

	got_action_exit_tag = false;

	// Parse all action tags.  This is done in a try block so we can delete the trigger on an error.

	try {
		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_ACTION, action_tag_list, false, &tag_token)) {

			// Parse the replace or exit tag.

			switch (tag_token) {
			case TOKEN_EXIT:
				action_ptr = parse_action_exit_tag();
				break;
			case TOKEN_REPLACE:
				action_ptr = parse_action_replace_tag();
				break;
			case TOKEN_RIPPLE:
				action_ptr = parse_action_ripple_tag(block_def_ptr);
				break;
			case TOKEN_SPIN:
				action_ptr = parse_action_spin_tag();
				break;
			case TOKEN_ORBIT:
				action_ptr = parse_action_orbit_tag();
				break;
			case TOKEN_MOVE:
				action_ptr = parse_action_move_tag();
				break;
			case TOKEN_SETFRAME:
				action_ptr = parse_action_setframe_tag();
				break;
			case TOKEN_ANIMATE:
				action_ptr = parse_action_animate_tag();
				break;
			case TOKEN_STOPSPIN:
				action_ptr = parse_action_stop_tag(STOPSPIN_ACTION);
				break;
			case TOKEN_STOPMOVE:
				action_ptr = parse_action_stop_tag(STOPMOVE_ACTION);
				break;
			case TOKEN_STOPRIPPLE:
				action_ptr = parse_action_stop_tag(STOPRIPPLE_ACTION);
				break;
			case TOKEN_STOPORBIT:
				action_ptr = parse_action_stop_tag(STOPORBIT_ACTION);
				break;
			case TOKEN_SETLOOP:
				action_ptr = parse_action_setloop_tag();
				break;
			}

			// Add the action to the end of the action list, if it was parsed.

			if (action_ptr != NULL) {
				if (last_action_ptr != NULL)
					last_action_ptr->next_action_ptr = action_ptr;
				else
					action_list = action_ptr;
				last_action_ptr = action_ptr;

				// Also connect the action to this trigger.

				action_ptr->trigger_ptr = trigger_ptr;
			}
		}
		stop_parsing_nested_tags();

		// Store pointer to action list in trigger, and return the trigger.

		trigger_ptr->action_list = action_list;
		return(trigger_ptr);
	}
	catch (char *message) {
		DEL(trigger_ptr, trigger);
		throw message;
	}
}

//------------------------------------------------------------------------------
// Parse the define tag.
//------------------------------------------------------------------------------

static void
parse_define_tag(string &script)
{
	// Convert all nested tags contained with the define tag into a string.

	script = nested_tags_to_string();
}

//------------------------------------------------------------------------------
// Parse entrance tag, and add an entrance to the specified list, then return
// a pointer to the entrance.
//------------------------------------------------------------------------------

static entrance *
parse_entrance_tag(entrance **entrance_list, entrance **last_entrance_ptr = NULL)
{
	entrance *entrance_ptr;

	// If the angle parameter wasn't specified, set the entrance direction
	// to 0,0.

	if (!parsed_attribute[ENTRANCE_ANGLE])
		entrance_angle.set(0.0f, 0.0f);
 
	// Create the entrance.

	NEW(entrance_ptr, entrance);
	if (entrance_ptr == NULL) {
		memory_warning("entrance");
		return(NULL);
	}

	// Initialise the entrance, and add it to the given entrance list.

	entrance_ptr->name = entrance_name;
	entrance_ptr->initial_direction = entrance_angle;
	entrance_ptr->next_entrance_ptr = NULL;
	if (last_entrance_ptr && *last_entrance_ptr) {
		(*last_entrance_ptr)->next_entrance_ptr = entrance_ptr;
	} else {
		*entrance_list = entrance_ptr;
	}
	if (last_entrance_ptr) {
		*last_entrance_ptr = entrance_ptr;
	}

	// Return a pointer to the entrance.

	return(entrance_ptr);
}

//------------------------------------------------------------------------------
// Parse imagemap tag.
//------------------------------------------------------------------------------

static void
parse_imagemap_tag(void)
{
	imagemap *imagemap_ptr;
	int tag_token;

	// Create a new imagemap object and add it to the imagemap list.

	imagemap_ptr = add_imagemap(imagemap_name);

	// Parse all imagemap tags.

	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_IMAGEMAP, imagemap_tag_list, false,
		&tag_token))
		switch (tag_token) {
		case TOKEN_ACTION:
			parse_imagemap_action_tag(imagemap_ptr);
			break;
		case TOKEN_AREA:
			parse_imagemap_area_tag(imagemap_ptr);
			break;
		case TOKEN_SCRIPT:
			parse_imagemap_script_tag(imagemap_ptr);
		}
	stop_parsing_nested_tags();
}

//------------------------------------------------------------------------------
// Parse the level tag.
//------------------------------------------------------------------------------

static void
parse_level_tag(void)
{
	entity *entity_ptr;
	const char *line_ptr;
	char ch1, ch2;
	int max_level;
	int column, row, level;
	square *row_ptr;

	// If the number parameter was not given, use the last level number + 1.

	if (!parsed_attribute[LEVEL_NUMBER]) {
		level_number = last_level + 1;
	}

	// Verify the level number is within range.

	max_level = world_ptr->ground_level_exists ? world_ptr->levels - 2 : world_ptr->levels - 1;
	if (level_number < 1 || level_number > max_level) {
		error("Level %d is out of range (should be between 1 and %d)", level_number, max_level);
	}

	// The level index is one less than the level number if the ground level does not exist.

	level = world_ptr->ground_level_exists ? level_number : level_number - 1;

	// Verify the level number hasn't been seen before, then mark it as having been seen.

	if (world_ptr->level_defined_list[level]) {
		error("Level %d has been seen already", level_number);
	}
	world_ptr->level_defined_list[level] = true;

	// Get a pointer to the nested text entity (or create an empty one).  Then store the entity in the world
	// object for later access.

	entity_ptr = nested_text_entity(TOKEN_LEVEL, true);
	world_ptr->set_level_entity(level, entity_ptr);

	// Skip over all whitespace at the beginning of the map text.

	line_ptr = entity_ptr->text;
	ch1 = *line_ptr;
	while (ch1 == ' ' || ch1 == '\t' || ch1 == '\n')
		ch1 = *++line_ptr;

	// Now read all rows in this level.

	for (row = 0; row < world_ptr->rows; row++) {

		// If we've reach the end of the text in the level tag, we're done.

		ch1 = *line_ptr;
		if (ch1 == '\0')
			break;

		// Skip over the leading white space in this row.

		while (ch1 == ' ' || ch1 == '\t')
			ch1 = *++line_ptr;
		
		// Get a pointer to the row in the map.

		row_ptr = world_ptr->get_square_ptr(0, row, level);

		// Parse the block single or double character symbols in this row,
		// until the expected number of symbols have been parsed or the end of
		// the line has been reached.  Whitespace is ignored.

		column = 0;
		switch (world_ptr->map_style) {
		case SINGLE_MAP:
			while (ch1 != '\0' && ch1 != '\n' && column < world_ptr->columns) {
				if (not_single_symbol(ch1, false))
					warning("Symbol at location (%d, %d, %d) was invalid", column + 1, row + 1, level_number);
				else
					row_ptr->orig_block_symbol = ch1;
				row_ptr++;
				column++;
				ch1 = *++line_ptr;
				while (ch1 == ' ' || ch1 == '\t')
					ch1 = *++line_ptr;
			}
			break;
		case DOUBLE_MAP:
			while (ch1 != '\0' && ch1 != '\n' && column < world_ptr->columns) {
				ch2 = *++line_ptr;
				if (ch2 == '\0' || ch2 == '\n' || ch2 == ' ' || ch2 == '\t' ||
					not_double_symbol(ch1, ch2, false)) {
					warning("Symbol at location (%d, %d, %d) was invalid", column + 1, row + 1, level_number);
					if (ch2 == '\0' || ch2 == '\n') {
						ch1 = ch2;
						break;
					}
				} else if (ch1 == '.')
					row_ptr->orig_block_symbol = ch2;
				else	
					row_ptr->orig_block_symbol = (ch1 << 7) + ch2;
				row_ptr++;
				column++;
				ch1 = *++line_ptr;
				while (ch1 == ' ' || ch1 == '\t')
					ch1 = *++line_ptr;
			}
		}

		// Skip to the start of the next row, if there is one.

		while (ch1 != '\n' && ch1 != '\0')
			ch1 = *line_ptr++;
		if (ch1 == '\n')
			line_ptr++;
	}

	// Remember this as the last level.

	last_level = level_number;
}

//------------------------------------------------------------------------------
// Parse the load tag.
//------------------------------------------------------------------------------

static void
parse_load_tag(void)
{
	load_tag *load_tag_ptr;

	// Either the texture or sound parameter must be present, but not both.

	if ((!parsed_attribute[LOAD_TEXTURE] && !parsed_attribute[LOAD_SOUND]) ||
		(parsed_attribute[LOAD_TEXTURE] && parsed_attribute[LOAD_SOUND])) {
		warning("Expected one texture or sound attribute; "
			"ignoring load tag");
		return;
	}

	// Create a new load_tag object, and add it to the end of the load_tag list.

	NEW(load_tag_ptr, load_tag);
	if (load_tag_ptr == NULL) {
		memory_warning("load_tag");
		return;
	}
	load_tag_ptr->next_load_tag_ptr = NULL;
	if (last_load_tag_ptr != NULL) {
		last_load_tag_ptr->next_load_tag_ptr = load_tag_ptr;
	} else {
		first_load_tag_ptr = load_tag_ptr;
	}
	last_load_tag_ptr = load_tag_ptr;

	// Initialize the load tag, and load the specified texture or wave URL.

	if (parsed_attribute[LOAD_TEXTURE]) {
		load_tag_ptr->is_texture_href = true;
		load_tag_ptr->href = load_texture_href;
		load_texture(custom_blockset_ptr, load_texture_href, true);
	} else {
		load_tag_ptr->is_texture_href = false;
		load_tag_ptr->href = load_sound_href;
		load_wave(custom_blockset_ptr, load_sound_href);
	}
}

//------------------------------------------------------------------------------
// Parse player tag.
//------------------------------------------------------------------------------

static void
parse_player_tag(void)
{
	// If we've seen the player tag before, just return without doing anything.

	if (got_player_tag)
		return;
	got_player_tag = true;

	// Save the block symbol if it were given, otherwise set the symbol to zero.

	if (parsed_attribute[PLAYER_BLOCK]) {
		player_block_symbol = player_block;
	} else {
		player_block_symbol = 0;
	}

	// Save the player size if it were given, otherwise set the size to a
	// default value.

	if (parsed_attribute[PLAYER_SIZE]) {
		got_player_size = true;
		player_dimensions = player_size;
	} else {
		player_dimensions.set(1.2f, 1.5f, 1.2f);
	}

	// Save the camera offset if it were given, otherwise select a default
	// camera offset.

	if (parsed_attribute[PLAYER_CAMERA]) {
		got_player_camera = true;
		player_camera_offset.dx = player_camera.x;
		player_camera_offset.dy = player_camera.y;
		player_camera_offset.dz = player_camera.z;
	} else {
		player_camera_offset.dx = 0.0f;
		player_camera_offset.dy = 0.0f;
		player_camera_offset.dz = -10.0f;
	}
}

//------------------------------------------------------------------------------
// Parse a script tag.
//------------------------------------------------------------------------------

static trigger *
parse_script_tag(block_def *block_def_ptr, bool need_location_param)
{
	trigger *trigger_ptr;
	string script;
	int part_no;
	part *part_ptr;

	// If the LOCATION parameter is missing and it is required, and the trigger is
	// not "timer", this is an error.

	got_location_param = parsed_attribute[ACTION_LOCATION];
	if (!got_location_param && need_location_param &&
		(!parsed_attribute[ACTION_TRIGGER] || action_trigger != TIMER))
		error("Stand-alone <I>script</I> tag with a <I>trigger</I> attribute "
			"other than \"timer\" must have a <I>location</I> attribute");

	// If the trigger is "key up/down/hold", make sure the key code parameter
	// has also been given.

	if (parsed_attribute[ACTION_TRIGGER] && !parsed_attribute[ACTION_KEY] &&
		(action_trigger & KEY_TRIGGERS) != 0)
		error("Expected the <I>key</I> attribute in the <I>script</I> tag");

	// If the LOCATION parameter is not missing, and the trigger is "timer",
	// indicate that the LOCATION parameter is not present.

	if (got_location_param && parsed_attribute[ACTION_TRIGGER] &&
		action_trigger == TIMER)
		got_location_param = false;

	// Create a trigger.  If this fails, skip the script tag.

	if ((trigger_ptr = new_trigger()) == NULL) {
		memory_warning("trigger");
		return(NULL);
	}

	// Initialize the part_ptr to be all parts which is the default.

    if (!need_location_param) {
		trigger_ptr->part_name = "*";
	    trigger_ptr->part_index = ALL_PARTS;
	}

	// If the trigger parameter was given, set the trigger flag.

	if (parsed_attribute[ACTION_TRIGGER])
		trigger_ptr->trigger_flag = action_trigger;

	// If the radius parameter was given, set the trigger radius, otherwise
	// use a default radius of one block.

	if (parsed_attribute[ACTION_RADIUS])
		trigger_ptr->radius_squared = action_radius * action_radius;
	else
		trigger_ptr->radius_squared = UNITS_PER_BLOCK * UNITS_PER_BLOCK;

	// If the delay parameter was given, set the delay range of the trigger.

	if (parsed_attribute[ACTION_DELAY])
		trigger_ptr->delay_range = action_delay;
	else {
		trigger_ptr->delay_range.min_delay_ms = 1000;
		trigger_ptr->delay_range.delay_range_ms = 0;
	}

	// If the text parameter as given, set the label of the trigger.

	if (parsed_attribute[ACTION_TEXT])
		trigger_ptr->label = action_text;

	// If the target parameter was given, set the target location.

	if (parsed_attribute[ACTION_TARGET])
		trigger_ptr->target = action_target;
	
	// If the key parameter was gievn, set the key codes.

	if (parsed_attribute[ACTION_KEY])
		trigger_ptr->key_code = action_key;

	// If the part parameter was given, set the part name.

	if (parsed_attribute[ACTION_PART_NAME] && !need_location_param) {
		trigger_ptr->part_name = action_part_name;
		if (!_stricmp(action_part_name, "*")) {
			trigger_ptr->part_index = ALL_PARTS;
		} else {

			// Search for this name in the block definition's part list.

            for (part_no = 0; part_no < block_def_ptr->parts; part_no++) {
				part_ptr = &block_def_ptr->part_list[part_no];
				if (!_stricmp(action_part_name, part_ptr->name))
					break;
			}

			// If not found, generate a warning message.

			if (part_no == block_def_ptr->parts) {
				warning("There is no part with name \"%s\"", (char *)action_part_name);
				trigger_ptr->part_index = ALL_PARTS;
			}

			// Otherwise hold the part name pointer in the trigger.

			else {
				trigger_ptr->part_index = part_no;
			}
		}
	}

	// Create a script definition for this script, and store it in the trigger.

	script = nested_text_to_string(TOKEN_SCRIPT);
	if ((trigger_ptr->script_def_ptr = create_script_def(script)) == NULL) {
		memory_warning("trigger script");
		delete trigger_ptr;
		return(NULL);
	}
	return(trigger_ptr);
}

//------------------------------------------------------------------------------
// Parse the next create tag.  This function may be called recursively if an 
// IMPORT tag is parsed (and is allowed).
//------------------------------------------------------------------------------

static void
parse_next_create_tag(int tag_token, tag_def *create_tag_list, 
					  block_def *custom_block_def_ptr, bool allow_import_tag)
{
	popup *popup_ptr;
	trigger *trigger_ptr; 
	string script;

	switch (tag_token) {
	case TOKEN_PART:
		parse_create_part_tag(custom_block_def_ptr);
		break;

	case TOKEN_PARAM:
		if (custom_block_def_ptr->type & SPRITE_BLOCK) 
			parse_sprite_param_tag(custom_block_def_ptr);
		else
			parse_param_tag(custom_block_def_ptr);
		break;

	case TOKEN_POINT_LIGHT:
		parse_point_light_tag(custom_block_def_ptr->light_list, custom_block_def_ptr->last_light_ptr, true);
		break;

	case TOKEN_SPOT_LIGHT:
		parse_spot_light_tag(custom_block_def_ptr->light_list, custom_block_def_ptr->last_light_ptr, true);
		break;

	case TOKEN_SOUND:
		parse_sound_tag(custom_block_def_ptr->sound_list, custom_block_def_ptr->last_sound_ptr, true, custom_blockset_ptr);
		break;

	case TOKEN_POPUP:

		// Parse the popup tag and add it to the custom block definition's
		// popup list.

		popup_ptr = parse_popup_tag(custom_block_def_ptr->popup_list, custom_block_def_ptr->last_popup_ptr, true);

		// Update the custom block definition's popup trigger flags.

		custom_block_def_ptr->popup_trigger_flags |= popup_ptr->trigger_flags;
		break;

	case TOKEN_ENTRANCE:
		if (custom_block_def_ptr->entrance_ptr == NULL) {
			if (custom_block_def_ptr->allow_entrance) {
				parse_entrance_tag(&custom_block_def_ptr->entrance_ptr);
			} else {
				warning("Block with name \"%s\" does not permit an entrance", custom_block_def_ptr->name);
			}
		} else {
			warning("Duplicate entrance tag in block with name \"%s\"", custom_block_def_ptr->name);
		}
		break;

	case TOKEN_EXIT:
		if (!got_exit_tag) {
			got_exit_tag = true;
			custom_block_def_ptr->custom_exit = true;
			parse_exit_tag(custom_block_def_ptr->exit_ptr);
		} else {
			warning("Duplicate exit tag in block with name \"%s\"", custom_block_def_ptr->name);
		}
		break;

	case TOKEN_ACTION:
	case TOKEN_SCRIPT:

		// Parse the ACTION or SCRIPT tag and create a trigger.

		if (tag_token == TOKEN_ACTION)
			trigger_ptr = parse_action_tag(custom_block_def_ptr, false);
		else
			trigger_ptr = parse_script_tag(custom_block_def_ptr, false);

		// Add the trigger the end of the trigger list.

		if (trigger_ptr != NULL) {
			if (custom_block_def_ptr->last_trigger_ptr != NULL)
				custom_block_def_ptr->last_trigger_ptr->next_trigger_ptr = trigger_ptr;
			else
				custom_block_def_ptr->trigger_list = trigger_ptr;
			custom_block_def_ptr->last_trigger_ptr = trigger_ptr;

			// Update the trigger flags so we know what kind of triggers are in the list.

			custom_block_def_ptr->trigger_flags |= trigger_ptr->trigger_flag;
		}

		break;

	case TOKEN_DEFINE:

		// Parse the DEFINE tag and append the script to the custom block's script.

		parse_define_tag(script);
		custom_block_def_ptr->script += script;
		break;

	case TOKEN_IMPORT:

		// If an import tag is allowed...

		if (allow_import_tag) {
		
			// Download and open the file specified in the HREF attribute.

			if (!download_URL(import_href, NULL, true))
				error("Unable to import file from URL %s", import_href);
			if (!push_file(curr_file_path, import_href, true))
				error("Unable to open file %s", curr_file_path);

			// Parse the imported file.

			parse_start_of_document(TOKEN_IMPORT, NULL, 0); 
			parse_rest_of_document(spot_XML_compliance);
			start_parsing_nested_tags();
			while (parse_next_nested_tag(TOKEN_IMPORT, create_tag_list, false,
				&tag_token)) 
				parse_next_create_tag(tag_token, create_tag_list,
					custom_block_def_ptr, false);
			stop_parsing_nested_tags();
			pop_file();
		}

		// Generate an error for nested IMPORT tags.

		else
			error("Nested <I>import</I> tags are not permitted");
	}
}

//------------------------------------------------------------------------------
// Parse the create tag.
//------------------------------------------------------------------------------

static void
parse_create_tag(void)
{
	char single_symbol;
	word double_symbol;
	block_def *custom_block_def_ptr, *block_def_ptr;
	tag_def *create_tag_list;
	int tag_token;

	// Check whether the source block exists; if it doesn't, skip over the
	// rest of the create tag and return.  Note that spots designed prior to
	// Rover 3.0 permitted either a single or double symbol, regardless of the
	// map style.  Spots designed after Rover 3.0 must use symbols that match
	// the map style.

	if (string_to_single_symbol(create_block, &single_symbol, true)) {
		if (world_ptr->map_style == DOUBLE_MAP && 
			min_rover_version >= 0x03000000) {
			warning("Expected a double-character symbol rather than \"%s\"",
				create_block);
			return;
		} else if ((block_def_ptr = get_block_def(single_symbol)) == NULL) {
			warning("There is no block with single symbol \"%s\"", 
				create_block);
			return;
		}
	} else if (string_to_double_symbol(create_block, &double_symbol, true)) {
		if (world_ptr->map_style == SINGLE_MAP && 
			min_rover_version >= 0x03000000) {
			warning("Expected a single-character symbol rather than \"%s\"",
				create_block);
			return;
		} else if ((block_def_ptr = get_block_def(double_symbol)) == NULL) {
			warning("There is no block with double symbol \"%s\"", 
				create_block);
			return;
		}
	} else if ((block_def_ptr = get_block_def(create_block)) == NULL) {
		warning("There is no block with name \"%s\"", create_block);
		return;
	}

	// Allocate a new custom block definition structure; if this fails, skip
	// the create tag.

	NEW(custom_block_def_ptr, block_def);
	if (custom_block_def_ptr == NULL) {
		memory_warning("custom block definition");
		return;
	}

	// If the custom block symbol exists, delete the custom block definition
	// currently assigned to it, removing it from the symbol table as well.

	if (custom_blockset_ptr->delete_block_def(create_symbol)) {
		warning("Created block with duplicate symbol; this block definition "
			"will replace the previous block definition");
		block_symbol_table[create_symbol] = NULL;
	}

	// Copy the source block definition to the target block definition, set
	// it's symbol, and add it to the custom blockset and symbol table.

	custom_block_def_ptr->dup_block_def(block_def_ptr);
	custom_block_def_ptr->source_block = create_block;
	switch (world_ptr->map_style) {
	case SINGLE_MAP:
		custom_block_def_ptr->single_symbol = (char)create_symbol;
		break;
	case DOUBLE_MAP:
		custom_block_def_ptr->double_symbol = create_symbol;
	}
	custom_blockset_ptr->add_block_def(custom_block_def_ptr);
	block_symbol_table[create_symbol] = custom_block_def_ptr;

	// Initialise some flags.

	got_param_tag = false;
	got_exit_tag = false;

	// Choose the appropiate create tag list.

	if (custom_block_def_ptr->type & SPRITE_BLOCK)
		create_tag_list = sprite_create_tag_list;
	else
		create_tag_list = structural_create_tag_list;

	// Parse all create tags.  If there were none (meaning it is an exact duplicate
	// of the source block definition), have the source block definition point to the
	// custom block definition, if it doesn't already point to one.  This is used
	// in build mode if the source block definition ends up being hidden and we need
	// to use the custom block definition in its place.

	bool found_nested_tag = false;
	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_CREATE, create_tag_list, false, &tag_token)) {
		found_nested_tag = true;
		parse_next_create_tag(tag_token, create_tag_list, custom_block_def_ptr, true);
	}
	stop_parsing_nested_tags();
	if (!found_nested_tag && block_def_ptr->custom_dup_block_def_ptr == NULL) {
		block_def_ptr->custom_dup_block_def_ptr = custom_block_def_ptr;
	}
}

//==============================================================================
// Functions to parse the SPOT tags.
//==============================================================================

//------------------------------------------------------------------------------
// Parse the next body tag.  This function may be called recursively if an 
// IMPORT tag is parsed (and is allowed).
//------------------------------------------------------------------------------

static void
parse_next_body_tag(int tag_token, bool allow_import_tag)
{
	entrance *entrance_ptr;
	square *square_ptr;
	light *light_ptr;
	sound *sound_ptr;
	vertex translation;
	trigger *trigger_ptr, *last_trigger_ptr;
	popup *popup_ptr;
	string script;

	switch (tag_token) {
	case TOKEN_CREATE:
		parse_create_tag();
		break;

	case TOKEN_DEFINE:
		parse_define_tag(script);
		global_script += script;
		break;

	case TOKEN_ENTRANCE:

		// Parse the entrance tag, add an entrance to the global entrance
		// list.

		entrance_ptr = parse_entrance_tag(&global_entrance_list, &last_global_entrance_ptr);

		// Store the pointer to the square in the entrance.

		if (entrance_ptr != NULL) {
			square_ptr = world_ptr->get_square_ptr(entrance_location.column,
				entrance_location.row, entrance_location.level);
			entrance_ptr->square_ptr = square_ptr;
			entrance_ptr->block_ptr = NULL;
		}
		break;

	case TOKEN_EXIT:

		// If the square at the given map coordinates does not already have
		// an exit, then parse the exit tag and add the exit to the square.

		square_ptr = world_ptr->get_square_ptr(exit_location.column,
			exit_location.row, exit_location.level);
		if (square_ptr->exit_ptr == NULL)
			parse_exit_tag(square_ptr->exit_ptr);
		break;

	case TOKEN_IMAGEMAP:
		parse_imagemap_tag();
		break;

	case TOKEN_IMPORT:

		// If an import tag is allowed...

		if (allow_import_tag) {
		
			// Download and open the file specified in the HREF attribute.

			if (!download_URL(import_href, NULL, true))
				error("Unable to import file from URL %s", import_href);
			if (!push_file(curr_file_path, import_href, true))
				error("Unable to open file %s", curr_file_path);

			// Parse the imported file.

			parse_start_of_document(TOKEN_IMPORT, NULL, 0);
			parse_rest_of_document(spot_XML_compliance);
			start_parsing_nested_tags();
			while (parse_next_nested_tag(TOKEN_IMPORT, body_tag_list, false,
				&tag_token))
				parse_next_body_tag(tag_token, false);
			stop_parsing_nested_tags();
			pop_file();
		}

		// Generate an error for nested IMPORT tags.

		else
			error("A nested <I>import</I> tag is not permitted");
		break;
	
	case TOKEN_LEVEL:
		parse_level_tag();
		break;
	case TOKEN_LOAD:
		parse_load_tag();
		break;
	case TOKEN_PLAYER:
		parse_player_tag();
		break;

	case TOKEN_POINT_LIGHT:

		// Parse the point light tag, adding the point light to the global
		// light list.

		light_ptr = parse_point_light_tag(global_light_list, last_global_light_ptr, false);

		// Save the light's map coordinates.

		if (light_ptr != NULL) {
			light_ptr->map_coords = point_light_location;
		}
		break;

	case TOKEN_POPUP:

		// Parse the popup tag, adding the popup to the global popup list.

		popup_ptr = parse_popup_tag(global_popup_list, 
			last_global_popup_ptr, false);

		// If the location parameter was given...

		if (parsed_attribute[POPUP_LOCATION]) {

			// Calculate the popup's position on the map.

			popup_ptr->position.set_map_position(popup_location.column, 
				popup_location.row, popup_location.level);

			// Store the pointer to the square in the popup

			square_ptr = world_ptr->get_square_ptr(popup_location.column,
				popup_location.row, popup_location.level);
			popup_ptr->square_ptr = square_ptr;
			popup_ptr->block_ptr = NULL;

			// Add the popup to the end of the square's popup list.

			if (square_ptr->last_popup_ptr != NULL)
				square_ptr->last_popup_ptr->next_square_popup_ptr = 
					popup_ptr;
			else
				square_ptr->popup_list = popup_ptr;
			square_ptr->last_popup_ptr = popup_ptr;

			// Update the square's popup trigger flags.

			square_ptr->popup_trigger_flags |= popup_ptr->trigger_flags;
		} 

		// Otherwise set a flag that indicates the popup is always visible.
		
		else {
			popup_ptr->square_ptr = NULL;
			popup_ptr->block_ptr = NULL;
			popup_ptr->always_visible = true;
		}
		break;
	
	case TOKEN_SOUND:

		// Parse the sound tag, adding the sound to the global sound list.

		sound_ptr = parse_sound_tag(global_sound_list, last_global_sound_ptr, false, custom_blockset_ptr);

		// Save the sound's map coordinates.

		if (sound_ptr != NULL) {
			sound_ptr->map_coords = sound_location;
		}
		break;

	case TOKEN_ACTION:
	case TOKEN_SCRIPT:

		// Parse the ACTION or SCRIPT tag and create a trigger.

		if (tag_token == TOKEN_ACTION)
			trigger_ptr = parse_action_tag(NULL, true);
		else
			trigger_ptr = parse_script_tag(NULL, true);

		// If a trigger was successfully created...

		if (trigger_ptr != NULL) {

			// Get a pointer to the action location's square and store
			// it in the trigger, if available.

			if (got_location_param) {
				square_ptr = world_ptr->get_square_ptr(action_location.column,
					action_location.row, action_location.level);
				trigger_ptr->square_ptr = square_ptr;
			} else
				trigger_ptr->square_ptr = NULL;

			// If this is a global trigger, add it to the global trigger list.
			// Note that "timer" and key triggers don't use a location.

			if (trigger_ptr->trigger_flag == STEP_IN ||
				trigger_ptr->trigger_flag == STEP_OUT ||
				trigger_ptr->trigger_flag == PROXIMITY ||
				trigger_ptr->trigger_flag == LOCATION)
				add_trigger_to_global_list(trigger_ptr, 
					action_location.column, action_location.row,
					action_location.level);
			else if (trigger_ptr->trigger_flag == TIMER ||
				trigger_ptr->trigger_flag == KEY_DOWN ||
				trigger_ptr->trigger_flag == KEY_UP ||
				trigger_ptr->trigger_flag == KEY_HOLD)
				add_trigger_to_global_list(trigger_ptr, 0, 0, 0);

			// Otherwise add it to the end of the square's trigger list,
			// and update the square's trigger flags.

			else {
				last_trigger_ptr = square_ptr->last_trigger_ptr;
				if (last_trigger_ptr != NULL)
					last_trigger_ptr->next_trigger_ptr = trigger_ptr;
				else
					square_ptr->trigger_list = trigger_ptr;
				square_ptr->last_trigger_ptr = trigger_ptr;
				square_ptr->trigger_flags |= trigger_ptr->trigger_flag;
			}
		}
		break;
	
	case TOKEN_SPOT_LIGHT:

		// Parse the spot light tag, adding the spot light to the global
		// light list.

		light_ptr = parse_spot_light_tag(global_light_list, last_global_light_ptr, false);

		// Save the map coordinates for the light.

		if (light_ptr != NULL) {
			light_ptr->map_coords = spot_light_location;
		}
	}
}

//------------------------------------------------------------------------------
// Parse all body tags.
//------------------------------------------------------------------------------

static void
parse_body_tags(void)
{
	blockset *blockset_ptr;
	block_def *block_def_ptr;
	word block_symbol;
	int tag_token;

	// If the body has been seen before, or the head tag hasn't yet been seen,
	// then this is an error.

	if (!got_head_tag)
		error("Missing <I>head</I> tag");
	if (got_body_tag) {
		warning("Duplicate <I>body</I> tag");
		return;
	}
	got_body_tag = true;

	// Display a message in the title indicating that we're loading the map.

	set_title("Loading map");

	// Add the block definitions in the blocksets to the symbol table.  Once a
	// symbol has been allocated, it cannot be reassigned to a block definition
	// in a later blockset.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		block_def_ptr = blockset_ptr->block_def_list;
		while (block_def_ptr != NULL) {
			switch (world_ptr->map_style) {
			case SINGLE_MAP:
				block_symbol = block_def_ptr->single_symbol;
				break;
			case DOUBLE_MAP:
				block_symbol = block_def_ptr->double_symbol;
			}
			if (block_symbol_table[block_symbol] == NULL)
				block_symbol_table[block_symbol] = block_def_ptr;
			block_def_ptr = block_def_ptr->next_block_def_ptr;
		}
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}

	// If the ground tag was seen in the header, add an additional level to the
	// map dimensions.

	if (world_ptr->ground_level_exists)
		world_ptr->levels++;

	// Create the square map.
	
	if (!world_ptr->create_square_map())
		memory_error("square map");

	// If the ground tag was seen in the header, initialise the bottommost
	// level with ground blocks.

	if (world_ptr->ground_level_exists)
		for (int row = 0; row < world_ptr->rows; row++)
			for (int column = 0; column < world_ptr->columns; column++) {
				square *square_ptr = world_ptr->get_square_ptr(column, row, 0);
				square_ptr->orig_block_symbol = GROUND_BLOCK_SYMBOL;
			}

	// Initialise some state variables.

	got_player_tag = false;
	got_player_size = false;
	got_player_camera = false;
	last_level = 0;

	// Parse the body tags.

	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_BODY, body_tag_list, false, &tag_token))
		parse_next_body_tag(tag_token, true);
	stop_parsing_nested_tags();
}

//------------------------------------------------------------------------------
// Parse all head tags.
//------------------------------------------------------------------------------

static void
parse_head_tags(void)
{
	int tag_token;

	// If the head tag has been seen already, or has appeared after the body
	// tag, this is an error.

	if (got_body_tag)
		error("The <I>head</I> tag must appear before the <I>body</I> tag");
	if (got_head_tag) {
		warning("Duplicate <I>head</I> tag");
		return;
	}
	got_head_tag = true;

	// Initialise some static variables.

	got_ambient_light_tag = false;
	got_ambient_sound_tag = false;
	got_base_tag = false;
	got_blockset_tag = false;
	got_client_tag = false;
	got_debug_tag = false;
	got_map_tag = false;
#ifdef STREAMING_MEDIA
	got_stream_tag = false;
#endif
	got_server_tag = false;
	got_title_tag = false;

	// Parse the content tags inside the head tag.
	
	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_HEAD, head_tag_list, false, &tag_token)) 
		switch (tag_token) {
		case TOKEN_AMBIENT_LIGHT:
			parse_ambient_light_tag();
			break;
		case TOKEN_AMBIENT_SOUND:
			parse_ambient_sound_tag();
			break;
		case TOKEN_BASE:
			parse_base_tag();
			break;
		case TOKEN_BLOCKSET:
			parse_blockset_tag();
			break;
		case TOKEN_DEBUG:
			parse_debug_tag();
			break;
		case TOKEN_FOG:
			parse_fog_tag(&global_fog);
			break;
		case TOKEN_GROUND:
			parse_ground_tag(custom_blockset_ptr);
			world_ptr->ground_level_exists = true;
			break;
		case TOKEN_META:
			parse_meta_tag();
			break;
		case TOKEN_MAP:
			parse_map_tag();
			break;
		case TOKEN_ORB:
			parse_orb_tag(custom_blockset_ptr);
			break;
		case TOKEN_PLACEHOLDER:
			parse_placeholder_tag(custom_blockset_ptr);
			break;
		case TOKEN_SKY:
			parse_sky_tag(custom_blockset_ptr);
			break;
#ifdef STREAMING_MEDIA
		case TOKEN_STREAM:
			parse_stream_tag();
			break;
#endif
		case TOKEN_TITLE:
			parse_title_tag();
		}
	stop_parsing_nested_tags();

	// Check that the blockset and map tags were seen.

	if (!got_blockset_tag)
		error("No valid blockset tags were seen in head tag");
	if (!got_map_tag)
		error("Map tag was not seen in head tag");

	// If no ambient_light tag was seen, set the ambient brightness to 100% and
	// the ambient colour to white.

	if (!got_ambient_light_tag) {
		ambient_light_colour.set_RGB(255, 255, 255);
		set_ambient_light(1.0f, ambient_light_colour);
	}
}

//==============================================================================
// Parse the spot file.
//==============================================================================

void
parse_spot_file(char *spot_URL, char *spot_file_path)
{
	FILE *fp;
	int tag_token;

	// Reset various parse variables.

	spot_entity_list = NULL;
	curr_load_index = 0;
	global_fog_enabled = false;

#ifdef STREAMING_MEDIA

	stream_set = false;
	name_of_stream = "";
	rp_stream_URL = "";
	wmp_stream_URL = "";
	unscaled_video_texture_ptr = NULL;
	scaled_video_texture_list = NULL;

#endif

	// Clear and initialise the error log file.

	if ((fp = fopen(error_log_file_path, "w")) != NULL) {
		fprintf(fp, "<HTML>\n<HEAD>\n<TITLE>Error log</TITLE>\n</HEAD>\n");
		fprintf(fp, "<BODY BGCOLOR=\"#ffcc66\">\n");
		fclose(fp);
	}

	// Delete the old blockset list, if it exists.  This will only happen if the
	// previous spot failed to load.

	if (old_blockset_list_ptr != NULL)
		DEL(old_blockset_list_ptr, blockset_list);

	// Make the current blockset list the old blockset list, and create a new
	// blockset list.

	old_blockset_list_ptr = blockset_list_ptr;
	NEW(blockset_list_ptr, blockset_list);
	if (blockset_list_ptr == NULL)
		memory_error("block set list");

	// Reset the spot title.

	spot_title = "Untitled Spot";

	// Reset the global script.

	global_script = "";

	// Attempt to open the spot file as a zip archive first.

	if (open_zip_archive(spot_file_path, spot_URL)) {

		// Open the first file with a ".3dml" extension in the zip archive.

		if (!push_zip_file_with_ext(".3dml", true)) {
			close_zip_archive();
			error("Unable to open a 3DML file inside zip file %s", spot_URL);
		}

		// Close the zip archive; the spot file has already been read into a
		// memory buffer.

		close_zip_archive();
	}

	// Otherwise open the spot file as an ordinary text file.

	else if (!push_file(spot_file_path, spot_URL, true))
		error("Unable to open 3DML file %s", spot_URL);

	// Copy the spot file to "curr_spot.txt" in the Flatland folder, making
	// sure it's converted to MS-DOS text format.

	copy_file(curr_spot_file_path, true);

	// First parse the spot start tag, in order to obtain the minimum Rover
	// version number.

	parse_start_of_document(TOKEN_SPOT, spot_attr_list, SPOT_ATTRIBUTES);
	if (parsed_attribute[SPOT_VERSION])
		min_rover_version = spot_version;
	else
		min_rover_version = 0;

	// If the minimum Rover version is 3.2 or higher, impose strict XML
	// compliance on the spot and any IMPORT files.

	if (min_rover_version >= 0x03020000)
		spot_XML_compliance = true;
	else
		spot_XML_compliance = false;

	// Parse the contents of the spot tag.

	got_body_tag = false;
	got_head_tag = false;
	parse_rest_of_document(spot_XML_compliance);
	start_parsing_nested_tags();
	while (parse_next_nested_tag(TOKEN_SPOT, spot_tag_list, false, &tag_token))
		switch (tag_token) {
		case TOKEN_BODY:
			parse_body_tags();
			break;
		case TOKEN_HEAD:
			parse_head_tags();
		}
	stop_parsing_nested_tags();

	// Make sure the body tag was seen.

	if (!got_body_tag)
		error("Missing body tag");

	// Close the spot file, storing its entity list for later.

	spot_entity_list = pop_file(true);

	// If a global script was constructed, assign it to the spot object.

	if (strlen(global_script) > 0)
		set_global_script(global_script);

	// If a player tag was seen, create the player block.  Otherwise set a
	// default camera offset and player collision box.

	if (!got_player_tag || !create_player_block()) {
		player_camera_offset.dx = 0.0f;
		player_camera_offset.dy = 0.0f;
		player_camera_offset.dz = -1.0f;
		player_dimensions.set(1.2f, 1.5f, 1.2f);
		set_player_size();
	} 

	// Delete the old blockset list, if it exists.

	if (old_blockset_list_ptr != NULL) {
		DEL(old_blockset_list_ptr, blockset_list);
		old_blockset_list_ptr = NULL;
	}

	// Set up the renderer data structures.

	set_up_renderer();

	// Render the builder icons for all block definitions.

	render_builder_icons();
	
	// Initialise various data structures of the spot based on what was just
	// parsed.

	init_spot();
}

//==============================================================================
// Spot saving function.
//==============================================================================

void
save_spot_file(const char *spot_file_path)
{
	// If there is not spot entity list, do nothing.

	if (!spot_entity_list) {
		return;
	}

	// If there is no base tag and this spot was downloaded from a URL, create a
	// base tag with the URL of this spot so that it will continue to work correctly
	// when loaded from the saved file.

	entity *head_tag_entity_ptr = find_tag_entity("head", spot_entity_list);
	if (!find_tag_entity("base", head_tag_entity_ptr->nested_entity_list) && !_strnicmp(spot_URL_dir, "http://", 7)) {
		entity *base_tag_entity_ptr = create_tag_entity("base", head_tag_entity_ptr->line_no, "href", (char *)spot_URL_dir, NULL);
		prepend_tag_entity(base_tag_entity_ptr, head_tag_entity_ptr->nested_entity_list);
	}

	// Reconstruct all of the level text entities with the current state of the map.  Skip over
	// levels that weren't defined in the original spot file.

	int start_level = world_ptr->ground_level_exists ? 1 : 0;
	int end_level = world_ptr->levels - 1;
	for (int level = start_level; level < end_level; level++) {
		if (!world_ptr->level_defined_list[level]) {
			continue;
		}
		entity *entity_ptr = world_ptr->get_level_entity(level);

		// Copy all whitespace at the beginning of the old map text.

		string new_map_text;
		const char *line_ptr = entity_ptr->text;
		char ch1 = *line_ptr;
		while (ch1 == ' ' || ch1 == '\t' || ch1 == '\n') {
			new_map_text += ch1;
			ch1 = *++line_ptr;
		}

		// Now generate all rows of the map.

		for (int row = 0; row < world_ptr->rows; row++) {

			// If we haven't reached the end of the old map text, copy all leading white space in this row,
			// then skip all characters to the end of the row (since we're going to replace them).

			while (ch1 == ' ' || ch1 == '\t') {
				new_map_text += ch1;
				ch1 = *++line_ptr;
			}
			while (ch1 != '\0' && ch1 != '\n') {
				ch1 = *++line_ptr;
			}

			// Get a pointer to the row in the map, and output a row of single or double symbols

			square *row_ptr = world_ptr->get_square_ptr(0, row, level);

			// Output a row of single or double symbols into the new map text.

			for (int column = 0; column < world_ptr->columns; column++) {
				if (world_ptr->map_style == DOUBLE_MAP && column > 0) {
					new_map_text += ' ';
				}
				block *block_ptr = row_ptr->block_ptr;
				new_map_text += block_ptr ? block_ptr->block_def_ptr->get_symbol() : get_symbol(NULL_BLOCK_SYMBOL);
				row_ptr++;
			}
			new_map_text += '\n';
	
			// Skip to the start of the next row, if there is one.

			if (ch1 == '\n') {
				ch1 = *++line_ptr;
			}
		}

		// Copy any trailing whitespace to the new map text.

		while (ch1 == ' ' || ch1 == '\t' || ch1 == '\n') {
			new_map_text += ch1;
			ch1 = *++line_ptr;
		}

		// Replace the entity's text with the new map.

		entity_ptr->text = new_map_text;
	}

	// Save the document represented by the spot entity list.

	save_document(spot_file_path, spot_entity_list);
}

//==============================================================================
// Generic line reading function.
//==============================================================================

//------------------------------------------------------------------------------
// Read a line from the specified file into a buffer, making sure any trailing
// carriage return is removed, and truncating the line to the size of the
// buffer.  Note that the entire line is read even if truncated.
//------------------------------------------------------------------------------

bool
read_string(FILE *fp, char *buffer, int max_buffer_length)
{
	int ch, buffer_length;

	buffer_length = 0;
	ch = fgetc(fp);
	while (buffer_length < max_buffer_length - 1 && ch != EOF && ch != '\n') {
		buffer[buffer_length++] = ch;
		ch = fgetc(fp);
	}
	buffer[buffer_length] = '\0';
	if (buffer_length == max_buffer_length - 1)
		while (ch != EOF && ch != '\n')
			ch = fgetc(fp);
	return(ch != EOF);
}

//==============================================================================
// Configuration file functions.
//==============================================================================

#define DEBUG_OPTION_VALUES 4
static value_def debug_option_value_list[DEBUG_OPTION_VALUES] = {
	{"be silent", BE_SILENT},
	{"let spot decide", LET_SPOT_DECIDE},
	{"show errors only", SHOW_ERRORS_ONLY},
	{"show errors and warnings", SHOW_ERRORS_AND_WARNINGS}
};

//------------------------------------------------------------------------------
// Read a configuration line.
//------------------------------------------------------------------------------

void
read_config_line(char *line, char *name, char *value)
{
	char *line_ptr, *start_string_ptr, *end_string_ptr;
	int string_length;

	// Skip over leading white space.

	line_ptr = line;
	while (*line_ptr == ' ' || *line_ptr == '\t')
		line_ptr++;
	start_string_ptr = line_ptr;

	// Locate the equals sign, then back up over trailing white space.

	while (*line_ptr != '=' && *line_ptr != '\0')
		line_ptr++;
	end_string_ptr = line_ptr;
	if (*line_ptr == '=')
		line_ptr++;
	while (end_string_ptr > start_string_ptr && 
		(*(end_string_ptr - 1) == ' ' || *(end_string_ptr - 1) == '\t'))
		end_string_ptr--;
	string_length = end_string_ptr - start_string_ptr;

	// Copy the name into the buffer.

	if (string_length > 0)
		strncpy(name, start_string_ptr, string_length);
	name[string_length] = '\0';

	// Skip over leading white space.

	while (*line_ptr == ' ' || *line_ptr == '\t')
		line_ptr++;
	start_string_ptr = line_ptr;

	// Locate the end of the line, then back up over trailing white space.

	while (*line_ptr != '\0')
		line_ptr++;
	end_string_ptr = line_ptr;
	if (*line_ptr == '=')
		line_ptr++;
	while (end_string_ptr > start_string_ptr && 
		(*(end_string_ptr - 1) == ' ' || *(end_string_ptr - 1) == '\t'))
		end_string_ptr--;
	string_length = end_string_ptr - start_string_ptr;

	// Copy the name into the buffer.

	if (string_length > 0)
		strncpy(value, start_string_ptr, string_length);
	value[string_length] = '\0';
}

//------------------------------------------------------------------------------
// Read a configuration value.
//------------------------------------------------------------------------------

void
read_config_bool(const char *value_string, int *value)
{
	if (!_stricmp(value_string, "yes"))
		*value = 1;
	else if (!_stricmp(value_string, "no"))
		*value = 0;
}

bool
read_config_int(const char *value_string, int *value)
{
	return(sscanf(value_string, "%d", value) == 1);
}

bool
read_config_time_t(const char *value_string, time_t *value)
{
	return(sscanf(value_string, "%lld", value) == 1);
}

bool
read_config_float(const char *value_string, float *value)
{
	return(sscanf(value_string, "%f", value) == 1);
}

void
read_config_enum(const char *value_string, int *value, 
				 value_def *value_def_list, int value_defs)
{
	for (int index = 0; index < value_defs; index++)
		if (!_stricmp(value_def_list[index].str, value_string)) {
			*value = value_def_list[index].value;
			return;
		}
}

//------------------------------------------------------------------------------
// Load the configuration file.
//------------------------------------------------------------------------------

void
load_config_file(void)
{
	FILE *fp;
	char line[80];
	char name[80], value[80];
	int use_classic_controls_value;
	int visible_block_radius_value;
	int curr_move_rate_value, curr_turn_rate_value;
	int user_debug_level_value;
	int force_software_rendering_value;
	float brightness_value;

	// Initialise the configuration options with their default values.

	use_classic_controls_value = 0;
	visible_block_radius_value = DEFAULT_VIEW_RADIUS;
	curr_move_rate_value = DEFAULT_MOVE_RATE;
	curr_turn_rate_value = DEFAULT_TURN_RATE;
	min_blockset_update_period = SECONDS_PER_WEEK;
	user_debug_level_value = BE_SILENT;
	force_software_rendering_value = 0;
	brightness_value = 0.0f;

	// First attempt to parse the configuration file.

	if ((fp = fopen(config_file_path, "r")) != NULL) {
		if (read_string(fp, line, 80) && 
			!_strnicmp(line, "Flatland Rover configuration:", 29))
			while (read_string(fp, line, 80)) {
				read_config_line(line, name, value);
				if (!_stricmp(name, "use classic controls"))
					read_config_bool(value, &use_classic_controls_value);
				else if (!_stricmp(name, "view radius"))
					read_config_int(value, &visible_block_radius_value);
				else if (!_stricmp(name, "move rate multiplier"))
					read_config_int(value, &curr_move_rate_value);
				else if (!_stricmp(name, "turn rate multiplier"))
					read_config_int(value, &curr_turn_rate_value);
				else if (!_stricmp(name, "minimum blockset update period")) {
					if (read_config_int(value, &min_blockset_update_period))
						min_blockset_update_period *= SECONDS_PER_DAY;
				} else if (!_stricmp(name, "debug option"))
					read_config_enum(value, &user_debug_level_value, debug_option_value_list, DEBUG_OPTION_VALUES);
				else if (!_stricmp(name, "force software rendering"))
					read_config_bool(value, &force_software_rendering_value);
				else if (!_stricmp(name, "brightness"))
					read_config_float(value, &brightness_value);
			}
		fclose(fp);
	}

	// Initialise any global variables affected by the configuration file.

	use_classic_controls.set(use_classic_controls_value ? true : false);
	visible_block_radius.set(visible_block_radius_value);
	curr_move_rate.set(curr_move_rate_value);
	curr_turn_rate.set(curr_turn_rate_value);
	user_debug_level.set(user_debug_level_value);
	force_software_rendering.set(force_software_rendering_value ? true : false);
	master_brightness.set(brightness_value / 100.0f);
}

//------------------------------------------------------------------------------
// Write a configuration line.
//------------------------------------------------------------------------------

void
write_config_bool(FILE *fp, const char *name, bool value)
{
	fprintf(fp, "%s = %s\n", name, value ? "yes" : "no");
}

void
write_config_int(FILE *fp, const char *name, int value, const char *units)
{
	fprintf(fp, "%s = %d %s\n", name, value, units);
}

void
write_config_time_t(FILE *fp, const char *name, time_t value, const char *units)
{
	fprintf(fp, "%s = %lld %s\n", name, value, units);
}

void
write_config_float(FILE *fp, const char *name, float value, const char *units)
{
	fprintf(fp, "%s = %g %s\n", name, value, units);
}

void
write_config_enum(FILE *fp, const char *name, int value,
				  value_def *value_def_list, int value_defs)
{
	for (int index = 0; index < value_defs; index++)
		if (value_def_list[index].value == value) {
			fprintf(fp, "%s = %s\n", name, value_def_list[index].str);
			return;
		}
}

//------------------------------------------------------------------------------
// Save the configuration file.
//------------------------------------------------------------------------------

void
save_config_file(void)
{
	FILE *fp;

	// Attempt to open the config file and write the various configuration values.

	if ((fp = fopen(config_file_path, "w")) != NULL) {
		fprintf(fp, "Flatland Rover configuration:\n");
		write_config_bool(fp, "use classic controls", use_classic_controls.get());
		write_config_int(fp, "view radius", visible_block_radius.get(), "blocks");
		write_config_int(fp, "move rate multiplier", curr_move_rate.get(), "x blocks/second");
		write_config_int(fp, "turn rate multiplier", curr_turn_rate.get(), "x degrees/second or degrees/mouse movement");
		write_config_int(fp, "minimum blockset update period", min_blockset_update_period / SECONDS_PER_DAY, "days");
		write_config_enum(fp, "debug option", user_debug_level.get(), debug_option_value_list, DEBUG_OPTION_VALUES);
		write_config_bool(fp, "force software rendering", force_software_rendering.get());
		write_config_float(fp, "brightness", master_brightness.get() * 100.0f, "% relative to ambient light");
		fclose(fp);
	}
}

//==============================================================================
// Cached blockset functions.
//==============================================================================

//------------------------------------------------------------------------------
// Delete the cached blockset list.
//------------------------------------------------------------------------------

void
delete_cached_blockset_list(void)
{
	cached_blockset *next_cached_blockset_ptr;
	while (cached_blockset_list != NULL) {
		next_cached_blockset_ptr = 
			cached_blockset_list->next_cached_blockset_ptr;
		DEL(cached_blockset_list, cached_blockset);
		cached_blockset_list = next_cached_blockset_ptr;
	}
	last_cached_blockset_ptr = NULL;
}

//------------------------------------------------------------------------------
// Add an entry to the cached blockset list.
//------------------------------------------------------------------------------

static cached_blockset *
add_cached_blockset(void)
{
	cached_blockset *cached_blockset_ptr;

	// Create the cached blockset entry.

	NEW(cached_blockset_ptr, cached_blockset);
	if (cached_blockset_ptr == NULL)
		return(NULL);

	// Add it to the end of the cached blockset list.

	cached_blockset_ptr->next_cached_blockset_ptr = NULL;
	if (last_cached_blockset_ptr != NULL)
		last_cached_blockset_ptr->next_cached_blockset_ptr = cached_blockset_ptr;
	else
		cached_blockset_list = cached_blockset_ptr;
	last_cached_blockset_ptr = cached_blockset_ptr;
	return(cached_blockset_ptr);
}

//------------------------------------------------------------------------------
// Add a new blockset to the cached blockset list.
//------------------------------------------------------------------------------

cached_blockset *
new_cached_blockset(const char *path, const char *href, int size, time_t updated)
{
	const char *name_ptr, *ext_ptr;
	string name;
	cached_blockset *cached_blockset_ptr;

	// Open the blockset.

	if (!open_zip_archive(path, href)) {
		diagnose("Failed to open blockset archive");
		return(NULL);
	}

	// Extract the blockset file name, replace the ".bset" extension with
	// ".style" to obtain the style file name.

	name_ptr = strrchr(path, '\\');
	name = name_ptr + 1;
	ext_ptr = strrchr(name, '.');
	name.truncate(ext_ptr - (char *)name);
	name += ".style";

	// Open the style file from the blockset archive.

	if (!push_zip_file(name, true)) {
		diagnose("Failed to open style file");
		close_zip_archive();
		return(NULL);
	}

	// Parse the opening style tag only.

	try {
		parse_start_of_document(TOKEN_STYLE, style_attr_list, STYLE_ATTRIBUTES);
		pop_file();
		close_zip_archive();
	}
	catch (char *) {
		pop_file();
		close_zip_archive();
		return(NULL);
	}

	// Create a new cached blockset entry.

	if ((cached_blockset_ptr = add_cached_blockset()) == NULL)
		return(NULL);

	// Initialise the cached blockset entry, and return a pointer to it.

	cached_blockset_ptr->href = href;
	cached_blockset_ptr->size = size;
	cached_blockset_ptr->updated = updated;
	if (parsed_attribute[STYLE_NAME])
		cached_blockset_ptr->name = style_name;
	if (parsed_attribute[STYLE_SYNOPSIS])
		cached_blockset_ptr->synopsis = style_synopsis;
	if (parsed_attribute[STYLE_VERSION])
		cached_blockset_ptr->version = style_version;
	else
		cached_blockset_ptr->version = 0;
	return(cached_blockset_ptr);
}

//------------------------------------------------------------------------------
// Load the cached blockset list from the cache file.
//------------------------------------------------------------------------------

bool
load_cached_blockset_list(void)
{
	cached_blockset *cached_blockset_ptr;
	int tag_token;

	// Open and parse the blockset cache file.

	cached_blockset_list = NULL;
	if (!push_file(cache_file_path, cache_file_path, true))
		return(false);
	try {

		// Parse the cache start tag.

		parse_start_of_document(TOKEN_CACHE, NULL, 0);

		// Now parse the blockset tags.

		parse_rest_of_document(true);
		start_parsing_nested_tags();
		while (parse_next_nested_tag(TOKEN_CACHE, cache_tag_list, false,
			&tag_token)) {

			// Create a cached blockset entry; if this fails generate an
			// memory error.

			if ((cached_blockset_ptr = add_cached_blockset()) == NULL)
				memory_error("cached blockset entry");

			// Initialise the cached blockset entry.

			cached_blockset_ptr->href = cached_blockset_href;
			cached_blockset_ptr->size = cached_blockset_size;
			cached_blockset_ptr->updated = cached_blockset_updated;
			if (parsed_attribute[CACHED_BLOCKSET_NAME])
				cached_blockset_ptr->name = cached_blockset_name;
			if (parsed_attribute[CACHED_BLOCKSET_SYNOPSIS])
				cached_blockset_ptr->synopsis = cached_blockset_synopsis;
			if (parsed_attribute[CACHED_BLOCKSET_VERSION])
				cached_blockset_ptr->version = cached_blockset_version;
			else
				cached_blockset_ptr->version = 0;
		}
		stop_parsing_nested_tags();

		// Close the file.

		pop_file();
		return(true);
	}

	// If an error occurs during parsing, close the file, destroy the cached 
	// blockset list, and return a failure status.

	catch (char *) {
		pop_file();
		delete_cached_blockset_list();
		return(false);			
	}
}

//------------------------------------------------------------------------------
// Save the cached blockset list to the cache file.
//------------------------------------------------------------------------------

void
save_cached_blockset_list(void)
{
	FILE *fp;
	cached_blockset *cached_blockset_ptr;

	// Open the cache file for writing.
 
	if ((fp = fopen(cache_file_path, "w")) != NULL) {

		// Write the opening cache tag.

		fprintf(fp, "<CACHE>\n");

		// Step through the list of cached blocksets, and write an entry to the
		// cache file for each.

		cached_blockset_ptr = cached_blockset_list;
		while (cached_blockset_ptr != NULL) {
			fprintf(fp, "\t<BLOCKSET HREF=\"%s\" SIZE=\"%d\""
				" UPDATED=\"%lld\"", (char *)cached_blockset_ptr->href,
				cached_blockset_ptr->size, cached_blockset_ptr->updated);
			if (strlen(cached_blockset_ptr->name) > 0)
				fprintf(fp, " NAME=\"%s\"", (char *)cached_blockset_ptr->name);
			if (strlen(cached_blockset_ptr->synopsis) > 0)
				fprintf(fp, " SYNOPSIS=\"%s\"", (char *)cached_blockset_ptr->synopsis);
			if (cached_blockset_ptr->version > 0)
				fprintf(fp, " VERSION=\"%s\"", (char *)version_number_to_string(cached_blockset_ptr->version));
			fprintf(fp, "/>\n");
			cached_blockset_ptr = cached_blockset_ptr->next_cached_blockset_ptr;
		}

		// Write the closing cache tag.

		fprintf(fp, "</CACHE>\n");
		fclose(fp);
	}
}

//------------------------------------------------------------------------------
// Search for cached blocksets in the given directory, and add them to the
// cached blockset list.
//------------------------------------------------------------------------------

static void
find_cached_blocksets(const char *dir_path)
{
	string path, href;
	struct _finddata_t file_info;
	long find_handle;
	char *ext_ptr, *ch_ptr;

	// Construct the wildcard file path for the specified directory.

	path = flatland_dir;
	path += dir_path;
	path += "*.*";

	// If there are no files in the given directory, just return.

	if ((find_handle = _findfirst(path, &file_info)) == -1)
		return;

	// Otherwise search for .bset files and subdirectories; for each of the
	// latter, recursively call this function.

	do {
		// Skip over the current and parent directory entries.

		if (!strcmp(file_info.name, ".") || !strcmp(file_info.name, ".."))
			continue;

		// If this entry is a subdirectory, construct a new directory path
		// and recursively call this function.

		if (file_info.attrib & _A_SUBDIR) {
			path = dir_path;
			path += file_info.name;
			path += "\\";
			find_cached_blocksets(path);
		}

		// Otherwise if this entry has an extension of ".bset", include this in
		// the cached blockset list.

		else {
			ext_ptr = strrchr(file_info.name, '.');
			if (!_stricmp(ext_ptr, ".bset")) {

				// Construct the path to the blockset.

				path = flatland_dir;
				path += dir_path;
				path += file_info.name;

				// Construct the URL to the blockset.

				href = "http://";
				href += dir_path;
				href += file_info.name;
				ch_ptr = (char *)href;
				while (*ch_ptr) {
					if (*ch_ptr == '\\')
						*ch_ptr = '/';
					ch_ptr++;
				}

				// Create a new cached blockset entry.

				new_cached_blockset(path, href, file_info.size, 0);
			}
		}

		// Find next file.

	} while (_findnext(find_handle, &file_info) == 0);

	// Done searching the directory.

   _findclose(find_handle);
}

//------------------------------------------------------------------------------
// Create the cached blockset list.
//------------------------------------------------------------------------------

void
create_cached_blockset_list(void)
{
	cached_blockset_list = NULL;
	last_cached_blockset_ptr = NULL;
	find_cached_blocksets("");
	save_cached_blockset_list();
}

//------------------------------------------------------------------------------
// Search for a cached blockset entry by URL.
//------------------------------------------------------------------------------

cached_blockset *
find_cached_blockset(const char *href)
{
	cached_blockset *cached_blockset_ptr = cached_blockset_list;
	while (cached_blockset_ptr != NULL) {
		if (!_stricmp(href, cached_blockset_ptr->href))
			return(cached_blockset_ptr);
		cached_blockset_ptr = cached_blockset_ptr->next_cached_blockset_ptr;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Delete the cached blockset entry with the given URL.
//------------------------------------------------------------------------------

bool
delete_cached_blockset(const char *href)
{
	// Search for the entry with the given URL, and also remember the previous
	// entry.

	cached_blockset *prev_cached_blockset_ptr = NULL;
	cached_blockset *cached_blockset_ptr = cached_blockset_list;
	while (cached_blockset_ptr != NULL) {

		// If this entry matches, delete it and return success.

		if (!_stricmp(href, cached_blockset_ptr->href)) {
			if (prev_cached_blockset_ptr != NULL) {
				prev_cached_blockset_ptr->next_cached_blockset_ptr = 
					cached_blockset_ptr->next_cached_blockset_ptr;
				if (last_cached_blockset_ptr == cached_blockset_ptr)
					last_cached_blockset_ptr = prev_cached_blockset_ptr;
			} else {
				cached_blockset_list = 
					cached_blockset_ptr->next_cached_blockset_ptr;
				if (last_cached_blockset_ptr == cached_blockset_ptr)
					last_cached_blockset_ptr = NULL;
			}
			DEL(cached_blockset_ptr, blockset);
			return(true);
		}

		// Move onto the next entry.

		prev_cached_blockset_ptr = cached_blockset_ptr;
		cached_blockset_ptr = cached_blockset_ptr->next_cached_blockset_ptr;
	}

	// Indicate failure.

	return(false);
}

//------------------------------------------------------------------------------
// Check for an update for the given blockset, and if there is one ask the
// user if they want to download it.
//------------------------------------------------------------------------------

bool
check_for_blockset_update(const char *version_file_URL, 
						  const char *blockset_name, 
						  unsigned int blockset_version)
{
	string message;

	// Download the specified version file URL to "version.txt" in the flatland
	// directory.  If it doesn't exist or cannot be opened, assume the current
	// version is up to date.

	requested_blockset_name = (char *)NULL;
	if (!download_URL(version_file_URL, version_file_path, true) ||
		!push_file(version_file_path, version_file_URL, true))
		return(false);

	// Parse the version start tag, then the text contained within it.

	try {
		parse_start_of_document(TOKEN_VERSION, blockset_version_attr_list, 
			BLOCKSET_VERSION_ATTRIBUTES);
		parse_rest_of_document(true);
		message = nested_text_to_string(TOKEN_VERSION);
		pop_file();
	}

	// If an error occurs during parsing, just assume blockset is up to date.

	catch (char *) {
		pop_file();
		return(false);			
	}

	// If the version number in the version file is higher than the version
	// number in the cached blockset, ask the user if they want to download
	// the new blockset.

	if (blockset_version_id > blockset_version && 
		query("New version of blockset available", true, 
			"Version %s of the %s blockset is available for download.\n\n%s\n\nWould you like to download it now?", 
			(char *)version_number_to_string(blockset_version_id), blockset_name, message))
		return(true);

	// Indicate no update is available or requested.

	return(false);
}

//==============================================================================
// Rover version file function.
//==============================================================================

//------------------------------------------------------------------------------
// Parse the Rover version file, and return the version number and message.
//------------------------------------------------------------------------------

bool
parse_rover_version_file(unsigned int &version_number, string &message)
{
	// Open the version file.

	if (!push_file(version_file_path, version_file_path, true))
		return(false);

	// Parse the VERSION start tag and the text contained within it.

	try {
		parse_start_of_document(TOKEN_VERSION, rover_version_attr_list, 
			ROVER_VERSION_ATTRIBUTES);
		parse_rest_of_document(true);
		version_number = rover_version_id;
		message = nested_text_to_string(TOKEN_VERSION);
		pop_file();
		return(true);
	}
	catch (char *) {
		pop_file();
		return(false);			
	}
}