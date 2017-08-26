//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

#include "tokens.h"

// Attribute definition structure.

struct attr_def {
	int token;
	int value_type;
	void *value_ptr;
	bool required;
};

// Tag definition class.

struct tag_def {
	int token;
	attr_def *attr_def_list;
	int attributes;
	bool is_start_tag;
};

// Array of flags indicating which attributes of a tag have been parsed.

#define MAX_ATTRIBUTES	32
extern bool parsed_attribute[MAX_ATTRIBUTES];

// Value definition class.

struct value_def {
	const char *str;
	int value;
};

// Initialisation function.

void
init_parser(void);

// URL and conversion functions.

void
split_URL(const char *URL, string *URL_dir, string *file_name, string *entrance_name);

string
create_URL(const char *URL_dir, const char *file_name);

string
encode_URL(const char *URL);

string
decode_URL(const char *URL);

string
URL_to_file_path(const char *URL);

void
parse_identifier(const char *identifier, string &style_name, string &object_name);

bool
not_single_symbol(char ch, bool disallow_dot);

bool
not_double_symbol(char ch1, char ch2, bool disallow_dot_dot);

bool
string_to_single_symbol(const char *string_ptr, char *symbol_ptr, bool disallow_dot);

bool
string_to_double_symbol(const char *string_ptr, word *symbol_ptr, bool disallow_dot_dot);

bool
string_to_symbol(const char *string_ptr, word *symbol_ptr, bool disallow_dot_dot);

string
version_number_to_string(unsigned int version_number);

// Zip archive functions.

bool
open_zip_archive(const char *file_path, const char *file_name);

bool
open_blockset(const char *blockset_URL, const char *blockset_name);

void
close_zip_archive(void);

// File functions.

void
destroy_entity_list(entity *entity_list);

void
destroy_entity(entity *entity_ptr);

bool
push_file(const char *file_path, const char *file_URL, bool text_file);

bool
push_zip_file(const char *file_path, bool text_file);

bool
push_zip_file_with_ext(const char *file_ext, bool text_file);

void
get_file_buffer(char **file_buffer_ptr, int *file_size);

bool
push_buffer(const char *buffer_ptr, int buffer_size);

void
rewind_file(void);

entity *
pop_file(bool return_entity_list = false);

void
pop_all_files(void);

int
read_file(byte *buffer_ptr, int bytes);

bool
copy_file(const char *file_path, bool text_file);

// Error checking and reporting functions.

void
bprintf(char *buffer, int size, const char *format, ...);

void
vbprintf(char *buffer, int size, const char *format, va_list arg_ptr);

void
write_error_log(const char *format, ...);

void 
diagnose(const char *format, ...);

void
log(const char *format, ...);

void
warning(const char *format, ...);

void
memory_warning(const char *object);

void
error(const char *format, ...);

void
memory_error(const char *object);

bool
check_int_range(int value, int min, int max);

bool
check_float_range(float value, float min, float max);

// Name parsing function.

bool
parse_name(const char *old_name, string *new_name, bool allow_list, bool allow_wildcard);

// Attribute value parsing functions.

void
start_parsing_value(int tag_token, int attr_token, char *attr_value, bool attr_required);

bool
parse_integer_in_value(int *int_ptr);

bool
parse_integer_range(intrange *intrange_ptr);

bool
parse_float_in_value(float *float_ptr);

bool
parse_playback_mode(char *attr_value, int *playback_mode_ptr);

bool
token_in_value_is(const char *token_string, bool generate_error);

bool
stop_parsing_value(bool generate_error);

// Document parsing functions.

entity *
create_entity(int type, int line_no, string text, attr *attr_list);

void
parse_start_of_document(int start_tag_token, attr_def *attr_def_list, int attributes);

void
parse_rest_of_document(bool strict_XML_compliance);

void
start_parsing_nested_tags(void);

bool
parse_next_nested_tag(int start_tag_token, tag_def *tag_def_list, bool allow_text, int *tag_token_ptr);

void
stop_parsing_nested_tags(void);

string
text_to_string(void);

string
nested_tags_to_string(void);

entity *
get_first_entity(void);

entity *
get_current_entity(void);

entity *
nested_text_entity(int start_tag_token, bool create_if_missing = false);

string
nested_text_to_string(int start_tag_token, bool remove_trailing_whitespace = false);

entity *
create_tag_entity(string tag_name, int line_no, ...);

entity *
find_tag_entity(string tag_name, entity *entity_list);

void
prepend_tag_entity(entity *tag_entity_ptr, entity *entity_list);

void
insert_tag_entity(entity *tag_entity_ptr, entity *entity_ptr);

void
set_entity_attr(entity *entity_ptr, string attr_name, string attr_value);

void
save_document(const char *file_name, entity *entity_list);

// Attribute value to string functions.

string
value_to_string(int value_type, int value);

string
boolean_to_string(bool flag);

string
key_code_to_string(byte key_code);

string
int_to_string(int value);

string
float_to_string(float value);

string
percentage_to_string(float percentage);

string
percentage_range_to_string(pcrange percentage_range);

string
colour_to_string(RGBcolour colour, bool normalized = false);

string
delay_range_to_string(delayrange delay_range);

string
radius_to_string(float radius);

string
direction_to_string(direction dir, bool add_parens);

string
direction_range_to_string(dirrange dir_range);

string
location_to_string(int column, int row, int level);

string
square_location_to_string(square *square_ptr);

string
map_coords_to_string(mapcoords map_coords);

string
vertex_to_string(vertex v);

string
size_to_string(int width, int height);