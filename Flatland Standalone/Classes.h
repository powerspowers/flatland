//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

// If symbolic debugging is enabled, disable the static keyword so that all
// variables and functions are exported into the symbol table.

#ifdef SYMBOLIC_DEBUG
#define static
#endif

#include "collision\collision.h"

// Type definitions for various integer types.

typedef unsigned char byte;
typedef unsigned char imagebyte;	// Used for memory tracing.
typedef unsigned char cachebyte;	// Used for memory tracing.
typedef unsigned char colmeshbyte;	// Used for memory tracing.
typedef unsigned int dword;
typedef unsigned short word;
typedef unsigned int pixel;

// Type definition and macros for fixed point values.

typedef int fixed;
#define FRAC_BITS	16
#define INT_MASK	0xffff0000

// XML entity types.

#define TEXT_ENTITY		0
#define COMMENT_ENTITY	1
#define TAG_ENTITY		2

// Minimum delays before checking for an update.

#define SECONDS_PER_MINUTE			60
#define SECONDS_PER_HOUR			3600
#define SECONDS_PER_DAY				86400
#define SECONDS_PER_WEEK			604800

// Maximum string size.

#define	STRING_SIZE					256

// Default field of view.

#define DEFAULT_FIELD_OF_VIEW		60.0f

// Clipping plane indices.

#define NEAR_CLIPPING_PLANE			0
#define FAR_CLIPPING_PLANE			4

// Frustrum vertex and plane indices.

#define FRUSTUM_VERTICES			8
#define FRUSTUM_NEAR_PLANE			0
#define FRUSTUM_FAR_PLANE			1
#define FRUSTUM_LEFT_PLANE			2
#define FRUSTUM_RIGHT_PLANE			3
#define FRUSTUM_TOP_PLANE			4
#define FRUSTUM_BOTTOM_PLANE		5
#define FRUSTUM_PLANES				6

// Task bar height.

#define TASK_BAR_HEIGHT				20

// Number of texels per unit (block size in texels is 256, and in world
// units is 3.2).

#define UNITS_PER_BLOCK				3.2f
#define UNITS_PER_HALF_BLOCK		1.6f
#define	TEXELS_PER_UNIT				80.0f

// First and last block symbol characters, the NULL block symbol, and the
// ground block symbol.

#define FIRST_BLOCK_SYMBOL		'!'
#define LAST_BLOCK_SYMBOL		'~'	
#define NULL_BLOCK_SYMBOL		'.'
#define GROUND_BLOCK_SYMBOL		' '

// Trinometry macros.

#define PI			3.141592654f
#define RAD(x)		((x) / 180.0f * PI)
#define DEG(x)		((x) / PI * 180.0f)

// Integer macros.

#define MIN(x,y)	((x) < (y) ? (x) : (y))
#define MAX(x,y)	((x) > (y) ? (x) : (y))

// Mathematical macros, using an epsilon value for cancelling out small errors
// in precision.

#define EPSILON		0.0001f
#define INV_EPSILON	10000.0f
#define FLT(x,y)	((x) < ((y) - EPSILON))
#define FLE(x,y)	((x) <= ((y) + EPSILON))
#define FGT(x,y)	((x) > ((y) + EPSILON))
#define FGE(x,y)	((x) >= ((y) - EPSILON))
#define FABS(x)		((x) < 0.0f ? -(x) : (x))
#define FEQ(x,y)	(FABS((x) - (y)) < EPSILON)
#define FNE(x,y)	(FABS((x) - (y)) >= EPSILON)
#define FMIN(x,y)	(FLT(x, y) ? (x) : (y))
#define FMAX(x,y)	(FGT(x, y) ? (x) : (y))
#define FINT(x)		((float)((int)(x)))
#define FCEIL(x)	((x) == FINT(x) ? FINT(x) : FINT(x) + 1.0f)
#define INT(x)		(FLT((x) - FINT(x), 1.0f) ? (int)(x) : (int)(x) + 1) 
#define FROUND(x)   ((x) - FINT(x) > 0.5f ? FINT(x) + 1 : FINT(x))

// Macros to simplify getting vertices from a particular polygon and block.

#define PREPARE_VERTEX_LIST(block_ptr) \
	vertex *vertex_list = block_ptr->vertex_list

#define PREPARE_VERTEX_DEF_LIST(polygon_def_ptr) \
	vertex_def *vertex_def_list = polygon_def_ptr->vertex_def_list

#define VERTEX(n)			vertex_list[vertex_def_list[n].vertex_no]
#define VERTEX_PTR(n)		&VERTEX(n)

// Compass directions.  The ordering is such that EAST + 3, SOUTH + 3 and
// UP + 3 give the opposite compass directions, and the first three directions
// are those of the simplified polygon adjacency tests, in the order they are
// applied.

#define EAST	0
#define SOUTH	1
#define UP		2
#define WEST	3
#define NORTH	4
#define DOWN	5
#define NONE	6

// Arrow cursors.

enum arrow {
	ARROW_N, ARROW_NE, ARROW_E, ARROW_SE, ARROW_S, ARROW_SW, ARROW_W, ARROW_NW
};

// Texture styles.

#define TILED_TEXTURE		0
#define SCALED_TEXTURE		1
#define STRETCHED_TEXTURE	2

// Light styles.

#define DIRECTIONAL_LIGHT		0
#define STATIC_POINT_LIGHT		1
#define PULSATING_POINT_LIGHT	2
#define STATIC_SPOT_LIGHT		3
#define REVOLVING_SPOT_LIGHT	4
#define SEARCHING_SPOT_LIGHT	5

// Fog styles.

#define LINEAR_FOG		0
#define EXPONENTIAL_FOG	1

// Ripple styles.

#define RAIN_RIPPLE		0
#define WAVES_RIPPLE	1

// Audio styles.

enum audiostyle { PASS_AUDIO, OCCLUDE_AUDIO };

// Map styles.

#define SINGLE_MAP	0
#define DOUBLE_MAP	1

// Action types.

#define REPLACE_ACTION		0
#define EXIT_ACTION			1
#define RIPPLE_ACTION		2
#define SPIN_ACTION			3
#define ORBIT_ACTION		4
#define MOVE_ACTION			5
#define SETFRAME_ACTION		6
#define ANIMATE_ACTION		7
#define SETLOOP_ACTION		8
#define STOPSPIN_ACTION		9
#define STOPMOVE_ACTION		10
#define STOPRIPPLE_ACTION	11
#define STOPORBIT_ACTION	12

// Trigger flags.

#define ROLL_ON			1
#define ROLL_OFF		2
#define ROLLOVER		4
#define CLICK_ON		8
#define STEP_ON			16
#define STEP_IN			32
#define STEP_OUT		64
#define PROXIMITY		128
#define TIMER			256
#define EVERYWHERE		512
#define LOCATION		1024
#define KEY_DOWN		2048
#define KEY_UP			4096
#define KEY_HOLD		8192
#define START_UP		16384
#define	MOUSE_TRIGGERS	(ROLL_ON | ROLL_OFF | ROLLOVER | CLICK_ON)
#define KEY_TRIGGERS	(KEY_DOWN | KEY_UP | KEY_HOLD)
#define RADIUS_TRIGGERS	(STEP_IN | STEP_OUT | PROXIMITY)
#define TIMER_TRIGGERS	(TIMER | KEY_HOLD)
#define PART_TRIGGERS	(ROLL_ON | CLICK_ON)

// Alignments.

#define TOP_LEFT		0
#define TOP				1
#define TOP_RIGHT		2
#define LEFT			3
#define CENTRE			4
#define RIGHT			5
#define BOTTOM_LEFT		6
#define BOTTOM			7
#define BOTTOM_RIGHT	8
#define MOUSE			9

// Block types.

#define STRUCTURAL_BLOCK	1
#define MULTIFACETED_SPRITE	2
#define ANGLED_SPRITE		4
#define REVOLVING_SPRITE	8
#define FACING_SPRITE		16
#define SPRITE_BLOCK		(MULTIFACETED_SPRITE | ANGLED_SPRITE | \
							 REVOLVING_SPRITE | FACING_SPRITE)

// Playback mode of a sound.

#define LOOPED_PLAY	0
#define RANDOM_PLAY	1
#define SINGLE_PLAY	2
#define ONE_PLAY	3

// Imagemap shapes.

#define RECT_SHAPE		0
#define CIRCLE_SHAPE	1

// Number of brightness levels.

#define BRIGHTNESS_LEVELS		8
#define	MAX_BRIGHTNESS_INDEX	(BRIGHTNESS_LEVELS - 1)

// Number of image sizes, and maximum texture size supported.

#define IMAGE_SIZES				10
#define MAX_TEXTURE_SIZE		1024

// Rate changes.

#define RATE_SLOWER			-1
#define RATE_FASTER			 1

// Number of blocks per audio block.

#define AUDIO_BLOCK_DIMENSIONS	4
#define AUDIO_BLOCK_SHIFT		2

// Video pixel formats.

#define RGB24	0
#define YUV12	1
#define RGB16	2

// constant for all parts selected in a trigger 
#define ALL_PARTS	-1

// Property and method types.

enum ptype { MAP_PROP, READY_PROP };
enum mtype { EXIT_METHOD, AMBIENT_LIGHT_METHOD };

//==============================================================================
// Basic data classes.
//==============================================================================

//------------------------------------------------------------------------------
// String class.  Semantically it behaves as a character array, only one that
// has a variable length.
//------------------------------------------------------------------------------

class string {
private:

public:
	char *text;
	string();
	string(char new_char);
	string(char *new_text);
	string(const char *new_text);
	string(const string &old);
	~string();
	operator char *();
	char& operator[](int index);
	string& operator=(const string &old);
	string& operator +=(char add_char);
	string& operator +=(const char *add_text);
	string operator +(char add_char);
	string operator +(const char *add_text);
	void truncate(unsigned int new_length);
	void copy(const char *new_text, unsigned int new_length);
	void append(const char *add_text, unsigned int add_length);
	void write(FILE *fp);
};

//------------------------------------------------------------------------------
// RGB colour class.
//------------------------------------------------------------------------------

struct RGBcolour {
	float red, green, blue, alpha;	// Colour components.

	RGBcolour();
	~RGBcolour();
	void set(RGBcolour colour);
	void set_RGB(float red, float green, float blue, float alpha = 255.0f);
	void clamp(void);
	void adjust_brightness(float brightness);
	void normalise(void);
	void blend(RGBcolour colour);
};

//------------------------------------------------------------------------------
// Fog class.
//------------------------------------------------------------------------------

struct fog {
	int style;
	RGBcolour colour;
	float start_radius;
	float end_radius;
	float density;
};

//------------------------------------------------------------------------------
// Pixel format class.
//------------------------------------------------------------------------------

struct pixel_format {
	pixel red_mask, green_mask, blue_mask;
	int red_right_shift, green_right_shift, blue_right_shift;
	int red_left_shift, green_left_shift, blue_left_shift;
	pixel alpha_comp_mask;
};

//------------------------------------------------------------------------------
// Texture coordinates class.
//------------------------------------------------------------------------------

struct texcoords {
	float u, v;

	texcoords();
	~texcoords();
};

//------------------------------------------------------------------------------
// Map coordinates class.
//------------------------------------------------------------------------------

struct mapcoords {
	int column, row, level;

	mapcoords();
	~mapcoords();
	void set(int set_column, int set_row, int set_level);
};

//------------------------------------------------------------------------------
// Relative integer class.
//------------------------------------------------------------------------------

struct relinteger {
	int value;
	bool relative_value;

	relinteger();
	~relinteger();
	void set(int set_value,	bool set_relative_value);
};

//------------------------------------------------------------------------------
// Relative integer triplet class .
//------------------------------------------------------------------------------

struct relinteger_triplet {
	int x, y, z;
	bool relative_x, relative_y, relative_z;

	relinteger_triplet();
	~relinteger_triplet();
	void set(int set_x, int set_y, int set_z,
		bool set_relative_x, bool set_relative_y, 
		bool set_relative_z);
};

//------------------------------------------------------------------------------
// Relative coordinates class.
//------------------------------------------------------------------------------

struct relcoords {
	int column, row, level;
	bool relative_column, relative_row, relative_level;

	relcoords();
	~relcoords();
	void set(int set_column, int set_row, int set_level,
		bool set_relative_column, bool set_relative_row, 
		bool set_relative_level);
};

//------------------------------------------------------------------------------
// Size class.
//------------------------------------------------------------------------------

struct size {
	int width, height;
};

//------------------------------------------------------------------------------
// Orientation class.
//------------------------------------------------------------------------------

struct orientation {
	float angle_x, angle_y, angle_z;
	int direction;						// NONE if (x,y,z) orientation defined.
	float angle;						// Angle around direction's axis.

	orientation();
	~orientation();
	void set(float set_angle_x, float set_angle_y, float set_angle_z);
};

//------------------------------------------------------------------------------
// Direction class.
//------------------------------------------------------------------------------

struct direction {
	float angle_x, angle_y;

	direction();
	~direction();
	void set(float set_angle_x, float set_angle_y);
};

//------------------------------------------------------------------------------
// Direction range class.
//------------------------------------------------------------------------------

struct dirrange {
	direction min_direction;
	direction max_direction;
};

//------------------------------------------------------------------------------
// Percentage range class.
//------------------------------------------------------------------------------

struct pcrange {
	float min_percentage;
	float max_percentage;

	pcrange();
	~pcrange();
};

//------------------------------------------------------------------------------
// Integer range class.
//------------------------------------------------------------------------------

struct intrange {
	int min;
	int max;

	intrange();
	~intrange();
};

//------------------------------------------------------------------------------
// Delay range class.
//------------------------------------------------------------------------------

struct delayrange {
	int min_delay_ms;
	int delay_range_ms;

	delayrange();
	~delayrange();
};

//------------------------------------------------------------------------------
// Block reference.
//------------------------------------------------------------------------------

struct blockref {
	bool is_symbol;			// TRUE if symbol valid, FALSE if location valid.
	word symbol;			// Block symbol.
	relcoords location;		// Block location.
};

//------------------------------------------------------------------------------
// Scale entry class - each value is a multiplier
//------------------------------------------------------------------------------

struct float_triplet {
	float x, y, z;
};

//------------------------------------------------------------------------------
// Vertex entry class.
//------------------------------------------------------------------------------

struct vertex_entry {
	float x, y, z;
	vertex_entry *next_vertex_entry_ptr;
};

//------------------------------------------------------------------------------
// Vertex class.
//------------------------------------------------------------------------------

struct vector;						// Forward declarations.
struct block;
struct polygon;

struct vertex {
	float x, y, z;

	vertex();
	vertex(float set_x, float set_y, float set_z);
	~vertex();
	void set(float x, float y, float z);
	void scale(void);
	void set_map_position(int column, int row, int level);
	void set_map_translation(int column, int row, int level);
	void get_map_position(int *column_ptr, int *row_ptr, int *level_ptr);
	int get_map_level(void);
	vertex operator +(vertex A);
	vector operator -(vertex A);
	vertex operator +(vector B);
	vertex operator -(vector B);
	vertex operator +(float A);
	vertex operator -(float A);
	vertex operator *(float A);
	vertex operator /(float A);
	vertex& operator +=(vertex &A);
	vertex& operator -=(vertex &A);
	vertex& operator +=(vector &A);
	vertex& operator -=(vector &A);
	vertex operator -();
	bool operator ==(vertex A);
	bool operator !=(vertex A);
	void rotate_x(float angle);
	void rotate_y(float angle);
	void rotate_z(float angle);
};

//------------------------------------------------------------------------------
// Vector class.
//------------------------------------------------------------------------------

struct vector {
	float dx, dy, dz;

	vector();
	vector(float set_dx, float set_dy, float set_dz);
	~vector();
	void set(float set_dx, float set_dy, float set_dz);

	vector operator *(float A);			// Scaling operator.
	vector operator *(vector A);		// Cross product.
	float operator &(vector A);			// Dot product.
	vector operator +(vector A);
	vector operator -(vector B);
	vector& operator +=(vector &A);
	vector& operator -=(vector &A);
	vector operator -();
	bool operator !();
	bool operator ==(vector A);
	float length(void);					// Return the length of the vector.
	float normalise(void);				// Normalise vector, returning its original length.
	void truncate(float max_length);	// Truncate the vector to a maximum length.
	void rotate_x(float look_angle);	// Rotate around X axis.
	void rotate_y(float turn_angle);	// Rotate around Y axis.
};	

//------------------------------------------------------------------------------
// Viewpoint class.
//------------------------------------------------------------------------------

struct viewpoint {
	vertex position;
	vertex last_position;
	float turn_angle, look_angle;
	float turn_angle_radians, look_angle_radians;
	float inv_turn_angle_radians, inv_look_angle_radians;
};

//------------------------------------------------------------------------------
// Rectangle class.
//------------------------------------------------------------------------------

struct rect {
	int x1, y1, x2, y2;
};

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Video rectangle class; each coordinate is expressed in pixel units or as a
// ratio.
//------------------------------------------------------------------------------

struct video_rect {
	float x1, y1, x2, y2;
	bool x1_is_ratio, y1_is_ratio, x2_is_ratio, y2_is_ratio;
};

#endif

//------------------------------------------------------------------------------
// Circle class.
//------------------------------------------------------------------------------

struct circle {
	int x, y, r;
};

//------------------------------------------------------------------------------
// Metadata class.
//------------------------------------------------------------------------------

struct metadata {
	string name;
	string content;
	metadata *next_metadata_ptr;
};

//------------------------------------------------------------------------------
// Load tag class.
//------------------------------------------------------------------------------

struct load_tag {
	bool is_texture_href;
	string href;
	load_tag *next_load_tag_ptr;
};

//------------------------------------------------------------------------------
// Bitmap class.
//------------------------------------------------------------------------------

// Bitmap class definition.

struct bitmap {
	void *handle;						// Bitmap handle.
	byte *pixels;						// Pointer to bitmap pixel data.
	int width, height;					// Size of bitmap.
	int bytes_per_row;					// Number of bytes per row.
	int colours;						// Number of colours in palette.
	RGBcolour *RGB_palette;				// Palette in RGB format.
	pixel *palette;						// Palette in pixel format.
	int transparent_index;				// Transparent pixel index.

	bitmap();
	~bitmap();
};

//==============================================================================
// XML classes.
//==============================================================================

//------------------------------------------------------------------------------
// Attribute structure.
//------------------------------------------------------------------------------

struct attr {
	string name;
	string value;
	attr *next_attr_ptr;
};

//------------------------------------------------------------------------------
// Entity structure.
//------------------------------------------------------------------------------

struct entity {
	int line_no;
	int type;
	string text;
	attr *attr_list;
	entity *nested_entity_list;
	entity *next_entity_ptr;
};

//==============================================================================
// Polygon rasterisation classes.
//==============================================================================

struct pixmap;						// Forward declarations.

//------------------------------------------------------------------------------
// Transformed vertex class.
//------------------------------------------------------------------------------

struct tvertex {
	float x, y, z;
	float u, v;
	RGBcolour colour;
	tvertex *next_tvertex_ptr;
};

//------------------------------------------------------------------------------
// Transformed polygon class.
//------------------------------------------------------------------------------

struct tpolygon {
	pixmap *pixmap_ptr;				// Pointer to pixmap.
	int tvertices;					// Number of transformed vertices.
	tvertex *tvertex_list;			// List of transformed vertices.
	float alpha;					// Translucency factor.
	tpolygon *next_tpolygon_ptr;	// Next transformed polygon in list.
};

//------------------------------------------------------------------------------
// Screen point class.
//------------------------------------------------------------------------------

struct spoint {
	float sx, sy;					// Projected screen point.
	float one_on_tz;				// Interpolated depth.
	float u_on_tz, v_on_tz;			// Interpolated texture coordinates.
	RGBcolour colour;				// Lit and normalised colour.
};

//------------------------------------------------------------------------------
// Screen polygon class.
//------------------------------------------------------------------------------

struct spolygon {
	pixmap *pixmap_ptr;				// Pointer to pixmap.
	int spoints;					// Number of screen points used.
	int max_spoints;				// Maximum number of screen points.
	spoint *spoint_list;			// List of screen points.
	float alpha;					// Translucency factor.
	spolygon *next_spolygon_ptr;	// Next screen polygon in list.
	spolygon *next_spolygon_ptr2;	// Next screen polygon in secondary list.

	spolygon();
	~spolygon();
	bool create_spoint_list(int set_spoints);
};

//------------------------------------------------------------------------------
// Polygon edge element.
//------------------------------------------------------------------------------

struct edge {
	float sx;						// Screen x coordinate (fractional).
	float one_on_tz;				// 1/tz.
	float u_on_tz;					// u/tz.
	float v_on_tz;					// v/tz.
};

//------------------------------------------------------------------------------
// Span data element.
//------------------------------------------------------------------------------

struct span_data {
	float one_on_tz;				// 1/tz.
	float u_on_tz;					// u/tz.
	float v_on_tz;					// v/tz.
};

//------------------------------------------------------------------------------
// Span buffer element.
//------------------------------------------------------------------------------

struct pixmap;							// Forward declaration.

struct span {
	int sy;								// Screen y coordinate for span.
	int start_sx;						// Start screen x coordinate for span.
	int end_sx;							// End screen x coordinate + 1 for span.
	span_data start_span;				// Start values for span.
	span_data delta_span;				// Delta values for span.
	bool is_popup;						// TRUE if this span belongs to a popup.
	pixmap *pixmap_ptr;					// Pixmap to render on span (or NULL).
	pixel colour_pixel;					// Colour to render on span.
	int brightness_index;				// Brightness index for span.
	span *next_span_ptr;				// Pointer to next span in list.

	span();
	~span();
	void adjust_start(int new_start_sx);
	bool span_in_front(span *span_ptr);
};

//------------------------------------------------------------------------------
// Span buffer row element.
//------------------------------------------------------------------------------

struct span_row {
	span *opaque_span_list;			// Opaque spans (sorted left to right).
	span *transparent_span_list;	// Transparent spans (sorted back to front).

	span_row();
	~span_row();
};

//------------------------------------------------------------------------------
// Span buffer class.
//------------------------------------------------------------------------------

struct span_buffer {
	int rows;					// Number of span buffer rows.
	span_row *buffer_ptr;		// Pointer to span buffer rows.

	span_buffer();
	~span_buffer();
	bool create_buffer(int set_rows);
	void clear_buffer();
	span_row *operator[](int row);
};

//==============================================================================
// Texture cache classes.
//==============================================================================

//------------------------------------------------------------------------------
// Texture cache entry.
//------------------------------------------------------------------------------

struct pixmap;							// Forward declaration.

struct cache_entry {
	pixmap *pixmap_ptr;					// Pointer to pixmap using this entry.
	int brightness_index;				// Brightness index used to light image.
	int frame_no;						// Frame number of last reference.
	cachebyte *lit_image_ptr;			// Pointer to lit image in cache.
	int lit_image_size;					// Size of lit image, in bytes.
	int lit_image_mask;					// u/v coordinate mask.
	int lit_image_shift;				// v coordinate shift.
	void *hardware_texture_ptr;			// Pointer to hardware texture.
	cache_entry *next_cache_entry_ptr;	// Pointer to next cache entry in list.

	cache_entry();
	~cache_entry();
	bool create_image_buffer(int image_size_index);
};

//------------------------------------------------------------------------------
// Texture cache.
//------------------------------------------------------------------------------

struct cache {
	int image_size_index;			// Size of images stored in this cache.
	cache_entry *cache_entry_list;	// Linked list of cache entries.

	cache();
	cache(int size_index);
	~cache();
	cache_entry *add_cache_entry(void);
};

//==============================================================================
// Texture map classes.
//==============================================================================

//------------------------------------------------------------------------------
// Pixmap class.
//------------------------------------------------------------------------------

struct pixmap {
	int width, height;				// Image dimensions.
	int size_index;					// Size index.
	int bytes_per_pixel;			// Number of bytes per pixel (1, 2 or 4).
	imagebyte *image_ptr;			// Pointer to 8-bit or 16-bit image data.
	int image_size;					// Size of image in bytes.
	int colours;					// Number of colours in palette.
	pixel *display_palette_list;	// Palette list using display pixel format.
	pixel *texture_palette_list;	// Palette list using texture pixel format.
	byte *palette_index_table;		// Palette index table for remapping texels.
	int transparent_index;			// Index of transparent colour.
	int delay_ms;					// Animation delay time in milliseconds.
	cache_entry *cache_entry_list[BRIGHTNESS_LEVELS];	// Cache entries.
	span *span_lists[BRIGHTNESS_LEVELS];				// Span lists.
	tpolygon *tpolygon_list;		// Transformed polygon list.

#ifdef STREAMING_MEDIA
	bool image_updated[BRIGHTNESS_LEVELS];				// Image updated list.
#endif

	pixmap();
	~pixmap();
};

//------------------------------------------------------------------------------
// Texture class.
//------------------------------------------------------------------------------

struct blockset;					// Forward declaration.

struct texture {
	blockset *blockset_ptr;			// Blockset of texture (or NULL if custom).
	string URL;						// URL of texture.
	int load_index;					// Load index for this texture (if custom).
	bool transparent;				// TRUE if at least 1 pixmap is transparent.
	bool loops;						// TRUE if animation loops.
	int bytes_per_pixel;			// Number of bytes per pixel (1, 2 or 4).
	int width, height;				// Dimensions of largest image.
	int pixmaps;					// Number of pixmaps.
	pixmap *pixmap_list;			// Array of pixmaps.
	int total_time_ms;				// Total time required for animation.
	int colours;					// Number of colours in palette.
	int brightness_levels;			// Number of brightness levels used.
	RGBcolour *RGB_palette;			// Original RGB palette.
	pixel *display_palette_list;	// Palette list using display pixel format.
	pixel *texture_palette_list;	// Palette list using texture pixel format.
	byte *palette_index_table;		// Palette index table for remapping texels.
	texture *next_texture_ptr;		// Pointer to next texture.

	texture();
	~texture();
	bool create_RGB_palette(int set_colours, int set_brightness_levels,
		RGBcolour *set_RGB_palette);
	bool create_display_palette_list(void);
	bool create_texture_palette_list(void);
	bool create_palette_index_table(void);
	pixmap *get_curr_pixmap_ptr(int elapsed_time_ms);
};

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Video texture class.
//------------------------------------------------------------------------------

struct video_texture {
	texture *texture_ptr;					// Pointer to texture.
	video_rect source_rect;					// Source rectangle in video frame.
	float delta_u, delta_v;					// Delta (u,v) for scaling.
	video_texture *next_video_texture_ptr;	// Pointer to next video texture.

	video_texture();
	~video_texture();
};

#endif

//==============================================================================
// Special object classes.
//==============================================================================

//------------------------------------------------------------------------------
// Light class.
//------------------------------------------------------------------------------

struct square;						// Forward declaration.

struct light {
	string name;					// Name.
	int style;						// Style.
	mapcoords map_coords;			// Map coordinates of light.
	vertex position;				// Map coordinates plus offset.
	dirrange dir_range;				// Direction range.
	direction delta_dir;			// Change in direction per second.
	direction curr_dir;				// Current direction.
	vector dir;						// Current direction as a vector.
	RGBcolour colour;				// Colour.
	pcrange intensity_range;		// Intensity range.
	float speed;					// Speed for intensity or direction.
	float delta_intensity;			// Change in intensity per second.
	float intensity;				// Intensity.
	RGBcolour lit_colour;			// Colour at intensity.
	float radius;					// Radius of light.
	float one_on_radius;			// 1 / radius.
	float cone_angle;				// Cone angle in degrees.
	float cos_cone_angle;			// Cosine of cone angle.
	float cone_angle_M;				// 1 / (1 - cosine of cone angle).
	bool flood;						// TRUE if flood light (no light dropoff).
	light *next_light_ptr;			// Pointer to next light in list.

	light();
	~light();
	void set_direction(direction light_direction);
	void set_dir_range(dirrange light_dir_range);
	void set_dir_speed(float light_speed);
	void set_intensity(float light_intensity);
	void set_intensity_range(pcrange light_intensity_range);
	void set_intensity_speed(float light_speed);
	void set_radius(float light_radius);
	void set_cone_angle(float light_cone_angle);
};

//------------------------------------------------------------------------------
// Light reference class.
//------------------------------------------------------------------------------

struct light_ref {
	light *light_ptr;
	vertex light_pos;
};

//------------------------------------------------------------------------------
// Wave class.
//------------------------------------------------------------------------------

struct wave {
	blockset *blockset_ptr;			// Blockset of wave (or NULL if custom).
	string URL;						// URL of wave.
	int load_index;					// Load index for this wave (if custom).
	void *format_ptr;				// Pointer to wave format.
	char *data_ptr;					// Pointer to wave data.
	int data_size;					// Size of wave data.
	wave *next_wave_ptr;			// Next wave in list.

	wave();
	~wave();
};

//------------------------------------------------------------------------------
// Sound class.
//------------------------------------------------------------------------------

struct sound {
	string name;					// Name.
	bool ambient;					// TRUE if an ambient sound.
	string URL;						// URL to sound file.
	wave *wave_ptr;					// Pointer to wave.
	void *sound_buffer_ptr;			// Pointer to sound buffer.
	mapcoords map_coords;			// Map coordinates of sound.
	vertex position;				// Position of sound.
	float volume;					// Volume of sound.
	float radius;					// Radius of sound field.
	int playback_mode;				// Playback mode.
	delayrange delay_range;			// Range of playback delays allowed.
	int start_time_ms;				// Start time for delay.
	int delay_ms;					// Current delay in milliseconds.
	bool flood;						// TRUE if flood mode (no sound rolloff).
	float rolloff;					// Sound rolloff factor.
	bool reflections;				// TRUE if reflections currently enabled.
	bool in_range;					// TRUE if sound is in range.
	bool played_once;				// TRUE if sound has been played once.
	sound *next_sound_ptr;			// Next sound in list.

	sound();
	~sound();
};

//------------------------------------------------------------------------------
// Entrance class.
//------------------------------------------------------------------------------

struct entrance {
	string name;						// Entrance name.
	square *square_ptr;					// Square entrance belong to (if any).
	block *block_ptr;					// Block entrance belongs to (if any).
	direction initial_direction;		// Initial direction of player.
	entrance *next_entrance_ptr;		// Pointer to next entrance in list.
	entrance *next_chosen_entrance_ptr;	// Pointer to next chosen entrance.

	entrance();
	~entrance();
};

//------------------------------------------------------------------------------
// Action class.
//------------------------------------------------------------------------------

struct trigger;  // Forward Declaration

struct action {
	int type;					// Action type.
	trigger *trigger_ptr;		// the parent trigger
	blockref source;			// Replace source symbol/location.
	relcoords target;			// Replace target location.
	bool target_is_trigger;		// TRUE if replace target is trigger.
	string exit_URL;			// Exit URL.
	bool is_spot_URL;			// TRUE if URL is for a spot.
	string exit_target;			// Exit target.
	float_triplet spin_angles;	// Value in the spin action 
	int style;					// Used by ripple for what style of ripple
	int vertices;				// Number of vertices that this action is acting upon
	int temp;					// Used by RIPPLE to delay each wave
	int speed;
	relinteger rel_number;		// Relative frame number used by setframe and setloop
	float force;				// Used by ripple for how high to make the drops
	float droprate;				// used in ripple for how often to make a raindrop
	float damp;					// used in ripple for dampening effect on forces
	float *prev_step_ptr;		// An array pointing to the last matrix of forces for ripple
	float *curr_step_ptr;		// An array pointing to the current matrix of forces for ripple
	float totalx, totaly, totalz;
	float speedx, speedy, speedz;
	char* charindex;
	action *next_action_ptr;	// Next action in list.
};

//------------------------------------------------------------------------------
// Script definition class.
//------------------------------------------------------------------------------

struct script_def {
	unsigned int ID;
	string script;
	void *script_simkin_object_ptr;
	script_def *next_script_def_ptr;
};

//------------------------------------------------------------------------------
// Trigger class.
//------------------------------------------------------------------------------

struct square;					// Forward declaration.
struct part;					// Forward declaration.

struct trigger {
	int trigger_flag;			// Trigger flag.
	square *square_ptr;			// Square that trigger belongs to (if any).
	block *block_ptr;			// Block that the trigger belongs to (if any).
	vertex position;			// Position of trigger (for "step in/out").
	float radius;				// Radius of trigger.
	float radius_squared;		// Radius squared of trigger (for "step in/out").
	bool previously_inside;		// TRUE if player was previously inside radius.
	delayrange delay_range;		// Range of timer delays allowed (for "timer").
	int start_time_ms;			// Start time of delay (for "timer") or time
								// trigger was put on active script queue.
	int delay_ms;				// Delay time (for "timer").
	mapcoords target;			// Target location (for "location").
	byte key_code;				// Key code (for "key up/down/hold").
	action *action_list;		// List of actions.
	script_def *script_def_ptr;	// Script to execute (if action list is NULL).
	string label;				// Text for label (only used for "click on").
	string part_name;			// Part name. 
	int part_index;             // Part index.
	trigger *next_trigger_ptr;	// Next trigger in list.

	trigger();
	~trigger();
};

//------------------------------------------------------------------------------
// Hyperlink class.
//------------------------------------------------------------------------------

struct hyperlink {
	int trigger_flags;				// Trigger flags.
	string label;					// Text for label.
	string URL;						// URL.
	bool is_spot_URL;				// TRUE if URL is for a spot.
	string target;					// Window target.
	hyperlink *next_hyperlink_ptr;	// Next hyperlink in list.

	hyperlink();
	~hyperlink();
};

//------------------------------------------------------------------------------
// Area class.
//------------------------------------------------------------------------------

struct area {
	int shape_type;					// Shape type.
	union {
		rect rect_shape;			// Rectangle coordinates.
		circle circle_shape;		// Circle coordinates and radius.
	};
	hyperlink *exit_ptr;			// Exit attached to this area.
	int trigger_flags;				// Trigger flags.
	trigger *trigger_list;			// List of triggers attached to this area.
	area *next_area_ptr;			// Next area in list.

	area();
	~area();
};

//------------------------------------------------------------------------------
// Imagemap class.
//------------------------------------------------------------------------------

struct imagemap {
	string name;					// Imagemap name.
	area *area_list;				// List of areas in this imagemap.
	imagemap *next_imagemap_ptr;	// Next imagemap in list.

	imagemap();
	~imagemap();
	area *find_area(rect *rect_ptr);
	area *find_area(circle *circle_ptr);
	bool add_area(rect *rect_ptr, hyperlink *exit_ptr);
	bool add_area(rect *rect_ptr, trigger *trigger_ptr);
	bool add_area(circle *circle_ptr, hyperlink *exit_ptr);
	bool add_area(circle *circle_ptr, trigger *trigger_ptr);
};

//------------------------------------------------------------------------------
// Popup class.
//------------------------------------------------------------------------------

struct popup {
	string name;					// Name of popup.
	square *square_ptr;				// Square that popup belongs to (if any).
	block *block_ptr;				// Block that popup belongs to (if any).
	int trigger_flags;				// Trigger flags.
	int visible_flags;				// Visible flags.
	bool always_visible;			// TRUE if popup is always visible.
	vertex position;				// Position of popup.
	int window_alignment;			// Placement of popup on window.
	float radius;					// Radius of popup.
	float radius_squared;			// Radius of popup squared.
	float brightness;				// Popup brightness.
	int brightness_index;			// Popup brightness index.
	RGBcolour colour;				// Background colour.
	string bg_texture_URL;			// URL of background texture.
	texture *bg_texture_ptr;		// Pointer to background texture (or NULL).
	texture *fg_texture_ptr;		// Pointer to foreground texture (or NULL).
	bool create_foreground;			// TRUE if foreground texture is required.
	bool transparent_background;	// TRUE for transparent colour background.
	int width, height;				// Size of popup in pixels.
	string text;					// Text to be drawn on popup.
	RGBcolour text_colour;			// Text colour.
	int text_alignment;				// Text alignment mode.
	string imagemap_name;			// Name of imagemap.
	imagemap *imagemap_ptr;			// Pointer to imagemap (or NULL).
	pixmap *bg_pixmap_ptr;			// Pointer to current background pixmap.
	int sx, sy;						// Current screen position.
	int start_time_ms;				// Time popup was made visible.
	popup *next_popup_ptr;			// Next popup in list.
	popup *next_square_popup_ptr;	// Next popup in square list.
	popup *next_visible_popup_ptr;	// Next popup in visible list.

	popup();
	~popup();
};

//==============================================================================
// Block definition and style classes.
//==============================================================================

//------------------------------------------------------------------------------
// Part class.
//------------------------------------------------------------------------------

struct part {
	string name;					// Name of part.
	texture *texture_ptr;			// Pointer to current texture (or NULL).
	texture *custom_texture_ptr;	// Pointer to custom texture (or NULL).
	bool colour_set;				// TRUE if colour is set.
	RGBcolour colour;				// RGB colour (used if no texture).
	RGBcolour normalised_colour;	// Normalised part colour.
	float alpha;					// Alpha value (0.0f = transparent).
	int texture_style;				// Texture style.
	float texture_angle;			// Texture angle.
	int faces;						// Number of faces per polygon (0, 1 or 2).
	int projection;					// Texture projection.
	bool solid;						// TRUE if part is solid.
	int trigger_flags;				// Trigger flags.
	int number;
	part *next_part_ptr;			// Next part in list.

	part();
	~part();
};

//------------------------------------------------------------------------------
// Vertex definition entry class.
//------------------------------------------------------------------------------

struct vertex_def_entry {
	int vertex_no;
	float u, v;
	vertex_def_entry *next_vertex_def_entry_ptr;
};

//------------------------------------------------------------------------------
// Vertex definition class.
//------------------------------------------------------------------------------

struct vertex_def {
	int vertex_no;					// Index of vertex in block's vertex list.
	float u, v;						// 2D texture coordinate at vertex.
	float orig_u, orig_v;			// Original texture coordinates.

	vertex_def();
	~vertex_def();
};

//------------------------------------------------------------------------------
// Polygon definition class.
//------------------------------------------------------------------------------

struct polygon_def {
	int part_no;						// Part number.
	part *part_ptr;						// Pointer to part.
	int vertices;						// Number of vertex definitions.
	vertex_def *vertex_def_list;		// Clockwise list of vertex definitions.
	int front_polygon_ref;				// Front polygon reference in BSP tree.
	int rear_polygon_ref;				// Rear polygon reference in BSP tree.
	polygon_def *next_polygon_def_ptr;	// Pointer to next polygon in list.

	polygon_def();
	~polygon_def();
	bool create_vertex_def_list(int set_vertices);
	void project_texture(vertex *vertex_list, int projection);
	polygon_def& operator=(const polygon_def &old_polygon_def);
};

//------------------------------------------------------------------------------
// Polygon class.
//------------------------------------------------------------------------------

struct polygon {
	polygon_def *polygon_def_ptr;		// Pointer to polygon definition.
	bool active;						// TRUE if polygon is active.
	vertex centroid;					// Centroid of polygon.
	vector normal_vector;				// Surface normal vector.
	float plane_offset;					// Offset in plane equation.
	int direction;						// Cardinal direction polygon is facing.
	bool side;							// TRUE if this is a side polygon.

	polygon();
	~polygon();
	void compute_centroid(vertex *vertex_list);
	void compute_normal_vector(vertex *vertex_list);
	void compute_plane_offset(vertex *vertex_list);
};

//------------------------------------------------------------------------------
// BSP node class.
//------------------------------------------------------------------------------

struct BSP_node {
	int polygon_no;
	BSP_node *front_node_ptr;
	BSP_node *rear_node_ptr;

	BSP_node();
	~BSP_node();
};

//------------------------------------------------------------------------------
// Frame definition class.
//------------------------------------------------------------------------------

struct frame_def {
	vertex *vertex_list;
};

//------------------------------------------------------------------------------
// Animation definition class.
//------------------------------------------------------------------------------

struct animation_def {
	int frames;						// number of frames in the animation
	bool original_frames;			// Whether this is the original block that contains the animation frames
	int *vertices;
	frame_def *frame_list;
	float *angles;
	int loops;						// Number of loop animations in this block
	intrange *loops_list;			// List of all loops in block
};

//------------------------------------------------------------------------------
// Block definition class.
//------------------------------------------------------------------------------

struct block;								// Forward declaration.

struct block_def {
	bool custom;							// TRUE if custom block definition.
	block_def *custom_dup_block_def_ptr;	// Pointer to an exact custom duplicate of this block definition (if any).
	string source_block;					// The name or symbol of the source block.
	char single_symbol;						// Single-character symbol.
	word double_symbol;						// Double-character symbol.
	string name;							// Block name.
	int type;								// Block type.
	bool allow_entrance;					// TRUE if block permits an entrance.
	bitmap *icon_bitmap_ptr;				// The icon bitmap (if any).
	relinteger_triplet position;			// The relative position set by the POSITION param.
	bool animated;							// Set if this block has frames.
	animation_def *animation;				// The structure holding all the animation frames.
	int parts;								// Size of part list.
	part *part_list;						// List of all parts in block.
	int vertices;							// Size of vertex list.
	vertex *vertex_list;					// List of all vertices in block.
	int polygons;							// Size of polygon definition list. 
	polygon_def *polygon_def_list;			// List of all polygon definitions in block.
	light *light_list;						// List of lights (if any).
	light *last_light_ptr;					// Pointer to last light in list (if any).
	sound *sound_list;						// List of sounds (if any).
	sound *last_sound_ptr;					// Pointer to last sound in list (if any).
	int popup_trigger_flags;				// Popup trigger flags.
	popup *popup_list;						// List of popups (if any).
	popup *last_popup_ptr;					// Pointer to last popup in list (if any).
	entrance *entrance_ptr;					// Pointer to entrance (if any).
	bool custom_exit;						// TRUE if custom block definition has an exit.
	hyperlink *exit_ptr;					// Pointer to exit (if any).
	int trigger_flags;						// Trigger flags.
	trigger *trigger_list;					// List of triggers (if any).
	trigger *last_trigger_ptr;				// Pointer to last trigger in list (if any).
	block *free_block_list;					// List of free blocks (if any).
	block *used_block_list;					// List of used blocks (if any).
	int root_polygon_ref;					// Root polygon reference in BSP tree.
	BSP_node *BSP_tree;						// Pointer to BSP tree (if any).
	orientation block_orientation;			// Orientation of structural block.
	vertex block_origin;					// Origin of structural block.
	float sprite_angle;						// Angle of sprite (if applicable).
	float degrees_per_ms;					// Rotational speed (if applicable).
	int sprite_alignment;					// Sprite alignment (if applicable).
	size sprite_size;						// Sprite size (if applicable).
	bool solid;								// TRUE if block is solid.
	bool movable;							// TRUE is block is movable.
	string script;							// A script providing SimKin definitions.
	block_def *next_block_def_ptr;			// Pointer to next block definition in list.

	block_def();
	~block_def();
	void create_part_list(int set_parts);
	void create_frames_list(int set_frames);
	void create_loops_list(int set_loops);
	void create_vertex_list(int set_vertices);
	void create_polygon_def_list(int set_polygons);
	void dup_block_def(block_def *block_def_ptr);
	block *create_simplified_block();
	block *new_block(square *square_ptr);
	block *del_block(block *block_ptr);
	string get_symbol(void);
    void rotate_x(float angle);
	void rotate_y(float angle);
	void rotate_z(float angle);
};

//------------------------------------------------------------------------------
// Blockset class.
//------------------------------------------------------------------------------

struct blockset {
	string URL;									// URL of blockset.
	string name;								// Name of blockset.
	block_def *block_def_list;					// List of block definitions.
	block_def *last_block_def_ptr;				// Pointer to last block definition.
	string placeholder_texture_URL;				// URL of placeholder texture.
	texture *placeholder_texture_ptr;			// Placeholder texture.
	bool sky_defined;							// TRUE if sky defined.
	string sky_texture_URL;						// URL of sky texture.
	texture *sky_texture_ptr;					// Pointer to sky texture.
	bool sky_colour_set;						// TRUE is sky colour set.
	RGBcolour sky_colour;						// Sky colour.
	bool sky_brightness_set;					// TRUE if sky brightness set.
	float sky_brightness;						// Sky brightness.
	bool ground_defined;						// TRUE if ground defined.
	string ground_texture_URL;					// URL of ground texture.
	texture *ground_texture_ptr;				// Pointer to ground texture.
	bool ground_colour_set;						// TRUE if ground colour set.
	RGBcolour ground_colour;					// Ground colour.
	bool orb_defined;							// TRUE if orb defined.
	string orb_texture_URL;						// URL of orb texture.
	texture *orb_texture_ptr;					// Pointer to orb texture.
	bool orb_direction_set;						// TRUE if orb direction set.
	direction orb_direction;					// Orb direction.
	bool orb_brightness_set;					// TRUE if orb brightness set.
	float orb_brightness;						// Orb brightness.
	bool orb_colour_set;						// TRUE if orb colour set.
	RGBcolour orb_colour;						// Orb colour.
	hyperlink *orb_exit_ptr;					// Pointer to orb exit.
	texture *first_texture_ptr;					// First texture in list.
	texture *last_texture_ptr;					// Last texture in list.
	wave *first_wave_ptr;						// First wave in list.
	wave *last_wave_ptr;						// Last wave in list.
	blockset *next_blockset_ptr;				// Next blockset in list.

	blockset();
	~blockset();
	void add_block_def(block_def *block_def_ptr);
	bool delete_block_def(word symbol);
	block_def *get_block_def(char single_symbol);
	block_def *get_block_def(word double_symbol);
	block_def *get_block_def(const char *name);
	block_def *get_block_def(char single_symbol, const char *name);
	void add_wave(wave *wave_ptr);
	void add_texture(texture *texture_ptr);
};

//------------------------------------------------------------------------------
// Cached blockset class.
//------------------------------------------------------------------------------

struct cached_blockset {
	string href;
	int size;
	time_t updated;
	string name;
	string synopsis;
	unsigned int version;
	cached_blockset *next_cached_blockset_ptr;
};

//------------------------------------------------------------------------------
// Blockset list class.
//------------------------------------------------------------------------------

struct blockset_list {
	blockset *first_blockset_ptr;				// First blockset in list.
	blockset *last_blockset_ptr;				// Last blockset in list.

	blockset_list();
	~blockset_list();
	void add_blockset(blockset *blockset_ptr);
	bool find_blockset(char *URL);
	blockset *find_blockset_by_name(char *name);
	blockset *remove_blockset(char *URL);
};

//==============================================================================
// Block and map classes.
//==============================================================================

//------------------------------------------------------------------------------
// Block class.
//------------------------------------------------------------------------------

struct block {
	block_def *block_def_ptr;		// Pointer to block definition.
	square *square_ptr;				// Pointer to square (if not movable).
	vertex translation;				// Translation of block in world space.
	int current_frame;				// the current frame being displayed
	int current_loop;				// The current loop that is playing
	int next_loop;					// The next loop that will be played
	bool current_repeat;			// repeat current loop
	bool next_repeat;				// repeat next loop
	int vertices;					// Size of vertex list.
	vertex *vertex_list;			// List of all vertices.
	int polygons;					// Size of polygon list. 
	polygon *polygon_list;			// List of all polygons in block.
	orientation block_orientation;	// Orientation of structural block.
	vertex block_origin;			// Origin of structural block.
	float sprite_angle;				// Angle of sprite (if applicable).
	int start_time_ms;				// Time block was placed on map.
	int last_time_ms;				// Time of last sprite rotational change.
	int pixmap_index;				// Sprite pixmap index (if applicable).
	bool solid;						// TRUE if block is solid.
	bool requires_col_mesh;			// TRUE if block requires a collision mesh.
	COL_MESH *col_mesh_ptr;			// Pointer to the collision mesh.
	int col_mesh_size;				// Size of collision mesh in bytes.
	bool scripted;					// TRUE if this block has scripts.
	void *block_simkin_object_ptr;	// Pointer to the block SimKin object.
	void *vertex_simkin_object_ptr;	// Pointer to the vertex SimKin object.
	light *light_list;				// List of lights in block.
	light_ref *active_light_list;	// List of lights illuminating block.
	bool set_active_lights;			// TRUE if active lights need to be set.
	sound *sound_list;				// List of sounds in block.
	int popup_trigger_flags;		// Popup trigger flags.
	popup *popup_list;				// List of popups in block.
	int trigger_flags;				// Trigger flags.
	trigger *trigger_list;			// List of triggers in block.
	hyperlink *exit_ptr;			// Pointer to exit (if any).
	entrance *entrance_ptr;			// Pointer to entrance (if any).
	block *next_block_ptr;			// Pointer to next block in list.
	block *next_used_block_ptr;		// Pointer to next block in used list.
	block *prev_used_block_ptr;		// Pointer to previous block in used list.

	block();
	~block();
	bool create_vertex_list(int set_vertices);
	bool create_polygon_list(int set_polygons);
	bool create_active_light_list(void);
	void reset_vertices(void);
	void update(void);
	void orient(void);
	void rotate_x(float angle);
	void rotate_y(float angle);
	void rotate_z(float angle);
	void set_frame(int number);
	void set_nextloop(int number);
};
	
//------------------------------------------------------------------------------
// Square class.
//------------------------------------------------------------------------------

struct square {
	word orig_block_symbol;				// Symbol of original block on square.
	block *block_ptr;					// Pointer to block (if any).
	hyperlink *exit_ptr;				// Pointer to exit (if any).
	int popup_trigger_flags;			// Popup trigger flags.
	popup *popup_list;					// List of popups (if any).
	popup *last_popup_ptr;				// Last popup in the list.
	int trigger_flags;					// Square trigger flags.
	trigger *trigger_list;				// List of square triggers (if any).
	trigger *last_trigger_ptr;			// Last square trigger in list.

	square();
	~square();
};

//------------------------------------------------------------------------------
// World class.
//------------------------------------------------------------------------------

struct world {
	int map_style;					// Map style.
	bool ground_level_exists;		// TRUE if ground level exists.
	int columns;					// Number of columns in square map.
	int rows;						// Number of rows in square map.
	int levels;						// Number of levels in square map.
	square *square_map;				// Map of squares.
	entity **level_entity_list;		// Array of level entities.
	float audio_scale;				// Audio scale (in metres per unit).

	world();
	~world();
	bool create_square_map(void);
	square *get_square_ptr(int column, int row, int level);
	block *get_block_ptr(int column, int row, int level);
	entity *get_level_entity(int level);
	void set_level_entity(int level, entity *entity_ptr);
	void get_square_location(square *square_ptr, int *column_ptr, int *row_ptr, int *level_ptr);
};