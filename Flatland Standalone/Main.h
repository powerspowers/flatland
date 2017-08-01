//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------

// Minimum blockset update period, and time of last Rover and spot directory
// update.

extern int min_blockset_update_period;
extern time_t last_rover_update;
extern time_t last_spot_dir_update;

// Display dimensions and other related information.

extern float half_window_width;
extern float half_window_height;
extern float horz_field_of_view;
extern float vert_field_of_view;
extern float horz_scaling_factor;
extern float vert_scaling_factor;
extern float viewport_width;
extern float viewport_height;
extern float half_viewport_width;
extern float half_viewport_height;
extern float horz_pixels_per_degree;
extern float vert_pixels_per_degree;

// Debug levels, and warnings and low memory flags.

#define BE_SILENT					0
#define LET_SPOT_DECIDE				1
#define SHOW_ERRORS_ONLY			2
#define SHOW_ERRORS_AND_WARNINGS	3

extern int spot_debug_level;
extern bool debug_on;
extern bool warnings;
extern bool low_memory;

// Cached blockset list.

extern cached_blockset *cached_blockset_list;
extern cached_blockset *last_cached_blockset_ptr;

// Spot directory list.

extern spot_dir_entry *spot_dir_list;
extern spot_dir_entry *selected_spot_dir_entry_ptr;

// Recent spot list.

#define MAX_RECENT_SPOTS	15
extern recent_spot recent_spot_list[MAX_RECENT_SPOTS];
extern int recent_spots;
extern string selected_recent_spot_URL;
extern void *recent_spot_list_semaphore;

// URL directory of currently open spot, current spot URL without the
// entrance name, and a flag indicating if the spot URL is web-based.

extern string spot_URL_dir;
extern string curr_spot_URL;
extern bool spot_on_web;

// Minimum Rover version required for the current spot.

extern unsigned int min_rover_version;

// Spot title.

extern string spot_title;

// Visible radius, and frustum vertex list (in view space).

extern float visible_radius;
extern vertex frustum_vertex_list[FRUSTUM_VERTICES];

// Frame buffer pointer and width.

extern byte *frame_buffer_ptr;
extern int frame_buffer_width;

// Span buffer.

extern span_buffer *span_buffer_ptr;

// Pointer to old and current blockset list, and custom blockset.

extern blockset_list *old_blockset_list_ptr;
extern blockset_list *blockset_list_ptr;
extern blockset *custom_blockset_ptr;

// Metadata list.

extern metadata *first_metadata_ptr;
extern metadata *last_metadata_ptr;

// Load tag list.

extern load_tag *first_load_tag_ptr;
extern load_tag *last_load_tag_ptr;

// Symbol to block definition mapping table.

extern block_def *block_symbol_table[16384];

// Pointer to world object, movable block list and fixed block list.  Both of
// these block lists may have lights, sounds, popups, entrances, exits or
// triggers that need to be handled.

extern world *world_ptr;
extern block *movable_block_list;
extern block *fixed_block_list;

// Global list of entrances.

extern entrance *global_entrance_list;
extern entrance *last_global_entrance_ptr;

// List of imagemaps.

extern imagemap *imagemap_list;

// Global list of lights.

extern light *global_light_list;

// Orb direction, light, texture, brightness, size and exit.

extern direction orb_direction;
extern light *orb_light_ptr;
extern texture *orb_texture_ptr;
extern texture *custom_orb_texture_ptr;
extern float orb_brightness;
extern int orb_brightness_index;
extern float orb_width, orb_height;
extern float half_orb_width, half_orb_height;
extern hyperlink *orb_exit_ptr;

// Global sound list, and ambient sound.

extern sound *global_sound_list;
extern sound *ambient_sound_ptr;

// Global list of popups.

extern popup *global_popup_list;
extern popup *last_global_popup_ptr;

// Global list of triggers.

extern trigger *global_trigger_list;
extern trigger *last_global_trigger_ptr;

// Placeholder texture.

extern texture *placeholder_texture_ptr;

// Sky definition.

extern texture *sky_texture_ptr;
extern texture *custom_sky_texture_ptr;
extern RGBcolour unlit_sky_colour, sky_colour;
extern pixel sky_colour_pixel;
extern float sky_brightness;
extern int sky_brightness_index;

// Ground block definition.

extern block_def *ground_block_def_ptr;

// Player block symbol and pointer, camera offset and viewpoint.

extern word player_block_symbol;
extern block *player_block_ptr;
extern vector player_camera_offset;
extern viewpoint player_viewpoint, last_player_viewpoint;

//extern block *client_block_ptr;

// Player movement flags.

extern bool enable_player_translation;
extern bool enable_player_rotation;

// Player's dimensions, collision box, and step height.

extern vertex player_dimensions;
extern COL_AABOX player_collision_box;
extern float player_step_height;

// Start and current time, and number of frames rendered.

extern int start_time_ms, curr_time_ms;
extern int frames_rendered;

// Flag to turn the renderer on or off.

extern bool renderer_on;

// Current mouse position.

extern int mouse_x;
extern int mouse_y;

#ifdef _DEBUG

// Number of currently selected polygon, and pointer to the block definition
// it belongs to.

extern int curr_selected_polygon_no;
extern block_def *curr_selected_block_def_ptr;

#endif

// Previously and currently selected block (and square if the block is fixed).

extern block *prev_selected_block_ptr;
extern block *curr_selected_block_ptr;
extern square *prev_selected_square_ptr;
extern square *curr_selected_square_ptr;

// Previously and currently selected area and it's block and/or square.

extern area *prev_selected_area_ptr;
extern area *curr_selected_area_ptr;
extern block *prev_area_block_ptr;
extern block *curr_area_block_ptr;
extern square *prev_area_square_ptr;
extern square *curr_area_square_ptr;

//*mp* Previously and currently selected part
extern part *prev_selected_part_ptr;
extern part *curr_selected_part_ptr;

// Current popup block and square.

extern block *curr_popup_block_ptr;
extern square *curr_popup_square_ptr;

// Previously and currently selected exit.

extern hyperlink *prev_selected_exit_ptr;
extern hyperlink *curr_selected_exit_ptr;

// Current location that player is in, and flag indicating if the block the
// player is standing on was replaced.

extern int player_column, player_row, player_level;
extern bool player_block_replaced;

// Current URL and file path.

extern string curr_URL;
extern string curr_file_path;

// Flag indicating if the viewpoint has changed.

extern bool viewpoint_has_changed;

// Current custom texture and wave in the download pipeline, and a flag 
// indicating whether the current download has completed.

extern texture *curr_custom_texture_ptr;
extern wave *curr_custom_wave_ptr;
extern bool curr_download_completed;

// Flag indicating if error log file has been displayed.

extern bool	displayed_error_log_file;

#ifdef STREAMING_MEDIA

// Stream flag, name, and URLs for RealPlayer and Windows Media Player.

extern bool stream_set;
extern string name_of_stream;
extern string rp_stream_URL;
extern string wmp_stream_URL;

// Pointer to unscaled video texture (for popups) and scaled video texture
// list.

extern texture *unscaled_video_texture_ptr;
extern video_texture *scaled_video_texture_list;

#endif

// List of active triggers.

extern trigger *active_trigger_list[512];
extern int active_trigger_count;

// Queue of active scripts.

extern trigger *active_script_list[256];
extern int active_script_count;
extern bool script_executing;

// List of action tied to the global clock.

extern action *active_clock_action_list[256];
extern int active_clock_action_count;

// Frustum normal vector list, frustum plane offset list, 

extern vector frustum_normal_vector_list[FRUSTUM_PLANES];
extern float frustum_plane_offset_list[FRUSTUM_PLANES];

// Global fog data.

extern bool global_fog_enabled;
extern fog global_fog;

//------------------------------------------------------------------------------
// Externally visible functions.
//------------------------------------------------------------------------------

// Angle functions.

float
neg_adjust_angle(float angle);

float
pos_adjust_angle(float angle);

float
clamp_angle(float angle, float min_angle, float max_angle);

// Sound function.

void
stop_sounds_in_sound_list(sound *sound_list);

// Exit function
bool
handle_exit(const char *exit_URL, const char *exit_target, bool is_spot_URL);

// Map function.

block_def *
symbol_to_block_def(word symbol);

// Refresh player window function.

bool
refresh_player_window(void);

// Main entry point to player thread.

void
player_thread(void *arg_list);

// Execute a trigger.

bool
execute_trigger(trigger* trigger_ptr);