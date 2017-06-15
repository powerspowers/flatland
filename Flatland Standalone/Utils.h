//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

// Current load index for textures and waves.

extern int curr_load_index;

// Flag indicating whether current URL has been opened.

extern bool curr_URL_opened;

// Externally visible functions.

block_def *
get_block_def(char block_symbol);

block_def *
get_block_def(word block_symbol);

block_def *
get_block_def(const char *block_identifier);

texture *
load_texture(blockset *blockset_ptr, char *texture_URL, bool add_to_blockset,
			 bool unlimited_size);

#ifdef STREAMING_MEDIA

char *
create_stream_URL(string stream_URL);

texture *
create_video_texture(char *name, video_rect *rect_ptr, bool unlimited_size);

#endif

bool
load_wave_file(const char *URL, const char *file_path, wave *wave_ptr);

wave *
load_wave(blockset *blockset_ptr, char *wave_URL);

entrance *
find_entrance(const char *name);

imagemap *
find_imagemap(const char *name);

imagemap *
add_imagemap(const char *name);

bool
check_for_popup_selection(popup *popup_ptr, int popup_width, int popup_height);

void
add_trigger_to_global_list(trigger *trigger_ptr, int column, int row, int level);

void
init_global_trigger(trigger *trigger_ptr);

void
compute_light_list_bounding_box(light *light_list, vertex translation,
								int &min_column, int &min_row, int &min_level,
								int &max_column, int &max_row, int &max_level);

void
reset_active_lights(int min_column, int min_row, int min_level, 
					int max_column, int max_row, int max_level);

void
set_trigger_delay(trigger *trigger_ptr, int curr_time_ms);

int
compute_active_polygons(block *block_ptr, int column, int row, int level,
						bool check_all_sides);

void
reset_active_polygons(int column, int row, int level);

void
add_clock_action(action *action_ptr);

void
remove_clock_action(action *action_ptr);

void
remove_clock_action_by_block(block *block_ptr);

void
remove_clock_action_by_type(action *action_ptr, int type);

block *
add_fixed_block(block_def *block_def_ptr, square *square_ptr, 
				bool update_active_polygons);

block *
add_movable_block(block_def *block_def_ptr, vertex translation);

void
remove_fixed_block(square *square_ptr);

void
remove_movable_block(block *block_ptr);

int
get_brightness_index(float brightness);

void
set_player_size(void);

bool
create_player_block(void);

block *
create_new_block(block_def *block_def_ptr, square *square_ptr, 
				 vertex translation);

void
create_sprite_polygon(block_def *block_def_ptr, part *part_ptr);

void
init_spot(void);

void
request_URL(const char *URL, const char *file_path, const char *target);

bool
download_URL(const char *URL, const char *file_path);

void
update_texture_dependancies(texture *custom_texture_ptr);

void
update_wave_dependancies(wave *wave_ptr);

void
initiate_first_download(void);

void
initiate_next_download(void);

void
handle_current_download(void);

light *
find_light(light *light_list, const char *light_name);

sound *
find_sound(sound *sound_list, const char *sound_name);

popup *
find_popup(popup *popup_list, const char *popup_name);