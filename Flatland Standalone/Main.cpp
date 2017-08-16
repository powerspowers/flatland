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
#include <time.h>
#include "Classes.h"
#include "Fileio.h"
#include "Image.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Render.h"
#include "Spans.h"
#include "Platform.h"
#include "Plugin.h"
#include "Utils.h"
#include "SimKin.h"

//------------------------------------------------------------------------------
// Global variable definitions.
//------------------------------------------------------------------------------

// Minimum blockset update period.

int min_blockset_update_period;

// Display dimensions and other related information.

float half_window_width;
float half_window_height;
float horz_field_of_view;
float vert_field_of_view;
float horz_scaling_factor;
float vert_scaling_factor;
float aspect_ratio;
float viewport_width;
float viewport_height;
float half_viewport_width;
float half_viewport_height;
float horz_pixels_per_degree;
float vert_pixels_per_degree;

// Debug level, and warnings and low memory flags.

int spot_debug_level;
bool warnings;
bool low_memory;

// Cached blockset list.

cached_blockset *cached_blockset_list;
cached_blockset *last_cached_blockset_ptr;

// URL of new spot to load.

string spot_URL_to_load;

// The XML entity list representing the current spot.

entity *spot_entity_list;

// URL directory of currently open spot, current spot URL without the
// entrance name, and a flag indicating if the spot URL is web-based.

string spot_URL_dir;
string curr_spot_URL;
bool spot_on_web;

// Minimum Rover version required for the current spot.

unsigned int min_rover_version;

// Spot title.

string spot_title;

// Visible radius, and frustum vertex list (in view space).

float visible_radius;
vertex frustum_vertex_list[FRUSTUM_VERTICES];

// Span buffer.

span_buffer *span_buffer_ptr;

// Pointer to old and current blockset list, and custom blockset.

blockset_list *old_blockset_list_ptr;
blockset_list *blockset_list_ptr;
blockset *custom_blockset_ptr;

// Metadata list.

metadata *first_metadata_ptr;
metadata *last_metadata_ptr;

// Load tag list.

load_tag *first_load_tag_ptr;
load_tag *last_load_tag_ptr;

// Symbol to block definition mapping table.  If single character symbols are
// being used, only the first 127 entries are valid.  If double character
// symbols are being used, all entries are valid.

block_def *block_symbol_table[16384];

// Pointer to world object, movable block list and fixed block list.  Both of
// these block lists may have lights, sounds, popups, entrances, exits or
// triggers that need to be handled.

world *world_ptr;
block *movable_block_list;
block *fixed_block_list;

// Global list of entrances.

entrance *global_entrance_list;
entrance *last_global_entrance_ptr;

// List of imagemaps.

imagemap *imagemap_list;

// Global list of lights.

light *global_light_list;
light *last_global_light_ptr;

// Orb direction, light, texture, brightness, size and exit.

direction orb_direction;
light *orb_light_ptr;
texture *orb_texture_ptr;
texture *custom_orb_texture_ptr;
float orb_brightness;
int orb_brightness_index;
float orb_width, orb_height;
float half_orb_width, half_orb_height;
hyperlink *orb_exit_ptr;

// Global list of sounds, and ambient sound.

sound *global_sound_list;
sound *last_global_sound_ptr;
sound *ambient_sound_ptr;

// Global list of popups.

popup *global_popup_list;
popup *last_global_popup_ptr;

// Global list of triggers.

trigger *global_trigger_list;
trigger *last_global_trigger_ptr;

// Placeholder texture.

texture *placeholder_texture_ptr;

// Sky definition.

texture *sky_texture_ptr;
texture *custom_sky_texture_ptr;
RGBcolour unlit_sky_colour, sky_colour;
pixel sky_colour_pixel;
float sky_brightness;
int sky_brightness_index;

// Ground block definition.

block_def *ground_block_def_ptr;

// Player block symbol and pointer, camera offset, and viewpoint.

word player_block_symbol;
block *player_block_ptr;
vector player_camera_offset;
viewpoint player_viewpoint, last_player_viewpoint;

// Player last turn delta.

float last_delta;

// Player movement flags.

bool enable_player_translation;
bool enable_player_rotation;

// Player's dimensions, collision box and step height.

vertex player_dimensions;
COL_AABOX player_collision_box;
float player_step_height;

// Start, current time, clock for global timer and number of frames rendered.

int start_time_ms, curr_time_ms, clocktimer_time_ms;
int frames_rendered;

// Current mouse position.

int mouse_x;
int mouse_y;

#ifdef _DEBUG

// Number of currently selected polygon, and pointer to the block definition
// it belongs to.

int curr_selected_polygon_no;
block_def *curr_selected_block_def_ptr;

#endif

// Previously and currently selected block (and square if the block is fixed).

block *prev_selected_block_ptr;
block *curr_selected_block_ptr;
square *prev_selected_square_ptr;
square *curr_selected_square_ptr;

// Previously and currently selected area and it's block and/or square.

area *prev_selected_area_ptr;
area *curr_selected_area_ptr;
block *prev_area_block_ptr;
block *curr_area_block_ptr;
square *prev_area_square_ptr;
square *curr_area_square_ptr;

// Previously and currently selected parts.

part *prev_selected_part_ptr;
part *curr_selected_part_ptr;

// Current popup block and square.

block *curr_popup_block_ptr;
square *curr_popup_square_ptr;

// Previously and currently selected exit.

hyperlink *prev_selected_exit_ptr;
hyperlink *curr_selected_exit_ptr;

// Current location that player is in, and flag indicating if the block the
// player is standing on was replaced.

int player_column, player_row, player_level;
bool player_block_replaced;

// Current URL and file path.

string curr_URL;
string curr_file_path;

// Flag indicating if the viewpoint has changed.

bool viewpoint_has_changed;

// Current custom texture and wave in the download pipeline, and a flag 
// indicating whether the current download has completed.

texture *curr_custom_texture_ptr;
wave *curr_custom_wave_ptr;
bool curr_download_completed;

// Flag indicating if the error log file has been displayed (this happens once
// all textures and waves have been downloaded, but not if additional textures
// and waves are downloaded later on).

bool displayed_error_log_file;

#ifdef STREAMING_MEDIA

// Stream flag, name, and URLs for RealPlayer and Windows Media Player.

bool stream_set;
string name_of_stream;
string rp_stream_URL;
string wmp_stream_URL;

// Pointer to unscaled video texture (for popups) and scaled video texture
// list.

texture *unscaled_video_texture_ptr;
video_texture *scaled_video_texture_list;

#endif

// List of active triggers.

trigger *active_trigger_list[512];
int active_trigger_count;

// Queue of active scripts.

trigger *active_script_list[256];
int active_script_count;
bool script_executing;

// List of action tied to the global clock.

action *active_clock_action_list[256];
int active_clock_action_count;

// Frustum normal vector list, frustum plane offset list, 

vector frustum_normal_vector_list[FRUSTUM_PLANES];
float frustum_plane_offset_list[FRUSTUM_PLANES];

// Global fog data.

bool global_fog_enabled;
fog global_fog;

// Square to be manipulated by build mode.

square *builder_square_ptr;

//------------------------------------------------------------------------------
// Local variable definitions.
//------------------------------------------------------------------------------

// Time that player window was last refreshed.

static int last_refresh_time_ms;
static unsigned int refresh_count;

// Full spot URL, and the current entrance name.

static string full_spot_URL;
static string curr_spot_entrance;

// Current link URL.

static const char *curr_link_URL;

// Collision data.

static float player_fall_delta;
static COL_MESH *col_mesh_list[512];
static VEC3 mesh_pos_list[512];
static int col_meshes;

// Flag indicating if player viewpoint has been set.

static bool player_viewpoint_set;

// Flag indicating if a left and right mouse clicked event has been recieved.

static bool left_mouse_was_clicked;
static bool right_mouse_was_clicked;

// Flag indicating if the player was teleported in last frame.

static bool player_was_teleported;

// A vector pointing up.

static vector world_y_axis(0.0, 1.0, 0.0);

// Current state of all keys, and the current key event being processed.

static bool key_down_list[256];
static key_event curr_key_event;

//==============================================================================
// Miscellaneous global functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to adjust an angle so that it is between -180 and 179, or 0 and 359.
//------------------------------------------------------------------------------

float
neg_adjust_angle(float angle)
{
	float degrees = ((angle / 360.0f) - FINT(angle / 360.0f)) * 360.0f;
	if (degrees <= -179.0f)
		degrees += 360.0f;
	else if (degrees > 180.0f)
		degrees -= 360.0f;
	return(degrees);
}

float
pos_adjust_angle(float angle)
{
	float degrees = ((angle / 360.0f) - FINT(angle / 360.0f)) * 360.0f;
	if (degrees < 0.0)
		degrees += 360.0f;
	return(degrees);

}

//------------------------------------------------------------------------------
// Function to clamp an angle to the minimum or maximum allowed range.
//------------------------------------------------------------------------------

float
clamp_angle(float angle, float min_angle, float max_angle)
{
	if (FLT(angle, min_angle))
		angle = min_angle;
	else if (FGT(angle, max_angle))
		angle = max_angle;
	return(angle);
}

//==============================================================================
// Main support functions.
//==============================================================================

//------------------------------------------------------------------------------
// Display a dialog box containing a low memory error message.
//------------------------------------------------------------------------------

static void
display_low_memory_error(void)
{
	fatal_error("Insufficient memory", "There is insufficient memory to "
		"run Flatland.  Try closing some applications first, then "
		"click on the RELOAD or REFRESH button on your browser to restart "
		"Flatland");
}

//------------------------------------------------------------------------------
// Write some rendering statistics to the log file.
//------------------------------------------------------------------------------

static void
log_stats(void)
{
	int end_time_ms;
	float elapsed_time;

	// Get the end time in milliseconds and seconds.

	end_time_ms = get_time_ms();

	// Compute the number of seconds that have elapsed, and divide the number
	// of frames rendered by the elapsed time to obtain the average frame rate.

	if (frames_rendered > 0) {
		elapsed_time = (float)(end_time_ms - start_time_ms) / 1000.0f;
		diagnose("Average frame rate was %f frames per second",
			(float)frames_rendered / elapsed_time);
	}
}

//------------------------------------------------------------------------------
// Update a light source that might be changing over time.
//------------------------------------------------------------------------------

static void
update_light(light *light_ptr, float elapsed_time)
{
	direction new_dir, *min_dir_ptr, *max_dir_ptr;
	float new_intensity;

	// Check whether this light is of a type that changes over time.

	switch (light_ptr->style) {
	case PULSATING_POINT_LIGHT:

		// Add the delta intensity, scaled by the elapsed time.

		new_intensity = light_ptr->intensity + 
			light_ptr->delta_intensity * elapsed_time;

		// If the intensity has exceeded the minimum or maximum intensity,
		// clamp it, then change the sign of the delta intensity.

		if (FGT(light_ptr->delta_intensity, 0.0)) {
			if (FGT(new_intensity, 
				light_ptr->intensity_range.max_percentage)) {
				new_intensity = light_ptr->intensity_range.max_percentage;
				light_ptr->delta_intensity = -light_ptr->delta_intensity;
			}
		} else {
			if (FLT(new_intensity,
				light_ptr->intensity_range.min_percentage)) {
				new_intensity = light_ptr->intensity_range.min_percentage;
				light_ptr->delta_intensity = -light_ptr->delta_intensity;
			}
		}

		// Set the new intensity.

		light_ptr->set_intensity(new_intensity);
		break;

	case REVOLVING_SPOT_LIGHT:

		// Add the delta direction, scaled by the elapsed time.  Don't allow
		// the delta to exceed 45 degrees around either axis.

		new_dir.angle_x = pos_adjust_angle(light_ptr->curr_dir.angle_x +
			clamp_angle(light_ptr->delta_dir.angle_x * elapsed_time,
			-45.0f, 45.0f));
		new_dir.angle_y = pos_adjust_angle(light_ptr->curr_dir.angle_y +
			clamp_angle(light_ptr->delta_dir.angle_y * elapsed_time,
			-45.0f, 45.0f));

		// Set the new direction vector.

		light_ptr->set_direction(new_dir);
		break;

	case SEARCHING_SPOT_LIGHT:

		// Add the delta direction, scaled by the elapsed time.

		new_dir.angle_x = light_ptr->curr_dir.angle_x +
			light_ptr->delta_dir.angle_x * elapsed_time;
		new_dir.angle_y = light_ptr->curr_dir.angle_y +
			light_ptr->delta_dir.angle_y * elapsed_time;

		// Get pointers to the minimum and maximum directions.

		min_dir_ptr = &light_ptr->dir_range.min_direction;
		max_dir_ptr = &light_ptr->dir_range.max_direction;

		// If the new direction has passed beyond the minimum or maximum
		// range around the X or Y axis, clamp it, then change the direction
		// around that axis.

		if (FGT(light_ptr->delta_dir.angle_x, 0.0f)) {
			if (FGT(new_dir.angle_x, max_dir_ptr->angle_x)) {
				new_dir.angle_x = max_dir_ptr->angle_x;
				light_ptr->delta_dir.angle_x = -light_ptr->delta_dir.angle_x;
			}
		} else {
			if (FLT(new_dir.angle_x, min_dir_ptr->angle_x)) {
				new_dir.angle_x = min_dir_ptr->angle_x;
				light_ptr->delta_dir.angle_x = -light_ptr->delta_dir.angle_x;
			}
		}
		if (FGT(light_ptr->delta_dir.angle_y, 0.0f)) {
			if (FGT(new_dir.angle_y, max_dir_ptr->angle_y)) {
				new_dir.angle_y = max_dir_ptr->angle_y;
				light_ptr->delta_dir.angle_y = -light_ptr->delta_dir.angle_y;
			}
		} else {
			if (FLT(new_dir.angle_y, min_dir_ptr->angle_y)) {
				new_dir.angle_y = min_dir_ptr->angle_y;
				light_ptr->delta_dir.angle_y = -light_ptr->delta_dir.angle_y;
			}
		}

		// Set the new direction vector.

		light_ptr->set_direction(new_dir);
	}
}

//------------------------------------------------------------------------------
// Update the lights in the given list.
//------------------------------------------------------------------------------

static void
update_lights_in_light_list(light *light_list, float elapsed_time)
{
	light *light_ptr = light_list;
	while (light_ptr != NULL) {
		update_light(light_ptr, elapsed_time);
		light_ptr = light_ptr->next_light_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the lights in the given block list.
//------------------------------------------------------------------------------

static void
update_lights_in_block_list(block *block_list, float elapsed_time)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->light_list != NULL)
			update_lights_in_light_list(block_ptr->light_list, elapsed_time); 
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Update all lights
//------------------------------------------------------------------------------

static void
update_all_lights(float elapsed_time)
{
	// Update the lights in the global list, and any lights in the movable and
	// fixed block lists.

	update_lights_in_light_list(global_light_list, elapsed_time);
	update_lights_in_block_list(movable_block_list, elapsed_time);
	update_lights_in_block_list(fixed_block_list, elapsed_time);
}

//------------------------------------------------------------------------------
// Update the sounds in the given list.
//------------------------------------------------------------------------------

static void
update_sounds_in_sound_list(sound *sound_list, vertex *translation_ptr)
{
	sound *sound_ptr = sound_list;
	while (sound_ptr != NULL) {
		update_sound(sound_ptr, translation_ptr);
		sound_ptr = sound_ptr->next_sound_ptr;
	}
}

//------------------------------------------------------------------------------
// Update the sounds in the given block list.
//------------------------------------------------------------------------------

static void
update_sounds_in_block_list(block *block_list)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->sound_list != NULL)
			update_sounds_in_sound_list(block_ptr->sound_list, &block_ptr->translation);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Update all sounds.
//------------------------------------------------------------------------------

static void
update_all_sounds(void)
{
	// Update the sounds in the global list, and any sounds in the movable and
	// fixed block lists.

	update_sounds_in_sound_list(global_sound_list, NULL);
	update_sounds_in_block_list(movable_block_list);
	update_sounds_in_block_list(fixed_block_list);

	// Update ambient sound.

	if (ambient_sound_ptr != NULL)
		update_sound(ambient_sound_ptr, NULL);
}

//------------------------------------------------------------------------------
// Stop the sounds in the given list.
//------------------------------------------------------------------------------

void
stop_sounds_in_sound_list(sound *sound_list)
{
	sound *sound_ptr = sound_list;
	while (sound_ptr != NULL) {
		stop_sound(sound_ptr);
		sound_ptr->in_range = false;
		sound_ptr = sound_ptr->next_sound_ptr;
	}
}

//------------------------------------------------------------------------------
// Stop the sounds in the given block list.
//------------------------------------------------------------------------------

static void
stop_sounds_in_block_list(block *block_list)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->sound_list != NULL)
			stop_sounds_in_sound_list(block_ptr->sound_list);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Stop all sounds.
//------------------------------------------------------------------------------

static void
stop_all_sounds(void)
{
	// Stop all sounds in global sound list, and any sounds in the movable and
	// fixed block lists.

	stop_sounds_in_sound_list(global_sound_list);
	stop_sounds_in_block_list(movable_block_list);
	stop_sounds_in_block_list(fixed_block_list);

	// Stop ambient sound.

	if (ambient_sound_ptr != NULL) {
		stop_sound(ambient_sound_ptr);
		ambient_sound_ptr->in_range = false;
	}
}

//------------------------------------------------------------------------------
// Teleport to the named entrance location.
//------------------------------------------------------------------------------

static void
teleport(const char *entrance_name)
{
	entrance *entrance_ptr;
	square *square_ptr;
	int column, row, level;
	block *block_ptr;

	// Search for the entrance with the specified name; if it doesn't exist,
	// use the default entrance.  If that doesn't exist, put the player on
	// square (0, 0, max_level -1), turned 135 degrees.  They'll simply fall
	// to the first occupied block, or the bottom of the map.

	if ((entrance_ptr = find_entrance(entrance_name)) != NULL ||
		(entrance_ptr = find_entrance("default")) != NULL) {
		square_ptr = entrance_ptr->square_ptr;
		block_ptr = entrance_ptr->block_ptr;
		player_viewpoint.turn_angle = entrance_ptr->initial_direction.angle_y;
		player_viewpoint.look_angle = -entrance_ptr->initial_direction.angle_x;
	} else {
		square_ptr = world_ptr->get_square_ptr(0, 0, world_ptr->levels - 1);
		block_ptr = NULL;
		player_viewpoint.turn_angle = 135.0f;
		player_viewpoint.look_angle = 0.0f;
	}

	// If this entrance does not belong to a square (meaning it belongs to a
	// movable block), then use the closest square to the movable block as
	// the entrance.  Note that if the entrance does belong to a square, a
	// pointer to the block on the square needs to be obtained, if present.

	if (square_ptr != NULL) {
		world_ptr->get_square_location(square_ptr, &column, &row, &level);
		block_ptr = world_ptr->get_block_ptr(column, row, level);
	} else
		block_ptr->translation.get_map_position(&column, &row, &level);

	// Set the player position, and initialize the player block, if defined.

	player_viewpoint.position.set_map_position(column, row, level);
	if (player_block_ptr != NULL) {
		player_block_ptr->set_frame(0);
		player_block_ptr->rotate_y(player_viewpoint.turn_angle);
	}

	// If the player has landed in a square occupied by a block that has a
	// polygonal model, put the player on top of the block's bounding box.
	// Otherwise put them at the bottom of the block.

	if (block_ptr != NULL && block_ptr->polygons > 0 && block_ptr->solid && 
		block_ptr->col_mesh_ptr != NULL)
		player_viewpoint.position.y = block_ptr->translation.y +
			block_ptr->col_mesh_ptr->maxBox.y;
	 else
		player_viewpoint.position.y -= UNITS_PER_HALF_BLOCK;

	// Set the last position to this one so that the user starts fresh from this location.

	player_viewpoint.last_position = player_viewpoint.position;

	// Set a flag indicating that a teleport took place.

	player_was_teleported = true;
}

//------------------------------------------------------------------------------
// Start up the current spot.
//------------------------------------------------------------------------------

static bool
start_up_spot(void)
{
	string spot_file_name;
	trigger *trigger_ptr;

	// Hide any label that might be shown.

	hide_label();

	// Clear all selection states.

	selection_active.set(false);
	left_mouse_clicked.reset_event();
	right_mouse_clicked.reset_event();
	curr_link_URL = NULL;
	curr_selected_block_ptr = NULL;
	curr_selected_exit_ptr = NULL;
	curr_selected_area_ptr = NULL;
	curr_area_block_ptr = NULL;
	curr_area_square_ptr = NULL;
	curr_popup_block_ptr = NULL;
	curr_popup_square_ptr = NULL;

	// Clear player-related data.

	enable_player_translation = true;
	enable_player_rotation = true;
	player_viewpoint_set = false;
	player_was_teleported = false;
	player_viewpoint.last_position.set(0.0, 0.0, 0.0);

	// Initialise triggers and scripts.

	init_free_trigger_list();
	global_trigger_list = NULL;
	last_global_trigger_ptr = NULL;
	active_trigger_list[0] = NULL;
	active_trigger_count = 0;
	active_script_list[0] = NULL;
	active_script_count = 0;
	script_executing = false;
	active_clock_action_list[0] = NULL;
	active_clock_action_count = 0;

	// Initialise various global lists.

	global_entrance_list = NULL;
	last_global_entrance_ptr = NULL;
	imagemap_list = NULL;
	global_light_list = NULL;
	last_global_light_ptr = NULL;
	global_sound_list = NULL;
	last_global_sound_ptr = NULL;
	global_popup_list = NULL;
	last_global_popup_ptr = NULL;
	movable_block_list = NULL;
	fixed_block_list = NULL;
	first_metadata_ptr = NULL;
	last_metadata_ptr = NULL;
	first_load_tag_ptr = NULL;
	last_load_tag_ptr = NULL;

	// Initialise various global objects.

	placeholder_texture_ptr = NULL;
	ambient_sound_ptr = NULL;
	orb_texture_ptr = NULL;
	orb_light_ptr = NULL;
	ground_block_def_ptr = NULL;
	player_block_ptr = NULL;

	// Create custom blockset and the world object.

	NEW(custom_blockset_ptr, blockset);
	NEW(world_ptr, world);
	if (custom_blockset_ptr == NULL || world_ptr == NULL) {
		display_low_memory_error();
		return(false);
	}

	// Clear the block symbol table.

	for (int index = 0; index < 16384; index++)
		block_symbol_table[index] = NULL;

	// Rest the spot debug level, warnings and low memory flags.

	spot_debug_level = BE_SILENT;
	warnings = false;
	low_memory = false;

	// Start up SimKin.

	if (!start_up_simkin())
		return(false);

	// Set loading message in the title.

	set_title("Loading %s", spot_file_name);

	// Split the current URL into it's components, after saving it as the
	// full spot URL. Then recreate the spot URL without the entrance name.

	full_spot_URL = curr_URL;
	split_URL(full_spot_URL, &spot_URL_dir, &spot_file_name, 
		&curr_spot_entrance);
	curr_spot_URL = create_URL(spot_URL_dir, spot_file_name);

	// Parse the spot, then remove the spot file if we are being hosted by the
	// Rover ActiveX control (since it is only a temporary file).  This is done
	// in a try block because any errors will be  thrown.

	try {
		parse_spot_file(curr_spot_URL, curr_file_path);
	}
	catch (char *message) {

		// Pop all files and write the message to the error log.

		pop_all_files();
		write_error_log(message);

		// If we got as far as producing an entity list, destroy it.

		if (spot_entity_list) {
			destroy_entity_list(spot_entity_list);
			spot_entity_list = NULL;
		}

		// If memory was low, display a low memory error.  Otherwise display
		// the error.

		if (low_memory)
			display_low_memory_error();
		else {
			show_error_log = (user_debug_level.get() >= SHOW_ERRORS_ONLY ||
				(user_debug_level.get() == LET_SPOT_DECIDE && 
				spot_debug_level >= SHOW_ERRORS_ONLY));
			display_error.send_event(true);
		}
		return(false);
	}

	// If the minimum version required for this spot is greater than Rover's version number, inform the user.

	if (min_rover_version > ROVER_VERSION_NUMBER) {
		information("Newer version of Flatland required", 
			"This spot makes use of features that are only present in\nversion %s or later of Flatland (you have version %s).", 
			(char *)version_number_to_string(min_rover_version), (char *)version_number_to_string(ROVER_VERSION_NUMBER));
	}

	// Initialise all triggers in the global trigger list.

	trigger_ptr = global_trigger_list;
	while (trigger_ptr != NULL) {
		init_global_trigger(trigger_ptr);
		trigger_ptr = trigger_ptr->next_trigger_ptr;
	}

	// Initialise some miscellaneous variables.

	start_time_ms = get_time_ms();
	curr_time_ms = start_time_ms;
	clocktimer_time_ms = 0;
	frames_rendered = 0;
	player_viewpoint_set = true;
	player_fall_delta = 0.0f;

	// Initiate the downloading of the first custom texture or wave.

	displayed_error_log_file = false;
	initiate_first_download();

	// If hardware acceleration is enabled, then update the fog settings for the first time.

	if (hardware_acceleration) {
		hardware_update_fog_settings(global_fog_enabled, &global_fog, visible_radius);
	}

#if STREAMING_MEDIA

	// If a stream is set, start the streaming thread.

	if (stream_set)
		start_streaming_thread();

#endif

	// Teleport to the selected entrance.

	teleport(curr_spot_entrance);

	// Call the "start" method of the global SimKin script, if there is one,
	// and wait for it to complete.

	script_executing = call_global_method("start");
	while (script_executing)
		script_executing = resume_script();

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Shut down spot.
//------------------------------------------------------------------------------

static void
shut_down_spot(void)
{
	block_def *block_def_ptr;
	block* next_block_ptr;
	imagemap *next_imagemap_ptr;
	light *next_light_ptr;
	sound *next_sound_ptr;
	popup *next_popup_ptr;
	trigger *next_trigger_ptr;
	entrance *next_entrance_ptr;

	// Log stats.

	log_stats();

	// Call the "stop" method of the global SimKin script, if there is
	// one, then shut down SimKin.  We must terminate any script currently
	// executing before we can call the "stop" method, then we wait for it to 
	// complete.

	if (script_executing)
		terminate_script();
	script_executing = call_global_method("stop");
	while (script_executing)
		script_executing = resume_script();
	shut_down_simkin();

#ifdef STREAMING_MEDIA

	// If a stream is set, stop the streaming thread.

	if (stream_set)
		stop_streaming_thread();

	// Delete the unscaled video texture, and the scaled video texture
	// list, if they exist.

	if (unscaled_video_texture_ptr != NULL)
		DEL(unscaled_video_texture_ptr, texture);
	while (scaled_video_texture_list != NULL) {
		video_texture *next_video_texture_ptr =
			scaled_video_texture_list->next_video_texture_ptr;
		DEL(scaled_video_texture_list, video_texture);
		scaled_video_texture_list = next_video_texture_ptr;
	}

#endif

	// Delete the metadata list.

	while (first_metadata_ptr != NULL) {
		metadata *next_metadata_ptr = first_metadata_ptr->next_metadata_ptr;
		DEL(first_metadata_ptr, metadata);
		first_metadata_ptr = next_metadata_ptr;
	}

	// Delete the log tag list.

	while (first_load_tag_ptr != NULL) {
		load_tag *next_load_tag_ptr = first_load_tag_ptr->next_load_tag_ptr;
		DEL(first_load_tag_ptr, load_tag);
		first_load_tag_ptr = next_load_tag_ptr;
	}

	// Delete map.

	if (world_ptr != NULL)
		DEL(world_ptr, world);

	// Delete the movable block list.

	while (movable_block_list != NULL) {
		next_block_ptr = movable_block_list;
		while (movable_block_list != NULL) {
			next_block_ptr = movable_block_list->next_block_ptr;
			DEL(movable_block_list,block);
			movable_block_list = next_block_ptr;
		}
	}

	// Delete all entrances in the global entrance list.

	while (global_entrance_list != NULL) {
		next_entrance_ptr = global_entrance_list->next_entrance_ptr;
		DEL(global_entrance_list, entrance);
		global_entrance_list = next_entrance_ptr;
	}

	// Delete all imagemaps.

	while (imagemap_list != NULL) {
		next_imagemap_ptr = imagemap_list->next_imagemap_ptr;
		DEL(imagemap_list, imagemap);
		imagemap_list = next_imagemap_ptr;
	}

	// Delete all lights in the global light list.

	while (global_light_list != NULL) {
		next_light_ptr = global_light_list->next_light_ptr;
		DEL(global_light_list, light);
		global_light_list = next_light_ptr;
	}

	// Delete the orb light.

	if (orb_light_ptr != NULL)
		DEL(orb_light_ptr, light);

	// Stop all sounds.

	stop_all_sounds();

	// Delete all sounds in the global sound list.

	while (global_sound_list != NULL) {
		next_sound_ptr = global_sound_list->next_sound_ptr;
		DEL(global_sound_list, sound);
		global_sound_list = next_sound_ptr;
	}

	// Delete ambient sound.

	if (ambient_sound_ptr != NULL)
		DEL(ambient_sound_ptr, sound);

	// Delete all popups in the global popup list, as well as their foreground
	// textures.

	while (global_popup_list != NULL) {
		next_popup_ptr = global_popup_list->next_popup_ptr;
		if (global_popup_list->fg_texture_ptr != NULL)
			DEL(global_popup_list->fg_texture_ptr, texture);
		DEL(global_popup_list, popup);
		global_popup_list = next_popup_ptr;
	}

	// Delete all global triggers.

	while (global_trigger_list != NULL) {
		next_trigger_ptr = global_trigger_list->next_trigger_ptr;
		DEL(global_trigger_list, trigger);
		global_trigger_list = next_trigger_ptr;
	}

	// Deleted triggers queued in the active script queue.

	active_trigger_list[0] = NULL;
	active_trigger_count = 0;
	active_script_list[0] = NULL;
	active_script_count = 0;

	// Delete the free lists of triggers.

	delete_free_trigger_list();

	// Clear the actions attached to the global clock
	active_clock_action_list[0] = NULL;
	active_clock_action_count = 0;

	// Delete player block.

	if (player_block_ptr != NULL) {
		block_def_ptr = player_block_ptr->block_def_ptr;
		block_def_ptr->del_block(player_block_ptr);
	}

	// Delete the custom blockset and the ground block definition.

	if (custom_blockset_ptr != NULL)
		DEL(custom_blockset_ptr, blockset);
	if (ground_block_def_ptr != NULL)
		DEL(ground_block_def_ptr, block_def);

	// Destroy the spot entity list, if it exists.

	if (spot_entity_list) {
		destroy_entity_list(spot_entity_list);
		spot_entity_list = NULL;
	}

	// Reset the movement delta so character is standing still.

	curr_turn_delta.set(0.0f);
	curr_move_delta.set(0.0f);
	curr_side_delta.set(0.0f);
	curr_look_delta.set(0.0f);

	// Clean up the renderer data structures.

	clean_up_renderer();
}

//------------------------------------------------------------------------------
// Handle the activation of an exit.
//------------------------------------------------------------------------------

static bool
handle_exit(const char *exit_URL, const char *exit_target, bool is_spot_URL, bool force_reload)
{
	// If the exit URL starts with the hash sign, then treat this as an
	// entrance name within the current spot.

	if (*(char *)exit_URL == '#') {
		string URL = curr_spot_URL + exit_URL;
		const char *entrance_name = (char *)exit_URL + 1;
		teleport(entrance_name);
	}

	// Otherwise assume we have a new spot or other URL to download...

	else {
		string URL;
		string URL_dir;
		string file_name;
		string entrance_name;
		char *ext_ptr;

		// Create the URL by prepending the spot file directory if the
		// exit reference is a relative path.  Then split the URL into
		// it's components.

		URL = create_URL(spot_URL_dir, exit_URL);
		split_URL(URL, &URL_dir, &file_name, &entrance_name);

		// If this is a spot URL, or the file name ends with ".3dml"...

		ext_ptr = strrchr(file_name, '.');
		if (is_spot_URL || (ext_ptr != NULL && !_stricmp(ext_ptr, ".3dml"))) {
			string spot_URL;

			// If there is a custom texture or wave currently being downloaded,
			// wait for it to be opened if it hasn't already, then cancel the
			// download and wait for the cancellation to take effect.

			if (!curr_download_completed) {
				if (!curr_URL_opened)
					URL_was_opened.wait_for_event();
				URL_cancel_requested.send_event(true);
				URL_was_downloaded.wait_for_event();
				curr_download_completed = true;
			}

			// Recreate the spot URL without the entrance name.  If a spot is
			// loaded, the new spot URL is the same as the current spot URL,
			// and we're not been asked to force a reload, then teleport to the
			// specified entrance.

			spot_URL = create_URL(URL_dir, file_name);
			if (spot_loaded.get() && !_stricmp(spot_URL, curr_spot_URL) && !force_reload) {
				teleport(entrance_name);
				return(true);
			}

			// Download the new 3DML file.  If the download failed, reset the
			// left mouse clicked event in case a link was selected while the URL
			// was downloading, then return with a success status to allow the
			// display of the current spot to continue.

			if (!download_URL(spot_URL, NULL, true)) {
				fatal_error("Unable to download 3DML document", 
					"Unable to download 3DML document from %s", spot_URL);
				left_mouse_clicked.reset_event();
				return(true);
			}

			// Append the entrance name to the current URL.

			curr_URL += "#";
			curr_URL += entrance_name;

			// Set a flag indicating if the spot is on the web or not.

			spot_on_web = !_strnicmp(curr_URL, "http://", 7);
			
			// Delete the image caches, and if a spot was loaded shut it down
			// before recreating the image caches.

			delete_image_caches();
			if (spot_loaded.get()) {
				shut_down_spot();
				spot_loaded.set(false);
				set_title("No spot loaded");
			}
			if (!create_image_caches())
				return(false);

			// Create the image caches and start up the new spot.

			if (!start_up_spot())
				return(false);
			spot_loaded.set(true);
		}

		// Otherwise have the browser request that the URL be downloaded into
		// the target window, or a new window if there is no target supplied.  

		else {
			if (exit_target != NULL && strlen(exit_target) != 0)
				request_URL(URL, NULL, exit_target, true);
			else
				request_URL(URL, NULL, "_blank", true);
		}
	}

	// Signal success.

	return(true);
}

//------------------------------------------------------------------------------
// Show the first trigger label in the given trigger list.
//------------------------------------------------------------------------------

static bool
show_trigger_label(trigger *trigger_list)
{
	trigger *trigger_ptr = trigger_list;
	bool label_shown = false;
	while (trigger_ptr != NULL) {
		if ((trigger_ptr->trigger_flag == CLICK_ON) &&
			(trigger_ptr->part_index == ALL_PARTS || trigger_ptr->part_index == curr_selected_part_ptr->number)) {
			selection_active.set(true);
			if (strlen(trigger_ptr->label) != 0) {
					show_label(trigger_ptr->label);
					label_shown = true;
			} 
			return(label_shown);
		}
		trigger_ptr = trigger_ptr->next_trigger_ptr;
	}
	return(label_shown);
}

//------------------------------------------------------------------------------
// Check to see whether the mouse is pointing at a block, popup area or exit
// with a "click on" action.
//------------------------------------------------------------------------------

static void
check_for_mouse_selection(void)
{
	bool label_shown;

	// If the currently selected part is different to the previously selected part, update the selection active flag and
	// show or hide the label, if necessary.

	if (curr_selected_part_ptr != prev_selected_part_ptr) {
		if (curr_selected_part_ptr == NULL) {
		    selection_active.set(false);
			hide_label();
		} else if (curr_selected_block_ptr->trigger_flags & CLICK_ON || curr_selected_block_ptr->trigger_flags & ROLL_ON) {
			selection_active.set(false);
			if (!show_trigger_label(curr_selected_block_ptr->trigger_list)) {
				hide_label();
			}
            return;
		} else {
			selection_active.set(false);
			hide_label();
		}
	} 

	// If the currently selected area or exit is different to the
	// previously selected block, area or exit...
    
	if (curr_selected_area_ptr != prev_selected_area_ptr ||
		curr_selected_exit_ptr != prev_selected_exit_ptr) {

		// If there is a currently selected area or exit with a
		// "click on" action...

		label_shown = false;
		if	((curr_selected_square_ptr != NULL &&
			 (curr_selected_square_ptr->trigger_flags & CLICK_ON)) ||
			(curr_selected_area_ptr != NULL && 
			 (curr_selected_area_ptr->trigger_flags & CLICK_ON)) ||
			(curr_selected_exit_ptr != NULL &&
			 (curr_selected_exit_ptr->trigger_flags & CLICK_ON))) {

			// Indicate that there is an active selection.

			selection_active.set(true);

			// If there is a currently selected exit and it has a label,
			// show it.

			if (curr_selected_exit_ptr != NULL && 
				strlen(curr_selected_exit_ptr->label) != 0) {
				show_label(curr_selected_exit_ptr->label);
				label_shown = true;
			}

			// Otherwise if there is a currently selected square and one of it's
			// triggers has a label, show it.

			else if (curr_selected_square_ptr != NULL && show_trigger_label(
				curr_selected_square_ptr->trigger_list))
				label_shown = true;

			// Otherwise if there is a currently selected area and one of it's
			// triggers has a label, show it.

			else if (curr_selected_area_ptr != NULL && show_trigger_label(
				curr_selected_area_ptr->trigger_list))
				label_shown = true;
		}

		// If there is nothing selected, indicate there is no active selection.

		else
			selection_active.set(false);

		// If no label was shown, hide any labels that might already be shown.

		if (!label_shown)
			hide_label();
	}
}

//------------------------------------------------------------------------------
// Create a list of blocks that overlap the bounding box of a new player
// position.
//------------------------------------------------------------------------------

static void
get_overlapping_blocks(float x, float y, float z, 
					   float old_x, float old_y, float old_z)
{
	vertex min_view, max_view;
	int min_level, min_row, min_column;
	int max_level, max_row, max_column;
	int level, row, column;
	block *block_ptr;
	COL_MESH *col_mesh_ptr;
	vertex min_bbox, max_bbox;

	// Compute a bounding box for the new view, and determine which blocks are
	// at the corners of this bounding box.  Note that min_row and max_row are
	// swapped because as the Z coordinate increases the row number decreases.

	min_view.x = FMIN(x - player_collision_box.maxDim.x,
		old_x - player_collision_box.maxDim.x);
	min_view.y = FMIN(y, old_y);
	min_view.z = FMIN(z - player_collision_box.maxDim.z,
		old_z - player_collision_box.maxDim.z);
	max_view.x = FMAX(x + player_collision_box.maxDim.x,
		old_x + player_collision_box.maxDim.x);
	max_view.y = FMAX(y + player_dimensions.y, old_y + player_dimensions.y);
	max_view.z = FMAX(z + player_collision_box.maxDim.z,
		old_z + player_collision_box.maxDim.z);
	min_view.get_map_position(&min_column, &max_row, &min_level);
	max_view.get_map_position(&max_column, &min_row, &max_level);

	// Include the level below the player collision box, if there is one.  This
	// ensures the floor height can be determined.

	if (min_level > 0)
		min_level--;

	// Step through the range of overlapping blocks and add them to a list of
	// blocks to check for collisions.

	col_meshes = 0;
	for (level = min_level; level <= max_level; level++)
		for (row = min_row; row <= max_row; row++)
			for (column = min_column; column <= max_column; column++)
				if ((block_ptr = world_ptr->get_block_ptr(column, row, level))
					!= NULL) {
					if (block_ptr->solid && block_ptr->col_mesh_ptr != NULL) {
						col_mesh_list[col_meshes] = block_ptr->col_mesh_ptr;
						mesh_pos_list[col_meshes].x = block_ptr->translation.x;
						mesh_pos_list[col_meshes].y = block_ptr->translation.y;
						mesh_pos_list[col_meshes].z = block_ptr->translation.z;
						col_meshes++;
					}
				}

	// Step through the list of movable blocks, and add them to the list of
	// blocks to check for collisions.

	block_ptr = movable_block_list;
	while (block_ptr != NULL) {
		if (block_ptr->solid && block_ptr->col_mesh_ptr != NULL) {
			col_mesh_ptr = block_ptr->col_mesh_ptr;
			min_bbox.x = col_mesh_ptr->minBox.x + block_ptr->translation.x;
			min_bbox.y = col_mesh_ptr->minBox.y + block_ptr->translation.y;
			min_bbox.z = col_mesh_ptr->minBox.z + block_ptr->translation.z;
			max_bbox.x = col_mesh_ptr->maxBox.x + block_ptr->translation.x;
			max_bbox.y = col_mesh_ptr->maxBox.y + block_ptr->translation.y;
			max_bbox.z = col_mesh_ptr->maxBox.z + block_ptr->translation.z;
			if (!(min_bbox.x > max_view.x || max_bbox.x < min_view.x ||
				  min_bbox.y > max_view.y || max_bbox.y < min_view.y ||
				  min_bbox.z > max_view.z || max_bbox.z < min_view.z)) {
				col_mesh_list[col_meshes] = block_ptr->col_mesh_ptr;
				mesh_pos_list[col_meshes].x = block_ptr->translation.x;
				mesh_pos_list[col_meshes].y = block_ptr->translation.y;
				mesh_pos_list[col_meshes].z = block_ptr->translation.z;
				col_meshes++;
			}
		}
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Adjust the player's trajectory to take into consideration collisions with
// polygons.
//------------------------------------------------------------------------------

static vector
adjust_trajectory(vector &trajectory, float elapsed_time)
{
	vector new_trajectory;
	vertex old_position, new_position;
	float floor_y;
	float jump_delta;
	int column, row, level;

	// Truncate the trajectory so that it does not exceed the player step height,
	// if necessary, then subtract the possibly truncated trajectory from the original.

	new_trajectory = trajectory;
	new_trajectory.truncate(player_step_height);
	trajectory -= new_trajectory;

	// If there is a jump delta and no fall delta or fly mode active, apply the jump delta to the trajectory, 
	// then reduce the jump delta by a fixed amount until it reaches zero.

	jump_delta = enable_player_translation && !fly_mode.get() && FEQ(player_fall_delta, 0.0f) ? curr_jump_delta.get() : 0.0f;
	if (FGT(jump_delta, 0.0f)) {
		new_trajectory.dy = jump_delta * elapsed_time;
		jump_delta -= JUMPING_DEACCELERATION * elapsed_time;
		if (FLT(jump_delta, 0.0f)) {
			jump_delta = 0.0f;
		}
	}
	curr_jump_delta.set(jump_delta);

	// If the new player position is off the map, don't move the player at all
	// and make the floor height invalid.

	old_position = player_viewpoint.position;
	new_position = old_position + new_trajectory;
	new_position.get_map_position(&column, &row, &level);
	if (new_position.x < 0.0 || column >= world_ptr->columns || 
		new_position.z < 0.0 || row < 0 ||
		new_position.y < 0.0 || level >= world_ptr->levels) {
		new_position = old_position;
		floor_y = -1.0f;
	}

	// Otherwise get the list of blocks that overlap the trajectory path,
	// and check for collisions along that path, adjusting the player's new
	// position approapiately.

	else {
		get_overlapping_blocks(new_position.x, new_position.y, new_position.z, 
			old_position.x, old_position.y, old_position.z);
		COL_checkCollisions(col_mesh_list, mesh_pos_list, col_meshes,
			&new_position.x, &new_position.y, &new_position.z,
			old_position.x, old_position.y, old_position.z,
			&floor_y, player_step_height, &player_collision_box);
	}
	
	// If fly mode is not active and the floor height is valid, do a gravity check.

	if (!fly_mode.get()) {
		if (FGE(floor_y, 0.0f)) {

			// If the player height is below the floor height, determine whether
			// the player can step up to that height; if not, the player should not
			// move at all.

			if (FLT(new_position.y, floor_y)) {
				if (FLT(floor_y - new_position.y, player_step_height))
					new_position.y = floor_y + player_step_height;
				else
					new_position = old_position;
			} 
		
			// If the player height is above the floor height, we are in free
			// fall...
	
			else {
		
				// If we can step down to the floor height, do so.

				if (FLT(new_position.y - floor_y, player_step_height)) {
					new_position.y = floor_y;
					player_fall_delta = 0.0f;
				} 
		
				// Otherwise, if we're not jumping, allow gravity to pull the player down by the given
				// fall delta.  If this puts the player below the floor height, put them at the floor
				// height and zero the fall delta.  Otherwise increase the fall delta over time until
				// a maximum delta is reached.
			
				else if (FEQ(jump_delta, 0.0f)) {
					if (FGT(elapsed_time, 0.0f)) {
						new_position.y -= player_fall_delta * elapsed_time;
					}
					if (FLE(new_position.y, floor_y)) {
						new_position.y = floor_y;
						player_fall_delta = 0.0f;
					} else {
						player_fall_delta += FALLING_ACCELERATION * elapsed_time;
						if (FGT(player_fall_delta, MAXIMUM_FALLING_SPEED))
							player_fall_delta = MAXIMUM_FALLING_SPEED;
					}
				}
			}
		}

		// If the floor height is not -1.0f, this means that COL_checkCollisions()
		// returned a bogus new position, so set it to the old position.
	
		else if (FNE(floor_y, -1.0f))
			new_position = old_position;
	}

	// Return the final trajectory vector.

	return(new_position - old_position);
}

//------------------------------------------------------------------------------
// Find a trigger on the active script queue with the same script ID and
// block pointer as the given trigger.
//------------------------------------------------------------------------------

static bool
find_active_script(trigger *trigger_ptr)
{
	for (int j = 0; j < active_script_count; j++) {
		if (active_script_list[j]->script_def_ptr->ID == trigger_ptr->script_def_ptr->ID &&
			active_script_list[j]->block_ptr == trigger_ptr->block_ptr)
			return(true);
	}
	return(false);
}

//------------------------------------------------------------------------------
// Add a trigger to the active trigger list if it has no script, or the active
// script queue if it has a script.
//------------------------------------------------------------------------------

static void
add_trigger_to_active_list(square *square_ptr, block *block_ptr,
						   trigger *trigger_ptr)
{
	trigger *new_trigger_ptr;

	// If the trigger neither has an action list nor a script, then it should
	// be ignored.

	if (trigger_ptr->action_list == NULL && trigger_ptr->script_def_ptr == NULL)
		return;

	// Initialise the trigger.
	
	new_trigger_ptr = trigger_ptr;
	if (new_trigger_ptr != NULL) {
		new_trigger_ptr->square_ptr = square_ptr;
		new_trigger_ptr->block_ptr = block_ptr;

		// If the trigger has an action list, add the trigger to the end of the
		// active trigger list.

		if (new_trigger_ptr->action_list != NULL) {
			active_trigger_list[active_trigger_count] = new_trigger_ptr;
			active_trigger_count++;
		}

		// If the trigger has a script, and there is currently no script on the
		// active script queue with the same script ID and block pointer, then
		// add this trigger to the active script queue, otherwise delete this
		// trigger.

		else if (!find_active_script(new_trigger_ptr)) {
			active_script_list[active_script_count] = new_trigger_ptr;
			active_script_count++;
		}
	}
}

//------------------------------------------------------------------------------
// Step through the given trigger list, adding those that are included in the
// given trigger flags to the active trigger list.
//------------------------------------------------------------------------------

static void
add_triggers_to_active_list(square *square_ptr, block *block_ptr,
							trigger *trigger_list, int trigger_flags,
							bool trigger_in_block)
{
	trigger *trigger_ptr;

	// Step through the trigger list, looking for triggers to add to the
	// active list.  Note that is trigger_in_block is FALSE and a trigger
	// contains a script rather than a list of actions, then we pass NULL as the
	// block pointer.

	trigger_ptr = trigger_list;
	while (trigger_ptr != NULL) {
		if (trigger_ptr->part_index == ALL_PARTS) {
			if ((trigger_ptr->trigger_flag & CLICK_ON) || (trigger_ptr->trigger_flag & ROLL_ON)) {
				add_trigger_to_active_list(square_ptr, block_ptr, trigger_ptr); 
			} else if ((trigger_ptr->trigger_flag & trigger_flags) != 0) {
				if (!trigger_in_block && trigger_ptr->action_list == NULL)
					add_trigger_to_active_list(square_ptr, NULL, trigger_ptr);
				else
					add_trigger_to_active_list(square_ptr, block_ptr, trigger_ptr);
			}
		} else if ((curr_selected_part_ptr->number == trigger_ptr->part_index) &&
			((trigger_ptr->trigger_flag & CLICK_ON) || (trigger_ptr->trigger_flag & ROLL_ON))) {
			add_trigger_to_active_list(square_ptr, block_ptr, trigger_ptr); 
		}
		trigger_ptr = trigger_ptr->next_trigger_ptr;
	}
}

//------------------------------------------------------------------------------
// Process the previously selected square, looking for active triggers.
//------------------------------------------------------------------------------

static void
process_prev_selected_square(void)
{
	int trigger_flags;

	// Reset the trigger flags.

	trigger_flags = 0;

	// If there was a previously selected block, and the currently selected
	// block is different...

	if (curr_selected_block_ptr != prev_selected_block_ptr &&
		prev_selected_block_ptr != NULL) {
		
		// If the previously selected block or square has a "roll off" trigger,
		// add this to the trigger flags.

		if ((prev_selected_block_ptr != NULL && 
			 (prev_selected_block_ptr->trigger_flags & ROLL_OFF)) ||
			(prev_selected_square_ptr != NULL &&
			 (prev_selected_square_ptr->trigger_flags & ROLL_OFF)))
			trigger_flags |= ROLL_OFF;
	}

	// If there are any trigger flags set, add the active triggers from the
	// previously selected block or square to the active trigger list.

	if (trigger_flags) {
		if (prev_selected_block_ptr != NULL)
			add_triggers_to_active_list(prev_selected_square_ptr,
				prev_selected_block_ptr, prev_selected_block_ptr->trigger_list,
				trigger_flags, true);
		if (prev_selected_square_ptr != NULL)
			add_triggers_to_active_list(prev_selected_square_ptr,
				prev_selected_block_ptr, prev_selected_square_ptr->trigger_list,
				trigger_flags, false);
	}
}

//------------------------------------------------------------------------------
// Process the currently selected square, looking for active triggers.
//------------------------------------------------------------------------------

static void
process_curr_selected_square(void)
{
	int trigger_flags;

	// Reset the trigger flags.

	trigger_flags = 0;

	// If there is a currently selected block, and the previously selected
	// block is different...

	if (curr_selected_block_ptr != prev_selected_block_ptr &&
		curr_selected_block_ptr != NULL) {
		
		// If the currently selected block or square has a "roll on" trigger, 
		// add this to the trigger flags.

		if ((curr_selected_block_ptr != NULL &&
			 (curr_selected_block_ptr->trigger_flags & ROLL_ON)) ||
			(curr_selected_square_ptr != NULL &&
			 (curr_selected_square_ptr->trigger_flags & ROLL_ON)))
			trigger_flags |= ROLL_ON;
	}

	// If the left mouse has been clicked while NOT in build mode, and the currently selected block or 
	// square has a "click on" trigger, add this to the trigger flags.

	if (left_mouse_was_clicked && !build_mode.get() &&
		((curr_selected_block_ptr != NULL && 
		  (curr_selected_block_ptr->trigger_flags & CLICK_ON)) ||
		 (curr_selected_square_ptr != NULL &&
		 (curr_selected_square_ptr->trigger_flags & CLICK_ON))))
		trigger_flags |= CLICK_ON;

	// If there are any trigger flags set, add the active triggers from the
	// currently selected square to the active trigger list.

	if (trigger_flags) {
		if (curr_selected_block_ptr != NULL)
			add_triggers_to_active_list(curr_selected_square_ptr,
				curr_selected_block_ptr, curr_selected_block_ptr->trigger_list,
				trigger_flags, true);
		if (curr_selected_square_ptr != NULL)
			add_triggers_to_active_list(curr_selected_square_ptr,
				curr_selected_block_ptr, curr_selected_square_ptr->trigger_list,
				trigger_flags, false);
	}
}

//------------------------------------------------------------------------------
// Process the previously selected area, looking for active triggers.
//------------------------------------------------------------------------------

static void
process_prev_selected_area(void)
{
	int trigger_flags;

	// Reset the trigger flags.

	trigger_flags = 0;

	// If there was a previously selected area, the currently selected area
	// is different, and the previously selected area has a "roll off" trigger,
	// add this to the trigger flags.

	if (curr_selected_area_ptr != prev_selected_area_ptr &&
		prev_selected_area_ptr != NULL &&
		(prev_selected_area_ptr->trigger_flags & ROLL_OFF))
		trigger_flags |= ROLL_OFF;

	// If there are any trigger flags set, add the active triggers from the
	// previously selected area to the active trigger list.

	if (trigger_flags)
		add_triggers_to_active_list(prev_area_square_ptr, prev_area_block_ptr, 
			prev_selected_area_ptr->trigger_list, trigger_flags, 
			prev_area_block_ptr != NULL);
}

//------------------------------------------------------------------------------
// Process the currently selected area, looking for active triggers.
//------------------------------------------------------------------------------

static void
process_curr_selected_area(void)
{
	int trigger_flags;

	// Reset the trigger flags.

	trigger_flags = 0;

	// If there is a currently selected area, the previously selected area 
	// is different, and the currently selected area has a "roll off" trigger,
	// add this to the trigger flags.

	if (curr_selected_area_ptr != prev_selected_area_ptr &&
		curr_selected_area_ptr != NULL &&
		(curr_selected_area_ptr->trigger_flags & ROLL_ON))
		trigger_flags |= ROLL_ON;

	// If the mouse has been clicked while NOT in build mode, and the currently selected area has
	// a "click on" trigger, add this to the trigger flags.

	if (left_mouse_was_clicked && !build_mode.get() && curr_selected_area_ptr != NULL &&
		(curr_selected_area_ptr->trigger_flags & CLICK_ON))
		trigger_flags |= CLICK_ON;

	// If there are any trigger flags set, add the active triggers from the
	// currently selected area to the active trigger list.

	if (trigger_flags) {
		add_triggers_to_active_list(curr_area_square_ptr, curr_area_block_ptr,
			curr_selected_area_ptr->trigger_list, trigger_flags, 
			curr_area_block_ptr != NULL);
	}
}

//------------------------------------------------------------------------------
// Process a list of triggers, looking for active global triggers.
//------------------------------------------------------------------------------

static void
process_global_triggers_in_trigger_list(trigger *trigger_list, 
										vertex *translation_ptr,
										bool trigger_in_block)
{
	trigger *trigger_ptr;
	bool activated;
	vector distance;
	float distance_squared;
	bool currently_inside;
	square *square_ptr;
	int column, row, level;

	// Step through the trigger list.

	trigger_ptr = trigger_list;
	while (trigger_ptr != NULL) {

		// If this is a "step in" or "step out" or "proximity" trigger, compute
		// the distance squared from the trigger to the player, and from this 
		// determine whether the player is inside the radius.

		if (trigger_ptr->trigger_flag == STEP_IN ||
			trigger_ptr->trigger_flag == STEP_OUT ||
			trigger_ptr->trigger_flag == PROXIMITY) {
			if (translation_ptr != NULL)
				distance = *translation_ptr + trigger_ptr->position - 
					player_viewpoint.position;
			else
				distance = trigger_ptr->position - player_viewpoint.position;
			distance_squared = distance.dx * distance.dx + 
				distance.dy * distance.dy + distance.dz * distance.dz;
			currently_inside = distance_squared <= trigger_ptr->radius_squared;
		}

		// Check for an activated trigger.

		activated = false;
		switch (trigger_ptr->trigger_flag) {
		case STEP_IN:

			// If the new state of the trigger is "inside", and the old state
			// of the trigger was not "inside", activate this trigger.

			if (currently_inside && !trigger_ptr->previously_inside)
				activated = true;
			trigger_ptr->previously_inside = currently_inside;
			break;

		case STEP_OUT:

			// If the new state of the trigger is "outside", and the old state
			// of the trigger was not "outside", activate this trigger.

			if (!currently_inside && trigger_ptr->previously_inside)
				 activated = true;
			trigger_ptr->previously_inside = currently_inside;
			break;

		case PROXIMITY:

			// If the player in inside the radius, activate this trigger.

			activated = currently_inside;
			break;

		case TIMER:

			// If the trigger delay has elapsed, activate this trigger and
			// set the next trigger delay.
	
			if (curr_time_ms - trigger_ptr->start_time_ms >= 
				trigger_ptr->delay_ms) {
				activated = true;
				set_trigger_delay(trigger_ptr, curr_time_ms);
			}
			break;

		case LOCATION:

			// Get the location of the trigger square, or if the trigger is
			// associated with a movable block the closest square to that
			// block.

			square_ptr = trigger_ptr->square_ptr;
			if (square_ptr != NULL)
				world_ptr->get_square_location(square_ptr, &column, &row, 
					&level);
			else
				trigger_ptr->block_ptr->translation.get_map_position(&column,
					&row, &level);

			// If this matches the target location, activate the trigger.

			if (column == trigger_ptr->target.column &&
				row == trigger_ptr->target.row && 
				level == trigger_ptr->target.level)
				activated = true;
			break;

		case KEY_DOWN:

			// If a key down event has occurred for the key code in the
			// trigger, activate the trigger.

			if (trigger_ptr->key_code == curr_key_event.key_code &&
				curr_key_event.key_down)
				activated = true;
			break;

		case KEY_UP:

			// If a key up event has occurred for the key code in the
			// trigger, activate the trigger.

			if (trigger_ptr->key_code == curr_key_event.key_code &&
				!curr_key_event.key_down)
				activated = true;
			break;

		case KEY_HOLD:

			// If a key down event has occurred for the key code in the
			// trigger, activate the trigger and initialise the trigger delay.

			if (trigger_ptr->key_code == curr_key_event.key_code &&
				curr_key_event.key_down) {
				activated = true;
				set_trigger_delay(trigger_ptr, curr_time_ms);
			}

			// If the trigger delay has elapsed and the key(s) are still down,
			// activate this trigger and set the next trigger delay.

			else if (curr_time_ms - trigger_ptr->start_time_ms >= 
				trigger_ptr->delay_ms && key_down_list[trigger_ptr->key_code]) {
				activated = true;
				set_trigger_delay(trigger_ptr, curr_time_ms);
			}
		}


		// If trigger was activated, add it to the active trigger list.  Note
		// that is trigger_in_block is FALSE and the trigger contains a script
		// rather than a list of actions, then we pass NULL as the block pointer.

		if (activated) {
			if (!trigger_in_block && trigger_ptr->action_list == NULL)
				add_trigger_to_active_list(trigger_ptr->square_ptr, NULL, 
					trigger_ptr);
			else
				add_trigger_to_active_list(trigger_ptr->square_ptr, 
					trigger_ptr->block_ptr, trigger_ptr);
		}

		// Move onto the next global trigger.

		trigger_ptr = trigger_ptr->next_trigger_ptr;
	}
}

//------------------------------------------------------------------------------
// Process all triggers in the given block list, looking for active global 
// triggers.
//------------------------------------------------------------------------------

static void
process_global_triggers_in_block_list(block *block_list)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->trigger_list != NULL)
			process_global_triggers_in_trigger_list(block_ptr->trigger_list,
				&block_ptr->translation, true);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Return a pointer to the block definition referred to by the given symbol.
//------------------------------------------------------------------------------

block_def *
symbol_to_block_def(word symbol)
{
	if (symbol != NULL_BLOCK_SYMBOL) {
		if (symbol == GROUND_BLOCK_SYMBOL)
			return(ground_block_def_ptr);
		else
			return(block_symbol_table[symbol]);
	} else
		return(NULL);
}

//------------------------------------------------------------------------------
// Execute a replace action.
//------------------------------------------------------------------------------

static void
execute_replace_action(trigger *trigger_ptr, action *action_ptr)
{
	square *trigger_square_ptr, *target_square_ptr;
	int trigger_column, trigger_row, trigger_level;
	int target_column, target_row, target_level;
	int source_column, source_row, source_level;
	relcoords *target_location_ptr, *source_location_ptr;
	block *block_ptr;
	block_def *block_def_ptr;
	vertex translation;
	bool removed_movable_block;

	// Get the location of the trigger square, if it is set (movable blocks
	// do not have a trigger square).  Otherwise use the square the movable
	// block is closest to as the trigger square.

	trigger_square_ptr = trigger_ptr->square_ptr;
	block_ptr = trigger_ptr->block_ptr;
	if (trigger_square_ptr != NULL)
		world_ptr->get_square_location(trigger_square_ptr, 
			&trigger_column, &trigger_row, &trigger_level);
	else {
		translation = block_ptr->translation;
		translation.get_map_position(&trigger_column, &trigger_row,
			&trigger_level);
		trigger_square_ptr = world_ptr->get_square_ptr(trigger_column, 
			trigger_row, trigger_level);
	}

	// If the trigger square is also the target square, use it.

	if (action_ptr->target_is_trigger) {
		target_square_ptr = trigger_square_ptr;
		target_column = trigger_column;
		target_row = trigger_row;
		target_level = trigger_level;
	}

	// If the trigger square is not the target square, calculate the
	// location of the target square, and get a pointer to that square.

	else {

		// Set the target column.

		target_location_ptr = &action_ptr->target;
		target_column = target_location_ptr->column;
		if (target_location_ptr->relative_column) {
			target_column = (trigger_column + target_column) % 
				world_ptr->columns;
			if (target_column < 0)
				target_column += world_ptr->columns;
		} else {
			target_column--;
		}

		// Set the target row.

		target_row = target_location_ptr->row;
		if (target_location_ptr->relative_row) {
			target_row = (trigger_row + target_row) % world_ptr->rows;
			if (target_row < 0)
				target_row += world_ptr->rows;
		} else {
			target_row--;
		}

		// Set the target level.  If the target uses a relative coordinate for
		// the level and a ground level exists, make sure the ground level is
		// never selected.  Also make sure the top empty level is never
		// selected.

		target_level = target_location_ptr->level;
		if (target_location_ptr->relative_level) {
			if (world_ptr->ground_level_exists) {
				target_level = ((trigger_level - 1) + target_level) %
					(world_ptr->levels - 2);
				if (target_level < 0)
					target_level += (world_ptr->levels - 1);
				else
					target_level++;
			} else {
				target_level = (trigger_level + target_level) %
					(world_ptr->levels - 1);
				if (target_level < 0)
					target_level += (world_ptr->levels - 1);
			}
		} else {
			target_level--;
		}

		// Get a pointer to the target square.

		target_square_ptr = world_ptr->get_square_ptr(target_column,
			target_row, target_level);
	}

	// Remove the old block.  If the trigger square is the target square, and
	// the block is movable, then we must remove the movable block.  Otherwise
	// we must remove the block on the target square.

	if (action_ptr->target_is_trigger && block_ptr != NULL && 
		block_ptr->block_def_ptr->movable) {
		remove_movable_block(block_ptr);
		removed_movable_block = true;
	} else {
		remove_fixed_block(target_square_ptr);
		removed_movable_block = false;
	}

	// If the source is a block symbol, get a pointer to the block
	// definition for that symbol.

	if (action_ptr->source.is_symbol)
		block_def_ptr = symbol_to_block_def(action_ptr->source.symbol);
	
	// If the source is a location, get a pointer to the block at that
	// location, and from that get a pointer to the block definition.
	
	else {

		// Set the source column.

		source_location_ptr = &action_ptr->source.location;
		source_column = source_location_ptr->column;
		if (source_location_ptr->relative_column) {
			source_column = (trigger_column + source_column) %
				world_ptr->columns;
			if (source_column < 0)
				source_column += world_ptr->columns;
		}

		// Set the source row.

		source_row = source_location_ptr->row;
		if (source_location_ptr->relative_row) {
			source_row = (trigger_row + source_row) % world_ptr->rows;
			if (source_row < 0)
				source_row += world_ptr->rows;
		}

		// Set the source level.  If the source uses a relative coordinate for
		// the level and a ground level exists, make sure the ground level is
		// never selected.  Also make sure the top empty level is never
		// selected.

		source_level = source_location_ptr->level;
		if (source_location_ptr->relative_level) {
			if (world_ptr->ground_level_exists) {
				source_level = ((trigger_level - 1) + source_level) %
					(world_ptr->levels - 2);
				if (source_level < 0)
					source_level += (world_ptr->levels - 1);
				else
					source_level++;
			} else {
				source_level = (trigger_level + source_level) %
					(world_ptr->levels - 1);
				if (source_level < 0)
					source_level += (world_ptr->levels - 1);
			}
		}

		// Get a pointer to the block and block definition, if there is one.

		block_ptr = world_ptr->get_block_ptr(source_column, 
			source_row, source_level);
		if (block_ptr != NULL)
			block_def_ptr = block_ptr->block_def_ptr;
		else
			block_def_ptr = NULL;
	}

	// Add the new block to the target square, or add a new movable block, if
	// the block definition exists.  If the new block is movable and the old
	// block was also movable, set the position of the new block to be the same
	// as the old block.

	if (block_def_ptr != NULL) {
		if (block_def_ptr->movable) {
			if (!removed_movable_block)
				translation.set_map_translation(target_column, target_row,
					target_level);
			add_movable_block(block_def_ptr, translation);
		} else
			add_fixed_block(block_def_ptr, target_square_ptr, true);
	}
}

//------------------------------------------------------------------------------
// Execute a ripple action.
//------------------------------------------------------------------------------

static void
execute_ripple_action(action *action_ptr)
{
	block *block_ptr;
	int s, n, p, index, drop;
	float t;
	float *temp;

	block_ptr = action_ptr->trigger_ptr->block_ptr;			
	s = (int)sqrt((float)action_ptr->vertices);
	drop = (int)((float)rand() / RAND_MAX * 100);
	if (drop < action_ptr->droprate * 100) {
		if (action_ptr->style == RAIN_RIPPLE) {
			drop = (int)((float)rand() / RAND_MAX * s * s);
			action_ptr->curr_step_ptr[drop] = (float)rand() / RAND_MAX * action_ptr->force;
		} else {
			if (action_ptr->temp > 5) {
				for (n=s+1;n < (2*s)-2;n++) {
					action_ptr->curr_step_ptr[n] = action_ptr->force;
				}
				action_ptr->temp = 0;
			} else {
				action_ptr->temp++;
			}
		}
		for (n=0;n < s;n++) {
			action_ptr->curr_step_ptr[n] = 0.0f;
			action_ptr->prev_step_ptr[n] = 0.0f;
			action_ptr->curr_step_ptr[n * s] = 0.0f;
			action_ptr->prev_step_ptr[n * s] = 0.0f;
			if (n != 0) {
				action_ptr->curr_step_ptr[n * s - 1] = 0.0f;
				action_ptr->prev_step_ptr[n * s - 1] = 0.0f;
			}
			action_ptr->curr_step_ptr[s * s - n] = 0.0f;
			action_ptr->prev_step_ptr[s * s - n] = 0.0f;
		}
	}

	for (n = 1; n < s - 1; n++) {
		for (p = 1; p < s - 1; p++) {
			index = (p * s) + n;
			t = action_ptr->damp * (((action_ptr->prev_step_ptr[index + 1] +
				action_ptr->prev_step_ptr[index - 1] +
				action_ptr->prev_step_ptr[index - s] +
				action_ptr->prev_step_ptr[index + s]) * 0.5f) -
				action_ptr->curr_step_ptr[index]);
			block_ptr->vertex_list[index].y += t  - action_ptr->prev_step_ptr[index];
			action_ptr->curr_step_ptr[index] = t;
		}
	}

	temp = action_ptr->prev_step_ptr;
	action_ptr->prev_step_ptr = action_ptr->curr_step_ptr;
	action_ptr->curr_step_ptr = temp;
 }

//------------------------------------------------------------------------------
// Execute a spin action.
//------------------------------------------------------------------------------

static void
execute_spin_action(action *action_ptr, int time_diff)
{
	block *block_ptr = action_ptr->trigger_ptr->block_ptr;
	block_ptr->rotate_x(action_ptr->spin_angles.x * (float)time_diff / 1000.0f);
	block_ptr->rotate_y(action_ptr->spin_angles.y * (float)time_diff / 1000.0f);
	block_ptr->rotate_z(action_ptr->spin_angles.z * (float)time_diff / 1000.0f);
}

//------------------------------------------------------------------------------
// Execute an orbit action.
//------------------------------------------------------------------------------

static void
execute_orbit_action(action *action_ptr,int time_diff)
{
	block *block_ptr, *center_ptr;
	block_def *block_def_ptr;

	block_ptr = action_ptr->trigger_ptr->block_ptr;
	action_ptr->temp = action_ptr->temp + (time_diff * action_ptr->speed);
	if (action_ptr->source.is_symbol) {
		block_def_ptr = symbol_to_block_def(action_ptr->source.symbol);
		center_ptr = block_def_ptr->used_block_list;
		if (center_ptr == NULL) return;
	} else
		return;
	
	block_ptr->translation.z = center_ptr->translation.z +
							(float)(sin(RAD((float)(action_ptr->temp) / 1000.0f)) * 
							action_ptr->spin_angles.z / TEXELS_PER_UNIT);
	block_ptr->translation.x = center_ptr->translation.x +
							(float)(sin(RAD((float)(action_ptr->temp) / 1000.0f) + 90.0f) *
							action_ptr->spin_angles.x / TEXELS_PER_UNIT);
	block_ptr->translation.y = center_ptr->translation.y +
							(float)(sin(RAD((float)(action_ptr->temp) / 1000.0f) + 180.0f) * 
							action_ptr->spin_angles.y / TEXELS_PER_UNIT);
}

//------------------------------------------------------------------------------
// Execute move action.
//------------------------------------------------------------------------------

static void
execute_move_action(action *action_ptr, int time_diff)
{
	block *block_ptr;
	float movex,movey,movez;
	int done;
	char *cptr;

	block_ptr = action_ptr->trigger_ptr->block_ptr;
	done = 0;

	movex = action_ptr->speedx * (float)(time_diff) / 1000.0f;
	movey = action_ptr->speedy * (float)(time_diff) / 1000.0f;
	movez = action_ptr->speedz * (float)(time_diff) / 1000.0f;

	if (fabs(action_ptr->totalx) <= movex) {
		movex = action_ptr->totalx;
		action_ptr->totalx = 0.0f;
		done++;
	} else {
		if (action_ptr->totalx < 0.0f) {
			action_ptr->totalx += movex;
			movex = movex * -1.0f;
		}
		else
			action_ptr->totalx -= movex;
	}

	if (fabs(action_ptr->totaly) <= movey) {
		movey = action_ptr->totaly;
		action_ptr->totaly = 0.0f;
		done++;
	} else {
		if (action_ptr->totaly < 0.0f) {
			action_ptr->totaly += movey;
			movey = movey * -1.0f;
		}
		else
			action_ptr->totaly -= movey;
	}

	if (fabs(action_ptr->totalz) <= movez) {
		movez = action_ptr->totalz;
		action_ptr->totalz = 0.0f;
		done++;
	} else {
		if (action_ptr->totalz < 0.0f) {
			action_ptr->totalz += movez;
			movey = movey * -1.0f;
		}
		else
			action_ptr->totalz -= movez;
	}

	block_ptr->translation.x += movex / TEXELS_PER_UNIT;
	block_ptr->translation.y += movey / TEXELS_PER_UNIT;
	block_ptr->translation.z += movez / TEXELS_PER_UNIT;
		
	if (done == 3) {				
		try {
			cptr = action_ptr->charindex;

			// Skip opening whitespace or an open paren.

			while (*cptr == ' ' || *cptr == '\t' || *cptr == ')' || *cptr == '(' || *cptr == ',') {
				cptr = cptr + 1;
			}

			if (*cptr == '/0' || *(cptr + 1) == '/0') 
				throw false;

			switch (*cptr) {
			case 's':
				cptr = cptr + 1;
				switch (*cptr) {
				case 'p': // speed
					cptr = cptr + 1;
					if (!sscanf(cptr, ",%f,%f,%f)", &action_ptr->speedx,&action_ptr->speedy,&action_ptr->speedz))
						throw false;
					break;
				default:
					throw false;
				}
				break;
			case 'l': 
				cptr = cptr + 1;
				switch (*cptr) {
				case 'p': // loop
					action_ptr->charindex = action_ptr->exit_URL.text;
					return;
					break;
				default:
					throw false;
				}
				break;
			case 'm':
				cptr = cptr + 1;
				switch (*cptr) {
				case 'v': // move
					cptr = cptr + 1;
					if (!sscanf(cptr, ",%f,%f,%f)", &action_ptr->totalx,&action_ptr->totaly,&action_ptr->totalz))
						throw false;
					break;
				default:
					throw false;
				}
				break;
			default:
				throw false;
			}
		}
		catch (...) {
			remove_clock_action(action_ptr);
			action_ptr->charindex = action_ptr->exit_URL.text;
			return;
		}

		while (*cptr != ')') 
			cptr = cptr + 1;
		action_ptr->charindex = cptr;
	}
}

//------------------------------------------------------------------------------
// Execute a setframe action.
//------------------------------------------------------------------------------

static void
execute_setframe_action(trigger *trigger_ptr, action *action_ptr)
{
	block *block_ptr;
	block_def *block_def_ptr;

	block_ptr = trigger_ptr->block_ptr;
	block_def_ptr = block_ptr->block_def_ptr;
		
	if (block_def_ptr->animated) {
		if (action_ptr->rel_number.relative_value) {
			block_ptr->current_frame = (block_ptr->current_frame + action_ptr->rel_number.value);
			if (block_ptr->current_frame < 0)
				block_ptr->current_frame = block_def_ptr->animation->frames - 1;
			else
				block_ptr->current_frame %= block_def_ptr->animation->frames;
			block_ptr->vertices = block_def_ptr->animation->vertices[block_ptr->current_frame];
			block_ptr->vertex_list = block_def_ptr->animation->frame_list[block_ptr->current_frame].vertex_list;
		}
		else {
			if (action_ptr->rel_number.value < (block_def_ptr->animation->frames)) {
				block_ptr->current_frame = action_ptr->rel_number.value;
				block_ptr->vertices = block_def_ptr->animation->vertices[block_ptr->current_frame];
				block_ptr->vertex_list = block_def_ptr->animation->frame_list[block_ptr->current_frame].vertex_list;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Execute an animation action.
//------------------------------------------------------------------------------

static void
execute_animate_action(trigger *trigger_ptr, action *action_ptr)
{
	block *block_ptr;
	block_def *block_def_ptr;
	intrange *loops_list;

	block_ptr = trigger_ptr->block_ptr;
	block_def_ptr = block_ptr->block_def_ptr;
	loops_list = block_def_ptr->animation->loops_list;
		
	if (block_def_ptr->animation->loops != 0 && block_ptr->current_loop != -1) {
		block_ptr->current_frame++;
		if (block_ptr->current_frame > loops_list[block_ptr->current_loop].max) {
			if (block_ptr->current_repeat) {
				block_ptr->set_frame(loops_list[block_ptr->current_loop].min);
			} else if (block_ptr->next_loop != -1) {
				block_ptr->current_loop = block_ptr->next_loop;
				block_ptr->next_loop = -1;
				block_ptr->set_frame(loops_list[block_ptr->current_loop].min);
				block_ptr->current_repeat = block_ptr->next_repeat;
				block_ptr->next_repeat = false;
			} else {
				block_ptr->current_loop = -1;
				block_ptr->current_repeat = false;
				block_ptr->current_frame--;
				return;
			}
		} else {
			block_ptr->set_frame(block_ptr->current_frame);
		}
	}
}

//------------------------------------------------------------------------------
// Execute a setloop action.
//------------------------------------------------------------------------------

static void
execute_setloop_action(trigger *trigger_ptr, action *action_ptr)
{
	block *block_ptr;
	block_def *block_def_ptr;

	block_ptr = trigger_ptr->block_ptr;
	block_def_ptr = block_ptr->block_def_ptr;
	
	if (block_def_ptr->animation->loops != 0) {
		if (action_ptr->rel_number.relative_value) {
			block_ptr->next_loop = (block_ptr->current_loop + action_ptr->rel_number.value);
			if (block_ptr->next_loop < 0)
				block_ptr->next_loop = block_def_ptr->animation->loops - 1;
			else
				block_ptr->next_loop %= block_def_ptr->animation->loops;
		} else {
			if (action_ptr->rel_number.value < (block_def_ptr->animation->loops)) {
				block_ptr->next_loop = action_ptr->rel_number.value;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Execute the list of active triggers.  These are triggers without scripts.
//------------------------------------------------------------------------------

static bool
execute_active_trigger_list(void)
{
	trigger *trigger_ptr;
	action *action_ptr;
	bool spot_continues;
	int j;

	// Step through the active triggers, and execute it's actions in turn.
	// Note that we stop executing actions if the trigger no longer has a block
	// or square pointer, indicating that the block the trigger belonged to was
	// removed from the map.

	for(j = 0; j < active_trigger_count; j++) {
		trigger_ptr = active_trigger_list[j];
		action_ptr = trigger_ptr->action_list;
		if (trigger_ptr != NULL && (trigger_ptr->block_ptr != NULL || trigger_ptr->square_ptr != NULL)) {
			while (action_ptr != NULL) {
				switch (action_ptr->type) {
				case REPLACE_ACTION:
					execute_replace_action(trigger_ptr, action_ptr);
					break;
				case RIPPLE_ACTION:
					add_clock_action(action_ptr);
					break;
				case SPIN_ACTION:
					add_clock_action(action_ptr);
					break;
				case ORBIT_ACTION:
					add_clock_action(action_ptr);
					break;
				case MOVE_ACTION:
					add_clock_action(action_ptr);
					break;
				case SETFRAME_ACTION:
					execute_setframe_action(trigger_ptr, action_ptr);
					break;
				case SETLOOP_ACTION:
					execute_setloop_action(trigger_ptr, action_ptr);
					break;
				case ANIMATE_ACTION:
					execute_animate_action(trigger_ptr, action_ptr);
					break;
				case STOPSPIN_ACTION:
					remove_clock_action_by_type(action_ptr,SPIN_ACTION);
					break;
				case STOPORBIT_ACTION:
					remove_clock_action_by_type(action_ptr,ORBIT_ACTION);
					break;
				case STOPRIPPLE_ACTION:
					remove_clock_action_by_type(action_ptr,RIPPLE_ACTION);
					break;
				case STOPMOVE_ACTION:
					remove_clock_action_by_type(action_ptr,MOVE_ACTION);
					break;
				case EXIT_ACTION:
					spot_continues = handle_exit(action_ptr->exit_URL, action_ptr->exit_target, action_ptr->is_spot_URL, false);
					active_trigger_list[0] = NULL;
					active_trigger_count = 0;
					return(spot_continues);
				}
				action_ptr = action_ptr->next_action_ptr;
			}
		}
	}

	// Clear the active trigger list and the last active trigger pointer.

	active_trigger_list[0] = NULL;
	active_trigger_count = 0;
	return(true);
}

//------------------------------------------------------------------------------
// Execute the list of active triggers that are tied to the master clock
// SPIN, RIPPLE, MOVE, ORBIT
//------------------------------------------------------------------------------

static bool
execute_global_clock_action_list(int time_diff)
{
	action *action_ptr;
	int j;

	// Step through the actions.

	for(j = 0; j < active_clock_action_count; j++) {
		action_ptr = active_clock_action_list[j];
		switch (action_ptr->type) {
		case RIPPLE_ACTION:
			execute_ripple_action(action_ptr);
			break;
		case SPIN_ACTION:
			execute_spin_action(action_ptr,time_diff);
			break;
		case ORBIT_ACTION:
			execute_orbit_action(action_ptr,time_diff);
			break;
		case MOVE_ACTION:
			execute_move_action(action_ptr,time_diff);
			break;
		}
	}
	return(true);
}

//------------------------------------------------------------------------------
// Execute a single trigger.
//------------------------------------------------------------------------------

bool
execute_trigger(trigger* trigger_ptr)
{
	action *action_ptr;
	bool spot_continues;

	action_ptr = trigger_ptr->action_list;
	while (action_ptr != NULL) {
		switch (action_ptr->type) {
			case REPLACE_ACTION:
				execute_replace_action(trigger_ptr, action_ptr);
				break;
			case RIPPLE_ACTION:
				add_clock_action(action_ptr);
				break;
			case SPIN_ACTION:
				add_clock_action(action_ptr);
				break;
			case ORBIT_ACTION:
				add_clock_action(action_ptr);
				break;
			case MOVE_ACTION:
				add_clock_action(action_ptr);
				break;
			case SETFRAME_ACTION:
				execute_setframe_action(trigger_ptr, action_ptr);
				break;
			case SETLOOP_ACTION:
				execute_setloop_action(trigger_ptr, action_ptr);
				break;
			case ANIMATE_ACTION:
				execute_animate_action(trigger_ptr, action_ptr);
				break;
			case STOPSPIN_ACTION:
				remove_clock_action_by_type(action_ptr,SPIN_ACTION);
				break;
			case STOPORBIT_ACTION:
				remove_clock_action_by_type(action_ptr,ORBIT_ACTION);
				break;
			case STOPRIPPLE_ACTION:
				remove_clock_action_by_type(action_ptr,RIPPLE_ACTION);
				break;
			case STOPMOVE_ACTION:
				remove_clock_action_by_type(action_ptr,MOVE_ACTION);
				break;
			case EXIT_ACTION:
				spot_continues = handle_exit(action_ptr->exit_URL, action_ptr->exit_target, action_ptr->is_spot_URL, false);
				return(spot_continues);
		}
		action_ptr = action_ptr->next_action_ptr;
	}
	return(true);
}

//------------------------------------------------------------------------------
// Function to set up a clipping plane.
//------------------------------------------------------------------------------

static void
set_clipping_plane(int plane_index, float z)
{
	frustum_vertex_list[plane_index].x = -half_viewport_width * z;
	frustum_vertex_list[plane_index].y = half_viewport_height * z;
	frustum_vertex_list[plane_index].z = z;
	frustum_vertex_list[plane_index + 1].x = half_viewport_width * z;
	frustum_vertex_list[plane_index + 1].y = half_viewport_height * z;
	frustum_vertex_list[plane_index + 1].z = z;
	frustum_vertex_list[plane_index + 2].x = half_viewport_width * z;
	frustum_vertex_list[plane_index + 2].y = -half_viewport_height * z;
	frustum_vertex_list[plane_index + 2].z = z;
	frustum_vertex_list[plane_index + 3].x = -half_viewport_width * z;
	frustum_vertex_list[plane_index + 3].y = -half_viewport_height * z;
	frustum_vertex_list[plane_index + 3].z = z;
}

//------------------------------------------------------------------------------
// Compute the normal vector for a given set of corner vertices (given in
// clockwise order around the front face).
//------------------------------------------------------------------------------

static void
compute_frustum_normal_vector(int plane_index, int vertex1_index, 
							  int vertex2_index, int vertex3_index)
{
	vector vector1 = frustum_vertex_list[vertex3_index] - 
		frustum_vertex_list[vertex2_index];
	vector vector2 = frustum_vertex_list[vertex1_index] - 
		frustum_vertex_list[vertex2_index];
	vector vector3 = vector1 * vector2;
	vector3.normalise();
	frustum_normal_vector_list[plane_index] = vector3;
}

//------------------------------------------------------------------------------
// Compute the plane offset for a given frustum normal vector and corner vertex.
//------------------------------------------------------------------------------

static void
compute_frustum_plane_offset(int plane_index, int vertex_index)
{
	vector *vector_ptr = &frustum_normal_vector_list[plane_index];
	vertex *vertex_ptr = &frustum_vertex_list[vertex_index];
	frustum_plane_offset_list[plane_index] = -(vector_ptr->dx * vertex_ptr->x + 
		vector_ptr->dy * vertex_ptr->y + vector_ptr->dz * vertex_ptr->z);
}

//------------------------------------------------------------------------------
// Compute the plane equations for the frustum.
//------------------------------------------------------------------------------

static void
compute_frustum_plane_equations(void)
{
	// Calculate the frustum normal vectors.

	compute_frustum_normal_vector(FRUSTUM_NEAR_PLANE, 3, 0, 1);
	compute_frustum_normal_vector(FRUSTUM_FAR_PLANE, 7, 6, 5);
	compute_frustum_normal_vector(FRUSTUM_LEFT_PLANE, 4, 0, 3);
	compute_frustum_normal_vector(FRUSTUM_RIGHT_PLANE, 5, 6, 2);
	compute_frustum_normal_vector(FRUSTUM_TOP_PLANE, 1, 0, 4);
	compute_frustum_normal_vector(FRUSTUM_BOTTOM_PLANE, 2, 6, 7);

	// Calculate the plane offsets.

	compute_frustum_plane_offset(FRUSTUM_NEAR_PLANE, 0);	
	compute_frustum_plane_offset(FRUSTUM_FAR_PLANE, 6);	
	compute_frustum_plane_offset(FRUSTUM_LEFT_PLANE, 0);	
	compute_frustum_plane_offset(FRUSTUM_RIGHT_PLANE, 6);	
	compute_frustum_plane_offset(FRUSTUM_TOP_PLANE, 0);	
	compute_frustum_plane_offset(FRUSTUM_BOTTOM_PLANE, 6);
}

//------------------------------------------------------------------------------
// Set the viewport variables.
//------------------------------------------------------------------------------

void
set_viewport(int width, int height)
{
	float half_horz_field_of_view, half_vert_field_of_view;

	// Compute half of the window dimensions, for convienance.

	half_window_width = (float)window_width * 0.5f;
	half_window_height = (float)window_height * 0.5f;

	// Set the horizontal field of view to 60 degrees if the aspect ratio is
	// wide (with the vertical field of view smaller), otherwise set the
	// vertical field of view to 60 degrees (with the horizontal field of view
	// smaller).

	aspect_ratio = (float)width / (float)height;
	if (aspect_ratio >= 1.0) {
		horz_field_of_view = DEFAULT_FIELD_OF_VIEW;
		vert_field_of_view = DEFAULT_FIELD_OF_VIEW / aspect_ratio;
	} else {
		horz_field_of_view = DEFAULT_FIELD_OF_VIEW * aspect_ratio;
		vert_field_of_view = DEFAULT_FIELD_OF_VIEW;
	}

	// Compute half the horizontal and vertical fields of view.

	half_horz_field_of_view = horz_field_of_view * 0.5f;
	half_vert_field_of_view = vert_field_of_view * 0.5f;

	// Compute the viewport dimensions.

	half_viewport_width = tanf(RAD(half_horz_field_of_view));
	half_viewport_height = tanf(RAD(half_vert_field_of_view));
	viewport_width = half_viewport_width * 2.0f;
	viewport_height = half_viewport_height * 2.0f;

	// Compute the horizontal and vertical scaling factors.

	horz_scaling_factor = 1.0f / half_viewport_width;
	vert_scaling_factor = 1.0f / half_viewport_height;

	// Compute the number of pixels per world unit at z = 1, and the number of
	// pixels per degree, in both the horizontal and vertical direction.

	horz_pixels_per_degree = width / horz_field_of_view;
	vert_pixels_per_degree = height / vert_field_of_view;

	// Set the clipping planes and plane equations of the frustum, and if
	// hardware acceleration is enabled set the projection transform and global fog.

	set_clipping_plane(NEAR_CLIPPING_PLANE, 1.0f);
	set_clipping_plane(FAR_CLIPPING_PLANE, visible_radius);
	compute_frustum_plane_equations();
	if (hardware_acceleration) {
		hardware_set_projection_transform(viewport_width, viewport_height, 1.0f, visible_radius);
		hardware_update_fog_settings(global_fog_enabled, &global_fog, visible_radius);
	}
}

//------------------------------------------------------------------------------
// Render next frame.
//------------------------------------------------------------------------------

static bool 
render_next_frame(void)
{
	int prev_time_ms;
	float elapsed_time;
	float move_delta, side_delta, turn_delta, look_delta;
	vector trajectory, new_trajectory, unit_trajectory;
	vector orig_direction, new_direction;
	int column, row, level;
	int last_column, last_row, last_level;
	square *square_ptr;
	hyperlink *exit_ptr;
	block *block_ptr;
	float old_visible_radius;

	// Update the current time in milliseconds, and compute the elapsed time in
	// seconds.

	if (frames_rendered > 0) {
		prev_time_ms = curr_time_ms;
		curr_time_ms = get_time_ms();
		elapsed_time = (float)(curr_time_ms - prev_time_ms) / 1000.0f;
	} else {
		curr_time_ms = get_time_ms();
		clocktimer_time_ms = curr_time_ms;
		elapsed_time = 0.0f;
	}

	// Get the current mouse position.

	mouse_x = curr_mouse_x.get();
	mouse_y = curr_mouse_y.get();

	// Determine the movement deltas.

	if (enable_player_translation) {
		move_delta = curr_move_delta.get() * curr_move_rate.get() * UNITS_PER_BLOCK * elapsed_time;
		side_delta = curr_side_delta.get() * curr_move_rate.get() * UNITS_PER_BLOCK * elapsed_time;
	} else {
		move_delta = 0.0f;
		side_delta = 0.0f;
	}

	// Determine the turn and look deltas based on whether mouse look mode is enabled or not.

	if (mouse_look_mode.get()) {
		if (enable_player_rotation) {
			turn_delta = curr_turn_delta.get() * curr_turn_rate.get();
			look_delta = curr_look_delta.get() * curr_turn_rate.get();
		} else {
			turn_delta = 0.0f;
			look_delta = 0.0f;
		}
		curr_turn_delta.set(0.0f);
		curr_look_delta.set(0.0f);
	} else {
		if (enable_player_rotation) {
			turn_delta = curr_turn_delta.get() * curr_turn_rate.get() * DEGREES_PER_TURN_RATE * elapsed_time;
			look_delta = curr_look_delta.get() * curr_turn_rate.get() * DEGREES_PER_TURN_RATE * elapsed_time;
		} else {
			turn_delta = 0.0f;
			look_delta = 0.0f;
		}
	}

	// Set the master brightness and visible radius.

	set_master_intensity(master_brightness.get());
	old_visible_radius = visible_radius;
	visible_radius = (float)visible_block_radius.get() * UNITS_PER_BLOCK;

	// Remove the first key event off the queue, provided it isn't empty, and
	// update it's key down state.  Otherwise there is no current key event.

	raise_semaphore(key_event_semaphore);
	if (key_events > 0) {
		curr_key_event = key_event_queue[first_key_event];
		first_key_event = (first_key_event + 1) % MAX_KEY_EVENTS;
		key_events--;
		key_down_list[curr_key_event.key_code] = curr_key_event.key_down;
	} else
		curr_key_event.key_code = 0;
	lower_semaphore(key_event_semaphore);

	// If the visible radius has changed, reset the viewport.

	if (visible_radius != old_visible_radius) {
		set_viewport(window_width, window_height);
	}

	// Update the player's last position and current turn angle.

	if (turn_delta != 0.0f) {
		if (player_block_ptr != NULL) {
			turn_delta += last_delta;
			if (turn_delta < 1.0f && turn_delta > -1.0f)
				last_delta = turn_delta;
			else {
				last_delta = 0.0f;
				turn_delta = FROUND(turn_delta);
				player_block_ptr->rotate_y(turn_delta);
				player_viewpoint.last_position = player_viewpoint.position;
				player_viewpoint.turn_angle = pos_adjust_angle(player_viewpoint.turn_angle + turn_delta);
			}
		} else {
			player_viewpoint.last_position = player_viewpoint.position;
			player_viewpoint.turn_angle = pos_adjust_angle(player_viewpoint.turn_angle + turn_delta);
		}
	}

	// Adjust the look angle by the look delta.

	player_viewpoint.look_angle = neg_adjust_angle(player_viewpoint.look_angle + look_delta);
	if (FGT(player_viewpoint.look_angle, 90.0f))
		player_viewpoint.look_angle = 90.0f;
	else if (FLT(player_viewpoint.look_angle, -90.0f))
		player_viewpoint.look_angle = -90.0f;

	// Compute the normal and inverse of the player turn and look angles, in radians.

	player_viewpoint.turn_angle_radians = RAD(player_viewpoint.turn_angle);
	player_viewpoint.look_angle_radians = RAD(player_viewpoint.look_angle);
	player_viewpoint.inv_turn_angle_radians = RAD(360.0f - player_viewpoint.turn_angle);
	player_viewpoint.inv_look_angle_radians = RAD(360.0f - pos_adjust_angle(player_viewpoint.look_angle));

	// Set the trajectory based upon the move delta, side delta, and the turn angle.
	// Note there is no Y component in the trajectory unless fly mode is active, in
	// which case the look angle is used.

	trajectory.dx = move_delta * sinf(player_viewpoint.turn_angle_radians) +
		side_delta * sinf(RAD(player_viewpoint.turn_angle + 90.0f));
	if (fly_mode.get()) {
		trajectory.dy = move_delta * sinf(player_viewpoint.inv_look_angle_radians);
	} else {
		trajectory.dy = 0.0f;
	}
	trajectory.dz = move_delta * cosf(player_viewpoint.turn_angle_radians) +
		side_delta * cosf(RAD(player_viewpoint.turn_angle + 90.0f));

	// Adjust the trajectory to take in account collisions, then move the player along this trajectory.
	// Note that there is a maximum distance the player can move in order for collision detection to
	// be accurate, so it may be necessary to adjust the trajectory more than once.

	do {
		new_trajectory = adjust_trajectory(trajectory, elapsed_time);
		player_viewpoint.position = player_viewpoint.position + new_trajectory;
	} while (FNE(trajectory.dx, 0.0f) || FNE(trajectory.dz, 0.0f));

	// Set a flag indicating whether the viewpoint has changed.

	viewpoint_has_changed = FNE(turn_delta, 0.0f) || FNE(look_delta, 0.0f) ||
		player_viewpoint.position != player_viewpoint.last_position;

	// Update all lights and sounds.

	update_all_lights(elapsed_time);
	if (sound_on)
		update_all_sounds();

	// Move the player viewpoint to eye height, then create a vertex that is the end point
	// of a vector extending from the eye point along the direction of sight, and determine
	// which square it lands in, which becomes the builder square if its on the map.

	player_viewpoint.position.y += player_dimensions.y;
	vertex builder_vertex(0.0f, 0.0f, UNITS_PER_BLOCK * 2.0f);
	builder_vertex.rotate_x(player_viewpoint.look_angle);
	builder_vertex.rotate_y(player_viewpoint.turn_angle);
	builder_vertex += player_viewpoint.position;
	builder_vertex.get_map_position(&column, &row, &level);
	if (column >= 0 && column < world_ptr->columns && 
		row >= 0 && row < world_ptr->rows &&
		level >= (world_ptr->ground_level_exists? 1 : 0) && level < world_ptr->levels - 1) {
		builder_square_ptr = world_ptr->get_square_ptr(column, row, level);
	} else {
		builder_square_ptr = NULL;
	}

	// Render the frame and display the frame buffer.  The player viewpoint
	// needs to be at eye height for the duration of the render, so move it
	// back to floor level after the render is complete.

	render_frame();
	player_viewpoint.position.y -= player_dimensions.y;
	display_frame_buffer();
	frames_rendered++;

	// If there is a script currently executing, resume it.  Otherwise execute
	// the script at the head of the active script queue.  When a script
	// completes, it is removed from the head of the queue.

	if (script_executing) {
		if (!(script_executing = resume_script()) && 
			active_script_count != 0) {
			for (int j = 0; j < active_script_count; j++)
				active_script_list[j] = active_script_list[j + 1];
			active_script_count--;
		}
	} else if (active_script_count != 0) {
		if (!(script_executing = execute_script(active_script_list[0]->block_ptr, 
			active_script_list[0]->script_def_ptr))) {
			for (int j = 0; j < active_script_count; j++)
				active_script_list[j] = active_script_list[j + 1];
			active_script_count--;
		}
	}

	// Check for a mouse selection and a left and right mouse clicked event.

	check_for_mouse_selection();
	left_mouse_was_clicked = left_mouse_clicked.event_sent();
	right_mouse_was_clicked = right_mouse_clicked.event_sent();

	// Determine the location the player is standing on.

	player_viewpoint.position.get_map_position(&player_column, &player_row, &player_level);

	// If we are in build mode and a builder square is selected...

	if (build_mode.get() && builder_square_ptr) {
		block_def *block_def_ptr;

		// If the left mouse button was clicked and a block definition has been selected in the builder window...

		if (left_mouse_was_clicked && (block_def_ptr = selected_block_def_ptr.get()) != NULL) {

			// If the selected block definition is hidden by another block definition in the block symbol table, and there
			// is no exact custom duplicate available, then create one with an unused symbol assigned to it.  A create tag
			// for this exact custom duplicate also needs to be added to the spot entity list.

			word symbol = world_ptr->map_style == SINGLE_MAP ? block_def_ptr->single_symbol : block_def_ptr->double_symbol;
			if (block_symbol_table[symbol] != block_def_ptr) {
				if (block_def_ptr->custom_dup_block_def_ptr == NULL) {
					block_def_ptr = create_custom_dup_block_def(block_def_ptr);
				} else {
					block_def_ptr = block_def_ptr->custom_dup_block_def_ptr;
				}
			}

			// Replace the block on the builder square with the currently selected block definition, provided the square
			// doesn't have an entrance or the selected block definition allows an entrance.

			if (block_def_ptr && (!square_has_entrance(builder_square_ptr) || block_def_ptr->allow_entrance)) {
				remove_fixed_block(builder_square_ptr);
				add_fixed_block(block_def_ptr, builder_square_ptr, true);
			}
		} 
		
		// If the right mouse button was clicked, remove the block on the builder square.

		else if (right_mouse_was_clicked) {
			remove_fixed_block(builder_square_ptr);
		}
	}

	// Process the currently and previously selected squares, followed by
	// the currently and previously selected areas, followed by the list of
	// global triggers.

	process_prev_selected_square();
	process_curr_selected_square();
	process_prev_selected_area();
	process_curr_selected_area();

	// Process all global triggers in the global trigger list, and in all
	// movable and fixed blocks.

	process_global_triggers_in_trigger_list(global_trigger_list, NULL, false);
	process_global_triggers_in_block_list(movable_block_list);
	process_global_triggers_in_block_list(fixed_block_list);
	process_global_triggers_in_block_list(player_block_ptr);

	// Execute the list of active triggers.

	player_block_replaced = false;
	if (!execute_active_trigger_list())
		return(false);

	// Check if global timer is ready - if so then activate triggers tied 
	// to the master clock.

	if (curr_time_ms - clocktimer_time_ms > 50) {
		if (!execute_global_clock_action_list(curr_time_ms - clocktimer_time_ms))
			return(false);
		clocktimer_time_ms = curr_time_ms;
	}

	// If a polygon info requested event has been sent, display the number of
	// the currently selected polygon, and the name of the block definition it
	// belongs to.

#ifdef _DEBUG
	if (polygon_info_requested.event_sent()) {
		if (curr_selected_polygon_no > 0)
			information("Polygon info", "Pointing at polygon %d of block %s", 
				curr_selected_polygon_no, curr_selected_block_def_ptr->name);
		else
			information("Polygon info", "No polygon is being pointed at");
	}
#endif

	// If the mouse was clicked while NOT in build mode, and an exit was selected, handle it.

	if (left_mouse_was_clicked && !build_mode.get() && curr_selected_exit_ptr != NULL) {
		return(handle_exit(curr_selected_exit_ptr->URL, curr_selected_exit_ptr->target, curr_selected_exit_ptr->is_spot_URL, false));
	}

	// If we are standing on the same map square as the previous frame (which
	// is always true after teleporting), the block the player is standing on
	// hasn't been replaced, and the player wasn't teleported in the last 
	// frame, then we don't need to check whether there is an exit on this 
	// square.

	player_viewpoint.last_position.get_map_position(&last_column, &last_row, &last_level);
	if (player_column == last_column && player_row == last_row && 
		player_level == last_level && !player_block_replaced && 
		!player_was_teleported)
		return(true);

	// Reset the player was teleported flag.

	player_was_teleported = false;

	// Check whether the square the player is standing on has an exit, the
	// block on that square has an exit, or a movable block "on" this square 
	// has an exit; and the exit has a "step on" trigger.  If found, we must
	// handle that exit.  Note that an exit on the square takes precedence
	// over an exit on a block, and a fixed block takes precedence over a
	// movable one.

	square_ptr = world_ptr->get_square_ptr(player_column, player_row, player_level);
	exit_ptr = NULL;
	if (square_ptr != NULL) {
		if (square_ptr->exit_ptr != NULL && 
			(square_ptr->exit_ptr->trigger_flags & STEP_ON)) {
			exit_ptr = square_ptr->exit_ptr;
		} else if ((block_ptr = square_ptr->block_ptr) != NULL &&
			block_ptr->exit_ptr != NULL && 
			(block_ptr->exit_ptr->trigger_flags & STEP_ON)) {
			exit_ptr = block_ptr->exit_ptr;
		} else {
			block_ptr = movable_block_list;
			while (block_ptr != NULL) {
				block_ptr->translation.get_map_position(&column, &row, &level);
				if (column == player_column && row == player_row &&
					level == player_level && block_ptr->exit_ptr != NULL &&
					(block_ptr->exit_ptr->trigger_flags & STEP_ON)) {
					exit_ptr = block_ptr->exit_ptr;
					break;
				}
				block_ptr = block_ptr->next_block_ptr;
			}
		}
	}
	if (exit_ptr != NULL) {
		return(handle_exit(exit_ptr->URL, exit_ptr->target, exit_ptr->is_spot_URL, false));		
	}
	return(true);
}

//==============================================================================
// Main initialisation functions.
//==============================================================================

//------------------------------------------------------------------------------
// Destroy the sound buffers of the sounds in the given list.
//------------------------------------------------------------------------------

static void
destroy_sound_buffers_in_sound_list(sound *sound_list)
{
	sound *sound_ptr = sound_list;
	while (sound_ptr != NULL) {
		destroy_sound_buffer(sound_ptr);
		sound_ptr = sound_ptr->next_sound_ptr;
	}
}

//------------------------------------------------------------------------------
// Destroy the sound buffers of the sounds in the given block list.
//------------------------------------------------------------------------------

static void
destroy_sound_buffers_in_block_list(block *block_list)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->sound_list != NULL)
			destroy_sound_buffers_in_sound_list(block_ptr->sound_list);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Function to pause a spot in preparation for changing window modes.
//------------------------------------------------------------------------------

static void
pause_spot(void)
{
	// Log the stats for the current display mode.

	log_stats();

	// Stop all sounds.

	stop_all_sounds();

	// Destroy all sound buffers of sounds in global sound list, movable
	// block list, and fixed block list.

	destroy_sound_buffers_in_sound_list(global_sound_list);
	destroy_sound_buffers_in_block_list(movable_block_list);
	destroy_sound_buffers_in_block_list(fixed_block_list);

	// Destroy sound buffer of ambient sound.

	if (ambient_sound_ptr != NULL) {
		ambient_sound_ptr->in_range = false;
		destroy_sound_buffer(ambient_sound_ptr);
	}

#ifdef STREAMING_MEDIA

	// If a stream is set, stop the streaming thread.

	if (stream_set)
		stop_streaming_thread();

#endif
}

//------------------------------------------------------------------------------
// Create the display palette list for the popups in the given list.
//------------------------------------------------------------------------------

static void
create_palettes_in_popup_list(popup *popup_list)
{
	popup *popup_ptr;
	texture *texture_ptr;

	popup_ptr = popup_list;
	while (popup_ptr != NULL) {
		if ((texture_ptr = popup_ptr->bg_texture_ptr) != NULL &&
			texture_ptr->pixmap_list != NULL && texture_ptr->bytes_per_pixel == 1)
			texture_ptr->create_display_palette_list();
		if ((texture_ptr = popup_ptr->fg_texture_ptr) != NULL &&
			texture_ptr->bytes_per_pixel == 1)
			texture_ptr->create_display_palette_list();
		popup_ptr = popup_ptr->next_popup_ptr;
	}
}

//------------------------------------------------------------------------------
// Create the display palette list for the popups in the given block list.
//------------------------------------------------------------------------------

static void
create_palettes_in_blockset(blockset *blockset_ptr)
{
	block_def *block_def_ptr = blockset_ptr->block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->popup_list != NULL)
			create_palettes_in_popup_list(block_def_ptr->popup_list);
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
}

//------------------------------------------------------------------------------
// Create the sound buffers of the sounds in the given list.
//------------------------------------------------------------------------------

static void
create_sound_buffers_in_sound_list(sound *sound_list)
{
	sound *sound_ptr = sound_list;
	while (sound_ptr != NULL) {
		create_sound_buffer(sound_ptr);
		sound_ptr = sound_ptr->next_sound_ptr;
	}
}

//------------------------------------------------------------------------------
// Create the sound buffers of the sounds in the given block list.
//------------------------------------------------------------------------------

static void
create_sound_buffers_in_block_list(block *block_list)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->sound_list != NULL)
			create_sound_buffers_in_sound_list(block_ptr->sound_list);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Function to resume a spot after changing window modes.
//------------------------------------------------------------------------------

static void
resume_spot(void)
{
	blockset *blockset_ptr;
	texture *texture_ptr;

	// Recreate the palette list for every 8-bit texture.  Custom textures that
	// don't yet have pixmaps are ignored.

	blockset_ptr = blockset_list_ptr->first_blockset_ptr;
	while (blockset_ptr != NULL) {
		texture_ptr = blockset_ptr->first_texture_ptr;
		while (texture_ptr != NULL) {
			if (texture_ptr->bytes_per_pixel == 1) {
				if (hardware_acceleration)
					texture_ptr->create_texture_palette_list();
				else
					texture_ptr->create_display_palette_list();
			}
			texture_ptr = texture_ptr->next_texture_ptr;
		}
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	texture_ptr = custom_blockset_ptr->first_texture_ptr;
	while (texture_ptr != NULL) {
		if (texture_ptr->pixmap_list != NULL && texture_ptr->bytes_per_pixel == 1) {
			if (hardware_acceleration)
				texture_ptr->create_texture_palette_list();
			else
				texture_ptr->create_display_palette_list();
		}
		texture_ptr = texture_ptr->next_texture_ptr;
	}

	// Reinitialise the display palette lists for every popup.  Custom
	// background textures that don't yet have pixmaps are ignored.

	create_palettes_in_popup_list(global_popup_list);
	create_palettes_in_blockset(custom_blockset_ptr);

	// Recreate all sound buffers.  The sounds will restart automatically.

	create_sound_buffers_in_sound_list(global_sound_list);
	create_sound_buffers_in_block_list(movable_block_list);
	create_sound_buffers_in_block_list(fixed_block_list);
	if (ambient_sound_ptr != NULL)
		create_sound_buffer(ambient_sound_ptr);

#ifdef STREAMING_MEDIA

	// If a stream is set, start the streaming thread.

	if (stream_set)
		start_streaming_thread();

#endif

	// Reset the stats for the new display mode.

	start_time_ms = get_time_ms();
	frames_rendered = 0;
}

//------------------------------------------------------------------------------
// Global player initialisation.
//------------------------------------------------------------------------------

static bool
init_player(void)
{
	// Initialise the random number generator.

	srand((unsigned int)time(NULL));
	rand();

	// Initialise global variables.

	spot_URL_dir = "";
	old_blockset_list_ptr = NULL;
	blockset_list_ptr = NULL;
	last_refresh_time_ms = 0;
	refresh_count = 0;

	// Initialise key down list.

	for (int index = 0; index < 256; index++)
		key_down_list[index] = false;

	// Initialise the parser, renderer and collision detection code.

	init_parser();
	init_renderer();
	COL_init();

	// Load the cached blockset list; if this fails, create it.

	if (!load_cached_blockset_list())
		create_cached_blockset_list();
	return(true);
}

//------------------------------------------------------------------------------
// Global player shutdown.
//------------------------------------------------------------------------------

static void
shut_down_player(void)
{
	// Delete the cached blockset list and loaded blockset lists, and clean up
	// the renderer and collision detection code.

	delete_cached_blockset_list();
	if (blockset_list_ptr != NULL)
		DEL(blockset_list_ptr, blockset_list);
	if (old_blockset_list_ptr != NULL)
		DEL(old_blockset_list_ptr, blockset_list);
	clean_up_renderer();
	COL_exit();
}

//------------------------------------------------------------------------------
// Player window initialisation.
//------------------------------------------------------------------------------

static bool
init_player_window(void)
{
	// Reset the visible radius.

	visible_radius = (float)visible_block_radius.get() * UNITS_PER_BLOCK;

	// Initialise free span list and the span buffer.

	init_free_span_list();
	span_buffer_ptr = NULL;

	// Set the viewport based upon the window size and the
	// horizontal and vertical fields of view.

	set_viewport(window_width, window_height);

	// If hardware acceleration is not enabled, create the span buffer.

	if (!hardware_acceleration) {
		NEW(span_buffer_ptr, span_buffer);
		if (span_buffer_ptr == NULL ||
			!span_buffer_ptr->create_buffer(window_height)) {
			display_low_memory_error();
			return(false);
		}
	}

	// Create the image caches.

	return(create_image_caches());
}

//------------------------------------------------------------------------------
// Player window shutdown.
//------------------------------------------------------------------------------

static void
shut_down_player_window(void)
{
	// Delete the image caches.

	delete_image_caches();

	// Delete span buffer and free span list.

	if (span_buffer_ptr != NULL)
		DEL(span_buffer_ptr, span_buffer);
	delete_free_span_list();

	// Signal the plugin thread that the player window has been shut down.

	player_window_shut_down.send_event(true);
}

//------------------------------------------------------------------------------
// Handle a window mode change.
//------------------------------------------------------------------------------

static bool
handle_window_mode_change(void)
{
	// Pause the spot, shut down the player window, then wait for signal from
	// plugin thread that main window has been recreated.  If it hasn't,
	// exit with a failure status.

	pause_spot();
	shut_down_player_window();
	if (!main_window_created.wait_for_event()) {
		return(false);
	}

	// Initialise player window, resume spot, and signal the plugin thread that
	// the player window has been initialised or not.

	if (init_player_window()) {
		player_window_initialised.send_event(true);
		resume_spot();
		return(true);
	} else {
		player_window_initialised.send_event(false);
		return(false);
	}
}

//------------------------------------------------------------------------------
// Handle a window resize.
//------------------------------------------------------------------------------

static bool
handle_window_resize(void)
{
	// Shut down the player window, then wait for signal from plugin thread that
	// main window has been resized.

	shut_down_player_window();
	if (!main_window_resized.wait_for_event())
		return(false);

	// Initialise player window, and signal the plugin thread that the player
	// window has been initialised or not.

	if (init_player_window()) {
		if (hardware_acceleration) {
			hardware_update_fog_settings(global_fog_enabled, &global_fog, visible_radius);
		}
		player_window_initialised.send_event(true);
		return(true);
	} else {
		player_window_initialised.send_event(false);
		return(false);
	}
}

//------------------------------------------------------------------------------
// Refresh the player window.  We handle window resizes and mode changes here.
// If these fail, or a player window shutdown event is received, we generate a
// new player window shutdown event and return FALSE.  The caller is then
// responsible for stopping what they are doing and returning to the main event
// loop, where the player window shutdown can proceed.
//------------------------------------------------------------------------------

bool
refresh_player_window(void)
{
	if ((++refresh_count & 255) == 0) {
		int curr_time_ms = get_time_ms();
		if (curr_time_ms >= last_refresh_time_ms + 100) {
			last_refresh_time_ms = curr_time_ms;
			if (player_window_shutdown_requested.event_sent()) {
				player_window_shutdown_requested.send_event(true);
				return(false);
			}
			if (main_window_ready) {
				if (window_resize_requested.event_sent() && !handle_window_resize()) {
					player_window_shutdown_requested.send_event(true);
					return(false);
				}
				if (window_mode_change_requested.event_sent() &&
					!handle_window_mode_change()) {
					player_window_shutdown_requested.send_event(true);
					return(false);
				}
				if (!hardware_acceleration) {
					clear_frame_buffer(0, 0, window_width, window_height);
				}
				display_frame_buffer();
			}
		}
	}
	return(true);
}

//------------------------------------------------------------------------------
// Load the spot from the current URL.
//------------------------------------------------------------------------------

static bool
load_spot(void)
{
	// Wait for signal from plugin thread that spot URL is ready.  If the
	// URL failed to download, return a failure status.

	if (!URL_was_opened.wait_for_event() ||
		!URL_was_downloaded.wait_for_event())
		return(false);

	// Set the current URL and file path.

	curr_URL = downloaded_URL.get();
	curr_file_path = downloaded_file_path.get();

	// Decode the current URL and file path, to remove encoded characters.

	curr_URL = decode_URL(curr_URL);
	curr_file_path = decode_URL(curr_file_path);
	curr_file_path = URL_to_file_path(curr_file_path);

	// Set a flag indicating if the spot is on the web or not.

	spot_on_web = !_strnicmp(curr_URL, "http://", 7);

	// Start up the spot.  If this fails, shut down the spot and return a
	// failure status.

	if (!start_up_spot()) {
		shut_down_spot();
		return(false);
	}
	return(true);
}

//------------------------------------------------------------------------------
// Handle any spot events.  Returns TRUE if the event loop should continue,
// or FALSE otherwise.
//------------------------------------------------------------------------------

static bool
handle_spot_events(void)
{
	// If a pause event has been sent by the plugin thread, stop all
	// sounds and wait until either a resume event or player window
	// shutdown event is recieved before continuing (the latter will
	// only occur as the result of an inactive plugin window being
	// activated).

	if (spot_loaded.get() && pause_player_thread.event_sent()) {
		bool got_shutdown_request;

		stop_all_sounds();
		while (true) {
			if (resume_player_thread.event_sent()) {
				got_shutdown_request = false;
				break;
			}
			if (player_window_shutdown_requested.event_sent()) {
				got_shutdown_request = true;
				break;
			}
		}
		if (got_shutdown_request)
			return(false);
	}

#ifdef STREAMING_MEDIA

	// If a stream is set...

	if (stream_set) {

		// If the stream is ready, update the texture dependencies.

		if (stream_ready()) {
			if (unscaled_video_texture_ptr != NULL)
				update_texture_dependancies(unscaled_video_texture_ptr);
			video_texture *video_texture_ptr = 
				scaled_video_texture_list;
			while (video_texture_ptr != NULL) {
				update_texture_dependancies(
					video_texture_ptr->texture_ptr);
				video_texture_ptr = 
					video_texture_ptr->next_video_texture_ptr;
			}
		}

		// If a request to download Windows Media Player
		// has been recieved, launch the appropriate download page.

		if (download_of_wmp_requested())
			request_URL(WMP_DOWNLOAD_URL, NULL, "_blank");
	}

#endif

	// Render the next frame.

	if (!render_next_frame())
		return(false);

	// If a window mode change is requested, handle it.  If the mode
	// change fails, break out of the event loop.

	if (window_mode_change_requested.event_sent() &&
		!handle_window_mode_change())
		return(false);
	
	// If a window resize is requested, handle it.

	if (window_resize_requested.event_sent() && !handle_window_resize())
		return(false);

	// Indicate the event loop should continue.
	
	return(true);
}

//------------------------------------------------------------------------------
// Player thread.
//------------------------------------------------------------------------------

void
player_thread(void *arg_list)
{
	// Decrease the priority level on this thread, to ensure that the browser
	// and the rest of the system remains responsive.

	decrease_thread_priority();

	// Perform global initialisation and signal the plugin thread as to whether
	// it succeeded or failure.  If it failed, the player thread will exit.

	if (init_player())
		player_thread_initialised.send_event(true);
	else {
		player_thread_initialised.send_event(false);
		shut_down_player();
		return;
	}

	// Wait for signal from plugin thread that a new player window is to be
	// initialised.

	while (player_window_init_requested.wait_for_event()) {

		// Wait for signal from plugin thread that main window created succeeded
		// or failed.  If it failed, jump to the bottom of this loop.

		if (!main_window_created.wait_for_event())
			continue;

		// Initialise player window, and signal plugin thread that it succeeded
		// or failed.  If it failed, shut down the player window and jump to the
		// bottom of this loop.

		if (init_player_window())
			player_window_initialised.send_event(true);
		else {
			player_window_initialised.send_event(false);
			shut_down_player_window();
			continue;
		}

		// Attempt to load the spot that was passed in as the initial URL for
		// this plugin window.

		if (load_spot())
			spot_loaded.set(true);
		else {
			spot_loaded.set(false);
			set_title("No spot loaded");
		}

		// Run the event loop until plugin thread signals the player window to
		// shut down.

		do {

			// If there is no download in progress and there is a custom 
			// texture or wave to download, initiate the next download.

			if (curr_download_completed && (curr_custom_texture_ptr != NULL ||
				curr_custom_wave_ptr != NULL))
				initiate_next_download();

			// If a spot is loaded, handle all spot events.  Otherwise just
			// refresh the player window as required.

			if (spot_loaded.get()) {
				if (!handle_spot_events()) {
					shut_down_spot();
					spot_loaded.set(false);
					set_title("No spot loaded");
				}
			} else if (!refresh_player_window())
				continue;

			// If a spot load is requested, do it.

			if (spot_load_requested.event_sent()) {
				if (!handle_exit(spot_URL_to_load, NULL, true, true)) {
					shut_down_spot();
					spot_loaded.set(false);
					set_title("No spot loaded");
				}
			}

			// If the user has requested to save the 3DML source for this spot,
			// do so now.

			if (save_3DML_source_requested.event_sent())
				save_spot_file(saved_spot_file_path);

			// Handle the current download, if there is one.
	
			if (curr_custom_texture_ptr != NULL || curr_custom_wave_ptr != NULL)
				handle_current_download();

		} while (!player_window_shutdown_requested.event_sent());

		// Shut down the spot if it were loaded.

		if (spot_loaded.get()) {
			shut_down_spot();
			spot_loaded.set(false);
		}
		
		// Shut down the spot and the player window.

		shut_down_player_window();
	}

	shut_down_player();
}