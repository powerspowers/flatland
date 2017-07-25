//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "Classes.h"
#include "Image.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "SimKin.h"
#include "Utils.h"

// Current load index.

int curr_load_index;

// Relative coordinates of adjacent blocks checked in polygon visibility test.

static int adj_block_column[6] = { 1, 0, 0, -1, 0, 0 };
static int adj_block_row[6] = { 0, 1, 0, 0, -1, 0 };
static int adj_block_level[6] = { 0, 0, 1, 0, 0, -1 };

// Flag indicating whether current URL has been opened.

bool curr_URL_opened;

//------------------------------------------------------------------------------
// Return a pointer to the block with the given single character symbol.
//------------------------------------------------------------------------------

block_def *
get_block_def(char block_symbol)
{	
	blockset *blockset_ptr;
	block_def *block_def_ptr;

	// Step through the blockset list looking for a block with the given
	// symbol.  Return a pointer to the first match.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if ((block_def_ptr = blockset_ptr->get_block_def(block_symbol)) != NULL)
			return(block_def_ptr);
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Return a pointer to the block with the given double character symbol.
//------------------------------------------------------------------------------

block_def *
get_block_def(word block_symbol)
{	
	blockset *blockset_ptr;
	block_def *block_def_ptr;

	// Step through the blockset list looking for a block with the given
	// symbol.  Return a pointer to the first match.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if ((block_def_ptr = blockset_ptr->get_block_def(block_symbol)) != NULL)
			return(block_def_ptr);
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Return a pointer to the block with the given name.  If the blockset name is
// prefixed to the block name, look in that blockset; otherwise search all
// blocksets and return the first match.
//------------------------------------------------------------------------------

block_def *
get_block_def(const char *block_identifier)
{
	string blockset_name, block_name;
	string new_block_name;
	blockset *blockset_ptr;
	block_def *block_def_ptr;

	// Parse the block identifier into a blockset name and block name.  If the
	// block name is invalid, return a NULL pointer.

	parse_identifier(block_identifier, blockset_name, block_name);
	if (!parse_name(block_name, &new_block_name, false))
		return(NULL);

	// If the blockset name is an empty string, search the blockset list for
	// the first block that matches the block name.

	if (strlen(blockset_name) == 0) {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			if ((block_def_ptr = blockset_ptr->get_block_def(new_block_name))
				!= NULL)
				return(block_def_ptr);
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
		return(NULL);
	}

	// If the blockset name is present, search the blockset list for the
	// blockset with that name, and search that blockset for a block that
	// matches the block name.

	else {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			if (!_stricmp(blockset_name, blockset_ptr->name))
				return(blockset_ptr->get_block_def(new_block_name));
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
		return(NULL);
	}
}

//------------------------------------------------------------------------------
// Find a texture in the given blockset, and return a pointer to it, or NULL if
// not found.
//------------------------------------------------------------------------------

static texture *
find_texture(blockset *blockset_ptr, const char *texture_URL)
{
	texture *texture_ptr;

	// Search for the new texture URL in the determined blockset, returning a
	// pointer to it if found.

	texture_ptr = blockset_ptr->first_texture_ptr;
	while (texture_ptr != NULL) {
		if (!_stricmp(texture_URL, texture_ptr->URL)) {
			return(texture_ptr);
		}
		texture_ptr = texture_ptr->next_texture_ptr;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Search for the named texture in the given blockset; if it doesn't exist, load
// the texture file and add the texture image to the given blockset if
// add_to_blockset is TRUE.
//------------------------------------------------------------------------------

texture *
load_texture(blockset *blockset_ptr, char *texture_URL, bool add_to_blockset)
{
	string blockset_name, texture_name;
	texture *texture_ptr;
	string new_texture_URL;

	// If the texture URL begins with a "@", parse it as a texture from a
	// blockset.

	if (*texture_URL == '@') {

		// If the given blockset is not the custom blockset, this is an error,
		// since only 3DML files may use this syntax.

		if (blockset_ptr != custom_blockset_ptr) {
			warning("Invalid texture URL %s", texture_URL);
			return(NULL);
		}

		// Skip over the "@" character, and parse the texture URL to obtain the
		// blockset name and texture name.

		parse_identifier(texture_URL + 1, blockset_name, texture_name);

		// If the blockset name is missing, select the first blockset by
		// default.  Otherwise search for the blockset with the given name.

		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		if (strlen(blockset_name) != 0) {
			while (blockset_ptr != NULL) {
				if (!_stricmp(blockset_name, blockset_ptr->name))
					break;
				blockset_ptr = blockset_ptr->next_blockset_ptr;
			}
			if (blockset_ptr == NULL) {
				warning("There is no blockset with name \"%s\"", blockset_name);
				return(NULL);
			}
		}

		// If a texture with the given name is already in the blockset,
		// return a pointer to it.

		if ((texture_ptr = find_texture(blockset_ptr, texture_name)) != NULL)
			return(texture_ptr);

		// Otherwise create the texture object, and initialise it.

		NEW(texture_ptr, texture);
		if (texture_ptr == NULL) { 
			memory_warning("texture");
			return(NULL);
		}
		texture_ptr->blockset_ptr = blockset_ptr;
		texture_ptr->URL = texture_name;

		// Open the blockset.

		if (!open_blockset(blockset_ptr->URL, blockset_ptr->name)) {
			DEL(texture_ptr, texture);
			return(NULL);
		}

		// Load the texture from the blockset.

		new_texture_URL = "textures/";
		new_texture_URL += texture_name;
		if (!load_image(NULL, new_texture_URL, texture_ptr)) {
			DEL(texture_ptr, texture);
			close_zip_archive();
			return(NULL);
		}

		// Close the given blockset.

		close_zip_archive();
	}

	// If the texture URL does not begin with a "@", parse it as a texture in
	// the given blockset.

	else {

		// If a texture with the given URL is already in the given blockset,
		// return a pointer to it.

		if ((texture_ptr = find_texture(blockset_ptr, texture_URL)) != NULL)
			return(texture_ptr);

		// Otherwise create the texture object, and initialise it.

		NEW(texture_ptr, texture);
		if (texture_ptr == NULL) { 
			memory_warning("texture");
			return(NULL);
		}
		texture_ptr->URL = texture_URL;

		// If the texture is a custom texture, then set it's load index (it will
		// be loaded later).

		if (blockset_ptr == custom_blockset_ptr) {
			texture_ptr->blockset_ptr = NULL;
			texture_ptr->load_index = curr_load_index++;
		}

		// Otherwise load it from the currently open blockset.

		else {
			texture_ptr->blockset_ptr = blockset_ptr;
			new_texture_URL = "textures/";
			new_texture_URL += texture_URL;
			if (!load_image(NULL, new_texture_URL, texture_ptr)) {
				DEL(texture_ptr, texture);
				return(NULL);
			}
		}
	}

	// Return a pointer to the texture, after setting adding it to the blockset
	// it belongs to if requested.

	if (add_to_blockset)
		blockset_ptr->add_texture(texture_ptr);
	return(texture_ptr);
}

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Search for a scaled video texture for the given source rectangle.
//------------------------------------------------------------------------------

static texture *
find_scaled_video_texture(video_rect *rect_ptr)
{
	video_texture *video_texture_ptr;
	video_rect *source_rect_ptr;

	video_texture_ptr = scaled_video_texture_list;
	while (video_texture_ptr != NULL) {
		source_rect_ptr = &video_texture_ptr->source_rect;
		if (rect_ptr->x1_is_ratio == source_rect_ptr->x1_is_ratio &&
			rect_ptr->y1_is_ratio == source_rect_ptr->y1_is_ratio &&
			rect_ptr->x2_is_ratio == source_rect_ptr->x2_is_ratio &&
			rect_ptr->y2_is_ratio == source_rect_ptr->y2_is_ratio &&
			rect_ptr->x1 == source_rect_ptr->x1 &&
			rect_ptr->y1 == source_rect_ptr->y1 &&
			rect_ptr->x2 == source_rect_ptr->x2 &&
			rect_ptr->y2 == source_rect_ptr->y2)
			return(video_texture_ptr->texture_ptr);
		video_texture_ptr = video_texture_ptr->next_video_texture_ptr;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Create a stream URL from a partial or absolute URL.
//------------------------------------------------------------------------------

char *
create_stream_URL(string stream_URL)
{
	string spot_dir;

	// Create the path to the streaming media file.  We must use Internet
	// Explorer syntax for file URLs.

	if (!strnicmp(spot_URL_dir, "file:", 5)) {
		spot_dir = "file://";
		spot_dir += (const char *)spot_URL_dir + 7;
		spot_dir[8] = ':';
	} else
		spot_dir = spot_URL_dir;
	return(create_URL(spot_dir, stream_URL));
}

//------------------------------------------------------------------------------
// Create a video texture for the given rectangle of the named stream.
//------------------------------------------------------------------------------

texture *
create_video_texture(char *name, video_rect *rect_ptr, bool unlimited_size)
{
	video_rect source_rect;
	texture *texture_ptr;

	// Make sure that the given name matches the stream name (if one is set).
	// If not, just issue a warning and return a NULL pointer.

	if (stricmp(name, name_of_stream)) {
		warning("There is no stream named \"%s\"", name);
		return(NULL);
	}

	// If a texture of unlimited size has been requested, and an unscaled
	// video texture already exists, return it.

	if (unlimited_size) {
		if (unscaled_video_texture_ptr != NULL)
			return(unscaled_video_texture_ptr);
	}

	// If a texture of limited size has been requested, and a scaled video
	// texture for the given source rectangle already exists, return it.
	// If no source rectangle was specified, use one that encompasses the
	// entire video frame.

	else {
		if (rect_ptr == NULL) {
			source_rect.x1_is_ratio = true;
			source_rect.y1_is_ratio = true;
			source_rect.x2_is_ratio = true;
			source_rect.y2_is_ratio = true;
			source_rect.x1 = 0.0f;
			source_rect.y1 = 0.0f;
			source_rect.x2 = 1.0f;
			source_rect.y2 = 1.0f;
			rect_ptr = &source_rect;
		}
		if ((texture_ptr = find_scaled_video_texture(rect_ptr)) != NULL)
			return(texture_ptr);
	}

	// Create a new custom texture object.

	NEW(texture_ptr, texture);
	if (texture_ptr == NULL) { 
		memory_warning("texture");
		return(NULL);
	}
	texture_ptr->blockset_ptr = NULL;

	// If a texture of unlimited size was requested, it becomes the unscaled
	// video texture.

	if (unlimited_size)
		unscaled_video_texture_ptr = texture_ptr;

	// If a texture of limited size was requested, it becomes a new scaled
	// video texture.

	else {
		video_texture *scaled_video_texture_ptr;

		NEW(scaled_video_texture_ptr, video_texture);
		if (scaled_video_texture_ptr == NULL) {
			memory_warning("scaled video texture");
			return(NULL);
		}
		scaled_video_texture_ptr->texture_ptr = texture_ptr;
		scaled_video_texture_ptr->source_rect = *rect_ptr;
		scaled_video_texture_ptr->next_video_texture_ptr =
			scaled_video_texture_list;
		scaled_video_texture_list = scaled_video_texture_ptr;
	}

	// Return a pointer to the new texture object.

	return(texture_ptr);
}

#endif // STREAMING_MEDIA

//------------------------------------------------------------------------------
// Load a wave file into a wave object.
//------------------------------------------------------------------------------

bool
load_wave_file(const char *URL, const char *file_path, wave *wave_ptr)
{
	bool result;
	char *file_buffer_ptr;
	int file_size;

	// Attempt to open the wave file.  If there is no URL specified, it is
	// assumed this is a wave file found in a currently open blockset.

	if (URL != NULL) {
		if (!push_file(file_path, URL, false))
			return(false);
	} else {
		if (!push_zip_file(file_path, false))
			return(false);
	}

	// Now load the wave data into the wave object.

	get_file_buffer(&file_buffer_ptr, &file_size);
	result = load_wave_data(wave_ptr, file_buffer_ptr, file_size);

	// Close the wave file and return success or failure.

	pop_file();
	return(result);
}

//------------------------------------------------------------------------------
// Search for the wave with the given URL in the given blockset, and return a
// pointer to it if found.
//------------------------------------------------------------------------------

static wave *
find_wave(blockset *blockset_ptr, const char *wave_URL)
{
	wave *wave_ptr;

	// Search for the new wave URL in the determined blockset, returning a
	// pointer to it if found.

	wave_ptr = blockset_ptr->first_wave_ptr;
	while (wave_ptr != NULL) {
		if (!_stricmp(wave_URL, wave_ptr->URL))
			return(wave_ptr);
		wave_ptr = wave_ptr->next_wave_ptr;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Search for the wave with the given URL in the given blockset; if not found,
// load the wave file and create a new wave.
//------------------------------------------------------------------------------

wave *
load_wave(blockset *blockset_ptr, char *wave_URL)
{
	wave *wave_ptr;
	string blockset_name, wave_name;
	string new_wave_URL;

	// If the wave URL begins with a "@", parse it as a wave in a blockset.

	if (*wave_URL == '@') {

		// If the given blockset is not the custom blockset, this is an error.

		if (blockset_ptr != custom_blockset_ptr) {
			warning("Invalid wave URL %s", wave_URL);
			return(NULL);
		}

		// Skip over the "@" character, and parse the wave URL to obtain the
		// blockset name and wave name.

		parse_identifier(wave_URL + 1, blockset_name, wave_name);

		// If the blockset name is missing, select the first blockset by
		// default.  Otherwise search for the blockset with the given name.

		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		if (strlen(blockset_name) != 0) {
			while (blockset_ptr != NULL) {
				if (!_stricmp(blockset_name, blockset_ptr->name))
					break;
				blockset_ptr = blockset_ptr->next_blockset_ptr;
			}
			if (blockset_ptr == NULL) {
				warning("There is no blockset with name \"%s\"", blockset_name);
				return(NULL);
			}
		}

		// If a wave with the given name is already in the given blockset, return
		// a pointer to it.

		if ((wave_ptr = find_wave(blockset_ptr, wave_name)) != NULL)
			return(wave_ptr);

		// Otherwise create the wave object and initialise it.

		NEW(wave_ptr, wave);
		if (wave_ptr == NULL) { 
			memory_warning("wave");
			return(NULL);
		}
		wave_ptr->blockset_ptr = blockset_ptr;
		wave_ptr->URL = wave_name;

		// Open the blockset.

		if (!open_blockset(blockset_ptr->URL, blockset_ptr->name)) {
			DEL(wave_ptr, wave);
			return(NULL);
		}

		// Load the wave file.

		new_wave_URL = "sounds/";
		new_wave_URL += wave_name;
		if (!load_wave_file(NULL, new_wave_URL, wave_ptr)) {
			DEL(wave_ptr, wave);
			close_zip_archive();
			return(NULL);
		}

		// Close the blockset.

		close_zip_archive();
	}

	// If the wave URL does not begin with a "@", parse it as a wave in
	// the given blockset.

	else {

		// If a wave with the given name is already in the given blockset, return
		// a pointer to it.

		if ((wave_ptr = find_wave(blockset_ptr, wave_URL)) != NULL)
			return(wave_ptr);

		// Otherwise create the wave object and initialise it.

		NEW(wave_ptr, wave);
		if (wave_ptr == NULL) { 
			memory_warning("wave");
			return(NULL);
		}
		wave_ptr->URL = wave_URL;
		
		// If it's a custom wave, just set it's load index.

		if (blockset_ptr == custom_blockset_ptr) {
			wave_ptr->blockset_ptr = NULL;
			wave_ptr->load_index = curr_load_index;
			curr_load_index++;
		}

		// Otherwise load it from the currently open blockset.

		else {
			wave_ptr->blockset_ptr = blockset_ptr;
			new_wave_URL = "sounds/";
			new_wave_URL += wave_URL;
			if (!load_wave_file(NULL, new_wave_URL, wave_ptr)) {
				DEL(wave_ptr, wave);
				return(NULL);
			}
		}
	}

	// Return a pointer to the wave, after adding it to the blockset it belongs 
	// to.

	blockset_ptr->add_wave(wave_ptr);
	return(wave_ptr);
}

//------------------------------------------------------------------------------
// Search for an entrance with the given name in the given block list, and
// add them to the given chosen entrance list.
//------------------------------------------------------------------------------

static void
find_entrances_in_block_list(const char *name, block *block_list,
							 entrance *&chosen_entrance_list, 
							 int &chosen_entrances)
{
	block *block_ptr;
	entrance *entrance_ptr;
	
	block_ptr = block_list;
	while (block_ptr != NULL) {
		entrance_ptr = block_ptr->entrance_ptr;
		if (entrance_ptr != NULL && !_stricmp(entrance_ptr->name, name)) {
			entrance_ptr->next_chosen_entrance_ptr = chosen_entrance_list;
			chosen_entrance_list = entrance_ptr;
			chosen_entrances++;
		}
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Search for an entrance with the given name, and return a pointer to it if
// found.  If more than one entrance with the same name is found, one is
// chosen at random.
//------------------------------------------------------------------------------

entrance *
find_entrance(const char *name)
{
	entrance *chosen_entrance_list, *chosen_entrance_ptr;
	int chosen_entrances;
	float random_no;
	float range_factor;
	int entrance_no;

	// Start with an empty chosen entrance list.

	chosen_entrance_list = NULL;
	chosen_entrances = 0;

	// Search the global entrance list for entrances with the given name, and
	// add each one to the chosen entrance list.

	entrance *entrance_ptr = global_entrance_list;
	while (entrance_ptr != NULL) {
		if (!_stricmp(entrance_ptr->name, name)) {
			entrance_ptr->next_chosen_entrance_ptr = chosen_entrance_list;
			chosen_entrance_list = entrance_ptr;
			chosen_entrances++;
		}
		entrance_ptr = entrance_ptr->next_entrance_ptr;
	}

	// Search the movable and fixed block lists for entrances with the given
	// name, and add each one to the chosen entrance list.

	find_entrances_in_block_list(name, movable_block_list, chosen_entrance_list,
		chosen_entrances);
	find_entrances_in_block_list(name, fixed_block_list, chosen_entrance_list,
		chosen_entrances);

	// If there are no chosen entrances, return NULL.

	if (chosen_entrances == 0)
		return(NULL);

	// Select a chosen entrance at random, and return a pointer to it.
	// XXX -- Evidently I believe that the random number divided by the
	// range factor may end up being one more than the maximum number of
	// chosen entrances, otherwise I wouldn't be so careful to get a valid
	// result.

	random_no = (float)rand();
	range_factor = ((float)RAND_MAX + 1.0f) / (float)chosen_entrances;
	entrance_no = (int)(random_no / range_factor);
	chosen_entrance_ptr = chosen_entrance_list;
	while (chosen_entrance_ptr != NULL && entrance_no > 0) {
		chosen_entrance_ptr = chosen_entrance_ptr->next_chosen_entrance_ptr;
		entrance_no--;
	}
	if (chosen_entrance_ptr == NULL)
		chosen_entrance_ptr = chosen_entrance_list;
	return(chosen_entrance_ptr);
}

//------------------------------------------------------------------------------
// Search for an imagemap with the given name, and return a pointer to it if
// found.
//------------------------------------------------------------------------------

imagemap *
find_imagemap(const char *name)
{
	imagemap *imagemap_ptr = imagemap_list;
	while (imagemap_ptr != NULL) {
		if (!_stricmp(imagemap_ptr->name, name))
			break;
		imagemap_ptr = imagemap_ptr->next_imagemap_ptr;
	}
	return(imagemap_ptr);
}

//------------------------------------------------------------------------------
// Add an imagemap with the given name to the imagemap list.
//------------------------------------------------------------------------------

imagemap *
add_imagemap(const char *name)
{
	imagemap *imagemap_ptr;

	// Search for an existing imagemap of the same name; if found, this is an
	// error.

	if ((imagemap_ptr = find_imagemap(name)) != NULL) {
		warning("Duplicate imagemap name \"%s\"", name);
		return(NULL);
	}

	// Create a new imagemap and initialise it.

	NEW(imagemap_ptr, imagemap);
	if (imagemap_ptr == NULL) {
		memory_warning("imagemap");
		return(NULL);
	}
	imagemap_ptr->name = name;

	// Add the imagemap to the imagemap list.

	imagemap_ptr->next_imagemap_ptr = imagemap_list;
	imagemap_list = imagemap_ptr;
	return(imagemap_ptr);
}

//------------------------------------------------------------------------------
// Determine if the mouse is currently pointing at an area within the imagemap
// of the given popup, if there is one.  Returns TRUE if the popup is at least
// selected.
//------------------------------------------------------------------------------

bool
check_for_popup_selection(popup *popup_ptr, int popup_width, int popup_height)
{
	int delta_mouse_x, delta_mouse_y;
	imagemap *imagemap_ptr;
	area *area_ptr;

	// If the current mouse position is outside of the popup rectangle, it
	// isn't selected.

	delta_mouse_x = mouse_x - popup_ptr->sx;
	delta_mouse_y = mouse_y - popup_ptr->sy;
	if (delta_mouse_x < 0 || delta_mouse_x >= popup_width ||
		delta_mouse_y < 0 || delta_mouse_y >= popup_height)
		return(false);

	// The popup is selected.  However, if it doesn't have an imagemap there
	// is nothing else to do.

	if (popup_ptr->imagemap_ptr == NULL)
		return(true);

	// Step through the areas of the popup's imagemap...

	imagemap_ptr = popup_ptr->imagemap_ptr;
	area_ptr = imagemap_ptr->area_list;
	while (area_ptr != NULL) {
		rect *rect_ptr;
		circle *circle_ptr;
		float delta_x, delta_y, distance;

		// If the current mouse position is inside this area's rectangle or
		// circle, make this area the selected one, and remember it's block
		// and/or square.  In addition, this becomes the selected exit if
		// the area has one.

		switch (area_ptr->shape_type) {
		case RECT_SHAPE:
			rect_ptr = &area_ptr->rect_shape;
			if (delta_mouse_x >= rect_ptr->x1 && delta_mouse_x < rect_ptr->x2 &&
				delta_mouse_y >= rect_ptr->y1 && delta_mouse_y < rect_ptr->y2) {
				curr_selected_area_ptr = area_ptr;
				curr_area_block_ptr = popup_ptr->block_ptr;
				curr_area_square_ptr = popup_ptr->square_ptr;
				curr_selected_exit_ptr = area_ptr->exit_ptr;
				return(true);
			}
			break;
		case CIRCLE_SHAPE:
			circle_ptr = &area_ptr->circle_shape;
			delta_x = (float)(delta_mouse_x - circle_ptr->x);
			delta_y = (float)(delta_mouse_y - circle_ptr->y);
			distance = (float)sqrt(delta_x * delta_x + delta_y * delta_y);
			if (distance <= circle_ptr->r) {
				curr_selected_area_ptr = area_ptr;
				curr_area_block_ptr = popup_ptr->block_ptr;
				curr_area_square_ptr = popup_ptr->square_ptr;
				curr_selected_exit_ptr = area_ptr->exit_ptr;
				return(true);
			}
		}

		// Move onto the next area.

		area_ptr = area_ptr->next_area_ptr;
	}

	// The popup was selected, but no area was active.

	return(true);
}

//------------------------------------------------------------------------------
// Initialise a sprite's collision box.
//------------------------------------------------------------------------------

static void
init_sprite_collision_box(block *block_ptr)
{	
	int vertices, vertex_no;
	vertex *vertex_list, *vertex_ptr;
	float min_x, min_y;
	float max_x, max_y;
	float z, min_z, max_z;
	float x_radius;

	// Initialise the minimum and maximum coordinates to the first vertex.
 
	vertices = block_ptr->vertices;
	vertex_list = block_ptr->vertex_list;
	vertex_ptr = &vertex_list[0];
	min_x = vertex_ptr->x;
	min_y = vertex_ptr->y;
	max_x = vertex_ptr->x;
	max_y = vertex_ptr->y;
	z = vertex_ptr->z;

	// Step through the remaining vertices and remember the minimum and
	// maximum X and Y coordinates.

	for (vertex_no = 1; vertex_no < vertices; vertex_no++) {
		vertex_ptr = &vertex_list[vertex_no];
		if (vertex_ptr->x < min_x)
			min_x = vertex_ptr->x;
		else if (vertex_ptr->x > max_x)
			max_x = vertex_ptr->x;
		if (vertex_ptr->y < min_y)
			min_y = vertex_ptr->y;
		else if (vertex_ptr->y > max_y)
			max_y = vertex_ptr->y;
	}

	// Generate the minimum and maximum Z coordinates from the minimum and
	// maximum X coordinates.

	x_radius = (max_x - min_x) / 2.0f;
	min_z = z - x_radius;
	max_z = z + x_radius;

	// Create a collision mesh from the collision box determined above.
	
	COL_convertSpriteToColMesh(block_ptr->col_mesh_ptr, min_x, min_y, min_z,
		max_x, max_y, max_z);
}

//------------------------------------------------------------------------------
// Initialise a polygon for a sprite that is centred in the block and facing
// north.  The polygon is sized to match the texture dimensions.
//------------------------------------------------------------------------------

static void
init_sprite_polygon(block *block_ptr, polygon *polygon_ptr, part *part_ptr)
{
	block_def *block_def_ptr;
	polygon_def *polygon_def_ptr;
	size *size_ptr;
	float size_x, size_y;
	float half_size_x, half_size_y;
	float y_offset;
	texture *texture_ptr;
	vertex *vertex_ptr;
	vertex_def *vertex_def_ptr;

	// Get a pointer to the block definition and sprite size.

	block_def_ptr = block_ptr->block_def_ptr;
	size_ptr = &block_def_ptr->sprite_size;

	// If the block definition defines a size, use it.

	if (size_ptr->width > 0 && size_ptr->height > 0) {
		size_x = (float)size_ptr->width / TEXELS_PER_UNIT;
		size_y = (float)size_ptr->height / TEXELS_PER_UNIT;
	}

	// If the sprite has a texture, use the size of the texture as the size of
	// the sprite.  Otherwise make the size of the sprite to be 256x256 texels.

	else {
		texture_ptr = part_ptr->texture_ptr;
		if (texture_ptr != NULL) {
			size_x = (float)texture_ptr->width / TEXELS_PER_UNIT;
			size_y = (float)texture_ptr->height / TEXELS_PER_UNIT;
		} else {
			size_x = UNITS_PER_BLOCK;
			size_y = UNITS_PER_BLOCK;
		}
	}

	// Compute half the width and height.

	half_size_x = size_x / 2.0f;
	half_size_y = size_y / 2.0f;

	// Determine the Y offset of the polygon based upon the sprite alignment.

	switch (block_def_ptr->sprite_alignment) {
	case TOP:
		y_offset = UNITS_PER_BLOCK - half_size_y;
		break;
	case CENTRE:
		y_offset = UNITS_PER_HALF_BLOCK;
		break;
	case BOTTOM:
		y_offset = half_size_y;
	}

	// Get a pointer to the vertex list and vertex definition list.

	PREPARE_VERTEX_LIST(block_ptr);
	polygon_def_ptr = polygon_ptr->polygon_def_ptr;
	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);

	// Initialise the vertex and texture coordinates of the polygon, making
	// sure the vertices are scaled.

	vertex_ptr = &vertex_list[0];
	vertex_def_ptr = &vertex_def_list[0];
	vertex_ptr->x = UNITS_PER_HALF_BLOCK + half_size_x;
	vertex_ptr->y = y_offset + half_size_y;
	vertex_ptr->z = UNITS_PER_HALF_BLOCK;
	vertex_def_ptr->u = 0.0f;
	vertex_def_ptr->v = 0.0f;
	vertex_ptr = &vertex_list[1];
	vertex_def_ptr = &vertex_def_list[1];
	vertex_ptr->x = UNITS_PER_HALF_BLOCK - half_size_x;
	vertex_ptr->y = y_offset + half_size_y;
	vertex_ptr->z = UNITS_PER_HALF_BLOCK;
	vertex_def_ptr->u = 1.0f;
	vertex_def_ptr->v = 0.0f;
	vertex_ptr = &vertex_list[2];
	vertex_def_ptr = &vertex_def_list[2];
	vertex_ptr->x = UNITS_PER_HALF_BLOCK - half_size_x;
	vertex_ptr->y = y_offset - half_size_y;
	vertex_ptr->z = UNITS_PER_HALF_BLOCK;
	vertex_def_ptr->u = 1.0f;
	vertex_def_ptr->v = 1.0f;
	vertex_ptr = &vertex_list[3];
	vertex_def_ptr = &vertex_def_list[3];
	vertex_ptr->x = UNITS_PER_HALF_BLOCK + half_size_x;
	vertex_ptr->y = y_offset - half_size_y;
	vertex_ptr->z = UNITS_PER_HALF_BLOCK;
	vertex_def_ptr->u = 0.0f;
	vertex_def_ptr->v = 1.0f;

	// Initialise the sprite polygon's centroid, normal vector and plane 
	// offset.

	polygon_ptr->compute_centroid(vertex_list);
	polygon_ptr->compute_normal_vector(vertex_list);
	polygon_ptr->compute_plane_offset(vertex_list);

	// Initialise the sprite's collision box.

	init_sprite_collision_box(block_ptr);
}

//------------------------------------------------------------------------------
// Compute the bounding box that emcompasses all the lights in the given
// light list, for the given translation.
//------------------------------------------------------------------------------

void
compute_light_list_bounding_box(light *light_list, vertex translation,
								int &min_column, int &min_row, int &min_level,
								int &max_column, int &max_row, int &max_level)
{
	light *light_ptr;
	vertex light_position, min_light_bbox, max_light_bbox;
	int min_light_column, min_light_row, min_light_level;
	int max_light_column, max_light_row, max_light_level;

	// Initialise the bounding box.

	min_column = world_ptr->columns;
	min_row = world_ptr->rows;
	min_level = world_ptr->levels;
	max_column = -1;
	max_row = -1;
	max_level = -1;

	// Step through each light in the light list...

	light_ptr = light_list;
	while (light_ptr != NULL) {

		// Calculate the bounding box for this light, based upon it's radius.

		light_position = light_ptr->position + translation;
		min_light_bbox = light_position - light_ptr->radius;
		max_light_bbox = light_position + light_ptr->radius;

		// Now calculate the bounding box in block units.

		min_light_bbox.get_map_position(&min_light_column, &max_light_row, 
			&min_light_level);
		max_light_bbox.get_map_position(&max_light_column, &min_light_row,
			&max_light_level);

		// Update the overall bounding box to include this one.

		if (min_light_column < min_column)
			min_column = min_light_column;
		if (min_light_row < min_row)
			min_row = min_light_row;
		if (min_light_level < min_level)
			min_level = min_light_level;
		if (max_light_column > max_column)
			max_column = max_light_column;
		if (max_light_row > max_row)
			max_row = max_light_row;
		if (max_light_level > max_level)
			max_level = max_light_level;

		// Move onto the next light.

		light_ptr = light_ptr->next_light_ptr;
	}
}

//------------------------------------------------------------------------------
// Reset the active lights on all blocks in the given bounding box.
//------------------------------------------------------------------------------

void
reset_active_lights(int min_column, int min_row, int min_level, 
					int max_column, int max_row, int max_level)
{
	block *block_ptr;

	// Clamp the minimum and maximum coordinates of the bounding box to the
	// map boundaries.

	if (min_column < 0)
		min_column = 0;
	if (min_row < 0)
		min_row = 0;
	if (min_level < 0)
		min_level = 0;
	if (max_column >= world_ptr->columns)
		max_column = world_ptr->columns;
	if (max_row >= world_ptr->rows)
		max_row = world_ptr->rows;
	if (max_level >= world_ptr->levels)
		max_level = world_ptr->levels;

	// Now step through all blocks in the bounding box, setting the flags to
	// calculate the active lights.

	for (int level = min_level; level <= max_level; level++)
		for (int row = min_row; row <= max_row; row++)
			for (int column = min_column; column < max_column; column++) {
				block_ptr = world_ptr->get_block_ptr(column, row, level);
				if (block_ptr != NULL)
					block_ptr->set_active_lights = true;
			}
}

//------------------------------------------------------------------------------
// Create a new block, which is a translated version of a block definition.
//------------------------------------------------------------------------------

static block *
create_new_block(block_def *block_def_ptr, square *square_ptr, 
				 vertex translation)		 
{
	block *block_ptr;
	polygon *polygon_ptr;
	polygon_def *polygon_def_ptr;
	part *part_ptr;
	int min_column, min_row, min_level;
	int max_column, max_row, max_level;

	// Create the new block structure, and initialise the block definition
	// pointer, square pointer, sprite angle, time block was put on map,
	// time of last rotational change, solid flag, block translation and
	// polygon list.

	if ((block_ptr = block_def_ptr->new_block(square_ptr)) == NULL)
		memory_error("block");
	block_ptr->start_time_ms = curr_time_ms;
	block_ptr->last_time_ms = 0;
	block_ptr->translation = translation;

	// If the block has one or more lights, calculating the bounding box
	// emcompassing all of these lights, and reset the active lights in all
	// blocks within that bounding box.

	if (block_ptr->light_list != NULL) {
		compute_light_list_bounding_box(block_ptr->light_list, 
			block_ptr->translation, min_column, min_row, min_level, max_column,
			max_row, max_level);
		reset_active_lights(min_column, min_row, min_level, max_column, max_row,
			max_level);
	}

	// If the block is a structural block, transform it into it's final
	// orientation and translation.

	if (block_def_ptr->type == STRUCTURAL_BLOCK) {
		block_ptr->reset_vertices();
		block_ptr->orient();
	}

	// If the block is a sprite, initialise it directly.
	
	else {
		polygon_ptr = block_ptr->polygon_list;
		polygon_def_ptr = polygon_ptr->polygon_def_ptr;
		part_ptr = polygon_def_ptr->part_ptr;
		init_sprite_polygon(block_ptr, polygon_ptr, part_ptr);
	}

	// Check if the block has a POSITION param change and adjust the block to match
	if (block_def_ptr->position.relative_x)
		block_ptr->translation.x += ((float)block_def_ptr->position.x / TEXELS_PER_UNIT);
	else
		block_ptr->translation.x = ((float)block_def_ptr->position.x / TEXELS_PER_UNIT);
	if (block_def_ptr->position.relative_y)
		block_ptr->translation.y += ((float)block_def_ptr->position.y / TEXELS_PER_UNIT);
	else
		block_ptr->translation.y = ((float)block_def_ptr->position.y / TEXELS_PER_UNIT);
	if (block_def_ptr->position.relative_z)
		block_ptr->translation.z += ((float)block_def_ptr->position.z / TEXELS_PER_UNIT);
	else
		block_ptr->translation.z = ((float)block_def_ptr->position.z / TEXELS_PER_UNIT);

	// Return a pointer to the new block.

	return(block_ptr);
}

//------------------------------------------------------------------------------
// Set the delay for a "timer" trigger.
//------------------------------------------------------------------------------

void
set_trigger_delay(trigger *trigger_ptr, int curr_time_ms)
{
	// Set the start time to the current time.

	trigger_ptr->start_time_ms = curr_time_ms;

	// Generate a random delay time in the allowable range.

	trigger_ptr->delay_ms = trigger_ptr->delay_range.min_delay_ms;
	if (trigger_ptr->delay_range.delay_range_ms > 0)
		trigger_ptr->delay_ms += (int)((float)rand() / RAND_MAX * 
			trigger_ptr->delay_range.delay_range_ms);
}


//------------------------------------------------------------------------------
// Add a copy of the given trigger to the global trigger list.
//------------------------------------------------------------------------------

void
add_trigger_to_global_list(trigger *trigger_ptr, int column, int row, int level)
{
	// Initialise the trigger.

	trigger_ptr->position.set_map_position(column, row, level);
	trigger_ptr->next_trigger_ptr = NULL;

	// Add the trigger to the end of the global trigger list.

	if (last_global_trigger_ptr != NULL)
		last_global_trigger_ptr->next_trigger_ptr = trigger_ptr;
	else
		global_trigger_list = trigger_ptr;
	last_global_trigger_ptr = trigger_ptr;
}

//------------------------------------------------------------------------------
// Initialise the state of a "step in", "step out" or "timer" trigger.
//------------------------------------------------------------------------------

void
init_global_trigger(trigger *trigger_ptr)
{
	vector distance;
	float distance_squared;

	switch (trigger_ptr->trigger_flag) {
	case STEP_IN:
	case STEP_OUT:
		distance = trigger_ptr->position - player_viewpoint.position;
		distance_squared = distance.dx * distance.dx + 
			distance.dy * distance.dy + distance.dz * distance.dz;
		trigger_ptr->previously_inside = 
			distance_squared <= trigger_ptr->radius_squared;
		break;
	case TIMER:
		set_trigger_delay(trigger_ptr, curr_time_ms);
	}
}

//------------------------------------------------------------------------------
// Set the player's collision box and step height.
//------------------------------------------------------------------------------

void
set_player_size(void)
{
	float half_y = player_dimensions.y / 2.0f;
	VEC_set(&player_collision_box.maxDim, player_dimensions.x / 2.0f, 
		half_y, player_dimensions.z / 2.0f);
	VEC_set(&player_collision_box.offsToCentre, 0.0f, half_y, 0.0f);
	player_step_height = half_y;
}

//------------------------------------------------------------------------------
// Initialise the player's collision box.
//------------------------------------------------------------------------------

void
init_player_collision_box(void)
{	
	int vertices, vertex_no;
	vertex *vertex_list, *vertex_ptr;
	float min_x, min_y;
	float max_x, max_y;

	// Initialise the minimum and maximum coordinates to the first vertex.
 
	vertices = player_block_ptr->vertices;
	vertex_list = player_block_ptr->vertex_list;
	vertex_ptr = &vertex_list[0];
	min_x = vertex_ptr->x;
	min_y = vertex_ptr->y;
	max_x = vertex_ptr->x;
	max_y = vertex_ptr->y;

	// Step through the remaining vertices and remember the minimum and
	// maximum X and Y coordinates.  We don't bother with the minimum and
	// maximum Z coordinates because they will be the same.

	for (vertex_no = 1; vertex_no < vertices; vertex_no++) {
		vertex_ptr = &vertex_list[vertex_no];
		if (vertex_ptr->x < min_x)
			min_x = vertex_ptr->x;
		else if (vertex_ptr->x > max_x)
			max_x = vertex_ptr->x;
		if (vertex_ptr->y < min_y)
			min_y = vertex_ptr->y;
		else if (vertex_ptr->y > max_y)
			max_y = vertex_ptr->y;
	}

	// Set the player's size.

	player_dimensions.set(max_x - min_x, max_y - min_y, max_x - min_x);
	set_player_size();
}

//------------------------------------------------------------------------------
// Create the player block.
//------------------------------------------------------------------------------

bool
create_player_block(void)
{
	block_def *block_def_ptr;
	vertex no_translation;

	// If a player block symbol was specified...

	if (player_block_symbol != 0) {

		// Get the player block definition.

		switch (world_ptr->map_style) {
		case SINGLE_MAP:
			if ((block_def_ptr = custom_blockset_ptr->get_block_def(
				(char)player_block_symbol)) == NULL && (block_def_ptr = 
				get_block_def((char)player_block_symbol)) == NULL) {
				warning("There is no block with symbol \"%c\"", 
					(char)player_block_symbol);
				return(false);
			}
			break;
		case DOUBLE_MAP:
			if ((block_def_ptr = custom_blockset_ptr->get_block_def(
				player_block_symbol)) == NULL && (block_def_ptr = 
				get_block_def(player_block_symbol)) == NULL) {
				warning("There is no block with symbol \"%c%c\"", 
					player_block_symbol >> 7, player_block_symbol & 127);
				return(false);
			}
		}

		// If the player block definition is not an animated block return warning

		if (!block_def_ptr->animated) {
			warning("Player block \"%c\" must be an animated block", 
					player_block_symbol);
			return(false);
		}

		// Create the player block with no translation; that will be added
		// when rendering the player block.

		player_block_ptr = create_new_block(block_def_ptr, NULL, no_translation);

		// Initialise the player's collision box.

		init_player_collision_box();
	}

	// If a player size was specified, set it.

	else
		set_player_size();

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Determine if two polygons are identical (share the same vertices).
//------------------------------------------------------------------------------

static bool
polygons_identical(block *block1_ptr, polygon_def *polygon_def1_ptr,
				   block *block2_ptr, polygon_def *polygon_def2_ptr)
{
	vertex vertex1, vertex2;

	// If the polygons don't have the same number of vertices, they are
	// different.

	if (polygon_def1_ptr->vertices != polygon_def2_ptr->vertices)
		return(false);

	// Otherwise we must compare each vertex in polygon #1 with the vertices
	// in polygon #2.  If all match, the polygons are identical.

	vertex *vertex_list1 = block1_ptr->vertex_list;
	vertex *vertex_list2 = block2_ptr->vertex_list;
	vertex_def *vertex_def_list1 = polygon_def1_ptr->vertex_def_list;
	vertex_def *vertex_def_list2 = polygon_def2_ptr->vertex_def_list;
	bool total_match = true;
	for (int index1 = 0; index1 < polygon_def1_ptr->vertices; index1++) {
		vertex1 = vertex_list1[vertex_def_list1[index1].vertex_no] +
			block1_ptr->translation;
		bool match = false;
		for (int index2 = 0; index2 < polygon_def2_ptr->vertices; index2++) {
			vertex2 = vertex_list2[vertex_def_list2[index2].vertex_no] +
				block2_ptr->translation;
			if (vertex1 == vertex2) {
				match = true;
				break;
			}
		}
		if (!match) {
			total_match = false;
			break;
		}
	}
	return(total_match);
}

//------------------------------------------------------------------------------
// Determine which polygons in a block and it's adjacent blocks are active.
// Pairs of one-sided polygons that share the same vertices and face each other
// can be deactivated safely.  Zero-sided polygons are *always* deactivated,
// and two-sided polygons are *never* deactivated.
//------------------------------------------------------------------------------

int
compute_active_polygons(block *block_ptr, int column, int row, int level,
						bool check_all_sides)
{
	int inactive_polygons;
	int adj_direction;
	int adj_column, adj_row, adj_level;
	block *adj_block_ptr;
	polygon *polygon_ptr, *adj_polygon_ptr;
	polygon_def *polygon_def_ptr, *adj_polygon_def_ptr;
	bool *polygon_active_ptr, *adj_polygon_active_ptr;
	part *part_ptr, *adj_part_ptr;
	int polygon_no, adj_polygon_no;

	// Step through the polygons of the center block, deactivating zero-sided
	// polygons and ignoring two-sided polygons.  If we're not checking all
	// sides of the center block, only one-sided polygons facing east, south or
	// up are processed further.
	
	inactive_polygons = 0;
	for (polygon_no = 0; polygon_no < block_ptr->polygons; polygon_no++) {
		polygon_ptr = &block_ptr->polygon_list[polygon_no];
		polygon_def_ptr = polygon_ptr->polygon_def_ptr;
		polygon_active_ptr = &polygon_ptr->active;
		part_ptr = polygon_def_ptr->part_ptr;
		switch (part_ptr->faces) {
		case 0:
			*polygon_active_ptr = false;
			inactive_polygons++;
			continue;
		case 1:
			if (!polygon_ptr->side || 
				(!check_all_sides && polygon_ptr->direction >= 3))
				continue;
			break;
		case 2:
			continue;
		}

		// Get a pointer to the block adjacent to this side polygon.  If it
		// doesn't exist, the side polygon remains active.

		adj_column = column + adj_block_column[polygon_ptr->direction];
		adj_row = row + adj_block_row[polygon_ptr->direction];
		adj_level = level + adj_block_level[polygon_ptr->direction];
		adj_block_ptr = world_ptr->get_block_ptr(adj_column, adj_row, 
			adj_level);
		if (adj_block_ptr == NULL)
			continue;

		// Step through the side polygons of the adjacent block that face
		// towards the side polygon of the center block, and are one-sided.

		adj_direction = (polygon_ptr->direction + 3) % 6;
		for (adj_polygon_no = 0; adj_polygon_no < adj_block_ptr->polygons;
			adj_polygon_no++) {
			adj_polygon_ptr = &adj_block_ptr->polygon_list[adj_polygon_no];
			adj_polygon_def_ptr = adj_polygon_ptr->polygon_def_ptr;
			adj_polygon_active_ptr = &adj_polygon_ptr->active;
			adj_part_ptr = adj_polygon_def_ptr->part_ptr;
			if (adj_part_ptr->faces != 1 || !adj_polygon_ptr->side || 
				adj_polygon_ptr->direction != adj_direction)
				continue;

			// Compare the current side polygon with the adjacent side polygon.
			// If their vertices match, we make both polygons inactive; we then
			// break out of the loop since no other adjacent polygon will match
			// the current one.

			if (polygons_identical(block_ptr, polygon_def_ptr, adj_block_ptr,
				adj_polygon_def_ptr)) {
					*polygon_active_ptr = false;
					*adj_polygon_active_ptr = false;
					inactive_polygons += 2;
				break;
			}
		}
	}
	return(inactive_polygons);
}

//------------------------------------------------------------------------------
// Reset the state of side polygons adjacent to this block.
//------------------------------------------------------------------------------

void
reset_active_polygons(int column, int row, int level)
{
	int direction, adj_direction;
	int adj_column, adj_row, adj_level;
	block *adj_block_ptr;
	polygon *adj_polygon_ptr;
	polygon_def *adj_polygon_def_ptr;
	bool *adj_polygon_active_ptr;
	part *adj_part_ptr;
	int adj_polygon_no;

	// Step through the six possible directions.
	
	for (direction = 0; direction < 6; direction++) {

		// Get a pointer to the adjacent block in this direction.  If it 
		// doesn't exist, continue.

		adj_column = column + adj_block_column[direction];
		adj_row = row + adj_block_row[direction];
		adj_level = level + adj_block_level[direction];
		adj_block_ptr = world_ptr->get_block_ptr(adj_column, adj_row, adj_level);
		if (adj_block_ptr == NULL)
			continue;

		// Step through the side polygons of the adjacent block that face
		// towards the center block and are one-sided, and make them active.

		adj_direction = (direction + 3) % 6;
		for (adj_polygon_no = 0; adj_polygon_no < adj_block_ptr->polygons;
			adj_polygon_no++) {
			adj_polygon_ptr = &adj_block_ptr->polygon_list[adj_polygon_no];
			adj_polygon_def_ptr = adj_polygon_ptr->polygon_def_ptr;
			adj_polygon_active_ptr = &adj_polygon_ptr->active;
			adj_part_ptr = adj_polygon_def_ptr->part_ptr;
			if (adj_polygon_ptr->side && adj_part_ptr->faces == 1 &&
				adj_polygon_ptr->direction == adj_direction)
				*adj_polygon_active_ptr = true;
		}
	}
}

//------------------------------------------------------------------------------
// Determine whether the given square has an entrance.
//------------------------------------------------------------------------------

static bool
square_has_entrance(square *square_ptr)
{
	entrance *entrance_ptr;

	// Check whether the square has a block with an entrance on it.

	if (square_ptr->block_ptr != NULL && 
		square_ptr->block_ptr->entrance_ptr != NULL)
		return(true);
		
	// Search the global entrance list looking for entrances on this square.

	entrance_ptr = global_entrance_list;
	while (entrance_ptr != NULL) {
		if (entrance_ptr->square_ptr == square_ptr)
			return(true);
		entrance_ptr = entrance_ptr->next_entrance_ptr;
	}
	return(false);
}


//------------------------------------------------------------------------------
// Add an action to the global list of clock actions.
//------------------------------------------------------------------------------

void
add_clock_action(action *action_ptr)
{
	for (int j = 0; j < active_clock_action_count; j++) {
		if (active_clock_action_list[j] == action_ptr)
			return;
	}
	active_clock_action_list[active_clock_action_count] = action_ptr;
	active_clock_action_count++;
}

//------------------------------------------------------------------------------
// Delete an action from the global list of clock actions.
//------------------------------------------------------------------------------
void
remove_clock_action(action *action_ptr)
{
	for (int j = 0; j < active_clock_action_count; j++) {
		if (active_clock_action_list[j] == action_ptr) {
			for (int k = j; k < active_clock_action_count; k++)
				active_clock_action_list[k] = active_clock_action_list[k + 1];
			active_clock_action_count--;
			return;
		}
	}
}

//------------------------------------------------------------------------------
// Delete an action from the global list of clock actions.
//------------------------------------------------------------------------------

void
remove_clock_action_by_block(block *block_ptr)
{
	for (int j = 0; j < active_clock_action_count; j++) {
		if (active_clock_action_list[j]->trigger_ptr->block_ptr == block_ptr) {
			for (int k = j; k < active_clock_action_count; k++)
				active_clock_action_list[k] = active_clock_action_list[k + 1];
			active_clock_action_count--;
			return;
		}
	}
}

//------------------------------------------------------------------------------
// Delete an action type and block from the global list of clock actions.
//------------------------------------------------------------------------------

void
remove_clock_action_by_type(action *action_ptr, int type)
{
	block *block_ptr = action_ptr->trigger_ptr->block_ptr;
	for (int j = 0; j < active_clock_action_count; j++) {
		if (active_clock_action_list[j]->type == type && active_clock_action_list[j]->trigger_ptr->block_ptr == block_ptr) {
			for (int k = j; k < active_clock_action_count; k++)
				active_clock_action_list[k] = active_clock_action_list[k + 1];
			active_clock_action_count--;
		}
	}
}

//------------------------------------------------------------------------------
// Add a new fixed block to the map on the given square, using the given
// block definition as the template.
//------------------------------------------------------------------------------

block *
add_fixed_block(block_def *block_def_ptr, square *square_ptr,
				bool update_active_polygons)
{
	int column, row, level;
	vertex translation;
	block *block_ptr;
	trigger * trigger_ptr;

	// Create a block from the given block definition, at the given square.

	world_ptr->get_square_location(square_ptr, &column, &row, &level);
	translation.set_map_translation(column, row, level);
	block_ptr = create_new_block(block_def_ptr, square_ptr, translation);
	square_ptr->block_ptr = block_ptr;

	// If this block has a light, sound, popup or trigger list, or an entrance,
	// add it to the fixed block list.

	if (block_ptr->light_list != NULL || block_ptr->sound_list != NULL ||
		block_ptr->popup_list != NULL || block_ptr->trigger_list != NULL ||
		block_ptr->entrance_ptr) {
		block_ptr->next_block_ptr = fixed_block_list;
		fixed_block_list = block_ptr;
	}

	// If the active polygons of the new block and adjacent blocks must be
	// updated, do so.

	if (update_active_polygons)
		compute_active_polygons(block_ptr, column, row, level, true);

	// If the player is standing on the square, set a flag indicating that the
	// player block has been replaced.

	if (player_column == column && player_row == row && player_level == level)
		player_block_replaced = true;

	// If this block has a start action on it fire it up.

	if (block_ptr->trigger_flags & START_UP) {
		trigger_ptr = block_ptr->trigger_list;
		while (trigger_ptr) {
			if (trigger_ptr->trigger_flag & START_UP) {
				execute_trigger(trigger_ptr);
				break;
			}
			trigger_ptr = trigger_ptr->next_trigger_ptr;
		}
	}

	// Return a pointer to the new block.

	return(block_ptr);
}

//------------------------------------------------------------------------------
// Add a new movable block to the map at the given map position, using the
// given block definition as the template.
//------------------------------------------------------------------------------

block *
add_movable_block(block_def *block_def_ptr, vertex translation)
{
	block *block_ptr;
	trigger * trigger_ptr;

	// Create a block from the given block definition, at the given map
	// position.

	block_ptr = create_new_block(block_def_ptr, NULL, translation);

	// Add the block to the movable block list.

	block_ptr->next_block_ptr = movable_block_list;
	movable_block_list = block_ptr;

	// If this block has a start action on it fire it up.

	if (block_ptr->trigger_flags & START_UP) {
		trigger_ptr = block_ptr->trigger_list;
		while (trigger_ptr) {
			if (trigger_ptr->trigger_flag & START_UP) {
				execute_trigger(trigger_ptr);
				break;
			}
			trigger_ptr = trigger_ptr->next_trigger_ptr;
		}
	}
	return(block_ptr);
}

//------------------------------------------------------------------------------
// Remove a fixed block from the given square.
//------------------------------------------------------------------------------

void
remove_fixed_block(square *square_ptr)
{
	block *block_ptr;
	int column, row, level;
	block_def *block_def_ptr;
	block *curr_block_ptr, *prev_block_ptr;
	int min_column, min_row, min_level;
	int max_column, max_row, max_level;

	// If there is no block on the square, there is nothing to do.

	block_ptr = square_ptr->block_ptr;
	if (block_ptr == NULL)
		return;

	// Reset the active polygons adjacent to this square.

	world_ptr->get_square_location(square_ptr, &column, &row, &level);
	reset_active_polygons(column, row, level);

	// If the player is standing on the square, set a flag indicating that the
	// player block has been replaced.

	if (player_column == column && player_row == row && player_level == level)
		player_block_replaced = true;

	// If this block has a light list, calculate the bounding box emcompassing
	// all these lights, and reset the active light lists for all blocks inside
	// this bounding box.

	if (block_ptr->light_list != NULL) {
		compute_light_list_bounding_box(block_ptr->light_list, 
			block_ptr->translation, min_column, min_row, min_level, max_column,
			max_row, max_level);
		reset_active_lights(min_column, min_row, min_level, max_column, max_row,
			max_level);
	}

	// If this block has a sound list, stop all sounds in that list.

	if (block_ptr->sound_list != NULL)
		stop_sounds_in_sound_list(block_ptr->sound_list);

	// If this block has a light, sound, popup or trigger list, or an entrance,
	// remove it from the fixed block list.

	if (block_ptr->light_list != NULL || block_ptr->sound_list != NULL ||
		block_ptr->popup_list != NULL || block_ptr->trigger_list != NULL ||
		block_ptr->entrance_ptr) {
		prev_block_ptr = NULL;
		curr_block_ptr = fixed_block_list;
		while (curr_block_ptr != block_ptr) {
			prev_block_ptr = curr_block_ptr;
			curr_block_ptr = curr_block_ptr->next_block_ptr;
		}
		if (prev_block_ptr == NULL)
			fixed_block_list = curr_block_ptr->next_block_ptr;
		else
			prev_block_ptr->next_block_ptr = curr_block_ptr->next_block_ptr;
	}

	// check to see if there are any clock actions to remove
	remove_clock_action_by_block(block_ptr);

	// Delete the block.

	block_def_ptr = block_ptr->block_def_ptr;
	block_def_ptr->del_block(block_ptr);

	// Clear the pointer to the block.

	square_ptr->block_ptr = NULL;
}

//------------------------------------------------------------------------------
// Remove a movable block.
//------------------------------------------------------------------------------

void
remove_movable_block(block *block_ptr)
{
	trigger *trigger_ptr;
	block *curr_block_ptr, *prev_block_ptr;
	block_def *block_def_ptr;
	int min_column, min_row, min_level;
	int max_column, max_row, max_level;

	// If this block has a light list, calculate the bounding box emcompassing
	// all these lights, and reset the active light lists for all blocks inside
	// this bounding box.

	if (block_ptr->light_list != NULL) {
		compute_light_list_bounding_box(block_ptr->light_list, 
			block_ptr->translation, min_column, min_row, min_level, max_column,
			max_row, max_level);
		reset_active_lights(min_column, min_row, min_level, max_column, max_row,
			max_level);
	}
	// If this block has a sound list, stop all sounds in that list.

	if (block_ptr->sound_list != NULL)
		stop_sounds_in_sound_list(block_ptr->sound_list);

	// Remove references to the block from the active trigger list.  This
	// prevents replacements and scripts from being executed that require
	// that block to exist.

	for (int j = 0; j < active_trigger_count; j++) {
		trigger_ptr = active_trigger_list[j];
		if (trigger_ptr != NULL && trigger_ptr->block_ptr == block_ptr)
			active_trigger_list[j] = NULL;
	}

	// Remove the block from the movable block list.

	prev_block_ptr = NULL;
	curr_block_ptr = movable_block_list;
	while (curr_block_ptr != block_ptr) {
		prev_block_ptr = curr_block_ptr;
		curr_block_ptr = curr_block_ptr->next_block_ptr;
	}
	if (prev_block_ptr == NULL)
		movable_block_list = curr_block_ptr->next_block_ptr;
	else
		prev_block_ptr->next_block_ptr = curr_block_ptr->next_block_ptr;

	// check to see if there are any clock actions to remove

	remove_clock_action_by_block(block_ptr);

	// Delete the block.

	block_def_ptr = block_ptr->block_def_ptr;
	block_def_ptr->del_block(block_ptr);
}

//------------------------------------------------------------------------------
// Convert a brightness level to a brightness index.
//------------------------------------------------------------------------------

int
get_brightness_index(float brightness)
{
	int brightness_index;

	// Make sure the brightness is between 0.0 and 1.0.

	if (brightness < 0.0)
		brightness = 0.0;
	if (brightness > 1.0)
		brightness = 1.0;

	// Determine the brightness index.

	brightness_index = (int)((1.0 - brightness) * MAX_BRIGHTNESS_INDEX);

	// Double-check the brightness index is within range before returning it.

	if (brightness_index < 0)
		brightness_index = 0;
	else if (brightness_index > MAX_BRIGHTNESS_INDEX)
		brightness_index = MAX_BRIGHTNESS_INDEX;
	return(brightness_index);
}

//------------------------------------------------------------------------------
// Convert a brightness index to a brightness level.
//------------------------------------------------------------------------------

float
get_brightness(int brightness_index)
{
	if (brightness_index < 0)
		brightness_index = 0;
	else if (brightness_index > MAX_BRIGHTNESS_INDEX)
		brightness_index = MAX_BRIGHTNESS_INDEX;
	return 1.0f - (float)brightness_index / MAX_BRIGHTNESS_INDEX;
}

//------------------------------------------------------------------------------
// Create the polygon and vertex lists for a sprite block.
//------------------------------------------------------------------------------

void
create_sprite_polygon(block_def *block_def_ptr, part *part_ptr)
{
	int index;
	polygon_def *polygon_def_ptr;

	// Create the vertex and polygon lists.

	block_def_ptr->create_vertex_list(4);
	block_def_ptr->create_polygon_def_list(1);
	polygon_def_ptr = block_def_ptr->polygon_def_list;
	polygon_def_ptr->part_no = 0;
	polygon_def_ptr->part_ptr = part_ptr;

	// Create the vertex definition list for the polygon and initialise it.

	polygon_def_ptr->create_vertex_def_list(4);
	for (index = 0; index < 4; index++)
		polygon_def_ptr->vertex_def_list[index].vertex_no = index;
}

//------------------------------------------------------------------------------
// Create the sky.
//------------------------------------------------------------------------------

static void
create_sky(void)
{
	blockset *blockset_ptr;
	texture *texture_ptr;

	// Find the first blockset that defines a sky, if there is one.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if (blockset_ptr->sky_defined)
			break;
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}

	// Initialise the sky texture, custom texture and colour.

	sky_texture_ptr = NULL;
	custom_sky_texture_ptr = NULL;
	unlit_sky_colour.set_RGB(0,0,0);
	
	// A custom sky texture/colour takes precedence over a blockset sky
	// texture/colour in the first blockset.  If the sky texture is a custom 
	// texture, use the custom sky colour or placeholder texture for now.

	texture_ptr = custom_blockset_ptr->sky_texture_ptr;
	if (texture_ptr != NULL) {
		if (texture_ptr->blockset_ptr == NULL) {
			custom_sky_texture_ptr = texture_ptr;
			if (custom_blockset_ptr->sky_colour_set)
				unlit_sky_colour = custom_blockset_ptr->sky_colour;
			else
				sky_texture_ptr = placeholder_texture_ptr;
		} else
			sky_texture_ptr = texture_ptr;
	} else if (custom_blockset_ptr->sky_colour_set)
		unlit_sky_colour = custom_blockset_ptr->sky_colour;
	else if (blockset_ptr != NULL) {
		sky_texture_ptr = blockset_ptr->sky_texture_ptr;
		unlit_sky_colour = blockset_ptr->sky_colour;
	}
	
	// Set the sky brightness and related data.  A custom sky brightness takes
	// prececedence over a blockset sky brightness.

	if (custom_blockset_ptr->sky_defined)
		sky_brightness = custom_blockset_ptr->sky_brightness;
	else if (blockset_ptr && blockset_ptr->sky_defined)
		sky_brightness = blockset_ptr->sky_brightness;
	else
		sky_brightness = 1.0f;
	sky_brightness_index = get_brightness_index(sky_brightness);
	sky_colour = unlit_sky_colour;
	sky_colour.adjust_brightness(sky_brightness);
	sky_colour_pixel = RGB_to_display_pixel(sky_colour);
}

//------------------------------------------------------------------------------
// Create the ground block definition.
//------------------------------------------------------------------------------

static void
create_ground_block_def(void)
{
	blockset *blockset_ptr;
	part *part_ptr;
	texture *ground_texture_ptr;
	vertex *vertex_list;
	polygon_def *polygon_def_ptr;
	BSP_node *BSP_node_ptr;
	int index;

	// If there is no custom ground definition, then we don't activate the
	// ground.

	if (!custom_blockset_ptr->ground_defined)
		return;

	// Find the first blockset that defines a ground, if there is one.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if (blockset_ptr->ground_defined)
			break;
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	
	// Create the ground block definition.  This is not a custom block since
	// it defines it's own BSP tree.

	NEW(ground_block_def_ptr, block_def);
	if (ground_block_def_ptr == NULL)
		memory_error("ground block definition");
	ground_block_def_ptr->custom = false;
	ground_block_def_ptr->single_symbol = ' ';
	ground_block_def_ptr->double_symbol = ' ';

	// Create the part list with one part.

	ground_block_def_ptr->create_part_list(1);
	part_ptr = ground_block_def_ptr->part_list;

	// A custom ground texture/colour takes precedence over a blockset ground
	// texture/colour in the first blockset.  If the ground texture is a custom 
	// texture, use the custom ground colour or placeholder texture for now.

	ground_texture_ptr = custom_blockset_ptr->ground_texture_ptr;
	if (ground_texture_ptr != NULL) {
		if (ground_texture_ptr->blockset_ptr == NULL) {
			part_ptr->custom_texture_ptr = ground_texture_ptr;
			if (custom_blockset_ptr->ground_colour_set)
				part_ptr->colour = custom_blockset_ptr->ground_colour;
			else
				part_ptr->texture_ptr = placeholder_texture_ptr;
		} else
			part_ptr->texture_ptr = ground_texture_ptr;
	} else if (custom_blockset_ptr->ground_colour_set)
		part_ptr->colour = custom_blockset_ptr->ground_colour;
	else if (blockset_ptr != NULL) {
		part_ptr->texture_ptr = blockset_ptr->ground_texture_ptr;
		part_ptr->colour = blockset_ptr->ground_colour;
	}
	part_ptr->normalised_colour = part_ptr->colour;
	part_ptr->normalised_colour.normalise();

	// Create the vertex list with four vertices.

	ground_block_def_ptr->create_vertex_list(4);

	// Initialise the vertices so that the polygon covers the top of the
	// bounding cube.

	vertex_list = ground_block_def_ptr->vertex_list;
	vertex_list[0].set(0.0, UNITS_PER_BLOCK, UNITS_PER_BLOCK);
	vertex_list[1].set(UNITS_PER_BLOCK, UNITS_PER_BLOCK, UNITS_PER_BLOCK);
	vertex_list[2].set(UNITS_PER_BLOCK, UNITS_PER_BLOCK, 0.0);
	vertex_list[3].set(0.0, UNITS_PER_BLOCK, 0.0);

	// Create the polygon list with one polygon.

	ground_block_def_ptr->create_polygon_def_list(1);
	polygon_def_ptr = ground_block_def_ptr->polygon_def_list;

	// Create the vertex definition list for the polygon and initialise it.

	if (!polygon_def_ptr->create_vertex_def_list(4))
		memory_error("ground vertex definition list");
	for (index = 0; index < 4; index++)
		polygon_def_ptr->vertex_def_list[index].vertex_no = index;

	// Initialise the polygon's part, and project it's texture from the top.

	polygon_def_ptr->part_no = 0;
	polygon_def_ptr->part_ptr = part_ptr;
	polygon_def_ptr->project_texture(vertex_list, UP);

	// Create a single BSP node that points to the polygon.

	NEW(BSP_node_ptr, BSP_node);
	if (BSP_node_ptr == NULL)
		memory_error("BSP tree node");
	ground_block_def_ptr->BSP_tree = BSP_node_ptr;
}

//------------------------------------------------------------------------------
// Create the orb.
//------------------------------------------------------------------------------

static void
create_orb(void)
{
	blockset *blockset_ptr;
	texture *texture_ptr;
	direction orb_light_direction;
	RGBcolour orb_colour;

	// If there is no custom orb definition, then we don't activate the orb.

	if (!custom_blockset_ptr->orb_defined)
		return;
	
	// Find the first blockset that defines an orb, if there is one.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if (blockset_ptr->orb_defined)
			break;
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	
	// The orb texture in the custom blockset takes precedence over the orb
	// texture in the first blockset.  If the orb texture is a custom texture,
	// use the placeholder texture for now.

	texture_ptr = custom_blockset_ptr->orb_texture_ptr;
	if (texture_ptr != NULL) {
		if (texture_ptr->blockset_ptr == NULL) {
			custom_orb_texture_ptr = texture_ptr;
			orb_texture_ptr = placeholder_texture_ptr;
		} else {
			custom_orb_texture_ptr = NULL;
			orb_texture_ptr = texture_ptr;
		}
	} else if (blockset_ptr != NULL && blockset_ptr->orb_texture_ptr != NULL) {
		custom_orb_texture_ptr = NULL;
		orb_texture_ptr = blockset_ptr->orb_texture_ptr;
	}
	
	// Create a directional light with default settings.

	NEW(orb_light_ptr, light);
	if (orb_light_ptr == NULL)
		memory_error("orb light");
	orb_light_ptr->style = DIRECTIONAL_LIGHT;

	// The orb direction in the custom blockset takes precedence over the
	// orb direction in the first available blockset.  If none define
	// one, the default orb direction is used.

	if (custom_blockset_ptr->orb_direction_set)
		orb_direction.set(custom_blockset_ptr->orb_direction.angle_x,
			custom_blockset_ptr->orb_direction.angle_y);
	else if (blockset_ptr && blockset_ptr->orb_direction_set)
		orb_direction.set(blockset_ptr->orb_direction.angle_x,
			blockset_ptr->orb_direction.angle_y);
	else
		orb_direction.set(0.0f,0.0f);

	// Set the light direction to be in the opposite direction to the orb
	// itself.

	orb_light_direction.set(-orb_direction.angle_x, 
		orb_direction.angle_y + 180.0f);
	orb_light_ptr->set_direction(orb_light_direction);

	// The orb colour in the custom blockset takes precedence over the orb
	// colour in the first available blockset.  If none define one, the default
	// orb colour is used.

	if (custom_blockset_ptr->orb_colour_set)
		orb_colour = custom_blockset_ptr->orb_colour;
	else if (blockset_ptr && blockset_ptr->orb_colour_set)
		orb_colour = blockset_ptr->orb_colour;
	else
		orb_colour.set_RGB(255,255,255);
	orb_light_ptr->colour = orb_colour;

	// The orb brightness in the custom blockset takes precedence over the
	// orb brightness in the first available blockset.  If none define
	// one, the default orb brightness is used.

	if (custom_blockset_ptr->orb_brightness_set)
		orb_brightness = custom_blockset_ptr->orb_brightness;
	else if (blockset_ptr && blockset_ptr->orb_brightness_set)
		orb_brightness = blockset_ptr->orb_brightness;
	else
		orb_brightness = 1.0f;
	orb_light_ptr->set_intensity(orb_brightness);
	orb_brightness_index = get_brightness_index(orb_brightness);

	// Compute the size and half size of the orb.

	orb_width = 12.0f * horz_pixels_per_degree;
	orb_height = 12.0f * vert_pixels_per_degree;
	half_orb_width = orb_width / 2.0f;
	half_orb_height = orb_height / 2.0f;

	// The orb exit in the custom blockset takes precedence over the orb exit
	// in the first available blockset.  If none define one, there is no orb
	// exit.

	if (custom_blockset_ptr->orb_exit_ptr != NULL)
		orb_exit_ptr = custom_blockset_ptr->orb_exit_ptr;
	else if (blockset_ptr != NULL && blockset_ptr->orb_exit_ptr != NULL)
		orb_exit_ptr = blockset_ptr->orb_exit_ptr;
	else
		orb_exit_ptr = NULL;
}

//------------------------------------------------------------------------------
// Update the popups in the given popup list that do not have a custom texture.
//------------------------------------------------------------------------------

static void
update_non_custom_textures_in_popup_list(popup *popup_list)
{
	popup *popup_ptr = popup_list;
	while (popup_ptr != NULL) {
		texture *bg_texture_ptr = popup_ptr->bg_texture_ptr;
		if (bg_texture_ptr == NULL || bg_texture_ptr->blockset_ptr != NULL)
			init_popup(popup_ptr);
		popup_ptr = popup_ptr->next_popup_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the popups in the given blockset that do not have a custom texture.
//------------------------------------------------------------------------------

static void
update_non_custom_textures_in_blockset(blockset *blockset_ptr)
{
	block_def *block_def_ptr = blockset_ptr->block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->popup_list != NULL)
			update_non_custom_textures_in_popup_list(block_def_ptr->popup_list);
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
}

//------------------------------------------------------------------------------
// Initialise the spot.
//------------------------------------------------------------------------------

void
init_spot(void)
{
	blockset *blockset_ptr;
	int column, row, level;
	word block_symbol;
	square *square_ptr;
	block_def *block_def_ptr;
	block *block_ptr;
	polygon *polygon_ptr;
	polygon_def *polygon_def_ptr;
	part *part_ptr;
	int inactive_polygons;
	vertex translation;

	// Set the placeholder texture.  The custom placeholder texture takes
	// precedence over any blockset texture.

	if (custom_blockset_ptr->placeholder_texture_ptr != NULL)
		placeholder_texture_ptr = custom_blockset_ptr->placeholder_texture_ptr;
	else {
		blockset_ptr = blockset_list_ptr->first_blockset_ptr;
		while (blockset_ptr != NULL) {
			if (blockset_ptr->placeholder_texture_ptr != NULL) {
				placeholder_texture_ptr = blockset_ptr->placeholder_texture_ptr;
				break;
			}
			blockset_ptr = blockset_ptr->next_blockset_ptr;
		}
	}

	// Create the sky and orb, and the ground block definition.

	create_sky();
	create_orb();
	create_ground_block_def();

	// Step through the map and create a block for each occupied square.
	// Undefined or invalid map symbols are replaced by the empty block.

	for (level = 0; level < world_ptr->levels; level++)
		for (row = 0; row < world_ptr->rows; row++)
			for (column = 0; column < world_ptr->columns; column++) {
				square_ptr = world_ptr->get_square_ptr(column, row, level);
				block_symbol = square_ptr->block_symbol;
				if (block_symbol != NULL_BLOCK_SYMBOL) {

					// Refresh the player window.

					if (!refresh_player_window())
						error("Parsing aborted");

					// Get a pointer to the block definition assigned to this
					// square.  An invalid or undefined map symbol will result
					// in no block.

					if (block_symbol == GROUND_BLOCK_SYMBOL)
						block_def_ptr = ground_block_def_ptr;
					else
						block_def_ptr = block_symbol_table[block_symbol];

					// If there is a block definition for this square, create
					// the block to put on the square or into the movable block
					// list, provided the block allows entrances or is not
					// on a square with an entrance.

					if (block_def_ptr != NULL) {
						if (!block_def_ptr->allow_entrance &&
							square_has_entrance(square_ptr))
							warning("Block \"%s\" cannot be placed on an "
								"entrance square", block_def_ptr->name);
						else if (block_def_ptr->movable) {
							translation.set_map_translation(column, row,
								level);
							add_movable_block(block_def_ptr, translation);
						} else
							add_fixed_block(block_def_ptr, square_ptr, false);
					}
				}
			}

	// Step through the map, and determine which polygons are active.  We
	// refresh the player window during this process, so it doesn't appear to
	// have frozen.

	inactive_polygons = 0;
	for (level = 0; level < world_ptr->levels; level++)
		for (row = 0; row < world_ptr->rows; row++)
			for (column = 0; column < world_ptr->columns; column++) {
				block_ptr = world_ptr->get_block_ptr(column, row, level);
				if (block_ptr != NULL) {
					if (!refresh_player_window())
						error("Parsing aborted");
					inactive_polygons += compute_active_polygons(block_ptr, 
						column, row, level, false);
				}
			}

	// Check that the default entrance was defined.

	if (find_entrance("default") == NULL)
		error("There is no default entrance on the map");

	// Initialise all popups with non-custom background textures.

	update_non_custom_textures_in_popup_list(global_popup_list);
	update_non_custom_textures_in_blockset(custom_blockset_ptr);

	// Create the sound buffer for the ambient sound if it doesn't use a custom
	// wave.

	if (ambient_sound_ptr != NULL && ambient_sound_ptr->wave_ptr != NULL) {
		if (ambient_sound_ptr->wave_ptr->blockset_ptr != NULL && 
			!create_sound_buffer(ambient_sound_ptr))
			warning("Unable to create sound buffer for wave file %s",
				ambient_sound_ptr->wave_ptr->URL);
	}

	// Step through the custom block definitions, and update all parts that
	// have a custom texture with the placeholder texture, if they don't
	// already have a custom colour set.

	block_def_ptr = custom_blockset_ptr->block_def_list;
	while (block_def_ptr != NULL) {
		for (int part_no = 0; part_no < block_def_ptr->parts; part_no++) {
			part_ptr = &block_def_ptr->part_list[part_no];
			if (part_ptr->custom_texture_ptr && !part_ptr->colour_set)
				part_ptr->texture_ptr = placeholder_texture_ptr;
		}
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}

	// Reinitialise the player block if it uses a custom texture to use the
	// placeholder texture, if it doesn't already have a custom colour set.

	if (player_block_ptr != NULL) {
		polygon_ptr = player_block_ptr->polygon_list;
		polygon_def_ptr = polygon_ptr->polygon_def_ptr;
		part_ptr = polygon_def_ptr->part_ptr;
		if (part_ptr->custom_texture_ptr != NULL && !part_ptr->colour_set) {
			part_ptr->texture_ptr = placeholder_texture_ptr;
			init_sprite_polygon(player_block_ptr, polygon_ptr, part_ptr);
			init_player_collision_box();
		}
	}

	// Step through the map and reinitialise all sprite blocks that use a
	// custom texture to use the placeholder texture, if they haven't already
	// got a custom colour set.

	for (column = 0; column < world_ptr->columns; column++)
		for (row = 0; row < world_ptr->rows; row++)
			for (level = 0; level < world_ptr->levels; level++) {
				block_ptr = world_ptr->get_block_ptr(column, row, level);
				if (block_ptr != NULL) {
					block_def_ptr = block_ptr->block_def_ptr;
					if (block_def_ptr->type & SPRITE_BLOCK) {
						polygon_ptr = block_ptr->polygon_list;
						polygon_def_ptr = polygon_ptr->polygon_def_ptr;
						part_ptr = polygon_def_ptr->part_ptr;
						if (part_ptr->custom_texture_ptr && 
							!part_ptr->colour_set) {
							part_ptr->texture_ptr = placeholder_texture_ptr;
							init_sprite_polygon(block_ptr, polygon_ptr, 
								part_ptr);
						}
					}
				}
			}
}

//------------------------------------------------------------------------------
// Display a message indicating that the file with the given URL is being
// loaded.
//------------------------------------------------------------------------------

static void
loading_message(const char *URL)
{
	string file_name;

	// Extract the file name from the URL.

	split_URL(URL, NULL, &file_name, NULL);

	// Display the file name on the task bar.

	set_title("Loading %s", file_name);
}

//------------------------------------------------------------------------------
// Request that a URL be downloaded into the given file path, or the given
// window target (but not both).  If both the file path and target are NULL,
// the URL will be downloaded into a temporary file.  If you don't want the
// URL to be retrieved from the internet cache, set no_cache to true.
//------------------------------------------------------------------------------

void
request_URL(const char *URL, const char *file_path, const char *target, bool no_cache)
{
	// Display a loading message on the toolbar, but only if there is no
	// file path and target specified.

	if (file_path == NULL && target == NULL)
		loading_message(URL);

	// Set the requested URL.

	requested_URL = create_URL(spot_URL_dir, URL);

	// Request that the given URL be downloaded to the given target.

	requested_file_path = file_path;
	requested_target = target;
	requested_no_cache = no_cache;
	URL_download_requested.send_event(true);

	// Reset the URL opened flag.

	curr_URL_opened = false;
}

//------------------------------------------------------------------------------
// Check whether the current URL has been downloaded.
//------------------------------------------------------------------------------

static int
URL_downloaded(void)
{
	bool success;

	// If a player window shutdown request has been sent, pass it on and 
	// indicate the download failed.

	if (player_window_shutdown_requested.event_sent()) {
		player_window_shutdown_requested.send_event(true);
		return(0);
	}

	// Check whether the URL has been opened.

	if (!curr_URL_opened) {
		if (!URL_was_opened.event_sent())
			return(-1);
		else if (!(curr_URL_opened = URL_was_opened.event_value)) {
			curr_URL = requested_URL;
			return(0);
		}
	}

	// Check whether the URL has been downloaded.

	if (!URL_was_downloaded.event_sent())
		return(-1);
	success = URL_was_downloaded.event_value;

	// Get the URL and file path from the plugin thread.  

	curr_URL = downloaded_URL.get();
	if (success)
		curr_file_path = downloaded_file_path.get();

	// If the URL downloaded is not the requested URL, then ignore it.

	if (_stricmp(curr_URL, requested_URL))
		return(-1);

	// Decode the URL and file path to remove encoded characters.

	curr_URL = decode_URL(curr_URL);
	if (success) {
		curr_file_path = decode_URL(curr_file_path);
		curr_file_path = URL_to_file_path(curr_file_path);
	}

	// Return the success or failure status.

	return(success ? 1 : 0);
}

//------------------------------------------------------------------------------
// Download a URL to the given file path, or to a temporary file if the file 
// path is NULL.  If you don't want the URL retrieved from the internet cache,
// set no_cache to true.
//------------------------------------------------------------------------------

bool
download_URL(const char *URL, const char *file_path, bool no_cache)
{
	int result;

	// Request that the URL be downloaded.

	request_URL(URL, file_path, NULL, no_cache);

	// Wait until the browser sends an event indicating that a URL was
	// or was not downloaded.
		
	while ((result = URL_downloaded()) < 0)
		;

	// Return a boolean value indicating if the URL was downloaded or not.

	return(result > 0 ? true : false);
}

//------------------------------------------------------------------------------
// Permit the user to display the error log file if the debug flag is on,
// there were warnings written to the file, and it hasn't already been displayed
// for this spot.
//------------------------------------------------------------------------------

void
display_error_log_file(void)
{
	// If the error log file has already been displayed, don't do it again.

	if (displayed_error_log_file)
		return;
	displayed_error_log_file = false;

	// If the debug level allows warnings, and there are warnings in the error
	// log, then allow it to be displayed in a new browser window if the user 
	// requests it.

	if ((user_debug_level.get() == SHOW_ERRORS_AND_WARNINGS ||
		(user_debug_level.get() == LET_SPOT_DECIDE &&
		spot_debug_level == SHOW_ERRORS_AND_WARNINGS)) && warnings) {
		show_error_log = true;
		display_error.send_event(true);
	}
}

//------------------------------------------------------------------------------
// Initiate the download of the first custom texture or wave.
//------------------------------------------------------------------------------

void
initiate_first_download(void)
{
	// Set the pointers to the first custom texture and wave to download, and
	// indicate no download is currently in progress.

	curr_custom_texture_ptr = custom_blockset_ptr->first_texture_ptr;
	curr_custom_wave_ptr = custom_blockset_ptr->first_wave_ptr;
	curr_download_completed = true;

	// If there aren't any custom textures or waves to be loaded, display the
	// spot title on the task bar, and display the error log file if necessary.

	if (curr_custom_texture_ptr == NULL && curr_custom_wave_ptr == NULL) {
		set_title("%s", spot_title);
		display_error_log_file();
	}
}

//------------------------------------------------------------------------------
// Initiate the download of the next custom texture or wave, if there is one.
//------------------------------------------------------------------------------

void
initiate_next_download(void)
{
	// Choose the next custom texture or wave to download based on which has
	// the lower load index.

	if (curr_custom_texture_ptr != NULL && (curr_custom_wave_ptr == NULL || 
		curr_custom_texture_ptr->load_index < curr_custom_wave_ptr->load_index))
		request_URL(curr_custom_texture_ptr->URL, NULL, NULL, false);
	else
		request_URL(curr_custom_wave_ptr->URL, NULL, NULL, false);

	// Indicate the download has not yet completed.

	curr_download_completed = false;
}

//------------------------------------------------------------------------------
// Generate an oversized texture warning.
//------------------------------------------------------------------------------

static void
oversized_texture_warning(const char *URL, const char *format, ...)
{
	va_list arg_ptr;
	char message[BUFSIZ];

	va_start(arg_ptr, format);
	vbprintf(message, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);
	warning("Cannot use texture %s %s; it has a width or height greater than "
		"256 pixels", URL, message);
}

//------------------------------------------------------------------------------
// Update the popups in the given popup list that depend on the given texture.
//------------------------------------------------------------------------------

static void
update_textures_in_popup_list(popup *popup_list, texture *texture_ptr)
{
	popup *popup_ptr = popup_list;
	while (popup_ptr != NULL) {
		if (popup_ptr->bg_texture_ptr == texture_ptr)
			init_popup(popup_ptr);
		popup_ptr = popup_ptr->next_popup_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the popups in the given blockset that depend on the given texture.
//------------------------------------------------------------------------------

static void
update_textures_in_blockset(blockset *blockset_ptr, texture *texture_ptr)
{
	block_def *block_def_ptr = blockset_ptr->block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->popup_list != NULL)
			update_textures_in_popup_list(block_def_ptr->popup_list, texture_ptr);
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the block definitions, sprites and popups that depend on the given
// custom texture.
//------------------------------------------------------------------------------

void
update_texture_dependancies(texture *custom_texture_ptr)
{
	int column, level, row;
	block_def *block_def_ptr;
	polygon *polygon_ptr;
	polygon_def *polygon_def_ptr;
	part *part_ptr;

 	// Initialise all popups that depend on the given texture.

	update_textures_in_popup_list(global_popup_list, custom_texture_ptr);
	update_textures_in_blockset(custom_blockset_ptr, custom_texture_ptr);


	// Step through the custom block definitions, and update all parts that
	// use the custom texture.

	block_def_ptr = custom_blockset_ptr->block_def_list;
	while (block_def_ptr != NULL) {
		for (int part_no = 0; part_no < block_def_ptr->parts; part_no++) {
			part_ptr = &block_def_ptr->part_list[part_no];
			if (part_ptr->custom_texture_ptr == custom_texture_ptr) {
				part_ptr->texture_ptr = custom_texture_ptr;
			}
		}
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}

	// Reinitialise the player block if it uses the custom texture.

	if (player_block_ptr != NULL) {
		polygon_ptr = player_block_ptr->polygon_list;
		polygon_def_ptr = polygon_ptr->polygon_def_ptr;
		part_ptr = polygon_def_ptr->part_ptr;
		if (part_ptr->texture_ptr == custom_texture_ptr) {
			init_sprite_polygon(player_block_ptr, polygon_ptr, part_ptr);
			init_player_collision_box();
		}
	}

	// Step through the map and reinitialise all sprite blocks that use the
	// custom texture.  If the custom texture is oversized, ignore it.

	for (column = 0; column < world_ptr->columns; column++)
		for (row = 0; row < world_ptr->rows; row++)
			for (level = 0; level < world_ptr->levels; level++) {
				block *block_ptr = world_ptr->get_block_ptr(column, row, level);
				if (block_ptr != NULL) {
					block_def *block_def_ptr = block_ptr->block_def_ptr;
					if (block_def_ptr->type & SPRITE_BLOCK) {
						polygon_ptr = block_ptr->polygon_list;
						polygon_def_ptr = polygon_ptr->polygon_def_ptr;
						part_ptr = polygon_def_ptr->part_ptr;
						if (part_ptr->custom_texture_ptr == custom_texture_ptr) {
							init_sprite_polygon(block_ptr, polygon_ptr, part_ptr);
						}
					}
				}
			}

	// Reinitialise the sky if it uses the custom texture.

	if (custom_sky_texture_ptr == custom_texture_ptr) {
		sky_texture_ptr = custom_texture_ptr;
	}

	// Reinitialise the ground part if it exists and it uses the custom texture.

	if (ground_block_def_ptr != NULL) {
		part_ptr = ground_block_def_ptr->part_list;
		if (part_ptr->custom_texture_ptr == custom_texture_ptr) {
			part_ptr->texture_ptr = custom_texture_ptr;
		}
	}

	// Reinitialise the orb if it exists and uses the custom texture.

	if (custom_orb_texture_ptr == custom_texture_ptr) {
		orb_texture_ptr = custom_texture_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the sounds that depend on the given wave in the given sound list.
//------------------------------------------------------------------------------

static void
update_waves_in_sound_list(sound *sound_list, wave *wave_ptr)
{
	sound *sound_ptr = sound_list;
	while (sound_ptr != NULL) {
		if (sound_ptr->wave_ptr == wave_ptr && !create_sound_buffer(sound_ptr))
			warning("Unable to create sound buffer for wave file %s",
				sound_ptr->wave_ptr->URL);
		sound_ptr = sound_ptr->next_sound_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the sounds that depend on the given wave in the given block list.
//------------------------------------------------------------------------------

static void
update_waves_in_block_list(block *block_list, wave *wave_ptr)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->sound_list != NULL)
			update_waves_in_sound_list(block_ptr->sound_list, wave_ptr);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the sounds that depend on the given wave.
//------------------------------------------------------------------------------

void
update_wave_dependancies(wave *wave_ptr)
{	
	// Create the sound buffer for the ambient sound if it depends on
	// the given wave.

	if (ambient_sound_ptr && ambient_sound_ptr->wave_ptr == wave_ptr &&
		!create_sound_buffer(ambient_sound_ptr))
		warning("Unable to create sound buffer for wave file %s",
			ambient_sound_ptr->wave_ptr->URL);

	// Create the sound buffer for all of the sounds that depend on the given
	// wave.

	update_waves_in_sound_list(global_sound_list, wave_ptr);
	update_waves_in_block_list(movable_block_list, wave_ptr);
	update_waves_in_block_list(fixed_block_list, wave_ptr);
}

//------------------------------------------------------------------------------
// Handle the current download.
//------------------------------------------------------------------------------

void
handle_current_download(void)
{
	int download_status;

	// Check the download status.  If the current URL has not been downloaded
	// yet, then just return.  Otherwise set a flag to indicate the download
	// has completed.

	download_status = URL_downloaded();
	if (download_status < 0)
		return;
	curr_download_completed = true;

	// If the current download was for a custom texture...

	if (curr_custom_texture_ptr != NULL && (curr_custom_wave_ptr == NULL || 
		curr_custom_texture_ptr->load_index < curr_custom_wave_ptr->load_index)) {

		// If the URL was not downloaded successfully or cannot be parsed,
		// generate a warning.

		if (download_status == 0 || !load_image(curr_URL, curr_file_path, curr_custom_texture_ptr))
			warning("Unable to download custom texture from %s", curr_URL);
		
		// Otherwise update all texture dependancies.

		else
			update_texture_dependancies(curr_custom_texture_ptr);

		// Get a pointer to the next custom texture to be loaded.

		curr_custom_texture_ptr = curr_custom_texture_ptr->next_texture_ptr;
	}

	// If the current download was for a custom wave...

	else {

		// If the URL was not downloaded successfully, or could not be loaded
		// into the wave object, then generate a warning.

		if (download_status == 0 || !load_wave_file(curr_URL, curr_file_path, 
			curr_custom_wave_ptr))
			warning("Unable to download custom sound from %s", curr_URL);

		// Otherwise update the wave dependancies. 

		else
			update_wave_dependancies(curr_custom_wave_ptr);

		// Get a pointer to the next custom wave to be loaded.

		curr_custom_wave_ptr = curr_custom_wave_ptr->next_wave_ptr;
	}

	// If there are no more custom textures or waves, display the spot title in
	// the task bar, and let the user display the error log file if necessary.

	if (curr_custom_texture_ptr == NULL && curr_custom_wave_ptr == NULL) {
		set_title("%s", spot_title);
		display_error_log_file();
	}
}

//------------------------------------------------------------------------------
// Find a light of the given name in the given list.
//------------------------------------------------------------------------------

light *
find_light(light *light_list, const char *light_name)
{
	light *light_ptr = light_list;
	while (light_ptr != NULL) {
		if (!_stricmp(light_ptr->name, light_name))
			break;
		light_ptr = light_ptr->next_light_ptr;
	}
	return(light_ptr);
}

//------------------------------------------------------------------------------
// Find a sound of the given name in the given list.
//------------------------------------------------------------------------------

sound *
find_sound(sound *sound_list, const char *sound_name)
{
	sound *sound_ptr = sound_list;
	while (sound_ptr != NULL) {
		if (!_stricmp(sound_ptr->name, sound_name))
			break;
		sound_ptr = sound_ptr->next_sound_ptr;
	}
	return(sound_ptr);
}

//------------------------------------------------------------------------------
// Find a popup of the given name in the given list.
//------------------------------------------------------------------------------

popup *
find_popup(popup *popup_list, const char *popup_name)
{
	popup *popup_ptr = popup_list;
	while (popup_ptr != NULL) {
		if (!_stricmp(popup_ptr->name, popup_name))
			break;
		popup_ptr = popup_ptr->next_popup_ptr;
	}
	return(popup_ptr);
}