//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#ifdef SIMKIN

#include "simkin\skInterpreter.h"
#include "simkin\skStatementStepper.h"
#include "simkin\skElementExecutable.h"
#include "simkin\skParseNode.h"
#include "simkin\skRValueArray.h"
#include "simkin\skParseException.h"
#include "simkin\skRuntimeException.h"
#include "Simkin\skInputSource.h"
#include <time.h>
#include <math.h>
#include <stdio.h>
#include "Classes.h"
#include "Light.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Main.h"
#include "Memory.h"
#include "SimKin.h"
#include "Utils.h"

// The SimKin interpreter and context object.

static skInterpreter *simkin_interpreter_ptr;
static skExecutableContext *simkin_context_ptr;

// Macro to create a SimKin object, and add it to the global variable list.

#define NEW_SIMKIN_OBJECT(simkin_object_ptr, simkin_object, name) \
{ \
	if ((simkin_object_ptr = new simkin_object) == NULL) \
		throw false; \
	skRValue simkin_variable(simkin_object_ptr); \
	simkin_interpreter_ptr->addGlobalVariable(name, simkin_variable); \
}

// Flag indicating if SimKin is running.

static bool running;

// Simkin thread handle and events.

static unsigned long simkin_thread_handle;
static event simkin_thread_initialised;
static event perform_command;
static event command_completed;
static int script_start_time_ms, script_resume_time_ms;

// Command code, script or method name (if required), and block that owns it.

#define EXECUTE_SCRIPT		0
#define CALL_GLOBAL_METHOD	1
#define RESUME_SCRIPT		2
#define TERMINATE_SCRIPT	3
#define TERMINATE_SIMKIN	4

static int command_code;
static skString simkin_method_name;
static script_def *simkin_script_def_ptr;
static block *simkin_block_ptr;

// Script definition list.

static script_def *script_def_list;
static unsigned int curr_script_def_ID;

//------------------------------------------------------------------------------
// Load a new custom texture, making sure that the download will be initiated
// where necessary.
//------------------------------------------------------------------------------

static void
load_custom_texture(texture *&texture_ptr, const char *URL)
{	
	texture_ptr = load_texture(custom_blockset_ptr, (char *)URL, true);
	if (texture_ptr != NULL) {
		if (texture_ptr->pixmap_list == NULL) {
			if (curr_custom_texture_ptr == NULL)
				curr_custom_texture_ptr = custom_blockset_ptr->first_texture_ptr;
		} else
			update_texture_dependancies(texture_ptr);
	}
}

//------------------------------------------------------------------------------
// Load a new custom wave, making sure that the download will be initiated
// where necessary.
//------------------------------------------------------------------------------

static void
load_custom_wave(wave *&wave_ptr, const char *URL)
{	
	wave_ptr = load_wave(custom_blockset_ptr, (char *)URL);
	if (wave_ptr != NULL) {
		if (wave_ptr->data_ptr == NULL) {
			if (curr_custom_wave_ptr == NULL)
				curr_custom_wave_ptr = custom_blockset_ptr->first_wave_ptr;
		} else
			update_wave_dependancies(wave_ptr);
	}
}

//------------------------------------------------------------------------------
// The vertex SimKin object class.
//------------------------------------------------------------------------------

class vertex_simkin_object : public skExecutable
{
public:
	block *block_ptr;
	int vertices;
	vertex *vertex_list;

	bool getValueAt(const skRValue& array_index, const skString& attribute, skRValue& value);
	bool setValueAt(const skRValue& array_index, const skString& attribute, const skRValue& value);
	bool method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt);
};

// Get the value of a vertex array element's attribute.

bool 
vertex_simkin_object::getValueAt(const skRValue& array_index, const skString& attribute, skRValue& value)
{
	int vertex_index;
	vertex *vertex_ptr;

	// If the block pointer and vertex list pointer is NULL, this this vertex
	// list belonged to a block that has been removed from the map.

	if (block_ptr == NULL && vertex_list == NULL) {
		diagnose("Script attempting to get vertex attribute on block that has "
			"been removed from the map");
		return(false);
	}

	// Verify the vertex index is within range, then get a pointer to the vertex
	// in the vertex list.

	vertex_index = array_index.intValue();
	if (vertex_index < 0 || vertex_index >= vertices)
		return(false);
	vertex_ptr = &vertex_list[vertex_index];

	// If the attribute is "x", "y", or "z",  return the approapiate coordinate
	// of the vertex in texel units, without the block translation.

	if (attribute == "x")
		value = vertex_ptr->x * TEXELS_PER_UNIT;
	else if (attribute == "y")
		value = vertex_ptr->y * TEXELS_PER_UNIT;
	else if (attribute == "z")
		value = vertex_ptr->z * TEXELS_PER_UNIT;
	else
		return(false);

	// Indicate success.

	return(true);
}

// Set the value of a vertex array element's attribute.

bool 
vertex_simkin_object::setValueAt(const skRValue& array_index, const skString& attribute, const skRValue& value)
{
	block_def *block_def_ptr;
	int vertex_index;
	vertex *vertex_ptr;
	float vertex_coord;

	// If the block pointer and vertex list pointer is NULL, this this vertex
	// list belonged to a block that has been removed from the map.

	if (block_ptr == NULL && vertex_list == NULL) {
		diagnose("Script attempting to set vertex attribute on block that has "
			"been removed from the map");
		return(false);
	}

	// If this vertex list belongs to a block, and it is not movable, do 
	// nothing.

	if (block_ptr != NULL) {
		block_def_ptr = block_ptr->block_def_ptr;
		if (!block_def_ptr->movable)
			return(true);
	}

	// Verify the vertex index is within range, then get a pointer to the vertex
	// in the vertex list.

	vertex_index = array_index.intValue();
	if (vertex_index < 0 || vertex_index >= vertices)
		return(false);
	vertex_ptr = &vertex_list[vertex_index];

	// If the attribute is "x", "y", or "z", set the approapiate coordinate of
	// the vertex from texel units, with the block translation included.

	vertex_coord = value.floatValue() / TEXELS_PER_UNIT;
	if (attribute == "x")
		vertex_ptr->x = vertex_coord;
	else if (attribute == "y")
		vertex_ptr->y = vertex_coord;
	else if (attribute == "z")
		vertex_ptr->z = vertex_coord;
	else
		return(false);

	// Update the block, if there is one associated with this vertex array.

	if (block_ptr != NULL)
		block_ptr->update();
	
	// Indicate success.

	return(true);
}

// Execute a method.

bool 
vertex_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{
	block_def *block_def_ptr;
	int vertices_to_change, vertex_index, vertex_no;
	vertex *vertex_ptr;

	// If the block pointer and vertex list pointer is NULL, this this vertex
	// list belonged to a block that has been removed from the map.

	if (block_ptr == NULL && vertex_list == NULL) {
		diagnose("Script attempting to call vertex method on block that has "
			"been removed from the map");
		return(false);
	}

	// If this vertex list belongs to a block, and it is not movable, do 
	// nothing.

	if (block_ptr != NULL) {
		block_def_ptr = block_ptr->block_def_ptr;
		if (!block_def_ptr->movable)
			return(true);
	}

	// If the method is "set", update all coordinates of a vertex at once...

	if (method == "set") {
		if (arguments.entries() != 4)
			return(false);
		vertex_index = arguments[0].intValue();
		if (vertex_index < 0 || vertex_index >= vertices)
			return(false);
		vertex_ptr = &vertex_list[vertex_index];
		vertex_ptr->x = arguments[1].floatValue() / TEXELS_PER_UNIT;
		vertex_ptr->y = arguments[2].floatValue() / TEXELS_PER_UNIT;
		vertex_ptr->z = arguments[3].floatValue() / TEXELS_PER_UNIT;

		// Update the block, if there is one associated with this vertex array.

		if (block_ptr != NULL)
			block_ptr->update();
	} 

	// If the method is "set_many", update the coordinates of more than one
	// vertex at once...

	else if (method == "set_many") {
		if ((arguments.entries() - 1) % 4 != 0)
			return(false);
		vertices_to_change = arguments[0].intValue();
		for (vertex_no = 0; vertex_no < vertices_to_change; vertex_no++) {
			vertex_index = arguments[1 + vertex_no * 4].intValue();
			if (vertex_index < 0 || vertex_index >= vertices)
				continue;
			vertex_ptr = &vertex_list[vertex_index];
			vertex_ptr->x = arguments[2 + vertex_no * 4].floatValue() / 
				TEXELS_PER_UNIT;
			vertex_ptr->y = arguments[3 + vertex_no * 4].floatValue() / 
				TEXELS_PER_UNIT;
			vertex_ptr->z = arguments[4 + vertex_no * 4].floatValue() / 
				TEXELS_PER_UNIT;
		}

		// Update the block, if there is one associated with this vertex array.

		if (block_ptr != NULL)
			block_ptr->update();
	} else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Create a vertex simkin object for a given vertex list, which may or may not
// be associated with a block.
//------------------------------------------------------------------------------

static skRValue *
create_vertex_simkin_object(block *block_ptr, int vertices, vertex *vertex_list)
{
	vertex_simkin_object *vertex_simkin_object_ptr;
	skRValue *simkin_value_ptr;

	if ((vertex_simkin_object_ptr = new vertex_simkin_object) == NULL)
		return(NULL);
	if ((simkin_value_ptr = new skRValue(vertex_simkin_object_ptr, true)) 
		== NULL) {
		delete vertex_simkin_object_ptr;
		return(NULL);
	}
	vertex_simkin_object_ptr->block_ptr = block_ptr;
	vertex_simkin_object_ptr->vertices = vertices; 
	vertex_simkin_object_ptr->vertex_list = vertex_list;
	return(simkin_value_ptr);
}

//------------------------------------------------------------------------------
// Destroy a vertex SimKin object contained in a skRValue object.
//------------------------------------------------------------------------------

static void
destroy_vertex_simkin_object(skRValue *simkin_value_ptr)
{
	// Set the block pointer and vertex list pointer in the vertex SimKin object
	// to NULL.  That way, if the object hangs around due to existing references
	// to it, it can produce safe error conditions when the object is accessed.

	vertex_simkin_object *vertex_simkin_object_ptr = 
		(vertex_simkin_object *)simkin_value_ptr->obj();
	vertex_simkin_object_ptr->block_ptr = NULL;
	vertex_simkin_object_ptr->vertices = 0;
	vertex_simkin_object_ptr->vertex_list = NULL;

	// Delete the skRValue object that holds a reference to the vertex SimKin
	// object.  This may or may not delete the object itself.

	delete simkin_value_ptr;
}

//------------------------------------------------------------------------------
// Return a skRValue object containing a reference to the given block's SimKin
// object.  It will be created if it doesn't already exist; if the creation
// fails, a SkRValue with no object reference is returned instead.
//------------------------------------------------------------------------------

static skRValue
get_block_simkin_object(block *block_ptr)
{
	if (block_ptr->block_simkin_object_ptr == NULL) {
		block_def *block_def_ptr = block_ptr->block_def_ptr;
		if (strlen(block_def_ptr->script) > 0)
			create_block_simkin_object(block_ptr, block_def_ptr->script);
		else
			create_block_simkin_object(block_ptr, NULL);
		if (block_ptr->block_simkin_object_ptr == NULL) {
			skRValue dummy_value;
			return(dummy_value);
		}
	}
	return(*(skRValue *)block_ptr->block_simkin_object_ptr);
}

//------------------------------------------------------------------------------
// Return a skRValue object containing a reference to the given block's vertex
// SimKin object.  It will be created if it doesn't already exist; if the
// creation fails, a SkRValue with no object reference is returned instead.
//------------------------------------------------------------------------------

static skRValue
get_vertex_simkin_object(block *block_ptr)
{
	if (block_ptr->vertex_simkin_object_ptr == NULL) {
		create_vertex_simkin_object(block_ptr);
		if (block_ptr->vertex_simkin_object_ptr == NULL) {
			skRValue dummy_value;
			return(dummy_value);
		}
	}
	return(*(skRValue *)block_ptr->vertex_simkin_object_ptr);
}

//------------------------------------------------------------------------------
// Get the value of a pre-defined block field/attribute.
//------------------------------------------------------------------------------

static bool
get_block_value(block *block_ptr, const skString& field, 
				const skString& attribute, skRValue& value)
{
	// If the block pointer is NULL, this this block has been removed from
	// the map and can no longer be accessed.

	if (block_ptr == NULL) {
		diagnose("Script attempting to get attribute on block that has been "
			"removed from the map");
		return(false);
	}

	// If the field is "location" and the attribute is "column", "row" or
	// "level", or "x", "y" or "z", return the approapiate coordinate of the 
	// block.

	block_def *block_def_ptr = block_ptr->block_def_ptr;
	COL_MESH *col_mesh_ptr = block_ptr->col_mesh_ptr;
	if (field == "location") {
		int column, row, level;

		// Determine the map position from the translation.

		block_ptr->translation.get_map_position(&column, &row, &level);
	 	if (world_ptr->ground_level_exists)
			level--;

		// Now return the desired value.

		if (attribute == "column")
			value = column + 1;
		else if (attribute == "row")
			value = row + 1;
		else if (attribute == "level")
			value = level + 1;
		else if (attribute == "x")
			value = block_ptr->translation.x * TEXELS_PER_UNIT;
		else if (attribute == "y")
			value = block_ptr->translation.y * TEXELS_PER_UNIT;
		else if (attribute == "z")
			value = block_ptr->translation.z * TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "origin" and the attribute is "x", "y" or "z", return
	// the approapiate coordinate of the block's origin.  Note that this field
	// is not available for sprites.

	else if (field == "origin" && block_def_ptr->type == STRUCTURAL_BLOCK) {
		if (attribute == "x")
			value = block_ptr->block_origin.x * TEXELS_PER_UNIT;
		else if (attribute == "y")
			value = block_ptr->block_origin.y * TEXELS_PER_UNIT;
		else if (attribute == "z")
			value = block_ptr->block_origin.z * TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "bbox" and the attribute is "min_x", "min_y", "min_z",
	// "max_x", "max_y", "max_z", return the aproapiate coordinate of the
	// block's bounding box.

	else if (field == "bbox" && col_mesh_ptr != NULL) {
		if (attribute == "min_x")
			value = col_mesh_ptr->minBox.x * TEXELS_PER_UNIT;
		else if (attribute == "min_y")
			value = col_mesh_ptr->minBox.y * TEXELS_PER_UNIT;
		else if (attribute == "min_z")
			value = col_mesh_ptr->minBox.z * TEXELS_PER_UNIT;
		else if (attribute == "max_x")
			value = col_mesh_ptr->maxBox.x * TEXELS_PER_UNIT;
		else if (attribute == "max_y")
			value = col_mesh_ptr->maxBox.y * TEXELS_PER_UNIT;
		else if (attribute == "max_z")
			value = col_mesh_ptr->maxBox.z * TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "vertices", return the number of vertices in the block.

	else if (field == "vertices")
		value = block_ptr->vertices;

	// If the field is "vertex", return the vertex SimKin object, if it exists.

	else if (field == "vertex")
		value = get_vertex_simkin_object(block_ptr);

	// If the field is "symbol", return the symbol of the block.

	else if (field == "symbol") {
		skString symbol = (char *)block_def_ptr->get_symbol();
		value = symbol;
	}

	// If the field is "name", return the name of the block.

	else if (field == "name") {
		skString symbol = (char *)block_def_ptr->name;
		value = symbol;
	}

	// If the field is "movable", return the movable flag.

	else if (field == "movable")
		value = block_def_ptr->movable;

	// All other fields are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Set the value of a pre-defined block field/attribute.
//------------------------------------------------------------------------------

static bool
set_block_value(block *block_ptr, const skString& field, 
				const skString& attribute, skRValue value)
{
	int old_min_column, old_min_row, old_min_level;
	int old_max_column, old_max_row, old_max_level;
	int new_min_column, new_min_row, new_min_level;
	int new_max_column, new_max_row, new_max_level;

	// If the block pointer is NULL, this this block has been removed from
	// the map and can no longer be accessed.

	if (block_ptr == NULL) {
		diagnose("Script attempting to set attribute on block that has been "
			"removed from the map");
		return(false);
	}

	// If the field is "location", the attribute is "x", "y" or "z", and the
	// block is movable, set the approapiate coordinate of the block.

	block_def *block_def_ptr = block_ptr->block_def_ptr;
	if (field == "location" && block_def_ptr->movable) {

		// If this block has lights, calculate the old bounding box for them.

		if (block_ptr->light_list != NULL)
			compute_light_list_bounding_box(block_ptr->light_list,
				block_ptr->translation, old_min_column, old_min_row,
				old_min_level, old_max_column, old_max_row, old_max_level);

		// Now update the specified attribute.

		if (attribute == "x")
			block_ptr->translation.x = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "y")
			block_ptr->translation.y = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "z")
			block_ptr->translation.z = value.floatValue() / TEXELS_PER_UNIT;
		else
			return(false);

		// If this block has lights, calculating the new bounding box for them.
		// If the old and new bounding boxes are different, then reset the
		// active lights in both the old and new bounding boxes.

		if (block_ptr->light_list != NULL) {
			compute_light_list_bounding_box(block_ptr->light_list,
				block_ptr->translation, new_min_column, new_min_row,
				new_min_level, new_max_column, new_max_row, new_max_level);
			if (old_min_column != new_min_column || 
				old_min_row != new_min_row || old_min_level != new_min_level ||
				old_max_column != new_max_column ||
				old_max_row != new_max_row || old_max_level != new_max_level) {
				reset_active_lights(old_min_column, old_min_row, old_min_level,
					old_max_column, old_max_row, old_max_level);
				reset_active_lights(new_min_column, new_min_row, new_min_level,
					new_max_column, new_max_row, new_max_level);
			}
		}
	}

	// If the field is "origin", the attribute is "x", "y" or "z", and the
	// block is structural and movable, set the approapiate coordinate of the
	// block's origin.

	else if (field == "origin" && block_def_ptr->type == STRUCTURAL_BLOCK
		&& block_def_ptr->movable) {
		if (attribute == "x")
			block_ptr->block_origin.x = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "y")
			block_ptr->block_origin.y = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "z")
			block_ptr->block_origin.z = value.floatValue() / TEXELS_PER_UNIT;
		else
			return(false);
	}

	// All other fields are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Call a pre-defined block method.
//------------------------------------------------------------------------------

static bool 
call_block_method(block *block_ptr, const skString& method,
				  skRValueArray& arguments, skRValue& returnValue)
{
	// If the block pointer is NULL, this this block has been removed from
	// the map and can no longer be accessed.

	if (block_ptr == NULL) {
		diagnose("Script attempting to call function on block that has been "
			"removed from the map");
		return(false);
	}

	// If the block is not a structural block nor movable, none of these 
	// methods apply to it.

	block_def *block_def_ptr = block_ptr->block_def_ptr;
	if (block_def_ptr->type != STRUCTURAL_BLOCK || !block_def_ptr->movable)
		return(false);

	// If the method is "reset_vertices", do just that and update the block.

	if (method == "reset_vertices") {
		if (arguments.entries() != 0)
			return(false);
		block_ptr->reset_vertices();
		block_ptr->update();
	}

	// If the method is "get_vertices", create a new vertex SimKin object with
	// a copy of the block's vertices in it.

	else if (method == "get_vertices") {
		vertex *vertex_list;
		skRValue *simkin_value_ptr;
		int index;

		// Make sure there aren't any arguments.

		if (arguments.entries() != 0)
			return(false);

		// Create a new vertex list, and copy the block's vertex list into it.

		if ((vertex_list = new vertex[block_ptr->vertices]) == NULL) {
			diagnose("Not enough memory to copy block vertices");
			return(false);
		}
		for (index = 0; index < block_ptr->vertices; index++)
			vertex_list[index] = block_ptr->vertex_list[index];

		// Now create a vertex SimKin object to hold the vertex list, and
		// return it.

		if ((simkin_value_ptr = create_vertex_simkin_object(NULL, 
			block_ptr->vertices, vertex_list)) != NULL)
			returnValue = *simkin_value_ptr;
		else {
			skRValue value;
			returnValue = value;
		}
	}

	// If the method is "set_vertices", copy the vertex list in the given
	// vertex SimKin object to the block's vertex list, and update the block.

	else if (method == "set_vertices") {
		vertex_simkin_object *vertex_simkin_object_ptr;
		int index;

		// Make sure there is one argument, and retrieve the SimKin vertex
		// object from it.  

		if (arguments.entries() != 1)
			return(false);
		vertex_simkin_object_ptr = (vertex_simkin_object *)arguments[0].obj();

		// If the number of vertices in the object don't match the number of
		// vertices in the block, we can't copy it.

		if (vertex_simkin_object_ptr->vertices != block_ptr->vertices) {
			diagnose("Cannot assign vertices to block because the vertex "
				"count is different");
			return(false);
		}

		// Copy the vertex list in the object to the block's vertex list,
		// and update the block data.

		for (index = 0; index < block_ptr->vertices; index++)
			block_ptr->vertex_list[index] = 
				vertex_simkin_object_ptr->vertex_list[index];
		block_ptr->update();
	}

	// If the method is "orient"...

	else if (method == "orient") {
		skString compass_direction;
		orientation block_orientation;

		// Make sure there aren't too few entries.

		if (arguments.entries() < 1)
			return(false);

		// If the first argument is a compass direction, this is the new
		// style of orientation.

		compass_direction = arguments[0].str();
		if (compass_direction == "up")
			block_orientation.direction = UP;
		else if (compass_direction == "down")
			block_orientation.direction = DOWN;
		else if (compass_direction == "north")
			block_orientation.direction = NORTH;
		else if (compass_direction == "south")
			block_orientation.direction = SOUTH;
		else if (compass_direction == "east")
			block_orientation.direction = EAST;
		else if (compass_direction == "west")
			block_orientation.direction = WEST;
		else
			block_orientation.direction = NONE;

		// If a compass direction was given and the second argument is present,
		// it must be an angle.

		if (block_orientation.direction != NONE) {
			if (arguments.entries() > 2)
				return(false);
			if (arguments.entries() == 2)
				block_orientation.angle = 
					pos_adjust_angle(arguments[1].floatValue());
			else
				block_orientation.angle = 0;
		}

		// Otherwise the first argument must be an angle around the Y axis, and
		// the second and third arguments, if present, must be angles around the
		// X and Z axes, in that order.

		else {
			if (arguments.entries() > 3)
				return(false);
			block_orientation.angle_y = 
				pos_adjust_angle(arguments[0].floatValue());
			if (arguments.entries() > 1) {
				block_orientation.angle_x =
					pos_adjust_angle(arguments[1].floatValue());
				if (arguments.entries() > 2)
					block_orientation.angle_z =
						pos_adjust_angle(arguments[2].floatValue());
				else
					block_orientation.angle_z = 0.0f;
			} else {
				block_orientation.angle_x = 0.0f;
				block_orientation.angle_z = 0.0f;
			}
		}

		// If this is a structual block, set it's new orientation.  All changes
		// to the vertices made prior to this method call are lost.

		block_ptr->block_orientation = block_orientation;
		block_ptr->reset_vertices();
		block_ptr->orient();
	}

	// If the method is "rotate_x", rotate the block around the X axis by the
	// angle given as an argument.

	else if (method == "rotate_x") {
		if (arguments.entries() != 1)
			return(false);
		block_ptr->rotate_x(pos_adjust_angle(arguments[0].floatValue()));
	}

	// If the method is "rotate_y", rotate the block around the Y axis by the
	// angle given as an argument.

	else if (method == "rotate_y") {
		if (arguments.entries() != 1)
			return(false);
		block_ptr->rotate_y(pos_adjust_angle(arguments[0].floatValue()));
	}

	// If the method is "rotate_z", rotate the block around the Z axis by the
	// angle given as an argument.

	else if (method == "rotate_z") {
		if (arguments.entries() != 1)
			return(false);
		block_ptr->rotate_z(pos_adjust_angle(arguments[0].floatValue()));
	}

	else if (method == "setframe") {
		if (arguments.entries() != 1)
			return(false);
		block_ptr->set_frame(arguments[0].intValue());
	}

	else if (method == "setloop") {
		if (arguments.entries() != 1)
			return(false);
		block_ptr->set_nextloop(arguments[0].intValue());
	}

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// The array SimKin object class.
//------------------------------------------------------------------------------

class array_simkin_object : public skExecutable
{
public:
	skRValueArray array;

	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool getValueAt(const skRValue& array_index, const skString& attribute, skRValue& value);
	bool setValueAt(const skRValue& array_index, const skString& attribute, const skRValue& value);
	bool method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt);
};

// Get the value of a global variable.

bool 
array_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the field is "elements", return the number of elements in the array.

	if (field == "elements") {
		value = skRValue(array.entries());
		return(true);
	}
	return(false);
}

// Get the value of an array element.

bool 
array_simkin_object::getValueAt(const skRValue& array_index, const skString& attribute, skRValue& value)
{
	int index;

	// Verify the array index is within range.

	index = array_index.intValue();
	if (index < 0 || index >= (int)array.entries())
		return(false);

	// Return the array element.

	value = array[index];
	return(true);
}

// Set the value of an array element.

bool 
array_simkin_object::setValueAt(const skRValue& array_index, const skString& attribute, const skRValue& value)
{
	int index;

	// Verify the array index is within range.

	index = array_index.intValue();
	if (index < 0 || index >= (int)array.entries())
		return(false);

	// Set the array element.

	array[index] = value;
	return(true);
}

// Execute an array method.

bool
array_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{	
	// If the method is "insert", insert the value in the second argument
	// before the index in the first argument.

	if (method == "insert") {
		if (arguments.entries() != 2)
			return(false);
		array.insert(arguments[1], arguments[0].intValue());
	}

	// If the method is "prepend", prepend the value in the first argument.
	// This is the same as insert(0, value).

	else if (method == "prepend") {
		if (arguments.entries() != 1)
			return(false);
		array.prepend(arguments[0]);
	}

	// If the method is "append", append the value in the first argument.

	else if (method == "append") {
		if (arguments.entries() != 1)
			return(false);
		array.append(arguments[0]);
	}

	// If the method is "delete", delete the value at the index in the first
	// argument.

	else if (method == "delete") {
		if (arguments.entries() != 1)
			return(false);
		array.deleteElt(arguments[0].intValue());
	}

	// All other method names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// The spot SimKin object class.
//------------------------------------------------------------------------------

class spot_simkin_object : public skElementExecutable
{
public:
	spot_simkin_object() : skElementExecutable() {};
	spot_simkin_object(const skString& scriptLocation, skInputSource& in, skExecutableContext& context) :
	  skElementExecutable(scriptLocation, in, context) {};

	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
	bool method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt);
};

// Get the value of a global variable.

bool 
spot_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the field is "title", return the current title string.

	if (field == "title") {
		skString title = get_title();
		value = title;
	} 
	
	// If the field is "time" and the attribute is "hour", "minute" or "second",
	// return that current time attribute.  If the attribute is "millisecond",
	// return the number of milliseconds since Windows started.

	else if (field == "time") {
		struct tm *time_ptr;
		time_t time_secs;

		time(&time_secs);
		time_ptr = localtime(&time_secs);
		if (attribute == "second")
			value = time_ptr->tm_sec;
		else if (attribute == "minute")
			value = time_ptr->tm_min;
		else if (attribute == "hour")
			value = time_ptr->tm_hour;
		else if (attribute == "millisecond")
			value = get_time_ms() - start_time_ms;
		else
			return(false);
	}

	// If the field is "user_can_move" or "user_can_look",
	// then return the approapiate global flag value.

	else if (field == "user_can_move")
		value = enable_player_translation;
	else if (field == "user_can_look")
		value = enable_player_rotation;
	
	// All other fields are passed to the default getValue function for
	// processing.
	
	else
		return(skElementExecutable::getValue(field, attribute, value));

	// Return a success indicator.

	return(true);
}

// Set the value of a global variable.

bool 
spot_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	// If the field is "title", set the current title.

	if (field == "title") {
		set_title("%s", (const char *)value.str());
		return(true);
	}

	// If the field is "user_can_move" or "user_can_look",
	// then set the approapiate global flag value.

	else if (field == "user_can_move") {
		enable_player_translation = value.boolValue();
		return(true);
	} else if (field == "user_can_look") {
		enable_player_rotation = value.boolValue();
		return(true);
	}

	// All other fields are passed to the default setValue function for
	// processing.

	return(skElementExecutable::setValue(field, attribute, value));
}

// Execute a global method.

bool
spot_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{
	// If the method is "log", write a message to the log file.

	if (method == "log") {
		if (arguments.entries() != 1)
			return(false);
		skString message = arguments[0].str();
		debug_message("%s\n", (char *)(const char *)message);
	}

	// If the method is "new_array", create a new array SimKin object and
	// return a pointer to it.

	else if (method == "new_array") {
		array_simkin_object *array_simkin_object_ptr;

		if (arguments.entries() != 0)
			return(false);
		NEW(array_simkin_object_ptr, array_simkin_object);
		returnValue = skRValue(array_simkin_object_ptr);
	}

	// If the method is "free_array", free the array SimKin object given as
	// an argument.

	else if (method == "free_array") {
		array_simkin_object *array_simkin_object_ptr;

		if (arguments.entries() != 1)
			return(false);
		array_simkin_object_ptr = (array_simkin_object *)arguments[0].obj();
		if (array_simkin_object_ptr != NULL)
			DEL(array_simkin_object_ptr, array_simkin_object);
	}

	// If the method is "show_spot_directory", then signal the plugin thread
	// to do just that.

	else if (method == "show_spot_directory")
		show_spot_directory.send_event(true);
	
	// All other methods are passed to the default method function for
	// processing.

	else
        return(skElementExecutable::method(method, arguments, returnValue, *simkin_context_ptr));

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// The math SimKin object class.
//------------------------------------------------------------------------------

class math_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt);
};

// Get the value of a math variable.

bool 
math_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the field is "pi", return the value of Pi.

	if (field == "pi")
		value = PI;

	// All other field names are errors.
	
	else
		return(false);

	// Indicate success.

	return(true);
}

bool
math_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{
	// If the method is "random", return a random number between the first
	// and second argument.

	if (method == "random") {
 		if (arguments.entries() != 2)
			return(false);
		int min = arguments[0].intValue();
		int max = arguments[1].intValue();
		returnValue = (int)(((float)rand() / RAND_MAX) * (max - min)) + min;
	}

	// If the method is "sqrt", return the square root of the first argument.

	else if (method == "sqrt") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)sqrt(arguments[0].floatValue());
	}

	// If the method is "sin", return the sine of the first argument.

	else if (method == "sin") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)sin(arguments[0].floatValue());
	}

	// If the method is "cos", return the cosine of the first argument.

	else if (method == "cos") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)cos(arguments[0].floatValue());
	}

	// If the method is "tan", return the tangent of the first argument.

	else if (method == "tan") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)tan(arguments[0].floatValue());
	}

	// If the method is "asin", return the arc sine of the first argument.

	else if (method == "asin") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)asin(arguments[0].floatValue());
	}

	// If the method is "acos", return the arc cosine of the first argument.

	else if (method == "acos") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)acos(arguments[0].floatValue());
	}

	// If the method is "atan", return the arc tangent of the first argument.

	else if (method == "atan") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)atan(arguments[0].floatValue());
	}

	// If the method is "rad", convert the first argument from degrees to
	// radians.

	else if (method == "rad") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = RAD(arguments[0].floatValue());
	}

	// If the method is "deg", convert the first argument from radius to
	// degrees.

	else if (method == "deg") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = DEG(arguments[0].floatValue());
	}

	// The the method is "abs", return the absolute value of the first argument.

	else if (method == "abs") {
		if (arguments.entries() != 1)
			return(false);
		returnValue = (float)fabs(arguments[0].floatValue());
	}

	// All other method names are errors.

	else
        return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// The sky SimKin object class.
//------------------------------------------------------------------------------

class sky_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
};

// Get the value of a sky variable.

bool 
sky_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the field is "brightness", return the sky brightness value as a
	// percentage.

	if (field == "brightness")
		value = sky_brightness * 100.0f;

	// If the field is "colour", and the attribute is "red", "green" or "blue",
	// return the approapiate component of the unlit sky colour.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			value = unlit_sky_colour.red;
		else if (attribute == "green")
			value = unlit_sky_colour.green;
		else if (attribute == "blue")
			value = unlit_sky_colour.blue;
		else
			return(false);
	}
	
	// If the field is "texture", return the URL of the sky texture, or an
	// empty string if there is no sky texture.

	else if (field == "texture") {
		skString texture_URL;
		if (sky_texture_ptr != NULL)
			texture_URL = sky_texture_ptr->URL;
		value = texture_URL;
	} 
	
	// All other field names are errors.
	
	else
		return(false);

	// Indicate success.

	return(true);
}

// Set the value of a sky variable.

bool 
sky_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	// If the field is "brightness", set the sky brightness from a percentage,
	// clamping it to a value between 0 and 1.

	if (field == "brightness") {
		sky_brightness = value.floatValue() / 100.0f;
		if (sky_brightness < 0.0f)
			sky_brightness = 0.0f;
		else if (sky_brightness > 1.0f)
			sky_brightness = 1.0f;
	}

	// If the field is "colour", set the unlit sky colour.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			unlit_sky_colour.red = value.floatValue();
		else if (attribute == "green")
			unlit_sky_colour.green = value.floatValue();
		else if (attribute == "blue")
			unlit_sky_colour.blue = value.floatValue();
		else
			return(false);
	} 
	
	// If the field is "texture", load the custom sky texture.

	else if (field == "texture") {
		load_custom_texture(custom_sky_texture_ptr, (const char *)value.str());
		return(true);
	}

	// All other field names are errors.

	else
		return(false);

	// Update the sky brightness and colour, and related values.

	unlit_sky_colour.clamp();
	sky_brightness_index = get_brightness_index(sky_brightness);
	sky_colour = unlit_sky_colour;
	sky_colour.adjust_brightness(sky_brightness);
	sky_colour_pixel = RGB_to_display_pixel(sky_colour);
	return(true);
}

//------------------------------------------------------------------------------
// The ambient_light SimKin object class.
//------------------------------------------------------------------------------

class ambient_light_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
};

// Get the value of an ambient light variable.

bool 
ambient_light_simkin_object::getValue(const skString& field, 
									  const skString& attribute,
									  skRValue& value)
{
	float brightness;
	RGBcolour colour;

	// Get the current ambient light data.

	get_ambient_light(&brightness, &colour);

	// If the field is "brightness", return the ambient brightness as a
	// percentage.

	if (field == "brightness")
		value = brightness * 100.0f;

	// If the field is "colour" or "color", and the attribute is "red", "green"
	// or "blue", return the approapiate ambient colour component.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			value = colour.red;
		else if (attribute == "green")
			value = colour.green;
		else if (attribute == "blue")
			value = colour.blue;
		else
			return(false);
	} 
	
	// All other field names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

// Set the value of an ambient light variable.

bool 
ambient_light_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	float brightness;
	RGBcolour colour;

	// Get the current ambient light data.

	get_ambient_light(&brightness, &colour);

	// If the field is "brightness, set the ambient brightness from a
	// percentage, clamped to a value between 0 and 1.

	if (field == "brightness") {
		brightness = value.floatValue() / 100.0f;
		if (brightness < 0.0f)
			brightness = 0.0f;
		else if (brightness > 1.0f)
			brightness = 1.0f;
	} 
	
	// If the field is "colour" or "color" and the attribute is "red", "green"
	// or "blue", set the approapiate ambient colour component.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			colour.red = value.floatValue();
		else if (attribute == "green")
			colour.green = value.floatValue();
		else if (attribute == "blue")
			colour.blue = value.floatValue();
		else
			return(false);
	} 
	
	// All other field names are errors.

	else
		return(false);

	// Update the brightness and colour of the ambient light.

	colour.clamp();
	set_ambient_light(brightness, colour);
	return(true);
}

//------------------------------------------------------------------------------
// The ambient_sound SimKin object class.
//------------------------------------------------------------------------------

class ambient_sound_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
};

// Get the value of an ambient sound variable.

bool 
ambient_sound_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{	
	// If the ambient sound does not exist, then none of the fields or
	// field attributes are accessible.

	if (ambient_sound_ptr == NULL)
		return(false);

	// If the field is "file", return the URL of the wave file, or an empty
	// string if there is no wave file.

	if (field == "file") {
		skString wave_URL;
		if (ambient_sound_ptr->wave_ptr != NULL)
			wave_URL = ambient_sound_ptr->wave_ptr->URL;
		value = wave_URL;
	}

	// If the field is "volume", return the volume of the ambient sound as a
	// percentage.

	else if (field == "volume")
		value = ambient_sound_ptr->volume * 100.0f;

	// If the field is "playback", return the playback mode.

	else if (field == "playback") {
		skString playback_mode(attribute_value_to_string(VALUE_PLAYBACK_MODE,
			ambient_sound_ptr->playback_mode));
		value = playback_mode;
	}

	// If the field is "delay" and the attribute is "minimum" or "range", get
	// the corresponding value. 

	else if (field == "delay") {
		if (attribute == "" || attribute == "minimum")
			value = (float)ambient_sound_ptr->delay_range.min_delay_ms / 
				1000.0f;
		else if (attribute == "range")
			value = (float)ambient_sound_ptr->delay_range.delay_range_ms /
				1000.0f;
		else
			return(false);
	}

	// All other field names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

// Set the value of an ambient sound variable.

bool 
ambient_sound_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	float volume;
	skString value_str;

	// If the ambient sound does not exist, then none of the fields or
	// field attributes are accessible.

	if (ambient_sound_ptr == NULL)
		return(false);

	// If the field is "file", set the ambient sound wave (after destroying the
	// existing sound buffe), and if it needs to be downloaded and no wave file
	// downloading is in progress, then start it up.
	
	if (field == "file") {
		stop_sound(ambient_sound_ptr);
		ambient_sound_ptr->in_range = false;
		destroy_sound_buffer(ambient_sound_ptr);
		load_custom_wave(ambient_sound_ptr->wave_ptr, 
			(const char *)value.str());
	}

	// If the field is "volume", set the volume of the ambient sound from a
	// percentage, clamping it to a value between 0 and 1.

	else if (field == "volume") {
		volume = value.floatValue() / 100.0f;
		if (volume < 0.0f)
			volume = 0.0f;
		else if (volume > 1.0f)
			volume = 1.0f;
		ambient_sound_ptr->volume = volume;
	}

	// If the field is "playback", set the playback mode, clear the played
	// once and in range flags, and stop the sound.

	else if (field == "playback") {
		value_str = value.str();
		if (parse_playback_mode((char *)(const char *)value_str, 
			&ambient_sound_ptr->playback_mode)) {
			ambient_sound_ptr->played_once = false;
			ambient_sound_ptr->in_range = false;
			stop_sound(ambient_sound_ptr);
		}
	}

	// If the field is "delay", set the minimum delay to the specified value and
	// the delay range to zero.  If an attribute of "minimum" or "range" is 
	// supplied, set the corresponding value. 

	else if (field == "delay") {
		if (attribute == "") {
			ambient_sound_ptr->delay_range.min_delay_ms = 
				(int)(value.floatValue() * 1000.0f);
			ambient_sound_ptr->delay_range.delay_range_ms = 0;
		} else if (attribute == "minimum")
			ambient_sound_ptr->delay_range.min_delay_ms =
				(int)(value.floatValue() * 1000.0f);
		else if (attribute == "range")
			ambient_sound_ptr->delay_range.delay_range_ms =
				(int)(value.floatValue() * 1000.0f);
		else
			return(false);
	}

	// All other field names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// The orb SimKin object class.
//------------------------------------------------------------------------------

class orb_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
};

// Get the value of an orb variable.

bool 
orb_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the orb does not exist, then none of the fields or field attributes
	// are accessible.

	if (orb_light_ptr == NULL)
		return(false);

	// If the field was "brightness", return the orb brightness as a
	// percentage.

	if (field == "brightness")
		value = orb_brightness * 100.0f;

	// If the field was "colour" and the attribute was "red", "green" or "blue",
	// return the approapiate component of the orb's light colour.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			value = orb_light_ptr->colour.red;
		else if (attribute == "green")
			value = orb_light_ptr->colour.green;
		else if (attribute == "blue")
			value = orb_light_ptr->colour.blue;
		else
			return(false);
	} 
	
	// If the field was "position" and the attribute was "angle_x" or "angle_y",
	// return the approapiate orb direction angle.

	else if (field == "position") {
		if (attribute == "angle_x")
			value = orb_direction.angle_x;
		else if (attribute == "angle_y")
			value = orb_direction.angle_y;
		else
			return(false);
	}
		
	// If the field is "texture", return the URL of the orb texture, or an
	// empty string if there is no orb texture.

	else if (field == "texture") {
		if (orb_texture_ptr != NULL) {
			skString texture_URL(orb_texture_ptr->URL);
			value = texture_URL;
		} else {
			skString texture_URL("");
			value = texture_URL;
		}
	}

	// If the field is "href", return the URL of the orb exit, or an empty
	// string if there is no URL set.

	else if (field == "href") {
		if (orb_exit_ptr != NULL) {
			skString exit_URL(orb_exit_ptr->URL);
			value = exit_URL;
		} else {
			skString exit_URL("");
			value = exit_URL;
		}
	}

	// If the field is "target", return the window target of the orb exit,
	// or an empty string if there is no target set.

	else if (field == "target") {
		if (orb_exit_ptr != NULL) {
			skString exit_URL(orb_exit_ptr->target);
			value = exit_URL;
		} else {
			skString exit_URL("");
			value = exit_URL;
		}
	}

	// If the field is "text", return the label of the orb exit, or an empty 
	// string if there is no label set.

	else if (field == "text") {
		if (orb_exit_ptr != NULL) {
			skString exit_URL(orb_exit_ptr->label);
			value = exit_URL;
		} else {
			skString exit_URL("");
			value = exit_URL;
		}
	}

	// All other field names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

// Set the value of an orb variable.

bool 
orb_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	direction orb_light_direction;

	// If the orb does not exist, then none of the fields or field attributes
	// are accessible.

	if (orb_light_ptr == NULL)
		return(false);

	// If the field is "brightness", set the orb brightness from a percentage,
	// clamped to a value between 0 and 1.

	if (field == "brightness") {
		orb_brightness = value.floatValue() / 100.0f;
		if (orb_brightness < 0.0f)
			orb_brightness = 0.0f;
		else if (orb_brightness > 1.0f)
			orb_brightness = 1.0f;
	} 
	
	// If the field is "colour" and the attribute is "red", "green" or "blue",
	// set the approapiate component of the orb's light colour.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			orb_light_ptr->colour.red = value.floatValue();
		else if (attribute == "green")
			orb_light_ptr->colour.green = value.floatValue();
		else if (attribute == "blue")
			orb_light_ptr->colour.blue = value.floatValue();
		else
			return(false);
	} 
	
	// If the field is "position", and the attribute is "angle_x" or "angle_y",
	// set the approapiate orb direction angle.

	else if (field == "position") {
		if (attribute == "angle_x")
			orb_direction.angle_x = value.floatValue();
		else if (attribute == "angle_y")
			orb_direction.angle_y = value.floatValue();
		else
			return(false);
	} 
	
	// If the field is "texture", load the custom orb texture.

	else if (field == "texture") {
		load_custom_texture(custom_orb_texture_ptr, (const char *)value.str());
		return(true);
	} 
	
	// If the field is "href", set the URL of the orb exit, creating the exit
	// if necessary.

	else if (field == "href") {
		if (orb_exit_ptr == NULL) {
			NEW(orb_exit_ptr, hyperlink);
			if (orb_exit_ptr == NULL)
				memory_warning("orb exit");
		} else
			orb_exit_ptr->URL = (const char *)value.str();
		return(true);
	}

	// If the field is "target", set the window target of the orb exit,
	// creating the exit if necessary.

	else if (field == "target") {
		if (orb_exit_ptr == NULL) {
			NEW(orb_exit_ptr, hyperlink);
			if (orb_exit_ptr == NULL)
				memory_warning("orb exit");
		} else
			orb_exit_ptr->target = (const char *)value.str();
		return(true);
	}

	// If the field is "text", return the label of the orb exit, or an empty 
	// string if there is no label set.

	else if (field == "text") {
		if (orb_exit_ptr == NULL) {
			NEW(orb_exit_ptr, hyperlink);
			if (orb_exit_ptr == NULL)
				memory_warning("orb exit");
		} else
			orb_exit_ptr->label = (const char *)value.str();
		return(true);
	}

	// All other field names are errors.

	else
		return(false);

	// Update the orb brightness, colour and direction.

	orb_light_ptr->colour.clamp();
	orb_light_ptr->set_intensity(orb_brightness);
	orb_brightness_index = get_brightness_index(orb_brightness);
	orb_light_direction.set(-orb_direction.angle_x, 
		orb_direction.angle_y + 180.0f);
	orb_light_ptr->set_direction(orb_light_direction);
	return(true);
}


//------------------------------------------------------------------------------
// The fog SimKin object class.
//------------------------------------------------------------------------------

class fog_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
};

// Get the value of an fog variable.

bool 
fog_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the fog does not exist, then none of the fields or field attributes
	// are accessible.

	if (!global_fog_enabled)
		return(false);

	// If the field was "style", return the fog style

	if (field == "style")
			value = global_fog.style;

	// If the field was "colour" and the attribute was "red", "green" or "blue",
	// return the appropriate component of the fog's light colour.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			value = global_fog.colour.red;
		else if (attribute == "green")
			value = global_fog.colour.green;
		else if (attribute == "blue")
			value = global_fog.colour.blue;
		else
			return(false);
	} 
	
	// If the field was "radius" 
	// return the fog radius.

	else if (field == "startradius") {
			value = global_fog.start_radius;
	}

	else if (field == "endradius") {
			value = global_fog.end_radius;
	}
		
	// If the field is "density", return the fog density

	else if (field == "density") {
			value = global_fog.density * 100.0f;
	}

	// All other field names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

// Set the value of an fog variable.

bool 
fog_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{

	// If the fog does not exist, then none of the fields or field attributes
	// are accessible.

	if (!global_fog_enabled)
		return(false);

	// If the field was "style", set the fog style
	if (field == "style") {
		if (value.intValue() == EXPONENTIAL_FOG || value.intValue() == LINEAR_FOG)
			global_fog.style = value.intValue() ;
	}

	// If the field was "colour" and the attribute was "red", "green" or "blue",
	// set the appropriate component of the fog's light colour.

	else if (field == "colour" || field == "color") {
		if (attribute == "red")
			global_fog.colour.red = value.floatValue();
		else if (attribute == "green")
			global_fog.colour.green = value.floatValue();
		else if (attribute == "blue")
			global_fog.colour.blue = value.floatValue();
		else
			return(false);
	} 
	
	// If the field was "radius" 
	// set the fog radius.

	else if (field == "startradius") {
			global_fog.start_radius = value.floatValue();
	}

	else  if (field == "endradius") {
			global_fog.end_radius = value.floatValue();
	}
		
	// If the field is "density", set the fog density

	else if (field == "density") {
		global_fog.density = value.floatValue() / 100.0f;
		if (global_fog.density < 0.0f)
			global_fog.density = 0.0f;
		else if (global_fog.density > 1.0f)
			global_fog.density = 1.0f;
	}

	// All other field names are errors.

	else
		return(false);

	// Update the fog setting in hardware

	if (hardware_acceleration && global_fog_enabled) {
		hardware_disable_fog();
		hardware_enable_fog();
		hardware_update_fog_settings(&global_fog);
	}

	return(true);
}


//------------------------------------------------------------------------------
// The map SimKin object class.
//------------------------------------------------------------------------------

class map_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool method(const skString& method_name, skRValueArray& arguments, skRValue& return_value, skExecutableContext& ctxt);
};

// Get the value of an map variable.

bool 
map_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// If the field is "dimensions" and the attribute is "column", "row" or
	// "level", return the appropiate map dimension.

	if (field == "dimensions") {
		if (attribute == "columns")
			value = world_ptr->columns;
		else if (attribute == "rows")
			value = world_ptr->rows;
		else if (attribute == "levels") {
			if (world_ptr->ground_level_exists)
				value = world_ptr->levels - 2;
			else
				value = world_ptr->levels - 1;
		} else
			return(false);
	} else
		return(false);

	// Indicate success.

	return(true);
}

// Execute a map method.

bool
map_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{
	// If the method is "get_block", return a pointer to the SimKin object
	// associated with the block with the given symbol or at the given 
	// location, otherwise return a NULL object.

    if (method == "get_block") {

		// If there is one argument, assume it's a symbol...

		if (arguments.entries() == 1) {
			skString symbol_str;
			word symbol;
			block_def *block_def_ptr;
			block *block_ptr;

			// Parse the symbol, obtain a pointer to it's block definition,
			// and obtain a pointer to the first block in the used block list.

			symbol_str = arguments[0].str();
			block_ptr = NULL;
			if (string_to_symbol(symbol_str, &symbol, false)) {
				block_def_ptr = block_symbol_table[symbol];
				block_ptr = block_def_ptr->used_block_list;
			}

			// If the block exists, return it's SimKin object.  Otherwise
			// return a null object.

			if (block_ptr != NULL)
				returnValue = get_block_simkin_object(block_ptr);
			else {
				skRValue value;
				returnValue = value;
			}
		}

		// If there are 3 arguments, assume it's a location...

		else if (arguments.entries() == 3) {
			int column, row, level;
			int block_column, block_row, block_level;
			square *square_ptr;
			block *block_ptr;

			column = arguments[0].intValue() - 1;
			row = arguments[1].intValue() - 1;
			level = arguments[2].intValue() - 1;
 			if (world_ptr->ground_level_exists)
				level++;
			if ((square_ptr = world_ptr->get_square_ptr(column, row, level)) 
				!= NULL && (block_ptr = square_ptr->block_ptr) != NULL)
				returnValue = get_block_simkin_object(block_ptr);
			else {
				skRValue value;
				returnValue = value;
			}

			// Get a pointer to the square at this location, and return the
			// SimKin object of the block occupying that square, if there is
			// one.

			column = arguments[0].intValue() - 1;
			row = arguments[1].intValue() - 1;
			level = arguments[2].intValue() - 1;
 			if (world_ptr->ground_level_exists)
				level++;
			if ((square_ptr = world_ptr->get_square_ptr(column, row, level)) 
				!= NULL && (block_ptr = square_ptr->block_ptr) != NULL)
				returnValue = get_block_simkin_object(block_ptr);

			// Otherwise step through the movable block list, and return the
			// first block on the given square.  If none is found, return a
			// null object.

			else {
				block_ptr = movable_block_list;
				while (block_ptr != NULL) {
					block_ptr->translation.get_map_position(&block_column, 
						&block_row, &block_level);
					if (block_column == column && block_row == row &&
						block_level == level) {
						returnValue = get_block_simkin_object(block_ptr);
						return(true);
					}
					block_ptr = block_ptr->next_block_ptr;
				}
				skRValue value;
				returnValue = value;
			}
		}
			
		// Any other number of arguments is an error.

		else
			return(false);
	}

	// If the method is "get_blocks"...

	else if (method == "get_blocks") {
		array_simkin_object *array_simkin_object_ptr;

		// If there is one argument, assume it's a symbol...

		if (arguments.entries() == 1) {
			skString symbol_str;
			word symbol;
			block_def *block_def_ptr;
			block *block_ptr;

			// Create an empty array.

			NEW(array_simkin_object_ptr, array_simkin_object);

			// Parse the symbol, and obtain a pointer to it's block definition.

			symbol_str = arguments[0].str();
			if (string_to_symbol(symbol_str, &symbol, false)) {
				block_def_ptr = block_symbol_table[symbol];

				// Insert all blocks that have been created from this block
				// definition into the array.

				block_ptr = block_def_ptr->used_block_list;
				while (block_ptr != NULL) {
					array_simkin_object_ptr->array.append(
						get_block_simkin_object(block_ptr));
					block_ptr = block_ptr->next_used_block_ptr;
				}
			}

			// Return the array.

			returnValue = skRValue(array_simkin_object_ptr);
		}

		// If there are three arguments, assume it's a map location.

		else if (arguments.entries() == 3) {
			int column, row, level;
			int block_column, block_row, block_level;
			square *square_ptr;
			block *block_ptr;

			// Create an empty array.

			NEW(array_simkin_object_ptr, array_simkin_object);

			// Get a pointer to the square at this location, and add the block
			// occupying that square to the array, if there is one.

			column = arguments[0].intValue() - 1;
			row = arguments[1].intValue() - 1;
			level = arguments[2].intValue() - 1;
 			if (world_ptr->ground_level_exists)
				level++;
			if ((square_ptr = world_ptr->get_square_ptr(column, row, level)) 
				!= NULL && (block_ptr = square_ptr->block_ptr) != NULL)
				array_simkin_object_ptr->array.append(
					get_block_simkin_object(block_ptr));

			// Now step through the movable block list, and add all blocks
			// that are on this square to the array.

			block_ptr = movable_block_list;
			while (block_ptr != NULL) {
				block_ptr->translation.get_map_position(&block_column, 
					&block_row, &block_level);
				if (block_column == column && block_row == row &&
					block_level == level)
					array_simkin_object_ptr->array.append(
						get_block_simkin_object(block_ptr));
				block_ptr = block_ptr->next_block_ptr;
			}

			// Return the array.

			returnValue = skRValue(array_simkin_object_ptr);
		}
	}

	// If the method is "set_block", create a new block based upon the block
	// definition with the given symbol, and place it on the map at the given
	// location.  The symbol "." or ".." causes the block at the given
	// location to be removed instead of added.

	else if (method == "set_block") {
		int column, row, level;
		skString symbol_str;
		square *square_ptr;
		word symbol;
		block_def *block_def_ptr;
		vertex translation;

		// Parse the arguments.

		if (arguments.entries() != 4)
			return(false);
        column = arguments[0].intValue() - 1;
		row = arguments[1].intValue() - 1;
		level = arguments[2].intValue() - 1;
	 	if (world_ptr->ground_level_exists)
			level++;
		symbol_str = arguments[3].str();

		// If the map location is valid...

		if ((square_ptr = world_ptr->get_square_ptr(column, row, level)) 
			!= NULL) {

			// Remove the current block from this location.

			remove_fixed_block(square_ptr);

			// If the symbol is valid, create a new block based upon the
			// block definition with that symbol, and add it to the map
			// at the same location.

			if (string_to_symbol(symbol_str, &symbol, false) &&
				(block_def_ptr = symbol_to_block_def(symbol)) != NULL) {
				if (block_def_ptr->movable) {
					translation.set_map_translation(column, row, level);
					add_movable_block(block_def_ptr, translation);
				} else
					add_fixed_block(block_def_ptr, square_ptr, true);
			}
		}
	}

	// If the method is "move_block", move a block from one location to
	// another.  If the source location is empty, the target location will
	// also become empty.

	else if (method == "move_block") {
		int source_column, source_row, source_level;
		int target_column, target_row, target_level;
		square *source_square_ptr, *target_square_ptr;
		block *block_ptr;
		trigger *trigger_ptr;
		vertex translation, relative_translation;
		int min_column, min_row, min_level;
		int max_column, max_row, max_level;

		// Parse the arguments.

		if (arguments.entries() != 6)
			return(false);
        source_column = arguments[0].intValue() - 1;
		source_row = arguments[1].intValue() - 1;
		source_level = arguments[2].intValue() - 1;
        target_column = arguments[3].intValue() - 1;
		target_row = arguments[4].intValue() - 1;
		target_level = arguments[5].intValue() - 1;
		if (world_ptr->ground_level_exists) {
			source_level++;	
			target_level++;
		}

		// Get the squares at the source and target locations.

		source_square_ptr = world_ptr->get_square_ptr(source_column, source_row,
			source_level);
		target_square_ptr = world_ptr->get_square_ptr(target_column, target_row,
			target_level);

		// If the target location was invalid, then simply remove the block
		// at the source location (assuming it was valid and there is a block
		// there, otherwise there is nothing to do).

		if (target_square_ptr == NULL) {
			if (source_square_ptr != NULL && 
				source_square_ptr->block_ptr != NULL)
				remove_fixed_block(source_square_ptr);
		}

		// If the target location was valid...

		else {
			
			// If the source location is valid and contains a block, remove the
			// block from the square without actually deleting it.

			if (source_square_ptr != NULL && 
				source_square_ptr->block_ptr != NULL) {
				block_ptr = source_square_ptr->block_ptr;
				source_square_ptr->block_ptr = NULL;

				// Reset the active polygons adjacent to this square.

				reset_active_polygons(source_column, source_row, source_level);

				// If the player is standing on the source square, set a flag 
				// indicating that the player block has been replaced.

				if (player_column == source_column && 
					player_row == source_row && player_level == source_level)
					player_block_replaced = true;

				// If this block has a light list, calculate the bounding box
				// emcompassing all these lights, and reset the active light 
				// lists for all blocks inside this bounding box.

				if (block_ptr->light_list != NULL) {
					compute_light_list_bounding_box(block_ptr->light_list, 
						block_ptr->translation, min_column, min_row, min_level,
						max_column, max_row, max_level);
					reset_active_lights(min_column, min_row, min_level, 
						max_column, max_row, max_level);
				}
			}

			// If the source location is invalid is there was no block on it,
			// use a NULL block pointer for the next step.

			else
				block_ptr = NULL;

			// Remove any fixed block that may exist at the target location.

			remove_fixed_block(target_square_ptr);

			// Now place the block from the source square onto the target
			// square, if there is one.

			if (block_ptr != NULL) {
				target_square_ptr->block_ptr = block_ptr;

				// Step through the trigger list, and update the square and
				// block pointers of each one.

				trigger_ptr = block_ptr->trigger_list;
				while (trigger_ptr != NULL) {
					trigger_ptr->block_ptr = block_ptr;
					trigger_ptr->square_ptr = target_square_ptr;
					trigger_ptr = trigger_ptr->next_trigger_ptr;
				}

				// Update the block's translation.

				translation.set_map_translation(target_column, target_row, 
					target_level);
				block_ptr->translation = translation;

				// Compute the active polygons surrounding the block.

				compute_active_polygons(block_ptr, target_column, target_row,
					target_level, true);

				// If the player is standing on the target square, set a flag 
				// indicating that the player block has been replaced.

				if (player_column == target_column && 
					player_row == target_row && player_level == target_level)
					player_block_replaced = true;

				// If this block has a light list, calculate the bounding box
				// emcompassing all these lights, and reset the active light 
				// lists for all blocks inside this bounding box.

				if (block_ptr->light_list != NULL) {
					compute_light_list_bounding_box(block_ptr->light_list, 
						block_ptr->translation, min_column, min_row, min_level,
						max_column, max_row, max_level);
					reset_active_lights(min_column, min_row, min_level, 
						max_column, max_row, max_level);
				}
			}
		}
	}
	
	// All other method names are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// The scripted block SimKin object class.
//------------------------------------------------------------------------------

class scripted_block_simkin_object : public skElementExecutable
{
public:
	block *block_ptr;

	scripted_block_simkin_object() : skElementExecutable() {};
	scripted_block_simkin_object(const skString& scriptLocation, skInputSource& in, skExecutableContext& context) :
		skElementExecutable(scriptLocation, in, context) {};

	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
	bool method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt);
};

// Get the value of a block variable.

bool 
scripted_block_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	// Check whether a pre-defined field/attribute has been requested.
	// If not, call the default getValue function.

	if (!get_block_value(block_ptr, field, attribute, value))
		return(skElementExecutable::getValue(field, attribute, value));

	// Return a success indicator.

	return(true);
}

// Set the value of a block variable.

bool 
scripted_block_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	// Check whether a pre-defined field/attribute has been requested.
	// All other fields are passed to the default setValue function for
	// processing.

	if (!set_block_value(block_ptr, field, attribute, value))
		return(skElementExecutable::setValue(field, attribute, value));

	// Return a success indicator.

	return(true);
}

// Call a method on a block.

bool 
scripted_block_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{
	// Check whether a pre-defined method has been requested.  All other methods
	// are passed to the default method function for processing.

	if (!call_block_method(block_ptr, method, arguments, returnValue))
		return(skElementExecutable::method(method, arguments, returnValue, *simkin_context_ptr));

	// Return a success indicator.

	return(true);
}

//------------------------------------------------------------------------------
// The unscripted block SimKin object class.
//------------------------------------------------------------------------------

class unscripted_block_simkin_object : public skExecutable
{
public:
	block *block_ptr;

	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
	bool method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt);
};

// Get the value of a block variable.

bool 
unscripted_block_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{	
	return(get_block_value(block_ptr, field, attribute, value));
}

// Set the value of a block variable.

bool 
unscripted_block_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	return(set_block_value(block_ptr, field, attribute, value));
}

// Call a method on a block.

bool 
unscripted_block_simkin_object::method(const skString& method, skRValueArray& arguments, skRValue& returnValue, skExecutableContext& ctxt)
{
	return(call_block_method(block_ptr, method, arguments, returnValue));
}

//------------------------------------------------------------------------------
// The player SimKin object class.
//------------------------------------------------------------------------------

class player_simkin_object : public skExecutable
{
public:
	bool getValue(const skString& field, const skString& attribute, skRValue& value);
	bool setValue(const skString& field, const skString& attribute, const skRValue& value); 
};

// Get the value of a block variable.

bool 
player_simkin_object::getValue(const skString& field, const skString& attribute, skRValue& value)
{
	int key_code_list[2];

	// If the field is "location" and the attribute is "column", "row" or
	// "level", or "x", "y" or "z", return the approapiate coordinate of the 
	// player

	if (field == "location") {
		vertex translation;
		int column, row, level;

		// Determine the map position of the player.

		player_viewpoint.position.get_map_position(&column, &row, &level);
	 	if (world_ptr->ground_level_exists)
			level--;

		// Now return the desired value.

		if (attribute == "column")
			value = column + 1;
		else if (attribute == "row")
			value = row + 1;
		else if (attribute == "level")
			value = level + 1;
		else if (attribute == "x")
			value = player_viewpoint.position.x * TEXELS_PER_UNIT;
		else if (attribute == "y")
			value = player_viewpoint.position.y * TEXELS_PER_UNIT;
		else if (attribute == "z")
			value = player_viewpoint.position.z * TEXELS_PER_UNIT;
		else
			return(false);
	}
	
	// If the field is "orientation" and the attribute is "turn_angle" or
	// "look_angle", return the angle around the appropiate axis.

	else if (field == "orientation") {
		if (attribute == "look_angle")
			value = player_viewpoint.look_angle;
		else if (attribute == "turn_angle")
			value = player_viewpoint.turn_angle;
		else
			return(false);
	}

	// If the field is "camera" and the attribute is "x", "y" or "z",
	// return the appropiate camera offset.

	else if (field == "camera") {
		if (attribute == "x")
			value = player_camera_offset.dx * TEXELS_PER_UNIT;
		else if (attribute == "y")
			value = player_camera_offset.dy * TEXELS_PER_UNIT;
		else if (attribute == "z")
			value = player_camera_offset.dz * TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "camera" and the attribute is "x", "y" or "z",
	// return the appropiate camera offset.

	else if (field == "camera") {
		if (attribute == "x")
			value = player_camera_offset.dx * TEXELS_PER_UNIT;
		else if (attribute == "y")
			value = player_camera_offset.dy * TEXELS_PER_UNIT;
		else if (attribute == "z")
			value = player_camera_offset.dz * TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "size" and the attribute is "x", "y" or "z",
	// return the appropiate player dimension.

	else if (field == "size") {
		if (attribute == "x")
			value = player_dimensions.x * TEXELS_PER_UNIT;
		else if (attribute == "y")
			value = player_dimensions.y * TEXELS_PER_UNIT;
		else if (attribute == "z")
			value = player_dimensions.z * TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "move_forward", "move_back", "move_left", "move_right",
	// "look_up", "look_down", "sidle_mode", "fast_mode", "go_faster" or
	// "go_slower", get the key codes for the associated key function.

	else if (field == "move_forward") {
		get_key_codes(MOVE_FORWARD, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "move_back") {
		get_key_codes(MOVE_BACK, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "move_left") {
		get_key_codes(MOVE_LEFT, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "move_right") {
		get_key_codes(MOVE_RIGHT, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "look_up") {
		get_key_codes(LOOK_UP, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "look_down") {
		get_key_codes(LOOK_DOWN, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "sidle_mode") {
		get_key_codes(SIDLE_MODE, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "fast_mode") {
		get_key_codes(FAST_MODE, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "go_faster") {
		get_key_codes(GO_FASTER, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "go_slower") {
		get_key_codes(GO_SLOWER, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	} else if (field == "jump") {
		get_key_codes(JUMP, key_code_list);
		skString value_str(key_codes_to_string(key_code_list));
		value = value_str;
	}

	// Indicate success.

	return(true);
}

// Set the value of a block variable.

bool 
player_simkin_object::setValue(const skString& field, const skString& attribute, const skRValue& value)
{
	int key_code_list[2];

	// If the field is "location", the attribute is "x", "y" or "z", and the
	// block is movable, set the approapiate coordinate of the player.

	if (field == "location") {
		if (attribute == "x")
			player_viewpoint.position.x = value.floatValue() / 
				TEXELS_PER_UNIT;
		else if (attribute == "y")
			player_viewpoint.position.y = value.floatValue() / 
				TEXELS_PER_UNIT;
		else if (attribute == "z")
			player_viewpoint.position.z = value.floatValue() / 
				TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "orientation", and the attribute is "look_angle" or
	// "turn_angle" set the appropiate angle of the player.

	else if (field == "orientation") {

		// Set the new orientation angle.

		if (attribute == "look_angle")
			player_viewpoint.look_angle = neg_adjust_angle(value.floatValue());
		else if (attribute == "turn_angle")
			player_viewpoint.turn_angle = pos_adjust_angle(value.floatValue());
		else
			return(false);
	}

	// If the field is "camera" and the attribute is "x", "y" or "z",
	// set the appropiate camera offset.

	else if (field == "camera") {
		if (attribute == "x")
			player_camera_offset.dx = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "y")
			player_camera_offset.dy = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "z")
			player_camera_offset.dz = value.floatValue() / TEXELS_PER_UNIT;
		else
			return(false);
	}

	// If the field is "size", the player does not have a block, and the
	// attribute is "x", "y" or "z", set the appropiate player dimension.

	else if (field == "size" && player_block_ptr == NULL) {
		if (attribute == "x")
			player_dimensions.x = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "y")
			player_dimensions.y = value.floatValue() / TEXELS_PER_UNIT;
		else if (attribute == "z")
			player_dimensions.z = value.floatValue() / TEXELS_PER_UNIT;
		else
			return(false);

		// Update the player collision box and step height.

		set_player_size();
	}

	// If the field is "move_forward", "move_back", "move_left", "move_right",
	// "look_up", "look_down", "sidle_mode", "fast_mode", "go_faster" or
	// "go_slower", set the key codes for the associated key function.

	else if (field == "move_forward") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(MOVE_FORWARD, key_code_list);
	} else if (field == "move_back") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(MOVE_BACK, key_code_list);
	} else if (field == "move_left") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(MOVE_LEFT, key_code_list);
	} else if (field == "move_right") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(MOVE_RIGHT, key_code_list);
	} else if (field == "look_up") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(LOOK_UP, key_code_list);
	} else if (field == "look_down") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(LOOK_DOWN, key_code_list);
	} else if (field == "sidle_mode") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(SIDLE_MODE, key_code_list);
	} else if (field == "fast_mode") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(FAST_MODE, key_code_list);
	} else if (field == "go_faster") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(GO_FASTER, key_code_list);
	} else if (field == "go_slower") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(GO_SLOWER, key_code_list);
	} else if (field == "jump") {
		if (!parse_key_code_list((char *)(const char *)value.str(), 
			key_code_list))
			return(false);
		set_key_codes(JUMP, key_code_list);
	}

	// All other fields are errors.

	else
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Statement stepper class.
//------------------------------------------------------------------------------

class statement_stepper : public skStatementStepper
{
private:
	bool handle_pause_or_termination(void);
public:
	bool statementExecuted(const skStackFrame &stack_frame, int statement_type);
	bool compoundStatementExecuted(const skStackFrame &stack_frame);
	bool exceptionEncountered(const skStackFrame *stack_frame, const skException &e);
	void breakpoint(const skStackFrame *stack_frame);
};

// Handle pausing or terminating the script being executed.

bool
statement_stepper::handle_pause_or_termination(void)
{
	// If the script has been running for at least 10 ms, then wait until a
	// RESUME_SCRIPT, TERMINATE_SCRIPT or TERMINATE_SIMKIN command is sent.
	
	int curr_time_ms = get_time_ms();
	if (curr_time_ms >= script_resume_time_ms + 10) {
		command_completed.send_event(false);
		perform_command.wait_for_event();
		if (command_code == TERMINATE_SCRIPT || 
			command_code == TERMINATE_SIMKIN)
			return(false);
		script_resume_time_ms = get_time_ms();
	}

	// If a TERMINATE_SCRIPT or TERMINATE_SIMKIN command was sent, then
	// stop executing this script.

	else if (perform_command.event_sent()) {
		switch (command_code) {
		case TERMINATE_SIMKIN:
			running = false;
		case TERMINATE_SCRIPT:
			return(false);
		}
	}

	// Continue execution of the script.

	return(true);
}

// Single-step a statement.

bool
statement_stepper::statementExecuted(const skStackFrame &stack_frame, int statement_type)
{
	return(handle_pause_or_termination());
}

// Single-step a compound statement

bool
statement_stepper::compoundStatementExecuted(const skStackFrame &stack_frame)
{
	return(handle_pause_or_termination());
}

bool
statement_stepper::exceptionEncountered(const skStackFrame *stack_frame, const skException &e)
{
	debug_message("Simkin exception: %s\n", (const char *)e.toString());
	return false;
}

void
statement_stepper::breakpoint(const skStackFrame *stack_frame)
{
}

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------

// The SimKin statement stepper object.

static statement_stepper *statement_stepper_ptr;

// The pre-defined SimKin objects.

static spot_simkin_object *spot_simkin_object_ptr;
static math_simkin_object *math_simkin_object_ptr;
static sky_simkin_object *sky_simkin_object_ptr;
static ambient_light_simkin_object *ambient_light_simkin_object_ptr;
static ambient_sound_simkin_object *ambient_sound_simkin_object_ptr;
static orb_simkin_object *orb_simkin_object_ptr;
static fog_simkin_object *fog_simkin_object_ptr;
static map_simkin_object *map_simkin_object_ptr;
static player_simkin_object *player_simkin_object_ptr;

//------------------------------------------------------------------------------
// Perform script execution.
//------------------------------------------------------------------------------

static void
perform_script_execution(void)
{
	skMethodDefNode *script_simkin_object_ptr;
	skRValueArray args;
	skRValue return_value;

	try {

		// If the script definition has not yet be parsed, do so now.

		if (simkin_script_def_ptr->script_simkin_object_ptr == NULL)
			simkin_script_def_ptr->script_simkin_object_ptr =
				simkin_interpreter_ptr->parseString("block", 
					(char *)simkin_script_def_ptr->script, *simkin_context_ptr);
		script_simkin_object_ptr = 
			(skMethodDefNode *)simkin_script_def_ptr->script_simkin_object_ptr;

		// Now execute the script, using either the block or the spot object
		// as the target.  In the case of a block being the target, references 
		// to it's SimKin objects are made to ensure they don't get deleted if
		// the block is removed from the map during script execution.  The
		// references will be automatically disposed of after script execution.

		if (simkin_block_ptr != NULL) {
			skRValue block_simkin_value =
				get_block_simkin_object(simkin_block_ptr);
			skRValue vertex_simkin_value =
				get_vertex_simkin_object(simkin_block_ptr);
			simkin_interpreter_ptr->executeParseTree("block", 
				block_simkin_value.obj(), script_simkin_object_ptr, args, 
				return_value, *simkin_context_ptr);
		} else {
			simkin_interpreter_ptr->executeParseTree("spot", 
				spot_simkin_object_ptr, script_simkin_object_ptr, args, 
				return_value, *simkin_context_ptr);
		}
	}
	catch (skRuntimeException e) {
		diagnose("%s", (const char *)e.toString());
	}
	catch (skBoundsException e) {
		diagnose("%s", (const char *)e.toString());
	}
	catch (skParseException e) {
		skCompileErrorList error_list = e.getErrors();
		for (unsigned int index = 0; index < error_list.entries(); index++)
			diagnose("Script error in line %d of %s: %s",
				error_list[index].lineNum(), 
				(const char *)error_list[index].location(),
				(const char *)error_list[index].msg());
	}
}

//------------------------------------------------------------------------------
// Perform method call.
//------------------------------------------------------------------------------

static void
perform_method_call(void)
{
	skRValueArray args;
	skRValue return_value;

	try {
		spot_simkin_object_ptr->method(simkin_method_name, args, return_value, *simkin_context_ptr);
	}
	catch (skRuntimeException e) {
		diagnose("%s", (const char *)e.toString());
	}
	catch (skBoundsException e) {
		diagnose("%s", (const char *)e.toString());
	}
	catch (skParseException e) {
		skCompileErrorList error_list = e.getErrors();
		for (unsigned int index = 0; index < error_list.entries(); index++)
			diagnose("Script error in line %d of %s: %s",
				error_list[index].lineNum(), 
				(const char *)error_list[index].location(),
				(const char *)error_list[index].msg());
	}
}

//------------------------------------------------------------------------------
// Simkin thread.
//------------------------------------------------------------------------------

static void
simkin_thread(void *arg_list)
{
	// Initialise all global variables.

	simkin_interpreter_ptr = NULL;
	spot_simkin_object_ptr = NULL;
	math_simkin_object_ptr = NULL;
	sky_simkin_object_ptr = NULL;
	ambient_light_simkin_object_ptr = NULL;
	ambient_sound_simkin_object_ptr = NULL;
	orb_simkin_object_ptr = NULL;
	fog_simkin_object_ptr = NULL;
	map_simkin_object_ptr = NULL;
	player_simkin_object_ptr = NULL;

	// The rest of the initialisation needs to occur in a try block.

	try {

		// Create and the interpreter.

		if ((simkin_interpreter_ptr = new skInterpreter) == NULL)
			throw false;

		// Create the context.

		if ((simkin_context_ptr = new skExecutableContext(simkin_interpreter_ptr)) == NULL)
			throw false;

		// Create a statement stepper and associate it with the interpreter.

		if ((statement_stepper_ptr = new statement_stepper) == NULL)
			throw false;
		simkin_interpreter_ptr->setStatementStepper(statement_stepper_ptr);

		// Create the pre-defined SimKin objects.

		NEW_SIMKIN_OBJECT(spot_simkin_object_ptr, spot_simkin_object, "spot");
		NEW_SIMKIN_OBJECT(math_simkin_object_ptr, math_simkin_object, "math");
		NEW_SIMKIN_OBJECT(sky_simkin_object_ptr, sky_simkin_object, "sky");
		NEW_SIMKIN_OBJECT(ambient_light_simkin_object_ptr, 
			ambient_light_simkin_object, "ambient_light");
		NEW_SIMKIN_OBJECT(ambient_sound_simkin_object_ptr,
			ambient_sound_simkin_object, "ambient_sound");
		NEW_SIMKIN_OBJECT(orb_simkin_object_ptr, orb_simkin_object, "orb");
		NEW_SIMKIN_OBJECT(fog_simkin_object_ptr, fog_simkin_object, "fog");
		NEW_SIMKIN_OBJECT(map_simkin_object_ptr, map_simkin_object, "map");
		NEW_SIMKIN_OBJECT(player_simkin_object_ptr, player_simkin_object, 
			"player");

		// Signal the player thread that initialisation was successful.

		simkin_thread_initialised.send_event(true);
	}

	// On any caught error, signal the player thread that initialisation was
	// unsuccessful, and terminate the thread.

	catch (...) {
		simkin_thread_initialised.send_event(false);
		return;
	}

	// Loop until a request to terminate the thread has been recieved.

	running = true;
	while (running) {

		// Wait for the next command request.  This ensures that this
		// thread is not using up valuable CPU time while it has nothing
		// to do.

		perform_command.wait_for_event();

		// Perform the command.
			
		switch (command_code) {
		case EXECUTE_SCRIPT:
			script_start_time_ms = get_time_ms();
			script_resume_time_ms = script_start_time_ms;
			perform_script_execution();
			break;
		case CALL_GLOBAL_METHOD:
			script_start_time_ms = get_time_ms();
			script_resume_time_ms = script_start_time_ms;
			perform_method_call();
			break;
		case TERMINATE_SIMKIN:
			running = false;
		}

		// Signal the player thread that the command was completed.

		command_completed.send_event(true);
	}

	// Delete the pre-defined SimKin objects.

	if (spot_simkin_object_ptr != NULL)
		delete spot_simkin_object_ptr;
	if (math_simkin_object_ptr != NULL)
		delete math_simkin_object_ptr;
	if (sky_simkin_object_ptr != NULL)
		delete sky_simkin_object_ptr;
	if (ambient_light_simkin_object_ptr != NULL)
		delete ambient_light_simkin_object_ptr;
	if (ambient_sound_simkin_object_ptr != NULL)
		delete ambient_sound_simkin_object_ptr;
	if (orb_simkin_object_ptr != NULL)
		delete orb_simkin_object_ptr;
	if (fog_simkin_object_ptr != NULL)
		delete fog_simkin_object_ptr;
	if (map_simkin_object_ptr != NULL)
		delete map_simkin_object_ptr;
	if (player_simkin_object_ptr != NULL)
		delete player_simkin_object_ptr;

	// Detach the statement stepper, then delete it.

	simkin_interpreter_ptr->setStatementStepper(NULL);
	delete statement_stepper_ptr;

	// Delete the global context and interpreter.

	delete simkin_context_ptr;
	delete simkin_interpreter_ptr;
}

//------------------------------------------------------------------------------
// Start up SimKin.
//------------------------------------------------------------------------------

bool
start_up_simkin(void)
{
	// Initialise the script definition list.

	script_def_list = NULL;
	curr_script_def_ID = 0;

	// Create the events used to communicate between the player and SimKin
	// threads.

	simkin_thread_initialised.create_event();
	perform_command.create_event();
	command_completed.create_event();
	
	// Start the SimKin thread, and wait for it to send an event indicating
	// whether it initialised or not.

	return((simkin_thread_handle = start_thread(simkin_thread)) > 0 &&
		simkin_thread_initialised.wait_for_event());
}

//------------------------------------------------------------------------------
// Shut down SimKin.
//------------------------------------------------------------------------------

void
shut_down_simkin(void)
{
	script_def *next_script_def_ptr;

	// Send the simkin thread a request to terminate, then wait for it to
	// do so.

	if (simkin_thread_handle >= 0) {
		command_code = TERMINATE_SIMKIN;
		perform_command.send_event(true);
		wait_for_thread_termination(simkin_thread_handle);
	}

	// Destroy the events used to communicate with the simkin thread.

	simkin_thread_initialised.destroy_event();
	perform_command.destroy_event();
	command_completed.destroy_event();

	// Delete the script definition list.

	while (script_def_list != NULL) {
		next_script_def_ptr = script_def_list->next_script_def_ptr;
		if (script_def_list->script_simkin_object_ptr != NULL)
			delete (skMethodDefNode *)script_def_list->script_simkin_object_ptr;
		DEL(script_def_list, script_def);
		script_def_list = next_script_def_ptr;
	}
}

//------------------------------------------------------------------------------
// Load the specified script into the spot SimKin object.
// XXX -- This must not be called if the SimKin thread is performing some task
// that involves the spot SimKin object.
//------------------------------------------------------------------------------

void
set_global_script(const char *script)
{
	string global_script;

	try {
		global_script = "<DEFINE>\n";
		global_script += script;
		global_script += "</DEFINE>";
		spot_simkin_object_ptr->load("spot", skInputString((char *)global_script), *simkin_context_ptr);
	}
	catch (...) {
		diagnose("Unable to load script into spot object");
	}
}

//------------------------------------------------------------------------------
// Create a block SimKin object with or without a script.
//------------------------------------------------------------------------------

void
create_block_simkin_object(block *block_ptr, const char *script)
{
	string block_script;
	skRValue *simkin_value_ptr;

	// If a script has been provided, create a scripted block SimKin object.

	if (script != NULL) {
		scripted_block_simkin_object *block_simkin_object_ptr;

		// Create the scripted block SimKin object, and initialise it.

		if ((block_simkin_object_ptr = new scripted_block_simkin_object)
			== NULL)
			return;
		if ((simkin_value_ptr = new skRValue(block_simkin_object_ptr, true))
			== NULL) {
			delete block_simkin_object_ptr;
			return;
		}

		// Load the script into this object, then store the block SimKin object
		// in the block itself.

		try {
			block_script = "<DEFINE>\n";
			block_script += script;
			block_script += "</DEFINE>";
			block_simkin_object_ptr->load("block", skInputString((char *)block_script), *simkin_context_ptr);
			block_simkin_object_ptr->block_ptr = block_ptr;
			block_ptr->scripted = true;
			block_ptr->block_simkin_object_ptr = simkin_value_ptr;
		}
		catch (...) {
			delete simkin_value_ptr;
			diagnose("Unable to load script into block '%s'",
				block_ptr->block_def_ptr->get_symbol());
		}
	}

	// Otherwise create an unscripted block SimKin object, initialise it, and
	// store it in the block.

	else {
		unscripted_block_simkin_object *block_simkin_object_ptr;

		if ((block_simkin_object_ptr = new unscripted_block_simkin_object) 
			== NULL)
			return;
		if ((simkin_value_ptr = new skRValue(block_simkin_object_ptr, true))
			== NULL) {
			delete block_simkin_object_ptr;
			return;
		}
		block_simkin_object_ptr->block_ptr = block_ptr;
		block_ptr->scripted = false;
		block_ptr->block_simkin_object_ptr = simkin_value_ptr;
	}
}
	
//------------------------------------------------------------------------------
// Destroy a scripted or unscripted block SimKin object.
//------------------------------------------------------------------------------

void
destroy_block_simkin_object(block *block_ptr)
{
	// Set the block pointer in the scripted or unscripted block SimKin object
	// to NULL.  That way, if the object hangs around due to existing references
	// to it, it can produce safe error conditions when the object is accessed.

	skRValue *simkin_value_ptr = (skRValue *)block_ptr->block_simkin_object_ptr;
	if (block_ptr->scripted) {
		scripted_block_simkin_object *block_simkin_object_ptr =
			(scripted_block_simkin_object *)simkin_value_ptr->obj();
		block_simkin_object_ptr->block_ptr = NULL;
	} else {
		unscripted_block_simkin_object *block_simkin_object_ptr =
			(unscripted_block_simkin_object *)simkin_value_ptr->obj();
		block_simkin_object_ptr->block_ptr = NULL;
	}

	// Delete the skRValue object that holds a reference to the block SimKin
	// object.  This may or may not delete the object itself.

	delete simkin_value_ptr;
	block_ptr->block_simkin_object_ptr = NULL;
}

//------------------------------------------------------------------------------
// Create a vertex SimKin object for a block.
//------------------------------------------------------------------------------

void
create_vertex_simkin_object(block *block_ptr)
{
	skRValue *simkin_value_ptr = create_vertex_simkin_object(block_ptr,
		block_ptr->vertices, block_ptr->vertex_list);
	block_ptr->vertex_simkin_object_ptr = simkin_value_ptr;
}

//------------------------------------------------------------------------------
// Destroy a vertex SimKin object on a block.
//------------------------------------------------------------------------------

void
destroy_vertex_simkin_object(block *block_ptr)
{
	destroy_vertex_simkin_object(
		(skRValue *)block_ptr->vertex_simkin_object_ptr);
	block_ptr->vertex_simkin_object_ptr = NULL;
}

//------------------------------------------------------------------------------
// Create a script definition with a unique ID for the given script, and add
// it to the script definition list.
//------------------------------------------------------------------------------

script_def *
create_script_def(const char *script)
{
	script_def *script_def_ptr;

	NEW(script_def_ptr, script_def);
	if (script_def_ptr != NULL) {
		script_def_ptr->ID = curr_script_def_ID++;
		script_def_ptr->script = script;
		script_def_ptr->script_simkin_object_ptr = NULL;
		script_def_ptr->next_script_def_ptr = script_def_list;
		script_def_list = script_def_ptr;
	}
	return(script_def_ptr);
}

//------------------------------------------------------------------------------
// Execute a script directly, using the specified block's SimKin object, or the
// spot SimKin object if the specified block is NULL.  Returns TRUE if the
// script completed within it's allotted time slot, or FALSE if the script has
// been paused awaiting another time slot.
//------------------------------------------------------------------------------

bool
execute_script(block *block_ptr, script_def *script_def_ptr)
{
	command_code = EXECUTE_SCRIPT;
	simkin_script_def_ptr = script_def_ptr;
	simkin_block_ptr = block_ptr;
	perform_command.send_event(true);
	return(!command_completed.wait_for_event());
}

//------------------------------------------------------------------------------
// Call a method on the spot SimKin object.  No error is generated if the method
// does not exist.  Returns TRUE if the method completed within it's allotted
// time slot, or FALSE if the method has been paused awaiting another time slot.
//------------------------------------------------------------------------------

bool
call_global_method(const char *method_name)
{
	command_code = CALL_GLOBAL_METHOD;
	simkin_method_name = method_name;
	perform_command.send_event(true);
	return(!command_completed.wait_for_event());
}

//------------------------------------------------------------------------------
// Resume the SimKin script currently being executed.  Returns TRUE if the 
// script completed within it's allotted time slot, or FALSE if the script
// has been paused again.
//------------------------------------------------------------------------------

bool
resume_script(void)
{
	command_code = RESUME_SCRIPT;
	perform_command.send_event(true);
	return(!command_completed.wait_for_event());
}

//------------------------------------------------------------------------------
// Terminate the SimKin script currently being executed, if any.
//------------------------------------------------------------------------------

void
terminate_script(void)
{
	command_code = TERMINATE_SCRIPT;
	perform_command.send_event(true);
	command_completed.wait_for_event();
}

#endif