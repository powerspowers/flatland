//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Classes.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"

#ifdef MEM_TRACE

// The following trace levels are defined:
// 0 = Display total peak bytes allocated only.
// 1 = Display peak bytes allocated to each data type.
// 2 = Detect multiple frees (causes deliberate memory leaks).

// Flag indicating whether memory tracing is active.

static bool trace_on = false;

// The current bytes allocated, and the peak bytes allocated.

static int curr_bytes_allocated;
static int peak_bytes_allocated;

// The current number of allocations, and the peak number of allocations.

static int curr_allocations;
static int peak_allocations;

#if TRACE_LEVEL >= 1

// List of object types that are being traced.

struct object {
	char type[32];
	int curr_bytes_allocated;
	int peak_bytes_allocated;
	int curr_allocations;
	int peak_allocations;
};

#define MAX_OBJECTS	128
static int objects;
static object object_list[MAX_OBJECTS];

#endif

#endif // MEM_TRACE

// Linked list of free spans.

static span *free_span_list;

// Screen polygon list, the last screen polygon in the list, and the
// current screen polygon.

static int max_spoints;
static spolygon *spolygon_list;
static spolygon *last_spolygon_ptr;
static spolygon *curr_spolygon_ptr;

// Linked list of free triggers.

static trigger *free_trigger_list;

#ifdef MEM_TRACE

//==============================================================================
// Memory tracing functions.
//==============================================================================

static int curr_line_no;
static const char *curr_file_name;

#if TRACE_LEVEL >= 2

void *
operator new(size_t bytes)
{
	byte *ptr = (byte *)malloc(bytes + sizeof(size_t));
	*(size_t *)ptr = bytes;
	return(ptr + sizeof(size_t));
}

void
operator delete(void *ptr)
{
	byte *data_ptr = (byte *)ptr;
	byte *mem_ptr = data_ptr - sizeof(size_t);
	size_t bytes = *(size_t *)mem_ptr;
	bool freed = true;
	for (size_t index = 0; index < bytes; index++) {
		if (*data_ptr != 0xAA) {
			freed = false;
			break;
		}
		data_ptr++;
	}
	if (freed)
		diagnose("Memory was freed twice at line %d of file %s!", curr_line_no,
			curr_file_name);
	else {
		data_ptr = (byte *)ptr;
		for (size_t index = 0; index < bytes; index++)
			*data_ptr++ = 0xAA;
	}
}

#endif

//------------------------------------------------------------------------------
// Start tracing memory.
//------------------------------------------------------------------------------

void
start_trace(void)
{
	// Clear the memory statistics.

	curr_bytes_allocated = 0;
	peak_bytes_allocated = 0;
	curr_allocations = 0;
	peak_allocations = 0;
#if TRACE_LEVEL >= 1
	objects = 0;
#endif

	// Enable memory tracing.

	trace_on = true;
}

//------------------------------------------------------------------------------
// Find an object of a given type, or create a new object of that type.
//------------------------------------------------------------------------------

#if TRACE_LEVEL >= 1

static object *
find_object(char *type)
{
	object *object_ptr;

	for (int index = 0; index < objects; index++) {
		object_ptr = &object_list[index];
		if (!stricmp(object_ptr->type, type))
			//			break;
			return(object_ptr);
	}
//	if (index == objects) {   //POWERS
		if (objects == MAX_OBJECTS)
			return(NULL);
		object_ptr = &object_list[objects++];
		strcpy(object_ptr->type, type);
		object_ptr->curr_bytes_allocated = 0;
		object_ptr->peak_bytes_allocated = 0;
		object_ptr->curr_allocations = 0;
		object_ptr->peak_allocations = 0;
//	}
//	return(object_ptr);
}

#endif

//------------------------------------------------------------------------------
// Trace the allocation of a new object.
//------------------------------------------------------------------------------

void
trace_new(void *ptr, int bytes, char *type, char *file_name, int line_no)
{
	if (trace_on && ptr != NULL) {
		curr_bytes_allocated += bytes;
		if (curr_bytes_allocated > peak_bytes_allocated)
			peak_bytes_allocated = curr_bytes_allocated;
		curr_allocations++;
		if (curr_allocations > peak_allocations)
			peak_allocations = curr_allocations;
#if TRACE_LEVEL >= 1
		object *object_ptr;
		if ((object_ptr = find_object(type)) != NULL) {
			object_ptr->curr_bytes_allocated += bytes;
			if (object_ptr->curr_bytes_allocated > 
				object_ptr->peak_bytes_allocated)
				object_ptr->peak_bytes_allocated = 
					object_ptr->curr_bytes_allocated;
			object_ptr->curr_allocations++;
			if (object_ptr->curr_allocations > object_ptr->peak_allocations)
				object_ptr->peak_allocations = object_ptr->curr_allocations;
		}
#endif
	}
}

//------------------------------------------------------------------------------
// Trace the allocation of a new array of objects.
//------------------------------------------------------------------------------

void
trace_newarray(void *ptr, int bytes, int elements, char *type, char *file_name,
			   int line_no)
{
	if (trace_on && ptr != NULL) {
		curr_bytes_allocated += bytes * elements;
		if (curr_bytes_allocated > peak_bytes_allocated)
			peak_bytes_allocated = curr_bytes_allocated;
		curr_allocations++;
		if (curr_allocations > peak_allocations)
			peak_allocations = curr_allocations;
#if TRACE_LEVEL >= 1
		object *object_ptr;
		if ((object_ptr = find_object(type)) != NULL) {
			object_ptr->curr_bytes_allocated += bytes * elements;
			if (object_ptr->curr_bytes_allocated > 
				object_ptr->peak_bytes_allocated)
				object_ptr->peak_bytes_allocated = 
					object_ptr->curr_bytes_allocated;
			object_ptr->curr_allocations++;
			if (object_ptr->curr_allocations > object_ptr->peak_allocations)
				object_ptr->peak_allocations = object_ptr->curr_allocations;
		}
#endif
	}
}

//------------------------------------------------------------------------------
// Trace the deletion of an allocated object.
//------------------------------------------------------------------------------

void
trace_del(void *ptr, int bytes, char *type, char *file_name, int line_no)
{
	if (trace_on) {
		curr_file_name = file_name;
		curr_line_no = line_no;
		curr_bytes_allocated -= bytes;
		curr_allocations--;
#if TRACE_LEVEL >= 1
		object *object_ptr;
		if ((object_ptr = find_object(type)) != NULL) {
			object_ptr->curr_bytes_allocated -= bytes;
			object_ptr->curr_allocations--;
		}
#endif
	}
}

//------------------------------------------------------------------------------
// Trace the deletion of an allocated array of blocks.
//------------------------------------------------------------------------------

void
trace_delarray(void *ptr, int offset, int bytes, int elements, char *type, 
			   char *file_name, int line_no)
{
	if (trace_on) {
		curr_file_name = file_name;
		curr_line_no = line_no;
		curr_bytes_allocated -= bytes * elements;
		curr_allocations--;
#if TRACE_LEVEL >= 1
		object *object_ptr;
		if ((object_ptr = find_object(type)) != NULL) {
			object_ptr->curr_bytes_allocated -= bytes * elements;
			object_ptr->curr_allocations--;
		}
#endif
	}
}

//------------------------------------------------------------------------------
// End tracing memory.
//------------------------------------------------------------------------------

void
end_trace(void)
{
	// Now report some additional statistics.

	diagnose("Peak allocation of %d bytes in %d allocations", 
		peak_bytes_allocated, peak_allocations);
	if (curr_bytes_allocated > 0)
		diagnose("*** Memory leak: %d bytes are still allocated", 
			curr_bytes_allocated);
	else if (curr_bytes_allocated < 0)
		diagnose("*** Reverse memory leak: %d bytes freed were not allocated",
			-curr_bytes_allocated);
#if TRACE_LEVEL >= 1
	for (int index = 0; index < objects; index++) {
		object *object_ptr = &object_list[index];
		diagnose("Peak allocation of %d bytes in %d allocations to objects of "
			"type %s", object_ptr->peak_bytes_allocated, 
			object_ptr->peak_allocations, object_ptr->type);
		if (object_ptr->curr_bytes_allocated > 0)
			diagnose("*** Memory leak for objects of type %s: "
			"%d bytes are still allocated", object_ptr->type, 
			object_ptr->curr_bytes_allocated);
		else if (object_ptr->curr_bytes_allocated < 0)
			diagnose("*** Reverse memory leak for objects of type %s: "
				"%d bytes freed were not allocated", object_ptr->type, 
				-object_ptr->curr_bytes_allocated);
	}
#endif
	trace_on = false;
}

#endif // MEM_TRACE

//==============================================================================
// Memory management functions.
//==============================================================================

//------------------------------------------------------------------------------
// Free span list management.
//------------------------------------------------------------------------------

// Initialise the free span list.

void
init_free_span_list(void)
{
	free_span_list = NULL;
}

// Delete the free span list.

void
delete_free_span_list(void)
{
	span *next_span_ptr;

	while (free_span_list != NULL) {
		next_span_ptr = free_span_list->next_span_ptr;
		DEL(free_span_list, span);
		free_span_list = next_span_ptr;
	}
}

// Return a pointer the next free span, or NULL if we are out of memory.

span *
new_span(void)
{
	span *span_ptr;

	span_ptr = free_span_list;
	if (span_ptr != NULL)
		free_span_list = span_ptr->next_span_ptr;
	else
		NEW(span_ptr, span);
	return(span_ptr);
}

// Return a pointer the next free span, after initialising it with the old
// span data.

span *
dup_span(span *old_span_ptr)
{
	span *span_ptr;

	span_ptr = new_span();
	if (span_ptr != NULL)
		*span_ptr = *old_span_ptr;
	return(span_ptr);
}

// Add the span to the head of the free span list, and return a pointer to the
// next span.

span *
del_span(span *span_ptr)
{
	span *next_span_ptr = span_ptr->next_span_ptr;
	span_ptr->next_span_ptr = free_span_list;
	free_span_list = span_ptr;
	return(next_span_ptr);
}

//------------------------------------------------------------------------------
// Screen polygon list management.
//------------------------------------------------------------------------------

// Initialise the screen polygon list.

void
init_screen_polygon_list(int set_max_spoints)
{
	spolygon_list = NULL;
	last_spolygon_ptr = NULL;
	max_spoints = set_max_spoints;
}

// Delete the screen polygon list.

void
delete_screen_polygon_list(void)
{
	spolygon *next_spolygon_ptr;

	while (spolygon_list != NULL) {
		next_spolygon_ptr = spolygon_list->next_spolygon_ptr;
		DEL(spolygon_list, spolygon);
		spolygon_list = next_spolygon_ptr;
	}
}

void
reset_screen_polygon_list(void)
{
	curr_spolygon_ptr = spolygon_list;
}

// Return a pointer to the next screen polygon, or create a new one.

spolygon *
get_next_screen_polygon(void)
{
	// If hardware acceleration is enabled...

	if (hardware_acceleration) {
		spolygon *spolygon_ptr;

		// If we have not come to the end of the screen polygon list, get the
		// pointer to the current screen polygon.

		if (curr_spolygon_ptr != NULL) {
			spolygon_ptr = curr_spolygon_ptr;
			curr_spolygon_ptr = curr_spolygon_ptr->next_spolygon_ptr;
		} else {

			// Otherwise create a new screen polygon.

			NEW(spolygon_ptr, spolygon);
			if (spolygon_ptr != NULL &&
				!spolygon_ptr->create_spoint_list(max_spoints)) {
				DEL(spolygon_ptr, spolygon);
				spolygon_ptr = NULL;
			}

			// If the screen polygon was created, add it to the end of the
			// screen polygon list.

			if (spolygon_ptr != NULL) {
				if (last_spolygon_ptr != NULL)
					last_spolygon_ptr->next_spolygon_ptr = spolygon_ptr;
				else
					spolygon_list = spolygon_ptr;
				last_spolygon_ptr = spolygon_ptr;
			}
		}

		// Return the pointer to the screen polygon.

		return(spolygon_ptr);
	}

	// If hardware acceleration is not enabled, simply return the same single
	// screen polygon, creating it first if it doesn't yet exist.

	else {
		if (spolygon_list == NULL) {
			NEW(spolygon_list, spolygon);
			if (spolygon_list != NULL &&
				!spolygon_list->create_spoint_list(max_spoints)) {
				DEL(spolygon_list, spolygon);
				spolygon_list = NULL;
			}
		}
		return(spolygon_list);
	}
}

//------------------------------------------------------------------------------
// Free trigger list management.
//------------------------------------------------------------------------------

// Initialise the free trigger list.

void
init_free_trigger_list(void)
{
	free_trigger_list = NULL;
}

// Delete the free trigger list.

void
delete_free_trigger_list(void)
{
	trigger *next_trigger_ptr;

	while (free_trigger_list != NULL) {
		next_trigger_ptr = free_trigger_list->next_trigger_ptr;
		free_trigger_list->action_list = NULL;
		DEL(free_trigger_list, trigger);
		free_trigger_list = next_trigger_ptr;
	}
}

// Return a pointer to the next free trigger, or NULL if we are out of memory.

trigger *
new_trigger(void)
{
	trigger *trigger_ptr;

	trigger_ptr = free_trigger_list;
	if (trigger_ptr != NULL)
		free_trigger_list = trigger_ptr->next_trigger_ptr;
	else
		NEW(trigger_ptr, trigger);
	
	// set the id of this trigger and increase the counter
	trigger_ptr->objectid = curr_triggerid;
	trigger_ptr->playerid = curr_playerid;
	curr_triggerid++;
	trigger_hash.add((hash *) trigger_ptr);  // add this trigger to the hash table

	return(trigger_ptr);
}

// Return a pointer to the next free trigger, after initialising it with the
// old trigger data.

trigger *
dup_trigger(trigger *old_trigger_ptr)
{
	trigger *trigger_ptr;
	action* old_action_ptr,*last_action_ptr,*action_ptr;
	int objectid, playerid;

	trigger_ptr = new_trigger();

	//	if (old_trigger_ptr->delay_ms > 20000) set_title("delay %d", old_trigger_ptr->delay_ms);

	if (trigger_ptr != NULL) {
		objectid = trigger_ptr->objectid;
		playerid = trigger_ptr->playerid;
		*trigger_ptr = *old_trigger_ptr;
		trigger_ptr->objectid = objectid;
		trigger_ptr->playerid = playerid;
	} else
		return(NULL);

	trigger_ptr->start_time_ms = curr_time_ms;

	// duplicate the action list if there is one
	old_action_ptr = old_trigger_ptr->action_list;
	last_action_ptr = NULL;
	trigger_ptr->action_list = NULL;


	while (old_action_ptr != NULL) {
		NEW(action_ptr, action);
		if (action_ptr == NULL) {
			memory_warning("action");
			return(NULL);
		}
		*action_ptr = *old_action_ptr;
		action_ptr->trigger_ptr = trigger_ptr;
		action_ptr->next_action_ptr = NULL;
		if (last_action_ptr == NULL) {
			last_action_ptr = action_ptr;
			trigger_ptr->action_list = action_ptr;
		}
		else {
			last_action_ptr->next_action_ptr = action_ptr;
			last_action_ptr = action_ptr;
		}
		old_action_ptr = old_action_ptr->next_action_ptr;
	}

	return(trigger_ptr);
}

// Add the trigger to the head of the free trigger list, and return a pointer
// to the next trigger.

trigger *
del_trigger(trigger *trigger_ptr)
{
	trigger *next_trigger_ptr = trigger_ptr->next_trigger_ptr;
	trigger_ptr->next_trigger_ptr = free_trigger_list;
	free_trigger_list = trigger_ptr;

	trigger_hash.remove((hash *)trigger_ptr);
	return(next_trigger_ptr);
}