//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

// Spot parsing functions.

void
add_block_symbols(blockset *blockset_ptr);

blockset *
parse_blockset(char *blockset_URL, bool show_title = true);

void
parse_spot_file();

// Spot saving function.

void
save_spot_file(const char *spot_file_path);

// Generic read string function.

bool
read_string(FILE *fp, char *buffer, int max_buffer_length);

// Configuration file functions.

void
load_config_file(void);

void
save_config_file(void);

// Blockset cache file functions.
 
void
delete_cached_blockset_list(void);

cached_blockset *
new_cached_blockset(const char *path, const char *href, int size, time_t updated);

bool
load_cached_blockset_list(void);

void
save_cached_blockset_list(void);

void
create_cached_blockset_list(void);

cached_blockset *
find_cached_blockset(const char *href);

bool
delete_cached_blockset(const char *href);

bool
check_for_blockset_update(const char *version_file_URL, const char *blockset_name,
						  unsigned int blockset_version);

// Rover version file parsing function.

bool
parse_rover_version_file(unsigned int &version_number, string &message);
