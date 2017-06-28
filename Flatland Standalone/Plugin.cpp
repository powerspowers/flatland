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
#include "Fileio.h"
#include "Image.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "resource.h"
#ifdef SIMKIN
#include "SimKin.h"
#endif
#include "Spans.h"

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

// Player and downloader thread handles.

static unsigned long player_thread_handle;
static unsigned long downloader_thread_handle;

// Acceleration mode and hardware acceleration flag.

int acceleration_mode;
bool hardware_acceleration;

// Variables used to request a URL.

string requested_URL;
string requested_target;
string requested_file_path;
string requested_blockset_name;			// Set if requesting a blockset.

// Downloaded URL and file path.

semaphore<string> downloaded_URL;
semaphore<string> downloaded_file_path;

// Events sent by player thread.

event player_thread_initialised;
event player_window_initialised;
event URL_download_requested;
event URL_cancel_requested;
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
event downloader_thread_termination_requested;
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
static bool app_window_minimised;
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
// Player window functions.
//==============================================================================

// Forward declaration of callback functions.

static void
key_event_callback(byte key_code, bool key_down);

static void
timer_event_callback(void);

static void
mouse_event_callback(int x, int y, int button_code);

static void
resize_event_callback(void *window_handle, int width, int height);

static void
display_event_callback(void);

//------------------------------------------------------------------------------
// Create the player window.
//------------------------------------------------------------------------------

static bool
create_player_window()
{
	// Tell the player thread we're about to create the player window.

	player_window_init_requested.send_event(true);

	// Create the main window.  If this fails, signal the player thread of
	// failure.

	if (!create_main_window(key_event_callback, mouse_event_callback,
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

//------------------------------------------------------------------------------
// Destroy the player window.
//------------------------------------------------------------------------------

static void
destroy_player_window()
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

	// Create the player window.

	if (!create_player_window()) {
		// TODO: Quit the app?
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

void
display_file_as_web_page(const char *file_path)
{
	FILE *fp;
	string URL;

	// If there is no file, there is nothing to display.

	if ((fp = fopen(file_path, "r")) == NULL)
		return;
	fclose(fp);

	// Construct the URL to the file.

	URL = "file:///";
	URL += file_path;

	// Open the URL in the default external app.

	open_URL_in_default_app(URL);
}

//------------------------------------------------------------------------------
// Display a web page.
//------------------------------------------------------------------------------

void
display_web_page(const char *URL)
{
	open_URL_in_default_app(URL);
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
		if (hardware_acceleration_flag != hardware_acceleration)
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
// Show the options window.
//------------------------------------------------------------------------------

void
show_options_window()
{
	download_sounds_flag = download_sounds.get();
	hardware_acceleration_flag = hardware_acceleration;
	old_visible_block_radius = visible_block_radius.get();
	old_user_debug_level = user_debug_level.get();
	open_options_window(download_sounds_flag, old_visible_block_radius,
		old_user_debug_level, options_window_callback);
}

void
show_light_window()
{
	open_light_window(master_brightness.get(), light_window_callback);
}

//------------------------------------------------------------------------------
// Callback function to handle mouse events.
//------------------------------------------------------------------------------

static void
mouse_event_callback(int x, int y, int button_code)
{
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
	// If the application window has been minimised, send a pause event to the
	// player thread.

	if (!app_window_minimised && app_window_is_minimised()) {
		app_window_minimised = true;
		pause_player_thread.send_event(true);
	}

	// If the app window has been restored, send a resume event to the
	// player thread.

	if (app_window_minimised && !app_window_is_minimised()) {
		app_window_minimised = false;
		resume_player_thread.send_event(true);
	}

	// If the app window is not minimised and a spot is loaded, update the
	// cursor position.

	if (!app_window_minimised && spot_loaded.get()) {
		int x, y;

		get_mouse_position(&x, &y, true);
		update_mouse_cursor(x, y);
		set_mouse_cursor();
	}

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
}

//------------------------------------------------------------------------------
// Resize callback procedure.
//------------------------------------------------------------------------------

static void
resize_event_callback(void *window_handle, int width, int height)
{
	if (!spot_loaded.get()) {
		return;
	}

	// Signal the player thread that a window resize is requested, and
	// wait for the player thread to signal that the player window has shut 
	// down.

	window_resize_requested.send_event(true);
	player_window_shut_down.wait_for_event();

	// Destroy the label texture.

	destroy_label_texture();

	// Destroy the frame buffer if hardware acceleration is not enabled.

	if (!hardware_acceleration)
		destroy_frame_buffer();

	// Resize the main window.

	set_main_window_size(width, height);

	// Recreate the label texture.

	create_label_texture();

	// Compute the window "centre" coordinates.

	window_half_width = window_width * 0.5f;
	window_top_half_height = window_height * 0.75f;
	window_bottom_half_height = window_height * 0.25f;

	// Create the frame buffer, and signal the player thread on it's success
	// or failure.  On failure, wait for the player thread to signal that the
	// player window has shut down, then destroy the main window.

	if ((hardware_acceleration && recreate_frame_buffer()) ||
		(!hardware_acceleration && create_frame_buffer()))
		main_window_resized.send_event(true);
	else {
		main_window_resized.send_event(false);
		player_window_shut_down.wait_for_event();
		destroy_main_window();
		// TODO: Quit the app?
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

	// Create the player window.

	hardware_acceleration = (acceleration_mode == TRY_HARDWARE);
	if (!create_player_window()) {
		// TODO: Quit the app?
	}

	// Reset the title, and label if there is one.

	set_title(NULL);
	show_label(NULL);
}

//------------------------------------------------------------------------------
// Quit callback procedure.
//------------------------------------------------------------------------------

static void shut_down_app();

static void
quit_callback()
{
	destroy_player_window();
	shut_down_app();
}

//------------------------------------------------------------------------------
// Open a local file.
//------------------------------------------------------------------------------

void
open_local_file(char *file_path)
{
	bool result;
	char cached_file_path[256];

	URL_was_opened.reset_event();
	URL_was_opened.send_event(true);
	*cached_file_path = '\0';
	result = download_URL_to_file(file_path, cached_file_path, 256);
	downloaded_URL.set(file_path);
	downloaded_file_path.set(cached_file_path);
	URL_was_downloaded.send_event(true);
}

//------------------------------------------------------------------------------
// Downloader thread.
//------------------------------------------------------------------------------

void
downloader_thread(void *arg_list)
{
	// Keep this thread alive until requested to terminate.

	while (!downloader_thread_termination_requested.event_sent()) {

		// Wait for a URL download request to be signalled by the player thread.

		if (URL_download_requested.event_sent()) {
			bool result;
			char file_path[256];

			// If a target window was requested, simply open the URL in the default app.

			if (requested_target != "") {
				open_URL_in_default_app(requested_URL);
			}

			// Reset the URL_was_opened flag, and send off the URL request to the
			// browser.

			else {
				URL_was_opened.reset_event();
				URL_was_opened.send_event(true);
				strcpy(file_path, (char *)requested_file_path);
				result = download_URL_to_file(requested_URL, file_path, 256);
				downloaded_URL.set(requested_URL);
				downloaded_file_path.set(file_path);
				URL_was_downloaded.send_event(result);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Run the app.
//------------------------------------------------------------------------------

int
run_app(void *instance_handle, int show_command)
{
	// Start the memory trace.

#if MEM_TRACE
	start_trace();
#endif

	// Indicate Rover hasn't yet started up successfully.

	rover_started_up = false;

	// Start up the platform API.

	if (!start_up_platform_API(instance_handle, show_command, quit_callback))
		return(false);

	// Create the events sent by the player thread.

	player_thread_initialised.create_event();
	player_window_initialised.create_event();
	URL_download_requested.create_event();
	URL_cancel_requested.create_event();
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
	downloader_thread_termination_requested.create_event();
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
	// and the app window is not minimised.

	player_active = false;
	player_window_created = false;
	app_window_minimised = false;

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

	// Start the player thread and the downloader thread.

	if ((player_thread_handle = start_thread(player_thread)) == 0)
		return(false);
	if ((downloader_thread_handle = start_thread(downloader_thread)) == 0)
		return(false);

	// Wait for an event from the player thread regarding it's initialisation.

	player_active = player_thread_initialised.wait_for_event();

	// Create the player window.

	if (!create_player_window()) {
		return false;
	}

	// Open the splash spot.

	string splash_file_path = flatland_dir + "splash.3dml";
	open_local_file(splash_file_path);

	// Indicate Rover started up sucessfully.

	rover_started_up = true;

	// Run the event loop until the user chooses to quit.

	return run_event_loop();
}

//------------------------------------------------------------------------------
// Shut down the app.
//------------------------------------------------------------------------------

static void
shut_down_app()
{
	// If the player thread is active, signal it to terminate, and wait for it
	// to do so.

	if (player_active) {
		player_window_init_requested.send_event(false);
		wait_for_thread_termination(player_thread_handle);
	}

	// Signal the downloader thread to terminate, and wait for it to do so.

	downloader_thread_termination_requested.send_event(true);
	wait_for_thread_termination(downloader_thread_handle);

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
	downloader_thread_termination_requested.destroy_event();
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