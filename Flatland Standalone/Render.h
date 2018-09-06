//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

// Externally visible functions.

void 
init_renderer(void);

void
set_up_renderer(void);

void 
clean_up_renderer(void);

void
translate_vertex(vertex *old_vertex_ptr, vertex *new_vertex_ptr);

void
rotate_vertex(vertex *old_vertex_ptr, vertex *new_vertex_ptr);

void
transform_vertex(vertex *old_vertex_ptr, vertex *new_vertex_ptr);

void 
render_frame(void);

void
render_builder_icons_for_blockset(blockset *blockset_ptr);

void
activate_builder_render_target();

void
deactivate_builder_render_target();