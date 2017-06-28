//******************************************************************************
// $Header$
// Copyright (C) 1998-2000 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

// Version string.

#define ROVER_VERSION_NUMBER			0x03060001 // ff at the end usually unless beta

// Activation status.

#define ERROR_STATUS		0
#define EXPIRED_STATUS		1
#define DEMO_STATUS			2
#define ACTIVATED_STATUS	3

// Activation check modes.

#define SILENT_CHECK		0
#define NORMAL_CHECK		1
#define DISPLAY_DIALOG		2

// Option window control IDs.

#define OK_BUTTON						0
#define	CANCEL_BUTTON					1
#define DOWNLOAD_SOUNDS_CHECKBOX		2
#define ENABLE_3D_ACCELERATION_CHECKBOX	3
#define	VISIBLE_RADIUS_EDITBOX			4
#define	DEBUG_LEVEL_OPTION				5

// Command menu item IDs.

#define ABOUT_ROVER_COMMAND				1
#define ROVER_HELP_COMMAND				2
#define DOWNLOAD_ROVER_COMMAND			5
#define VIEW_3DML_SOURCE_COMMAND		6
#define SAVE_3DML_SOURCE_COMMAND		7
#define TAKE_SNAPSHOT_COMMAND			8
#define MANAGE_BLOCKSETS_COMMAND		9

// Snapshot positions.

#define CURRENT_VIEW		0
#define TOP_NW_CORNER		1
#define TOP_NE_CORNER		2
#define TOP_SW_CORNER		3
#define TOP_SE_CORNER		4

// Key codes for non-alphabetical keys (ASCII codes used for 'A' to 'Z' and
// '0' to '9').

#define KEY_CODES			32
#define ESC_KEY				1
#define SHIFT_KEY			2
#define CONTROL_KEY			3
#define ALT_KEY				4
#define SPACE_BAR_KEY		5
#define BACK_SPACE_KEY		6
#define ENTER_KEY			7
#define INSERT_KEY			8
#define DELETE_KEY			9
#define HOME_KEY			10
#define END_KEY				11
#define PAGE_UP_KEY			12
#define PAGE_DOWN_KEY		13
#define UP_KEY				14
#define DOWN_KEY			15
#define LEFT_KEY			16
#define RIGHT_KEY			17
#define NUMPAD_0_KEY		18
#define NUMPAD_1_KEY		19
#define NUMPAD_2_KEY		20
#define NUMPAD_3_KEY		21
#define NUMPAD_4_KEY		22
#define NUMPAD_5_KEY		23
#define NUMPAD_6_KEY		24
#define NUMPAD_7_KEY		25
#define NUMPAD_8_KEY		26
#define NUMPAD_9_KEY		27
#define NUMPAD_ADD_KEY		28
#define NUMPAD_SUBTRACT_KEY	29
#define NUMPAD_MULTIPLY_KEY	30
#define NUMPAD_DIVIDE_KEY	31
#define NUMPAD_PERIOD_KEY	32

// Button codes (not all platforms support a two-button mouse, so a keyboard
// modifier may be used as a substitute for the right button).

#define MOUSE_MOVE_ONLY		0
#define LEFT_BUTTON_DOWN	1
#define LEFT_BUTTON_UP		2
#define RIGHT_BUTTON_DOWN	3
#define RIGHT_BUTTON_UP		4

// Media players supported.

#define ANY_PLAYER				0
#define REAL_PLAYER				1
#define WINDOWS_MEDIA_PLAYER	2

//------------------------------------------------------------------------------
// Event class.
//------------------------------------------------------------------------------

struct event {
	void *event_handle;
	bool event_value;

	event();
	~event();
	void create_event(void);			// Create event.
	void destroy_event(void);			// Destroy event.
	void send_event(bool value);		// Send event.
	void reset_event(void);				// Reset event.
	bool event_sent(void);				// Check if event was sent.
	bool wait_for_event(void);			// Wait for event.
};

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------

// Application path.

extern string app_dir;

// Display and texture pixel formats.

extern pixel_format display_pixel_format;
extern pixel_format texture_pixel_format;

// Display properties.

extern int display_width, display_height, display_depth;
extern int window_width, window_height;

// Flag indicating whether the main window is ready.

extern bool main_window_ready;

// Flag indicating whether sound is available and enabled.

extern bool sound_available;
extern bool sound_on;

//------------------------------------------------------------------------------
// Externally visible classes and functions.
//------------------------------------------------------------------------------

// Semaphore functions.

void *
create_semaphore(void);

void
destroy_semaphore(void *semaphore_handle);

void
raise_semaphore(void *semaphore_handle);

void
lower_semaphore(void *semaphore_handle);

// Semaphore template class.

template <class A>
class semaphore {
private:
	A var;
	void *semaphore_handle;

public:
	semaphore()
	{
		semaphore_handle = NULL;
	}

	~semaphore()
	{
	}

	void create_semaphore(void)
	{
		semaphore_handle = ::create_semaphore();
	}

	void destroy_semaphore(void)
	{
		::destroy_semaphore(semaphore_handle);
	}

	A get(void)
	{
		raise_semaphore(semaphore_handle);
		A temp_var = var;
		lower_semaphore(semaphore_handle);
		return(temp_var);
	}

	void set(A value)
	{
		raise_semaphore(semaphore_handle);
		var = value;
		lower_semaphore(semaphore_handle);
	}
};

// Start up/shut down functions (called by the plugin thread only).

bool
start_up_platform_API(void *instance_handle, int show_command, void (*quit_callback)());

void
shut_down_platform_API(void);

// Main event loop.

int
run_event_loop();

// Thread functions.

unsigned long
start_thread(void (*thread_func)(void *arg_list));

void
wait_for_thread_termination(unsigned long thread_handle);

void
decrease_thread_priority(void);

// Main window functions (called by the plugin thread only).

void
set_main_window_size(int width, int height);

bool
create_main_window(void (*key_callback)(byte key_code, bool key_down),
				   void (*mouse_callback)(int x, int y, int button_code),
				   void (*timer_callback)(void),
   				   void (*resize_callback)(void *window_handle, int width,
										   int height),
				   void (*display_callback)(void));

void
resize_main_window();

void
destroy_main_window(void);

// Application window function (called by the plugin thread only).

bool
app_window_is_minimised(void);

// Message functions.

void
debug_message(char *format, ...);

void
fatal_error(char *title, char *format, ...);

void
information(char *title, char *format, ...);

bool
query(char *title, bool yes_no_format, char *format, ...);

// URL functions.

void
open_URL_in_default_app(const char *URL);

bool
download_URL_to_file(const char *URL, char *file_path_buffer, int buffer_size);

// Open file and URL dialogs.

bool
open_file_dialog(char *file_path_buffer, int buffer_size);

bool
open_URL_dialog(string *URL_ptr);

// Progress window functions (called by the plugin thread only).

void
open_progress_window(int file_size, void (*progress_callback)(void),
					 char *format, ...);

void
update_progress_window(int file_pos, int file_size);

void
close_progress_window(void);

// Light window functions (called by the plugin thread only).

void
open_light_window(float brightness, void (*light_callback)(float brightness,
				  bool window_closed));

void
close_light_window(void);

// Options window functions (called by the plugin thread only).

void
open_options_window(bool download_sounds_value, int visible_radius_value,
					int user_debug_level_value,
					void (*options_callback)(int option_ID, int option_value));

void
close_options_window(void);

// About window functions (called by the plugin thread only).

void
open_about_window(void);

void
close_about_window(void);

// Help window functions (called by the plugin thread only).

void
open_help_window(void);

void
close_help_window(void);

// Snapshot window functions (called by the plugin thread only).

void
open_snapshot_window(int width, int height, void (*snapshot_callback)(int width,
					 int height, int position));

void
close_snapshot_window(void);

// Filename function (called by the plugin thread only).

const char *
get_save_file_name(char *title, char *filter, char *initial_dir_path);

// Blockset manager window functions (called by the plugin thread only).

void
open_blockset_manager_window(void);

void
close_blockset_manager_window(void);

#ifdef STREAMING_MEDIA

// Password window function (called by the stream thread only).

bool
get_password(string *username_ptr, string *password_ptr);

#endif

// Label texture function.

bool
create_label_texture(void);

void
destroy_label_texture(void);

// Frame buffer functions.

bool
create_frame_buffer(void);

bool
recreate_frame_buffer(void);

void
destroy_frame_buffer(void);

void
begin_3D_scene(void);

void
end_3D_scene(void);

bool
lock_frame_buffer(byte *&frame_buffer_ptr, int &frame_buffer_width);

void
unlock_frame_buffer(void);

bool
display_frame_buffer(bool show_splash_graphic);

void
clear_frame_buffer(int x, int y, int width, int height);

bool
save_frame_buffer(byte *image_buffer, int width, int height);

// Software rendering functions (called by the player thread only).

void
create_lit_image(cache_entry *cache_entry_ptr, int image_dimensions);

void
render_colour_span16(span *span_ptr);

void
render_colour_span24(span *span_ptr);

void
render_colour_span32(span *span_ptr);

void
render_opaque_span16(span *span_ptr);

void
render_opaque_span24(span *span_ptr);

void
render_opaque_span32(span *span_ptr);

void
render_transparent_span16(span *span_ptr);

void
render_transparent_span24(span *span_ptr);

void
render_transparent_span32(span *span_ptr);

void
render_popup_span16(span *span_ptr);

void
render_popup_span24(span *span_ptr);

void
render_popup_span32(span *span_ptr);


// Hardware rendering functions (called by the player thread only).

void
hardware_init_vertex_list(void);

bool
hardware_create_vertex_list(int max_vertices);

void
hardware_destroy_vertex_list(int max_vertices);

void
hardware_set_projection_transform(float horz_field_of_view,
								  float vert_field_of_view,
								  float near_z, float far_z);

void
hardware_enable_fog(void);

void
hardware_disable_fog(void);

void
hardware_update_fog_settings(fog *fog_ptr);

void *
hardware_create_texture(int image_size_index);

void
hardware_destroy_texture(void *hardware_texture_ptr);

void
hardware_set_texture(cache_entry *cache_entry_ptr);

void
hardware_render_2D_polygon(pixmap *pixmap_ptr, RGBcolour colour, 
						   float brightness, float x, float y, float width,
						   float height, float start_u, float start_v, 
						   float end_u, float end_v, bool disable_transparency);

void
hardware_render_polygon(spolygon *spolygon_ptr);

// Pixmap drawing function (called by the player thread only).

void
draw_pixmap(pixmap *pixmap_ptr, int brightness_index, int x, int y,
			int width, int height);

// Colour functions (called by the player thread only).

pixel
RGB_to_display_pixel(RGBcolour colour);

void
display_pixel_to_RGB(pixel display_pixel, byte *red_ptr, byte *green_ptr, 
					 byte *blue_ptr); 

pixel
RGB_to_texture_pixel(RGBcolour colour);

RGBcolour *
get_standard_RGB_palette(void);

byte
get_standard_palette_index(RGBcolour colour_ptr);

// Task bar functions (called by the player thread only).

const char *
get_title(void);

void
set_title(char *format, ...);

// URL functions (called by the player thread only).

void
show_label(const char *label);

void
hide_label(void);

// Popup function (called by the player thread only).

void
init_popup(popup *popup_ptr);

// Cursor and mouse functions (called by the plugin thread only).

void
get_mouse_position(int *x, int *y, bool relative);

void
set_movement_cursor(arrow movement_arrow);

void
set_crosshair_cursor(void);

void
set_hand_cursor(void);

void
set_arrow_cursor(void);

void
capture_mouse(void);

void
release_mouse(void);

// Time function.

int
get_time_ms(void);

// Functions to load wave data.

bool
load_wave_data(wave *wave_ptr, char *wave_file_buffer, int wave_file_size);

void
destroy_wave_data(wave *wave_ptr);

// Functions to create and destroy sound buffers (called by the player thread 
// and stream thread).

void
update_sound_buffer(void *sound_buffer_ptr, char *data_ptr, int data_size,
					int data_start);

bool
create_sound_buffer(sound *sound_ptr);

void
destroy_sound_buffer(sound *sound_ptr);

// Functions to control playing of sounds (called by the player thread only).

void
set_sound_volume(sound *sound_ptr, float volume);

void
play_sound(sound *sound_ptr, bool looped);

void
stop_sound(sound *sound_ptr);

void
update_sound(sound *sound_ptr, vertex *translation_ptr);

#ifdef STREAMING_MEDIA

// Functions to control playing of streaming media.

void
init_video_textures(int video_width, int video_height, int pixel_format);

void
draw_frame(byte *image_ptr);

bool
stream_ready(void);

bool
download_of_rp_requested(void);

bool
download_of_wmp_requested(void);

void
start_streaming_thread(void);

void
stop_streaming_thread(void);

#endif // STREAMING_MEDIA