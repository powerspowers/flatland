//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

bool
start_up_simkin(void);

void
shut_down_simkin(void);

void
set_global_script(const char *script);

void
create_block_simkin_object(block *block_ptr, const char *script);

void
destroy_block_simkin_object(block *block_ptr);

void
create_vertex_simkin_object(block *block_ptr);

void
destroy_vertex_simkin_object(block *block_ptr);

script_def *
create_script_def(const char *script);

bool
execute_script(block *block_ptr, script_def *script_def_ptr);

bool
call_global_method(const char *method_name);

bool
resume_script(void);

void
terminate_script(void);