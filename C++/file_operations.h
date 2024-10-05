// file_operations.h
#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <stdio.h>
#include <stdbool.h>
#include "cJSON.h"

// Function prototypes
char *read_file(FILE *file);
char *remove_sufix(const char *s, char *sufix);
int should_replace(cJSON *original, cJSON *update);
void apply_patch_array(cJSON *original_array, cJSON *patch_array);
void apply_patch(cJSON *original, cJSON *patch);
FILE *create_empty_file_copy(char *file_sufix, const char *original_file_filepath, char *file_copy_filepath_placeholder);
bool update_file_content(char *file_sufix, const char *original_file_filepath, const char *updated_content);
bool update_text_file(const char *original_file_filepath, const char *updated_content);
char *update_json(FILE *original_file, const char *updated_content);
bool update_json_file(FILE *file_to_update, const char *updated_content, const char *original_file_filepath);

#endif // FILE_OPERATIONS_H

