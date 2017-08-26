//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <direct.h>
#include <time.h>
#include "Unzip\unzip.h"
#include "Classes.h"
#include "Fileio.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Utils.h"

// String buffer for error messages.

#define ERROR_MSG_SIZE	1024
static char error_str[ERROR_MSG_SIZE];

// Flag indicating whether strict XML compliance is required, and if we're 
// inside of a tag

static bool strict_XML_compliance;
static bool inside_tag;

// Parse stack element class.

struct parse_stack_element {
	entity *entity_list;
	entity *curr_entity_ptr;
};

// File stack element class.

#define MAX_PARSE_STACK_DEPTH 8

struct file_stack_element {
	string file_URL;
	bool text_file;
	int line_no;
	long file_size;
	long file_position;
	char *file_buffer;
	char *file_buffer_ptr;
	parse_stack_element parse_stack[MAX_PARSE_STACK_DEPTH];
	parse_stack_element *parse_stack_ptr;
	int parse_stack_depth;
};

// Zip archive name and handle.

static string zip_archive_name;
static unzFile zip_archive_handle;

// File stack.

#define MAX_FILE_STACK_DEPTH 4
static file_stack_element file_stack[MAX_FILE_STACK_DEPTH];
static file_stack_element *file_stack_ptr;
static int file_stack_depth;

// Last file token parsed, as both a string and a numerical token.

static string file_token_string;
static int file_token;
static int file_token_line_no;

// Array of flags indicating which attributes of a tag have been parsed.

bool parsed_attribute[MAX_ATTRIBUTES];

// XML symbol string to token conversion table.

struct symbol_def {
	const char *name;
	int token;
};

// XML symbol table.

static symbol_def xml_symbol_table[] = {
	{"<!--",			TOKEN_OPEN_COMMENT},
	{"-->",				TOKEN_CLOSE_COMMENT},
	{"</",				TOKEN_OPEN_END_TAG},
	{"/>",				TOKEN_CLOSE_SINGLE_TAG},
	{"<",				TOKEN_OPEN_TAG},
	{">",				TOKEN_CLOSE_TAG},
	{"=",				TOKEN_EQUALS_SIGN},
	{"\"",				TOKEN_QUOTE},
	{"'",				TOKEN_QUOTE},
	{" ",				TOKEN_WHITESPACE},
	{"\t",				TOKEN_WHITESPACE},
	{"\n",				TOKEN_WHITESPACE},
	{"\r",				TOKEN_WHITESPACE},
	{NULL,				TOKEN_NONE}
};

// Entity name to value conversion table.

struct entity_ref {
	char *name;
	char value;
};

entity_ref xml_entity_table[] = {
	{"lt",		'<'},
	{"gt",		'>'},
	{"amp",		'&'},
	{"apos",	'\''},
	{"quot",	 '"'},
	{NULL,		'\0'}
};

// Token symbol table.

static symbol_def symbol_table[] = {
	{"action",			TOKEN_ACTION},
	{"align",			TOKEN_ALIGN},
	{"ambient_light",	TOKEN_AMBIENT_LIGHT},
	{"ambient_sound",	TOKEN_AMBIENT_SOUND},
	{"animate",			TOKEN_ANIMATE},
	{"angle",			TOKEN_ANGLE},
	{"angles",			TOKEN_ANGLES},
	{"area",			TOKEN_AREA},
	{"base",			TOKEN_BASE},
	{"block",			TOKEN_BLOCK},
	{"blockset",		TOKEN_BLOCKSET},
	{"body",			TOKEN_BODY},
	{"brightness",		TOKEN_BRIGHTNESS},
	{"bsp_tree",		TOKEN_BSP_TREE},
	{"cache",			TOKEN_CACHE},
	{"camera",			TOKEN_CAMERA},
	{"category",		TOKEN_CATEGORY},
	{"class",			TOKEN_CLASS},
	{"coords",			TOKEN_COORDS},
	{"cone",			TOKEN_CONE},
	{"color",			TOKEN_COLOUR},
	{"colour",			TOKEN_COLOUR},
	{"command",			TOKEN_COMMAND},
	{"content",			TOKEN_CONTENT},
	{"create",			TOKEN_CREATE},
	{"damp",			TOKEN_DAMP},
	{"debug",			TOKEN_DEBUG},
	{"define",			TOKEN_DEFINE},
	{"delay",			TOKEN_DELAY},
	{"density",			TOKEN_DENSITY},
	{"dimensions",		TOKEN_DIMENSIONS},
	{"direction",		TOKEN_DIRECTION},
	{"directory",		TOKEN_DIRECTORY},
	{"distance",		TOKEN_DISTANCE},
	{"double",			TOKEN_DOUBLE},
	{"droprate",		TOKEN_DROPRATE},
	{"end",				TOKEN_END},
	{"entrance",		TOKEN_ENTRANCE},
	{"exit",			TOKEN_EXIT},
	{"extends",			TOKEN_EXTENDS},
	{"faces",			TOKEN_FACES},
	{"fast_mode",		TOKEN_FAST_MODE},
	{"file",			TOKEN_FILE},
	{"flood",			TOKEN_FLOOD},
	{"fog",				TOKEN_FOG},
	{"force",			TOKEN_FORCE},
	{"frames",			TOKEN_FRAMES},
	{"frame",			TOKEN_FRAME},
	{"front",			TOKEN_FRONT},
	{"go_faster",		TOKEN_GO_FASTER},
	{"go_slower",		TOKEN_GO_SLOWER},
	{"ground",			TOKEN_GROUND},
	{"head",			TOKEN_HEAD},
	{"href",			TOKEN_HREF},
	{"icon",			TOKEN_ICON},
	{"id",				TOKEN_ID},
	{"imagemap",		TOKEN_IMAGEMAP},
	{"import",			TOKEN_IMPORT},
	{"is_spot",			TOKEN_IS_SPOT},
	{"jump",			TOKEN_JUMP},
	{"key",				TOKEN_KEY},
	{"level",			TOKEN_LEVEL},
	{"light",			TOKEN_POINT_LIGHT},
	{"load",			TOKEN_LOAD},
	{"location",		TOKEN_LOCATION},
	{"look_down",		TOKEN_LOOK_DOWN},
	{"look_up",			TOKEN_LOOK_UP},
	{"loop",			TOKEN_LOOP},
	{"loops",			TOKEN_LOOPS},
	{"map",				TOKEN_MAP},
	{"meta",			TOKEN_META},
	{"movable",			TOKEN_MOVABLE},
	{"move",			TOKEN_MOVE},
	{"move_back",		TOKEN_MOVE_BACK},
	{"move_forward",	TOKEN_MOVE_FORWARD},
	{"move_left",		TOKEN_MOVE_LEFT},
	{"move_right",		TOKEN_MOVE_RIGHT},
	{"name",			TOKEN_NAME},
	{"number",			TOKEN_NUMBER},
	{"orb",				TOKEN_ORB},
	{"orbit",			TOKEN_ORBIT},
	{"orient",			TOKEN_ORIENTATION},
	{"orientation",		TOKEN_ORIENTATION},
	{"origin",			TOKEN_ORIGINS},
	{"param",			TOKEN_PARAM},
	{"partname",		TOKEN_PARTNAME},
	{"part",			TOKEN_PART},
	{"parts",			TOKEN_PARTS},
	{"pattern",			TOKEN_PATTERN},
	{"placeholder",		TOKEN_PLACEHOLDER},
	{"placement",		TOKEN_PLACEMENT},
	{"playback",		TOKEN_PLAYBACK},
	{"player",			TOKEN_PLAYER},
	{"point_light",		TOKEN_POINT_LIGHT},
	{"polygon",			TOKEN_POLYGON},
	{"polygons",		TOKEN_POLYGONS},
	{"popup",			TOKEN_POPUP},
	{"port",			TOKEN_PORT},
	{"position",		TOKEN_POSITION},
	{"projection",		TOKEN_PROJECTION},
	{"radius",			TOKEN_RADIUS},
	{"rear",			TOKEN_REAR},
#ifdef STREAMING_MEDIA
	{"rect",			TOKEN_RECT},
#endif
	{"ref",				TOKEN_REF},
	{"replace",			TOKEN_REPLACE},
	{"ripple",			TOKEN_RIPPLE},
	{"rolloff",			TOKEN_ROLLOFF},
	{"root",			TOKEN_ROOT},
	{"rotate",			TOKEN_ROTATE},
#ifdef STREAMING_MEDIA
	{"rp",				TOKEN_RP},
#endif
	{"scale",			TOKEN_SCALE},
	{"script",			TOKEN_SCRIPT},
	{"setframe",		TOKEN_SETFRAME},
	{"setloop",			TOKEN_SETLOOP},
	{"sky",				TOKEN_SKY},
	{"shape",			TOKEN_SHAPE},
	{"sidle_mode",		TOKEN_SIDLE_MODE},
	{"size",			TOKEN_SIZE},
	{"solid",			TOKEN_SOLID},
	{"sound",			TOKEN_SOUND},
	{"source",			TOKEN__SOURCE},
	{"spdesc",			TOKEN_SPDESC},
	{"speed",			TOKEN_SPEED},
	{"spin",			TOKEN_SPIN},
	{"spot",			TOKEN_SPOT},
	{"spot_light",		TOKEN_SPOT_LIGHT},
	{"sprite",			TOKEN_SPRITE},
	{"start",			TOKEN_START},
	{"stopat",			TOKEN_STOPAT},
	{"stopspin",		TOKEN_STOPSPIN},
	{"stopmove",		TOKEN_STOPMOVE},
	{"stopripple",		TOKEN_STOPRIPPLE},
	{"stoporbit",		TOKEN_STOPORBIT},
#ifdef STREAMING_MEDIA
	{"stream",			TOKEN_STREAM},
#endif
	{"style",			TOKEN_STYLE},
	{"symbol",			TOKEN_SYMBOL},
	{"synopsis",		TOKEN_SYNOPSIS},
	{"target",			TOKEN_TARGET},
	{"texcoords",		TOKEN_TEXCOORDS},
	{"text",			TOKEN_TEXT},
	{"textalign",		TOKEN_TEXTALIGN},
	{"textcolor",		TOKEN_TEXTCOLOUR},
	{"textcolour",		TOKEN_TEXTCOLOUR},
	{"texture",			TOKEN_TEXTURE},
	{"title",			TOKEN_TITLE},
	{"trigger",			TOKEN_TRIGGER},
	{"translucency",	TOKEN_TRANSLUCENCY},
	{"type",			TOKEN__TYPE},
	{"updated",			TOKEN_UPDATED},
	{"velocity",		TOKEN_VELOCITY},
	{"version",			TOKEN_VERSION},
	{"vertex",			TOKEN_VERTEX},
	{"vertices",		TOKEN_VERTICES},
	{"volume",			TOKEN_VOLUME},
	{"warnings",		TOKEN_WARNINGS},
#ifdef STREAMING_MEDIA
	{"wmp",				TOKEN_WMP},
#endif
	{NULL,				TOKEN_NONE}
};

// Action trigger values.

#define ACTION_TRIGGER_VALUES 12
static value_def action_trigger_value_list[ACTION_TRIGGER_VALUES] = {
	{"start",  START_UP},
	{"roll on", ROLL_ON},
	{"roll off", ROLL_OFF},
	{"step in", STEP_IN},
	{"step out", STEP_OUT},
	{"click on", CLICK_ON},
	{"timer", TIMER},
	{"location", LOCATION},
	{"proximity", PROXIMITY},
	{"key down", KEY_DOWN},
	{"key up", KEY_UP},
	{"key hold", KEY_HOLD}
};

// Alignment values.

#define ALIGNMENT_VALUES 11
static value_def alignment_value_list[ALIGNMENT_VALUES] = {
	{"top-left", TOP_LEFT},
	{"top-right", TOP_RIGHT},
	{"top", TOP},
	{"left", LEFT},
	{"centre", CENTRE},
	{"center", CENTRE},
	{"right", RIGHT},
	{"bottom-left", BOTTOM_LEFT},
	{"bottom-right", BOTTOM_RIGHT},
	{"bottom", BOTTOM},
	{"mouse", MOUSE}
};

// Boolean values.

#define BOOLEAN_VALUES 2
static value_def boolean_value_list[BOOLEAN_VALUES] = {
	{"yes", 1},
	{"no", 0}
};

// Block type values.

#define BLOCK_TYPE_VALUES 6
static value_def block_type_value_list[BLOCK_TYPE_VALUES] = {
	{"structural", STRUCTURAL_BLOCK},
	{"multifaceted sprite", MULTIFACETED_SPRITE},
	{"angled sprite", ANGLED_SPRITE},
	{"revolving sprite", REVOLVING_SPRITE},
	{"facing sprite", FACING_SPRITE}
};

// Exit trigger values.

#define EXIT_TRIGGER_VALUES 2
static value_def exit_trigger_value_list[EXIT_TRIGGER_VALUES] = {
	{"click on", CLICK_ON},
	{"step on", STEP_ON}
};

// Fog style values.

#define FOG_STYLE_VALUES 2
static value_def fog_style_value_list[FOG_STYLE_VALUES] = {
	{"linear", LINEAR_FOG},
	{"exponential", EXPONENTIAL_FOG}
};

// Ripple style values.

#define RIPPLE_STYLE_VALUES 2
static value_def ripple_style_value_list[RIPPLE_STYLE_VALUES] = {
	{"raindrops", RAIN_RIPPLE},
	{"waves", WAVES_RIPPLE}
};

// Imagemap trigger values.

#define IMAGEMAP_TRIGGER_VALUES 3
static value_def imagemap_trigger_value_list[IMAGEMAP_TRIGGER_VALUES] = {
	{"roll on", ROLL_ON},
	{"roll off", ROLL_OFF},
	{"click on", CLICK_ON}
};

// Map style values.

#define MAP_STYLE_VALUES 2
static value_def map_style_value_list[MAP_STYLE_VALUES] = {
	{"single", SINGLE_MAP},
	{"double", DOUBLE_MAP}
};

// Orientation values.

#define ORIENTATION_VALUES 6
static value_def orientation_value_list[ORIENTATION_VALUES] = {
	{"north", NORTH},
	{"south", SOUTH},
	{"east", EAST},
	{"west", WEST},
	{"up", UP},
	{"down", DOWN}
};

// Playback mode values.

#define PLAYBACK_MODE_VALUES 4
static value_def playback_mode_value_list[PLAYBACK_MODE_VALUES] = {
	{"looped", LOOPED_PLAY},
	{"random", RANDOM_PLAY},
	{"single", SINGLE_PLAY},
	{"once", ONE_PLAY}
};

// Point light style.

#define POINT_LIGHT_STYLE_VALUES 2
static value_def point_light_style_value_list[POINT_LIGHT_STYLE_VALUES] = {
	{"static", STATIC_POINT_LIGHT},
	{"pulsate", PULSATING_POINT_LIGHT}
};

// Popup trigger values.

#define POPUP_TRIGGER_VALUES 3
static value_def popup_trigger_value_list[POPUP_TRIGGER_VALUES] = {
	{"proximity", PROXIMITY},
	{"rollover", ROLLOVER},
	{"everywhere", EVERYWHERE}
};

// Projection values.

#define PROJECTION_VALUES 6
static value_def projection_value_list[PROJECTION_VALUES] = {
	{"north", NORTH},
	{"south", SOUTH},
	{"east", EAST},
	{"west", WEST},
	{"top", UP},
	{"bottom", DOWN}
};

// Shape values.

#define SHAPE_VALUES 2
static value_def shape_value_list[SHAPE_VALUES] = {
	{"rect", RECT_SHAPE},
	{"circle", CIRCLE_SHAPE}
};

// Spot light style values.

#define SPOT_LIGHT_STYLE_VALUES 3
static value_def spot_light_style_value_list[SPOT_LIGHT_STYLE_VALUES] = {
	{"static", STATIC_SPOT_LIGHT},
	{"revolve", REVOLVING_SPOT_LIGHT},
	{"search", SEARCHING_SPOT_LIGHT}
};

// Texture style values.

#define TEXTURE_STYLE_VALUES 3
static value_def texture_style_value_list[TEXTURE_STYLE_VALUES] = {
	{"tiled", TILED_TEXTURE},
	{"scaled", SCALED_TEXTURE},
	{"stretched", STRETCHED_TEXTURE}
};

// Vertical alignment values.

#define VERTICAL_ALIGNMENT_VALUES 4
static value_def vertical_alignment_value_list[VERTICAL_ALIGNMENT_VALUES] = {
	{"top", TOP},
	{"centre", CENTRE},
	{"center", CENTRE},
	{"bottom", BOTTOM}
};

// Key code values.

#define KEY_CODE_VALUES	KEY_CODES - 1
static value_def key_code_value_list[KEY_CODE_VALUES] = {
	{"shift", SHIFT_KEY},
	{"ctrl", CONTROL_KEY},
	{"alt", ALT_KEY},
	{"space", SPACE_BAR_KEY},
	{"backspace", BACK_SPACE_KEY},
	{"enter", ENTER_KEY},
	{"insert", INSERT_KEY},
	{"delete", DELETE_KEY},
	{"home", HOME_KEY},
	{"end", END_KEY},
	{"page up", PAGE_UP_KEY},
	{"page down", PAGE_DOWN_KEY},
	{"up", UP_KEY},
	{"down", DOWN_KEY},
	{"left", LEFT_KEY},
	{"right", RIGHT_KEY},
	{"pad 0", NUMPAD_0_KEY},
	{"pad 1", NUMPAD_1_KEY},
	{"pad 2", NUMPAD_2_KEY},
	{"pad 3", NUMPAD_3_KEY},
	{"pad 4", NUMPAD_4_KEY},
	{"pad 5", NUMPAD_5_KEY},
	{"pad 6", NUMPAD_6_KEY},
	{"pad 7", NUMPAD_7_KEY},
	{"pad 8", NUMPAD_8_KEY},
	{"pad 9", NUMPAD_9_KEY},
	{"pad +", NUMPAD_ADD_KEY},
	{"pad -", NUMPAD_SUBTRACT_KEY},
	{"pad *", NUMPAD_MULTIPLY_KEY},
	{"pad /", NUMPAD_DIVIDE_KEY},
	{"pad .", NUMPAD_PERIOD_KEY}
};

// The current tag and parameter being parsed.

static int curr_tag_token;
static int curr_attr_token;
static bool curr_attr_required;

// Pointer to the current attribute value, and the part of it being parsed.

static char *value_string;
static char *value_string_ptr;
static char *last_value_string_ptr;

// Table of unsafe characters in a URL which must be encoded.

#define	UNSAFE_CHARS	13	
static char unsafe_char[UNSAFE_CHARS] = {
	' ', '<', '>', '%', '{', '}', '|', '\\', '^', '~', '[', ']', '`'
};

// Indent level for when converting a nested entity list into a string.

static int indent_level;

//==============================================================================
// Parser intialisation function.
//==============================================================================

void
init_parser(void)
{
	// Initialise the zip archive handle.

	zip_archive_handle = NULL;

	// Initialise the file stack.

	file_stack_depth = 0;
	file_stack_ptr = NULL;
}

//==============================================================================
// URL and conversion functions.
//==============================================================================

//------------------------------------------------------------------------------
// Split a URL into it's components.
//------------------------------------------------------------------------------

void 
split_URL(const char *URL, string *URL_dir, string *file_name,
		  string *entrance_name)
{
	const char *name_ptr;
	string new_URL;
	int index;

	// First locate the division between the file path and entrance name, and
	// seperate them.  If there is no entrance name, it is set to "default".
	// If the entrance parameter is NULL, the entrance name is not copied.

	name_ptr = strrchr(URL, '#');
	if (name_ptr != NULL) {
		new_URL = URL;
		new_URL.truncate(name_ptr - URL);
		if (entrance_name)
			*entrance_name = name_ptr + 1;
	} else {
		new_URL = URL;
		if (entrance_name)
			*entrance_name = "default";
	}

	// Next locate the division between the URL directory and the file name,
	// and seperate them.  If there is no URL directory, it is set to an empty
	// string.  If either the URL_dir or the file_name parameter are NULL, the
	// URL directory or file name components are not copied.

	index = strlen(new_URL) - 1;
	name_ptr = new_URL;
	while (index >= 0) {
		if (name_ptr[index] == '/' || name_ptr[index] == '\\')
			break;
		index--;
	}
	if (index >= 0) {
		if (file_name)
			*file_name = name_ptr + index + 1;
		if (URL_dir) {
			*URL_dir = name_ptr;
			URL_dir->truncate(index + 1);
		}
	} else {
		if (file_name)
			*file_name = name_ptr;
		if (URL_dir)
			*URL_dir = (const char *)NULL;
	}
}

//------------------------------------------------------------------------------
// Create a URL from URL directory, and file name components.  If the file name 
// contains an absolute URL, it overrides the URL directory component.
//------------------------------------------------------------------------------

string
create_URL(const char *URL_dir, const char *file_name)
{
	string URL;

	// If the file name starts with a slash, a drive letter followed by a colon,
	// or the strings "http:" or "file:", then assume it's an absolute URL.

	if (file_name[0] == '/' || file_name[0] == '\\' ||
		(strlen(file_name) > 1 && isalpha(file_name[0]) && file_name[1] == ':') ||
		!_strnicmp(file_name, "http:", 5) || !_strnicmp(file_name, "file:", 5))
		URL = file_name;

	// Otherwise concatenate the URL directory and file name parameters to create
	// the URL.  It is assumed the URL directory parameters has a trailing
	// slash.
	
	else {
		if (URL_dir != NULL)
			URL += URL_dir;
		URL += file_name;
	}
	return(URL);
}

//------------------------------------------------------------------------------
// Convert a character to a hexadecimal digit.
//------------------------------------------------------------------------------

static char
char_to_hex_digit(char ch)
{
	if (ch >= '0' && ch <= '9')
		return(ch - '0');
	else if (ch >= 'A' && ch <= 'F')
		return(ch - 'A' + 10);
	else
		return(ch - 'a' + 10);
}

//------------------------------------------------------------------------------
// Convert a hexadecimal digit to a character.
//------------------------------------------------------------------------------

static char
hex_digit_to_char(char digit)
{
	if (digit >= 0 && digit <= 9)
		return(digit + '0');
	else
		return(digit - 10 + 'A');
}

//------------------------------------------------------------------------------
// Change a URL so that unsafe characters are encoded as "%nn", where nn is the
// ASCII code in hexadecimal.
//------------------------------------------------------------------------------

string
encode_URL(const char *URL)
{
	const char *old_char_ptr;
	char new_URL[_MAX_PATH];
	char *new_char_ptr;
	int index, new_size;

	// Copy the URL into the new URL string, encoding the unsafe characters
	// along the way.  If the encoded URL becomes too large, just truncate it.

	old_char_ptr = URL;
	new_char_ptr = new_URL;
	new_size = 0;
	while (*old_char_ptr != '\0' && new_size < _MAX_PATH - 4) {
		for (index = 0; index < UNSAFE_CHARS; index++)
			if (*old_char_ptr == unsafe_char[index]) {
				*new_char_ptr++ = '%';
				*new_char_ptr++ = hex_digit_to_char((*old_char_ptr & 0xf0) >> 4);
				*new_char_ptr++ = hex_digit_to_char(*old_char_ptr & 0x0f);
				new_size += 3;
				break;
			} 
		if (index == UNSAFE_CHARS) {
			*new_char_ptr++ = *old_char_ptr;
			new_size++;
		}
		old_char_ptr++;
	}
	*new_char_ptr = '\0';
	return(new_URL);
}

//------------------------------------------------------------------------------
// Change a URL so that characters encoded as "%nn" are decoded, where nn is the
// ASCII code in hexadecimal.
//------------------------------------------------------------------------------

string
decode_URL(const char *URL)
{
	string new_URL;
	char *old_char_ptr, *new_char_ptr;

	if (URL == NULL || *URL == '\0') {
		return URL;
	}
	new_URL = URL;
	old_char_ptr = new_URL;
	new_char_ptr = new_URL;
	while (*old_char_ptr) {
		if (*old_char_ptr == '%') {
			old_char_ptr++;
			char first_digit = char_to_hex_digit(*old_char_ptr++);
			char second_digit = char_to_hex_digit(*old_char_ptr++);
			*new_char_ptr++ = (first_digit << 4) + second_digit;
		} else
			*new_char_ptr++ = *old_char_ptr++;
	}		
	*new_char_ptr = '\0';
	return(new_URL);
}

//------------------------------------------------------------------------------
// Convert a URL to a file path.
//------------------------------------------------------------------------------

string
URL_to_file_path(const char *URL)
{
	string file_path;
	char *char_ptr;

	// If the URL begins with "file:///", "file://localhost/" or "file://",
	// skip over it.

	if (!_strnicmp(URL, "file:///", 8))
		file_path = URL + 8;
	else if (!_strnicmp(URL, "file://localhost/", 17))
		file_path = URL + 17;
	else if (!_strnicmp(URL, "file://", 7))
		file_path = URL + 7;
	else
		file_path = URL;
		
	// Replace forward slashes with back slashes, and vertical bars to colons
	// in the file path.

	char_ptr = file_path;
	while (*char_ptr) {
		if (*char_ptr == '/')
			*char_ptr = '\\';
		else if (*char_ptr == '|')
			*char_ptr = ':';
		char_ptr++;
	}

	// Return the file path.

	return(file_path);
}

//------------------------------------------------------------------------------
// Split an identifier into a style name and object name.
//------------------------------------------------------------------------------

void
parse_identifier(const char *identifier, string &style_name, 
				 string &object_name)
{
	const char *colon_ptr;
	int style_name_length;

	// Find the first colon in the identifier.  If not found, the object name is
	// the entire identifier.

	if ((colon_ptr = strchr(identifier, ':')) == NULL) {
		style_name = (const char *)NULL;
		object_name = identifier;
	}

	// Otherwise copy the string before the colon into the style name buffer,
	// and the string after the colon into the object name buffer.

	else {
		style_name_length = colon_ptr - identifier;
		style_name = identifier;
		style_name.truncate(style_name_length);
		object_name = identifier + style_name_length + 1;
	}
}

//------------------------------------------------------------------------------
// Determine if the given character is not a legal symbol.  For spots with a
// version number < 3.2, only '<' is illegal.  For all other spots, '&' is also
// illegal.
//------------------------------------------------------------------------------

bool
not_single_symbol(char ch, bool disallow_dot)
{
	return(ch < FIRST_BLOCK_SYMBOL || ch > LAST_BLOCK_SYMBOL ||
		(disallow_dot && ch == '.') || ch == '<' || 
		(min_rover_version >= 0x03020000 && ch == '&'));
}

//------------------------------------------------------------------------------
// Determine if the given double-character combination is a legal symbol.  For
// spots with a version number < 3.2, only '<' in the first character is
// illegal.  For all other spots, '<' and '&' in either character is illegal.
//------------------------------------------------------------------------------

bool
not_double_symbol(char ch1, char ch2, bool disallow_dot_dot)
{
	return(ch1 < FIRST_BLOCK_SYMBOL || ch1 > LAST_BLOCK_SYMBOL ||
		ch2 < FIRST_BLOCK_SYMBOL || ch2 > LAST_BLOCK_SYMBOL ||
		ch1 == '<' || ch2 == '<' || 
		(min_rover_version >= 0x03020000 && (ch1 == '&' || ch2 == '&')) ||
		(disallow_dot_dot && ch1 == '.' && ch2 == '.'));
}

//------------------------------------------------------------------------------
// Parse a string as a single character symbol; it must be a printable ASCII
// character except '<', ' ', or '.' if disallow_dot is TRUE.
//------------------------------------------------------------------------------

bool
string_to_single_symbol(const char *string_ptr, char *symbol_ptr, bool disallow_dot)
{
	if (strlen(string_ptr) != 1 || not_single_symbol(*string_ptr, disallow_dot))
		return(false);
	*symbol_ptr = *string_ptr;
	return(true);
}

//------------------------------------------------------------------------------
// Parse a string as a double character symbol; it must be a combination of two
// printable ASCII characters except '<' as the first character or ' ' as either
// character, and cannot be '..' if disallow_dot_dot is TRUE.
//------------------------------------------------------------------------------

bool
string_to_double_symbol(const char *string_ptr, word *symbol_ptr, bool disallow_dot_dot)
{
	if (strlen(string_ptr) != 2 || not_double_symbol(*string_ptr, *(string_ptr + 1), disallow_dot_dot))
		return(false);
	if (*string_ptr == '.')
		*symbol_ptr = *(string_ptr + 1);
	else
		*symbol_ptr = (*string_ptr << 7) + *(string_ptr + 1);
	return(true);
}

//------------------------------------------------------------------------------
// Parse a string as a single or double character symbol.
//------------------------------------------------------------------------------

bool
string_to_symbol(const char *string_ptr, word *symbol_ptr, bool disallow_dot_dot)
{
	char symbol;

	switch (world_ptr->map_style) {
	case SINGLE_MAP:
		if (string_to_single_symbol(string_ptr, &symbol, disallow_dot_dot)) {
			*symbol_ptr = symbol;
			return(true);
		}
		break;
	case DOUBLE_MAP:
		if (string_to_double_symbol(string_ptr, symbol_ptr, disallow_dot_dot))
			return(true);
	}
	return(false);
}

//------------------------------------------------------------------------------
// Convert a version number to a version string.
//------------------------------------------------------------------------------

static char version_string[16];

string
version_number_to_string(unsigned int version_number)
{
	int version1, version2, version3, version4;

	version1 = (version_number >> 24) & 255;
	version2 = (version_number >> 16) & 255;
	version3 = (version_number >> 8) & 255;
	version4 = version_number & 255;
	if (version3 == 0) {
		if (version4 == 255)
			bprintf(version_string, 16, "%d.%d", version1, version2);
		else
			bprintf(version_string, 16, "%d.%db%d", version1, version2, version4);
	} else {
		if (version4 == 255)
			bprintf(version_string, 16, "%d.%d.%d", version1, version2, version3);
		else
			bprintf(version_string, 16, "%d.%d.%db%d", version1, version2, version3, version4);
	}
	return(version_string);
}

//==============================================================================
// Zip archive functions.
//==============================================================================

//------------------------------------------------------------------------------
// Open a zip archive.
//------------------------------------------------------------------------------

bool
open_zip_archive(const char *file_path, const char *file_name)
{
	zip_archive_name = file_name;
	if ((zip_archive_handle = unzOpen(file_path)) == NULL)
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Download the blockset at the given URL into the cache.
//------------------------------------------------------------------------------

static cached_blockset *
download_blockset(const char *URL, const char *name)
{
	string file_dir;
	string file_name;
	string cache_file_path;
	cached_blockset *cached_blockset_ptr;
	char *folder;
	FILE *fp;
	int size;

	// Split the URL into a file directory and name, omitting the leading
	// "http://".

 	split_URL(URL + 7, &file_dir, &file_name, NULL);

	// Build the cache file path by starting at the Flatland folder and adding
	// each folder of the file directory one by one.  Finally, add the file
	// name to complete the cache file path.

	cache_file_path = flatland_dir;
	folder = strtok(file_dir, "/\\");
	while (folder) {
		cache_file_path = cache_file_path + folder;
		_mkdir(cache_file_path);
		cache_file_path = cache_file_path + "\\";
		folder = strtok(NULL, "/\\");
	}
	cache_file_path = cache_file_path + file_name;

	// Download the blockset URL to the cache file path.

	requested_blockset_name = name;
	if (!download_URL(URL, cache_file_path, true))
		return(NULL);

	// Determine the size of the blockset.

	if ((fp = fopen(cache_file_path, "r")) == NULL)
		return(NULL);
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fclose(fp);

	// Add the cached blockset to the cached blockset list, and save the list
	// to the cache file.  Then return a pointer to the cached blockset entry.

	if ((cached_blockset_ptr = new_cached_blockset(cache_file_path, URL, size, time(NULL))) == NULL)
		return(NULL);
	save_cached_blockset_list();
	return(cached_blockset_ptr);
}

//------------------------------------------------------------------------------
// Open a cached blockset zip archive, downloading and caching it first if it
// doesn't already exist. 
//------------------------------------------------------------------------------

bool
open_blockset(const char *blockset_URL, const char *blockset_name)
{
	FILE *fp;
	int size;
	string zip_archive_name;
	string blockset_path;
	string cached_blockset_URL;
	string cached_blockset_path;
	string version_file_URL;
	char *ext_ptr;
	cached_blockset *cached_blockset_ptr;

	// If the blockset URL begins with "file://", assume it's a local file that
	// can be opened directly.

	zip_archive_name = blockset_name;
	zip_archive_name += " blockset";
	if (!_strnicmp(blockset_URL, "file://", 7)) {
		blockset_path = URL_to_file_path(blockset_URL);
		if (!open_zip_archive(blockset_path, zip_archive_name))
			return(false);
	}

	// If the blockset path begins with "http://", check to see whether the
	// blockset exists in the cache.  If not, download the blockset and cache
	// it.

	else {

		// Create the file path of the blockset in the cache.

		cached_blockset_URL = create_URL(flatland_dir, blockset_URL + 7);
		cached_blockset_path = URL_to_file_path(cached_blockset_URL);

		// First look for the blockset in the cached blockset list; if not
		// found, check whether the blockset exists in the cache and add it to
		// the cached blockset list if found; otherwise attempt to download the
		// blockset from the given blockset URL.

		if ((cached_blockset_ptr = find_cached_blockset(blockset_URL)) == NULL) {
			if ((fp = fopen(cached_blockset_path, "r")) == NULL) {
				if ((cached_blockset_ptr = download_blockset(blockset_URL, blockset_name)) == NULL)
					return(false);
			} else {
				fseek(fp, 0, SEEK_END);
				size = ftell(fp);
				fclose(fp);
				if ((cached_blockset_ptr = new_cached_blockset(cached_blockset_path, blockset_URL, size, time(NULL))) == NULL)
					return(false);
				save_cached_blockset_list();
			}
		}

		// If the blockset does exist in the cache, the mininum update period
		// has expired, and the current spot is from a web-based URL, 
		// check whether a new update is available.

		if (time(NULL) - cached_blockset_ptr->updated > min_blockset_update_period && spot_on_web) {

			// Construct the URL to the blockset's version file.

			version_file_URL = blockset_URL;
			ext_ptr = strrchr(version_file_URL, '.');
			version_file_URL.truncate(ext_ptr - (char *)version_file_URL);
			version_file_URL += ".txt";

			// Check if there is a new version of the blockset and the user has
			// agreed to download it.  If so, remove the current version of the
			// blockset and download the new one.

			if (check_for_blockset_update(version_file_URL, blockset_name, cached_blockset_ptr->version)) {
				delete_cached_blockset(blockset_URL);
				save_cached_blockset_list();
				remove(cached_blockset_path);
				if (!download_blockset(blockset_URL, blockset_name))
					return(false);
			}

			// If there is no new version or the user has not agreed to download
			// it, set the time of this update check so it won't happen again
			// for another update period.

			else {
				cached_blockset_ptr->updated = time(NULL);
				save_cached_blockset_list();
			}
		}

		// Attempt to open the cached blockset.

		if (!open_zip_archive(cached_blockset_path, zip_archive_name))
			return(false);
	}

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Close the currently open zip archive.
//------------------------------------------------------------------------------

void
close_zip_archive(void)
{
	unzClose(zip_archive_handle);
	zip_archive_handle = NULL;
}

//==============================================================================
// File functions.
//==============================================================================

//------------------------------------------------------------------------------
// Destroy an attribute list.
//------------------------------------------------------------------------------

static void
destroy_attr_list(attr *attr_list)
{
	attr *next_attr_ptr;

	while (attr_list != NULL) {
			next_attr_ptr = attr_list->next_attr_ptr;
		DEL(attr_list, attr);
		attr_list = next_attr_ptr;
	}
}

//------------------------------------------------------------------------------
// Destroy an entity list.
//------------------------------------------------------------------------------

void
destroy_entity_list(entity *entity_list)
{
	entity *entity_ptr, *next_entity_ptr;

	// Destroy each entity in the list.

	entity_ptr = entity_list;
	while (entity_ptr != NULL) {

		// Destroy the attribute list, if it exists.

		destroy_attr_list(entity_ptr->attr_list);

		// Destroy the nested entity list, if it exists.

		if (entity_ptr->nested_entity_list != NULL)
			destroy_entity_list(entity_ptr->nested_entity_list);

		// Destroy the entity itself, then move onto the next entity in the
		// list.

		next_entity_ptr = entity_ptr->next_entity_ptr;
		DEL(entity_ptr, entity);
		entity_ptr = next_entity_ptr;
	}
}

//------------------------------------------------------------------------------
// Destroy an entity.
//------------------------------------------------------------------------------

void
destroy_entity(entity *entity_ptr)
{
	// Destroy the attribute list, if it exists.

	destroy_attr_list(entity_ptr->attr_list);

	// Destroy the nested entity list, if it exists.

	if (entity_ptr->nested_entity_list != NULL)
		destroy_entity_list(entity_ptr->nested_entity_list);

	// Destroy the entity itself.

	DEL(entity_ptr, entity);
}

//------------------------------------------------------------------------------
// Push a new entry onto the file stack, returning a pointer to the file buffer.
//------------------------------------------------------------------------------

static void
push_file_on_stack(string file_URL, bool text_file, bool in_zip_archive, char *file_buffer, int file_size)
{
	// If the file is in the currently open zip archive, add it's name to the
	// file URL.

	if (in_zip_archive) {
		file_URL += " in ";
		file_URL += zip_archive_name;
	}

	// Get a pointer to the top file stack element, and initialise it.

	file_stack_ptr = &file_stack[file_stack_depth++];
	file_stack_ptr->text_file = text_file;
	file_stack_ptr->file_URL = file_URL;
	file_stack_ptr->file_buffer = file_buffer;
	file_stack_ptr->file_size = file_size;
	file_stack_ptr->parse_stack_depth = 0;
	file_stack_ptr->parse_stack_ptr = NULL;

	// If this is a text file, add a NULL terminating character to the buffer,
	// and initialise the current line number.

	if (text_file) {
		file_stack_ptr->file_buffer[file_size] = '\0';
		file_stack_ptr->line_no = 1;
	}

	// Initialise the file position and file buffer pointer.

	file_stack_ptr->file_position = 0;
	file_stack_ptr->file_buffer_ptr = file_stack_ptr->file_buffer;
}

//------------------------------------------------------------------------------
// Open a file and push it onto the parser's file stack.  This file becomes
// the one that is being parsed.
//------------------------------------------------------------------------------

bool
push_file(const char *file_path, const char *file_URL, bool text_file)
{
	FILE *fp;
	char *file_buffer;
	unsigned int file_size;

	// Open the file for reading in binary mode.

	if ((fp = fopen(file_path, "rb")) == NULL)
		return(false);

	// Seek to the end of the file to determine it's size, then seek back to
	// the beginning of the file.

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	// Allocate the file buffer.

	NEWARRAY(file_buffer, char, file_size + 1);
	if (file_buffer == NULL) {
		fclose(fp);
		return(false);
	}

	// Read the contents of the file into the file buffer.

	if (fread(file_buffer, 1, file_size, fp) != file_size) {
		DELARRAY(file_buffer, char, file_size + 1);
		fclose(fp);
		return(false);
	}
	fclose(fp);

	// Push the file onto the file stack.

	push_file_on_stack(file_URL, text_file, false, file_buffer, file_size);
	return(true);
}

//------------------------------------------------------------------------------
// Open the currently selected file in the currently open zip archive, and
// push it onto the parser's file stack.
//------------------------------------------------------------------------------

bool
push_curr_zip_file(const char *file_path, bool text_file, int file_size)
{
	char *file_buffer;

	// Allocate the file buffer.

	NEWARRAY(file_buffer, char, file_size + 1);
	if (file_buffer == NULL)
		return(false);

	// Open the file, read it into the file buffer, then close the file.

	if (unzOpenCurrentFile(zip_archive_handle) != UNZ_OK ||
		unzReadCurrentFile(zip_archive_handle, file_buffer, file_size) < file_size ||
		unzCloseCurrentFile(zip_archive_handle) != UNZ_OK) {
		DELARRAY(file_buffer, char, file_size + 1);
		return(false);
	}

	// Push the file onto the file stack.

	push_file_on_stack(file_path, text_file, true, file_buffer, file_size);
	return(true);
}

//------------------------------------------------------------------------------
// Open a file in the currently open zip archive, and push it onto the
// parser's file stack.
//------------------------------------------------------------------------------

bool
push_zip_file(const char *file_path, bool text_file)
{
	unz_file_info info;

	// Attempt to locate the file in the zip archive.

	if (unzLocateFile(zip_archive_handle, file_path, 0) != UNZ_OK)
		return(false);

	// Get the uncompressed size of the file.

	if (unzGetCurrentFileInfo(zip_archive_handle, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
		return(false);

	// Push the file.

	return(push_curr_zip_file(file_path, text_file, info.uncompressed_size));
}

//------------------------------------------------------------------------------
// Open the first file with the given extension in the currently open zip 
// archive, and push it onto the parser's file stack.  This file becomes the one
// that is being parsed.
//------------------------------------------------------------------------------

bool
push_zip_file_with_ext(const char *file_ext, bool text_file)
{
	unz_file_info info;
	char file_name[_MAX_PATH];
	char *ext_ptr;

	// Go to the first file in the zip archive.

	if (unzGoToFirstFile(zip_archive_handle) != UNZ_OK)
		return(false);

	// Look for a file with the requested extension...

	do {

		// Get the information for the current file.

		if (unzGetCurrentFileInfo(zip_archive_handle, &info, file_name, _MAX_PATH, NULL, 0, NULL, 0) != UNZ_OK)
			return(false);

		// If the file name ends with the requested extension, push it onto the
		// stack.

		ext_ptr = strrchr(file_name, '.');
		if (ext_ptr && !_stricmp(ext_ptr, file_ext))
			return(push_curr_zip_file(file_name, text_file, info.uncompressed_size));

		// Otherwise move onto the next file in the zip archive.

	} while (unzGoToNextFile(zip_archive_handle) == UNZ_OK);
	return(false);
}

//------------------------------------------------------------------------------
// Return the file buffer and size for the top file.
//------------------------------------------------------------------------------

void
get_file_buffer(char **file_buffer_ptr, int *file_size)
{
	*file_buffer_ptr = file_stack_ptr->file_buffer;
	*file_size = file_stack_ptr->file_size;
}

//------------------------------------------------------------------------------
// Push a buffer onto the parser's file stack.  This "file" becomes the one
// that is being parsed.
//------------------------------------------------------------------------------

bool
push_buffer(const char *buffer_ptr, int buffer_size)
{
	char *file_buffer;

	// Allocate the file buffer, and copy the contents of the data buffer into
	// it.

	NEWARRAY(file_buffer, char, buffer_size + 1);
	if (file_buffer == NULL)
		return(false);
	memcpy(file_buffer, buffer_ptr, buffer_size);

	// Push the file onto the file stack.

	push_file_on_stack("", false, false, file_buffer, buffer_size);
	return(true);
}

//------------------------------------------------------------------------------
// Rewind the file on top of the stack.
//------------------------------------------------------------------------------

void
rewind_file(void)
{
	// Initialise the file position, and file buffer pointer.

	file_stack_ptr->file_position = 0;
	file_stack_ptr->file_buffer_ptr = file_stack_ptr->file_buffer;

	// If this is a text file, reset the current line number.

	if (file_stack_ptr->text_file)
		file_stack_ptr->line_no = 1;

	// Destroy the entity list, if it exists.

	if (file_stack_ptr->parse_stack_depth > 0) {
		destroy_entity_list(file_stack_ptr->parse_stack->entity_list);
		file_stack_ptr->parse_stack_depth = 0;
		file_stack_ptr->parse_stack_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// Pop the top file from the stack, returning its entity list if requested
// (otherwise the entity list will be destroyed).
//------------------------------------------------------------------------------

entity *
pop_file(bool return_entity_list)
{
	entity *entity_list = NULL;

	// Delete the file buffer, if it exists.

	if (file_stack_ptr->file_buffer != NULL)
		DELBASEARRAY(file_stack_ptr->file_buffer, char, file_stack_ptr->file_size + 1);

	// Save or destroy the entity list, if it exists.

	if (file_stack_ptr->parse_stack_depth > 0) {
		if (return_entity_list) {
			entity_list = file_stack_ptr->parse_stack->entity_list;
		} else {
			destroy_entity_list(file_stack_ptr->parse_stack->entity_list);
		}
	}
	
	// Pop the file off the stack.  If there is a file below it, make it the
	// new top file.

	file_stack_depth--;
	if (file_stack_depth > 0)
		file_stack_ptr = &file_stack[file_stack_depth - 1];
	else
		file_stack_ptr = NULL;

	// Return the entity list that was saved, if any.

	return entity_list;
}

//------------------------------------------------------------------------------
// Pop all files from the file stack.
//------------------------------------------------------------------------------

void
pop_all_files(void)
{
	while (file_stack_ptr != NULL)
		pop_file();
}

//------------------------------------------------------------------------------
// Read the specified number of bytes from the top file into the specified 
// buffer.  The number of bytes actually read is returned.
//------------------------------------------------------------------------------

int
read_file(byte *buffer_ptr, int bytes)
{
	int bytes_to_copy;

	// If the file position is at the end of the file, don't copy any bytes and
	// return zero.

	if (file_stack_ptr->file_position == file_stack_ptr->file_size)
		return(0);

	// Determine how many bytes can be read from the current file position.

	if (file_stack_ptr->file_position + bytes > file_stack_ptr->file_size)
		bytes_to_copy = file_stack_ptr->file_size - file_stack_ptr->file_position;
	else
		bytes_to_copy = bytes;

	// Copy the available bytes into the supplied buffer, increment the file
	// position and file buffer pointer, and return the number of bytes copied.

	memcpy(buffer_ptr, file_stack_ptr->file_buffer_ptr, bytes_to_copy);
	file_stack_ptr->file_position += bytes_to_copy;
	file_stack_ptr->file_buffer_ptr += bytes_to_copy;
	return(bytes_to_copy);
}

//------------------------------------------------------------------------------
// Copy the contents of the top file into the specified file, performing
// carriage return/line feed conversions if text_file is TRUE.
//------------------------------------------------------------------------------

bool
copy_file(const char *file_path, bool text_file)
{
	FILE *fp;
	char *file_buffer_ptr, *end_buffer_ptr;
	char ch;

	// If there is no file open, return failure.

	if (file_stack_ptr == NULL)
		return(false);

	// Open the target file.

	if ((fp = fopen(file_path, "wb")) == NULL)
		return(false);

	// If the top file is a text file, copy the buffer of the top file byte by
	// byte, converting individual \r or \n characters to the combination
	// \r\n.

	if (text_file) {
		file_buffer_ptr = file_stack_ptr->file_buffer;
		end_buffer_ptr = file_buffer_ptr + file_stack_ptr->file_size;
		while (file_buffer_ptr < end_buffer_ptr) {
			ch = *file_buffer_ptr++;
			switch (ch) {
			case '\r':
				if (file_buffer_ptr < end_buffer_ptr && *file_buffer_ptr == '\n')
					file_buffer_ptr++;
			case '\n':
				fputc('\r', fp);
				fputc('\n', fp);
				break;
			default:
				fputc(ch, fp);
			}
		}
	}
	
	// If the top file is not a text file, copy the entire buffer of the top
	// file into the target file without modifications.

	else {
		if (!fwrite(file_stack_ptr->file_buffer, file_stack_ptr->file_size, 1, fp)) {
			fclose(fp);
			return(false);
		}
	}
	
	// Return success.

	fclose(fp);
	return(true);
}

//==============================================================================
// Error checking and reporting functions.
//==============================================================================

//------------------------------------------------------------------------------
// Functions to write a formatted message to a character buffer, ensuring that
// it does not exceed the size of the buffer.
//------------------------------------------------------------------------------

void
bprintf(char *buffer, int size, const char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	if (_vsnprintf(buffer, size - 1, format, arg_ptr) < 0)
		buffer[size - 1] = '\0';
	va_end(arg_ptr);
}

void
vbprintf(char *buffer, int size, const char *format, va_list arg_ptr)
{
	if (_vsnprintf(buffer, size - 1, format, arg_ptr) < 0)
		buffer[size - 1] = '\0';
}

//------------------------------------------------------------------------------
// Write a formatted message to the error log.
//------------------------------------------------------------------------------

void
write_error_log(const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];
	FILE *fp;

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);
	if ((fp = fopen(error_log_file_path, "a")) != NULL) {
		fputs(message, fp);
		fclose(fp);
	}
}

//------------------------------------------------------------------------------
// Write a formatted diagnostic message to the log file in the Flatland 
// directory.
//------------------------------------------------------------------------------

void
diagnose(const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];
	FILE *fp;

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);
	if ((fp = fopen(log_file_path, "a")) != NULL) {
		fprintf(fp, "%s\n", message);
		fclose(fp);
	}
}

//------------------------------------------------------------------------------
// Write a formatted diagnostic message to the log file in the Flatland 
// directory.
//------------------------------------------------------------------------------

void
log(const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];
	FILE *fp;

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);
	if ((fp = fopen(log_file_path, "a")) != NULL) {
		fprintf(fp, "%s", message);
		fclose(fp);
	}
}

//------------------------------------------------------------------------------
// Write a formatted warning message to the error log file, using the given
// line number.
//------------------------------------------------------------------------------

static void
warning(int line_no, const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];

	// Create the formatted warning message.

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);

	// Add the line number of entity to the message.

	write_error_log("<B>Warning on line %d of %s:</B> %s.\n<BR>\n", line_no, file_stack_ptr->file_URL, message);

	// Set a flag indicating warnings are present in the error log.

	warnings = true;
}

//------------------------------------------------------------------------------
// Write a formatted warning message to the error log file.
//------------------------------------------------------------------------------

void
warning(const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];
	parse_stack_element *parse_stack_ptr;

	// Create the formatted warning message.

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);

	// If a file is open...

	if (file_stack_ptr != NULL) {

		// If an entity is currently been parsed, add the line number of the
		// entity, and the file URL to the error message.

		parse_stack_ptr = file_stack_ptr->parse_stack_ptr;
		if (parse_stack_ptr != NULL && parse_stack_ptr->curr_entity_ptr != NULL) {
			warning(parse_stack_ptr->curr_entity_ptr->line_no, "%s", message);
			return;
		}

		// If an entity is not being parsed, but a file is open, then add the
		// line number and URL of the file if it's text, otherwise just add the
		// URL.

		else if (file_stack_ptr->text_file)
			warning(file_stack_ptr->line_no, "%s", file_stack_ptr->file_URL, message);
		else
			write_error_log("<B>Warning in %s:</B> %s.\n<BR>\n", file_stack_ptr->file_URL, message);
	} 
	
	// Otherwise just write a generic error format.

	else
		write_error_log("<B>Warning:</B> %s.\n<BR>\n", message);

	// Set a flag indicating warnings are present in the error log.

	warnings = true;
}

//------------------------------------------------------------------------------
// Write a formatted memory warning message to the error log file.
//------------------------------------------------------------------------------

void
memory_warning(const char *object)
{
	write_error_log("Insufficient memory for allocating %s", object);
}

//------------------------------------------------------------------------------
// Throw a formatted error message referencing the given line number.
//------------------------------------------------------------------------------

static void
error(int line_no, const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];

	// Create the formatted error message.

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);

	// Add the line number of the entity to it.

	bprintf(error_str, ERROR_MSG_SIZE, "<B>Error on line %d of %s:</B> %s.\n<BR>\n", line_no, file_stack_ptr->file_URL, message);

	// Throw the error message.

	throw (char *)error_str;
}

//------------------------------------------------------------------------------
// Throw a formatted error message.
//------------------------------------------------------------------------------

void
error(const char *format, ...)
{
	va_list arg_ptr;
	char message[ERROR_MSG_SIZE];
	parse_stack_element *parse_stack_ptr;

	// Create the formatted error message.

	va_start(arg_ptr, format);
	vbprintf(message, ERROR_MSG_SIZE, format, arg_ptr);
	va_end(arg_ptr);

	// If a file is open...

	if (file_stack_ptr != NULL) {

		// If an entity is currently been parsed, add the line number of the
		// entity, and the file URL to the error message.

		parse_stack_ptr = file_stack_ptr->parse_stack_ptr;
		if (parse_stack_ptr != NULL && parse_stack_ptr->curr_entity_ptr != NULL)
			error(parse_stack_ptr->curr_entity_ptr->line_no, "%s", message);

		// If an entity is not being parsed, but a file is open, then add the 
		// line number and URL of the file if it's text, otherwise just add the
		// URL.

		else if (file_stack_ptr->text_file)
			error(file_stack_ptr->line_no, "%s", file_stack_ptr->file_URL, message);
		else
			bprintf(error_str, ERROR_MSG_SIZE, "<B>Error in %s:</B> %s.\n<BR>\n", file_stack_ptr->file_URL, message);
	} 
	
	// Otherwise just display a generic error format.

	else
		bprintf(error_str, ERROR_MSG_SIZE, "<B>Error:</B> %s.<BR>\n", message);

	// Throw the error message.

	throw (char *)error_str;
}

//------------------------------------------------------------------------------
// Set the low memory flag, then throw a formatted memory error.
//------------------------------------------------------------------------------

void
memory_error(const char *object)
{
	low_memory = true;
	bprintf(error_str, ERROR_MSG_SIZE, "Insufficient memory for allocating %s", object);
	throw (char *)error_str;
}

//------------------------------------------------------------------------------
// Return the name associated with a given token.
//------------------------------------------------------------------------------

static const char *
get_name(int token)
{
	int index = 0;
	while (symbol_table[index].name != NULL) {
		if (token == symbol_table[index].token)
			return(symbol_table[index].name);
		index++;
	}
	return(NULL);
}

//------------------------------------------------------------------------------
// Return the token for a given name.
//------------------------------------------------------------------------------

static int
get_token(const char *name)
{
	int index = 0;
	while (symbol_table[index].name != NULL) {
		if (!_stricmp(name, symbol_table[index].name))
			return(symbol_table[index].token);
		index++;
	}
	return(TOKEN_NONE);
}

//------------------------------------------------------------------------------
// Return the string associated with a given value in the given value definition
// list.
//------------------------------------------------------------------------------

static string
get_value_str_from_list(int value, value_def *value_def_list, int value_defs, 
						bool multiple_values = false)
{
	string value_str;
	for (int index = 0; index < value_defs; index++) {
		if (multiple_values) {
			if (value & value_def_list[index].value) {
				if (strlen(value_str) > 0) {
					value_str += ", ";
				}
				value_str += value_def_list[index].str;
			}
		} else if (value == value_def_list[index].value) {
			return(value_def_list[index].str);
		}
	}
	return(value_str);
}

//------------------------------------------------------------------------------
// Display an error or warning message for bad attribute values.
//------------------------------------------------------------------------------

static void
bad_attribute_value(const char *pre_message, const char *post_message)
{
	int value_string_size;
	char message[ERROR_MSG_SIZE];
	
	value_string_size = last_value_string_ptr - value_string;
	if (value_string_size > 0)
		bprintf(message, ERROR_MSG_SIZE, "Following <I>\"%.*s\"</I>, expected %s in the <I>%s</I> attribute of the <I>%s</I> tag%s", 
			value_string_size, value_string, pre_message, get_name(curr_attr_token), get_name(curr_tag_token), post_message);
	else
		bprintf(message, ERROR_MSG_SIZE, "Expected %s in the <I>%s</I> attribute of the <I>%s</I> tag%s", pre_message, 
			get_name(curr_attr_token), get_name(curr_tag_token), post_message);

	// Display it as an error or warning.

	if (curr_attr_required)
		error(message);
	else
		warning("%s; using default value for this attribute instead", message);
}

//------------------------------------------------------------------------------
// Verify that the given integer is within the specified range; if not,
// generate a bad attribute value error.
//------------------------------------------------------------------------------

bool
check_int_range(int value, int min, int max)
{
	char message[ERROR_MSG_SIZE];

	if (value >= min && value <= max)
		return(true);
	bprintf(message, ERROR_MSG_SIZE, "a whole number between %d and %d", min, max);
	bad_attribute_value(message, "");
	return(false);
}

//------------------------------------------------------------------------------
// Verify that the given integer is greater than a given value; if not,
// generate a bad attribute value error.
//------------------------------------------------------------------------------

bool
check_int_greater_than(int value, int min)
{
	char message[ERROR_MSG_SIZE];

	if (value > min)
		return(true);
	bprintf(message, ERROR_MSG_SIZE, "a whole number > %d", min);
	bad_attribute_value(message, "");
	return(false);
}

//------------------------------------------------------------------------------
// Verify that the given integer is less than or equal to a given value; if not,
// generate a bad attribute value error.
//------------------------------------------------------------------------------

bool
check_int_less_or_equal(int value, int max)
{
	char message[ERROR_MSG_SIZE];

	if (value <= max)
		return(true);
	bprintf(message, ERROR_MSG_SIZE, "a whole number <= %d", max);
	bad_attribute_value(message, "");
	return(false);
}

//------------------------------------------------------------------------------
// Verify that the given float is within the specified range; if not,
// generate a bad attribute value error.
//------------------------------------------------------------------------------

bool
check_float_range(float value, float min, float max)
{
	char message[ERROR_MSG_SIZE];

	if (FGE(value, min) && FLE(value, max))
		return(true);
	bprintf(message, ERROR_MSG_SIZE, "a floating point number between %g and %g", min, max);
	bad_attribute_value(message, "");
	return(false);
}

//------------------------------------------------------------------------------
// Verify that the given float is greater than a given value; if 
// not, generate a bad attribute value error.
//------------------------------------------------------------------------------

bool
check_float_greater_than(float value, float min)
{
	char message[ERROR_MSG_SIZE];

	if (FGT(value, min))
		return(true);
	bprintf(message, ERROR_MSG_SIZE, "a floating point number > %g", min);
	bad_attribute_value(message, "");
	return(false);
}

//------------------------------------------------------------------------------
// Verify that the given float is greater than or equal to a given value; if 
// not, generate a bad attribute value error.
//------------------------------------------------------------------------------

bool
check_float_greater_or_equal(float value, float min)
{
	char message[ERROR_MSG_SIZE];

	if (FGE(value, min))
		return(true);
	bprintf(message, ERROR_MSG_SIZE, "a floating point number >= %g", min);
	bad_attribute_value(message, "");
	return(false);
}

//==============================================================================
// General parameter value parsing functions.
//==============================================================================

//------------------------------------------------------------------------------
// Parse a name, a wildcard, or a list of names seperated by commas.  
// Only letters, digits and underscores are allowed in a name.  Leading and
// trailing spaces will be removed.
//------------------------------------------------------------------------------

bool
parse_name(const char *old_name, string *new_name, bool allow_list, bool allow_wildcard)
{
	const char *old_name_ptr;
	char *temp_name;
	char *temp_name_ptr;

	// Allocate a temporary name string the same size as the old name string.

	NEWARRAY(temp_name, char, strlen(old_name) + 1);
	if (temp_name == NULL)
		return(false);

	// Initialise the name pointers.

	old_name_ptr = old_name;
	temp_name_ptr = temp_name;

	// Parse one part name at a time...

	do {
		// Skip over the leading spaces.  If we reach the end of the string,
		// this is an error.

		while (*old_name_ptr == ' ')
			old_name_ptr++;
		if (!*old_name_ptr) {
			DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
			return(false);
		}

		// If the first character is a wildcard...

		if (*old_name_ptr == '*') {

			// If we're not allowing a wildcard, or this is not the first
			// name parsed, this is an error.

			if (!allow_wildcard || temp_name_ptr != temp_name) {
				DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
				return(false);
			}

			// Copy the wildcard to the temporary string, and skip over
			// trailing spaces.  Any additional characters after this is an
			// error.

			*temp_name_ptr++ = *old_name_ptr++;
			while (*old_name_ptr == ' ')
				old_name_ptr++;
			if (*old_name_ptr) {
				DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
				return(false);
			}

			// Terminate the temporary string, copy it to the new name string,
			// then delete the temporary string before returning success.

			*temp_name_ptr = '\0';
			*new_name = temp_name;
			DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
			return(true);
		}

		// Step through the sequence of letters, digits and underscores, 
		// copying them to the temporary string.

		while (*old_name_ptr == '_' || isalnum(*old_name_ptr))
			*temp_name_ptr++ = *old_name_ptr++;

		// Skip over any trailing spaces.

		while (*old_name_ptr == ' ')
			old_name_ptr++;

		// If the next character is not a string terminator or comma, this is
		// an error.  Otherwise append the string terminator or comma to the
		// temporary string.

		if (*old_name_ptr != '\0' && *old_name_ptr != ',') {
			DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
			return(false);
		}
		*temp_name_ptr = *old_name_ptr;

		// If the last character was a string terminator, we're done so copy
		// the temporary string to the new name string, delete the temporary
		// string and return success.

		if (*old_name_ptr == '\0') {
			*new_name = temp_name;
			DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
			return(true);
		}

		// If a list was not requested, a comma is an error.

		if (!allow_list) {
			DELBASEARRAY(temp_name, char, strlen(old_name) + 1);
			return(false);
		}

		// Otherwise skip over the comma and parse the next name.

		old_name_ptr++;
		temp_name_ptr++;
	} while (true);
}

//------------------------------------------------------------------------------
// Begin the parsing of the given attribute value.
//------------------------------------------------------------------------------

void
start_parsing_value(int tag_token, int attr_token, char *attr_value, bool attr_required)
{
	// Remember the tag token as the current one.

	curr_tag_token = tag_token;

	// Remember the parameter token as the current one.

	curr_attr_token = attr_token;
	curr_attr_required = attr_required;

	// Skip over leading white space.

	value_string_ptr = attr_value;
	while (*value_string_ptr == ' ' || *value_string_ptr == '\t')
		value_string_ptr++;

	// Set the start of the value string to this.

	value_string = value_string_ptr;
	last_value_string_ptr = value_string_ptr;
}

//------------------------------------------------------------------------------
// If a token matches in the parameter value string, skip over it and return
// TRUE.  If generate_error is TRUE, then a bad attribute value message is
// also generated.
//------------------------------------------------------------------------------

bool
token_in_value_is(const char *token_string, bool generate_error)
{
	char message[ERROR_MSG_SIZE];

	// Skip over leading white space.

	while (*value_string_ptr == ' ' || *value_string_ptr == '\t')
		value_string_ptr++;
	last_value_string_ptr = value_string_ptr;

	// If we've reached the end of the parameter value string or the token
	// doesn't match the parameter value string, return false.

	if (*value_string_ptr == '\0' ||
		_strnicmp(value_string_ptr, token_string, strlen(token_string))) {
		if (generate_error) {
			bprintf(message, ERROR_MSG_SIZE, "<I>\"%s\"</I>", token_string);
			bad_attribute_value(message, "");
		}
		return(false);
	}
	
	// Otherwise skip over the matched token string and return true.

	value_string_ptr += strlen(token_string);
	return(true);
}

//------------------------------------------------------------------------------
// If the next non-white space in the current value string is alphanumeric, 
// return it and skip over it.
//------------------------------------------------------------------------------

bool
alphanumeric_in_value(int *ch_ptr, bool generate_error)
{
	char next_ch;

	// Skip over leading white space.

	while (*value_string_ptr == ' ' || *value_string_ptr == '\t')
		value_string_ptr++;
	last_value_string_ptr = value_string_ptr;

	// Convert the next character to upper case and remember it.  Also
	// remember the next character, if there is one.

	*ch_ptr = toupper(*value_string_ptr);
	if (*ch_ptr != '\0')
		next_ch = *(value_string_ptr + 1);
	else
		next_ch = '\0';

	// If we've reached the end of the parameter value string, the next
	// character is not alphanumeric, or the character after the alphanumeric
	// one is not white space, a comma or the end of the value string, return
	// false.

	if (*ch_ptr == '\0' || !isalnum(*ch_ptr) || (next_ch != '\0' &&
		next_ch != ' ' && next_ch != '\t' && next_ch != ',')) {
		if (generate_error)
			bad_attribute_value("an alphanumeric character", "");
		return(false);
	}
	
	// Skip over the alphanumeric character and return true.

	*ch_ptr = toupper(*value_string_ptr);
	value_string_ptr++;
	return(true);
}

//------------------------------------------------------------------------------
// Parse an integer in the current parameter value string.
//------------------------------------------------------------------------------

bool
parse_integer_in_value(int *int_ptr)
{
	char *last_ch_ptr;

	// Skip over leading white space.

	while (*value_string_ptr == ' ' || *value_string_ptr == '\t')
		value_string_ptr++;
	last_value_string_ptr = value_string_ptr;

	// Attempt to parse the integer; if no characters were parsed, this means
	// there was no valid integer in the parameter value string so return false.

	*int_ptr = strtol(value_string_ptr, &last_ch_ptr, 10);
	if (last_ch_ptr == value_string_ptr) {
		bad_attribute_value("a whole number", ""); 
		return(false);
	}

	// Otherwise skip over matched integer in parameter value string and return
	// true.

	value_string_ptr = last_ch_ptr;
	return(true);
}

//------------------------------------------------------------------------------
// Parse the current file token as an integer range.
//------------------------------------------------------------------------------

static bool
parse_integer_range(intrange *intrange_ptr)
{
	if (!parse_integer_in_value(&intrange_ptr->min))
		return(false);
	if (token_in_value_is("..", false)) {
		if (!parse_integer_in_value(&intrange_ptr->max))
			return(false);
	} else
		intrange_ptr->max = intrange_ptr->min;
	return(true);
}

//------------------------------------------------------------------------------
// Parse an integer in the current parameter value string; a signed or zero
// integer is considered to be relative.  An unsigned integer must not exceed
// the maximum value given.
//------------------------------------------------------------------------------

static bool
parse_relative_integer_in_value(int *int_ptr, bool *sign_ptr, int max_int)
{
	// If a "+" or "-" sign precedes the integer, set the sign flag and back
	// up one character.

	*sign_ptr = false;
	if (token_in_value_is("+", false) || token_in_value_is("-", false)) {
		*sign_ptr = true;
		value_string_ptr--;
	}

	// If the integer cannot be parsed, this is an error.

	if (!parse_integer_in_value(int_ptr))
		return(false);

	// If the integer parsed was zero, set the sign flag.

	if (*int_ptr == 0) {
		*sign_ptr = true;
	}

	// If there was no sign, compare the integer against the maximum value.

	if (!*sign_ptr && max_int > 0) {
		if (!check_int_less_or_equal(*int_ptr, max_int))
			return(false);
	}
	return(true);
}

//------------------------------------------------------------------------------
// Parse a float in the current parameter value string.
//------------------------------------------------------------------------------

bool
parse_float_in_value(float *float_ptr)
{
	char *last_ch_ptr;

	// Skip over leading white space.

	while (*value_string_ptr == ' ' || *value_string_ptr == '\t')
		value_string_ptr++;
	last_value_string_ptr = value_string_ptr;

	// Attempt to parse the float; if no characters were parsed, this means
	// there was no valid float in the parameter value string, so return false.

	*float_ptr = (float)strtod(value_string_ptr, &last_ch_ptr);
	if (last_ch_ptr == value_string_ptr) {
		bad_attribute_value("a floating point number", ""); 
		return(false);
	}

	// If the last character passed was a decimal point, and there is a second
	// decimal point immediately following, then back up one character; this
	// allows the ".." token to follow a float.

	if (*(last_ch_ptr - 1) == '.' && *last_ch_ptr == '.')
		last_ch_ptr--;

	// Otherwise skip over matched float in parameter value string and return
	// true.

	value_string_ptr = last_ch_ptr;
	return(true);
}

//------------------------------------------------------------------------------
// Parse a name in the current parameter value string.
//------------------------------------------------------------------------------

static bool
parse_name_in_value(string *string_ptr, bool allow_list, bool allow_wildcard)
{
	const char *end_msg = " (a valid name begins with an alphanumeric character, and may only contain alphanumeric and underscore characters)";
	if (!parse_name(value_string_ptr, string_ptr, allow_list, allow_wildcard)) {
		if (allow_list) {
			if (allow_wildcard) {
				bad_attribute_value("either a list of one or more valid names seperated by commas, or the wildcard <I>\"*\"</I>", end_msg);
			} else {
				bad_attribute_value("a list of one or more valid names seperated by commas", end_msg);
			}
		} else if (allow_wildcard) {
			bad_attribute_value("either a valid name or the wildcard <I>\"*\"</I>", end_msg);
		} else {
			bad_attribute_value("a valid name", end_msg);
		}
		return(false);
	}
	value_string_ptr += strlen(value_string_ptr);
	return(true);
}

//------------------------------------------------------------------------------
// End parsing of the current parameter value string.
//------------------------------------------------------------------------------

bool
stop_parsing_value(bool generate_error)
{
	// Skip over trailing white space in the parameter value string.

	while (*value_string_ptr == ' ' || *value_string_ptr == '\t')
		value_string_ptr++;
	last_value_string_ptr = value_string_ptr;

	// If we haven't reached the end of the parameter value string, this is an
	// error.

	if (*value_string_ptr != '\0') {
		if (generate_error)
			bad_attribute_value("nothing", "");
		return(false);
	}
	return(true);
}

//------------------------------------------------------------------------------
// Match the current file token with one string value in the given list,
// ORing the equivilant integer value with any current integer values.
//------------------------------------------------------------------------------

static bool
parse_one_value(int *value_ptr, value_def *value_def_list, int value_defs)
{
	for (int index = 0; index < value_defs; index++)
		if (token_in_value_is(value_def_list[index].str, false)) {
			*value_ptr |= value_def_list[index].value;
			return(true);
		}
	return(false);
}

//------------------------------------------------------------------------------
// Match the current file token with a list of string values, returning the
// equivilant integer value.  On failure, display a warning or error.
//------------------------------------------------------------------------------

static bool
parse_value(int *value_ptr, value_def *value_def_list, int value_defs,
			bool multiple_values, bool generate_error)
{
	char pre_message[ERROR_MSG_SIZE];
	char value[ERROR_MSG_SIZE];
	char post_message[ERROR_MSG_SIZE];

	// Clear the integer value.

	*value_ptr = 0;

	// If multiple values are permitted, keep parsing values until none are
	// left.

	if (multiple_values) {
		bool succeeded = true;
		do
			if (!parse_one_value(value_ptr, value_def_list, value_defs)) {
				succeeded = false;
				break;
			}
		while (token_in_value_is(",", false));
		if (succeeded)
			return(true);
	}

	// If multiple values are not permitted, step through the list of value
	// definitions, and if a match is found return it's value.

	else if (parse_one_value(value_ptr, value_def_list, value_defs))
		return(true);

	// If an error should be generated...

	if (generate_error) {

		// Construct the pre-message text.

		strcpy(pre_message, "one ");
		if (multiple_values)
			strcat(pre_message, "or more ");
		strcat(pre_message, "of the following values");
		if (multiple_values)
			strcat(pre_message, ", seperated by commas,");

		// Construct the post-message text.

		bprintf(post_message, ERROR_MSG_SIZE, ": <I>\"%s\"</I>", 
			value_def_list[0].str);
		for (int index = 1; index < value_defs - 1; index++) {
			bprintf(value, ERROR_MSG_SIZE, ", <I>\"%s\"</I>", 
				value_def_list[index].str);
			strcat(post_message, value);
		}
		if (multiple_values)
			strcat(post_message, " and/");
		strcat(post_message, "or ");
		bprintf(value, ERROR_MSG_SIZE, "<I>\"%s\"</I>", 
			value_def_list[value_defs - 1].str);
		strcat(post_message, value);

		// Display the message as an error (which will throw an exception) or
		// as a warning.

		bad_attribute_value(pre_message, post_message);
	}

	// Indicate the parsing failed.

	return(false);
}

//==============================================================================
// Specialised parameter value parsing functions.
//==============================================================================

//------------------------------------------------------------------------------
// Parse current file token as a block reference (either a symbol or location).
//------------------------------------------------------------------------------

static bool parse_relative_coordinates(relcoords *relcoords_ptr);
static bool parse_symbol(word *symbol_ptr, bool disallow_empty_block_symbol, bool generate_error);

static bool
parse_block_reference(blockref *block_ref_ptr)
{
	if (parse_symbol(&block_ref_ptr->symbol, false, false)) {
		block_ref_ptr->is_symbol = true;
		return(true);
	}
	if (parse_relative_coordinates(&block_ref_ptr->location)) {
		block_ref_ptr->is_symbol = false;
		return(true);
	}
	return(false);
}

//------------------------------------------------------------------------------
// Parse the current file token as a boolean "yes" or "no".
//------------------------------------------------------------------------------

static bool
parse_boolean(bool *bool_ptr)
{
	int value;

	if (parse_value(&value, boolean_value_list, BOOLEAN_VALUES, false, true)) {
		*bool_ptr = (value == 1 ? true : false);
		return(true);
	}
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as an angle in degrees, and adjust it so that it's
// in a positive range.
//------------------------------------------------------------------------------

static bool
parse_degrees(float *float_ptr)
{
	if (!parse_float_in_value(float_ptr))
		return(false);
	*float_ptr = pos_adjust_angle(*float_ptr);
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as a range of delays, in seconds.
//------------------------------------------------------------------------------

static bool
parse_delay_range(delayrange *delay_range_ptr)
{
	float min_delay, max_delay;

	if (!parse_float_in_value(&min_delay) || !check_float_greater_or_equal(min_delay, 0.0f))
		return(false);
	delay_range_ptr->min_delay_ms = (int)(min_delay * 1000.0f);
	if (token_in_value_is("..", false)) {
		if (!parse_float_in_value(&max_delay) || !check_float_greater_or_equal(max_delay, min_delay))
			return(false);
		delay_range_ptr->delay_range_ms = (int)(max_delay * 1000.0f) - delay_range_ptr->min_delay_ms;
	} else
		delay_range_ptr->delay_range_ms = 0;
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as a direction (rotations around the Y and X axes,
// in that order).
//------------------------------------------------------------------------------

static bool
parse_direction(direction *direction_ptr)
{
	if (!token_in_value_is("(", true) ||
		!parse_float_in_value(&direction_ptr->angle_y) ||
		!token_in_value_is(",", true) ||
		!parse_float_in_value(&direction_ptr->angle_x) || 
		!token_in_value_is(")", true))
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as a range of directions.
//------------------------------------------------------------------------------

static bool
parse_direction_range(dirrange *dirrange_ptr)
{
	if (!parse_direction(&dirrange_ptr->min_direction))
		return(false);
	if (token_in_value_is("..", false)) {
		if (!parse_direction(&dirrange_ptr->max_direction))
			return(false);
	} else {
		dirrange_ptr->max_direction.angle_y = dirrange_ptr->min_direction.angle_y;
		dirrange_ptr->max_direction.angle_x = dirrange_ptr->min_direction.angle_x;
	}
	return(true);
}

//------------------------------------------------------------------------------
// Parse the current file token as a double character symbol.
//------------------------------------------------------------------------------

static bool
parse_double_symbol(word *symbol_ptr)
{
	if (string_to_double_symbol(value_string_ptr, symbol_ptr, true)) {
		value_string_ptr += 2;
		return(true);
	}
	bad_attribute_value("a valid double-character symbol", "");
	return(false);
}

//------------------------------------------------------------------------------
// Parse current file token as a direction (rotations around the Y and X axes,
// in that order), but the second angle is optional.
//------------------------------------------------------------------------------

static bool
parse_heading(direction *direction_ptr)
{
	if (!parse_float_in_value(&direction_ptr->angle_y))
		return(false);
	if (token_in_value_is(",", false)) {
		if (!parse_float_in_value(&direction_ptr->angle_x))
			return(false);
	} else
		direction_ptr->angle_x = 0.0f;
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as a key code.
//------------------------------------------------------------------------------

static bool
parse_key_code(int *key_code_ptr, bool generate_error)
{
	// Parse the key code, which is either a special key name in square
	// brackets, or an alphanumeric character.

	if (token_in_value_is("[", false)) {
		if (!parse_value(key_code_ptr, key_code_value_list, 
			KEY_CODE_VALUES, false, generate_error) || 
			!token_in_value_is("]", generate_error))
			return(false);
	} else if (!alphanumeric_in_value(key_code_ptr, generate_error))
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as map coordinates, and convert to zero based
// coordinates, taking the existence of the ground level and empty top level
// into account.
//------------------------------------------------------------------------------

static bool
parse_map_coordinates(mapcoords *mapcoords_ptr)
{
	int levels;

	// The number of available levels depends on whether the ground level
	// exists, and excludes the empty top level.

	if (world_ptr->ground_level_exists)
		levels = world_ptr->levels - 2;
	else
		levels = world_ptr->levels - 1;

	// Parse the map coordinates.

	if (!token_in_value_is("(", true) || 
		!parse_integer_in_value(&mapcoords_ptr->column) ||
		!check_int_range(mapcoords_ptr->column, 1, world_ptr->columns) ||
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&mapcoords_ptr->row) ||
		!check_int_range(mapcoords_ptr->row, 1, world_ptr->rows) ||
		!token_in_value_is(",", true) ||
		!parse_integer_in_value(&mapcoords_ptr->level) ||
		!check_int_range(mapcoords_ptr->level, 1, levels) ||
		!token_in_value_is(")", true))
		return(false);
	
	// Convert the coordinates to zero based numbers, taking the ground level
	// into account.

	mapcoords_ptr->column--;
	mapcoords_ptr->row--;
	if (!world_ptr->ground_level_exists)
		mapcoords_ptr->level--;
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as map dimensions.
//------------------------------------------------------------------------------

static bool
parse_map_dimensions(mapcoords *mapdims_ptr)
{
	return(token_in_value_is("(", true) &&
		parse_integer_in_value(&mapdims_ptr->column) && 
		check_int_greater_than(mapdims_ptr->column, 0) && 
		token_in_value_is(",", true) &&
		parse_integer_in_value(&mapdims_ptr->row) && 
		check_int_greater_than(mapdims_ptr->row, 0) &&
		token_in_value_is(",", true) && 
		parse_integer_in_value(&mapdims_ptr->level) &&
		check_int_greater_than(mapdims_ptr->level, 0) &&
		token_in_value_is(")", true));
}

//------------------------------------------------------------------------------
// Parse current file token as an orientation (rotations around the Y, X and Z
// axes, in that order).
//------------------------------------------------------------------------------

static bool
parse_orientation(orientation *orientation_ptr)
{
	// If the parameter value starts with a compass direction, this is the new
	// style of orientation.

	if (!parse_value(&orientation_ptr->direction, orientation_value_list, ORIENTATION_VALUES, false, false))
		orientation_ptr->direction = NONE;

	// If a compass direction was given, parse an angle of rotation around
	// the vector pointing in that direction.

	if (orientation_ptr->direction != NONE) {
		if (token_in_value_is(",", false)) {
			if (!parse_float_in_value(&orientation_ptr->angle))
				return(false);
			orientation_ptr->angle = pos_adjust_angle(orientation_ptr->angle);
		} else
			orientation_ptr->angle = 0.0f;
	}

	// Otherwise parse three angles of rotation around the Y, X and Z axes,
	// in that order.

	else {
		if (!parse_float_in_value(&orientation_ptr->angle_y))
			return(false);
		orientation_ptr->angle_y = pos_adjust_angle(orientation_ptr->angle_y);
		if (token_in_value_is(",", false)) {
			if (!parse_float_in_value(&orientation_ptr->angle_x))
				return(false);
			orientation_ptr->angle_x = pos_adjust_angle(orientation_ptr->angle_x);
			if (token_in_value_is(",", false)) {
				if (!parse_float_in_value(&orientation_ptr->angle_z))
					return(false);
				orientation_ptr->angle_z = pos_adjust_angle(orientation_ptr->angle_z);
			} else
				orientation_ptr->angle_z = 0.0f;
		} else {
			orientation_ptr->angle_x = 0.0f;
			orientation_ptr->angle_z = 0.0f;
		}
	}

	// Return success status.

	return(true);
}

//------------------------------------------------------------------------------
// Parse the current file token as a floating point percentage, then convert it
// into a value between 0.0 and 1.0.
//------------------------------------------------------------------------------

static bool
parse_percentage(float *percentage_ptr, float *min_percentage_ptr)
{
	if (!parse_float_in_value(percentage_ptr) ||
		!check_float_range(*percentage_ptr, 0.0, 100.0) ||
		(min_percentage_ptr != NULL && 
		 !check_float_greater_or_equal(*percentage_ptr, 
		 *min_percentage_ptr * 100.0f)) ||
		!token_in_value_is("%", true))
		return(false);
	*percentage_ptr /= 100.0f;
	return(true);
}

//------------------------------------------------------------------------------
// Parse the current file token as a floating point percentage range.
//------------------------------------------------------------------------------

static bool
parse_percentage_range(pcrange *pcrange_ptr)
{
	if (!parse_percentage(&pcrange_ptr->min_percentage, NULL))
		return(false);
	if (token_in_value_is("..", false)) {
		if (!parse_percentage(&pcrange_ptr->max_percentage, &pcrange_ptr->min_percentage))
			return(false);
	} else
		pcrange_ptr->max_percentage = pcrange_ptr->min_percentage;
	return(true);
}

//------------------------------------------------------------------------------
// Parse the given attribute value as a playback mode.
//------------------------------------------------------------------------------

bool
parse_playback_mode(char *attr_value, int *playback_mode_ptr)
{
	start_parsing_value(TOKEN_NONE, TOKEN_NONE, attr_value, false);
	return(parse_value(playback_mode_ptr, playback_mode_value_list, PLAYBACK_MODE_VALUES, false, false) && stop_parsing_value(false));
}

//------------------------------------------------------------------------------
// Parse the current file token as a radius in units of blocks, then convert it
// into world units.
//------------------------------------------------------------------------------

static bool
parse_radius(float *radius_ptr)
{
	if (!parse_float_in_value(radius_ptr) || !check_float_greater_than(*radius_ptr, 0.0f))
		return(false);
	*radius_ptr *= units_per_block;
	return(true);
}

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Parse the current file token as a rectangle in units of pixels or
// percentages.
//------------------------------------------------------------------------------

static bool
parse_rectangle(video_rect *rect_ptr)
{
	// Parse x1.

	if (!parse_float_in_value(&rect_ptr->x1))
		return(false);
	if (token_in_value_is("%", false)) {
		rect_ptr->x1_is_ratio = true;
		rect_ptr->x1 /= 100.0f;
	} else
		rect_ptr->x1_is_ratio = false;

	// Parse y1.

	if (!token_in_value_is(",", true) || !parse_float_in_value(&rect_ptr->y1))
		return(false);
	if (token_in_value_is("%", false)) {
		rect_ptr->y1_is_ratio = true;
		rect_ptr->y1 /= 100.0f;
	} else
		rect_ptr->x1_is_ratio = false;

	// Parse x2.

	if (!token_in_value_is(",", true) || !parse_float_in_value(&rect_ptr->x2))
		return(false);
	if (token_in_value_is("%", false)) {
		rect_ptr->x2_is_ratio = true;
		rect_ptr->x2 /= 100.0f;
	} else
		rect_ptr->x2_is_ratio = false;

	// Parse y2.

	if (!token_in_value_is(",", true) || !parse_float_in_value(&rect_ptr->y2))
		return(false);
	if (token_in_value_is("%", false)) {
		rect_ptr->y2_is_ratio = true;
		rect_ptr->y2 /= 100.0f;
	} else
		rect_ptr->y2_is_ratio = false;

	// Return success status.

	return(true);
}

#endif

//------------------------------------------------------------------------------
// Parse current file token as absolute or relative integers.
//------------------------------------------------------------------------------

static bool
parse_relative_integer_triplet(relinteger_triplet *relinteger_ptr)
{
	// Parse the integers.

	if (!token_in_value_is("(", true) || 
		!parse_relative_integer_in_value(&relinteger_ptr->x,
		 &relinteger_ptr->relative_x, 0) ||
		!token_in_value_is(",", true) || 
		!parse_relative_integer_in_value(&relinteger_ptr->y,
		 &relinteger_ptr->relative_y, 0) ||
		!token_in_value_is(",", true) || 
		!parse_relative_integer_in_value(&relinteger_ptr->z,
		 &relinteger_ptr->relative_z, 0) ||
		!token_in_value_is(")", true))
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as absolute or relative map coordinates.
//------------------------------------------------------------------------------

static bool
parse_relative_coordinates(relcoords *relcoords_ptr)
{
	int levels;

	// The number of levels available depends on the existence of the ground
	// level, and omits the top empty level.

	if (world_ptr->ground_level_exists)
		levels = world_ptr->levels - 2;
	else
		levels = world_ptr->levels - 1;

	// Parse the coordinates.

	if (!token_in_value_is("(", true) || 
		!parse_relative_integer_in_value(&relcoords_ptr->column,
		 &relcoords_ptr->relative_column, world_ptr->columns) ||
		!token_in_value_is(",", true) || 
		!parse_relative_integer_in_value(&relcoords_ptr->row,
		 &relcoords_ptr->relative_row, world_ptr->rows) ||
		!token_in_value_is(",", true) || 
		!parse_relative_integer_in_value(&relcoords_ptr->level,
		 &relcoords_ptr->relative_level, levels) ||
		!token_in_value_is(")", true))
		return(false);

	// Adjust the level if the ground level exists and it isn't relative.

	if (world_ptr->ground_level_exists && !relcoords_ptr->relative_level)
		relcoords_ptr->level++;
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as absolute or relative integer.
//------------------------------------------------------------------------------

static bool
parse_relative_integer(relinteger *int_ptr)
{
	// If a "+" or "-" sign precedes the integer, set the sign flag and back
	// up one character.

	int_ptr->relative_value = false;
	if (token_in_value_is("+", false) || token_in_value_is("-", false)) {
		int_ptr->relative_value = true;
		value_string_ptr--;
	}

	// If the integer cannot be parsed, this is an error.

	if (!parse_integer_in_value(&int_ptr->value))
		return(false);

	// If the integer parsed was zero, set the sign flag.

	if (int_ptr->value == 0) {
		int_ptr->relative_value = true;
	}

	// If there was no sign, 
	// decrease the integer by one to make it zero-based.

	if (!int_ptr->relative_value) {
		(int_ptr->value)--;
	}
	return(true);
}

//------------------------------------------------------------------------------
// Parse the current file token as an RGB colour triplet.
//------------------------------------------------------------------------------

static bool
parse_RGB(RGBcolour *colour_ptr)
{
	int red, green, blue;

	if (!token_in_value_is("(", true) || !parse_integer_in_value(&red) ||
		!check_int_range(red, 0, 255) || !token_in_value_is(",", true) ||
		!parse_integer_in_value(&green) || !check_int_range(green, 0, 255) ||
		!token_in_value_is(",", true) || !parse_integer_in_value(&blue) ||
		!check_int_range(blue, 0, 255) || !token_in_value_is(")", true))
		return(false);
	colour_ptr->red = (float)red;
	colour_ptr->green = (float)green;
	colour_ptr->blue = (float)blue;
	return(true);
}

//--------------------------------------------------------------------------
// Parse scale values as an x,y,z multiple
//--------------------------------------------------------------------------

static bool
parse_scale(float_triplet *scale_entry_ptr)
{
	if (!token_in_value_is("(", true) || 
		!parse_float_in_value(&scale_entry_ptr->x) ||
		!token_in_value_is(",", true) || 
		!parse_float_in_value(&scale_entry_ptr->y) ||
		!token_in_value_is(",", true) || 
		!parse_float_in_value(&scale_entry_ptr->z) ||
		!token_in_value_is(")", true))
		return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Parse the current file token as a single character symbol.
//------------------------------------------------------------------------------

static bool
parse_single_symbol(char *symbol_ptr)
{
	char symbol;

	if (string_to_single_symbol(value_string_ptr, &symbol, true)) {
		*symbol_ptr = symbol;
		value_string_ptr++;
		return(true);
	}
	bad_attribute_value("a valid single-character symbol", "");
	return(false);
}

//------------------------------------------------------------------------------
// Parse the current file token as a size.
//------------------------------------------------------------------------------

static bool
parse_size(size *size_ptr)
{
	return(token_in_value_is("(", true) && 
		parse_integer_in_value(&size_ptr->width) &&
		check_int_greater_than(size_ptr->width, 0) && 
		token_in_value_is(",", true) &&
		parse_integer_in_value(&size_ptr->height) && 
		check_int_greater_than(size_ptr->height, 0) &&
		token_in_value_is(")", true));
}

//------------------------------------------------------------------------------
// Parse the current file token as a size.
//------------------------------------------------------------------------------

static bool
parse_sprite_size(size *size_ptr)
{
	return(token_in_value_is("(", true) && 
		parse_integer_in_value(&size_ptr->width) &&
		check_int_range(size_ptr->width, 1, 256) && 
		token_in_value_is(",", true) &&
		parse_integer_in_value(&size_ptr->height) && 
		check_int_range(size_ptr->height, 1, 256) && 
		token_in_value_is(")", true));
}

//------------------------------------------------------------------------------
// Parse the current file token as a single or double character symbol,
// depending on what map style is active.
//------------------------------------------------------------------------------

static bool
parse_symbol(word *symbol_ptr, bool disallow_empty_block_symbol, bool generate_error)
{
	char symbol;

	switch (world_ptr->map_style) {
	case SINGLE_MAP:
		if (string_to_single_symbol(value_string_ptr, &symbol, disallow_empty_block_symbol)) {
			*symbol_ptr = symbol;
			value_string_ptr++;
			return(true);
		}
		if (generate_error)
			bad_attribute_value("a valid single-character symbol", "");
		break;
	case DOUBLE_MAP:
		if (string_to_double_symbol(value_string_ptr, symbol_ptr, disallow_empty_block_symbol)) {
			value_string_ptr += 2;
			return(true);
		}
		if (generate_error)
			bad_attribute_value("a valid double-character symbol", "");
	}
	return(false);
}

//------------------------------------------------------------------------------
// Parse current file token as a version string, and convert it into a unique
// integer.
//------------------------------------------------------------------------------

static bool
parse_version(unsigned int *version_ptr)
{
	int version1, version2, version3, version4;

	// Parse first two integers in version string, seperated by a period.  If
	// they are not in the range of 0-255, this is an error.

	if (!parse_integer_in_value(&version1) || 
		!check_int_range(version1, 0, 255) ||
		!token_in_value_is(".", true) || 
		!parse_integer_in_value(&version2) ||
		!check_int_range(version2, 0, 255))
		return(false);

	// If the next token is a period, parse the third integer, otherwise set it
	// to zero.  If the parsed value is not in the range of 0-255, this is an
	// error.

	if (token_in_value_is(".", false)) {
		if (!parse_integer_in_value(&version3) || !check_int_range(version3, 0, 255))
			return(false);
	} else
		version3 = 0;

	// If the next token is "b", parse the fourth integer, otherwise set it to
	// 255.  If the parsed value is not in the range of 1-254, this is an error.

	if (token_in_value_is("b", false)) {
		if (!parse_integer_in_value(&version4) || !check_int_range(version4, 1, 254))
			return(false);
	} else
		version4 = 255;

	// Construct a single version number from the four integers.

	*version_ptr = (version1 << 24) | (version2 << 16) | (version3 << 8) | 
		version4;
	return(true);
}

//------------------------------------------------------------------------------
// Parse current file token as vertex coordinates, and convert them into world
// units.
//------------------------------------------------------------------------------

static bool
parse_vertex_coordinates(vertex_entry *vertex_entry_ptr)
{
	if (!token_in_value_is("(", true) || 
		!parse_float_in_value(&vertex_entry_ptr->x) ||
		!token_in_value_is(",", true) || 
		!parse_float_in_value(&vertex_entry_ptr->y) ||
		!token_in_value_is(",", true) || 
		!parse_float_in_value(&vertex_entry_ptr->z) ||
		!token_in_value_is(")", true))
		return(false);
	vertex_entry_ptr->x /= texels_per_unit;
	vertex_entry_ptr->y /= texels_per_unit;
	vertex_entry_ptr->z /= texels_per_unit;
	return(true);
}

//==============================================================================
// General parsing functions.
//==============================================================================

//------------------------------------------------------------------------------
// Push a new element onto the parse stack of the top file, and return a
// pointer to it.
//------------------------------------------------------------------------------

parse_stack_element *
push_parse_stack(void)
{
	if (file_stack_ptr->parse_stack_depth == MAX_PARSE_STACK_DEPTH)
		error("Too many nested tags");
	return(file_stack_ptr->parse_stack_ptr = &file_stack_ptr->parse_stack[file_stack_ptr->parse_stack_depth++]);
}

//------------------------------------------------------------------------------
// Pop an old element from the parse stack of the top file, and return a
// pointer to it.
//------------------------------------------------------------------------------

parse_stack_element *
pop_parse_stack(void)
{
	file_stack_ptr->parse_stack_depth--;
	if (file_stack_ptr->parse_stack_depth > 0) {
		file_stack_ptr->parse_stack_ptr = &file_stack_ptr->parse_stack[file_stack_ptr->parse_stack_depth - 1];
	} else {
		file_stack_ptr->parse_stack_ptr = NULL;
	}
	return(file_stack_ptr->parse_stack_ptr);
}

//------------------------------------------------------------------------------
// Extract text from the given buffer, resolving all character and entity
// references, and normalising whitespace if requested.
//------------------------------------------------------------------------------

static void
extract_text(string *string_ptr, const char *buffer, int length, 
			 bool normalise_whitespace)
{
	char *new_buffer, *old_ch_ptr, *new_ch_ptr, *end_ch_ptr;
	string entity_name;
	entity_ref *entity_ref_ptr;
	int ch_value;

	// If the buffer is zero length, simply return an empty string.

	if (length == 0) {
		*string_ptr = "";
		return;
	}

	// Make a copy of the buffer so that we can modify it more easily.

	NEWARRAY(new_buffer, char, length + 1); 
	if (new_buffer == NULL)
		memory_error("new buffer");
	strncpy(new_buffer, buffer, length);
	new_buffer[length] = '\0';

	// Step through the given buffer, copying literal characters to the new
	// buffer, and replacing character and entity references with literal
	// characters in the new buffer.

	old_ch_ptr = new_buffer;
	new_ch_ptr = new_buffer;
	while (*old_ch_ptr != '\0') {

		// If the next character is not an ampersand, or strict_XML_compliance
		// is FALSE, it is a literal character that should be preserved. 
		// However, if normalise_whitespace is true, then '\n' and '\t'
		// characters are converted to spaces, and '\r' characters are removed.
		// Otherwise '\r\n' and '\r' are replaced with '\n'.

		if (*old_ch_ptr != '&' || !strict_XML_compliance) {		
			if (normalise_whitespace) {
				if (*old_ch_ptr == '\r' || *old_ch_ptr == '\n' || 
					*old_ch_ptr == '\t') {
					if (*old_ch_ptr != '\r')
						*new_ch_ptr++ = ' ';
					old_ch_ptr++;
				} else
					*new_ch_ptr++ = *old_ch_ptr++;
			} else {
				if (*old_ch_ptr == '\r') {
					*new_ch_ptr++ = '\n';
					if (*++old_ch_ptr == '\n')
						old_ch_ptr++;
				} else
					*new_ch_ptr++ = *old_ch_ptr++;
			}
		}

		// Otherwise it is a character or entity reference...

		else {

			// Extract the entity name.

			old_ch_ptr++;
			end_ch_ptr = old_ch_ptr;
			while (*end_ch_ptr != ';') {
				if (*end_ch_ptr == '\0') {
					DELARRAY(new_buffer, char, length + 1);
					error(file_token_line_no, "Character or entity reference is missing a closing semicolon");
				}
				end_ch_ptr++;
			}
			entity_name.copy(old_ch_ptr, end_ch_ptr - old_ch_ptr);
			old_ch_ptr = end_ch_ptr + 1;

			// If the entity name begins with a hash character, it is a
			// character reference.

			if (entity_name[0] == '#') {

				// If the next character is an 'x', parse the remainder of the
				// entity name as a hexdecimal value, otherwise parse it as
				// a decimal value.

				if (entity_name[1] == 'x')
					ch_value = strtol(&entity_name[2], &end_ch_ptr, 16);
				else
					ch_value = strtol(&entity_name[1], &end_ch_ptr, 10);

				// If there are additional characters after the parsed value,
				// or the value is out of range, then this is an error.

				if (*end_ch_ptr != '\0' || ch_value < 0x20 || ch_value > 0x7e) {
					DELARRAY(new_buffer, char, length + 1);
					error(file_token_line_no, "<I>&%s;</I> is not a valid character reference", entity_name);
				}

				// Store the literal character in the new buffer.

				*new_ch_ptr++ = (char)ch_value;
			} 
			
			// Otherwise, replace the entity reference with the corresponding
			// literal character.

			else {
				entity_ref_ptr = xml_entity_table;
				while (entity_ref_ptr->name != NULL) {
					if (!strcmp(entity_name, entity_ref_ptr->name)) {
						*new_ch_ptr++ = entity_ref_ptr->value;
						break;
					}
					entity_ref_ptr++;
				}
				if (entity_ref_ptr->name == NULL) {
					DELARRAY(new_buffer, char, length + 1);
					error(file_token_line_no, "<I>&%s;</I> is an undefined entity reference", entity_name);
				}
			}
		}
	}
	*new_ch_ptr = '\0';

	// Copy the new buffer to the target string, then delete the new buffer.

	*string_ptr = new_buffer;
	DELARRAY(new_buffer, char, length + 1);
}

//------------------------------------------------------------------------------
// Extract the next token from the file buffer.  Any parsing errors will cause
// an exception to be thrown.
//------------------------------------------------------------------------------

static void
extract_token(void)
{
	char *file_buffer_ptr;
	symbol_def *symbol_def_ptr;
	char ch, quote_ch;
	char *symbol_ptr, *ident_ptr, *end_string_ptr, *quote_ptr;
	int string_length;
	bool invalid_identifier;

	// If we are outside of a tag or comment, then we may want to return a 
	// character data token.

	file_buffer_ptr = file_stack_ptr->file_buffer_ptr;
	file_token_line_no = file_stack_ptr->line_no;
	if (!inside_tag) {

		// Look for an open bracket or the end of the file, signifying the end
		// of the character data.

		end_string_ptr = file_buffer_ptr;
		ch = *end_string_ptr;
		while (ch != '<' && ch != '\0') {
			if (ch == '\n')
				file_stack_ptr->line_no++;
			ch = *++end_string_ptr;
		}

		// Return the character data token, provided it isn't zero length.

		string_length = end_string_ptr - file_buffer_ptr;
		if (string_length > 0) {
			file_token = TOKEN_CHARACTER_DATA;
			extract_text(&file_token_string, file_buffer_ptr, string_length, false);
			file_stack_ptr->file_buffer_ptr = end_string_ptr;
			return;
		}
	}

	// Skip over leading white space: the first non-whitespace character will be
	// the start of the token.

	ch = *file_buffer_ptr;
	while (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
		if (ch == '\n')
			file_stack_ptr->line_no++;
		ch = *++file_buffer_ptr;
	}
	file_token_line_no = file_stack_ptr->line_no;

	// If we've come to the end of the file, return TOKEN_NONE.

	if (*file_buffer_ptr == '\0') {
		file_token = TOKEN_NONE;
		file_token_string = "end of file";
		file_stack_ptr->file_buffer_ptr = file_buffer_ptr;
		return;
	}

	// Find the first occurrance of an XML symbol or whitespace from the
	// current file buffer position.

	symbol_ptr = file_buffer_ptr;
	while (*symbol_ptr != '\0') {
		if (*symbol_ptr == '\n')
			file_stack_ptr->line_no++;
		symbol_def_ptr = xml_symbol_table;
		while (symbol_def_ptr->name != NULL) {
			if (!strncmp(symbol_ptr, symbol_def_ptr->name, strlen(symbol_def_ptr->name)))
				break;
			symbol_def_ptr++;
		}
		if (symbol_def_ptr->name != NULL)
			break;
		symbol_ptr++;
	}

	// If an XML symbol appears at the current buffer position...

	if (symbol_ptr == file_buffer_ptr)
		switch (symbol_def_ptr->token) {

		// If the symbol is a quotation mark, all characters up to a matching
		// quotation mark will be returned as a string value.  If the end of 
		// the line is reached before a matching quotation mark is found, this
		// is an error.

		case TOKEN_QUOTE:

			// Remember the quotation mark used.

			quote_ch = *file_buffer_ptr++;

			// Skip over leading white space in string.

			ch = *file_buffer_ptr;
			while (ch == ' ' || ch == '\t') {
				if (ch == '\n')
					file_stack_ptr->line_no++;
				ch = *++file_buffer_ptr;
			}

			// Now locate end of string.

			end_string_ptr = file_buffer_ptr;
			ch = *end_string_ptr;
			while (ch != '\0' && ch != quote_ch) {
				if (ch == '\n')
					file_stack_ptr->line_no++;
				ch = *++end_string_ptr;
			}
			if (ch != quote_ch)
				error(file_token_line_no, "String is missing closing quotation mark");
			quote_ptr = end_string_ptr;

			// Back up over any trailing white space in string.

			do
				ch = *--end_string_ptr;
			while (end_string_ptr >= file_buffer_ptr && (ch == ' ' || ch == '\t'));
			end_string_ptr++;

			// Return a string token consisting of the characters between the
			// quotation marks.

			file_token = TOKEN_STRING;
			string_length = end_string_ptr - file_buffer_ptr;
			extract_text(&file_token_string, file_buffer_ptr, string_length, true);
			file_buffer_ptr = quote_ptr + 1;
			break;

		// If the symbol is the start of a comment, all characters up to the
		// end of comment symbol will be returned as a comment string.  If the
		// end of the file is reached before the end of the comment is reached,
		// this is an error.

		case TOKEN_OPEN_COMMENT:

			// A comment cannot occur inside of another tag.

			if (inside_tag)
				error(file_token_line_no, "Comment tag cannot appear inside of another tag");

			// Locate the end of the comment.  Note that "--" by itself is an
			// illegal combination inside of a comment.

			file_buffer_ptr += 4;
			end_string_ptr = file_buffer_ptr;
			while (*end_string_ptr != '\0') {
				if (!strncmp(end_string_ptr, "-->", 3))
					break;
				if (!strncmp(end_string_ptr, "--", 2))
					error(file_token_line_no, "Comment tag cannot contain \"--\"");
				end_string_ptr++;
			}
			if (*end_string_ptr == '\0')
				error(file_token_line_no, "Comment is missing closing tag");

			// Return a comment token consisting of the characters between the
			// start and end comment symbols.

			file_token = TOKEN_COMMENT;
			string_length = end_string_ptr - file_buffer_ptr;
			extract_text(&file_token_string, file_buffer_ptr, string_length, false);
			file_buffer_ptr = end_string_ptr + 3;
			break;

		// Simply return any other symbol.

		default:
			file_token = symbol_def_ptr->token;
			file_token_string = symbol_def_ptr->name;
			file_buffer_ptr += strlen(symbol_def_ptr->name);
		}

	// Extract an identifier up to the next XML symbol, whitespace or end of
	// file, and verify that it's a legal identifier.

	else {
		file_token = TOKEN_IDENTIFIER;
		string_length = symbol_ptr - file_buffer_ptr;
		file_token_string.copy(file_buffer_ptr, string_length);
		file_buffer_ptr = symbol_ptr;
		ident_ptr = file_token_string;
		invalid_identifier = false;
		ch = *ident_ptr;
		if (!isalpha(ch) && ch != '_' && ch != ':')
			invalid_identifier = true;
		else {
			ch = *++ident_ptr;
			while (ch != '\0') {
				if (!isalnum(ch) && ch != '_' && ch != ':' && ch != '-' && ch != '.') {
					invalid_identifier = true;
					break;
				}
				ch = *++ident_ptr;
			}
		}
		if (invalid_identifier) 
			error(file_token_line_no, "<I>%s</I> was not expected here", file_token_string);
	}

	// Update the file buffer pointer in the top file.

	file_stack_ptr->file_buffer_ptr = file_buffer_ptr;
}

//------------------------------------------------------------------------------
// Read one token from the currently open file.  Any read, memory or parsing
// errors will cause an exception to be thrown.
//------------------------------------------------------------------------------

static int
read_token(void)
{
	// Extract the next token, and process it if necesary.

	extract_token();
	switch (file_token) {

	// If the token is "<" or "</", return it after setting a flag to
	// indicate we're inside of a tag.

	case TOKEN_OPEN_TAG:
	case TOKEN_OPEN_END_TAG:
		inside_tag = true;
		return(file_token);

	// If the token is ">" or "/>", return it after setting a flag to
	// indicate we're outside of a tag.

	case TOKEN_CLOSE_TAG:
	case TOKEN_CLOSE_SINGLE_TAG:
		inside_tag = false;
		return(file_token);

	// Any other token can simply be returned as-is.

	default:
		return(file_token);
	}
}

//------------------------------------------------------------------------------
// Create an attribute with the given name and value.
//------------------------------------------------------------------------------

static attr *
create_attr(string name, string value)
{
	attr *attr_ptr;

	NEW(attr_ptr, attr);
	if (attr_ptr == NULL)
		memory_error("attribute");
	attr_ptr->name = name;
	attr_ptr->value = value;
	attr_ptr->next_attr_ptr = NULL;
	return attr_ptr;
}

//------------------------------------------------------------------------------
// Parse an attribute, returning a pointer to it.  It is assumed that the
// attribute name identifier token has already been read.  Any error will cause
// an exception to be thrown.
//------------------------------------------------------------------------------

static attr *
parse_attribute(void)
{
	string attr_name;

	// Parse the attribute name identifier.
	
	if (file_token != TOKEN_IDENTIFIER)
		error(file_token_line_no, "Expected an attribute name rather than <I>%s</I>", file_token_string);
	attr_name = file_token_string;

	// Parse the equals sign symbol, followed by the attribute value string.

	if (read_token() != TOKEN_EQUALS_SIGN)
		error(file_token_line_no, "Expected <I>=</I> rather than <I>%s</I>", file_token_string);
	if (read_token() != TOKEN_STRING)
		error(file_token_line_no, "Expected an attribute value string rather than <I>%s</I>", file_token_string);

	// Create an attribute, and initialise it with the parsed token and value,
	// then return a pointer to it.

	return(create_attr(attr_name, file_token_string));
}

//------------------------------------------------------------------------------
// Create a tag entity with the given type, line number, text and attribute list.
//------------------------------------------------------------------------------

entity *
create_entity(int type, int line_no, string text, attr *attr_list)
{
	entity *entity_ptr;

	NEW(entity_ptr, entity);
	if (entity_ptr == NULL)
		memory_error("XML entity");
	entity_ptr->type = type;
	entity_ptr->line_no = line_no;
	entity_ptr->text = text;
	entity_ptr->attr_list = attr_list;
	entity_ptr->nested_entity_list = NULL;
	entity_ptr->next_entity_ptr = NULL;
	return entity_ptr;
}

//------------------------------------------------------------------------------
// Parse all tags up to the given end tag.  An error will cause an exception to
// be thrown.
//------------------------------------------------------------------------------

// Forward declaration.

static entity *parse_tag(bool *is_start_tag);

static entity *
parse_nested_entity_list(const char *end_tag_name)
{
	entity *entity_list, *last_entity_ptr;
	entity *entity_ptr;
	int token;
	bool is_start_tag;

	// Parse each entity in turn, until the named end tag has been reached, or the document start tag is found.

	entity_list = NULL;
	last_entity_ptr = NULL;
	while (true) {

		// Read the next token.

		token = read_token();

		// Refresh the player window.

		if (!refresh_player_window())
			error(file_token_line_no, "Parsing aborted");

		// Parse the token.

		switch (token) {

		// If the token is character data or a comment, create a new text or comment entity.

		case TOKEN_CHARACTER_DATA:
		case TOKEN_COMMENT:
			entity_ptr = create_entity(token == TOKEN_CHARACTER_DATA ? TEXT_ENTITY : COMMENT_ENTITY, file_token_line_no, file_token_string, NULL);
			break;
			
		// If the token is an open tag symbol, parse the tag and create a 
		// tag entity for it, then parse it's nested entity list if it were
		// a start tag.

		case TOKEN_OPEN_TAG:
			entity_ptr = parse_tag(&is_start_tag);
			if (is_start_tag)
				entity_ptr->nested_entity_list = parse_nested_entity_list(entity_ptr->text);
			break;

		// If the token is an open end tag symbol, the next token must be an 
		// identifier with the requested end token, and the token after
		// that must be a close tag symbol.  If this is the case, then we
		// return the entity list that we've parsed.

		case TOKEN_OPEN_END_TAG:
			if (read_token() != TOKEN_IDENTIFIER)
				error(file_token_line_no, "Expected a tag name rather than <I>%s</I>", file_token_string);
			if ((strict_XML_compliance && strcmp(file_token_string, end_tag_name)) ||
				(!strict_XML_compliance && _stricmp(file_token_string, end_tag_name)) || 
				read_token() != TOKEN_CLOSE_TAG)
				error(file_token_line_no, "Expected <I>&lt;/%s&gt;</I> rather than <I>&lt;/%s&gt;</I>", end_tag_name, file_token_string);
			return(entity_list);

		// If we've come to the end of the file, and there is no end token
		// name specified, then this is an error.

		case TOKEN_NONE:
			error(file_token_line_no, "Expected <I>&lt;/%s&gt;</I> rather than the end of the file", end_tag_name);
			break;

		// Any other token is invalid.

		default:
			error(file_token_line_no, "Expected a tag or text rather than <I>%s</I>", file_token_string);
		}

		// Add the entity to the end of the entity list.

		entity_ptr->next_entity_ptr = NULL;
		if (last_entity_ptr == NULL)
			entity_list = entity_ptr;
		else
			last_entity_ptr->next_entity_ptr = entity_ptr;
		last_entity_ptr = entity_ptr;
	}
}

//------------------------------------------------------------------------------
// Parse a tag, and return a pointer to a tag entity, as well as a flag to
// indicate whether it's a start tag or empty tag.  It is assumed the open
// tag symbol has already been parsed before this function was called.  An 
// error will cause an exception to be thrown.
//------------------------------------------------------------------------------

static entity *
parse_tag(bool *is_start_tag)
{
	string tag_name;
	attr *attr_list, *last_attr_ptr;
	attr *attr_ptr;
	int tag_line_no;

	// Read the tag name identifier, and remember the line it was on.

	if (read_token() != TOKEN_IDENTIFIER)
		error(file_token_line_no, "Expected a tag name rather than <I>%s</I>", file_token_string);
	tag_name = file_token_string;
	tag_line_no = file_token_line_no;

	// Parse the attribute list, if there is one.

	read_token();
	attr_list = NULL;
	last_attr_ptr = NULL;
	while (file_token != TOKEN_CLOSE_TAG && file_token != TOKEN_CLOSE_SINGLE_TAG) {
		attr_ptr = parse_attribute();
		if (last_attr_ptr) {
			last_attr_ptr->next_attr_ptr = attr_ptr;
		} else {
			attr_list = attr_ptr;
		}
		last_attr_ptr = attr_ptr;
		read_token();
	}
	if (file_token == TOKEN_CLOSE_TAG)
		*is_start_tag = true;
	else
		*is_start_tag = false;
	
	// Create the tag entity, and initialise it with the line number, tag name token and attribute list.

	return(create_entity(TAG_ENTITY, tag_line_no, tag_name, attr_list));
}

//------------------------------------------------------------------------------
// Determine if the given text does not contain all whitespace characters.
//------------------------------------------------------------------------------

static bool
not_all_whitespace(const char *text)
{
	char ch;

	while ((ch = *text++) != '\0')
		if (ch != ' ' && ch != '\t' && ch != '\n')
			return(true);
	return(false);
}

//------------------------------------------------------------------------------
// Parse a start tag, and return a tag entity for it.
//------------------------------------------------------------------------------

static entity *
parse_start_tag(int start_tag_token)
{
	entity *entity_ptr;
	int token, tag_token;
	bool is_start_tag;

	while (true) {

		// Read the next token.

		token = read_token();

		// Refresh the player window.

		if (!refresh_player_window())
			error(file_token_line_no, "Parsing aborted");

		// Read and parse the next token.

		switch (token) {

		// If the token is character data, skip over it after generating a
		// warning if the text is not all whitespace.

		case TOKEN_CHARACTER_DATA:
			if (not_all_whitespace(file_token_string))
				warning(file_token_line_no, "Text before <I>&lt;%s&gt;</I> is not permitted", get_name(start_tag_token));
			break;

		// If the token is a comment, skip over it.

		case TOKEN_COMMENT:
			break;

		// If the token is an open tag symbol, parse the start tag and it's
		// attributes, verify it's the expected start tag, and return it's
		// tag entity.

		case TOKEN_OPEN_TAG:
			entity_ptr = parse_tag(&is_start_tag);
			if (!is_start_tag || (tag_token = get_token(entity_ptr->text)) == TOKEN_NONE || tag_token != start_tag_token)
				error(entity_ptr->line_no, "Expected <I>&lt;%s&gt;</I> rather than <I>&lt;%s&gt;</I>", get_name(start_tag_token), entity_ptr->text);
			return(entity_ptr);

		// If we've come to the end of the file, this is an error.

		case TOKEN_NONE:
			error(file_token_line_no, "Did not see <I>&lt;%s&gt;</I>", get_name(start_tag_token));
			break;

		// Any other token is invalid.

		default:
			error(file_token_line_no, "Expected <I>&lt;%s&gt;</I> rather than <I>%s</I>", file_token_string);
		}
	}
}

//------------------------------------------------------------------------------
// Parse an attribute's value according to it's type.  Returns TRUE on success,
// or FALSE on failure if the attribute has a default value that can be used.
// Otherwise an exception is thrown.
//------------------------------------------------------------------------------

static bool
parse_attribute_value(int value_type, void *value_ptr)
{
	switch (value_type) {

	case VALUE_ACTION_TRIGGER:
		return(parse_value((int *)value_ptr, action_trigger_value_list, ACTION_TRIGGER_VALUES, false, true));

	case VALUE_ALIGNMENT:
		return(parse_value((int *)value_ptr, alignment_value_list, ALIGNMENT_VALUES - 1, false, true));

	case VALUE_ANGLES:
		return(parse_scale((float_triplet *)value_ptr));  

	case VALUE_BLOCK_REF:
		return(parse_block_reference((blockref *)value_ptr));

	case VALUE_BLOCK_TYPE:
		return(parse_value((int *)value_ptr, block_type_value_list, BLOCK_TYPE_VALUES, false, true));

	case VALUE_BOOLEAN:
		return(parse_boolean((bool *)value_ptr));

	case VALUE_DEGREES:
		return(parse_degrees((float *)value_ptr));

	case VALUE_DELAY_RANGE:
		return(parse_delay_range((delayrange *)value_ptr));

	case VALUE_DIRECTION:
		return(parse_direction((direction *)value_ptr));

	case VALUE_DIRRANGE:
		return(parse_direction_range((dirrange *)value_ptr));

	case VALUE_DOUBLE_SYMBOL:
		return(parse_double_symbol((word *)value_ptr));

	case VALUE_EXIT_TRIGGER:
		return(parse_value((int *)value_ptr, exit_trigger_value_list, EXIT_TRIGGER_VALUES, true, true));

	case VALUE_FOG_STYLE:
		return(parse_value((int *)value_ptr, fog_style_value_list, FOG_STYLE_VALUES, false, true));

	case VALUE_FLOAT:
		return(parse_float_in_value((float *)value_ptr));

	case VALUE_FORCE:
		return(parse_scale((float_triplet *)value_ptr));

	case VALUE_HEADING:
		return(parse_heading((direction *)value_ptr));

	case VALUE_INTEGER:
		return(parse_integer_in_value((int *)value_ptr));

	case VALUE_INTEGER_RANGE:
		return(parse_integer_range((intrange *)value_ptr));

	case VALUE_IMAGEMAP_TRIGGER:
		return(parse_value((int *)value_ptr, imagemap_trigger_value_list, IMAGEMAP_TRIGGER_VALUES, false, true));

	case VALUE_KEY_CODE:
		return(parse_key_code((int *)value_ptr, true));

	case VALUE_MAP_COORDS:
		return(parse_map_coordinates((mapcoords *)value_ptr));

	case VALUE_MAP_DIMENSIONS:
		return(parse_map_dimensions((mapcoords *)value_ptr));

	case VALUE_MAP_STYLE:
		return(parse_value((int *)value_ptr, map_style_value_list, MAP_STYLE_VALUES, false, true));

	case VALUE_NAME:
		return(parse_name_in_value((string *)value_ptr, false, false));

	case VALUE_NAME_LIST:
		return(parse_name_in_value((string *)value_ptr, true, true));

	case VALUE_NAME_OR_WILDCARD:
		return(parse_name_in_value((string *)value_ptr, false, true));
		
	case VALUE_ORIENTATION:
		return(parse_orientation((orientation *)value_ptr));

	case VALUE_PCRANGE:
		return(parse_percentage_range((pcrange *)value_ptr));

	case VALUE_PERCENTAGE:
		return(parse_percentage((float *)value_ptr, NULL));

	case VALUE_PLACEMENT:
		return(parse_value((int *)value_ptr, alignment_value_list, ALIGNMENT_VALUES, false, true));

	case VALUE_PLAYBACK_MODE:
		return(parse_value((int *)value_ptr, playback_mode_value_list, PLAYBACK_MODE_VALUES, false, true));

	case VALUE_POINT_LIGHT_STYLE:
		return(parse_value((int *)value_ptr, point_light_style_value_list, POINT_LIGHT_STYLE_VALUES, false, true));

	case VALUE_POPUP_TRIGGER:
		return(parse_value((int *)value_ptr, popup_trigger_value_list, POPUP_TRIGGER_VALUES, true, true));

	case VALUE_PROJECTION:
		return(parse_value((int *)value_ptr, projection_value_list, PROJECTION_VALUES, false, true));

	case VALUE_RADIUS:
		return(parse_radius((float *)value_ptr));

#ifdef STREAMING_MEDIA

	case VALUE_RECT:
		return(parse_rectangle((video_rect *)value_ptr));

#endif

	case VALUE_REL_COORDS:
		return(parse_relative_coordinates((relcoords *)value_ptr));

	case VALUE_REL_INTEGER:
		return(parse_relative_integer((relinteger *)value_ptr));

	case VALUE_REL_INTEGER_TRIPLET:
		return(parse_relative_integer_triplet((relinteger_triplet *)value_ptr));

	case VALUE_RGB:
		return(parse_RGB((RGBcolour *)value_ptr));

	case VALUE_RIPPLE_STYLE:
		return(parse_value((int *)value_ptr, ripple_style_value_list, RIPPLE_STYLE_VALUES, false, true));

	case VALUE_SCALE:
		return(parse_scale((float_triplet *)value_ptr));
	
	case VALUE_SHAPE:
		return(parse_value((int *)value_ptr, shape_value_list, SHAPE_VALUES, false, true));

	case VALUE_SINGLE_SYMBOL:
		return(parse_single_symbol((char *)value_ptr));

	case VALUE_SIZE:
		return(parse_size((size *)value_ptr));

	case VALUE_SPOT_LIGHT_STYLE:
		return(parse_value((int *)value_ptr, spot_light_style_value_list, SPOT_LIGHT_STYLE_VALUES, false, true));

	case VALUE_SPRITE_SIZE:
		return(parse_sprite_size((size *)value_ptr));

	case VALUE_STRING:
		*(string *)value_ptr = value_string_ptr;
		value_string_ptr += strlen(value_string_ptr);
		return(true);

	case VALUE_SYMBOL:
		return(parse_symbol((word *)value_ptr, true, true));

	case VALUE_TEXTURE_STYLE:
		return(parse_value((int *)value_ptr, texture_style_value_list, TEXTURE_STYLE_VALUES, false, true));

	case VALUE_VALIGNMENT:
		return(parse_value((int *)value_ptr, vertical_alignment_value_list, VERTICAL_ALIGNMENT_VALUES, false, true));

	case VALUE_VERSION:
		return(parse_version((unsigned int *)value_ptr));

	case VALUE_VERTEX_COORDS:
		return(parse_vertex_coordinates((vertex_entry *)value_ptr));			
	}
	return(false);
}

//------------------------------------------------------------------------------
// Parse an attribute list in accordance to the given attribute definition list.
// Throws an exception upon an error.
//------------------------------------------------------------------------------

static void
parse_attribute_list(attr_def *attr_def_list, int attributes)
{
	int index, tag_token, attr_token;
	attr *attr_ptr;
	attr_def *attr_def_ptr;
	entity *entity_ptr;

	// Clear the parsed attribute list.

	for (index = 0; index < MAX_ATTRIBUTES; index++)
		parsed_attribute[index] = false;

	// Match each attribute with an attribute definition, and parse it's value.
	// Duplicates are not allowed.

	entity_ptr = file_stack_ptr->parse_stack_ptr->curr_entity_ptr;
	tag_token = get_token(entity_ptr->text);
	attr_ptr = entity_ptr->attr_list;
	while (attr_ptr != NULL) {
		if ((attr_token = get_token(attr_ptr->name)) == TOKEN_NONE)
			warning("Unrecognised attribute name <I>%s</I>", attr_ptr->name);
		else {
			attr_def_ptr = attr_def_list;
			for (index = 0; index < attributes; index++) {
				if (attr_token == attr_def_ptr->token) {
					if (parsed_attribute[index])
						warning("Duplicate <I>%s</I> attribute encountered", attr_ptr->name);
					else {
						start_parsing_value(tag_token, attr_token, attr_ptr->value, attr_def_ptr->required);
						if (parse_attribute_value(attr_def_ptr->value_type, attr_def_ptr->value_ptr) && stop_parsing_value(true))
							parsed_attribute[index] = true;
					}
					break;
				}
				attr_def_ptr++;
			}
		}
		if (index == attributes)
			warning("The <I>%s</I> tag does not have <I>%s</I> as an attribute", entity_ptr->text, attr_ptr->name);
		attr_ptr = attr_ptr->next_attr_ptr;
	}

	// Check if any attributes that were required are missing.

	attr_def_ptr = attr_def_list;
	for (index = 0; index < attributes; index++) {
		if (attr_def_ptr->required && !parsed_attribute[index])
			error("The <I>%s</I> attribute is missing from the <I>%s</I> tag", get_name(attr_def_ptr->token), entity_ptr->text);
		attr_def_ptr++;
	}
}

//------------------------------------------------------------------------------
// Get the current indentation as a string.
//------------------------------------------------------------------------------

static string
get_indentation()
{
	string indentation = "";
	for (int i = 0; i < indent_level; i++) {
		indentation += '\t';
	}
	return indentation;
}

//------------------------------------------------------------------------------
// Convert the current nested entity list to a string.
//------------------------------------------------------------------------------

static string
entity_list_to_string(entity *entity_list, bool add_formatting)
{
	string text;
	string name;
	attr *attr_ptr;
	entity *entity_ptr;

	// Convert each entity in the list.

	entity_ptr = entity_list;
	while (entity_ptr != NULL) {
		switch (entity_ptr->type) {

		// If the entity is deleted text, only use it if not adding formatting.

		case DELETED_TEXT_ENTITY:
			if (!add_formatting) {
				text += entity_ptr->text;
			}
			break;
		
		// If the entity is text, use it as-is.

		case TEXT_ENTITY:
			text += entity_ptr->text;
			break;

		// If the entity is a comment, reconstruct the comment tag.

		case COMMENT_ENTITY:
			if (add_formatting) {
				text += get_indentation();
			}
			text += "<!--";
			text += entity_ptr->text;
			text += "-->";
			if (add_formatting) {
				text += '\n';
			}
			break;

		// If the entity is a tag...

		case TAG_ENTITY:
			if (add_formatting) {
				text += get_indentation();
			}

			// Convert the tag name.

			text += "<";
			name = entity_ptr->text;
			if (add_formatting) {
				name.to_lowercase();
			}
			text += name;

			// Convert the attribute list.

			attr_ptr = entity_ptr->attr_list;
			while (attr_ptr) {
				text += " ";
				name = attr_ptr->name;
				if (add_formatting) {
					name.to_lowercase();
				}
				text += name;
				text += "=\"";
				text += attr_ptr->value;
				text += "\"";
				attr_ptr = attr_ptr->next_attr_ptr;
			}

			// Convert the nested entity list, if it exists, followed by an end tag.  Otherwise just display an empty-element tag.

			if (entity_ptr->nested_entity_list != NULL) {
				text += ">";
				bool got_define_tag = !_stricmp(entity_ptr->text, "define");
				bool got_script_tag = !_stricmp(entity_ptr->text, "script");
				if (add_formatting && !got_define_tag && !got_script_tag) {
					text += '\n';
				}
				indent_level++;
				text += entity_list_to_string(entity_ptr->nested_entity_list, got_define_tag ? false : add_formatting);
				indent_level--;
				if (add_formatting) {
					text += get_indentation();
				}
				text += "</";
				name = entity_ptr->text;
				if (add_formatting) {
					name.to_lowercase();
				}
				text += name;
				text += ">";
			} else
				text += "/>";
			if (add_formatting) {
				text += '\n';
			}
		} 

		// Move onto the next entity in the list.

		entity_ptr = entity_ptr->next_entity_ptr;
	}

	// Return the string.

	return(text);
}

//==============================================================================
// Exported parsing functions.
//==============================================================================

//------------------------------------------------------------------------------
// Parse the start of the XML document in the top file.  The given tag token and
// attribute list must match the document start tag.
//------------------------------------------------------------------------------

void
parse_start_of_document(int start_tag_token, attr_def *attr_def_list, int attributes)
{
	parse_stack_element *parse_stack_ptr;
	entity *entity_ptr;

	// Set the flag for strict XML compliance, then parse the document start
	// tag.

	strict_XML_compliance = true;
	inside_tag = false;
	entity_ptr = parse_start_tag(start_tag_token);

	// Push an element onto the top file's parser stack.  This will hold the
	// document entity list.

	parse_stack_ptr = push_parse_stack();
	parse_stack_ptr->entity_list = entity_ptr;
	parse_stack_ptr->curr_entity_ptr = entity_ptr;

	// Parse the attribute list of the document start tag.

	parse_attribute_list(attr_def_list, attributes);
}

//------------------------------------------------------------------------------
// Parse the rest of the XML document in the top file, starting with the nested
// entity list of the document start tag.
//------------------------------------------------------------------------------

void
parse_rest_of_document(bool strict_XML_compliance_flag)
{
	strict_XML_compliance = strict_XML_compliance_flag;
	entity *entity_ptr = file_stack_ptr->parse_stack_ptr->entity_list;
	entity_ptr->nested_entity_list = parse_nested_entity_list(entity_ptr->text);
}

//------------------------------------------------------------------------------
// Start parsing nested tags in the current entity.
//------------------------------------------------------------------------------

void
start_parsing_nested_tags()
{
	entity *nested_entity_list = file_stack_ptr->parse_stack_ptr->curr_entity_ptr->nested_entity_list;
	parse_stack_element *parse_stack_ptr = push_parse_stack();
	parse_stack_ptr->entity_list = nested_entity_list;
	parse_stack_ptr->curr_entity_ptr = NULL;
}

//------------------------------------------------------------------------------
// Parse the next nested tag by matching it against a tag in the given tag
// definition list. Returns TRUE and the tag token if a match was found,
// otherwise it returns FALSE.  If allow_text is TRUE, then character data will
// cause TOKEN_CHARACTER_DATA to be returned, at which point text_to_string()
// can be called to retrieve that character data as a string.
//------------------------------------------------------------------------------

bool
parse_next_nested_tag(int start_tag_token, tag_def *tag_def_list, bool allow_text, int *tag_token)
{
	tag_def *tag_def_ptr;
	parse_stack_element *parse_stack_ptr;
	entity *prev_entity_ptr;
	entity *entity_ptr;

	// Skip over the last nested tag entity parsed, if there is one.  Otherwise
	// start at the beginning of the nested entity list.

	parse_stack_ptr = file_stack_ptr->parse_stack_ptr;
	prev_entity_ptr = parse_stack_ptr->curr_entity_ptr;
	if (prev_entity_ptr != NULL)
		entity_ptr = prev_entity_ptr->next_entity_ptr;
	else
		entity_ptr = parse_stack_ptr->entity_list;

	// Keep looking for a tag entity until the end of the nested entity list has been reached.

	while (entity_ptr != NULL) {

		// Refresh the player window.

		if (!refresh_player_window())
			error(entity_ptr->line_no, "Parsing aborted");

		// Process this entity...

		switch (entity_ptr->type) {

		// If this is a text entity and text is allowed, return TOKEN_CHARACTER_DATA.  
		// Otherwise mark it as a deleted entity, after warning about non-whitespace text.
	
		case TEXT_ENTITY:
			if (allow_text) {
				parse_stack_ptr->curr_entity_ptr = entity_ptr;
				*tag_token = TOKEN_CHARACTER_DATA;
				return(true);
			} else {
				if (not_all_whitespace(entity_ptr->text)) 
					warning(entity_ptr->line_no, "Text is not allowed inside the <I>%s</I> tag", get_name(start_tag_token));
				entity_ptr->type = DELETED_TEXT_ENTITY;
			}
			break;

		// If this is a tag entity...

		case TAG_ENTITY:

			// If the tag name of this entity does not resolve to a valid token, generate a warning.

			if ((*tag_token = get_token(entity_ptr->text)) == TOKEN_NONE)
				warning(entity_ptr->line_no, "Unrecognised tag name <I>%s</I>", entity_ptr->text);

			// Otherwise attempt to match the tag entity against the tag definition list.

			else {
				tag_def_ptr = tag_def_list;
				while (tag_def_ptr->token != TOKEN_NONE) {
					if (*tag_token == tag_def_ptr->token) {
						parse_stack_ptr->curr_entity_ptr = entity_ptr;

						// If the tag entity has a nested entity list but shouldn't, generate a warning.

						if (entity_ptr->nested_entity_list != NULL && !tag_def_ptr->is_start_tag)
							warning("The <I>%s</I> tag does not permit anything inside of it", entity_ptr->text);

						// Parse the attribute list, if there is one.

						parse_attribute_list(tag_def_ptr->attr_def_list, tag_def_ptr->attributes);
						return(true);
					}
					tag_def_ptr++;
				}
				warning(entity_ptr->line_no, "The <I>%s</I> tag is not permitted inside of the <I>%s</I> tag", entity_ptr->text, get_name(start_tag_token));
			}
		}

		// Skip over this entity.

		entity_ptr = entity_ptr->next_entity_ptr;
	}
	return(false);
}

//------------------------------------------------------------------------------
// Stop parsing nested tags in the current entity.
//------------------------------------------------------------------------------

void
stop_parsing_nested_tags(void)
{
	pop_parse_stack();
}

//------------------------------------------------------------------------------
// Return the text of the current entity being parsed.
//------------------------------------------------------------------------------

string
text_to_string(void)
{
	return(file_stack_ptr->parse_stack_ptr->curr_entity_ptr->text);
}

//------------------------------------------------------------------------------
// Convert all nested tags into a string.
//------------------------------------------------------------------------------

string
nested_tags_to_string(void)
{
	return(entity_list_to_string(file_stack_ptr->parse_stack_ptr->curr_entity_ptr->nested_entity_list, false));
}

//------------------------------------------------------------------------------
// Return the first entity of the current entity list.
//------------------------------------------------------------------------------

entity *
get_first_entity(void)
{
	return file_stack_ptr->parse_stack_ptr->entity_list;
}

//------------------------------------------------------------------------------
// Return the current entity.
//------------------------------------------------------------------------------

entity *
get_current_entity(void)
{
	return file_stack_ptr->parse_stack_ptr->curr_entity_ptr;
}

//------------------------------------------------------------------------------
// Return a nested text entity, creating one with an empty string if missing and
// requested.  Throws an error if there are also nested tags present.
//------------------------------------------------------------------------------

entity *
nested_text_entity(int start_tag_token, bool create_if_missing)
{
	// If there is no nested entity list, then either return NULL or create a
	// text entity with an empty string, add it to the current entity's nested
	// entity list, and return it.

	entity *curr_entity_ptr = file_stack_ptr->parse_stack_ptr->curr_entity_ptr;
	entity *entity_ptr = curr_entity_ptr->nested_entity_list;
	if (entity_ptr == NULL) {
		if (create_if_missing) {
			NEW(entity_ptr, entity);
			if (entity_ptr == NULL)
				memory_error("XML entity");
			entity_ptr->line_no = curr_entity_ptr->line_no;
			entity_ptr->type = TEXT_ENTITY;
			entity_ptr->text = "";
			entity_ptr->attr_list = NULL;
			entity_ptr->nested_entity_list = NULL;
			entity_ptr->next_entity_ptr = NULL;
			curr_entity_ptr->nested_entity_list = entity_ptr;
		}
		return entity_ptr;
	}

	// There must be one text entity in the list, anything else is an error.

	if (entity_ptr->type != TEXT_ENTITY || entity_ptr->next_entity_ptr != NULL) {
		error(entity_ptr->line_no, "Tags are not permitted inside of the <I>%s</I> tag", get_name(start_tag_token));
	}
	return entity_ptr;
}

//------------------------------------------------------------------------------
// Return nested text.  Trailing whitespace is removed and a newline added, if
// requested. Throws an error if there are also nested tags present.
//------------------------------------------------------------------------------

string
nested_text_to_string(int start_tag_token, bool remove_trailing_whitespace)
{
	entity *entity_ptr = nested_text_entity(start_tag_token);
	if (entity_ptr) {
		if (remove_trailing_whitespace) {
			entity_ptr->text.remove_trailing_whitespace();
			entity_ptr->text += '\n';
		}
		return entity_ptr->text;
	}
	return "";
}

//------------------------------------------------------------------------------
// Create a tag entity with the given name and zero or more attribute name/value
// pairs.
//------------------------------------------------------------------------------

entity *
create_tag_entity(string tag_name, int line_no, ...)
{
	va_list arg_ptr;
	attr *attr_list = NULL;
	attr *last_attr_ptr = NULL;

	// Parse the name/value pairs until a NULL pointer is reached, and create
	// an attribute list for them.

	va_start(arg_ptr, line_no);
	for (;;) {
		char *name = va_arg(arg_ptr, char *);
		if (name == NULL) {
			break;
		}
		char *value = va_arg(arg_ptr, char *);
		attr *attr_ptr = create_attr(name, value);
		if (last_attr_ptr) {
			last_attr_ptr->next_attr_ptr = attr_ptr;
		} else {
			attr_list = attr_ptr;
		}
		last_attr_ptr = attr_ptr;
	}
	va_end(arg_ptr);

	// Create a tag entity with the specified name, line number and attribute
	// list.

	return create_entity(TAG_ENTITY, line_no, tag_name, attr_list);
}

//------------------------------------------------------------------------------
// Find the first instance of a tag entity with the given name in the given
// entity list (or one of its children).
//------------------------------------------------------------------------------

entity *
find_tag_entity(string tag_name, entity *entity_list)
{
	entity *entity_ptr = entity_list;
	while (entity_ptr) {
		if (entity_ptr->type == TAG_ENTITY && !_stricmp(entity_ptr->text, tag_name)) {
			return entity_ptr;
		}
		if (entity_ptr->nested_entity_list) {
			entity *nested_entity_ptr = find_tag_entity(tag_name, entity_ptr->nested_entity_list);
			if (nested_entity_ptr) {
				return nested_entity_ptr;
			}
		}
		entity_ptr = entity_ptr->next_entity_ptr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// Prepend an entity to an entity list.
//------------------------------------------------------------------------------

void
prepend_tag_entity(entity *tag_entity_ptr, entity *entity_list)
{
	tag_entity_ptr->next_entity_ptr = entity_list->next_entity_ptr;
	entity_list->next_entity_ptr = tag_entity_ptr;
}

//------------------------------------------------------------------------------
// Insert a new entity after an existing one.
//------------------------------------------------------------------------------

void
insert_tag_entity(entity *tag_entity_ptr, entity *entity_ptr)
{
	entity *next_entity_ptr = entity_ptr->next_entity_ptr;
	entity_ptr->next_entity_ptr = tag_entity_ptr;
	tag_entity_ptr->next_entity_ptr = next_entity_ptr;
}

//------------------------------------------------------------------------------
// Find the first instance of an attribute with the given name in the given
// entity, or create a new one, and assign it the given value.
//------------------------------------------------------------------------------

void
set_entity_attr(entity *entity_ptr, string attr_name, string attr_value)
{
	attr *attr_ptr = entity_ptr->attr_list;
	attr *prev_attr_ptr = NULL;
	while (attr_ptr) {
		if (!_stricmp(attr_ptr->name, attr_name)) {
			attr_ptr->value = attr_value;
			return;
		}
		prev_attr_ptr = attr_ptr;
		attr_ptr = attr_ptr->next_attr_ptr;
	}
	attr_ptr = create_attr(attr_name, attr_value);
	if (prev_attr_ptr) {
		prev_attr_ptr = attr_ptr;
	} else {
		entity_ptr->attr_list = attr_ptr;
	}
}

//------------------------------------------------------------------------------
// Save the current XML document to a file.
//------------------------------------------------------------------------------

void
save_document(const char *file_name, entity *entity_list)
{
	string text;
	FILE *fp;

	indent_level = 0;
	text = entity_list_to_string(entity_list, true);
	if ((fp = fopen(file_name, "w")) != NULL) {
		text.write(fp);
		fclose(fp);
	}
}

//==============================================================================
// Attribute value to string functions.
//==============================================================================

static char attribute_string[256];

//------------------------------------------------------------------------------
// Convert an integer value to a value string based on its type.
//------------------------------------------------------------------------------

string
value_to_string(int value_type, int value)
{
	switch (value_type) {
	case VALUE_BOOLEAN:
		return get_value_str_from_list(value, boolean_value_list, BOOLEAN_VALUES);
	case VALUE_ACTION_TRIGGER:
		return get_value_str_from_list(value, action_trigger_value_list, ACTION_TRIGGER_VALUES);
	case VALUE_EXIT_TRIGGER:
		return get_value_str_from_list(value, exit_trigger_value_list, EXIT_TRIGGER_VALUES);
	case VALUE_FOG_STYLE:
		return get_value_str_from_list(value, fog_style_value_list, FOG_STYLE_VALUES);
	case VALUE_KEY_CODE:
		return get_value_str_from_list(value, key_code_value_list, KEY_CODE_VALUES);
	case VALUE_MAP_STYLE:
		return get_value_str_from_list(value, map_style_value_list, MAP_STYLE_VALUES);
	case VALUE_ALIGNMENT:
	case VALUE_PLACEMENT:
		return get_value_str_from_list(value, alignment_value_list, ALIGNMENT_VALUES);
	case VALUE_PLAYBACK_MODE:
		return get_value_str_from_list(value, playback_mode_value_list, PLAYBACK_MODE_VALUES);
	case VALUE_POINT_LIGHT_STYLE:
		return get_value_str_from_list(value, point_light_style_value_list, POINT_LIGHT_STYLE_VALUES);
	case VALUE_POPUP_TRIGGER:
		return get_value_str_from_list(value, popup_trigger_value_list, POPUP_TRIGGER_VALUES, true);
	case VALUE_SPOT_LIGHT_STYLE:
		return get_value_str_from_list(value, spot_light_style_value_list, SPOT_LIGHT_STYLE_VALUES);
	default:
		return "";
	}
}

//------------------------------------------------------------------------------
// Convert various data types to strings.
//------------------------------------------------------------------------------

string
boolean_to_string(bool flag)
{
	return value_to_string(VALUE_BOOLEAN, flag);
}

string
key_code_to_string(byte key_code)
{
	if ((key_code >= '0' && key_code <= '9') || (key_code >= 'A' && key_code <= 'Z')) {
		sprintf(attribute_string, "%c", key_code);
	} else {
		sprintf(attribute_string, "[%s]", (char *)value_to_string(VALUE_KEY_CODE, key_code));
	}
	return attribute_string;
}

string
int_to_string(int value)
{
	sprintf(attribute_string, "%d", value);
	return attribute_string;
}

string
float_to_string(float value)
{
	sprintf(attribute_string, "%g", value);
	return attribute_string;
}

string
percentage_to_string(float percentage)
{
	sprintf(attribute_string, "%g%%", percentage * 100.0f);
	return attribute_string;
}

string
percentage_range_to_string(pcrange percentage_range)
{
	string min_percentage = percentage_to_string(percentage_range.min_percentage);
	if (percentage_range.min_percentage == percentage_range.max_percentage) {
		return min_percentage;
	}
	string max_percentage = percentage_to_string(percentage_range.max_percentage);
	sprintf(attribute_string, "%s..%s", (char *)min_percentage, (char *)max_percentage);
	return attribute_string;
}

string
colour_to_string(RGBcolour colour, bool normalized)
{
	if (normalized) {
		sprintf(attribute_string, "(%d, %d, %d)", (int)(colour.red * 255.0f), (int)(colour.green * 255.0f), (int)(colour.blue * 255.0f));
	} else {
		sprintf(attribute_string, "(%g, %g, %g)", colour.red, colour.green, colour.blue);
	}
	return attribute_string;
}

string
delay_range_to_string(delayrange delay_range)
{
	if (delay_range.delay_range_ms > 0) {
		sprintf(attribute_string, "%g..%g", (float)delay_range.min_delay_ms / 1000.0f, (float)(delay_range.min_delay_ms + delay_range.delay_range_ms) / 1000.0f);
	} else {
		sprintf(attribute_string, "%g", (float)delay_range.min_delay_ms / 1000.0f);
	}
	return attribute_string;
}

string
radius_to_string(float radius)
{
	sprintf(attribute_string, "%g", radius / units_per_block);
	return attribute_string;
}

string
direction_to_string(direction dir, bool add_parens)
{
	if (add_parens) {
		sprintf(attribute_string, "(%g, %g)", dir.angle_y, dir.angle_x);
	} else {
		sprintf(attribute_string, "%g, %g", dir.angle_y, dir.angle_x);
	}
	return attribute_string;
}

string
direction_range_to_string(dirrange dir_range)
{
	string min_direction = direction_to_string(dir_range.min_direction, true);
	if (dir_range.min_direction.angle_x == dir_range.max_direction.angle_x && dir_range.min_direction.angle_y == dir_range.max_direction.angle_y) {
		return min_direction;
	}
	string max_direction = direction_to_string(dir_range.max_direction, true);
	sprintf(attribute_string, "%s..%s", (char *)min_direction, (char *)max_direction);
	return attribute_string;
}

string
location_to_string(int column, int row, int level)
{
	sprintf(attribute_string, "(%d, %d, %d)", column + 1, row + 1, world_ptr->ground_level_exists ? level : level + 1);
	return attribute_string;
}

string
square_location_to_string(square *square_ptr)
{
	int column, row, level;
	world_ptr->get_square_location(square_ptr, &column, &row, &level);
	return location_to_string(column, row, level);
}

string
map_coords_to_string(mapcoords map_coords)
{
	return location_to_string(map_coords.column, map_coords.row, map_coords.level);
}

string
vertex_to_string(vertex v)
{
	sprintf(attribute_string, "(%g, %g, %g)", v.x * texels_per_unit, v.y * texels_per_unit, v.z * texels_per_unit);
	return attribute_string;
}

string
size_to_string(int width, int height)
{
	sprintf(attribute_string, "(%d, %d)", width, height);
	return attribute_string;
}