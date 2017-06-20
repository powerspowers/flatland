//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "Classes.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Spans.h"
//POWERS #include "SimKin.h"
#include "Utils.h"

//==============================================================================
// Basic data classes.
//==============================================================================

//------------------------------------------------------------------------------
// String class.
//------------------------------------------------------------------------------

// The empty string: used to guarentee that a string will have a value in case
// of an out of memory status.

static char *empty_string = "";

// Default constructor allocates a empty string.

string::string()
{
	text = empty_string;
}

// Constructor for char.

string::string(char new_char)
{
	if (new_char != '\0' && (text = new char[2]) != NULL) {
		text[0] = new_char;
		text[1] = '\0';
	} else
		text = empty_string;
}

// Constructor for char *.

string::string(char *new_text)
{
	if (new_text != NULL && new_text[0] != '\0' &&
		(text = new char[strlen(new_text) + 1]) != NULL)
		strcpy(text, new_text);
	else
		text = empty_string;
}

// Constructor for const char *.

string::string(const char *new_text)
{
	if (new_text != NULL && new_text[0] != '\0' &&
		(text = new char[strlen(new_text) + 1]) != NULL)
		strcpy(text, new_text);
	else
		text = empty_string;
}

// Copy constructor.

string::string(const string &old)
{
	if (old.text != empty_string &&
		(text = new char[strlen(old.text) + 1]) != NULL)
		strcpy(text, old.text);
	else
		text = empty_string;
}

// Default destructor deletes the text.

string::~string()
{
	if (text != empty_string)
		delete []text;
}

// Conversion operator to return a pointer to the text.

string::operator char *()
{
	return(text);
}

// Character index operator.  If the index is out of range, a reference to the
// terminating zero byte is returned.

char&
string::operator [](int index)
{
		return(text[index]);
}

// Assignment operator.

string&
string::operator=(const string &old)
{
	if (text != empty_string)
		delete []text;
	if (old.text[0] != '\0' &&
		(text = new char[strlen(old.text) + 1]) != NULL)
		strcpy(text, old.text);
	else
		text = empty_string;
	return(*this);
}

// Concatenate and assignment operators.

string&
string::operator+=(char add_char)
{
	char *new_text;
	int length;

	length = strlen(text);
	if (add_char != '\0' &&
		(new_text = new char[length + 2]) != NULL) {
		strcpy(new_text, text);
		new_text[length] = add_char;
		new_text[length + 1] = '\0';
		if (text != empty_string)
			delete []text;
		text = new_text;
	}
	return(*this);
}

string&
string::operator+=(const char *add_text)
{
	char *new_text;

	if (add_text != NULL && add_text[0] != '\0' &&
		(new_text = new char[strlen(text) + strlen(add_text) + 1]) != NULL) {
		strcpy(new_text, text);
		strcat(new_text, add_text);
		if (text != empty_string)
			delete []text;
		text = new_text;
	}
	return(*this);
}

// Plus operator appends a character to the string and returns a new string.

string
string::operator+(char add_char)
{
	string new_string;

	new_string = text;
	new_string += add_char;
	return(new_string);
}

// Plus operator appends text to the string and returns a new string.

string
string::operator+(const char *add_text)
{
	string new_string;

	new_string = text;
	if (add_text != NULL)
		new_string += add_text;
	return(new_string);
}

// Method to truncate a string to the given length.

void
string::truncate(unsigned int new_length)
{
	char *text_buf;
	
	if (strlen(text) > new_length) {
		if (new_length == 0 || (text_buf = new char[new_length + 1]) == NULL) {
			if (text != empty_string)
				delete []text;
			text = empty_string;
		} else {
			strncpy(text_buf, text, new_length);
			text_buf[new_length] = '\0';
			if (text != empty_string)
				delete []text;
			text = text_buf;
		}
	}
}

// Method to copy a fixed number of characters from a string.  It is assumed
// that the specified length is not greater than the string.

void
string::copy(const char *new_text, unsigned int new_length)
{
	if (text != empty_string)
		delete []text;
	if (new_text != NULL && new_text[0] != '\0' && new_length > 0 &&
		(text = new char[new_length + 1]) != NULL) {
		strncpy(text, new_text, new_length);
		text[new_length] = '\0';
	} else
		text = empty_string;
}

// Method to append a character buffer to the end of the string.

void 
string::append(const char *add_text, unsigned int add_length)
{
	if (add_text != NULL && add_length > 0) {
		char *new_text;

		if ((new_text = new char[strlen(text) + add_length + 1]) != NULL) {
			strcpy(new_text, text);
			strncat(new_text, add_text, add_length);
			if (text != empty_string)
				delete []text;
			text = new_text;
		}
	}
}

// Method to write the string to a file.

void
string::write(FILE *fp)
{
	int length = strlen(text);
	char *ch_ptr = text;
	for (int index = 0; index < length; index++)
		fputc(*ch_ptr++, fp);
}

//------------------------------------------------------------------------------
// RGB colour class.
//------------------------------------------------------------------------------

// Default constructor initialises colour to black.

RGBcolour::RGBcolour()
{
	red = 0.0f;
	green = 0.0f;
	blue = 0.0f;
}

// Default destructor does nothing.

RGBcolour::~RGBcolour()
{
}

// Method to initialise colour with another colour.

void
RGBcolour::set(RGBcolour colour)
{
	red = colour.red;
	green = colour.green;
	blue = colour.blue;
}

// Method to set colour components.

void
RGBcolour::set_RGB(float new_red, float new_green, float new_blue)
{
	red = new_red;
	green = new_green;
	blue = new_blue;
}

// Method to clamp the colour components to values between 0 and 255.

void 
RGBcolour::clamp(void)
{
	if (FLT(red, 0.0f))
		red = 0.0f;
	else if (FGT(red, 255.0f))
		red = 255.0f;
	if (FLT(green, 0.0f))
		green = 0.0f;
	else if (FGT(green, 255.0f))
		green = 255.0f;
	if (FLT(blue, 0.0f))
		blue = 0.0f;
	else if (FGT(blue, 255.0f))
		blue = 255.0f;
}

// Method to adjust brightness of colour components by the given positive 
// factor.

void
RGBcolour::adjust_brightness(float brightness)
{
	// Compute the new RGB components, clamping them to a maximum of 255.

	red *= brightness;
	if (FGT(red, 255.0f))
		red = 255.0f;
	green *= brightness;
	if (FGT(green, 255.0f))
		green = 255.0f;
	blue *= brightness;
	if (FGT(blue, 255.0f))
		blue = 255.0f;
}

// Method to normalise the components so that they are between 0 and 1.

void
RGBcolour::normalise(void)
{
	red /= 255.0f;
	green /= 255.0f;
	blue /= 255.0f;
}

// Method to blend another colour with this one.  Both must be normalised
// beforehand.

void
RGBcolour::blend(RGBcolour colour)
{
	red *= colour.red;
	if (FGT(red, 1.0f))
		red = 1.0f;
	green *= colour.green;
	if (FGT(green, 1.0f))
		green = 1.0f;
	blue *= colour.blue;
	if (FGT(blue, 1.0f))
		blue = 1.0f;
}

//------------------------------------------------------------------------------
// Texture coordinates class.
//------------------------------------------------------------------------------

// Default constructor initialises texture coordinate to (0,0).

texcoords::texcoords()
{
	u = 0.0;
	v = 0.0;
}

// Default destructor does nothing.

texcoords::~texcoords()
{
}

//------------------------------------------------------------------------------
// Map coordinates class.
//------------------------------------------------------------------------------

// Default constructor initialises map coordinate to (0,0,0).

mapcoords::mapcoords()
{
	column = 0;
	row = 0;
	level = 0;
}

// Default destructor does nothing.

mapcoords::~mapcoords()
{
}

// Method to set the map coordinates.

void 
mapcoords::set(int set_column, int set_row, int set_level)
{
	column = set_column;
	row = set_row;
	level = set_level;
}

//------------------------------------------------------------------------------
// Relative integer class.
//------------------------------------------------------------------------------

// Default constructor initialises relative integer to absolute 0.

relinteger::relinteger()
{
	value = 0;
	relative_value = false;
}

// Default destructor does nothing.

relinteger::~relinteger()
{
}

// Method to set the relative integer.

void 
relinteger::set(int set_value, bool set_relative_value)
{
	value = set_value;
	relative_value = set_relative_value;
}

//------------------------------------------------------------------------------
// Relative integer triplet class.
//------------------------------------------------------------------------------

// Default constructor initialises relative integer to absolute (0,0,0).

relinteger_triplet::relinteger_triplet()
{
	x = 0;
	y = 0;
	z = 0;
	relative_x = true;
	relative_y = true;
	relative_z = true;
}

// Default destructor does nothing.

relinteger_triplet::~relinteger_triplet()
{
}

// Method to set the relative coordinates.

void 
relinteger_triplet::set(int set_x, int set_y, int set_z,
			   bool set_relative_x, bool set_relative_y,
			   bool set_relative_z)
{
	x = set_x;
	y = set_y;
	z = set_z;
	relative_x = set_relative_x;
	relative_y = set_relative_y;
	relative_z = set_relative_z;
}

//------------------------------------------------------------------------------
// Relative coordinates class.
//------------------------------------------------------------------------------

// Default constructor initialises relative coordinate to absolute (0,0,0).

relcoords::relcoords()
{
	column = 0;
	row = 0;
	level = 0;
	relative_column = false;
	relative_row = false;
	relative_level = false;
}

// Default destructor does nothing.

relcoords::~relcoords()
{
}

// Method to set the relative coordinates.

void 
relcoords::set(int set_column, int set_row, int set_level,
			   bool set_relative_column, bool set_relative_row,
			   bool set_relative_level)
{
	column = set_column;
	row = set_row;
	level = set_level;
	relative_column = set_relative_column;
	relative_row = set_relative_row;
	relative_level = set_relative_level;
}

//------------------------------------------------------------------------------
// Orientation class.
//------------------------------------------------------------------------------

// Default constructor creates a north orientation.

orientation::orientation()
{
	angle_x = 0.0f;
	angle_y = 0.0f;
	angle_z = 0.0f;
	direction = NONE;
	angle = 0.0f;
}

// Default destructor does nothing.

orientation::~orientation()
{
}

// Method to set the orientation.

void
orientation::set(float set_angle_x, float set_angle_y, float set_angle_z)
{
	angle_x = set_angle_x;
	angle_y = set_angle_y;
	angle_z = set_angle_z;
	direction = NONE;
	angle = 0.0f;
}

//------------------------------------------------------------------------------
// Direction class.
//------------------------------------------------------------------------------

// Default constructor sets the direction to north.

direction::direction()
{
	angle_x = 0.0;
	angle_y = 0.0;
}

// Default destructor does nothing.

direction::~direction()
{
}

// Method to set a direction.

void
direction::set(float set_angle_x, float set_angle_y)
{
	angle_x = set_angle_x;
	angle_y = set_angle_y;
}

//------------------------------------------------------------------------------
// Percentage range class.
//------------------------------------------------------------------------------

// Default constructor sets the minimum and maximum percentage to 100%.

pcrange::pcrange()
{
	min_percentage = 1.0f;
	max_percentage = 1.0f;
}

// Default destructor does nothing.

pcrange::~pcrange()
{
}

//------------------------------------------------------------------------------
// Integer range class.
//------------------------------------------------------------------------------

// Default constructor sets the minimum and maximum integers
intrange::intrange()
{
	min = 1;
	max = 1;
}

// Default destructor does nothing.

intrange::~intrange()
{
}

//------------------------------------------------------------------------------
// Delay range class.
//------------------------------------------------------------------------------

// Default constructor sets the minimum delay to 5 seconds, and the delay
// range to 5 seconds.

delayrange::delayrange()
{
	min_delay_ms = 5000;
	delay_range_ms = 5000;
}

// Default destructor does nothing.

delayrange::~delayrange()
{
}

//------------------------------------------------------------------------------
// Vertex class.
//------------------------------------------------------------------------------

// Default constructor creates a zero vertex.

vertex::vertex()
{
	x = 0.0f;
	y = 0.0f;
	z = 0.0f;
}

// Constructor for creating an arbitrary vertex.

vertex::vertex(float set_x, float set_y, float set_z)
{
	x = set_x;
	y = set_y;
	z = set_z;
}

// Default destructor does nothing.

vertex::~vertex()
{
}

// Method to set the vertex.

void 
vertex::set(float set_x, float set_y, float set_z)
{
	x = set_x;
	y = set_y;
	z = set_z;
}

// Method to set the vertex to a map position.  The vertex will be in the centre
// of the block.

void
vertex::set_map_position(int column, int row, int level)
{
	x = ((float)column + 0.5f) * UNITS_PER_BLOCK;
	y = ((float)level + 0.5f) * UNITS_PER_BLOCK;
	z = ((float)(world_ptr->rows - row - 1) + 0.5f) * UNITS_PER_BLOCK;
}

// Method to set a translation to a map position.

void
vertex::set_map_translation(int column, int row, int level)
{
	x = (float)column * UNITS_PER_BLOCK;
	y = (float)level * UNITS_PER_BLOCK;
	z = (float)(world_ptr->rows - row - 1) * UNITS_PER_BLOCK;
}

// Method to return the scaled map position the vertex is in.  Note that we
// use a special rounding function to ensure that the closest map coordinates
// are selected, in case of small inaccuracies in the (x, y, z) coordinates.

void
vertex::get_map_position(int *column_ptr, int *row_ptr, int *level_ptr)
{
	*column_ptr = INT(x / UNITS_PER_BLOCK);
	*row_ptr = world_ptr->rows - INT(z / UNITS_PER_BLOCK) - 1;
	*level_ptr = INT(y / UNITS_PER_BLOCK);
}

// Method to return the map level the vertex is in.

int
vertex::get_map_level(void)
{
	return(INT(y / UNITS_PER_BLOCK));
}

// Operator to add a vertex.

vertex
vertex::operator +(vertex A)
{
	vertex B;

	B.x = x + A.x;
	B.y = y + A.y;
	B.z = z + A.z;
	return(B);
}

// Operator to subtract a vertex and create a vector.

vector
vertex::operator -(vertex A)
{
	vector B;

	B.dx = x - A.x;
	B.dy = y - A.y;
	B.dz = z - A.z;
	return(B);
}

// Operator to add a vector to create another vertex.

vertex
vertex::operator +(vector A)
{
	vertex B;

	B.x = x + A.dx;
	B.y = y + A.dy;
	B.z = z + A.dz;
	return(B);
}

// Operator to subtract a vector to create another vertex.

vertex
vertex::operator -(vector A)
{
	vertex B;

	B.x = x - A.dx;
	B.y = y - A.dy;
	B.z = z - A.dz;
	return(B);
}

// Operator to add a constant.

vertex
vertex::operator +(float A)
{
	vertex B;

	B.x = x + A;
	B.y = y + A;
	B.z = z + A;
	return(B);
}

// Operator to subtract a constant.

vertex
vertex::operator -(float A)
{
	vertex B;

	B.x = x - A;
	B.y = y - A;
	B.z = z - A;
	return(B);
}

// Operator to multiply by a constant.

vertex
vertex::operator *(float A)
{
	vertex B;

	B.x = x * A;
	B.y = y * A;
	B.z = z * A;
	return(B);
}

// Operator to divide by a constant.

vertex
vertex::operator /(float A)
{
	vertex B;

	B.x = x / A;
	B.y = y / A;
	B.z = z / A;
	return(B);
}

// Addition assignment operators.

vertex&
vertex::operator +=(vertex &A)
{
	x += A.x;
	y += A.y;
	z += A.z;
	return(*this);
}

vertex&
vertex::operator +=(vector &A)
{
	x += A.dx;
	y += A.dy;
	z += A.dz;
	return(*this);
}

// Subtraction assignment operators.

vertex&
vertex::operator -=(vertex &A)
{
	x -= A.x;
	y -= A.y;
	z -= A.z;
	return(*this);
}

vertex&
vertex::operator -=(vector &A)
{
	x -= A.dx;
	y -= A.dy;
	z -= A.dz;
	return(*this);
}

// Negation operator.

vertex
vertex::operator -()
{
	vertex A;

	A.x = -x;
	A.y = -y;
	A.z = -z;
	return(A);
}

// Comparison operators.

bool
vertex::operator ==(vertex A)
{
	return(FEQ(x, A.x) && FEQ(y, A.y) && FEQ(z, A.z));
}

bool
vertex::operator !=(vertex A)
{
	return(FNE(x, A.x) || FNE(y, A.y) || FNE(z, A.z));
}

// Method to rotate the vertex around the X axis.

void 
vertex::rotate_x(float angle)
{
	float temp_y;

	temp_y = y * cosine[angle] - z * sine[angle];
	z = z * cosine[angle] + y * sine[angle];
	//temp_y = y * cos(angle) - z * sin(angle);
	//z = z * cos(angle) + y * sin(angle);
	y = temp_y;
}

// Method to rotate the vertex around the Y axis.

void
vertex::rotate_y(float angle)
{
	float temp_x;

	temp_x = x * cosine[angle] + z * sine[angle];
	z = z * cosine[angle] - x * sine[angle];
	//temp_x = x * cos(angle) + z * sin(angle);
	//z = z * cos(angle) - x * sin(angle);
	x = temp_x;
}

// Method to rotate the vertex around the Z axis.

void
vertex::rotate_z(float angle)
{
	float temp_x;

	temp_x = x * cosine[angle] - y * sine[angle];
	y = y * cosine[angle] + x * sine[angle];
	//temp_x = x * cos(angle) - y * sin(angle);
	//y = y * cos(angle) + x * sin(angle);
	x = temp_x;
}

//------------------------------------------------------------------------------
// Vector class.
//------------------------------------------------------------------------------

// Default constructor creates a zero vector.

vector::vector()
{
	dx = 0.0f;
	dy = 0.0f;
	dz = 0.0f;
}

// Constructor for creating an arbitrary vector.

vector::vector(float set_dx, float set_dy, float set_dz)
{
	dx = set_dx;
	dy = set_dy;
	dz = set_dz;
}

// Default destructor does nothing.

vector::~vector()
{
}

// Method to set the vector.

void
vector::set(float set_dx, float set_dy, float set_dz)
{
	dx = set_dx;
	dy = set_dy;
	dz = set_dz;
}

// Operator to scale a vector.

vector
vector::operator *(float A)
{
	vector B;

	B.dx = dx * A;
	B.dy = dy * A;
	B.dz = dz * A;
	return(B);
}

// Operator to compute the cross product.

vector
vector::operator *(vector A)
{
	vector B;

	B.dx = dy * A.dz - dz * A.dy;
	B.dy = dz * A.dx - dx * A.dz;
	B.dz = dx * A.dy - dy * A.dx;
	return(B);
}

// Operator to compute the dot product of two vectors.

float
vector::operator &(vector A)
{
	return(dx * A.dx + dy * A.dy + dz * A.dz);
}

// Operator to add one vector to another to create a third vector.

vector
vector::operator +(vector A)
{
	vector B;

	B.dx = dx + A.dx;
	B.dy = dy + A.dy;
	B.dz = dz + A.dz;
	return(B);
}

// Negation operator.

vector
vector::operator -()
{
	vector A;

	A.dx = -dx;
	A.dy = -dy;
	A.dz = -dz;
	return(A);
}

// Operator to compare vector with zero.

bool
vector::operator !()
{
	return(FEQ(dx, 0.0) && FEQ(dy, 0.0) && FEQ(dz, 0.0));
}

// Comparison operator.

bool
vector::operator ==(vector A)
{
	return(FEQ(dx, A.dx) && FEQ(dy, A.dy) && FEQ(dz, A.dz));
}

// Method to normalise the vector.

void
vector::normalise(void)
{
	float magnitude = (float)sqrt(dx * dx + dy * dy + dz * dz);
	if (FGT(magnitude, 0.0f)) {
		float one_on_magnitude = 1.0f / magnitude;
		dx = dx * one_on_magnitude;
		dy = dy * one_on_magnitude;
		dz = dz * one_on_magnitude;
	}
}

// Method to normalise the vector, returning the length of the vector.

float
vector::length(void)
{
	float magnitude = MATHS_sqrt(dx * dx + dy * dy + dz * dz);
	if (FGT(magnitude, 0.0f)) {
		float one_on_magnitude = 1.0f / magnitude;
		dx = dx * one_on_magnitude;
		dy = dy * one_on_magnitude;
		dz = dz * one_on_magnitude;
	}
	return(magnitude);
}

// Method to rotate the vector around the X axis (look).

void 
vector::rotate_x(float look_angle)
{
	float temp_dy;

	//temp_dy = dy * cosine[look_angle] - dz * sine[look_angle];
	//dz = dz * cosine[look_angle] + dy * sine[look_angle];
	temp_dy = (float)(dy * cos(look_angle) - dz * sin(look_angle));
	dz = (float)(dz * cos(look_angle) + dy * sin(look_angle));
	dy = temp_dy;
}

// Method to rotate the vector around the Y axis (turn).

void
vector::rotate_y(float turn_angle)
{
	float temp_dx;

	//temp_dx = dx * cosine[turn_angle] + dz * sine[turn_angle];
	//dz = dz * cosine[turn_angle] - dx * sine[turn_angle];
	temp_dx = (float)(dx * cos(turn_angle) + dz * sin(turn_angle));
	dz = (float)(dz * cos(turn_angle) - dx * sin(turn_angle));
	dx = temp_dx;
}

//==============================================================================
// Polygon rasterisation classes.
//==============================================================================

//------------------------------------------------------------------------------
// Screen polygon class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

spolygon::spolygon()
{
	spoint_list = NULL;
	next_spolygon_ptr = NULL;
}

// Default destructor deletes the screen point list, if it exists.

spolygon::~spolygon()
{
	if (spoint_list != NULL)
		DELARRAY(spoint_list, spoint, max_spoints);
}

// Method to create the screen point list.

bool 
spolygon::create_spoint_list(int set_spoints)
{
	max_spoints = set_spoints;
	NEWARRAY(spoint_list, spoint, max_spoints);
	return(spoint_list != NULL);
}

//------------------------------------------------------------------------------
// Span class.
//------------------------------------------------------------------------------

// Default constructor initialises the next span pointer field to NULL.

span::span()
{
	next_span_ptr = NULL;
}

// Default destructor does nothing.

span::~span()
{
}

// Method to adjust the starting values of the span so they are valid for the
// given screen x coordinate.

void
span::adjust_start(int new_start_sx)
{
	int delta_sx = new_start_sx - start_sx;
	start_sx = new_start_sx;
	start_span.one_on_tz += delta_span.one_on_tz * delta_sx;
	start_span.u_on_tz += delta_span.u_on_tz * delta_sx;
	start_span.v_on_tz += delta_span.v_on_tz * delta_sx;
}

// Method to determine if the given span is in front or behind this span, by
// comparing the 1/tz values at the leftmost or rightmost overlap points.

bool
span::span_in_front(span *span_ptr)
{
	float our_one_on_tz, their_one_on_tz;

	// If the two spans don't meet at a common start point, compute 1/tz for
	// both spans at the leftmost overlap point.

	if (start_sx != span_ptr->start_sx || 
		FNE(start_span.one_on_tz, span_ptr->start_span.one_on_tz)) {

		// Determine which span's start point is the leftmost overlap point,
		// and calculate 1/tz for the other span at this point.

		if (start_sx > span_ptr->start_sx) {
			our_one_on_tz = start_span.one_on_tz;
			their_one_on_tz = span_ptr->start_span.one_on_tz +
				span_ptr->delta_span.one_on_tz * (start_sx - span_ptr->start_sx);
		} else {
			our_one_on_tz = start_span.one_on_tz + delta_span.one_on_tz *
				(span_ptr->start_sx - start_sx);
			their_one_on_tz = span_ptr->start_span.one_on_tz;
		}
	} 
	
	// If the two spans do meet at a common start point, compute 1/tz for both
	// spans at the rightmost overlap point.
	
	else {

		// Determine which span's end point is the rightmost overlap point,
		// and calculate 1/tz for the other span at this point.  Then determine
		// which span is in front.

		if (end_sx < span_ptr->start_sx) {
			our_one_on_tz = start_span.one_on_tz + delta_span.one_on_tz *
				(end_sx - start_sx);
			their_one_on_tz = span_ptr->start_span.one_on_tz +
				span_ptr->delta_span.one_on_tz * (end_sx - span_ptr->start_sx);
		} else {
			our_one_on_tz = start_span.one_on_tz + delta_span.one_on_tz *
				(span_ptr->end_sx - start_sx);
			their_one_on_tz = span_ptr->start_span.one_on_tz +
				span_ptr->delta_span.one_on_tz * 
				(span_ptr->end_sx - span_ptr->start_sx);
		}
	}

	// Determine which span is in front at the chosen overlap point.

	return(their_one_on_tz > our_one_on_tz);
}

//------------------------------------------------------------------------------
// Span row class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

span_row::span_row()
{
	opaque_span_list = NULL;
	transparent_span_list = NULL;
}

// Default destructor does nothing.

span_row::~span_row()
{
}

//------------------------------------------------------------------------------
// Span buffer class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

span_buffer::span_buffer()
{
	rows = 0;
	buffer_ptr = NULL;
}

// Default destructor deletes the span buffer.

span_buffer::~span_buffer()
{
	if (buffer_ptr != NULL)
		DELARRAY(buffer_ptr, span_row, rows);
}

// Method to create the span buffer with the given number of rows.

bool
span_buffer::create_buffer(int set_rows)
{
	rows = set_rows;
	NEWARRAY(buffer_ptr, span_row, rows);
	if (buffer_ptr == NULL)
		return(false);
	return(true);
}

// Method to return the pointer to a span buffer row.

span_row *
span_buffer::operator[](int row)
{
	return(&buffer_ptr[row]);
}

//==============================================================================
// Texture cache classes.
//==============================================================================

//------------------------------------------------------------------------------
// Texture cache entry.
//------------------------------------------------------------------------------

// Default constructor initialise the cache entry.

cache_entry::cache_entry()
{
	pixmap_ptr = NULL;
	frame_no = -1;
	lit_image_ptr = NULL;
	hardware_texture_ptr = NULL;
	next_cache_entry_ptr = NULL;
}

// Default destructor deletes the image buffer and hardware texture, if they
// exist, and resets the cache entry pointer in the associated pixmap, if it
// exists.

cache_entry::~cache_entry()
{
	if (lit_image_ptr != NULL)
		DELBASEARRAY(lit_image_ptr, cachebyte, lit_image_size);
	if (hardware_texture_ptr != NULL)
		hardware_destroy_texture(hardware_texture_ptr);
	if (pixmap_ptr != NULL)
		pixmap_ptr->cache_entry_list[brightness_index] = NULL;
}

// Method to create the image buffer if software rendering is enabled, or the
// hardware texture if hardware acceleration is enabled.

bool
cache_entry::create_image_buffer(int image_size_index)
{
	int image_dimensions;
	
	// Make sure the image size index is valid.

	if (image_size_index < 0 || image_size_index >= IMAGE_SIZES)
		return(false);

	// If hardware acceleration is enabled, create the hardware texture.

	if (hardware_acceleration) {
		if ((hardware_texture_ptr = hardware_create_texture(image_size_index)) 
			== NULL)
			return(false);
	} 
	
	// If software rendering is enabled, create the image buffer.

	else {
		image_dimensions = image_dimensions_list[image_size_index];
		if (display_depth <= 16)
			lit_image_size = image_dimensions * image_dimensions * 2;
		else
			lit_image_size = image_dimensions * image_dimensions * 4;
		NEWARRAY(lit_image_ptr, cachebyte, lit_image_size);
		if (lit_image_ptr == NULL)
			return(false);
	}

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Texture cache.
//------------------------------------------------------------------------------

// Default constructor initialises the cache entry list.

cache::cache()
{
	image_size_index = 0;
	cache_entry_list = NULL;
}

// Constructor to initialis the cache entry list and set a image size index.

cache::cache(int size_index)
{
	image_size_index = size_index;
	cache_entry_list = NULL;
}

// Default destructor deletes the cache entries.

cache::~cache()
{
	cache_entry *cache_entry_ptr = cache_entry_list;
	while (cache_entry_ptr != NULL) {
		cache_entry *next_cache_entry_ptr = 
			cache_entry_ptr->next_cache_entry_ptr;
		DEL(cache_entry_ptr, cache_entry);
		cache_entry_ptr = next_cache_entry_ptr;
	}
	cache_entry_list = NULL;

}

// Method to add a new cache entry to the list, and to return a pointer to it.

cache_entry *
cache::add_cache_entry(void)
{
	cache_entry *cache_entry_ptr;

	// Create the cache entry.
		
	NEW(cache_entry_ptr, cache_entry);
	if (cache_entry_ptr == NULL)
		return(NULL);

	// Create an image buffer for this cache entry.

	if (!cache_entry_ptr->create_image_buffer(image_size_index)) {
		DEL(cache_entry_ptr, cache_entry);
		return(NULL);
	}

	// Add the cache entry to the head of the list.

	cache_entry_ptr->next_cache_entry_ptr = cache_entry_list;
	cache_entry_list = cache_entry_ptr;
	return(cache_entry_ptr);
}

//==============================================================================
// Texture map classes.
//==============================================================================

//------------------------------------------------------------------------------
// Pixmap class.
//------------------------------------------------------------------------------

// Default constructor initialises the image pointer, cache entry list, span
// lists, image updated list, and polygon definition list.

pixmap::pixmap()
{
	image_ptr = NULL;
	transparent_index = -1;
	delay_ms = 0;
	for (int index = 0; index < BRIGHTNESS_LEVELS; index++) {
		cache_entry_list[index] = NULL;
		span_lists[index] = NULL;
#ifdef STREAMING_MEDIA
		image_updated[index] = false;
#endif
	}
	spolygon_list = NULL;
}

// Default destructor deletes the image buffer and cache entries.

pixmap::~pixmap()
{
	if (image_ptr != NULL)
		DELBASEARRAY(image_ptr, imagebyte, image_size);
	for (int index = 0; index < BRIGHTNESS_LEVELS; index++) {
		cache_entry *cache_entry_ptr = cache_entry_list[index];
		if (cache_entry_ptr != NULL)
			cache_entry_ptr->pixmap_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// Texture class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

texture::texture()
{
	pixmaps = 0;
	pixmap_list = NULL;
	colours = 0;
	brightness_levels = 0;
	RGB_palette = NULL;
	display_palette_list = NULL;
	texture_palette_list = NULL;
	palette_index_table = NULL;
	next_texture_ptr = NULL;
}

// Default destructor deletes the pixmap list, RGB palette, palette list and
// palette index table, if they exist.

texture::~texture()
{
	if (pixmap_list != NULL)
		DELARRAY(pixmap_list, pixmap, pixmaps);
	if (RGB_palette != NULL)
		DELARRAY(RGB_palette, RGBcolour, colours);
	if (display_palette_list != NULL)
		DELBASEARRAY(display_palette_list, pixel, brightness_levels * colours);
	if (texture_palette_list != NULL)
		DELBASEARRAY(texture_palette_list, pixel, brightness_levels * colours);
	if (palette_index_table != NULL)
		DELBASEARRAY(palette_index_table, byte, 256);
}

// Method to create and initialise the RGB palette.

bool
texture::create_RGB_palette(int set_colours, int set_brightness_levels,
							RGBcolour *set_RGB_palette)
{
	int index;
	int pixmap_no;

	// Set the number of colours and brightness levels this texture will
	// support.

	colours = set_colours;
	brightness_levels = set_brightness_levels;

	// Store the number of colours in each pixmap, for easier access.

	for (pixmap_no = 0; pixmap_no < pixmaps; pixmap_no++)
		pixmap_list[pixmap_no].colours = colours;

	// Create the new RGB palette.

	NEWARRAY(RGB_palette, RGBcolour, colours);
	if (RGB_palette == NULL)
		return(false);

	// Initialise the RGB palette.

	for (index = 0; index < colours; index++)
		RGB_palette[index] = set_RGB_palette[index];
	return(true);
}

// Method to create and initialise the display palette list.

bool
texture::create_display_palette_list(void)
{
	int pixmap_no;
	int level, index;
	float brightness;
	pixel *palette_ptr;

	// If the display palette list does not exist, create it, and store it's
	// pointer in each pixmap.

	if (display_palette_list == NULL) {
		NEWARRAY(display_palette_list, pixel, brightness_levels * colours);
		if (display_palette_list == NULL)
			return(false);
		for (pixmap_no = 0; pixmap_no < pixmaps; pixmap_no++)
			pixmap_list[pixmap_no].display_palette_list = display_palette_list;
	}

	// Step through the brightness levels.

	for (level = 0; level < brightness_levels; level++) {

		// Choose a brightness factor for this brightness level.

		if (brightness_levels > 1)
			brightness = (float)((brightness_levels - 1) - level) /
				(float)(brightness_levels - 1);
		else
			brightness = 1.0f;

		// Initialise the display palette for this brightness level.

		palette_ptr = display_palette_list + level * colours;
		for (index = 0; index < colours; index++) {
			RGBcolour colour;

			colour = RGB_palette[index];
			colour.adjust_brightness(brightness);
			palette_ptr[index] = RGB_to_display_pixel(colour);
		}
	}
	return(true);
}

// Method to create and initialise the texture palette list.

bool
texture::create_texture_palette_list(void)
{
	int pixmap_no;
	int level, index;
	float brightness;
	pixel *palette_ptr;

	// If the texture palette list does not exist, create it, and store it's
	// pointer in each pixmap.

	if (texture_palette_list == NULL) {
		NEWARRAY(texture_palette_list, pixel, brightness_levels * colours);
		if (texture_palette_list == NULL)
			return(false);
		for (pixmap_no = 0; pixmap_no < pixmaps; pixmap_no++)
			pixmap_list[pixmap_no].texture_palette_list = texture_palette_list;
	}

	// Step through the brightness levels.

	for (level = 0; level < brightness_levels; level++) {

		// Choose a brightness factor for this brightness level.

		if (brightness_levels > 1)
			brightness = (float)((brightness_levels - 1) - level) /
				(float)(brightness_levels - 1);
		else
			brightness = 1.0f;

		// Initialise the texture palette for this brightness level.

		palette_ptr = texture_palette_list + level * colours;
		for (index = 0; index < colours; index++) {
			RGBcolour colour;

			colour = RGB_palette[index];
			colour.adjust_brightness(brightness);
			palette_ptr[index] = RGB_to_texture_pixel(colour);
		}
	}
	return(true);
}

// Method to create a palette index table that will remap texels to the
// nearest colours in the standard RGB palette.

bool
texture::create_palette_index_table(void)
{
	int pixmap_no;
	int index;

	// Create the palette index table if it hasn't already been created, and
	// store it's pointer in each pixmap.

	if (palette_index_table == NULL) {
		NEWARRAY(palette_index_table, byte, 256);
		if (palette_index_table == NULL)
			return(false);
		for (pixmap_no = 0; pixmap_no < pixmaps; pixmap_no++)
			pixmap_list[pixmap_no].palette_index_table = palette_index_table;
	}

	// Step through the texture's RGB palette, and find the indices of the
	// nearest colours in the remapped RGB palette.

	for (index = 0; index < colours; index++)
		palette_index_table[index] = 
			get_standard_palette_index(RGB_palette[index]);
	return(true);
}

// Method to return a pointer to the current pixmap, based upon the elapsed
// time and the delay times specified in each pixmap.

pixmap *
texture::get_curr_pixmap_ptr(int elapsed_time_ms)
{
	int pixmap_no;
	pixmap *pixmap_ptr;
	int max_time_ms;

	// If this texture has only one pixmap, just return it.

	if (pixmaps == 1)
		return(pixmap_list);

	// If this is a looping texture, adjust the elapsed time so that it wraps
	// approapiately.

	if (loops)
		elapsed_time_ms = elapsed_time_ms % total_time_ms;

	// Step through the list of pixmaps, looking for the one that would be
	// displayed at this particular elapsed time.  If the elapsed time exceeds
	// the total animation time, the last pixmap will be selected.

	max_time_ms = 0;
	for (pixmap_no = 0; pixmap_no < pixmaps; pixmap_no++) {
		pixmap_ptr = &pixmap_list[pixmap_no];
		max_time_ms += pixmap_ptr->delay_ms;
		if (elapsed_time_ms < max_time_ms)
			break;
	}
	return(pixmap_ptr);
}

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Video texture class.
//------------------------------------------------------------------------------

// Default constructor initialises texture pointer and next video texture
// pointer.

video_texture::video_texture()
{
	texture_ptr = NULL;
	next_video_texture_ptr = NULL;
}

// Default destructor deletes texture, if it exists.

video_texture::~video_texture()
{
	if (texture_ptr != NULL)
		DEL(texture_ptr, texture);
}

#endif

//==============================================================================
// Special object classes.
//==============================================================================

//------------------------------------------------------------------------------
// Light class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

light::light()
{
	style = STATIC_POINT_LIGHT;
	position.set(UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK, 
		UNITS_PER_HALF_BLOCK);
	dir.set(0.0f, 0.0f, 1.0f);
	colour.set_RGB(255,255,255);
	intensity_range.min_percentage = 1.0f;
	intensity_range.max_percentage = 1.0f;
	intensity = 1.0f;
	delta_intensity = 0.0f;
	lit_colour.set_RGB(255,255,255);
	set_radius(1.0f);
	set_cone_angle(45.0f);
	flood = false;
	next_light_ptr = NULL;
}

// Default destructor does nothing.

light::~light()
{
}

// Method to set the light direction.  Starting with a north-pointing vector,
// it is rotated around the X axis first (tilt) then around the Y axis (turn).
// The tilt is anti-clockwise so that a positive angle generates an upward tilt.

void
light::set_direction(direction light_direction)
{
	curr_dir = light_direction;
	dir.set(0.0, 0.0, 1.0);
	dir.rotate_x(-curr_dir.angle_x);
	dir.rotate_y(curr_dir.angle_y);
}

// Method to set the light direction range.

void
light::set_dir_range(dirrange light_dir_range)
{
	dir_range = light_dir_range;
	set_direction(dir_range.min_direction);
}

// Method to set the delta direction from the light speed (in cycles per second).
// set_dir_range() must be called before this function is called.

void
light::set_dir_speed(float light_speed)
{
	float angle_x_range, angle_y_range, angle_h_range;
	float degrees_per_second;
	float temp_angle;

	// Determine the range of angles around the X and Y axes, and from this
	// compute the degrees travelled around the sphere.  If this is zero,
	// the direction speed is set to zero.

	angle_x_range = dir_range.max_direction.angle_x - 
		dir_range.min_direction.angle_x;
	angle_y_range = dir_range.max_direction.angle_y -
		dir_range.min_direction.angle_y;
	angle_h_range = (float)sqrt(angle_x_range * angle_x_range + 
		angle_y_range * angle_y_range);
	if (FEQ(angle_h_range, 0.0f)) {
		delta_dir.angle_x = 0.0f;
		delta_dir.angle_y = 0.0f;
		return;
	}

	// Compute the degrees to travel per second.

	if (style == REVOLVING_SPOT_LIGHT)
		degrees_per_second = 360.0f * light_speed;
	else
		degrees_per_second = angle_h_range * 2.0f * light_speed;

	// Compute the degrees to travel around the X and Y axes per second.

	delta_dir.angle_x = angle_x_range / angle_h_range * degrees_per_second;
	delta_dir.angle_y = angle_y_range / angle_h_range * degrees_per_second;

	// If the minimum and maximum angles need to be swapped, do so now that the
	// delta angles have been calculated.

	if (FGT(dir_range.min_direction.angle_x, dir_range.max_direction.angle_x)) {
		temp_angle = dir_range.min_direction.angle_x;
		dir_range.min_direction.angle_x = dir_range.max_direction.angle_x;
		dir_range.max_direction.angle_x = temp_angle;
	}
	if (FGT(dir_range.min_direction.angle_y, dir_range.max_direction.angle_y)) {
		temp_angle = dir_range.min_direction.angle_y;
		dir_range.min_direction.angle_y = dir_range.max_direction.angle_y;
		dir_range.max_direction.angle_y = temp_angle;
	}
}

// Method to set the light intensity, and to generate a lit colour.

void 
light::set_intensity(float light_intensity)
{
	if (FLT(light_intensity, 0.0f))
		light_intensity = 0.0f;
	else if (FGT(light_intensity, 1.0f))
		light_intensity = 1.0f;
	intensity = light_intensity;
	lit_colour.red = colour.red * intensity;
	lit_colour.green = colour.green * intensity;
	lit_colour.blue = colour.blue * intensity;
}

// Method to set the light intensity range.

void
light::set_intensity_range(pcrange light_intensity_range)
{
	intensity_range = light_intensity_range;
	set_intensity(intensity_range.min_percentage);
}

// Method to compute the delta intensity from the light speed (in cycles per
// second).

void
light::set_intensity_speed(float light_speed)
{
	delta_intensity = (intensity_range.max_percentage - 
		intensity_range.min_percentage) * 2.0f * light_speed;
}

// Method to set the light radius, and 1 / radius.

void
light::set_radius(float light_radius)
{
	radius = light_radius;
	one_on_radius = 1.0f / radius;
}

// Method to set the cone angle, and compute associated values.

void
light::set_cone_angle(float light_cone_angle)
{
	cos_cone_angle = cosine[light_cone_angle];
	cone_angle_M = 1.0f / (1.0f - cos_cone_angle);
}

//------------------------------------------------------------------------------
// Wave class.
//------------------------------------------------------------------------------

// Default constructor initialises some pointers.

wave::wave()
{
	format_ptr = NULL;
	data_ptr = NULL;
	next_wave_ptr = NULL;
}

// Default destructor deletes the wave format and data, if they exist.

wave::~wave()
{
	destroy_wave_data(this);
}

//------------------------------------------------------------------------------
// Sound class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

sound::sound()
{
	ambient = false;
	wave_ptr = NULL;
	sound_buffer_ptr = NULL;
	position.x = UNITS_PER_HALF_BLOCK;
	position.y = UNITS_PER_HALF_BLOCK;
	position.z = UNITS_PER_HALF_BLOCK;
	volume = 1.0f;
	radius = 0.0f;
	playback_mode = LOOPED_PLAY;
	flood = false;
	rolloff = 1.0f;
	reflections = true;
	in_range = false;
	played_once = false;
	next_sound_ptr = NULL;
}

// Default destructor deletes the sound buffer, if it exists.

sound::~sound()
{
	destroy_sound_buffer(this);
}

//------------------------------------------------------------------------------
// Entrance class.
//------------------------------------------------------------------------------

// Default constructor initialises some pointers.

entrance::entrance()
{
	square_ptr = NULL;
	next_entrance_ptr = NULL;
}

// Default destructor does nothing.

entrance::~entrance()
{
}

//------------------------------------------------------------------------------
// Trigger class.
//------------------------------------------------------------------------------

// Default constructor initialises the trigger flag, action list and next
// trigger pointer.

trigger::trigger()
{
	trigger_flag = CLICK_ON;
	position.set(UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK, 
		UNITS_PER_HALF_BLOCK);
	square_ptr = NULL;
	block_ptr = NULL;
	action_list = NULL;
//POWERS	script_def_ptr = NULL;
	next_trigger_ptr = NULL;
}

// Default destructor deletes action list, if there is one.

trigger::~trigger()
{
	action *next_action_ptr;

	while (action_list != NULL) {
		next_action_ptr = action_list->next_action_ptr;
		if (action_list->type == RIPPLE_ACTION){
			if (action_list->curr_step_ptr != NULL)
				DELARRAY(action_list->curr_step_ptr,float,action_list->vertices);
			if (action_list->prev_step_ptr != NULL)
				DELARRAY(action_list->prev_step_ptr,float,action_list->vertices);
		}
		DEL(action_list, action);
		action_list = next_action_ptr;
	}
}

//------------------------------------------------------------------------------
// Hyperlink class.
//------------------------------------------------------------------------------

// Default constructor set "click on" trigger flag.

hyperlink::hyperlink()
{
	trigger_flags = CLICK_ON;
	is_spot_URL = false;
	next_hyperlink_ptr = NULL;
}

// Default destructor does nothing.

hyperlink::~hyperlink()
{
}

//------------------------------------------------------------------------------
// Area class.
//------------------------------------------------------------------------------

// Default constructor initialises all pointers.

area::area()
{
	exit_ptr = NULL;
	trigger_flags = 0;
	trigger_list = NULL;
	next_area_ptr = NULL;
}

// Default destructor deletes the exit and trigger list, if they exist.

area::~area()
{
	trigger *next_trigger_ptr;

	if (exit_ptr != NULL)
		DEL(exit_ptr, hyperlink);
	if (trigger_list != NULL) {
		next_trigger_ptr = trigger_list->next_trigger_ptr;
		DEL(trigger_list, trigger);
		trigger_list = next_trigger_ptr;
	}
}

//------------------------------------------------------------------------------
// Imagemap class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

imagemap::imagemap()
{
	area_list = NULL;
	next_imagemap_ptr = NULL;
}

// Default destructor deletes the list of areas.

imagemap::~imagemap()
{
	while (area_list != NULL) {
		area *next_area_ptr = area_list->next_area_ptr;
		DEL(area_list, area);
		area_list = next_area_ptr;
	}
}

// Method to find the given rectangular area.

area *
imagemap::find_area(rect *rect_ptr)
{
	area *area_ptr = area_list;
	while (area_ptr != NULL) {
		if (area_ptr->shape_type == RECT_SHAPE && 
			area_ptr->rect_shape.x1 == rect_ptr->x1 &&
			area_ptr->rect_shape.y1 == rect_ptr->y1 &&
			area_ptr->rect_shape.x2 == rect_ptr->x2 &&
			area_ptr->rect_shape.y2 == rect_ptr->y2)
			return(area_ptr);
		area_ptr = area_ptr->next_area_ptr;
	}
	return(NULL);
}

// Method to find the given circular area.

area *
imagemap::find_area(circle *circle_ptr)
{
	area *area_ptr = area_list;
	while (area_ptr != NULL) {
		if (area_ptr->shape_type == CIRCLE_SHAPE && 
			area_ptr->circle_shape.x == circle_ptr->x &&
			area_ptr->circle_shape.y == circle_ptr->y &&
			area_ptr->circle_shape.r == circle_ptr->r)
			return(area_ptr);
		area_ptr = area_ptr->next_area_ptr;
	}
	return(NULL);
}

// Method to add a rectangular area with an exit to the area list.

bool
imagemap::add_area(rect *rect_ptr, hyperlink *exit_ptr)
{
	area *area_ptr;

	// If the area already exists and has an exit, do nothing.

	if ((area_ptr = find_area(rect_ptr)) != NULL) {
		if (area_ptr->exit_ptr != NULL)
			return(false);
	}

	// If the area doesn't exist, create it and add it to the head of the
	// area list.

	else {
		NEW(area_ptr, area);
		if (area_ptr == NULL)
			return(false);
		area_ptr->shape_type = RECT_SHAPE;
		area_ptr->rect_shape = *rect_ptr;
		area_ptr->next_area_ptr = area_list;
		area_list = area_ptr;
	}

	// Initialise the exit pointer.

	area_ptr->exit_ptr = exit_ptr;
	return(true);
}

// Method to add a rectangular area with a trigger to the area list.

bool
imagemap::add_area(rect *rect_ptr, trigger *trigger_ptr)
{
	area *area_ptr;

	// If the area doesn't already exist, create a new area and add it to the
	// head of the area list.

	if ((area_ptr = find_area(rect_ptr)) == NULL) {
		NEW(area_ptr, area);
		if (area_ptr == NULL)
			return(false);
		area_ptr->shape_type = RECT_SHAPE;
		area_ptr->rect_shape = *rect_ptr;
		area_ptr->next_area_ptr = area_list;
		area_list = area_ptr;
	}

	// Update the area's trigger flags and add the trigger to the head of the
	// area's trigger list.

	area_ptr->trigger_flags |= trigger_ptr->trigger_flag;
	trigger_ptr->next_trigger_ptr = area_ptr->trigger_list;
	area_ptr->trigger_list = trigger_ptr;
	return(true);
}

// Method to add a circular area with an exit to the area list.

bool
imagemap::add_area(circle *circle_ptr, hyperlink *exit_ptr)
{
	area *area_ptr;

	// If the area already exists and has an exit, do nothing.

	if ((area_ptr = find_area(circle_ptr)) != NULL) {
		if (area_ptr->exit_ptr != NULL)
			return(false);
	}

	// If the area doesn't exist, create it and add it to the head of the
	// area list.

	else {
		NEW(area_ptr, area);
		if (area_ptr == NULL)
			return(false);
		area_ptr->shape_type = CIRCLE_SHAPE;
		area_ptr->circle_shape = *circle_ptr;
		area_ptr->next_area_ptr = area_list;
		area_list = area_ptr;
	}

	// Initialise the exit pointer.

	area_ptr->exit_ptr = exit_ptr;
	return(true);
}

// Method to add a circular area with a trigger to the area list.

bool
imagemap::add_area(circle *circle_ptr, trigger *trigger_ptr)
{
	area *area_ptr;

	// If the area doesn't already exist, create a new area and add it to the
	// head of the area list.

	if ((area_ptr = find_area(circle_ptr)) == NULL) {
		NEW(area_ptr, area);
		if (area_ptr == NULL)
			return(false);
		area_ptr->shape_type = CIRCLE_SHAPE;
		area_ptr->circle_shape = *circle_ptr;
		area_ptr->next_area_ptr = area_list;
		area_list = area_ptr;
	}

	// Update the area's trigger flags and add the trigger to the head of the
	// area's trigger list.

	area_ptr->trigger_flags |= trigger_ptr->trigger_flag;
	trigger_ptr->next_trigger_ptr = area_ptr->trigger_list;
	area_ptr->trigger_list = trigger_ptr;
	return(true);
}

//------------------------------------------------------------------------------
// Popup class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

popup::popup()
{
	trigger_flags = PROXIMITY;
	visible_flags = 0;
	always_visible = false;
	position.x = UNITS_PER_HALF_BLOCK;
	position.y = UNITS_PER_HALF_BLOCK;
	position.z = UNITS_PER_HALF_BLOCK;
	window_alignment = CENTRE;
	radius_squared = UNITS_PER_BLOCK * UNITS_PER_BLOCK;
	brightness = 1.0f;
	brightness_index = 0;
	colour.set_RGB(0, 0, 0);
	bg_texture_ptr = NULL;
	fg_texture_ptr = NULL;
	transparent_background = true;
	width = 320;
	height = 240;
	text_colour.set_RGB(255, 255, 255);
	text_alignment = CENTRE;
	imagemap_ptr = NULL;
	next_popup_ptr = NULL;
	next_square_popup_ptr = NULL;
}

// Default destructor does nothing.

popup::~popup()
{
}

//==============================================================================
// Block definition and block set classes.
//==============================================================================

//------------------------------------------------------------------------------
// Part class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

part::part()
{
	texture_ptr = NULL;
	custom_texture_ptr = NULL;
	colour_set = false;
	colour.set_RGB(0.0f, 0.0f, 0.0f);
	normalised_colour.set_RGB(0.0f, 0.0f, 0.0f);
	alpha = 1.0f;
	texture_style = STRETCHED_TEXTURE;
	texture_angle = 0.0f;
	faces = 1;
	projection = NONE;
	solid = true;
    trigger_flags = 0;
}

// Default destructor does nothing.

part::~part()
{
}

//------------------------------------------------------------------------------
// Vertex definition class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

vertex_def::vertex_def()
{
	vertex_no = 0;
	u = 0.0f;
	v = 0.0f;
	orig_u = 0.0f;
	orig_v = 0.0f;
}

// Default destructor does nothing.

vertex_def::~vertex_def()
{
}

//------------------------------------------------------------------------------
// Polygon definition class.
//------------------------------------------------------------------------------

// Default constructor initialises the vertex definition list, and BSP data.

polygon_def::polygon_def()
{
	vertices = 0;
	vertex_def_list = NULL;
	front_polygon_ref = 0;
	rear_polygon_ref = 0;
}

// Default destructor deletes the vertex definition list, if it was created.

polygon_def::~polygon_def()
{
	if (vertex_def_list != NULL)
		DELARRAY(vertex_def_list, vertex_def, vertices);
}

// Method to create the vertex definition list with the given size.

bool
polygon_def::create_vertex_def_list(int set_vertices)
{
	vertices = set_vertices;
	NEWARRAY(vertex_def_list, vertex_def, vertices);
	return(vertex_def_list != NULL);
}

// Method to compute the texture coordinates for each vertex based upon the
// following procedure:
// (1) Imagine the texture has been applied onto the outside face of
//	   the block's bounding box.  The face is the one the polygon is
//	   pointing towards.
// (2) Perform an orthographic projection to place the texture onto
//	   the polygon.
// (3) Rotate the texture coordinates by the desired angle.
//
// If the projection parameter is NONE, then the direction of the polygon's
// normal is used. 

void 
polygon_def::project_texture(vertex *vertex_list, int projection)
{
	int index;

	// Step through the vertex list.

	for (index = 0; index < vertices; index++) {
		vertex *vertex_ptr;
		vertex_def *vertex_def_ptr;
		float u, v;

		// If the projection is NONE, use the direction of the polygon instead.

		if (projection == NONE) {

			// Get pointers to the first three vertices.

			vertex *vertex1_ptr = VERTEX_PTR(0);
			vertex *vertex2_ptr = VERTEX_PTR(1);
			vertex *vertex3_ptr = VERTEX_PTR(2);
				
			// Compute the vectors leading from the middle vertex to the adjacent
			// vertices.
			
			vector A = *vertex1_ptr - *vertex2_ptr;
			vector B = *vertex3_ptr - *vertex2_ptr;

			// Compute the normal vector as B * A.  This is pointing away from
			// the front face.
			
			vector normal_vector = B * A;
			normal_vector.normalise();

			// Determine the direction the polygon is facing in by looking at
			// which component of the normal vector is largest, and it's sign.

			if (FGE(FABS(normal_vector.dx), FABS(normal_vector.dy)) &&
				FGE(FABS(normal_vector.dx), FABS(normal_vector.dz))) {
				if (FLT(normal_vector.dx, 0.0))
					projection = WEST;
				else
					projection = EAST;
			}
			if (FGE(FABS(normal_vector.dy), FABS(normal_vector.dx)) &&
				FGE(FABS(normal_vector.dy), FABS(normal_vector.dz))) {
				if (FLT(normal_vector.dy, 0.0))
					projection = DOWN;
				else
					projection = UP;
			}
			if (FGE(FABS(normal_vector.dz), FABS(normal_vector.dx)) &&
				FGE(FABS(normal_vector.dz), FABS(normal_vector.dy))) {
				if (FLT(normal_vector.dz, 0.0))
					projection = SOUTH;
				else
					projection = NORTH;
			}
		}

		// Perform the orthogonal projection.

		vertex_ptr = VERTEX_PTR(index);
		switch (projection) {
		case UP:
			u = vertex_ptr->x;
			v = UNITS_PER_BLOCK - vertex_ptr->z;
			break;
		case DOWN:
			u = vertex_ptr->x;
			v = vertex_ptr->z;
			break;
		case NORTH:
			u = UNITS_PER_BLOCK - vertex_ptr->x;
			v = UNITS_PER_BLOCK - vertex_ptr->y;
			break;
		case SOUTH:
			u = vertex_ptr->x;
			v = UNITS_PER_BLOCK - vertex_ptr->y;
			break;
		case EAST:
			u = vertex_ptr->z;
			v = UNITS_PER_BLOCK - vertex_ptr->y;
			break;
		case WEST:
			u = UNITS_PER_BLOCK - vertex_ptr->z;
			v = UNITS_PER_BLOCK - vertex_ptr->y;
		}

		// Save the texture coordinates in the vertex definition
		// structure, converted to normalised coordinates between 0 and 1.

		vertex_def_ptr = &vertex_def_list[index];
		vertex_def_ptr->u = u / UNITS_PER_BLOCK;
		vertex_def_ptr->v = v / UNITS_PER_BLOCK;
	}
}

// Assignment operator for correctly duplicating a polygon.

polygon_def &
polygon_def::operator=(const polygon_def &old_polygon_def)
{
	// Make copies of the simple fields.

	part_no = old_polygon_def.part_no;
	vertices = old_polygon_def.vertices;
	front_polygon_ref = old_polygon_def.front_polygon_ref;
	rear_polygon_ref = old_polygon_def.rear_polygon_ref;

	// Make a copy of the vertex definition list.

	create_vertex_def_list(vertices);
	for (int index = 0; index < vertices; index++)
		vertex_def_list[index] = old_polygon_def.vertex_def_list[index];

	// Return this polygon.

	return(*this);
}

//------------------------------------------------------------------------------
// Polygon definition class.
//------------------------------------------------------------------------------

// Default constructor does nothing.

polygon::polygon()
{
}

// Default destructor does nothing.

polygon::~polygon()
{
}

// Method to compute the centroid of the polygon by averaging the components of
// it's vertices.

void
polygon::compute_centroid(vertex *vertex_list)
{	
	int vertices;
	int vertex_no;
	
	centroid.x = 0.0;
	centroid.y = 0.0;
	centroid.z = 0.0;
	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
	vertices = polygon_def_ptr->vertices;
	for (vertex_no = 0; vertex_no < vertices; vertex_no++)
		centroid = centroid + VERTEX(vertex_no);
	centroid = centroid / (float)vertices;
}

// Method to compute the normal vector.  This method also sets the polygon's
// direction and side flag.

void
polygon::compute_normal_vector(vertex *vertex_list)
{
	vector A, B;
	int vertices;
	int vertex_no;

	// Get pointers to the first three vertices.

	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
	vertex *vertex1_ptr = VERTEX_PTR(0);
	vertex *vertex2_ptr = VERTEX_PTR(1);
	vertex *vertex3_ptr = VERTEX_PTR(2);
				
	// Compute the vectors leading from the middle vertex to the adjacent
	// vertices.
	
	A = *vertex1_ptr - *vertex2_ptr;
	B = *vertex3_ptr - *vertex2_ptr;

	// Compute the normal vector as B * A.  This is pointing away from the
	// front face.
	
	normal_vector = B * A;
	normal_vector.normalise();

	// Determine the direction the polygon is facing in by looking at which
	// component of the normal vector is largest, and it's sign.

	if (FGE(FABS(normal_vector.dx), FABS(normal_vector.dy)) &&
		FGE(FABS(normal_vector.dx), FABS(normal_vector.dz))) {
		if (FLT(normal_vector.dx, 0.0))
			direction = WEST;
		else
			direction = EAST;
	}
	if (FGE(FABS(normal_vector.dy), FABS(normal_vector.dx)) &&
		FGE(FABS(normal_vector.dy), FABS(normal_vector.dz))) {
		if (FLT(normal_vector.dy, 0.0))
			direction = DOWN;
		else
			direction = UP;
	}
	if (FGE(FABS(normal_vector.dz), FABS(normal_vector.dx)) &&
		FGE(FABS(normal_vector.dz), FABS(normal_vector.dy))) {
		if (FLT(normal_vector.dz, 0.0))
			direction = SOUTH;
		else
			direction = NORTH;
	}

	// Depending on which direction the polygon faces, determine whether
	// all of it's vertices are on the corresponding bounding box plane,
	// meaning it's a side polygon.

	side = true;
	vertices = polygon_def_ptr->vertices;
	switch (direction) {
	case NORTH:
		for (vertex_no = 0; vertex_no < vertices; vertex_no++)
			if (FNE((VERTEX_PTR(vertex_no))->z, UNITS_PER_BLOCK)) {
				side = false;
				break;
			}
		break;
	case SOUTH:
		for (vertex_no = 0; vertex_no < vertices; vertex_no++)
			if (FNE((VERTEX_PTR(vertex_no))->z, 0.0f)) {
				side = false;
				break;
			}
		break;
	case EAST:
		for (vertex_no = 0; vertex_no < vertices; vertex_no++)
			if (FNE((VERTEX_PTR(vertex_no))->x, UNITS_PER_BLOCK)) {
				side = false;
				break;
			}
		break;
	case WEST:
		for (vertex_no = 0; vertex_no < vertices; vertex_no++)
			if (FNE((VERTEX_PTR(vertex_no))->x, 0.0f)) {
				side = false;
				break;
			}
		break;
	case UP:
		for (vertex_no = 0; vertex_no < vertices; vertex_no++)
			if (FNE((VERTEX_PTR(vertex_no))->y, UNITS_PER_BLOCK)) {
				side = false;
				break;
			}
		break;
	case DOWN:
		for (vertex_no = 0; vertex_no < vertices; vertex_no++)
			if (FNE((VERTEX_PTR(vertex_no))->y, 0.0f)) {
				side = false;
				break;
			}
	}
}

// Method to compute the plane offset, which is D in the polygon's plane 
// equation Ax + By + Cz + D = 0.  We'll use the surface normal for (A,B,C) and
// vertex #1 for (x,y,z).

void
polygon::compute_plane_offset(vertex *vertex_list)
{
	PREPARE_VERTEX_DEF_LIST(polygon_def_ptr);
	vertex *vertex_ptr = VERTEX_PTR(0);
	plane_offset = -(normal_vector.dx * vertex_ptr->x + 
		normal_vector.dy * vertex_ptr->y + normal_vector.dz * vertex_ptr->z);
}

//------------------------------------------------------------------------------
// BSP node class.
//------------------------------------------------------------------------------

// Default constructor initalises all fields.

BSP_node::BSP_node()
{
	polygon_no = 0;
	front_node_ptr = NULL;
	rear_node_ptr = NULL;
}

// Default destructor deletes the front and rear nodes.

BSP_node::~BSP_node()
{
	if (front_node_ptr != NULL)
		DEL(front_node_ptr, BSP_node);
	if (rear_node_ptr != NULL)
		DEL(rear_node_ptr, BSP_node);
}

//------------------------------------------------------------------------------
// Block definition class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.
	
block_def::block_def()
{
	custom = false;
	type = STRUCTURAL_BLOCK;
	allow_entrance = false;
	physics = false;
	mass = 0.0;
	animated = false;
	animation = NULL;
	parts = 0;
	part_list = NULL;
	vertices = 0;
	vertex_list = NULL;
	polygons = 0;
	polygon_def_list = NULL;
	light_list = NULL;
	sound_list = NULL;
	popup_trigger_flags = 0;
	popup_list = NULL;
	last_popup_ptr = NULL;
	entrance_ptr = NULL;
	exit_ptr = NULL;
	trigger_flags = 0;
	trigger_list = NULL;
	free_block_list = NULL;
	used_block_list = NULL;
	root_polygon_ref = 0;
	BSP_tree = NULL;
	block_orientation.set(0.0f, 0.0f, 0.0f);
	block_origin.set(UNITS_PER_HALF_BLOCK, UNITS_PER_HALF_BLOCK,
		UNITS_PER_HALF_BLOCK);
	sprite_angle = 0.0;
	degrees_per_ms = 0.0;
	sprite_alignment = BOTTOM;
	sprite_size.width = 0;
	sprite_size.height = 0;
	solid = true;
	movable = false;
	next_block_def_ptr = NULL;
}	

// Default destructor deletes all allocated data.
	
block_def::~block_def()
{
	light *next_light_ptr;
	sound *next_sound_ptr;
	popup *next_popup_ptr;
	trigger *next_trigger_ptr;
	block *next_block_ptr;
	int index;

	// Delete the BSP tree if this is not a custom block.

	if (!custom && BSP_tree != NULL)
		DEL(BSP_tree, BSP_node);

	// Delete the vertex, part and polygon definition lists.

	if (animated && animation->original_frames) {
		vertex_list = NULL;
		for (index = 0; index < animation->frames; index++) {
			//if (animation->frames_list[index].vertices > 0)
			//	DELARRAY(animation->frames_list[index].vertex_list, vertex, animation->frames_list[index].vertices);
			if (animation->vertices[index] > 0)
				DELARRAY(animation->frame_list[index].vertex_list, vertex, animation->vertices[index]);

		}
		DELARRAY(animation->vertices, int, animation->frames);
		DELARRAY(animation->angles, float, animation->frames);
		DELARRAY(animation->frame_list, frame_def, animation->frames);
		if (animation->loops_list != NULL)
			DELARRAY(animation->loops_list, intrange, animation->loops);
		DEL(animation,animation_def);
	}
	if (vertex_list != NULL)
		DELARRAY(vertex_list, vertex, vertices);
	if (part_list != NULL)
		DELARRAY(part_list, part, parts);
	if (polygon_def_list != NULL)
		DELARRAY(polygon_def_list, polygon_def, polygons);

	// Delete the list of lights.

	while (light_list != NULL) {
		next_light_ptr = light_list->next_light_ptr;
		DEL(light_list, light);
		light_list = next_light_ptr;
	}

	// Delete the list of sounds.

	while (sound_list != NULL) {
		next_sound_ptr = sound_list->next_sound_ptr;
		DEL(sound_list, sound);
		sound_list = next_sound_ptr;
	}

	// Delete the list of popups.  If this is a custom block definition, the
	// foreground textures must also be deleted.

	while (popup_list != NULL) {
		next_popup_ptr = popup_list->next_popup_ptr;
		if (custom && popup_list->fg_texture_ptr != NULL)
			DEL(popup_list->fg_texture_ptr, texture);
		DEL(popup_list, popup);
		popup_list = next_popup_ptr;
	}

	// Delete the exit.

	if (exit_ptr != NULL)
		DEL(exit_ptr, hyperlink);

	// Delete the entrance.

	if (entrance_ptr != NULL)
		DEL(entrance_ptr, entrance);

	// Delete the list of triggers.

	while (trigger_list != NULL) {
		next_trigger_ptr = trigger_list->next_trigger_ptr;
		DEL(trigger_list, trigger);
		trigger_list = next_trigger_ptr;
	}

	// Delete the list of free blocks.

	while (free_block_list != NULL) {
		next_block_ptr = free_block_list->next_block_ptr;
		DEL(free_block_list, block);
		free_block_list = next_block_ptr;
	}
}

void
block_def::create_frames_list(int set_frames)
{
	animation->frames = set_frames;
	NEWARRAY(animation->vertices, int, set_frames);
	if (animation->vertices == NULL)
		memory_error("vertices list");
	NEWARRAY(animation->angles, float, set_frames);
	if (animation->angles == NULL)
		memory_error("angles list");
	NEWARRAY(animation->frame_list, frame_def, set_frames);
	if (animation->frame_list == NULL)
		memory_error("frame list");
	//NEWARRAY(animation->frames_list, frame_def, set_frames);
	//if (animation->frames_list == NULL)
	//	memory_error("frames list");
}

void
block_def::create_loops_list(int set_loops)
{
	animation->loops = set_loops;

	NEWARRAY(animation->loops_list, intrange, set_loops);
	if (animation->loops_list == NULL)
		memory_error("loops list");
}

// Methods to create vertex, part and polygon lists.

void
block_def::create_vertex_list(int set_vertices)
{
	vertices = set_vertices;
	if (vertices > 0) {
		NEWARRAY(vertex_list, vertex, vertices);
		if (vertex_list == NULL)
			memory_error("vertex list");
	}
}

void
block_def::create_part_list(int set_parts)
{
	parts = set_parts;
	if (parts > 0) {
		NEWARRAY(part_list, part, parts);
		if (part_list == NULL)
			memory_error("part list");
	}
}

void
block_def::create_polygon_def_list(int set_polygons)
{
	polygons = set_polygons;
	if (polygons > 0) {
		NEWARRAY(polygon_def_list, polygon_def, polygons);
		if (polygon_def_list == NULL)
			memory_error("polygon definition list");
	}
}

// Method for making a duplicate of a block definition.

void
block_def::dup_block_def(block_def *block_def_ptr)
{
	int index, j;
	light *old_light_ptr, *new_light_ptr;
	sound *old_sound_ptr, *new_sound_ptr;
	popup *old_popup_ptr, *new_popup_ptr;

	// Set the custom flag to true, but copy the name, type, entrance flag,
	// BSP tree pointer and solid flag, since they won't be changing.

	custom = true;
	name = block_def_ptr->name;
	type = block_def_ptr->type;
	allow_entrance = block_def_ptr->allow_entrance;
	BSP_tree = block_def_ptr->BSP_tree;
	solid = block_def_ptr->solid;
    physics = block_def_ptr->physics;
	mass = block_def_ptr->mass;
	position = block_def_ptr->position;

	// Copy the sprite angle, rotational speed, and alignment.

	sprite_angle = block_def_ptr->sprite_angle;
	degrees_per_ms = block_def_ptr->degrees_per_ms;
	sprite_alignment = block_def_ptr->sprite_alignment;

	// Make a copy of the parts list.

	create_part_list(block_def_ptr->parts);
	for (index = 0; index < parts; index++)
		part_list[index] = block_def_ptr->part_list[index];

	// Make a copy of the polygon definition list, and initialise the part 
	// pointer of each polygon definition.

	create_polygon_def_list(block_def_ptr->polygons);
	for (index = 0; index < polygons; index++) {
		polygon_def *polygon_def_ptr = &polygon_def_list[index];
		*polygon_def_ptr = block_def_ptr->polygon_def_list[index];
		polygon_def_ptr->part_ptr = &part_list[polygon_def_ptr->part_no];
	}

	// Make a copy of the light list, if it exists.

	old_light_ptr = block_def_ptr->light_list;
	while (old_light_ptr != NULL) {
		NEW(new_light_ptr, light);
		if (new_light_ptr == NULL)
			memory_error("light");
		*new_light_ptr = *old_light_ptr;
		new_light_ptr->next_light_ptr = light_list;
		light_list = new_light_ptr;
		old_light_ptr = old_light_ptr->next_light_ptr;
	}

	// Make a copy of the sound list, if one exists.

	old_sound_ptr = block_def_ptr->sound_list;
	while (old_sound_ptr != NULL) {
		NEW(new_sound_ptr, sound);
		if (new_sound_ptr == NULL)
			memory_error("sound");
		*new_sound_ptr = *old_sound_ptr;
		new_sound_ptr->next_sound_ptr = sound_list;
		sound_list = new_sound_ptr;
		old_sound_ptr = old_sound_ptr->next_sound_ptr;
	}

	// Make a copy of the popup list, if one exists.

	old_popup_ptr = block_def_ptr->popup_list;
	while (old_popup_ptr != NULL) {
		NEW(new_popup_ptr, popup);
		if (new_popup_ptr == NULL)
			memory_error("popup");
		*new_popup_ptr = *old_popup_ptr;
		new_popup_ptr->next_popup_ptr = popup_list;
		popup_list = new_popup_ptr;
		old_popup_ptr = old_popup_ptr->next_popup_ptr;
	}

	// Make a copy of the exit, if one exists.

	if (block_def_ptr->exit_ptr != NULL) {
		NEW(exit_ptr, hyperlink);
		if (exit_ptr == NULL)
			memory_error("exit");
		*exit_ptr = *block_def_ptr->exit_ptr;
	}

	// Copy the frame data even though it might be large

	if (block_def_ptr->animated) {
		NEW(animation,animation_def);
		if (animation == NULL)
			memory_error("animation_def");
		animated = true;
		*animation = *block_def_ptr->animation;
		// copy the frame list with the sets of vertices
		create_frames_list(block_def_ptr->animation->frames);
		for (index=0; index < animation->frames; index++) {
			animation->angles[index]   = block_def_ptr->animation->angles[index];
			animation->vertices[index] = block_def_ptr->animation->vertices[index];
			create_vertex_list(block_def_ptr->animation->vertices[index]);
			for (j = 0; j < block_def_ptr->animation->vertices[index]; j++)
				vertex_list[j] = block_def_ptr->animation->frame_list[index].vertex_list[j];
			animation->frame_list[index].vertex_list = vertex_list;
			vertex_list = NULL;
		}
		// copy the loop information
		create_loops_list(animation->loops);
		for (j=0; j < animation->loops; j++) {
			animation->loops_list[j] = block_def_ptr->animation->loops_list[j];
		}
		animation->original_frames = true;

		//set_title("frames %d",block_def_ptr->animation->frames);
		vertices = animation->vertices[0];
		vertex_list = animation->frame_list[0].vertex_list;

	} else {

	// if this is an animation block then don't copy the vertex list
	//if (frames_list != NULL) 
	//	vertex_list = block_def_ptr->vertex_list;
	//else {
			// Make a copy of the vertex list.
	
			create_vertex_list(block_def_ptr->vertices);
			for (index = 0; index < vertices; index++)
				vertex_list[index] = block_def_ptr->vertex_list[index];
	}
	//}
}

// Method to return a pointer to the next free block, or NULL if we are out of
// memory.

block *
block_def::new_block(square *square_ptr)
{
	block *block_ptr;
	int index;
	light *old_light_ptr, *new_light_ptr, *next_light_ptr, *last_light_ptr;
	sound *old_sound_ptr, *new_sound_ptr, *next_sound_ptr, *last_sound_ptr;
	popup *old_popup_ptr, *new_popup_ptr, *next_popup_ptr, *last_popup_ptr;
	trigger *old_trigger_ptr, *new_trigger_ptr, 
		*last_trigger_ptr;
	entrance *new_entrance_ptr;
	wave *wave_ptr;
	action *action_ptr;

	// If there is a block in the free block list, use it.

	block_ptr = free_block_list;
	if (block_ptr != NULL) {
		free_block_list = block_ptr->next_block_ptr;

	// Initialise the trigger list for this block.  Note that we must not
	// disturb the linkages in the block's trigger list.

	old_trigger_ptr = trigger_list;
	new_trigger_ptr = block_ptr->trigger_list;
	while (new_trigger_ptr != NULL) {
		//objectid = new_trigger_ptr->objectid;
		//playerid = new_trigger_ptr->playerid;
		//next_trigger_ptr = new_trigger_ptr->next_trigger_ptr;
		//*new_trigger_ptr = *old_trigger_ptr;
		//new_trigger_ptr->objectid = objectid;
		//new_trigger_ptr->playerid = playerid;
		//new_trigger_ptr->next_trigger_ptr = next_trigger_ptr;
		new_trigger_ptr->square_ptr = square_ptr;
		new_trigger_ptr->block_ptr = block_ptr;
		init_global_trigger(new_trigger_ptr);

		// now init the actions inside the old block
		action_ptr = new_trigger_ptr->action_list;
		while(action_ptr) {
			action_ptr->trigger_ptr = new_trigger_ptr;
			action_ptr = action_ptr->next_action_ptr;
		}

		//old_trigger_ptr = old_trigger_ptr->next_trigger_ptr;
		new_trigger_ptr = new_trigger_ptr->next_trigger_ptr;
	}

	}
	// Otherwise create a new block.

	else {
		NEW(block_ptr, block);
		if (block_ptr == NULL)
			return(NULL);
		block_ptr->block_def_ptr = this;

		// Create the vertex list, polygon list and active light list for this
		// block.

		if (!block_ptr->create_vertex_list(vertices) ||
			!block_ptr->create_polygon_list(polygons) ||
			!block_ptr->create_active_light_list()) {
			DEL(block_ptr, block);
			return(NULL);
		}

		// Create the collision mesh structure for this block.

		if (type == STRUCTURAL_BLOCK) {
			if (!COL_createBlockColMesh(block_ptr)) {
				DEL(block_ptr, block);
				return(NULL);
			}
		} else {
			if (!COL_createSpriteColMesh(block_ptr)) {
				DEL(block_ptr, block);
				return(NULL);
			}
		}

		// Create the light list for this block by duplicating the light list
		// from the block definition, maintaining the order of lights.

		old_light_ptr = light_list;
		last_light_ptr = NULL;
		while (old_light_ptr != NULL) {
			NEW(new_light_ptr, light);
			if (new_light_ptr == NULL)
				memory_error("light");
			if (last_light_ptr != NULL)
				last_light_ptr->next_light_ptr = new_light_ptr;
			else
				block_ptr->light_list = new_light_ptr;
			last_light_ptr = new_light_ptr;
			old_light_ptr = old_light_ptr->next_light_ptr;
		}

		// Create the sound list for this block by duplicating the sound
		// list from the block definition, maintaining the order of sounds.

		old_sound_ptr = sound_list;
		last_sound_ptr = NULL;
		while (old_sound_ptr != NULL) {
			NEW(new_sound_ptr, sound);
			if (new_sound_ptr == NULL)
				memory_error("sound");
			if (last_sound_ptr != NULL)
				last_sound_ptr->next_sound_ptr = new_sound_ptr;
			else
				block_ptr->sound_list = new_sound_ptr;
			last_sound_ptr = new_sound_ptr;
			old_sound_ptr = old_sound_ptr->next_sound_ptr;
		}

		// Create the popup list for this block by duplicating the popup
		// list from the block definition, maintaining the order of popups.

		block_ptr->popup_trigger_flags = popup_trigger_flags;
		old_popup_ptr = popup_list;
		last_popup_ptr = NULL;
		while (old_popup_ptr != NULL) {
			NEW(new_popup_ptr, popup);
			if (new_popup_ptr == NULL)
				memory_error("popup");
			if (last_popup_ptr != NULL)
				last_popup_ptr->next_popup_ptr = new_popup_ptr;
			else
				block_ptr->popup_list = new_popup_ptr;
			last_popup_ptr = new_popup_ptr;
			old_popup_ptr = old_popup_ptr->next_popup_ptr;
		}

		// Create the trigger list for this block by duplicating the trigger
		// list from the block definition, maintaining the order of triggers.

		block_ptr->trigger_flags = trigger_flags;
		old_trigger_ptr = trigger_list;
		last_trigger_ptr = NULL;
		while (old_trigger_ptr != NULL) {
			//NEW(new_trigger_ptr, trigger); *mp*
			new_trigger_ptr = dup_trigger(old_trigger_ptr);

			new_trigger_ptr->block_ptr = block_ptr;
			//set_title("trig %d %d",new_trigger_ptr->action_list->trigger_ptr,new_trigger_ptr->block_ptr);
			if (new_trigger_ptr == NULL)
				memory_error("trigger");
			if (last_trigger_ptr != NULL)
				last_trigger_ptr->next_trigger_ptr = new_trigger_ptr;
			else
				block_ptr->trigger_list = new_trigger_ptr;
			last_trigger_ptr = new_trigger_ptr;
			init_global_trigger(new_trigger_ptr);
			old_trigger_ptr = old_trigger_ptr->next_trigger_ptr;
		}

		// Create the exit, if there is one on the block definition.

		if (exit_ptr != NULL) {
			NEW(block_ptr->exit_ptr, hyperlink);
			if (block_ptr->exit_ptr == NULL)
				memory_error("exit");
		}

		// Create the entrance, if there is one on the block definition.

		if (entrance_ptr != NULL) {
			NEW(block_ptr->entrance_ptr, entrance);
			if (block_ptr->entrance_ptr == NULL)
				memory_error("entrance");
		}
	}

	// Initialise some miscellaneous block fields.

	block_ptr->block_def_ptr = this;
	block_ptr->square_ptr = square_ptr;
	block_ptr->sprite_angle = sprite_angle;
	block_ptr->solid = solid;
	block_ptr->block_orientation = block_orientation;
	block_ptr->block_origin = block_origin;
	block_ptr->set_active_lights = true;
	block_ptr->current_frame = -1;

	// Initialise the polygon list.

	for (index = 0; index < polygons; index++) {
		polygon *polygon_ptr = &block_ptr->polygon_list[index];
		polygon_ptr->polygon_def_ptr = &polygon_def_list[index];
		polygon_ptr->active = true;
	}

	// Initialise the light list for this block.  Note that we must not
	// disturb the linkages in the block's light list.

	old_light_ptr = light_list;
	new_light_ptr = block_ptr->light_list;
	while (old_light_ptr != NULL) {
		next_light_ptr = new_light_ptr->next_light_ptr;
		*new_light_ptr = *old_light_ptr;
		new_light_ptr->next_light_ptr = next_light_ptr;
		old_light_ptr = old_light_ptr->next_light_ptr;
		new_light_ptr = next_light_ptr;
	}

	// Initialise the sound list for this block.  If necessary the sound
	// buffer from the previous use of this block is removed first, then it
	// is created if there is a wave available.  Note that we must not
	// disturb the linkages in the block's sound list.

	old_sound_ptr = sound_list;
	new_sound_ptr = block_ptr->sound_list;
	while (old_sound_ptr != NULL) {
		destroy_sound_buffer(new_sound_ptr);
		next_sound_ptr = new_sound_ptr->next_sound_ptr;
		*new_sound_ptr = *old_sound_ptr;
		new_sound_ptr->next_sound_ptr = next_sound_ptr;
		new_sound_ptr->in_range = false;
		wave_ptr = new_sound_ptr->wave_ptr;		
		if (wave_ptr != NULL && wave_ptr->data_ptr != NULL)
			create_sound_buffer(new_sound_ptr);
		old_sound_ptr = old_sound_ptr->next_sound_ptr;
		new_sound_ptr = next_sound_ptr;
	}

	// Initialise the popup list for this block.  Note that we must not
	// disturb the linkages in the block's popup list.

	old_popup_ptr = popup_list;
	new_popup_ptr = block_ptr->popup_list;
	while (old_popup_ptr != NULL) {
		next_popup_ptr = new_popup_ptr->next_popup_ptr;
		*new_popup_ptr = *old_popup_ptr;
		new_popup_ptr->next_popup_ptr = next_popup_ptr;
		new_popup_ptr->square_ptr = square_ptr;
		new_popup_ptr->block_ptr = block_ptr;
		old_popup_ptr = old_popup_ptr->next_popup_ptr;
		new_popup_ptr = next_popup_ptr;
	}



	// Initialise the entrance, if there is one.

	if (entrance_ptr != NULL) {
		new_entrance_ptr = block_ptr->entrance_ptr;
		*new_entrance_ptr = *entrance_ptr;
		new_entrance_ptr->square_ptr = square_ptr;
		new_entrance_ptr->block_ptr = block_ptr;
	}

	// Initialise the exit, if there is one.

	if (exit_ptr != NULL)
		*block_ptr->exit_ptr = *exit_ptr;

	// Put the block on the used block list.

	if (used_block_list != NULL)
		used_block_list->prev_used_block_ptr = block_ptr;
	block_ptr->prev_used_block_ptr = NULL;
	block_ptr->next_used_block_ptr = used_block_list;
	used_block_list = block_ptr;

	// Return the pointer to the block.

	return(block_ptr);
}

// Method to add a block to the head of the free block list, and return a
// pointer to the next block.

block *
block_def::del_block(block *block_ptr)
{
	// Delete the block and vertex SimKin objects, if they exist.

//POWERS
//	if (block_ptr->block_simkin_object_ptr != NULL)
//		destroy_block_simkin_object(block_ptr);
//	if (block_ptr->vertex_simkin_object_ptr != NULL)
//		destroy_vertex_simkin_object(block_ptr);

	// Remove the block from the used block list.

	if (block_ptr->prev_used_block_ptr != NULL)
		block_ptr->prev_used_block_ptr->next_used_block_ptr =
			block_ptr->next_used_block_ptr;
	else
		used_block_list = block_ptr->next_used_block_ptr;

	// Put the block back on the free block list.

	block *next_block_ptr = block_ptr->next_block_ptr;
	block_ptr->next_block_ptr = free_block_list;
	free_block_list = block_ptr;
	return(next_block_ptr);
}

// Return the single or double symbol as a string.

string
block_def::get_symbol(void)
{
	string symbol;

	switch (world_ptr->map_style) {
	case SINGLE_MAP:
		symbol = single_symbol;
		break;
	case DOUBLE_MAP:
		symbol = (char)(double_symbol >> 7);
		symbol += (char)(double_symbol & 127);
	}
	return(symbol);
}

//------------------------------------------------------------------------------
// Blockset class.
//------------------------------------------------------------------------------

// Default constructor initialises the block definition, texture and wave lists.

blockset::blockset()
{
	block_def_list = NULL;
	placeholder_texture_ptr = NULL;
	sky_defined = false;
	sky_texture_ptr = NULL;
	sky_colour_set = false;
	sky_colour.set_RGB(0,0,0);
	sky_brightness_set = false;
	sky_brightness = 1.0f;
	ground_defined = false;
	ground_texture_ptr = NULL;
	ground_colour_set = false;
	ground_colour.set_RGB(0,0,0);
	orb_defined = false;
	orb_texture_ptr = NULL;
	orb_direction_set = false;
	orb_direction.set(0.0f,0.0f);
	orb_brightness_set = false;
	orb_brightness = 1.0f;
	orb_colour_set = false;
	orb_colour.set_RGB(255,255,255);
	orb_exit_ptr = NULL;
	first_texture_ptr = NULL;
	last_texture_ptr = NULL;
	first_wave_ptr = NULL;
	last_wave_ptr = NULL;
	next_blockset_ptr = NULL;
}

// Default destructor deletes all block definitions, textures and waves.

blockset::~blockset()
{
	block_def *next_block_def_ptr;
	texture *next_texture_ptr;
	wave *next_wave_ptr;

	// Delete the orb exit, if it exists.

	if (orb_exit_ptr != NULL)
		DEL(orb_exit_ptr, hyperlink);

	// Delete the block definitions.

	while (block_def_list != NULL) {
		next_block_def_ptr = block_def_list->next_block_def_ptr;
		DEL(block_def_list, block_def);
		block_def_list = next_block_def_ptr;
	}

	// Delete the placeholder texture, if it exists.

	if (placeholder_texture_ptr != NULL)
		DEL(placeholder_texture_ptr, texture);

	// Delete the texture list.

	while (first_texture_ptr != NULL) {
		next_texture_ptr = first_texture_ptr->next_texture_ptr;
		DEL(first_texture_ptr, texture);
		first_texture_ptr = next_texture_ptr;
	}

	// Delete the wave list.

	while (first_wave_ptr != NULL) {
		next_wave_ptr = first_wave_ptr->next_wave_ptr;
		DEL(first_wave_ptr, wave);
		first_wave_ptr = next_wave_ptr;
	}
}

// Add block definition to the head of the block definition list.

void
blockset::add_block_def(block_def *block_def_ptr)
{
	block_def_ptr->next_block_def_ptr = block_def_list;
	block_def_list = block_def_ptr;
}

// Delete the block definition with the given single or double character symbol,
// if it exists.

bool
blockset::delete_block_def(word symbol)
{
	block_def *prev_block_def_ptr, *block_def_ptr;

	prev_block_def_ptr = NULL;
	block_def_ptr = block_def_list;
	while (block_def_ptr != NULL) {
		if ((world_ptr->map_style == SINGLE_MAP && 
			 block_def_ptr->single_symbol == (byte)symbol) ||
			(world_ptr->map_style == DOUBLE_MAP && 
			 block_def_ptr->double_symbol == symbol)) {
			if (prev_block_def_ptr != NULL)
				prev_block_def_ptr->next_block_def_ptr =
					block_def_ptr->next_block_def_ptr;
			else
				block_def_list = block_def_ptr->next_block_def_ptr;
			DEL(block_def_ptr, block_def);
			return(true);
		}
		prev_block_def_ptr = block_def_ptr;
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
	return(false);
}

// Return a pointer to the block definition with the given single-character
// symbol.

block_def *
blockset::get_block_def(char single_symbol)
{
	block_def *block_def_ptr = block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->single_symbol == single_symbol)
			break;
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
	return(block_def_ptr);
}

// Return a pointer to the block definition with the given double-character
// symbol.

block_def *
blockset::get_block_def(word double_symbol)
{
	block_def *block_def_ptr = block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->double_symbol == double_symbol)
			break;
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
	return(block_def_ptr);
}

// Return a pointer to the block definition with the given name.

block_def *
blockset::get_block_def(const char *name)
{
	block_def *block_def_ptr = block_def_list;
	while (block_def_ptr != NULL) {
		if (!_stricmp(block_def_ptr->name, name))
			break;
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
	return(block_def_ptr);
}

// Return a pointer to the block definition with the given single-character
// symbol or name.

block_def *
blockset::get_block_def(char single_symbol, const char *name)
{
	block_def *block_def_ptr = block_def_list;
	while (block_def_ptr != NULL) {
		if (block_def_ptr->single_symbol == single_symbol ||
			!_stricmp(block_def_ptr->name, name))
			break;
		block_def_ptr = block_def_ptr->next_block_def_ptr;
	}
	return(block_def_ptr);
}

// Method to rotate a blockdef around the X axis.

void 
block_def::rotate_x(float angle)
{
	vertex *vertex_ptr;

	// Perform the rotation around the X axis.

	for (int index = 0; index < vertices; index++) {
		vertex_ptr = &vertex_list[index];
		*vertex_ptr -= block_origin;
		vertex_ptr->rotate_x(angle);
		*vertex_ptr += block_origin;
	}

}

// Method to rotate a blockdef around the Y axis.

void
block_def::rotate_y(float angle)
{
	vertex *vertex_ptr;

	// Perform the rotation around the Y axis.

	for (int index = 0; index < vertices; index++) {
		vertex_ptr = &vertex_list[index];
		*vertex_ptr -= block_origin;
		vertex_ptr->rotate_y(angle);
		*vertex_ptr += block_origin;
	}

}

// Method to rotate a block around the Z axis.

void
block_def::rotate_z(float angle)
{
	vertex *vertex_ptr;

	// Perform the rotation around the Z axis.

	for (int index = 0; index < vertices; index++) {
		vertex_ptr = &vertex_list[index];
		*vertex_ptr -= block_origin;
		vertex_ptr->rotate_z(angle);
		*vertex_ptr += block_origin;
	}

}




// Add a texture to the end of the texture list.

void
blockset::add_texture(texture *texture_ptr)
{
	if (last_texture_ptr != NULL)
		last_texture_ptr->next_texture_ptr = texture_ptr;
	else
		first_texture_ptr = texture_ptr;
	last_texture_ptr = texture_ptr;
}

// Add a wave to the end of the wave list.

void
blockset::add_wave(wave *wave_ptr)
{
	if (last_wave_ptr != NULL)
		last_wave_ptr->next_wave_ptr = wave_ptr;
	else
		first_wave_ptr = wave_ptr;
	last_wave_ptr = wave_ptr;
}

//------------------------------------------------------------------------------
// Blockset list class.
//------------------------------------------------------------------------------

// Default constructor initialises the list of blocksets.

blockset_list::blockset_list()
{
	first_blockset_ptr = NULL;
	last_blockset_ptr = NULL;
}

// Default destructor deletes the list, if it exists.

blockset_list::~blockset_list()
{
	while (first_blockset_ptr != NULL) {
		blockset *next_blockset_ptr = first_blockset_ptr->next_blockset_ptr;
		DEL(first_blockset_ptr, blockset);
		first_blockset_ptr = next_blockset_ptr;
	}
}

// Method to add a blockset to the end of the list.

void
blockset_list::add_blockset(blockset *blockset_ptr)
{
	if (last_blockset_ptr != NULL)
		last_blockset_ptr->next_blockset_ptr = blockset_ptr;
	else
		first_blockset_ptr = blockset_ptr;
	last_blockset_ptr = blockset_ptr;
}

// Method to search for the blockset with the given URL, and return a boolean 
// result indicating whether it was found.

bool
blockset_list::find_blockset(char *URL)
{
	blockset *blockset_ptr = first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if (!_stricmp(blockset_ptr->URL, URL))
			return(true);
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	return(false);
}

// Method to find and remove the blockset that has the given URL, returning a
// pointer to the blockset removed.

blockset *
blockset_list::remove_blockset(char *URL)
{
	blockset *prev_blockset_ptr = NULL;
	blockset *blockset_ptr = first_blockset_ptr;
	while (blockset_ptr != NULL) {
		if (!_stricmp(blockset_ptr->URL, URL)) {
			if (prev_blockset_ptr != NULL)
				prev_blockset_ptr->next_blockset_ptr = 
					blockset_ptr->next_blockset_ptr;
			else
				first_blockset_ptr = blockset_ptr->next_blockset_ptr;
			blockset_ptr->next_blockset_ptr = NULL;
			return(blockset_ptr);
		}
		prev_blockset_ptr = blockset_ptr;
		blockset_ptr = blockset_ptr->next_blockset_ptr;
	}
	return(NULL);
}

//==============================================================================
// Block and map classes.
//==============================================================================

//------------------------------------------------------------------------------
// Block class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.
	
block::block()
{
	vertices = 0;
	vertex_list = NULL;
	current_frame = -1;
	current_loop = -1;
	current_repeat = false;
	next_repeat = false;
	polygons = 0;
	polygon_list = NULL;
	pixmap_index = 0;
	col_mesh_ptr = NULL;
	solid = true;
//POWERS	block_simkin_object_ptr = NULL;
//	vertex_simkin_object_ptr = NULL;
	light_list = NULL;
	active_light_list = NULL;
	set_active_lights = false;
	sound_list = NULL;
	popup_list = NULL;
	trigger_list = NULL;
	entrance_ptr = NULL;
	exit_ptr = NULL;
	next_block_ptr = NULL;
}

// Default destructor deletes all data.
	
block::~block()
{
	light *next_light_ptr;
	sound *next_sound_ptr;
	popup *next_popup_ptr;
	trigger *next_trigger_ptr;

	// Delete the vertex list.

	if (current_frame == -1 && vertex_list != NULL)
		DELARRAY(vertex_list, vertex, vertices);

	// Delete the polygon list.

	if (polygon_list != NULL)
		DELARRAY(polygon_list, polygon, polygons);

	// Delete the active light list.

	if (active_light_list != NULL)
		DELARRAY(active_light_list, light_ref, max_active_lights);

	// Delete the collision mesh.

	if (col_mesh_ptr != NULL)
		DELBASEARRAY((colmeshbyte *)col_mesh_ptr, colmeshbyte, col_mesh_size);

	// Destroy the block and vertex SimKin objects, if they exist.
//POWERS
//	if (block_simkin_object_ptr != NULL)
//		destroy_block_simkin_object(this);
//	if (vertex_simkin_object_ptr != NULL)
//		destroy_vertex_simkin_object(this);

	// Delete the list of lights.

	while (light_list != NULL) {
		next_light_ptr = light_list->next_light_ptr;
		DEL(light_list, light);
		light_list = next_light_ptr;
	}

	// Delete the list of sounds.

	while (sound_list != NULL) {
		next_sound_ptr = sound_list->next_sound_ptr;
		DEL(sound_list, sound);
		sound_list = next_sound_ptr;
	}

	// Delete the list of popups.

	while (popup_list != NULL) {
		next_popup_ptr = popup_list->next_popup_ptr;
		DEL(popup_list, popup);
		popup_list = next_popup_ptr;
	}

	// Delete the list of triggers.

	while (trigger_list != NULL) {
		trigger_list->action_list = NULL;
		next_trigger_ptr = trigger_list->next_trigger_ptr;
		DEL(trigger_list, trigger);
		trigger_list = next_trigger_ptr;
	}

	// Delete the entrance.

	if (entrance_ptr != NULL)
		DEL(entrance_ptr, entrance);

	// Delete the exit.

	if (exit_ptr != NULL)
		DEL(exit_ptr, hyperlink);
}

// Method to create the vertex list.

bool
block::create_vertex_list(int set_vertices)
{
	if (!block_def_ptr->animated) {
		vertices = set_vertices;
		if (vertices > 0) {
			NEWARRAY(vertex_list, vertex, vertices);
			if (vertex_list == NULL)
				return(false);
		}
	} else {
		vertices = block_def_ptr->vertices;
		vertex_list = block_def_ptr->vertex_list;
	}
	return(true);
}

// Method to create the polygon list.

bool
block::create_polygon_list(int set_polygons)
{
	polygons = set_polygons;
	if (polygons > 0) {
		NEWARRAY(polygon_list, polygon, polygons);
		if (polygon_list == NULL)
			return(false);
	}
	return(true);
}

// Method to create the active light list.

bool
block::create_active_light_list(void)
{
	if (max_active_lights > 0) {
		NEWARRAY(active_light_list, light_ref, max_active_lights);
		if (active_light_list == NULL)
			return(false);
	}
	return(true);
}

// Method to reset the vertices to those found in the block definition.

void
block::reset_vertices(void)
{
	for (int index = 0; index < vertices; index++)
		vertex_list[index] = block_def_ptr->vertex_list[index];
}

// Method to calculate necessary data after a rotation or vertex manipulation.

void
block::update(void)
{
	polygon *polygon_ptr;

	// Recalculate the centroid, normal vector and plane offset of each 
	// polygon definition.

	for (int index = 0; index < polygons; index++) {
		polygon_ptr = &polygon_list[index];
		polygon_ptr->compute_centroid(vertex_list);
		polygon_ptr->compute_normal_vector(vertex_list);
		polygon_ptr->compute_plane_offset(vertex_list);
	}

	// Calculate the block's collision mesh.

	COL_convertBlockToColMesh(this);
}

// Method to orient a block by it's current orientation.

void 
block::orient(void)
{
	int index;
	vertex *vertex_ptr;

	// If a compass direction was given, first tilt the block so that the top
	// points in the given compass direction, then rotate the block around the
	// axis pointing in that direction, using the block origin as the central
	// point.

	if (block_orientation.direction != NONE) {
		for (index = 0; index < vertices; index++) {
			vertex_ptr = &vertex_list[index];
			*vertex_ptr -= block_origin;
			switch (block_orientation.direction) {
			case UP:
				vertex_ptr->rotate_y(block_orientation.angle);
				break;
			case DOWN:
				vertex_ptr->rotate_x(180.0f);
				vertex_ptr->rotate_y(-block_orientation.angle);
				break;
			case NORTH:
				vertex_ptr->rotate_x(90.0f);
				vertex_ptr->rotate_z(block_orientation.angle);
				break;
			case SOUTH:
				vertex_ptr->rotate_x(-90.0f);
				vertex_ptr->rotate_z(-block_orientation.angle);
				break;
			case EAST:
				vertex_ptr->rotate_z(-90.0f);
				vertex_ptr->rotate_x(block_orientation.angle);
				break;
			case WEST:
				vertex_ptr->rotate_z(90.0f);
				vertex_ptr->rotate_x(-block_orientation.angle);
			}
			*vertex_ptr += block_origin;
		}
	}

	// If there is no compass direction given, rotate the block around the Y, X 
	// and Z axes, in that order, using the block origin as the central point.
	// NOTE: Prior to Rover 3.0, the rotation around the Z axis was reversed.

	else {
		for (index = 0; index < vertices; index++) {
			vertex_ptr = &vertex_list[index];
			*vertex_ptr -= block_origin;
			vertex_ptr->rotate_y(block_orientation.angle_y);
			vertex_ptr->rotate_x(block_orientation.angle_x);
			if (min_rover_version >= 0x03000000)
				vertex_ptr->rotate_z(block_orientation.angle_z);
			else
				vertex_ptr->rotate_z(-block_orientation.angle_z);
			*vertex_ptr += block_origin;
		}
	}

	// Update the block in it's new orientation.

	update();
}

// Method to rotate a block around the X axis.

void 
block::rotate_x(float angle)
{
	vertex *vertex_ptr;

	// Perform the rotation around the X axis.

	for (int index = 0; index < vertices; index++) {
		vertex_ptr = &vertex_list[index];
		*vertex_ptr -= block_origin;
		vertex_ptr->rotate_x(angle);
		*vertex_ptr += block_origin;
	}

	// Update the block in it's new orientation.

	update();
}

// Method to rotate a block around the Y axis.

void
block::rotate_y(float angle)
{
	vertex *vertex_ptr;

    if (block_def_ptr->animated) {

		// Perform the rotation around the Y axis.

		for (int index = 0; index < vertices; index++) {
			vertex_ptr = &vertex_list[index];
			*vertex_ptr -= block_origin;
			vertex_ptr->rotate_y(angle);
			*vertex_ptr += block_origin;
		}

		block_orientation.angle_y = pos_adjust_angle(angle + block_orientation.angle_y);
		block_def_ptr->animation->angles[current_frame] = block_orientation.angle_y; //pos_adjust_angle(angle + block_def_ptr->animation->angles[current_frame]);

	} else {
		// Perform the rotation around the Y axis.

		for (int index = 0; index < vertices; index++) {
				vertex_ptr = &vertex_list[index];
				*vertex_ptr -= block_origin;
				vertex_ptr->rotate_y(angle);
				*vertex_ptr += block_origin;
		}
	}

	// Update the block in it's new orientation.

	update();
}

// Method to rotate a block around the Z axis.

void
block::rotate_z(float angle)
{
	vertex *vertex_ptr;

	// Perform the rotation around the Z axis.

	for (int index = 0; index < vertices; index++) {
		vertex_ptr = &vertex_list[index];
		*vertex_ptr -= block_origin;
		vertex_ptr->rotate_z(angle);
		*vertex_ptr += block_origin;
	}

	// Update the block in it's new orientation.

	update();
}

// Method to change the current frame in an animated block.

void
block::set_frame(int number)
{
	vertex *vertex_ptr;
	float delta;

	// change the frame number
	if (block_def_ptr->animated) {
			if (number > -1 && number < (block_def_ptr->animation->frames)) {
				current_frame = number;
				vertices = block_def_ptr->animation->vertices[number];
				vertex_list = block_def_ptr->animation->frame_list[number].vertex_list;

				if (block_orientation.angle_y != block_def_ptr->animation->angles[number]) {
					delta = block_orientation.angle_y - block_def_ptr->animation->angles[number];
					block_def_ptr->animation->angles[number] += delta;

					// Perform the rotation around the Y axis.

					for (int index = 0; index < vertices; index++) {
						vertex_ptr = &vertex_list[index];
						*vertex_ptr -= block_origin;
						vertex_ptr->rotate_y(delta);
						*vertex_ptr += block_origin;
					}
				}
			}
	}


	// Update the block in it's new frame.

	update();
}

// Method to change the next loop in an animated block.

void
block::set_nextloop(int number)
{
	
	// set the next loop number
	if (block_def_ptr->animation->loops != 0) {
			if (number > 0 && number < (block_def_ptr->animation->loops)) {
				next_loop = number;
			}
	}

}

//------------------------------------------------------------------------------
// Square class.
//------------------------------------------------------------------------------

// Default constructor initialises all fields.

square::square()
{
	block_symbol = NULL_BLOCK_SYMBOL;
	block_ptr = NULL;
	exit_ptr = NULL;
	popup_trigger_flags = 0;
	popup_list = NULL;
	last_popup_ptr = NULL;
	trigger_flags = 0;
	trigger_list = NULL;
	last_trigger_ptr = NULL;
}

// Default destructor deletes the exit, block and square trigger list, if they
// exist.

square::~square()
{
	block_def *block_def_ptr;
	trigger *next_trigger_ptr;

	// Delete the exit.

	if (exit_ptr != NULL)
		DEL(exit_ptr, hyperlink);

	// Delete the block.

	if (block_ptr != NULL) {
		block_def_ptr = block_ptr->block_def_ptr;
		block_def_ptr->del_block(block_ptr);
	}

	// Delete the square trigger list.

	while (trigger_list != NULL) {
		next_trigger_ptr = trigger_list->next_trigger_ptr;
		DEL(trigger_list, trigger);
		trigger_list = next_trigger_ptr;
	}
}

//------------------------------------------------------------------------------
// World class.
//------------------------------------------------------------------------------

// Default constructor initialises the maps and block scaling factors.

world::world()
{
	map_style = SINGLE_MAP;
	ground_level_exists = false;
	square_map = NULL;
	audio_scale = 2.0f / UNITS_PER_BLOCK;
	gravity.x = 0.0;
	gravity.y = (float)-9.81;
	gravity.z = 0.0;
}

// Default destructor deletes the square, if it exists.

world::~world()
{
	if (square_map != NULL)
		DELARRAY(square_map, square, columns * rows * levels);
}

// Method to create the square map.  It is assumed the dimensions for the
// square map are already set.

bool
world::create_square_map(void)
{
	NEWARRAY(square_map, square, columns * rows * levels);
	return(square_map != NULL);
}

// Method to return a pointer to the square at a given map position.

square *
world::get_square_ptr(int column, int row, int level)
{
	if (column < 0 || column >= columns || row < 0 || row >= rows ||
		level < 0 || level >= levels)
		return(NULL);
	return(&square_map[(level * rows + row) * columns + column]);
}

// Method to return a pointer to the block at a given map position.

block *
world::get_block_ptr(int column, int row, int level)
{
	square *square_ptr = get_square_ptr(column, row, level);
	if (square_ptr != NULL)
		return(square_ptr->block_ptr);
	return(NULL);
}

// Method to return the location of the given square.

void 
world::get_square_location(square *square_ptr, int *column_ptr, int *row_ptr,
						   int *level_ptr)
{
	int offset;

	// Determine the offset of the square from the beginning of the square map.
	// NOTE: Visual C++ generates an array index from the subtraction of two
	// pointers; I don't know if this is standard C++ semantics or not.

	offset = square_ptr - square_map;

	// Determine which level, row and column the square is on.

	*level_ptr = offset / (rows * columns);
	offset = offset % (rows * columns);
	*row_ptr = offset / columns;
	offset = offset % columns;
	*column_ptr = offset;
}

//==============================================================================
// Trigonometry classes.
//==============================================================================

//------------------------------------------------------------------------------
// Sine table class.
//------------------------------------------------------------------------------

// Default constructor creates the sine table for angles 0-359.

sine_table::sine_table()
{
	int index;
	float angle;

	for (index = 0, angle = 0.0; index <= 360; index++, angle += 1.0)
		table[index] = (float)sin(RAD(angle));
}

// Default destructor does nothing.

sine_table::~sine_table()
{
}

// Operator [] returns the sine of an angle.

float
sine_table::operator[](float angle)
{
	int index;

	index = (int)angle % 360;
	if (index < 0)
		index += 360;
	return(table[index]);
}

//------------------------------------------------------------------------------
// Cosine table class.
//------------------------------------------------------------------------------

// Default constructor creates the cosine table for angles 0-359.

cosine_table::cosine_table()
{
	int index;
	float angle;

	for (index = 0, angle = 0.0; index <= 360; index++, angle += 1.0)
		table[index] = (float)cos(RAD(angle));
}

// Default destructor does nothing.

cosine_table::~cosine_table()
{
}

// Operator [] returns the cosine of an angle.

float
cosine_table::operator[](float angle)
{
	int index;
	
	index = (int)angle % 360;
	if (index < 0)
		index += 360;
	return(table[index]);
}

//------------------------------------------------------------------------------
// Hash table class.
//------------------------------------------------------------------------------



// returns the pointer the given hash value

hash *
hash_table::get(int objectid, int playerid)
{
	hash *place;

	place = table[objectid % 256];

	while (place != NULL) {
		if (place->objectid == objectid && place->playerid == playerid)
			return (place);
		place = place->next_hash;
	}
	
	return(NULL);
}

// method to add this object to the hash table

hash *
hash_table::add(hash* obj)
{
	int index;

	index = obj->objectid % 256;

	obj->next_hash = table[index];
	table[index] = obj;

	return(obj);
}

void
hash_table::remove(hash* obj)
{
	hash *place;

	place = table[obj->objectid % 256];

	if (place == NULL) return;

	if (place == obj)
		table[obj->objectid % 256] = place->next_hash;

	while (place->next_hash) {
		if (place->next_hash == obj) {
			place->next_hash = obj->next_hash;
			return;
		}
		place = place->next_hash;
	}
}

void
hash_table::clear()
{
	int index;

	for (index=0;index<256;index++) {
		table[index] = NULL;
	}
}