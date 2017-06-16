//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <direct.h>
#include "Classes.h"
#include "Plugin\npapi.h"
#include "Fileio.h"
#include "Image.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "resource.h"
//POWERS #include "SimKin.h"
#include "Spans.h"


// Structure used to hold a plugin instance's data.

struct InstData {
	bool		active_instance;	// TRUE if this is the active instance.
	NPP			instance_ptr;		// Pointer to instance.
	NPWindow	*window_ptr;		// Pointer to window.
	string		spot_URL;			// Original spot URL.
	string		window_text;		// Inactive window text.

	InstData();
	~InstData();
};

// Pointer to active plugin instance's data.

static InstData *active_inst_data_ptr;

// Important directories and file paths.

string flatland_dir;
string log_file_path;
string error_log_file_path;
string config_file_path;
string version_file_path;
string directory_file_path;
string new_directory_file_path;
string recent_spots_file_path;
string curr_spot_file_path;
string cache_file_path;
string new_rover_file_path;

// Parent window handle.

static HWND parent_window_handle;

// Player thread handle.

static unsigned long player_thread_handle;

// Acceleration mode and hardware acceleration flag.

int acceleration_mode;
bool hardware_acceleration;

// Variables used to request a URL.

string requested_URL;
string requested_target;
string requested_file_path;
string requested_blockset_name;			// Set if requesting a blockset.
string javascript_URL;					// Set if requesting a javascript URL.

// Pointers to the current stream being downloaded.  Note that we don't support
// multiple streams other than overlapping javascript streams, whose data is
// ignored in any case.

static NPStream *curr_stream_ptr;

// Downloaded URL and file path.

semaphore<string> downloaded_URL;
semaphore<string> downloaded_file_path;

// Web browser ID and version.

int web_browser_ID;
string web_browser_version;

// Events sent by player thread.

event player_thread_initialised;
event player_window_initialised;
event URL_download_requested;
event URL_cancel_requested;
event javascript_URL_download_requested;
event player_window_shut_down;
event show_spot_directory;

// Display error event.

event display_error;
bool show_error_log;

// Events sent by plugin thread.

event main_window_created;
event main_window_resized;
event URL_was_opened;
event URL_was_downloaded;
event window_mode_change_requested;
event window_resize_requested;
event mouse_clicked;
event player_window_shutdown_requested;
event player_window_init_requested;
event pause_player_thread;
event resume_player_thread;
event spot_dir_entry_selected;
event recent_spot_selected;
event check_for_update_requested;
event registration_requested;
event save_3DML_source_requested;
event snapshot_requested;
#ifdef _DEBUG
event polygon_info_requested;
#endif

// Global variables that require synchronised access, and the semaphores that
// protect them.

semaphore<int> user_debug_level;
semaphore<bool> spot_loaded;
semaphore<bool> selection_active;
semaphore<bool> absolute_motion;
semaphore<int> curr_mouse_x;
semaphore<int> curr_mouse_y;
semaphore<float> curr_move_delta;
semaphore<float> curr_side_delta;
semaphore<float> curr_turn_delta;
semaphore<float> curr_look_delta;
semaphore<float> curr_jump_delta;
semaphore<float> curr_move_rate;
semaphore<float> curr_rotate_rate;
semaphore<float> master_brightness;
semaphore<bool> download_sounds;
semaphore<int> visible_block_radius;
semaphore<bool> snapshot_in_progress;

// Saved spot file path.

const char *saved_spot_file_path;

// Snapshot size, position and file path.

int snapshot_width, snapshot_height;
int snapshot_position;
const char *snapshot_file_path;

// Chat message text
char chat_message[256];

// Key event queue, and the semaphore that protects it.

key_event key_event_queue[MAX_KEY_EVENTS];
int first_key_event;
int last_key_event;
int key_events;
void *key_event_semaphore;

// Size of movement border in window.

#define MOVEMENT_BORDER	32

// Mouse cursor variables.

static int last_x, last_y;
static arrow movement_arrow;
static bool selection_active_flag;
static bool inside_3D_window;
static bool disable_cursor_changes;

// Miscellaneous local variables.

static bool player_active;
static bool player_window_created;
static bool browser_window_minimised;
static float prev_move_rate, prev_rotate_rate;
static bool sidle_mode_enabled;
static bool fast_mode_enabled;
static int curr_button_status;
static bool movement_enabled;
static bool download_sounds_flag;
static bool hardware_acceleration_flag;
static int old_visible_block_radius;
static int old_user_debug_level;
static float window_half_width;
static float window_top_half_height, window_bottom_half_height;

// Various URLs.

#define FLATLAND_URL		"http://www.flatland.com"

// Key code to function structure.

struct key_code_to_func {
	byte key_code;
	byte alt_key_code;
	void (*func_ptr)(bool key_down);
};

// Forward declaration of key functions.

static void move_forward(bool key_down);
static void move_back(bool key_down);
static void move_left(bool key_down);
static void move_right(bool key_down);
static void look_up(bool key_down);
static void look_down(bool key_down);
static void go_faster(bool key_down);
static void go_slower(bool key_down);
static void sidle_mode(bool key_down);
static void fast_mode(bool key_down);
static void jump(bool key_down);

// Key code to function table.

#define KEY_FUNCTIONS 11
static key_code_to_func key_func_table[KEY_FUNCTIONS] = {
	{UP_KEY, 0, move_forward},
	{DOWN_KEY, 0, move_back},
	{LEFT_KEY, 0, move_left},
	{RIGHT_KEY, 0, move_right},
	{'A', 0, look_up},
	{'Z', 'Y', look_down},
	{SHIFT_KEY, 0, sidle_mode},
	{CONTROL_KEY, 0, fast_mode},
	{NUMPAD_ADD_KEY, 0, go_faster},
	{NUMPAD_SUBTRACT_KEY, 0, go_slower},
	{'J', 0, jump}
};

// Key function set event, and related data that requires synchronised access.

static event set_key_function;
static event get_key_function;
static event key_function_request_completed;
static int key_function;
static byte key_code;
static byte alt_key_code;

// Flag indicating if Rover started up successfully.

static bool rover_started_up;

//==============================================================================
// InstData class.
//==============================================================================

// Default constructor initialises all fields.

InstData::InstData()
{
	active_instance = false;
	instance_ptr = NULL;
	window_ptr = NULL;
}

// Default destructor does nothing.

InstData::~InstData()
{
}

//==============================================================================
// Player window functions.
//==============================================================================

// Forward declaration of callback functions.

static void
key_event_callback(byte key_code, bool key_down);

static void
timer_event_callback(void);

static void
mouse_event_callback(int x, int y, int button_code, int task_bar_button_code);

static void
resize_event_callback(void *window_handle, int width, int height);

static void
display_event_callback(void);

//------------------------------------------------------------------------------
// Create and the player window.
//------------------------------------------------------------------------------

bool
create_player_window()
{
	RECT window_rect;

	// Tell the player thread we're about to create the player window.

	player_window_init_requested.send_event(true);

	// Determine the size of the parent window.

	GetClientRect(parent_window_handle, &window_rect);

	// Create the main window.  If this fails, signal the player thread of
	// failure.

	if (!create_main_window(parent_window_handle, window_rect.right,
		window_rect.bottom, key_event_callback, mouse_event_callback,
		timer_event_callback, resize_event_callback, display_event_callback)) {
		destroy_main_window();
		main_window_created.send_event(false);
		return(false);
	}

	// Otherwise...

	else {
	
		// Compute the window "centre" coordinates (the point around which the
		// movement arrows are determined).

		window_half_width = window_width * 0.5f;
		window_top_half_height = window_height * 0.75f;
		window_bottom_half_height = window_height * 0.25f;

		// Signal the player thread that the main window was created,
		// and wait for the player thread to signal that the player window was
		// initialised.  If this failed, wait for the player thread to signal
		// that the player window was shut down, then destroy the main window.

		main_window_created.send_event(true);
		if (!player_window_initialised.wait_for_event()) {
			player_window_shut_down.wait_for_event();
			destroy_main_window();
			return(false);
		}
		player_window_created = true;
		return(true);
	}
}

void
open_local_file(char *file_path)
{
	URL_was_opened.send_event(true);
	downloaded_URL.set(file_path);
	downloaded_file_path.set(file_path);
	URL_was_downloaded.send_event(true);
}

//------------------------------------------------------------------------------
// Destroy the player window.
//------------------------------------------------------------------------------

static void
destroy_player_window(void)
{
	// Send the sequence of signals required to ensure that the player thread
	// has shut down the player window, and wait for the player thread to 
	// confirm this has happened.

	main_window_created.send_event(false);
	player_window_shutdown_requested.send_event(true);
	player_window_shut_down.wait_for_event();

	// Destroy the main window, then reset all signals in case the player thread
	// didn't see some of them.

	destroy_main_window();
	main_window_created.reset_event();
	URL_was_downloaded.reset_event();
	player_window_shutdown_requested.reset_event();
	player_window_created = false;
}

//------------------------------------------------------------------------------
// Function to toggle the window mode between accelerated and non-accelerated
// graphics.
//------------------------------------------------------------------------------

static void
toggle_window_mode(void)
{
	// Signal the player thread that a window mode change is requested, and
	// wait for the player thread to signal that the player window has shut
	// down.

	window_mode_change_requested.send_event(true);
	player_window_shut_down.wait_for_event();

	// Destroy the main window.

	destroy_main_window();

	// Toggle the hardware acceleration mode.

	hardware_acceleration = !hardware_acceleration;

	// Create the player window.  If this fails, make the active instance
	// inactive.

	if (!create_player_window()) {
		active_inst_data_ptr->active_instance = false;
		active_inst_data_ptr = NULL;
	}

	// Reset the title, and the label if there is one.

	set_title(NULL);
	show_label(NULL);
}

//==============================================================================
// Miscellaneous functions.
//==============================================================================

//------------------------------------------------------------------------------
// Display a file as a web page in a new browser window.
//------------------------------------------------------------------------------

static void
display_file_as_web_page(const char *file_path)
{
	FILE *fp;
	string URL;

	// If there is no file, there is nothing to display.

	if ((fp = fopen(file_path, "r")) == NULL)
		return;
	fclose(fp);

	// Construct the URL to the file.

	switch (web_browser_ID) {
	case NAVIGATOR:
		URL = "file:///";
		break;
	case OPERA:
		URL = "file://localhost/";
		break;
	default:
		URL = "file://";
	}
	URL += file_path;
	if (web_browser_ID == NAVIGATOR) {
		URL = encode_URL(URL);
		if (URL[9] == ':')
			URL[9] = '|';
	}

	// Ask the browser to load this URL into a new window.

	NPN_GetURL(active_inst_data_ptr->instance_ptr, URL, "_blank");
}

//------------------------------------------------------------------------------
// Display a web page.
//------------------------------------------------------------------------------

void
display_web_page(const char *URL)
{
	NPN_GetURL(active_inst_data_ptr->instance_ptr, URL, "_blank");
}

//------------------------------------------------------------------------------
// Update the mouse cursor.
//------------------------------------------------------------------------------

static void
update_mouse_cursor(int x, int y)
{
	float delta_x, delta_y, angle;

	// Set the current mouse position, and get the value of the selection active
	// flag.

	curr_mouse_x.set(x);
	curr_mouse_y.set(y);
	selection_active_flag = selection_active.get();

	// Determine the current movement arrow based upon the angle between the
	// "centre" of the main window and the current mouse position.

	delta_x = ((float)x - window_half_width) / window_half_width;
	delta_y = window_top_half_height - (float)y;
	if (FGT(delta_y, 0.0f))
		delta_y /= window_top_half_height;
	else
		delta_y /= window_bottom_half_height;
	if (FEQ(delta_x, 0.0f)) {
		if (FGT(delta_y, 0.0f))
			angle = 0.0f;
		else
			angle = 180.0f;
	} else {
		angle = (float)DEG(atan(delta_y / delta_x));
		if (FGT(delta_x, 0.0f))
			angle = 90.0f - angle;
		else
			angle = 270.0f - angle;
	}
	if (FGE(angle, 325.0f) || FLT(angle, 35.0f))
		movement_arrow = ARROW_N;
	else if (FLT(angle, 50.0f))
		movement_arrow = ARROW_NE;
	else if (FLT(angle, 125.0f))
		movement_arrow = ARROW_E;
	else if (FLT(angle, 140.0f))
		movement_arrow = ARROW_SE;
	else if (FLT(angle, 220.0f))
		movement_arrow = ARROW_S;
	else if (FLT(angle, 235.0f))
		movement_arrow = ARROW_SW;
	else if (FLT(angle, 310.0f))
		movement_arrow = ARROW_W;
	else
		movement_arrow = ARROW_NW;

	// Determine whether or not the mouse is inside the 3D window.

	inside_3D_window = x >= 0 && x < window_width && y >= 0 &&
		y < window_height;
}

//------------------------------------------------------------------------------
// Set the mouse cursor.
//------------------------------------------------------------------------------

static void
set_mouse_cursor(void)
{
	// If mouse cursor changes have been disabled, do nothing.

	if (disable_cursor_changes)
		return;

	// If a spot is not loaded, set the arrow cursor.

	if (!spot_loaded.get()) {
		set_arrow_cursor();
		return;
	}

	// Otherwise...

	switch (curr_button_status) {

	// If both buttons are up, set the cursor according to what the mouse is
	// pointing at.

	case MOUSE_MOVE_ONLY:
		if (!inside_3D_window)
			set_arrow_cursor();
		else if (selection_active_flag)
			set_hand_cursor();
		else 
			set_movement_cursor(movement_arrow);
		break;

	// If the left moust button is down, set the mouse cursor according to where
	// it is pointing and whether movement is enabled.

	case LEFT_BUTTON_DOWN:
		if (!inside_3D_window)
			set_arrow_cursor();
		else if (selection_active_flag && !movement_enabled)
			set_hand_cursor();
		else
			set_movement_cursor(movement_arrow);
	}
}

//------------------------------------------------------------------------------
// Launch a builder web page.
//------------------------------------------------------------------------------

static void
launch_builder_web_page(const char *URL)
{
	if (URL != NULL)
		NPN_GetURL(active_inst_data_ptr->instance_ptr, URL, "_blank");
}

//------------------------------------------------------------------------------
// Update the move, look and turn rates.
//------------------------------------------------------------------------------

static void
update_motion_rates(int rate_dir)
{
	float new_move_rate = curr_move_rate.get() + rate_dir * DELTA_MOVE_RATE;
	if (new_move_rate < MIN_MOVE_RATE)
		new_move_rate = MIN_MOVE_RATE;
	else if (new_move_rate > MAX_MOVE_RATE)
		new_move_rate = MAX_MOVE_RATE;
	curr_move_rate.set(new_move_rate);

	float new_rotate_rate = curr_rotate_rate.get() + rate_dir * 
		DELTA_ROTATE_RATE;
	if (new_rotate_rate < MIN_ROTATE_RATE)
		new_rotate_rate = MIN_ROTATE_RATE;
	else if (new_rotate_rate > MAX_ROTATE_RATE)
		new_rotate_rate = MAX_ROTATE_RATE;
	curr_rotate_rate.set(new_rotate_rate);
}

//------------------------------------------------------------------------------
// Display the spot directory.
//------------------------------------------------------------------------------

static void
display_spot_directory(void)
{
	disable_cursor_changes = true;
	set_arrow_cursor();
	open_directory_menu(spot_dir_list);
	if ((selected_spot_dir_entry_ptr = track_directory_menu()) != NULL)
		spot_dir_entry_selected.send_event(true);
	close_directory_menu();
	disable_cursor_changes = false;
}

//==============================================================================
// Key functions.
//==============================================================================

//------------------------------------------------------------------------------
// Set the key codes for a given key function.
//------------------------------------------------------------------------------

void
set_key_codes(int key_func, int *key_codes)
{
	key_function = key_func;
	key_code = key_codes[0];
	alt_key_code = key_codes[1];
	set_key_function.send_event(true);
	key_function_request_completed.wait_for_event();
}

//------------------------------------------------------------------------------
// Get the key codes for a given key function.
//------------------------------------------------------------------------------

void
get_key_codes(int key_func, int *key_codes)
{
	key_function = key_func;
	get_key_function.send_event(true);
	key_function_request_completed.wait_for_event();
	key_codes[0] = key_code;
	key_codes[1] = alt_key_code;
}

//------------------------------------------------------------------------------
// Start or stop forward motion.
//------------------------------------------------------------------------------

static void
move_forward(bool key_down)
{
	if (key_down) {
		curr_move_delta.set(1.0f);
		if (player_block_ptr != NULL) {
			if (player_block_ptr->current_loop == -1) {
			player_block_ptr->current_loop = 1;
			player_block_ptr->current_repeat = true;
			} 
		}
	}
	else {
		curr_move_delta.set(0.0f);
		if (player_block_ptr != NULL) {
		player_block_ptr->current_repeat = false;
		player_block_ptr->next_repeat = false;
		player_block_ptr->next_loop = -1;
		}
	}
}

//------------------------------------------------------------------------------
// Start jump motion.
//------------------------------------------------------------------------------

static void
jump(bool key_down)
{
	if (key_down ) { //&& curr_jump_delta.get() <= (float)0.0) {
		curr_jump_delta.set((float)1.5);
	}
}

//------------------------------------------------------------------------------
// Start or stop backward motion.
//------------------------------------------------------------------------------

static void
move_back(bool key_down)
{
	if (key_down)
		curr_move_delta.set(-1.0f);
	else
		curr_move_delta.set(0.0f);
}

//------------------------------------------------------------------------------
// Starts or stop anti-clockwise turn, or left sidle.
//------------------------------------------------------------------------------

static void
move_left(bool key_down)
{
	if (key_down) {
		if (sidle_mode_enabled) {
			curr_side_delta.set(-1.0f);
			curr_turn_delta.set(0.0f);
		} else {
			curr_side_delta.set(0.0f);
			curr_turn_delta.set(-1.0f);
		}
	} else {
		curr_side_delta.set(0.0f);
		curr_turn_delta.set(0.0f);
	}
}

//------------------------------------------------------------------------------
// Start or stop clockwise turn, or right sidle.
//------------------------------------------------------------------------------

static void
move_right(bool key_down)
{
	if (key_down) {
		if (sidle_mode_enabled) {
			curr_side_delta.set(1.0f);
			curr_turn_delta.set(0.0f);
		} else {
			curr_side_delta.set(0.0f);
			curr_turn_delta.set(1.0f);
		}
	} else {
		curr_side_delta.set(0.0f);
		curr_turn_delta.set(0.0f);
	}
}

//------------------------------------------------------------------------------
// Start or stop upward look.
//------------------------------------------------------------------------------

static void
look_up(bool key_down)
{
	if (key_down)
		curr_look_delta.set(-1.0f);
	else
		curr_look_delta.set(0.0f);
}

//------------------------------------------------------------------------------
// Start or stop downward look.
//------------------------------------------------------------------------------

static void
look_down(bool key_down)
{
	if (key_down)
		curr_look_delta.set(1.0f);
	else
		curr_look_delta.set(0.0f);
}

//------------------------------------------------------------------------------
// Increase the rate of motion.
//------------------------------------------------------------------------------

static void
go_faster(bool key_down)
{
	if (!key_down)
		update_motion_rates(RATE_FASTER);
}

//------------------------------------------------------------------------------
// Decrease the rate of motion.
//------------------------------------------------------------------------------

static void
go_slower(bool key_down)
{
	if (!key_down)
		update_motion_rates(RATE_SLOWER);
}

//------------------------------------------------------------------------------
// Toggle between rotating or moving sideways if such movement is already in
// progress.
//------------------------------------------------------------------------------

static void
sidle_mode(bool key_down)
{
	sidle_mode_enabled = key_down;
	float curr_side_delta_value = curr_side_delta.get();
	float curr_turn_delta_value = curr_turn_delta.get();
	if (FNE(curr_side_delta_value, 0.0f)) {
		curr_turn_delta.set(curr_side_delta_value);
		curr_side_delta.set(0.0f);
	} else if (FNE(curr_turn_delta_value, 0.0f)) {
		curr_side_delta.set(curr_turn_delta_value);
		curr_turn_delta.set(0.0f);
	}
}

//------------------------------------------------------------------------------
// Set or reset the current move and rotate rates.
//------------------------------------------------------------------------------

static void
fast_mode(bool key_down)
{
	if (key_down && !fast_mode_enabled) {
		fast_mode_enabled = true;
		prev_move_rate = curr_move_rate.get();
		prev_rotate_rate = curr_rotate_rate.get();
		curr_move_rate.set(MAX_MOVE_RATE);
		curr_rotate_rate.set(MAX_ROTATE_RATE);
	} 
	if (!key_down && fast_mode_enabled) {
		fast_mode_enabled = false;
		curr_move_rate.set(prev_move_rate);
		curr_rotate_rate.set(prev_rotate_rate);
	}
}

//==============================================================================
// Callback functions.
//==============================================================================

//------------------------------------------------------------------------------
// Light window callback function.
//------------------------------------------------------------------------------

static void
light_window_callback(float brightness, bool window_closed)
{
	if (window_closed)
		save_config_file();
	else
		master_brightness.set(-brightness / 100.0f);
}

//------------------------------------------------------------------------------
// Options window callback function.
//------------------------------------------------------------------------------

static void
options_window_callback(int option_ID, int option_value)
{
	switch (option_ID) {
	case OK_BUTTON:
		download_sounds.set(download_sounds_flag);
		close_options_window();
		if (hardware_acceleration_available && 
			hardware_acceleration_flag != hardware_acceleration)
				toggle_window_mode();
		save_config_file();
		break;
	case CANCEL_BUTTON:
		visible_block_radius.set(old_visible_block_radius);
		user_debug_level.set(old_user_debug_level);
		close_options_window();
		break;
	case DOWNLOAD_SOUNDS_CHECKBOX:
		download_sounds_flag = option_value ? true : false;
		break;
	case ENABLE_3D_ACCELERATION_CHECKBOX:
		hardware_acceleration_flag = option_value ? true : false;
		break;
	case VISIBLE_RADIUS_EDITBOX:
		visible_block_radius.set(option_value);
		break;
	case DEBUG_LEVEL_OPTION:
		user_debug_level.set(option_value);
	}
}

//------------------------------------------------------------------------------
// Snapshot window callback function.
//------------------------------------------------------------------------------

static void
snapshot_window_callback(int width, int height, int position)
{
	// Get the path of the snapshot JPEG file.

	if ((snapshot_file_path = get_save_file_name("Save snapshot to JPEG file",
		"JPEG image file\0*.jpeg;*.jpg\0", NULL)) != NULL) {

		// Now send an event to the player thread to take a snapshot of the given
		// size and save it to the given JPEG file.

		snapshot_width = width;
		snapshot_height = height;
		snapshot_position = position;
		snapshot_in_progress.set(true);
		snapshot_requested.send_event(true);
	}
}

//------------------------------------------------------------------------------
// Callback function to handle key events.
//------------------------------------------------------------------------------

static void
key_event_callback(byte key_code, bool key_down)
{
	int index;
	key_code_to_func *key_func_ptr;

	// If a spot is not currently loaded, don't process any key events.

	if (!spot_loaded.get())
		return;

#ifdef _DEBUG

	// If the 'P' key was released, send an event indicating polygon info is
	// requested.

	if (key_code == 'P' && !key_down) {
		polygon_info_requested.send_event(true);
		return;
	}

#endif

	if (key_code == 'C' && !key_down)
		open_snapshot_window(window_width, window_height,
			snapshot_window_callback);

	// Check whether the key is a recognised function key, and if so execute
	// it's function.

	for (index = 0; index < KEY_FUNCTIONS; index++) {
		key_func_ptr = &key_func_table[index];
		if (key_code == key_func_ptr->key_code ||
			key_code == key_func_ptr->alt_key_code) {
			(*key_func_ptr->func_ptr)(key_down);
			break;
		}
	}

	// Add this key event to the queue, provided it is not full.

	raise_semaphore(key_event_semaphore);
	if (key_events < MAX_KEY_EVENTS) {
		key_event_queue[last_key_event].key_down = key_down;
		key_event_queue[last_key_event].key_code = key_code;
		last_key_event = (last_key_event + 1) % MAX_KEY_EVENTS;
		key_events++;
	}
	lower_semaphore(key_event_semaphore);
}

//------------------------------------------------------------------------------
// Callback function to handle mouse events.
//------------------------------------------------------------------------------

static void
mouse_event_callback(int x, int y, int button_code, int task_bar_button_code)
{
	int selected_recent_spot_index;

	// Remove the light window if it is currently displayed.

	close_light_window();

	// Update the mouse cursor.

	update_mouse_cursor(x, y);

	// Handle the button code...

	switch (button_code) {

	// If the left mouse button was pressed, set the button status to reflect
	// this and determine whether movement should be enabled according to where
	// the mouse is pointing; if not, reset the motion deltas.  Then capture
	// the mouse.

	case LEFT_BUTTON_DOWN:
		curr_button_status = LEFT_BUTTON_DOWN;
		if (inside_3D_window && !selection_active_flag)
			movement_enabled = true;
		else {
			movement_enabled = false;
			curr_move_delta.set(0.0f);
			curr_side_delta.set(0.0f);
			curr_turn_delta.set(0.0f);
		}
		capture_mouse();
		break;

	// If the right mouse button was pressed...

	case RIGHT_BUTTON_DOWN:

		// Set the button status and absolute motion flag, and reset the motion
		// deltas.

		curr_button_status = RIGHT_BUTTON_DOWN;
		absolute_motion.set(true);
		curr_turn_delta.set(0.0f);
		curr_look_delta.set(0.0f);

		// Initialise the last mouse position.

		last_x = x;
		last_y = y;

		// Set the cursor to a crosshair and capture the mouse.

		set_crosshair_cursor();
		capture_mouse();
		break;

	// If the left mouse button was released...

	case LEFT_BUTTON_UP:

		// Send an event to the player thread if an active link has been 
		// selected.
		
		if (inside_3D_window && selection_active_flag && !movement_enabled)
			mouse_clicked.send_event(true);

		// If one of the task bar buttons is active, perform the button action.

		switch (task_bar_button_code) {
		case LOGO_BUTTON:
			NPN_GetURL(active_inst_data_ptr->instance_ptr, FLATLAND_URL,
				"_blank");
			break;
		case RECENT_SPOTS_BUTTON:
			disable_cursor_changes = true;
			set_arrow_cursor();
			raise_semaphore(recent_spot_list_semaphore);
			open_recent_spots_menu(recent_spot_list, recent_spots);
			if ((selected_recent_spot_index = track_recent_spots_menu() - 1) 
				>= 0) {
				selected_recent_spot_URL = 
					recent_spot_list[selected_recent_spot_index].URL;
				recent_spot_selected.send_event(true);
			}
			close_recent_spots_menu();
			lower_semaphore(recent_spot_list_semaphore);
			disable_cursor_changes = false;
			break;
		case OPTIONS_BUTTON:
			download_sounds_flag = download_sounds.get();
			hardware_acceleration_flag = hardware_acceleration;
			old_visible_block_radius = visible_block_radius.get();
			old_user_debug_level = user_debug_level.get();
			open_options_window(download_sounds_flag, old_visible_block_radius, 
				old_user_debug_level, options_window_callback);
			break;
		case LIGHT_BUTTON:
			open_light_window(master_brightness.get(), light_window_callback);
			break;
		case BUILDER_BUTTON:
			disable_cursor_changes = true;
			set_arrow_cursor();
			open_builder_menu();
			launch_builder_web_page(track_builder_menu());
			close_builder_menu();
			disable_cursor_changes = false;
			break;
		case DIRECTORY_BUTTON:
			display_spot_directory();
			break;
		case COMMAND_BUTTON:
			disable_cursor_changes = true;
			set_arrow_cursor();
			open_command_menu();
			switch (track_command_menu()) {
			case ABOUT_ROVER_COMMAND:
				open_about_window();
				break;
			case ROVER_HELP_COMMAND:
				open_help_window();
				break;
			case DOWNLOAD_ROVER_COMMAND:
				NPN_GetURL(active_inst_data_ptr->instance_ptr,
					"http://www.flatland.com/download", "_self");
				break;
			case VIEW_3DML_SOURCE_COMMAND:
				display_file_as_web_page(curr_spot_file_path);
				break;
			case SAVE_3DML_SOURCE_COMMAND:
				if ((saved_spot_file_path = get_save_file_name(
					"Save spot to 3DML file", "3DML file\0*.3dml\0", NULL)) 
					!= NULL)
					save_3DML_source_requested.send_event(true);
				break;
			case TAKE_SNAPSHOT_COMMAND:
				open_snapshot_window(window_width, window_height,
					snapshot_window_callback);
				break;
			case MANAGE_BLOCKSETS_COMMAND:
				open_blockset_manager_window();
			}
			close_command_menu();
			disable_cursor_changes = false;
		}

		// Set the current button status, reset the motion deltas, and release
		// the mouse if it was captured.

		curr_button_status = MOUSE_MOVE_ONLY;
		curr_move_delta.set(0.0f);
		curr_side_delta.set(0.0f);
		curr_turn_delta.set(0.0f);
		release_mouse();
		break;

	// If the right mouse button was released, set the button status, reset the
	// motion deltas, and release the mouse if it was captured.

	case RIGHT_BUTTON_UP:
		curr_button_status = MOUSE_MOVE_ONLY;
		absolute_motion.set(false);
		curr_turn_delta.set(0.0f);
		curr_look_delta.set(0.0f);
		release_mouse();
	}

	// Set the mouse cursor.

	set_mouse_cursor();

	// React to the mouse button status.

	switch (curr_button_status) {

	// If the left button is down...

	case LEFT_BUTTON_DOWN:
		
		// Compute the player movement or rotation based upon the current mouse
		// direction, if movement is enabled.

		if (movement_enabled) {
			switch (movement_arrow) {
			case ARROW_NW:
			case ARROW_N:
			case ARROW_NE:
				curr_move_delta.set(1.0f);
				break;
			case ARROW_SW:
			case ARROW_S:
			case ARROW_SE:
				curr_move_delta.set(-1.0f);
				break;
			default:
				curr_move_delta.set(0.0f);
			}
			switch (movement_arrow) {
			case ARROW_NE:
			case ARROW_E:
			case ARROW_SE:
				if (sidle_mode_enabled) {
					curr_side_delta.set(1.0f);
					curr_turn_delta.set(0.0f);
				} else {
					curr_side_delta.set(0.0f);
					curr_turn_delta.set(1.0f);
				}
				break;
			case ARROW_NW:
			case ARROW_W:
			case ARROW_SW:
				if (sidle_mode_enabled) {
					curr_side_delta.set(-1.0f);
					curr_turn_delta.set(0.0f);
				} else {
					curr_side_delta.set(0.0f);
					curr_turn_delta.set(-1.0f);
				}
				break;
			default:
				curr_side_delta.set(0.0f);
				curr_turn_delta.set(0.0f);
			}
		} 
		break;

	// If the right mouse button is down, compute a player rotation that is
	// proportional to the distance the mouse has travelled since the last mouse
	// event (current one pixel = one degree), and add it to the current turn
	// or look delta.

	case RIGHT_BUTTON_DOWN:
		curr_turn_delta.set(curr_turn_delta.get() + x - last_x);
		curr_look_delta.set(curr_look_delta.get() + y - last_y);
		last_x = x;
		last_y = y;
	}
}

//------------------------------------------------------------------------------
// Callback function to handle timer events.
//------------------------------------------------------------------------------

static void
timer_event_callback(void)
{
	// If the browser window has been minimised, send a pause event to the
	// player thread.

	if (!browser_window_minimised && browser_window_is_minimised()) {
		browser_window_minimised = true;
		pause_player_thread.send_event(true);
	}

	// If the browser window has been restored, send a resume event to the
	// player thread.

	if (browser_window_minimised && !browser_window_is_minimised()) {
		browser_window_minimised = false;
		resume_player_thread.send_event(true);
	}

	// If the browser window is not minimised and a spot is loaded, update the
	// cursor position.

	if (!browser_window_minimised && spot_loaded.get()) {
		int x, y;

		get_mouse_position(&x, &y, true);
		update_mouse_cursor(x, y);
		set_mouse_cursor();
	}

	// Check to see whether a URL download request has been signalled by the
	// player thread.

	if (URL_download_requested.event_sent()) {

		// Reset the URL_was_opened flag, and send off the URL request to the
		// browser.

		URL_was_opened.reset_event();
		open_local_file(requested_URL);
		/*
		if (web_browser_ID == INTERNET_EXPLORER) {
			if (strlen(requested_target) != 0)
				NPN_GetURL(active_inst_data_ptr->instance_ptr, requested_URL,
					requested_target);
			else
				NPN_GetURL(active_inst_data_ptr->instance_ptr, requested_URL,
					NULL);
		} else {
			if (strlen(requested_target) != 0)
				NPN_GetURLNotify(active_inst_data_ptr->instance_ptr,
					requested_URL, requested_target, NULL);
			else
				NPN_GetURLNotify(active_inst_data_ptr->instance_ptr,
					requested_URL, NULL, NULL);
		}
		*/
	}

	// Check to see whether a URL cancel request has been signalled by the
	// player thread, and if there is currently a stream being downloaded,
	// destroy it.

	if (URL_cancel_requested.event_sent() && curr_stream_ptr != NULL)
		NPN_DestroyStream(active_inst_data_ptr->instance_ptr, curr_stream_ptr,
			NPRES_USER_BREAK);

	// Check to see whether a javascript URL download request has been signalled
	// by the player thread, and if so send off the URL request to the browser.
	// We don't bother with notification of failure.

	if (javascript_URL_download_requested.event_sent())
		NPN_GetURL(active_inst_data_ptr->instance_ptr, javascript_URL, NULL);

	// Check to see whether a display error request has been signalled by
	// the player thread, and if so either display the error log if the player
	// requests it, or display a generic error message.

	if (display_error.event_sent()) {

		// Clear all movement deltas, so the player doesn't spin out of control
		// when the error window comes up.

		curr_turn_delta.set(0.0f);
		curr_look_delta.set(0.0f);
		curr_move_delta.set(0.0f);
		curr_side_delta.set(0.0f);

		// Now show the error window.

		if (show_error_log) {
			if (query("Errors in 3DML document", true,
				"One or more errors were encountered during the parsing of the "
				"3DML document.\nDo you want to view the error log?")) {
				FILE *fp;

				// Finish off the error log file so that it's ready to display
				// as a web page.

				if ((fp = fopen(error_log_file_path, "a")) != NULL) {
					fprintf(fp, "</BODY>\n</HTML>\n");
					fclose(fp);
				}
				display_file_as_web_page(error_log_file_path);
			}
		} else
			fatal_error("Unable to display 3DML document", "One or more errors "
				"in the 3DML document has prevented the Flatland Rover from "
				"displaying it.");
	}

	// Check to see whether a set or get key function request has been
	// signalled by the player thread, and if so perform that request and then
	// signal that it is complete.

	if (set_key_function.event_sent()) {
		key_func_table[key_function].key_code = key_code;
		key_func_table[key_function].alt_key_code = alt_key_code;
		key_function_request_completed.send_event(true);
	} else if (get_key_function.event_sent()) {
		key_code = key_func_table[key_function].key_code;
		alt_key_code = key_func_table[key_function].alt_key_code;
		key_function_request_completed.send_event(true);
	}

	// Check to see whether a show spot directory request has been signalled
	// by the player thread, and if so perform that request.

	if (show_spot_directory.event_sent())
		display_spot_directory();
}

//------------------------------------------------------------------------------
// Resize callback procedure.
//------------------------------------------------------------------------------

static void
resize_event_callback(void *window_handle, int width, int height)
{
	// Signal the player thread that a window resize is requested, and
	// wait for the player thread to signal that the player window has shut 
	// down.

	window_resize_requested.send_event(true);
	player_window_shut_down.wait_for_event();

	// Destroy the title and label textures.

	destroy_title_and_label_textures();

	// Destroy the frame buffer if hardware acceleration is not enabled.

	if (!hardware_acceleration)
		destroy_frame_buffer();

	// Resize the main window.

	set_main_window_size(width, height);

	// Recreate the title and label textures.

	create_title_and_label_textures();

	// Compute the window "centre" coordinates.

	window_half_width = window_width * 0.5f;
	window_top_half_height = window_height * 0.75f;
	window_bottom_half_height = window_height * 0.25f;

	// Create the frame buffer, and signal the player thread on it's success
	// or failure.  On failure, wait for the player thread to signal that the
	// player window has shut down, then destroy the main window and make the
	// instance inactive.

	if ((hardware_acceleration && recreate_frame_buffer()) ||
		(!hardware_acceleration && create_frame_buffer()))
		main_window_resized.send_event(true);
	else {
		main_window_resized.send_event(false);
		player_window_shut_down.wait_for_event();
		destroy_main_window();
		active_inst_data_ptr->active_instance = false;
		active_inst_data_ptr = NULL;
	}

	// Draw the current title, and label if there is one.

	set_title(NULL);
	show_label(NULL);
}

//------------------------------------------------------------------------------
// Display resolution change callback procedure.
//------------------------------------------------------------------------------

static void
display_event_callback(void)
{
	// Signal the player thread that a window mode change is requested, and
	// wait for the player thread to signal that the player window has shut
	// down.

	window_mode_change_requested.send_event(true);
	player_window_shut_down.wait_for_event();

	// Destroy the main window.

	destroy_main_window();

	// Create the player window.  If this fails, make the active instance
	// inactive.

	hardware_acceleration = (acceleration_mode == TRY_HARDWARE);
	if (!create_player_window()) {
		active_inst_data_ptr->active_instance = false;
		active_inst_data_ptr = NULL;
	}

	// Reset the title, and label if there is one.

	set_title(NULL);
	show_label(NULL);
}

//==============================================================================
// Plugin interface to browser.
//==============================================================================

//------------------------------------------------------------------------------
// NPP_Initialize:
// Provides global initialization for a plug-in, and returns an error value. 
//
// This function is called once when a plug-in is loaded, before the first
// instance is created. You should allocate any memory or resources shared by
// all instances of your plug-in at this time. After the last instance has been
// deleted, NPP_Shutdown will be called, where you can release any memory or
// resources allocated by NPP_Initialize. 
//------------------------------------------------------------------------------

NPError
NPP_Initialize(void)
{
	return(NPERR_NO_ERROR);
}

bool
init_flatland(HWND window_handle)
{
	// Remember the parent window.

	parent_window_handle = window_handle;

	// Start the memory trace.

#if MEM_TRACE
	start_trace();
#endif

	// Indicate Rover hasn't yet started up successfully.

	rover_started_up = false;

	// Start up the platform API.

	if (!start_up_platform_API())
		return(false);

	// Create the events sent by the player thread.

	player_thread_initialised.create_event();
	player_window_initialised.create_event();
	URL_download_requested.create_event();
	URL_cancel_requested.create_event();
	javascript_URL_download_requested.create_event();
	player_window_shut_down.create_event();
	show_spot_directory.create_event();
	display_error.create_event();
	set_key_function.create_event();
	get_key_function.create_event();

	// Create the events sent by the plugin thread.

	main_window_created.create_event();
	main_window_resized.create_event();
	URL_was_opened.create_event();
	URL_was_downloaded.create_event();
	window_mode_change_requested.create_event();
	window_resize_requested.create_event();
	mouse_clicked.create_event();
	player_window_shutdown_requested.create_event();
	player_window_init_requested.create_event();
	pause_player_thread.create_event();
	resume_player_thread.create_event();
	spot_dir_entry_selected.create_event();
	recent_spot_selected.create_event();
	check_for_update_requested.create_event();
	registration_requested.create_event();
	save_3DML_source_requested.create_event();
#ifdef _DEBUG
	polygon_info_requested.create_event();
#endif
	key_function_request_completed.create_event();
	snapshot_requested.create_event();

	// Create the semaphores which are protecting multiple data structures.

	recent_spot_list_semaphore = create_semaphore();
	key_event_semaphore = create_semaphore();
#ifdef STREAMING_MEDIA 
	image_updated_semaphore = create_semaphore();
#endif

	// Create the semaphores for all variables that require synchornised 
	// access.

	user_debug_level.create_semaphore();
	spot_loaded.create_semaphore();
	selection_active.create_semaphore();
	absolute_motion.create_semaphore();
	curr_mouse_x.create_semaphore();
	curr_mouse_y.create_semaphore();
	curr_move_delta.create_semaphore();
	curr_side_delta.create_semaphore();
	curr_turn_delta.create_semaphore();
	curr_look_delta.create_semaphore();
	curr_jump_delta.create_semaphore();
	curr_move_rate.create_semaphore();
	curr_rotate_rate.create_semaphore();
	master_brightness.create_semaphore();
	download_sounds.create_semaphore();
	visible_block_radius.create_semaphore();
	downloaded_URL.create_semaphore();
	downloaded_file_path.create_semaphore();
	snapshot_in_progress.create_semaphore();

	// Load the configuration file.

	load_config_file();

	// Indicate the player is not active yet, there is no player window,
	// no active plugin instance, and the browser window is not minimised.

	player_active = false;
	player_window_created = false;
	active_inst_data_ptr = NULL;
	browser_window_minimised = false;

	// Initialise the current stream pointer.

	curr_stream_ptr = NULL;

	// Initialise all variables that require synchronised access.

	spot_loaded.set(false);
	curr_move_delta.set(0.0f);
	curr_turn_delta.set(0.0f);
	curr_look_delta.set(0.0f);
	curr_jump_delta.set((float)0.0);
	curr_mouse_x.set(-1);
	curr_mouse_y.set(-1);
	snapshot_in_progress.set(false);

	// Initialise other variables.

	movement_enabled = false;
	sidle_mode_enabled = false;
	fast_mode_enabled = false;
	curr_button_status = MOUSE_MOVE_ONLY;
	disable_cursor_changes = false;

	// Initialise the key event queue.

	key_events = 0;
	first_key_event = 0;
	last_key_event = 0;

	// Start the player thread.

	if ((player_thread_handle = start_thread(player_thread)) == 0)
		return(false);

	// Wait for an event from the player thread regarding it's initialisation.

	player_active = player_thread_initialised.wait_for_event();

	// Indicate Rover started up sucessfully.

	rover_started_up = true;
	return(true);
}

//------------------------------------------------------------------------------
// NPP_GetJavaClass:
// New in Netscape Navigator 3.0. 
//
// NPP_GetJavaClass is called during initialization to ask your plugin
// what its associated Java class is. If you don't have one, just return
// NULL. Otherwise, use the javah-generated "use_" function to both
// initialize your class and return it. If you can't find your class, an
// error will be signalled by "use_" and will cause the Navigator to
// complain to the user.
//------------------------------------------------------------------------------

jref
NPP_GetJavaClass(void)
{
	return(NULL);
}

//------------------------------------------------------------------------------
// NPP_Shutdown:
// Provides global deinitialization for a plug-in. 
// 
// This function is called once after the last instance of your plug-in is
// destroyed. Use this function to release any memory or resources shared
// across all instances of your plug-in. You should be a good citizen and
// declare that you're not using your java class any more. This allows java to
// unload it, freeing up memory.
//------------------------------------------------------------------------------

void
NPP_Shutdown(void)
{
	// If the player thread is active, signal it to terminate, and wait for it
	// to do so.

	if (player_active) {
		player_window_init_requested.send_event(false);
		wait_for_thread_termination(player_thread_handle);
	}

	// Close any windows that might still be open.

	close_progress_window();
	close_light_window();
	close_options_window();
	close_about_window();
	close_help_window();
	close_snapshot_window();
	close_blockset_manager_window();

	// If Rover had started up successfully, save the configuration file.

	if (rover_started_up)
		save_config_file();

	// Destroy the semaphores that were protecting multiple data structures.

	destroy_semaphore(recent_spot_list_semaphore);
	destroy_semaphore(key_event_semaphore);
#ifdef STREAMING_MEDIA
	destroy_semaphore(image_updated_semaphore);
#endif

	// Destroy semaphores for all variables that required synchronised access.

	user_debug_level.destroy_semaphore();
	spot_loaded.destroy_semaphore();
	selection_active.destroy_semaphore();
	absolute_motion.destroy_semaphore();
	curr_mouse_x.destroy_semaphore();
	curr_mouse_y.destroy_semaphore();
	curr_move_delta.destroy_semaphore();
	curr_side_delta.destroy_semaphore();
	curr_turn_delta.destroy_semaphore();
	curr_look_delta.destroy_semaphore();
	curr_jump_delta.destroy_semaphore();
	curr_move_rate.destroy_semaphore();
	curr_rotate_rate.destroy_semaphore();
	master_brightness.destroy_semaphore();
	download_sounds.destroy_semaphore();
	visible_block_radius.destroy_semaphore();
	downloaded_URL.destroy_semaphore();
	downloaded_file_path.destroy_semaphore();
	snapshot_in_progress.destroy_semaphore();

	// Destroy the events sent by the player thread.

	player_thread_initialised.destroy_event();
	player_window_initialised.destroy_event();
	URL_download_requested.destroy_event();
	URL_cancel_requested.destroy_event();
	javascript_URL_download_requested.destroy_event();
	player_window_shut_down.destroy_event();
	show_spot_directory.destroy_event();
	display_error.destroy_event();
	set_key_function.destroy_event();
	get_key_function.destroy_event();

	// Destroy the events sent by the plugin thread.

	main_window_created.destroy_event();
	main_window_resized.destroy_event();
	URL_was_opened.destroy_event();
	URL_was_downloaded.destroy_event();
	window_mode_change_requested.destroy_event();
	window_resize_requested.destroy_event();
	mouse_clicked.destroy_event();
	player_window_shutdown_requested.destroy_event();
	player_window_init_requested.destroy_event();
	pause_player_thread.destroy_event();
	resume_player_thread.destroy_event();
	spot_dir_entry_selected.destroy_event();
	recent_spot_selected.destroy_event();
	check_for_update_requested.destroy_event();
	registration_requested.destroy_event();
	save_3DML_source_requested.destroy_event();
#ifdef _DEBUG
	polygon_info_requested.destroy_event();
#endif
	key_function_request_completed.destroy_event();
	snapshot_requested.destroy_event();

	// Shut down the platform API.

	shut_down_platform_API();

	// End the memory trace.

#if MEM_TRACE
	end_trace();
#endif
}

//------------------------------------------------------------------------------
// NPP_New:
// Creates a new instance of a plug-in and returns an error value. 
// 
// NPP_New creates a new instance of your plug-in with MIME type specified
// by pluginType. The parameter mode is NP_EMBED if the instance was created
// by an EMBED tag, or NP_FULL if the instance was created by a separate file.
// You can allocate any instance-specific private data in instance->pdata at
// this time. The NPP pointer is valid until the instance is destroyed. 
//------------------------------------------------------------------------------

NPError 
NPP_New(NPMIMEType pluginType, NPP instance_ptr, uint16 mode, int16 argc,
		char *argn[], char *argv[], NPSavedData *saved_data_ptr)
{
	InstData *inst_data_ptr;
	string user_agent;
	char version_number[32];
	HKEY key_handle;
	DWORD version_size;
	int index;

	// Check for a NULL instance.

	if (instance_ptr == NULL)
		return(NPERR_INVALID_INSTANCE_ERROR);

	// Determine which browser we are running under.

	user_agent = NPN_UserAgent(instance_ptr);
	_strlwr(user_agent);
	if (strstr(user_agent, "opera")) {
		web_browser_ID = OPERA;
		web_browser_version = "Opera";
	} else if (strstr(user_agent, "mozilla")) {
		web_browser_ID = NAVIGATOR;
		web_browser_version = "Navigator";
		if (sscanf(user_agent, "mozilla/%s", version_number) == 1) {
			web_browser_version += " ";
			web_browser_version += version_number;
		}
	} else if (strstr(user_agent, "internet explorer")) {
		web_browser_ID = INTERNET_EXPLORER;
		web_browser_version = "MSIE";
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			"Software\\Microsoft\\Internet Explorer\\", 0, 
			KEY_QUERY_VALUE, &key_handle) == ERROR_SUCCESS) {
			version_size = 16;
			if (RegQueryValueEx(key_handle, "Version", NULL, NULL,
				(LPBYTE)&version_number, &version_size) == ERROR_SUCCESS) {
				web_browser_version += " ";
				web_browser_version += version_number;
			}
			RegCloseKey(key_handle);
		}
	} else if (strstr(user_agent, "activex control")) {
		web_browser_ID = ACTIVEX_CONTROL;
		web_browser_version = "ActiveX control";
	} else {
		web_browser_ID = UNKNOWN_BROWSER;
		web_browser_version = user_agent;
	}

	// Allocate the instance data structure, and initialise the instance
	// pointer.

	if ((instance_ptr->pdata = new InstData) == NULL)
		return(NPERR_OUT_OF_MEMORY_ERROR);
	inst_data_ptr = (InstData *)instance_ptr->pdata;
	inst_data_ptr->instance_ptr = instance_ptr;

	// Look for an argument with the name "TEXT", and if found store it's value
	// in the instance data structure as the inactive window text.

	for (index = 0; index < argc; index++)
		if (!_stricmp(argn[index], "text"))
			break;
	if (index < argc)
		inst_data_ptr->window_text = argv[index];
	else
		inst_data_ptr->window_text = "Click here to view spot";
	
	// If the player is active...

	if (player_active) {

		// If this is the first instance to be created, then make it the active
		// one and send a request to the player thread to initialise the player
		// window.

		if (active_inst_data_ptr == NULL) {
			active_inst_data_ptr = inst_data_ptr;
			inst_data_ptr->active_instance = true;
			player_window_init_requested.send_event(true);
		} else
			inst_data_ptr->active_instance = false;
	}

	// If the player is inactive, all instances are inactive.

	else
		inst_data_ptr->active_instance = false;
	
	// Return success status.

	return(NPERR_NO_ERROR);
}

//------------------------------------------------------------------------------
// NPP_Destroy:
// Deletes a specific instance of a plug-in and returns an error value. 
//
// NPP_Destroy is called when a plug-in instance is deleted, typically because
// the user has left the page containing the instance, closed the window, or
// quit the application. You should delete any private instance-specific
// information stored in instance->pdata.  If the instance being deleted is the
// last instance created by your plug-in, NPP_Shutdown will subsequently be 
// called, where you can delete any data allocated in NPP_Initialize to be
// shared by all your plug-in's instances.  Note that you should not perform
// any graphics operations in NPP_Destroy as the instance's window is no longer
// guaranteed to be valid. 
//------------------------------------------------------------------------------

NPError 
NPP_Destroy(NPP instance_ptr, NPSavedData **saved_data_handle)
{
	InstData *inst_data_ptr;
	NPWindow *window_ptr;

	// Check for a NULL instance.

	if (instance_ptr == NULL)
		return(NPERR_INVALID_INSTANCE_ERROR);
	inst_data_ptr = (InstData *)instance_ptr->pdata;

	// If the player thread is active...

	if (player_active) {

		// If this is the active instance, destroy the player window and make
		// this instance inactive.
		
		if (inst_data_ptr == active_inst_data_ptr) {
			destroy_player_window();
			active_inst_data_ptr = NULL;
		}

		// Otherwise restore the plugin window.

		else {
			window_ptr = inst_data_ptr->window_ptr;
			restore_plugin_window(window_ptr->window);
		}
	}

	// Delete the instance data.

	delete inst_data_ptr;
	instance_ptr->pdata = NULL;
	return(NPERR_NO_ERROR);
}

//------------------------------------------------------------------------------
// Inactive plugin window callback procedure.
//------------------------------------------------------------------------------

static void
inactive_window_callback(void *window_data_ptr)
{
	InstData *inst_data_ptr;
	NPWindow *window_ptr;

	// If the player is not active or this instance does not yet have a spot 
	// URL, do nothing.

	inst_data_ptr = (InstData *)window_data_ptr;
	if (!player_active || strlen(inst_data_ptr->spot_URL) == 0)
		return;

	// If there is an active plugin window, make it inactive.

	if (active_inst_data_ptr != NULL) {
		window_ptr = active_inst_data_ptr->window_ptr;

		// Destroy the player window.

		destroy_player_window();

		// Make the plugin window inactive.

		set_plugin_window(window_ptr->window, active_inst_data_ptr,
			active_inst_data_ptr->window_text, inactive_window_callback);

		// Make the plugin instance inactive.

		active_inst_data_ptr->active_instance = false;
		active_inst_data_ptr = NULL;
	}

	// Signal the player thread that a new player window is requested.
	
	player_window_init_requested.send_event(true);

	// Restore the inactive plugin window.

	window_ptr = inst_data_ptr->window_ptr;
	restore_plugin_window(window_ptr->window);

	// Make the new instance active.

	active_inst_data_ptr = inst_data_ptr;
	active_inst_data_ptr->active_instance = true;

	// Create the player window, and if this succeeds request that the 
	// instance's spot URL be downloaded. 

	if (create_player_window()) {
		URL_was_opened.reset_event();
		if (web_browser_ID == INTERNET_EXPLORER)
			NPN_GetURL(active_inst_data_ptr->instance_ptr, 
				active_inst_data_ptr->spot_URL, NULL);
		else
			NPN_GetURLNotify(active_inst_data_ptr->instance_ptr, 
				active_inst_data_ptr->spot_URL, NULL, NULL);
	}

	// If the player window wasn't created, make this instance inactive again.

	else {
		active_inst_data_ptr->active_instance = false;
		set_plugin_window(active_inst_data_ptr->window_ptr->window, 
			active_inst_data_ptr, active_inst_data_ptr->window_text, 
			inactive_window_callback);
		active_inst_data_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// NPP_SetWindow:
// Sets the window in which a plug-in draws, and returns an error value. 
// 
// NPP_SetWindow informs the plug-in instance specified by instance of the
// the window denoted by window in which the instance draws.  This NPWindow
// pointer is valid for the life of the instance, or until NPP_SetWindow is
// called again with a different value.  Subsequent calls to NPP_SetWindow for
// a given instance typically indicate that the window has been resized.  If
// either window or window->window are NULL, the plug-in must not perform any
// additional graphics operations on the window and should free any resources
// associated with the window. 
//------------------------------------------------------------------------------

NPError 
NPP_SetWindow(NPP instance_ptr, NPWindow *new_window_ptr)
{
	InstData *inst_data_ptr;
	NPWindow *window_ptr;

	// Check for a NULL instance.

	if (instance_ptr == NULL)
		return(NPERR_INVALID_INSTANCE_ERROR);

	// PLUGIN DEVELOPERS:
	// Before setting window to point to the new window, you may wish to
	// compare the new window info to the previous window (if any) to note
	// window size changes, etc.

	inst_data_ptr = (InstData *)instance_ptr->pdata;
	window_ptr = inst_data_ptr->window_ptr;
	if (window_ptr != NULL) {

		// If there is no new window, restore the existing plugin window if
		// this is an inactive instance, and return.

		if ((new_window_ptr == NULL) || (new_window_ptr->window == NULL)) {
			if (inst_data_ptr != active_inst_data_ptr)
				restore_plugin_window(window_ptr->window);
			inst_data_ptr->window_ptr = NULL;
			return(NPERR_NO_ERROR);
		} 
		
		// If the new window is the same as the old window, just get out after
		// recording the new window pointer.

		else if (window_ptr->window == new_window_ptr->window) {
			inst_data_ptr->window_ptr = new_window_ptr;
			return(NPERR_NO_ERROR);
		}
			
		// If the new window is different than the old window, then restore
		// the old window if this is an inactive instance before continuing.
		// NOTE: We don't want this path invoked for active windows, as it
		// will play havoc with DirectX.  Current browsers don't invoke this
		// path at all.

		else if (inst_data_ptr != active_inst_data_ptr)
			restore_plugin_window(window_ptr->window);
	} 
	
	// We can just get out of here if there is no current window and there
	// is no new window to use.

	else if ((new_window_ptr == NULL) || (new_window_ptr->window == NULL)) {
		inst_data_ptr->window_ptr = NULL;
		return(NPERR_NO_ERROR);
	}

	// Set the window pointer in the instance data.

	inst_data_ptr->window_ptr = new_window_ptr;
	window_ptr = new_window_ptr;

	// If this instance is the active one...

	if (inst_data_ptr == active_inst_data_ptr) {

		// If we don't yet have a player window, create it and signal the player
		// thread, then wait for a signal from the player thread that the main
		// window was initialised.  If either of these result in failure, the
		// instance is made inactive.

		if (!player_window_created) {

			// Create the player window.  If this fails, make the active
			// instance inactive.

			if (!create_player_window()) {
				active_inst_data_ptr->active_instance = false;
				active_inst_data_ptr = NULL;
			}
		}
	} 
	
	// If this is not the active instance, make the plugin window inactive.
	
	if (inst_data_ptr != active_inst_data_ptr)
		set_plugin_window(window_ptr->window, inst_data_ptr,
			inst_data_ptr->window_text, inactive_window_callback);

	// Return success status.

	return(NPERR_NO_ERROR);
}

//------------------------------------------------------------------------------
// Progress window callback function: this is called if the user hit the cancel
// button on the progress window.
//------------------------------------------------------------------------------

static void
progress_window_callback(void)
{
	// Ask the browser to destroy the stream that is currently open, if there
	// is one.

	if (curr_stream_ptr != NULL)
		NPN_DestroyStream(active_inst_data_ptr->instance_ptr, curr_stream_ptr,
			NPRES_USER_BREAK);
}

//------------------------------------------------------------------------------
// NPP_NewStream:
// Notifies an instance of a new data stream and returns an error value. 
// 
// NPP_NewStream notifies the instance denoted by instance of the creation of
// a new stream specifed by stream. The NPStream pointer is valid until the
// stream is destroyed. The MIME type of the stream is provided by the
// parameter type. 
//------------------------------------------------------------------------------

NPError 
NPP_NewStream(NPP instance_ptr, NPMIMEType type, NPStream *stream_ptr,
			  NPBool seekable, uint16 *stype_ptr)
{
	InstData *inst_data_ptr;
	FILE *fp;

	// Return an error if the instance is NULL.

	if (instance_ptr == NULL)
		return(NPERR_INVALID_INSTANCE_ERROR);

	// If the stream URL matches the javascript URL, we're going to stream it
	// into oblivion.

	if (!_stricmp(stream_ptr->url, javascript_URL)) {
		*stype_ptr = NP_NORMAL;
		javascript_URL = "";
	}

	// Otherwise ask for the stream to be downloaded into a local file, and
	// inform the player thread that the URL stream was opened.  We use this 
	// flag to ignore the reason code passed to NPP_Notify(), and instead allow
	// NPP_DestroyStream() to signal failure to the player thread.  This is
	// necessary because Navigator 3.0 sends a NPRES_NETWORK_ERR reason code to
	// NPP_Notify() even upon success.

	else {

		// Remember this as the current stream.

		curr_stream_ptr = stream_ptr;

		// If a requested file path was given, stream the URL into this file,
		// otherwise let the browser choose a file path.

		if (strlen(requested_file_path) > 0) {
			*stype_ptr = NP_NORMAL;
			if ((fp = fopen(requested_file_path, "wb")) != NULL)
				fclose(fp);

			// If requested_blockset_name is set, a blockset is being downloaded
			// so open the progress window with an approapiate message.

			if (strlen(requested_blockset_name) > 0)
				open_progress_window(stream_ptr->end, progress_window_callback, 
					"Downloading %s blockset.", requested_blockset_name);
		} else
			*stype_ptr = NP_ASFILE;

		// Inform the player thread that the URL was opened.

		URL_was_opened.send_event(true);
		
		// If the spot URL has not yet been set for this instance, then this is
		// the original spot URL, which needs to be stored in the instance
		// data structure.

		inst_data_ptr = (InstData *)instance_ptr->pdata;
		if (strlen(inst_data_ptr->spot_URL) == 0) {
			inst_data_ptr->spot_URL = stream_ptr->url;
			inst_data_ptr->spot_URL = decode_URL(inst_data_ptr->spot_URL);
		}
	}

	// Return success code.

	return(NPERR_NO_ERROR);
}

// PLUGIN DEVELOPERS:
// These next 2 functions are directly relevant in a plug-in which handles the
// data in a streaming manner.  If you want zero bytes because no buffer space
// is YET available, return 0.  As long as the stream has not been written to
// the plugin, Navigator will continue trying to send bytes.  If the plugin
// doesn't want them, just return some large number from NPP_WriteReady(), and
// ignore them in NPP_Write().  For a NP_ASFILE stream, they are still called
// but can safely be ignored using this strategy.

//------------------------------------------------------------------------------
// NPP_WriteReady:
// Returns the maximum number of bytes that an instance is prepared to accept
// from the stream. 
// 
// NPP_WriteReady determines the maximum number of bytes that the instance will
// consume from the stream in a subsequent call NPP_Write. This function allows
// Netscape to only send as much data to the instance as the instance is
// capable of handling at a time, allowing more efficient use of resources
// within both Netscape and the plug-in. 
//------------------------------------------------------------------------------

int32 
NPP_WriteReady(NPP instance_ptr, NPStream *stream_ptr)
{
	// Allow Netscape to send as much as it likes.

	return(0x7fffffff);
}

//------------------------------------------------------------------------------
// NPP_Write:
// Delivers data from a stream and returns the number of bytes written. 
// 
// NPP_Write is called after a call to NPP_NewStream in which the plug-in
// requested a normal-mode stream, in which the data in the stream is delivered
// progressively over a series of calls to NPP_WriteReady and NPP_Write. The
// function delivers a buffer buf of len bytes of data from the stream
// identified by stream to the instance. The parameter offset is the logical
// position of buf from the beginning of the data in the stream. 
//
// The function returns the number of bytes written (consumed by the instance).
// A negative return value causes an error on the stream, which will
// subsequently be destroyed via a call to NPP_DestroyStream. 
// 
// Note that a plug-in must consume at least as many bytes as it indicated in
// the preceeding NPP_WriteReady call. All data consumed must be either
// processed immediately or copied to memory allocated by the plug-in: the buf
// parameter is not persistent. 
//------------------------------------------------------------------------------

int32 
NPP_Write(NPP instance_ptr, NPStream *stream_ptr, int32 offset, int32 len,
		  void *buffer_ptr)
{
	FILE *fp;
	
	// If the stream is not recognised, or does not have a requested file path,
	// then just ignore it.

	if (stream_ptr != curr_stream_ptr || strlen(requested_file_path) == 0)
		return(len);

	// Open the destination file, and write the supplied buffer contents to the
	// file at the given file offset.

	if ((fp = fopen(requested_file_path, "rb+")) != NULL) {
		fseek(fp, offset, SEEK_SET);
		fwrite(buffer_ptr, len, 1, fp);
		fclose(fp);
	}

	// If a requested blockset name was specified, update the progress window.

	if (strlen(requested_blockset_name) > 0)
		update_progress_window(offset + len, stream_ptr->end);

	// Return the number of bytes written.

	return(len);
}

//------------------------------------------------------------------------------
// NPP_DestroyStream:
// Indicates the closure and deletion of a stream, and returns an error value. 
// 
// The NPP_DestroyStream function is called when the stream identified by
// stream for the plug-in instance denoted by instance will be destroyed. You
// should delete any private data allocated in stream->pdata at this time. 
//------------------------------------------------------------------------------

NPError 
NPP_DestroyStream(NPP instance_ptr, NPStream *stream_ptr, NPError reason)
{
	InstData *inst_data_ptr;

	// Check for NULL instance.

	if (instance_ptr == NULL)
		return(NPERR_INVALID_INSTANCE_ERROR);

	// Signal the plugin if the file was not downloaded successfully.  If the
	// URL was streamed into a requested file, also signal the plugin if the
	// file *was* downloaded successfully.

	inst_data_ptr = (InstData *)instance_ptr->pdata;
	if (inst_data_ptr == active_inst_data_ptr) {

		// Set the download URL.

		downloaded_URL.set((char *)stream_ptr->url);

		// If a requested file path was set, set the downloaded file path,
		// and send an event to the player thread informing it whether the
		// download was successful or not.

		if (strlen(requested_file_path) > 0) {
			downloaded_file_path.set(requested_file_path);
			URL_was_downloaded.send_event(reason == NPRES_DONE);

			// If a blockset was been downloaded, close the progress window and
			// delete the partial blockset file if the download failed.

			if (strlen(requested_blockset_name) > 0) {
				close_progress_window();
				if (reason != NPRES_DONE)
					remove(requested_file_path);
			}
		} 
		
		// If no requested file path was set, send an event to the player thread
		// if the download was not successful.

		else if (reason != NPRES_DONE)
			URL_was_downloaded.send_event(false);

		// Reset the current stream pointer.

		curr_stream_ptr = NULL;
	}
	return(NPERR_NO_ERROR);
}

//------------------------------------------------------------------------------
// NPP_StreamAsFile:
// Provides a local file name for the data from a stream. 
// 
// NPP_StreamAsFile provides the instance with a full path to a local file,
// identified by fname, for the stream specified by stream. NPP_StreamAsFile is
// called as a result of the plug-in requesting mode NP_ASFILEONLY or
// NP_ASFILE in a previous call to NPP_NewStream. If an error occurs while
// retrieving the data or writing the file, fname may be NULL. 
//------------------------------------------------------------------------------

void 
NPP_StreamAsFile(NPP instance_ptr, NPStream *stream_ptr, const char *file_name)
{
	InstData *inst_data_ptr;

	// Check for NULL instance.

	if (instance_ptr == NULL)
		return;
	inst_data_ptr = (InstData *)instance_ptr->pdata;

	// If this is the active instance...

	if (inst_data_ptr == active_inst_data_ptr) {

		// If the file name is NULL, the URL was not downloaded.  This error
		// ought to be reported via NPP_Notify(), but for some reason it comes
		// here and NPP_Notify() gets a success notification.

		if (file_name == NULL) {
			downloaded_URL.set((char *)stream_ptr->url);
			URL_was_downloaded.send_event(false);
		}

		// Signal the player thread that the URL was downloaded, making sure the
		// downloaded URL and local file path is set.

		else {
			downloaded_URL.set((char *)stream_ptr->url);
			downloaded_file_path.set((char *)file_name);
			URL_was_downloaded.send_event(true);
		}

		// Reset the current stream pointer.

		curr_stream_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// NPP_URLNotify:
// Notifies the instance of the completion of a URL request. 
//
// NPP_URLNotify is called when Netscape completes a NPN_GetURLNotify or
// NPN_PostURLNotify request, to inform the plug-in that the request,
// identified by url, has completed for the reason specified by reason.  The
// most common reason code is NPRES_DONE, indicating simply that the request
// completed normally.  Other possible reason codes are NPRES_USER_BREAK,
// indicating that the request was halted due to a user action (for example,
// clicking the "Stop" button), and NPRES_NETWORK_ERR, indicating that the
// request could not be completed (for example, because the URL could not be
// found).  The complete list of reason codes is found in npapi.h. 
// 
// The parameter notifyData is the same plug-in-private value passed as an
// argument to the corresponding NPN_GetURLNotify or NPN_PostURLNotify
// call, and can be used by your plug-in to uniquely identify the request. 
//------------------------------------------------------------------------------

void
NPP_URLNotify(NPP instance_ptr, const char *URL, NPReason reason, 
			  void *notifyData_ptr)
{
	InstData *inst_data_ptr;

	// If the instance pointer is NULL or URL_was_opened is TRUE, ignore
	// the reason code.

	if (instance_ptr == NULL || URL_was_opened.event_value)
		return;

	// If the reason parameter is anything other than NPRES_DONE, inform the
	// plugin that the URL was not downloaded.

	inst_data_ptr = (InstData *)instance_ptr->pdata;
	if (inst_data_ptr == active_inst_data_ptr && reason != NPRES_DONE) {
		downloaded_URL.set((char *)URL);
		URL_was_downloaded.send_event(false);
		curr_stream_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// NPP_Print:
// Handle a print request.
//------------------------------------------------------------------------------

void 
NPP_Print(NPP instance_ptr, NPPrint *printInfo_ptr)
{
	if (instance_ptr == NULL || printInfo_ptr == NULL)
		return;

	if (printInfo_ptr->mode == NP_FULL) {
		    
	    // PLUGIN DEVELOPERS:
	    //	If your plugin would like to take over printing completely when
		// it is in full-screen mode, set printInfo->pluginPrinted to TRUE
		// and print your plugin as you see fit.  If your plugin wants
		// Netscape	to handle printing in this case, set 
		// printInfo->pluginPrinted to FALSE (the default) and do nothing.
		// If you do want to handle printing yourself, printOne is true if
		// the print button	(as opposed to the print menu) was clicked.
		// On the Macintosh, platformPrint is a THPrint; on Windows,
		// platformPrint is a structure (defined in npapi.h) containing the
		// printer name, port,etc.

		void *platformPrint = printInfo_ptr->print.fullPrint.platformPrint;
		NPBool printOne = printInfo_ptr->print.fullPrint.printOne;
		
		// Do the default.

		printInfo_ptr->print.fullPrint.pluginPrinted = FALSE;
	} else {
		
		// If not fullscreen, we must be embedded.

		// PLUGIN DEVELOPERS:
		// If your plugin is embedded, or is full-screen but you returned
		// false in pluginPrinted above, NPP_Print will be called with mode
		// == NP_EMBED.  The NPWindow in the printInfo gives the location
		// and dimensions of the embedded plugin on the printed page.  On
		// the Macintosh, platformPrint is the printer port; on	Windows,
		// platformPrint is the handle to the printing device context.

		NPWindow *printWindow =	&(printInfo_ptr->print.embedPrint.window);
		void *platformPrint = printInfo_ptr->print.embedPrint.platformPrint;
	}
}

//------------------------------------------------------------------------------
// NPP_HandleEvent:
// Mac-only, but stub must be present for Windows.
// Delivers a platform-specific event to the instance. 
// 
// On the Macintosh, event is a pointer to a standard Macintosh EventRecord.
// All standard event types are passed to the instance as appropriate.  In
// general, return TRUE if you handle the event and FALSE if you ignore the
// event. 
//------------------------------------------------------------------------------

int16
NPP_HandleEvent(NPP instance_ptr, void *event_ptr)
{
	// Do nothing--this is not a Mac.

	return(0);
}
