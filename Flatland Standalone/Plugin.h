//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

// Web browser IDs.

#define	UNKNOWN_BROWSER		0
#define	NAVIGATOR			1
#define INTERNET_EXPLORER	2
#define OPERA				3
#define ACTIVEX_CONTROL		4

// Acceleration modes.

#define TRY_HARDWARE		0
#define TRY_SOFTWARE		1

// Minimum, maximum and default move rates (world units per second), and the
// delta move rate.

#define MIN_MOVE_RATE				3.2f
#define MAX_MOVE_RATE				12.8f
#define DEFAULT_MOVE_RATE			9.6f
#define DELTA_MOVE_RATE				0.8f

// Minimum, maximum and default rotation rates (degrees per second), and the
// delta rotation rate.

#define MIN_ROTATE_RATE				30.0f
#define MAX_ROTATE_RATE				60.0f
#define DEFAULT_ROTATE_RATE			45.0f
#define DELTA_ROTATE_RATE			10.0f

// Important directories and file paths.

extern string flatland_dir;
extern string log_file_path;
extern string error_log_file_path;
extern string config_file_path;
extern string version_file_path;
extern string directory_file_path;
extern string new_directory_file_path;
extern string recent_spots_file_path;
extern string curr_spot_file_path;
extern string cache_file_path;
extern string new_rover_file_path;

// Acceleration mode and hardware acceleration flag.

extern int acceleration_mode;
extern bool hardware_acceleration;

// Variables used to request a URL.

extern string requested_URL;
extern string requested_target;
extern string requested_file_path;
extern string requested_blockset_name;

// Downloaded URL and file path.

extern semaphore<string> downloaded_URL;
extern semaphore<string> downloaded_file_path;

// Javascript URL.

extern string javascript_URL;

// Web browser ID and version.

extern int web_browser_ID;
extern string web_browser_version;

// Events sent by player thread.

extern event player_thread_initialised;
extern event player_window_initialised;
extern event URL_download_requested;
extern event URL_cancel_requested;
extern event javascript_URL_download_requested;
extern event player_window_shut_down;
extern event show_spot_directory;

// Display error event.

extern event display_error;
extern bool show_error_log;

// Events sent by plugin thread.

extern event main_window_created;
extern event main_window_resized;
extern event URL_was_opened;
extern event URL_was_downloaded;
extern event window_mode_change_requested;
extern event window_resize_requested;
extern event mouse_clicked;
extern event player_window_shutdown_requested;
extern event player_window_init_requested;
extern event pause_player_thread;
extern event resume_player_thread;
extern event spot_dir_entry_selected;
extern event recent_spot_selected;
extern event check_for_update_requested;
extern event registration_requested;
extern event save_3DML_source_requested;
extern event snapshot_requested;
#ifdef _DEBUG
extern event polygon_info_requested;
#endif

// Global variables that require synchronised access, and the semaphores that
// protect them.

extern semaphore<int> user_debug_level;
extern semaphore<bool> spot_loaded;
extern semaphore<bool> selection_active;
extern semaphore<bool> absolute_motion;
extern semaphore<int> curr_mouse_x;
extern semaphore<int> curr_mouse_y;
extern semaphore<float> curr_move_delta;
extern semaphore<float> curr_side_delta;
extern semaphore<float> curr_turn_delta;
extern semaphore<float> curr_look_delta;
extern semaphore<float> curr_jump_delta;
extern semaphore<float> curr_move_rate;
extern semaphore<float> curr_rotate_rate;
extern semaphore<float> master_brightness;
extern semaphore<bool> download_sounds;
extern semaphore<int> visible_block_radius;
extern semaphore<bool> snapshot_in_progress;

// Saved spot file path.

extern const char *saved_spot_file_path;

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

bool
init_flatland();

bool
create_player_window();

void
open_local_file(char *file_path);

void
show_options_window();

// Function to display web page.

void
display_web_page(const char *URL);

// Function to set and get the key codes for a specific key function (called by
// the player thread only).

extern void
set_key_codes(int key_func, int *key_codes);

extern void
get_key_codes(int key_func, int *key_codes);

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
