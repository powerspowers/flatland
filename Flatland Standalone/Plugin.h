//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

// Web browser IDs.

#define	UNKNOWN_BROWSER		0
#define	NAVIGATOR			1
#define INTERNET_EXPLORER	2
#define OPERA				3
#define ACTIVEX_CONTROL		4

// Default view radius.

#define DEFAULT_VIEW_RADIUS			50

// Minimum, maximum and default move rates (in units of blocks).

#define MIN_MOVE_RATE				1
#define MAX_MOVE_RATE				8
#define DEFAULT_MOVE_RATE			3

// Minimum, maximum and default turn rates (in units of 45 degrees for classic movement, and degrees per mouse move for new movement).

#define MIN_TURN_RATE				1
#define MAX_TURN_RATE				8
#define DEFAULT_TURN_RATE			3
#define DEGREES_PER_TURN_RATE		45.0f

// Important directories and file paths.

extern string flatland_dir;
extern string log_file_path;
extern string error_log_file_path;
extern string prev_error_log_file_path;
extern string config_file_path;
extern string version_file_path;
extern string curr_spot_file_path;
extern string cache_file_path;
extern string new_rover_file_path;

// Hardware acceleration flag.

extern bool hardware_acceleration;

// Variables used to request a URL.

extern string requested_URL;
extern string requested_file_path;
extern string requested_target;
extern bool requested_no_cache;
extern string requested_blockset_name;

// Downloaded URL and file path.

extern semaphore<string> downloaded_URL;
extern semaphore<string> downloaded_file_path;

// Events sent by player thread.

extern event player_thread_initialised;
extern event player_window_initialised;
extern event URL_download_requested;
extern event URL_cancel_requested;
extern event player_window_shut_down;
extern event display_error;
extern event main_window_created;
extern event main_window_resized;
extern event URL_was_opened;
extern event URL_was_downloaded;
extern event window_mode_change_requested;
extern event window_resize_requested;
extern event left_mouse_clicked;
extern event right_mouse_clicked;
extern event player_window_shutdown_requested;
extern event player_window_init_requested;
extern event downloader_thread_termination_requested;
extern event pause_player_thread;
extern event resume_player_thread;
extern event spot_load_requested;
extern event save_3DML_source_requested;
extern event cached_blockset_load_requested;
extern event cached_blockset_load_completed;
extern event block_palette_entry_selected;
#ifdef _DEBUG
extern event polygon_info_requested;
#endif

// Global variables that require synchronised access, and the semaphores that
// protect them.

extern semaphore<bool> force_software_rendering;
extern semaphore<bool> spot_loaded;
extern semaphore<bool> selection_active;
extern semaphore<bool> mouse_look_mode;
extern semaphore<int> curr_mouse_x;
extern semaphore<int> curr_mouse_y;
extern semaphore<float> curr_move_delta;
extern semaphore<float> curr_side_delta;
extern semaphore<float> curr_turn_delta;
extern semaphore<float> curr_look_delta;
extern semaphore<float> curr_jump_delta;
extern semaphore<int> curr_move_rate;
extern semaphore<int> curr_turn_rate;
extern semaphore<float> master_brightness;
extern semaphore<bool> use_classic_controls;
extern semaphore<int> visible_block_radius;
extern semaphore<bool> fly_mode;
extern semaphore<bool> build_mode;
extern semaphore<block_def *> selected_block_def_ptr;
extern semaphore<block_def *> block_palette_list[10];
extern semaphore<block_def *> placeable_block_def_ptr;

// Saved spot file path.

extern char *saved_spot_file_path;

// Snapshot size, position and file path.

extern int snapshot_width, snapshot_height;
extern int snapshot_position;
extern const char *snapshot_file_path;

// Key functions.

#define MOVE_FORWARD		0
#define MOVE_BACK			1
#define MOVE_LEFT			2
#define MOVE_RIGHT			3
#define LOOK_UP				4
#define LOOK_DOWN			5
#define SIDLE_MODE			6
#define FAST_MODE			7
#define GO_FASTER			8
#define GO_SLOWER			9
#define JUMP				10

// Entry points called by the application.

int
run_app(void *instance_handle, int show_command, char *spot_file_path);

void
show_options_window();

void
show_light_window();

// Functions to display web page.

void
display_file_as_web_page(const char *file_path);

void
display_web_page(const char *URL);

// Key event structure.

struct key_event {
	bool key_down;
	int key_code;
};

// Key event queue.  Access must be synchronised.

#define MAX_KEY_EVENTS	10
extern key_event key_event_queue[MAX_KEY_EVENTS];
extern int first_key_event;
extern int last_key_event;
extern int key_events;
extern void *key_event_semaphore;
