//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

//------------------------------------------------------------------------------
// Parameter lists for tags, in alphabetical order.
//------------------------------------------------------------------------------

// ACTION/SCRIPT tag (spot file only).

#define ACTION_ATTRIBUTES	8
#define ACTION_TRIGGER		0
#define ACTION_TEXT			1
#define ACTION_RADIUS		2
#define ACTION_DELAY		3
#define ACTION_TARGET		4
#define ACTION_KEY			5
#define ACTION_PART_NAME	6
#define ACTION_LOCATION		7
static int action_trigger;
static float action_radius;
static delayrange action_delay;
static string action_text;
static mapcoords action_target;
static int action_key;
static mapcoords action_location;
static string action_part_name;
static attr_def action_attr_list[ACTION_ATTRIBUTES] = {
	{TOKEN_TRIGGER, VALUE_ACTION_TRIGGER, &action_trigger, false},
	{TOKEN_TEXT, VALUE_STRING, &action_text, false},
	{TOKEN_RADIUS, VALUE_RADIUS, &action_radius, false},
	{TOKEN_DELAY, VALUE_DELAY_RANGE, &action_delay, false},
	{TOKEN_TARGET, VALUE_MAP_COORDS, &action_target, false}, 
	{TOKEN_KEY, VALUE_KEY_CODE, &action_key, false},
	{TOKEN_PARTNAME, VALUE_NAME_LIST, &action_part_name, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &action_location, false}
};

// ACTION/SCRIPT tag inside IMAGEMAP tag (spot file only).

#define IMAGEMAP_ACTION_ATTRIBUTES	4
static int action_shape;
static string action_coords;
static attr_def imagemap_action_attr_list[IMAGEMAP_ACTION_ATTRIBUTES] = {
	{TOKEN_TRIGGER, VALUE_IMAGEMAP_TRIGGER, &action_trigger, false},
	{TOKEN_TEXT, VALUE_STRING, &action_text, false},
	{TOKEN_SHAPE, VALUE_SHAPE, &action_shape, true},
	{TOKEN_COORDS, VALUE_STRING, &action_coords, true},
};

// AMBIENT_LIGHT tag (spot file only).

#define AMBIENT_LIGHT_ATTRIBUTES	2
#define AMBIENT_LIGHT_COLOUR		1
static float ambient_light_brightness;
static RGBcolour ambient_light_colour;
static attr_def ambient_light_attr_list[AMBIENT_LIGHT_ATTRIBUTES] = {
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &ambient_light_brightness, true},
	{TOKEN_COLOUR, VALUE_RGB, &ambient_light_colour, false}
};

// AMBIENT_SOUND tag (spot file only).

#define AMBIENT_SOUND_ATTRIBUTES	4
#define AMBIENT_SOUND_VOLUME		1
#define AMBIENT_SOUND_PLAYBACK		2
#define AMBIENT_SOUND_DELAY			3
static string ambient_sound_file;
static float ambient_sound_volume;
static int ambient_sound_playback;
static delayrange ambient_sound_delay;
static attr_def ambient_sound_attr_list[] = {
	{TOKEN_FILE, VALUE_STRING, &ambient_sound_file, true},
	{TOKEN_VOLUME, VALUE_PERCENTAGE, &ambient_sound_volume, false},
	{TOKEN_PLAYBACK, VALUE_PLAYBACK_MODE, &ambient_sound_playback, false},
	{TOKEN_DELAY, VALUE_DELAY_RANGE, &ambient_sound_delay, false}
};

// AREA tag (spot file only).

#define AREA_ATTRIBUTES	6
#define AREA_IS_SPOT	3
#define AREA_TARGET		4
#define AREA_TEXT		5
static int area_shape;
static string area_coords;
static string area_href;
static bool area_is_spot;
static string area_target;
static string area_text;
static attr_def area_attr_list[AREA_ATTRIBUTES] = {
	{TOKEN_SHAPE, VALUE_SHAPE, &area_shape, true},
	{TOKEN_COORDS, VALUE_STRING, &area_coords, true},
	{TOKEN_HREF, VALUE_STRING, &area_href, true},
	{TOKEN_IS_SPOT, VALUE_BOOLEAN, &area_is_spot, false},
	{TOKEN_TARGET, VALUE_STRING, &area_target, false},
	{TOKEN_TEXT, VALUE_STRING, &area_text, false}
};

// BASE tag (spot file only).

#define BASE_ATTRIBUTES	1
static string base_href;
static attr_def base_attr_list[BASE_ATTRIBUTES] = {
	{TOKEN_HREF, VALUE_STRING, &base_href, true}
};

// BLOCK tag (block file only).

#define BLOCK_ATTRIBUTES	3
#define BLOCK_TYPE			1
#define BLOCK_ENTRANCE		2
static string block_name;
static int block_type;
static bool block_entrance;
static attr_def block_attr_list[BLOCK_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &block_name, true},
	{TOKEN__TYPE, VALUE_BLOCK_TYPE, &block_type, false},
	{TOKEN_ENTRANCE, VALUE_BOOLEAN, &block_entrance, false}
};

// BLOCK tag (style file only).

#define STYLE_BLOCK_ATTRIBUTES	4
#define STYLE_BLOCK_DOUBLE		1
#define STYLE_BLOCK_NAME		2
static char style_block_symbol;
static word style_block_double;
static string style_block_name;
static string style_block_file;
static attr_def style_block_attr_list[STYLE_BLOCK_ATTRIBUTES] = {
	{TOKEN_SYMBOL, VALUE_SINGLE_SYMBOL, &style_block_symbol, true},
	{TOKEN_DOUBLE, VALUE_DOUBLE_SYMBOL, &style_block_double, false},
	{TOKEN_NAME, VALUE_NAME, &style_block_name, false},
	{TOKEN_FILE, VALUE_STRING, &style_block_file, true}
};

// BLOCKSET tag (spot file only).

#define BLOCKSET_ATTRIBUTES 1
static string blockset_href;
static attr_def blockset_attr_list[BLOCKSET_ATTRIBUTES] = {
	{TOKEN_HREF, VALUE_STRING, &blockset_href, true}
};

// BLOCKSET tag (cache file only).

#define CACHED_BLOCKSET_ATTRIBUTES	6
#define CACHED_BLOCKSET_NAME		3
#define CACHED_BLOCKSET_SYNOPSIS	4
#define CACHED_BLOCKSET_VERSION		5
static string cached_blockset_href;
static int cached_blockset_size;
static int cached_blockset_updated;
static string cached_blockset_name;
static string cached_blockset_synopsis;
static unsigned int cached_blockset_version;
static attr_def cached_blockset_attr_list[CACHED_BLOCKSET_ATTRIBUTES] = {
	{TOKEN_HREF, VALUE_STRING, &cached_blockset_href, true},
	{TOKEN_SIZE, VALUE_INTEGER, &cached_blockset_size, true},
	{TOKEN_UPDATED, VALUE_INTEGER, &cached_blockset_updated, true},
	{TOKEN_NAME, VALUE_STRING, &cached_blockset_name, false},
	{TOKEN_SYNOPSIS, VALUE_STRING, &cached_blockset_synopsis, false},
	{TOKEN_VERSION, VALUE_VERSION, &cached_blockset_version, false}
};

// BSP_TREE tag (block file only).

#define BSP_TREE_ATTRIBUTES 1
static int BSP_tree_root;
static attr_def BSP_tree_attr_list[BSP_TREE_ATTRIBUTES] = {
	{TOKEN_ROOT, VALUE_INTEGER, &BSP_tree_root, true}
};

// CREATE tag (spot file only).

#define CREATE_ATTRIBUTES 3
static word create_symbol;
static string create_block;
static string create_spdesc;
static attr_def create_attr_list[CREATE_ATTRIBUTES] = {
	{TOKEN_SYMBOL, VALUE_SYMBOL, &create_symbol, true},
	{TOKEN_BLOCK, VALUE_STRING, &create_block, true},
	{TOKEN_SPDESC, VALUE_STRING, &create_spdesc, false}		// Used by Spoknik.
};

#define CATEGORY_ATTRIBUTES 1
static string category_name;
static attr_def category_attr_list[CATEGORY_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_STRING, &category_name, true}
};

// DEBUG tag (spot file only).

#define DEBUG_ATTRIBUTES	1
#define DEBUG_WARNINGS		0
static bool debug_warnings;
static attr_def debug_attr_list[DEBUG_ATTRIBUTES] = {
	{TOKEN_WARNINGS, VALUE_BOOLEAN, &debug_warnings, false}
};

// ENTRANCE tag (spot file only).

#define ENTRANCE_ATTRIBUTES	3
#define ENTRANCE_ANGLE		1
static string entrance_name;
static direction entrance_angle;
static mapcoords entrance_location;
static attr_def entrance_attr_list[ENTRANCE_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &entrance_name, true},
	{TOKEN_ANGLE, VALUE_HEADING, &entrance_angle, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &entrance_location, true}
};

// EXIT tag (block and spot files).

#define EXIT_ATTRIBUTES			6
#define ACTION_EXIT_ATTRIBUTES	3
#define EXIT_IS_SPOT			1
#define EXIT_TARGET				2
#define EXIT_TRIGGER			3
#define EXIT_TEXT				4
static string exit_href;
static bool exit_is_spot;
static string exit_target;
static int exit_trigger;
static string exit_text;
static mapcoords exit_location;
static attr_def exit_attr_list[EXIT_ATTRIBUTES] = {
	{TOKEN_HREF, VALUE_STRING, &exit_href, true},
	{TOKEN_IS_SPOT, VALUE_BOOLEAN, &exit_is_spot, false},
	{TOKEN_TARGET, VALUE_STRING, &exit_target, false},
	{TOKEN_TRIGGER, VALUE_EXIT_TRIGGER, &exit_trigger, false},
	{TOKEN_TEXT, VALUE_STRING, &exit_text, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &exit_location, true}
};

// FOG tag (spot file only).

#define FOG_ATTRIBUTES		5
#define FOG_STYLE			0
#define FOG_COLOUR			1
#define FOG_START			2
#define FOG_END				3
#define FOG_DENSITY			4
static RGBcolour fog_colour;
static int fog_style;
static float fog_start;
static float fog_end;
static float fog_density;
static attr_def fog_attr_list[FOG_ATTRIBUTES] = {
	{TOKEN_STYLE, VALUE_FOG_STYLE, &fog_style, false},
	{TOKEN_COLOUR, VALUE_RGB, &fog_colour, false},
	{TOKEN_START, VALUE_RADIUS, &fog_start, false},
	{TOKEN_END, VALUE_RADIUS, &fog_end, false},
	{TOKEN_DENSITY, VALUE_PERCENTAGE, &fog_density, false}
};

// GROUND tag (style and spot files).

#ifdef STREAMING_MEDIA

#define GROUND_ATTRIBUTES	3
#define GROUND_TEXTURE		0
#define GROUND_COLOUR		1
#define GROUND_STREAM		2
static string ground_texture;
static RGBcolour ground_colour;
static string ground_stream;
static attr_def ground_attr_list[GROUND_ATTRIBUTES] = {
	{TOKEN_TEXTURE, VALUE_STRING, &ground_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &ground_colour, false},
	{TOKEN_STREAM, VALUE_NAME, &ground_stream, false}
};

#else

#define GROUND_ATTRIBUTES	2
#define GROUND_TEXTURE		0
#define GROUND_COLOUR		1
static string ground_texture;
static RGBcolour ground_colour;
static attr_def ground_attr_list[GROUND_ATTRIBUTES] = {
	{TOKEN_TEXTURE, VALUE_STRING, &ground_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &ground_colour, false}
};

#endif

// IMAGEMAP tag (spot file only).

#define IMAGEMAP_ATTRIBUTES 1
static string imagemap_name;
static attr_def imagemap_attr_list[IMAGEMAP_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &imagemap_name, true}
};

// IMPORT tag (spot file only).

#define IMPORT_ATTRIBUTES	1
static string import_href;
static attr_def import_attr_list[IMPORT_ATTRIBUTES] = {
	{TOKEN_HREF, VALUE_STRING, &import_href, true}
};

// LEVEL tag (spot file only).

#define LEVEL_ATTRIBUTES	1
#define LEVEL_NUMBER		0
static int level_number;
static attr_def level_attr_list[LEVEL_ATTRIBUTES] = {
	{TOKEN_NUMBER, VALUE_INTEGER, &level_number, false}
};

// LOAD tag (spot file only).

#define LOAD_ATTRIBUTES	2
#define LOAD_TEXTURE	0
#define LOAD_SOUND		1
static string load_texture_href;
static string load_sound_href;
static attr_def load_attr_list[LOAD_ATTRIBUTES] = {
	{TOKEN_TEXTURE, VALUE_STRING, &load_texture_href, false},
	{TOKEN_SOUND, VALUE_STRING, &load_sound_href, false}
};

// MAP tag (spot file only).

#define MAP_ATTRIBUTES	2
#define MAP_STYLE		1
static mapcoords map_dimensions;
static int map_style;
static attr_def map_attr_list[MAP_ATTRIBUTES] = {
	{TOKEN_DIMENSIONS, VALUE_MAP_DIMENSIONS, &map_dimensions, true},
	{TOKEN_STYLE, VALUE_MAP_STYLE, &map_style, false}
};


// META tag (spot file only).

#define META_ATTRIBUTES	2
#define META_NAME		0
#define META_CONTENT	1
static string meta_name;
static string meta_content;
static attr_def meta_attr_list[META_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_STRING, &meta_name, false},
	{TOKEN_CONTENT, VALUE_STRING, &meta_content, false}
};

// ORB tag (style and spot files).

#ifdef STREAMING_MEDIA

#define ORB_ATTRIBUTES	9
#define ORB_TEXTURE		0
#define ORB_POSITION	1
#define ORB_BRIGHTNESS	2
#define ORB_COLOUR		3
#define ORB_HREF		4
#define ORB_IS_SPOT		5
#define ORB_TARGET		6
#define ORB_TEXT		7
#define ORB_STREAM		8
static string orb_texture;
static direction orb_position;
static float orb_intensity;
static RGBcolour orb_colour;
static string orb_href;
static bool orb_is_spot;
static string orb_target;
static string orb_text;
static string orb_stream;
static attr_def orb_attr_list[ORB_ATTRIBUTES] = {
	{TOKEN_TEXTURE, VALUE_STRING, &orb_texture, false},
	{TOKEN_POSITION, VALUE_DIRECTION, &orb_position, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &orb_intensity, false},
	{TOKEN_COLOUR, VALUE_RGB, &orb_colour, false},
	{TOKEN_HREF, VALUE_STRING, &orb_href, false},
	{TOKEN_IS_SPOT, VALUE_BOOLEAN, &orb_is_spot, false},
	{TOKEN_TARGET, VALUE_STRING, &orb_target, false},
	{TOKEN_TEXT, VALUE_STRING, &orb_text, false},
	{TOKEN_STREAM, VALUE_NAME, &orb_stream, false}
};

#else

#define ORB_ATTRIBUTES	8
#define ORB_TEXTURE		0
#define ORB_POSITION	1
#define ORB_BRIGHTNESS	2
#define ORB_COLOUR		3
#define ORB_HREF		4
#define ORB_IS_SPOT		5
#define ORB_TARGET		6
#define ORB_TEXT		7
static string orb_texture;
static direction orb_position;
static float orb_intensity;
static RGBcolour orb_colour;
static string orb_href;
static bool orb_is_spot;
static string orb_target;
static string orb_text;
static attr_def orb_attr_list[ORB_ATTRIBUTES] = {
	{TOKEN_TEXTURE, VALUE_STRING, &orb_texture, false},
	{TOKEN_POSITION, VALUE_DIRECTION, &orb_position, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &orb_intensity, false},
	{TOKEN_COLOUR, VALUE_RGB, &orb_colour, false},
	{TOKEN_HREF, VALUE_STRING, &orb_href, false},
	{TOKEN_IS_SPOT, VALUE_BOOLEAN, &orb_is_spot, false},
	{TOKEN_TARGET, VALUE_STRING, &orb_target, false},
	{TOKEN_TEXT, VALUE_STRING, &orb_text, false}
};

#endif

// PARAM tag for structural blocks (block and spot files).

#define PARAM_ATTRIBUTES	8
#define PARAM_ORIENTATION	0
#define PARAM_ORIGIN		1
#define PARAM_SOLID			2
#define PARAM_MOVABLE		3
#define PARAM_SCALE			4
#define PARAM_ROTATE		5
#define PARAM_POSITION		6
static orientation param_orientation;
static vertex param_origin;
static bool param_solid;
static bool param_movable;
static float_triplet param_scale;
static float_triplet param_rotate;
static relinteger_triplet param_position;
static attr_def param_attr_list[PARAM_ATTRIBUTES] = {
	{TOKEN_ORIENTATION, VALUE_ORIENTATION, &param_orientation, false},
	{TOKEN_ORIGINS, VALUE_VERTEX_COORDS, &param_origin, false},
	{TOKEN_SOLID, VALUE_BOOLEAN, &param_solid, false},
	{TOKEN_MOVABLE, VALUE_BOOLEAN, &param_movable, false},
	{TOKEN_SCALE, VALUE_SCALE, &param_scale, false},
	{TOKEN_ROTATE, VALUE_SCALE, &param_rotate, false},
	{TOKEN_POSITION, VALUE_REL_INTEGER_TRIPLET, &param_position, false}
};

// PARAM tag for sprite blocks (block and spot files).

#define SPRITE_PARAM_ATTRIBUTES	6
#define SPRITE_ANGLE			0
#define SPRITE_SPEED			1
#define SPRITE_ALIGNMENT		2
#define SPRITE_SOLID			3
#define SPRITE_MOVABLE			4
#define SPRITE_SIZE				5
static float sprite_angle;
static float sprite_speed;
static int sprite_alignment;
static bool sprite_solid;
static bool sprite_movable;
static size sprite_size;
static attr_def sprite_attr_param_list[SPRITE_PARAM_ATTRIBUTES] = {
	{TOKEN_ANGLE, VALUE_DEGREES, &sprite_angle, false},
	{TOKEN_SPEED, VALUE_FLOAT, &sprite_speed, false},
	{TOKEN_ALIGN, VALUE_VALIGNMENT, &sprite_alignment, false},
	{TOKEN_SOLID, VALUE_BOOLEAN, &sprite_solid, false},
	{TOKEN_MOVABLE, VALUE_BOOLEAN, &sprite_movable, false},
	{TOKEN_SIZE, VALUE_SPRITE_SIZE, &sprite_size, false}
};

// PART tag (block file only).

#ifdef STREAMING_MEDIA

#define PART_ATTRIBUTES			9
#define SPRITE_PART_ATTRIBUTES	6
#define PART_TEXTURE			1
#define PART_STREAM				2
#define PART_COLOUR				3
#define PART_TRANSLUCENCY		4
#define PART_STYLE				5
#define PART_FACES				6
#define PART_ANGLE				7
#define PART_SOLID				8
static string part_name;
static string part_texture;
static string part_stream;
static RGBcolour part_colour;
static float part_translucency;
static int part_style;
static int part_faces;
static float part_angle;
static bool part_solid;
static attr_def part_attr_list[PART_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &part_name, true},
	{TOKEN_TEXTURE, VALUE_STRING, &part_texture, false},
	{TOKEN_STREAM, VALUE_NAME, &part_stream, false},
	{TOKEN_COLOUR, VALUE_RGB, &part_colour, false},
	{TOKEN_TRANSLUCENCY, VALUE_PERCENTAGE, &part_translucency, false},
	{TOKEN_STYLE, VALUE_TEXTURE_STYLE, &part_style, false},
	{TOKEN_FACES, VALUE_INTEGER, &part_faces, false},
	{TOKEN_ANGLE, VALUE_DEGREES, &part_angle, false},
	{TOKEN_SOLID, VALUE_BOOLEAN, &part_solid, false}
};

#else

#define PART_ATTRIBUTES			8
#define SPRITE_PART_ATTRIBUTES	5
#define PART_TEXTURE			1
#define PART_COLOUR				2
#define PART_TRANSLUCENCY		3
#define PART_STYLE				4
#define PART_FACES				5
#define PART_ANGLE				6
#define PART_SOLID				7
static string part_name;
static string part_texture;
static RGBcolour part_colour;
static float part_translucency;
static int part_style;
static int part_faces;
static float part_angle;
static bool part_solid;
static attr_def part_attr_list[PART_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &part_name, true},
	{TOKEN_TEXTURE, VALUE_STRING, &part_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &part_colour, false},
	{TOKEN_TRANSLUCENCY, VALUE_PERCENTAGE, &part_translucency, false},
	{TOKEN_STYLE, VALUE_TEXTURE_STYLE, &part_style, false},
	{TOKEN_FACES, VALUE_INTEGER, &part_faces, false},
	{TOKEN_ANGLE, VALUE_DEGREES, &part_angle, false},
	{TOKEN_SOLID, VALUE_BOOLEAN, &part_solid, false}
};

#endif

// PART tag (spot file only). 

#ifdef STREAMING_MEDIA

#define CREATE_PART_ATTRIBUTES	11
#define PART_PROJECTION			9
#define PART_RECT				10
static int part_projection;
static video_rect part_rect;
static attr_def create_part_attr_list[CREATE_PART_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME_LIST, &part_name, true},
	{TOKEN_TEXTURE,	VALUE_STRING, &part_texture, false},
	{TOKEN_STREAM, VALUE_NAME, &part_stream, false},
	{TOKEN_COLOUR, VALUE_RGB, &part_colour, false},
	{TOKEN_TRANSLUCENCY, VALUE_PERCENTAGE, &part_translucency, false},
	{TOKEN_STYLE, VALUE_TEXTURE_STYLE, &part_style, false},
	{TOKEN_FACES, VALUE_INTEGER, &part_faces, false},
	{TOKEN_ANGLE, VALUE_DEGREES, &part_angle, false},
	{TOKEN_PROJECTION, VALUE_PROJECTION, &part_projection, false},
	{TOKEN_SOLID, VALUE_BOOLEAN, &part_solid, false},
	{TOKEN_RECT, VALUE_RECT, &part_rect, false}
};

#else

#define CREATE_PART_ATTRIBUTES	9
#define PART_PROJECTION			8
static int part_projection;
static attr_def create_part_attr_list[CREATE_PART_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME_LIST, &part_name, true},
	{TOKEN_TEXTURE,	VALUE_STRING, &part_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &part_colour, false},
	{TOKEN_TRANSLUCENCY, VALUE_PERCENTAGE, &part_translucency, false},
	{TOKEN_STYLE, VALUE_TEXTURE_STYLE, &part_style, false},
	{TOKEN_FACES, VALUE_INTEGER, &part_faces, false},
	{TOKEN_ANGLE, VALUE_DEGREES, &part_angle, false},
	{TOKEN_SOLID, VALUE_BOOLEAN, &part_solid, false},
	{TOKEN_PROJECTION, VALUE_PROJECTION, &part_projection, false}

};

#endif


// PLACEHOLDER tag (style and spot files).

#define PLACEHOLDER_ATTRIBUTES 1
static string placeholder_texture;
static attr_def placeholder_attr_list[PLACEHOLDER_ATTRIBUTES] = {
	{TOKEN_TEXTURE, VALUE_STRING, &placeholder_texture, true}
};

// PLAYER tag (spot file only).

#define PLAYER_ATTRIBUTES		14
#define PLAYER_BLOCK			0
#define PLAYER_SIZE				1
#define PLAYER_CAMERA			2
static word player_block;
static vertex player_size;
static float player_eye_level;
static vertex player_camera;
static attr_def player_attr_list[PLAYER_ATTRIBUTES] = {
	{TOKEN_BLOCK, VALUE_SYMBOL, &player_block, false},
	{TOKEN_SIZE, VALUE_VERTEX_COORDS, &player_size, false},
	{TOKEN_CAMERA, VALUE_VERTEX_COORDS, &player_camera, false},
};

// POINT_LIGHT tag (block and spot files).

#define POINT_LIGHT_ATTRIBUTES	9
#define POINT_LIGHT_NAME		0
#define POINT_LIGHT_STYLE		1
#define POINT_LIGHT_POSITION	2
#define POINT_LIGHT_BRIGHTNESS	3
#define POINT_LIGHT_RADIUS		4
#define POINT_LIGHT_SPEED		5
#define POINT_LIGHT_FLOOD		6
#define POINT_LIGHT_COLOUR		7
static string point_light_name;
static int point_light_style;
static vertex point_light_position;
static pcrange point_light_brightness;
static float point_light_radius;
static float point_light_speed;
static bool point_light_flood;
static RGBcolour point_light_colour;
static mapcoords point_light_location;
static attr_def point_light_attr_list[POINT_LIGHT_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &point_light_name, false},
	{TOKEN_STYLE, VALUE_POINT_LIGHT_STYLE, &point_light_style, false},
	{TOKEN_POSITION, VALUE_VERTEX_COORDS, &point_light_position, false},
	{TOKEN_BRIGHTNESS, VALUE_PCRANGE, &point_light_brightness, false},
	{TOKEN_RADIUS, VALUE_RADIUS, &point_light_radius, false},
	{TOKEN_SPEED, VALUE_FLOAT, &point_light_speed, false},
	{TOKEN_FLOOD, VALUE_BOOLEAN, &point_light_flood, false},
	{TOKEN_COLOUR, VALUE_RGB, &point_light_colour, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &point_light_location, true}
};

// POLYGON tag (block file only).

#define POLYGON_ATTRIBUTES	5
#define POLYGON_FRONT		3
#define POLYGON_REAR		4 
static int polygon_ref;
static string polygon_vertices;
static string polygon_texcoords;
static int polygon_front;
static int polygon_rear;
static attr_def polygon_attr_list[POLYGON_ATTRIBUTES] = {
	{TOKEN_REF,	VALUE_INTEGER, &polygon_ref, true},
	{TOKEN_VERTICES, VALUE_STRING, &polygon_vertices, true},
	{TOKEN_TEXCOORDS, VALUE_STRING, &polygon_texcoords, true},
	{TOKEN_FRONT, VALUE_INTEGER, &polygon_front, false},
	{TOKEN_REAR, VALUE_INTEGER, &polygon_rear, false}
};

// POPUP tag (spot file only).

#ifdef STREAMING_MEDIA

#define POPUP_ATTRIBUTES	14
#define POPUP_NAME			0
#define POPUP_PLACEMENT		1
#define POPUP_RADIUS		2
#define POPUP_BRIGHTNESS	3
#define POPUP_TEXTURE		4
#define POPUP_COLOUR		5
#define POPUP_SIZE			6
#define POPUP_TEXT			7
#define POPUP_TEXTCOLOUR	8
#define POPUP_TEXTALIGN		9
#define POPUP_IMAGEMAP		10
#define POPUP_TRIGGER		11
#define POPUP_STREAM		12
#define POPUP_LOCATION		13
static string popup_name;
static int popup_placement;
static float popup_radius;
static float popup_brightness;
static string popup_texture;
static RGBcolour popup_colour;
static size popup_size;
static string popup_text;
static RGBcolour popup_textcolour;
static int popup_textalign;
static string popup_imagemap;
static int popup_trigger;
static string popup_stream;
static mapcoords popup_location;
static attr_def popup_attr_list[POPUP_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &popup_name, false},
	{TOKEN_PLACEMENT, VALUE_PLACEMENT, &popup_placement, false},
	{TOKEN_RADIUS, VALUE_RADIUS, &popup_radius, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &popup_brightness, false},
	{TOKEN_TEXTURE,	VALUE_STRING, &popup_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &popup_colour, false},
	{TOKEN_SIZE, VALUE_SIZE, &popup_size, false},
	{TOKEN_TEXT, VALUE_STRING, &popup_text, false},
	{TOKEN_TEXTCOLOUR, VALUE_RGB, &popup_textcolour, false},
	{TOKEN_TEXTALIGN, VALUE_ALIGNMENT, &popup_textalign, false},
	{TOKEN_IMAGEMAP, VALUE_NAME, &popup_imagemap, false},
	{TOKEN_TRIGGER, VALUE_POPUP_TRIGGER, &popup_trigger, false},
	{TOKEN_STREAM, VALUE_NAME, &popup_stream, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &popup_location, false}
};

#else

#define POPUP_ATTRIBUTES	13
#define POPUP_NAME			0
#define POPUP_PLACEMENT		1
#define POPUP_RADIUS		2
#define POPUP_BRIGHTNESS	3
#define POPUP_TEXTURE		4
#define POPUP_COLOUR		5
#define POPUP_SIZE			6
#define POPUP_TEXT			7
#define POPUP_TEXTCOLOUR	8
#define POPUP_TEXTALIGN		9
#define POPUP_IMAGEMAP		10
#define POPUP_TRIGGER		11
#define POPUP_LOCATION		12
static string popup_name;
static int popup_placement;
static float popup_radius;
static float popup_brightness;
static string popup_texture;
static RGBcolour popup_colour;
static size popup_size;
static string popup_text;
static RGBcolour popup_textcolour;
static int popup_textalign;
static string popup_imagemap;
static int popup_trigger;
static mapcoords popup_location;
static attr_def popup_attr_list[POPUP_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &popup_name, false},
	{TOKEN_PLACEMENT, VALUE_PLACEMENT, &popup_placement, false},
	{TOKEN_RADIUS, VALUE_RADIUS, &popup_radius, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &popup_brightness, false},
	{TOKEN_TEXTURE,	VALUE_STRING, &popup_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &popup_colour, false},
	{TOKEN_SIZE, VALUE_SIZE, &popup_size, false},
	{TOKEN_TEXT, VALUE_STRING, &popup_text, false},
	{TOKEN_TEXTCOLOUR, VALUE_RGB, &popup_textcolour, false},
	{TOKEN_TEXTALIGN, VALUE_ALIGNMENT, &popup_textalign, false},
	{TOKEN_IMAGEMAP, VALUE_NAME, &popup_imagemap, false},
	{TOKEN_TRIGGER, VALUE_POPUP_TRIGGER, &popup_trigger, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &popup_location, false}
};

#endif

// REPLACE tag (spot file only).

#define REPLACE_ATTRIBUTES	2
#define REPLACE_TARGET		1
static blockref replace_source;
static relcoords replace_target;
static attr_def replace_attr_list[REPLACE_ATTRIBUTES] = {
	{TOKEN__SOURCE, VALUE_BLOCK_REF, &replace_source, true},
	{TOKEN_TARGET, VALUE_REL_COORDS, &replace_target, false},
};

// RIPPLE tag (spot file only)

#define RIPPLE_ATTRIBUTES	4
#define RIPPLE_STYLE		0
#define RIPPLE_FORCE		1
#define RIPPLE_DROPRATE		2
#define RIPPLE_DAMP			3
static int	 ripple_style;
static float ripple_force;
static float ripple_droprate;
static float ripple_damp;
static attr_def ripple_attr_list[RIPPLE_ATTRIBUTES] = {
	{TOKEN_STYLE, VALUE_RIPPLE_STYLE, &ripple_style, false},
	{TOKEN_FORCE, VALUE_FLOAT, &ripple_force, false},
	{TOKEN_DROPRATE, VALUE_PERCENTAGE, &ripple_droprate, false},
	{TOKEN_DAMP, VALUE_PERCENTAGE, &ripple_damp, false}
};

// SPIN tag (spot file only)

#define SPIN_ATTRIBUTES	1
#define SPIN_ANGLES		0
static float_triplet	spin_angles; 
static attr_def spin_attr_list[SPIN_ATTRIBUTES] = {
	{TOKEN_ANGLES, VALUE_ANGLES, &spin_angles, false}
};

// ORBIT tag (spot file only)

#define ORBIT_ATTRIBUTES	3
#define ORBIT_SOURCE		0
#define ORBIT_DISTANCE		1
#define ORBIT_SPEED			2
static blockref			orbit_source;
static float_triplet	orbit_distance;
static int				orbit_speed; 
static attr_def orbit_attr_list[ORBIT_ATTRIBUTES] = {
	{TOKEN__SOURCE, VALUE_BLOCK_REF, &orbit_source, false},
	{TOKEN_DISTANCE, VALUE_ANGLES, &orbit_distance, false},
	{TOKEN_SPEED, VALUE_INTEGER, &orbit_speed, false}
};

// MOVE tag (spot file only)

#define MOVE_ATTRIBUTES		1
#define MOVE_PATTERN		0
static string			move_pattern;
static attr_def move_attr_list[MOVE_ATTRIBUTES] = {
	{TOKEN_PATTERN, VALUE_STRING, &move_pattern, false}
};

// SETFRAME tag (spot file only)

#define SETFRAME_ATTRIBUTES	1
#define SETFRAME_NUMBER		0
static relinteger	setframe_number; 
static attr_def setframe_attr_list[SETFRAME_ATTRIBUTES] = {
	{TOKEN_NUMBER, VALUE_REL_INTEGER, &setframe_number, true}
};

// SETLOOP tag (spot file only)

#define SETLOOP_ATTRIBUTES	1
#define SETLOOP_NUMBER		0
static relinteger	setloop_number; 
static attr_def setloop_attr_list[SETLOOP_ATTRIBUTES] = {
	{TOKEN_NUMBER, VALUE_REL_INTEGER, &setloop_number, true}
};

// SKY tag (style and spot files).

#ifdef STREAMING_MEDIA

#define SKY_ATTRIBUTES	4
#define SKY_TEXTURE		0
#define SKY_COLOUR		1
#define SKY_BRIGHTNESS	2
#define SKY_STREAM		3
static string sky_texture;
static RGBcolour sky_color;
static float sky_intensity;
static string sky_stream;
static attr_def sky_attr_list[SKY_ATTRIBUTES] = {
	{TOKEN_TEXTURE,	VALUE_STRING, &sky_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &sky_color, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &sky_intensity, false},
	{TOKEN_STREAM, VALUE_NAME, &sky_stream, false}
};

#else

#define SKY_ATTRIBUTES	3
#define SKY_TEXTURE		0
#define SKY_COLOUR		1
#define SKY_BRIGHTNESS	2
static string sky_texture;
static RGBcolour sky_color;
static float sky_intensity;
static attr_def sky_attr_list[SKY_ATTRIBUTES] = {
	{TOKEN_TEXTURE,	VALUE_STRING, &sky_texture, false},
	{TOKEN_COLOUR, VALUE_RGB, &sky_color, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &sky_intensity, false}
};

#endif

// SOUND tag (block and spot files)

#define SOUND_ATTRIBUTES	9
#define SOUND_NAME			0
#define SOUND_FILE			1
#define SOUND_RADIUS		2
#define SOUND_VOLUME		3
#define SOUND_PLAYBACK		4
#define SOUND_DELAY			5
#define SOUND_FLOOD			6
#define SOUND_ROLLOFF		7
static string sound_name;
static string sound_file;
static float sound_radius;
static float sound_volume;
static int sound_playback;
static delayrange sound_delay;
static bool sound_flood;
static float sound_rolloff;
static mapcoords sound_location;
static attr_def sound_attr_list[SOUND_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &sound_name, false},
	{TOKEN_FILE, VALUE_STRING, &sound_file, false},
	{TOKEN_RADIUS, VALUE_RADIUS, &sound_radius, false},
	{TOKEN_VOLUME, VALUE_PERCENTAGE, &sound_volume, false},
	{TOKEN_PLAYBACK, VALUE_PLAYBACK_MODE, &sound_playback, false},
	{TOKEN_DELAY, VALUE_DELAY_RANGE, &sound_delay, false},
	{TOKEN_FLOOD, VALUE_BOOLEAN, &sound_flood, false},
	{TOKEN_ROLLOFF, VALUE_FLOAT, &sound_rolloff, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &sound_location, true}
};

// SPOT tag (spot file only).

#define SPOT_ATTRIBUTES	1
#define SPOT_VERSION	0
static int spot_version;
static attr_def spot_attr_list[SPOT_ATTRIBUTES] = {
	{TOKEN_VERSION, VALUE_VERSION, &spot_version, false}
};

// SPOT_LIGHT tag (block and spot files).

#define SPOT_LIGHT_ATTRIBUTES	11
#define SPOT_LIGHT_NAME			0
#define SPOT_LIGHT_STYLE		1
#define SPOT_LIGHT_POSITION		2
#define SPOT_LIGHT_BRIGHTNESS	3
#define SPOT_LIGHT_RADIUS		4
#define SPOT_LIGHT_DIRECTION	5
#define SPOT_LIGHT_SPEED		6
#define SPOT_LIGHT_CONE			7
#define SPOT_LIGHT_FLOOD		8
#define SPOT_LIGHT_COLOUR		9
static string spot_light_name;
static int spot_light_style;
static vertex spot_light_position;
static float spot_light_brightness;
static float spot_light_radius;
static dirrange spot_light_direction;
static float spot_light_speed;
static float spot_light_cone;
static bool spot_light_flood;
static RGBcolour spot_light_colour;
static mapcoords spot_light_location;
static attr_def spot_light_attr_list[SPOT_LIGHT_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &spot_light_name, false},
	{TOKEN_STYLE, VALUE_SPOT_LIGHT_STYLE, &spot_light_style, false},
	{TOKEN_POSITION, VALUE_VERTEX_COORDS, &spot_light_position, false},
	{TOKEN_BRIGHTNESS, VALUE_PERCENTAGE, &spot_light_brightness, false},
	{TOKEN_RADIUS, VALUE_RADIUS, &spot_light_radius, false},
	{TOKEN_DIRECTION, VALUE_DIRRANGE, &spot_light_direction, false},
	{TOKEN_SPEED, VALUE_FLOAT, &spot_light_speed, false},
	{TOKEN_CONE, VALUE_DEGREES, &spot_light_cone, false},
	{TOKEN_FLOOD, VALUE_BOOLEAN, &spot_light_flood, false},
	{TOKEN_COLOUR, VALUE_RGB, &spot_light_colour, false},
	{TOKEN_LOCATION, VALUE_MAP_COORDS, &spot_light_location, true}
};

#ifdef STREAMING_MEDIA

// STREAM tag (spot file only).

#define STREAM_ATTRIBUTES	3
#define STREAM_RP			1
#define STREAM_WMP			2
static string stream_name;
static string stream_rp;
static string stream_wmp;
static attr_def stream_attr_list[STREAM_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_NAME, &stream_name, true},
	{TOKEN_RP, VALUE_STRING, &stream_rp, false},
	{TOKEN_WMP, VALUE_STRING, &stream_wmp, false}
};

#endif

// STYLE tag (style file only).

#define STYLE_ATTRIBUTES	3
#define STYLE_NAME			0
#define STYLE_SYNOPSIS		1
#define STYLE_VERSION		2
static string style_name;
static string style_synopsis;
static int style_version;
static attr_def style_attr_list[STYLE_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_STRING, &style_name, false},
	{TOKEN_SYNOPSIS, VALUE_STRING, &style_synopsis, false},
	{TOKEN_VERSION, VALUE_VERSION, &style_version, false}
};

// TITLE tag (spot file only).

#define TITLE_ATTRIBUTES 1
static string title_name;
static attr_def title_attr_list[TITLE_ATTRIBUTES] = {
	{TOKEN_NAME, VALUE_STRING, &title_name, true}
};

// VERSION tag (blockset version file only).

#define BLOCKSET_VERSION_ATTRIBUTES	2
static unsigned int blockset_version_id;
static int blockset_version_size;
static attr_def blockset_version_attr_list[BLOCKSET_VERSION_ATTRIBUTES] = {
	{TOKEN_ID, VALUE_VERSION, &blockset_version_id, true},
	{TOKEN_SIZE, VALUE_INTEGER, &blockset_version_size, true}
};

// VERSION tag (Rover version file only).

#define ROVER_VERSION_ATTRIBUTES 1
static unsigned int rover_version_id;
static attr_def rover_version_attr_list[ROVER_VERSION_ATTRIBUTES] = {
	{TOKEN_ID, VALUE_VERSION, &rover_version_id, true}
};

// VERTEX tag (block file only).

#define VERTEX_ATTRIBUTES	2
static int vertex_ref;
static vertex_entry vertex_coords;
static attr_def vertex_attr_list[VERTEX_ATTRIBUTES] = {
	{TOKEN_REF,	VALUE_INTEGER, &vertex_ref,	true},
	{TOKEN_COORDS, VALUE_VERTEX_COORDS,	&vertex_coords, true}
};

// VERTICES tag (block file only)
#define VERTICES_ATTRIBUTES	1
#define VERTICES_SIZE		0
static int vertices_size;
static attr_def vertices_attr_list[VERTICES_ATTRIBUTES] = {
	{TOKEN_SIZE,	VALUE_INTEGER, &vertices_size,	false}
};


// FRAMES tag (block file only)
#define FRAMES_ATTRIBUTES	1
#define FRAMES_SIZE			0
static int frames_size;
static attr_def frames_attr_list[FRAMES_ATTRIBUTES] = {
	{TOKEN_SIZE,	VALUE_INTEGER, &frames_size,	true}
};

// FRAME tag (block file only).

#define FRAME_ATTRIBUTES	1
static int frame_ref;
static attr_def frame_attr_list[FRAME_ATTRIBUTES] = {
	{TOKEN_REF,	VALUE_INTEGER, &frame_ref,	true}
};

// LOOPS tag (block file only)
#define LOOPS_ATTRIBUTES	1
#define LOOPS_SIZE			0
static int loops_size;
static attr_def loops_attr_list[LOOPS_ATTRIBUTES] = {
	{TOKEN_SIZE,	VALUE_INTEGER, &loops_size,	true}
};

// LOOP tag (block file only).

#define LOOP_ATTRIBUTES	3
#define LOOP_REF		0
#define LOOP_SIZE		1
#define LOOP_FRAMES		2
static int loop_ref;
static intrange loop_range;
static attr_def loop_attr_list[LOOP_ATTRIBUTES] = {
	{TOKEN_REF,	VALUE_INTEGER, &loop_ref,	true},
	{TOKEN_FRAMES,	VALUE_INTEGER_RANGE, &loop_range, true}
};

//------------------------------------------------------------------------------
// Tag lists found inside other tags.
//------------------------------------------------------------------------------

// ACTION tag list (spot file only).

static tag_def action_tag_list[] = {
	{TOKEN_EXIT, exit_attr_list, ACTION_EXIT_ATTRIBUTES, false},
	{TOKEN_REPLACE, replace_attr_list, REPLACE_ATTRIBUTES, false},
	{TOKEN_RIPPLE, ripple_attr_list, RIPPLE_ATTRIBUTES, false},
	{TOKEN_SPIN, spin_attr_list, SPIN_ATTRIBUTES, false},
	{TOKEN_ORBIT, orbit_attr_list, ORBIT_ATTRIBUTES, false},
	{TOKEN_MOVE, move_attr_list, MOVE_ATTRIBUTES, false},
	{TOKEN_SETFRAME, setframe_attr_list, SETFRAME_ATTRIBUTES, false},
	{TOKEN_SETLOOP, setloop_attr_list, SETLOOP_ATTRIBUTES, false},
	{TOKEN_ANIMATE, NULL, 0, false},
	{TOKEN_STOPSPIN, NULL, 0, false},
	{TOKEN_STOPMOVE, NULL, 0, false},
	{TOKEN_STOPRIPPLE, NULL, 0, false},
	{TOKEN_STOPORBIT, NULL, 0, false},
	{TOKEN_NONE}
};

// BLOCK tag list for structural blocks (block file only).

static tag_def block_tag_list[] = {
	{TOKEN_BSP_TREE, BSP_tree_attr_list, BSP_TREE_ATTRIBUTES, false},
	{TOKEN_EXIT, exit_attr_list, EXIT_ATTRIBUTES - 1, false},
	{TOKEN_PARAM, param_attr_list, PARAM_ATTRIBUTES, false},
	{TOKEN_FRAMES, frames_attr_list, FRAMES_ATTRIBUTES, true},
	{TOKEN_LOOPS, loops_attr_list, LOOPS_ATTRIBUTES, true},
	{TOKEN_PARTS, NULL, 0, true},
	{TOKEN_POINT_LIGHT, point_light_attr_list, POINT_LIGHT_ATTRIBUTES - 1, 
		false},
	{TOKEN_SOUND, sound_attr_list, SOUND_ATTRIBUTES - 1, false},
	{TOKEN_SPOT_LIGHT, spot_light_attr_list, SPOT_LIGHT_ATTRIBUTES - 1, false},
	{TOKEN_VERTICES, vertices_attr_list, VERTICES_ATTRIBUTES, true},
	{TOKEN_NONE}
};

// BLOCK tag list for sprite blocks (block file only).

static tag_def sprite_block_tag_list[] = {
	{TOKEN_EXIT, exit_attr_list, EXIT_ATTRIBUTES - 1, false},
	{TOKEN_PARAM, sprite_attr_param_list, SPRITE_PARAM_ATTRIBUTES, false},
	{TOKEN_PART, part_attr_list, SPRITE_PART_ATTRIBUTES, false},
	{TOKEN_POINT_LIGHT, point_light_attr_list, POINT_LIGHT_ATTRIBUTES - 1, 
		false},
	{TOKEN_SOUND, sound_attr_list, SOUND_ATTRIBUTES - 1, false},
	{TOKEN_SPOT_LIGHT, spot_light_attr_list, SPOT_LIGHT_ATTRIBUTES - 1, false},
	{TOKEN_NONE}
};

// BODY tag list (spot file only).

static tag_def body_tag_list[] = {
	{TOKEN_ACTION, action_attr_list, ACTION_ATTRIBUTES, true},
	{TOKEN_CREATE, create_attr_list, CREATE_ATTRIBUTES, true},
	{TOKEN_DEFINE, NULL, 0, true},
	{TOKEN_ENTRANCE, entrance_attr_list, ENTRANCE_ATTRIBUTES, false},
	{TOKEN_EXIT, exit_attr_list, EXIT_ATTRIBUTES, false},
	{TOKEN_IMAGEMAP, imagemap_attr_list, IMAGEMAP_ATTRIBUTES, true},
	{TOKEN_IMPORT, import_attr_list, IMPORT_ATTRIBUTES, false},
	{TOKEN_LEVEL, level_attr_list, LEVEL_ATTRIBUTES, true},
	{TOKEN_LOAD, load_attr_list, LOAD_ATTRIBUTES, false},
	{TOKEN_PLAYER, player_attr_list, PLAYER_ATTRIBUTES, false},
	{TOKEN_POINT_LIGHT, point_light_attr_list, POINT_LIGHT_ATTRIBUTES, false},
	{TOKEN_POPUP, popup_attr_list, POPUP_ATTRIBUTES, false},
	{TOKEN_SCRIPT, action_attr_list, ACTION_ATTRIBUTES, true},
	{TOKEN_SOUND, sound_attr_list, SOUND_ATTRIBUTES, false},
	{TOKEN_SPOT_LIGHT, spot_light_attr_list, SPOT_LIGHT_ATTRIBUTES, false},
	{TOKEN_NONE}
};

// CACHE tag list (cache file only).

static tag_def cache_tag_list[] = {
	{TOKEN_BLOCKSET, cached_blockset_attr_list, CACHED_BLOCKSET_ATTRIBUTES, 
		false},
	{TOKEN_NONE}
};

// CREATE tag list for structural blocks (block file only).

static tag_def structural_create_tag_list[] = {
	{TOKEN_ACTION, action_attr_list, ACTION_ATTRIBUTES - 1, true},
	{TOKEN_DEFINE, NULL, 0, true},
	{TOKEN_ENTRANCE, entrance_attr_list, ENTRANCE_ATTRIBUTES - 1, false},
	{TOKEN_EXIT, exit_attr_list, EXIT_ATTRIBUTES - 1, false},
	{TOKEN_IMPORT, import_attr_list, IMPORT_ATTRIBUTES, false},
	{TOKEN_PARAM, param_attr_list, PARAM_ATTRIBUTES, false},
	{TOKEN_PART, create_part_attr_list, CREATE_PART_ATTRIBUTES, false},
	{TOKEN_POINT_LIGHT, point_light_attr_list, POINT_LIGHT_ATTRIBUTES - 1,
		false},
	{TOKEN_POPUP, popup_attr_list, POPUP_ATTRIBUTES - 1, false},
	{TOKEN_SCRIPT, action_attr_list, ACTION_ATTRIBUTES - 1, true},
	{TOKEN_SOUND, sound_attr_list, SOUND_ATTRIBUTES - 1, false},
	{TOKEN_SPOT_LIGHT, spot_light_attr_list, SPOT_LIGHT_ATTRIBUTES - 1, false},
	{TOKEN_NONE}
};

// CREATE tag list for sprites (block file only).

static tag_def sprite_create_tag_list[] = {
	{TOKEN_ACTION, action_attr_list, ACTION_ATTRIBUTES - 1, true},
	{TOKEN_DEFINE, NULL, 0, true},
	{TOKEN_ENTRANCE, entrance_attr_list, ENTRANCE_ATTRIBUTES - 1, false},
	{TOKEN_EXIT, exit_attr_list, EXIT_ATTRIBUTES - 1, false},
	{TOKEN_IMPORT, import_attr_list, IMPORT_ATTRIBUTES, false},
	{TOKEN_PARAM, sprite_attr_param_list, SPRITE_PARAM_ATTRIBUTES, false},
	{TOKEN_PART, create_part_attr_list, PART_ATTRIBUTES, false},
	{TOKEN_POINT_LIGHT, point_light_attr_list, POINT_LIGHT_ATTRIBUTES - 1,
		false},
	{TOKEN_POPUP, popup_attr_list, POPUP_ATTRIBUTES - 1, false},
	{TOKEN_SCRIPT, action_attr_list, ACTION_ATTRIBUTES - 1, true},
	{TOKEN_SOUND, sound_attr_list, SOUND_ATTRIBUTES - 1, false},
	{TOKEN_SPOT_LIGHT, spot_light_attr_list, SPOT_LIGHT_ATTRIBUTES - 1, false},
	{TOKEN_NONE}
};

// DIRECTORY tag list (directory file only).

static tag_def directory_tag_list[] = {
	{TOKEN_CATEGORY, category_attr_list, CATEGORY_ATTRIBUTES, true},
	{TOKEN_NONE}
};

// HEAD tag list (spot file only).

static tag_def head_tag_list[] = {
	{TOKEN_AMBIENT_LIGHT, ambient_light_attr_list, AMBIENT_LIGHT_ATTRIBUTES,
		false},
	{TOKEN_AMBIENT_SOUND, ambient_sound_attr_list, AMBIENT_SOUND_ATTRIBUTES,
		false},
	{TOKEN_BASE, base_attr_list, BASE_ATTRIBUTES, false},
	{TOKEN_BLOCKSET, blockset_attr_list, BLOCKSET_ATTRIBUTES, false},
	{TOKEN_DEBUG, debug_attr_list, DEBUG_ATTRIBUTES, false},
	{TOKEN_FOG, fog_attr_list, FOG_ATTRIBUTES, false},
	{TOKEN_GROUND, ground_attr_list, GROUND_ATTRIBUTES, false},
	{TOKEN_MAP, map_attr_list, MAP_ATTRIBUTES, false},
	{TOKEN_META,meta_attr_list,META_ATTRIBUTES, false},
	{TOKEN_ORB, orb_attr_list, ORB_ATTRIBUTES, false},
	{TOKEN_PLACEHOLDER, placeholder_attr_list, PLACEHOLDER_ATTRIBUTES, false},
	{TOKEN_SKY, sky_attr_list, SKY_ATTRIBUTES, false},
#ifdef STREAMING_MEDIA
	{TOKEN_STREAM, stream_attr_list, STREAM_ATTRIBUTES, false},
#endif
	{TOKEN_TITLE, title_attr_list, TITLE_ATTRIBUTES, false},
	{TOKEN_NONE}
};

// IMAGEMAP tag list (block file only).

static tag_def imagemap_tag_list[] = {
	{TOKEN_ACTION, imagemap_action_attr_list, IMAGEMAP_ACTION_ATTRIBUTES,
		true},
	{TOKEN_AREA, area_attr_list, AREA_ATTRIBUTES, true},
	{TOKEN_SCRIPT, imagemap_action_attr_list, IMAGEMAP_ACTION_ATTRIBUTES,
		true},
	{TOKEN_NONE}
};

// FRAME tag list (block file only).

static tag_def frame_tag_list[] = {
	{TOKEN_VERTICES, vertices_attr_list, VERTICES_ATTRIBUTES, true},
	{TOKEN_NONE}
};

// FRAMES tag list (block file only)

static tag_def frames_tag_list[] = {
	{TOKEN_FRAME, frame_attr_list, FRAME_ATTRIBUTES, true},
	{TOKEN_NONE}
};

// LOOPS tag list (block file only)

static tag_def loops_tag_list[] = {
	{TOKEN_LOOP, loop_attr_list, LOOP_ATTRIBUTES, true},
	{TOKEN_NONE}
};

// PART tag list (block file only).

static tag_def part_tag_list[] = {
	{TOKEN_POLYGON, polygon_attr_list, POLYGON_ATTRIBUTES, false},
	{TOKEN_NONE}
};

// PARTS tag list (block file only).

static tag_def parts_tag_list[] = {
	{TOKEN_PART, part_attr_list, PART_ATTRIBUTES, true},
	{TOKEN_NONE}
};

// SPOT tag list (spot file only).

static tag_def spot_tag_list[] = {
	{TOKEN_BODY, NULL, 0, true},
	{TOKEN_HEAD, NULL, 0, true},
	{TOKEN_NONE}
};

// STYLE tag list (style file only).

static tag_def style_tag_list[] = {
	{TOKEN_BLOCK, style_block_attr_list, STYLE_BLOCK_ATTRIBUTES, false},
	{TOKEN_GROUND, ground_attr_list, GROUND_ATTRIBUTES, false},
	{TOKEN_ORB, orb_attr_list, ORB_ATTRIBUTES, false},
	{TOKEN_PLACEHOLDER, placeholder_attr_list, PLACEHOLDER_ATTRIBUTES, false},
	{TOKEN_SKY, sky_attr_list, SKY_ATTRIBUTES, false},
	{TOKEN_NONE}
};

// VERTICES tag list (block file only).

static tag_def vertices_tag_list[] = {
	{TOKEN_VERTEX, vertex_attr_list, VERTEX_ATTRIBUTES, false},
	{TOKEN_NONE}
};
