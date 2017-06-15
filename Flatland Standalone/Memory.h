//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#ifdef MEM_TRACE

// Macros for allocating and freeing memory.

#define NEW(ptr, type) \
{ \
	ptr = new type; \
	trace_new(ptr, sizeof(type), #type, __FILE__, __LINE__); \
}

#define NEWARRAY(ptr, type, elements) \
{ \
	ptr = new type[elements]; \
	trace_newarray(ptr, sizeof(type), elements, #type, __FILE__, __LINE__); \
}

#define DEL(ptr, type) \
{ \
	trace_del(ptr, sizeof(type), #type, __FILE__, __LINE__); \
	delete ptr; \
}

#define DELARRAY(ptr, type, elements) \
{ \
	trace_delarray(ptr, 4, sizeof(type), elements, #type, __FILE__, __LINE__); \
	delete []ptr; \
}

#define DELBASEARRAY(ptr, type, elements) \
{ \
	trace_delarray(ptr, 0, sizeof(type), elements, #type, __FILE__, __LINE__); \
	delete []ptr; \
}

#else

#define NEW(ptr, type)							ptr	= new type
#define NEWARRAY(ptr, type, elements)			ptr = new type[elements]
#define DEL(ptr, type)							delete ptr
#define DELARRAY(ptr, type, elements)			delete []ptr
#define DELBASEARRAY(ptr, type, elements)		delete []ptr

#endif

#ifdef MEM_TRACE

// Functions for tracing memory allocations and frees.

void
start_trace();

void
trace_new(void *ptr, int bytes, char *type, char *file_name, int line_no);

void
trace_newarray(void *ptr, int bytes, int elements, char *type, char *file_name,
			   int line_no);

void
trace_del(void *ptr, int bytes, char *type, char *file_name, int line_no);

void
trace_delarray(void *ptr, int offset, int bytes, int elements, char *type, 
			   char *file_name, int line_no);

void
end_trace();

#endif

// Functions for managing free spans.

void
init_free_span_list(void);

void
delete_free_span_list(void);

span *
new_span(void);

span *
dup_span(span *old_span_ptr);

span *
del_span(span *span_ptr);

// Functions for managing screen polygons.

void
init_screen_polygon_list(int set_max_spoints);

void
delete_screen_polygon_list(void);

void
reset_screen_polygon_list(void);

spolygon *
get_next_screen_polygon(void);

// Functions for managing free triggers.

void
init_free_trigger_list(void);

void
delete_free_trigger_list(void);

trigger *
new_trigger(void);

trigger *
dup_trigger(trigger *old_trigger_ptr);

trigger *
del_trigger(trigger *trigger_ptr);
