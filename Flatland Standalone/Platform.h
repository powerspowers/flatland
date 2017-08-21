//******************************************************************************
// $Header$
// Copyright (C) 1998-2000 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

// Version string.

#define ROVER_VERSION_NUMBER	0x03060012

// Option window control IDs.

#define OK_BUTTON							0
#define	CANCEL_BUTTON						1
#define CLASSIC_CONTROLS_CHECKBOX			2
#define	VIEW_RADIUS_SLIDER					3
#define MOVE_RATE_SLIDER					4
#define TURN_RATE_SLIDER					5
#define	DEBUG_LEVEL_OPTION					6
#define FORCE_SOFTWARE_RENDERING_CHECKBOX	7

// Command menu item IDs.

#define ABOUT_ROVER_COMMAND				1
#define ROVER_HELP_COMMAND				2
#define DOWNLOAD_ROVER_COMMAND			3
#define VIEW_3DML_SOURCE_COMMAND		4
#define SAVE_3DML_SOURCE_COMMAND		5
#define MANAGE_BLOCKSETS_COMMAND		6

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

// Falling acceleration and maximum speed, and jump deacceleration and maximum speed.

#define FALLING_ACCELERATION	(UNITS_PER_BLOCK * 10.0f)
#define MAXIMUM_FALLING_SPEED	(UNITS_PER_BLOCK * 20.0f)
#define JUMPING_DEACCELERATION	(UNITS_PER_BLOCK * 100.0f)
#define MAXIMUM_JUMPING_SPEED	(UNITS_PER_BLOCK * 20.0f)

// Builder icon size.

#define BUILDER_ICON_WIDTH	128
#define BUILDER_ICON_HEIGHT	128

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

// Largest texture size permitted.

extern int max_texture_size;

// Display properties.

extern int window_width;
extern int window_height;
extern float half_window_width;
extern float half_window_height;

// Flag indicating whether the main window is ready.

extern bool main_window_ready;

// Flag indicating whether sound is on.

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
download_URL_to_file(const char *URL, char *file_path_buffer, bool no_cache);

// Open file and URL dialogs.

bool
open_file_dialog(char *file_path_buffer, int buffer_size);

bool
open_URL_dialog(string *URL_ptr);

// Light window functions.

void
open_light_window(float brightness, void (*light_callback)(float brightness,
				  bool window_closed));

void
close_light_window(void);

// Options window functions.

void
open_options_window(int viewing_distance_value, bool use_classic_controls_value,
					int move_rate_value, int turn_rate_value, int user_debug_level_value, bool force_software_rendering,
					void (*options_callback)(int option_ID, int option_value));

void
close_options_window(void);

// About window functions.

void
open_about_window(void);

void
close_about_window(void);

// Help window functions.

void
open_help_window(void);

void
close_help_window(void);

// Blockset manager window functions.

void
open_blockset_manager_window(void);

void
close_blockset_manager_window(void);

// Builder window functions.

void
open_builder_window();

void
close_builder_window();

#ifdef STREAMING_MEDIA

// Password window function (called by the stream thread only).

bool
get_password(string *username_ptr, string *password_ptr);

#endif

// Bitmap functions.

bitmap *
create_bitmap_from_texture(texture *texture_ptr);

bitmap *
create_bitmap_from_builder_render_target();

void
destroy_bitmap_handle(void *bitmap_handle);

// Label texture function.

bool
create_label_texture(void);

void
destroy_label_texture(void);

// Frame buffer functions.

void
select_main_render_target();

void
select_builder_render_target();

bool
create_frame_buffer(void);

bool
recreate_frame_buffer(void);

void
destroy_frame_buffer(void);

bool
lock_frame_buffer(void);

void
unlock_frame_buffer(void);

bool
display_frame_buffer(void);

void
clear_frame_buffer(void);

void
clear_builder_frame_buffer();

// Software rendering functions (called by the player thread only).

bool
create_lit_image(cache_entry *cache_entry_ptr, int image_dimensions);

void
set_lit_image(cache_entry *cache_entry_ptr, int image_dimensions);

void
render_colour_span(span *span_ptr);

void
render_transparent_span(span *span_ptr);

void
render_popup_span(span *span_ptr);

void
render_lines(spoint *spoint_list, int spoints, RGBcolour colour);

// Function to determine intersection of the mouse with a polygon.

bool
mouse_intersects_with_polygon(float mouse_x, float mouse_y, vector *camera_direction_ptr, tpolygon *tpolygon_ptr);

// Hardware rendering functions (called by the player thread only).

void
hardware_set_projection_transform(float viewport_width, float viewport_height, float near_z, float far_z);

void
hardware_update_fog_settings(bool enabled, fog *fog_ptr, float max_radius);

void *
hardware_create_texture(int image_size_index);

void
hardware_destroy_texture(void *hardware_texture_ptr);

void
hardware_set_texture(cache_entry *cache_entry_ptr);

void
hardware_render_2D_polygon(pixmap *pixmap_ptr, RGBcolour colour, float brightness, 
					       float sx, float sy, float width, float height, 
						   float start_u, float start_v, float end_u, float end_v);

void
hardware_render_polygon(tpolygon *tpolygon_ptr);

void
hardware_render_lines(vertex *vertex_list, int vertices, RGBcolour colour);

// Pixmap drawing function (called by the player thread only).

void
draw_pixmap(pixmap *pixmap_ptr, int brightness_index, int x, int y, int width, int height);

// Colour functions (called by the player thread only).

pixel
RGB_to_display_pixel(RGBcolour colour);

pixel
RGB_to_texture_pixel(RGBcolour colour);

byte
get_standard_palette_index(RGBcolour colour_ptr);

// Task bar functions (called by the player thread only).

const char *
get_title(void);

void
set_title(char *format, ...);

void
set_status_text(char *format, ...);

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
enable_mouse_look_mode(void);

void
disable_mouse_look_mode(void);

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

bool
stream_ready(void);

bool
download_of_wmp_requested(void);

void
start_streaming_thread(void);

void
stop_streaming_thread(void);

#endif // STREAMING_MEDIA