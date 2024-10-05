#include "file_operations.h"
#include <string.h>
#include <stdlib.h>




// Leer contenido de archivo a string
char *read_file(FILE *file) {
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(length + 1);

    if(content ==NULL){
        perror("\nError: no se pudo leer el contendion");
        return NULL;
    }
    int bytes_read=fread(content, 1, length, file);

    if (bytes_read != length) {
        perror("\nError: no se pudo copiar el contenido correctamente");
        return NULL;
    }

    content[length] = '\0';  // Asegurar que el string tenga caracter de terminacion

    return content;
}

//necesario para poder nombrar la copia de archivo (aplica solo para el contexto de la logica usada en este codigo)
char * remove_sufix(const char * s, char * sufix) {
    char *before= strdup(s);
    printf("\ncopy is: %s", before);
    if (before == NULL) {
        perror("\nError: no se pudo alocar memoria para copiar el string original");
        free(before);
        return NULL;
    }
    char * sufix_pointer = strstr(before, sufix);
    if (sufix_pointer) {
        *sufix_pointer = '\0';
        printf("\nbefore es: %s", before);
        return before;
    }
    perror("\nbefore es nulo");
    return NULL;
     
}

// Funcion para decidir si reemplazar elemento en json
int should_replace(cJSON *original, cJSON *update) {
    if (original == NULL || update == NULL) {
        return 1;
    }
    if (original->type != update->type) {
        return 1;
    }
    return 0; 
}


void apply_patch_array(cJSON *original_array, cJSON *patch_array) {
    int original_size = cJSON_GetArraySize(original_array);
    int patch_size = cJSON_GetArraySize(patch_array);

    for (int i = 0; i < patch_size; i++) {
        cJSON *patch_item = cJSON_GetArrayItem(patch_array, i);
        
        if (i < original_size) {
            cJSON *original_item = cJSON_GetArrayItem(original_array, i);

            if (should_replace(original_item, patch_item)) {
                //reemplazar item de array
                cJSON_ReplaceItemInArray(original_array, i, cJSON_Duplicate(patch_item, 1));
            } else if (cJSON_IsObject(original_item) && cJSON_IsObject(patch_item)) {
                // reemplazar recursivamente 
                apply_patch(original_item, patch_item);
            } else if (cJSON_IsArray(original_item) && cJSON_IsArray(patch_item)) {
                // reemplazar recursivamente
                apply_patch_array(original_item, patch_item);
            }
        } else {
            //si se sobrepasa el indice, significa que no existe en el original, se anade
            cJSON_AddItemToArray(original_array, cJSON_Duplicate(patch_item, 1));
        }
    }
}

void apply_patch(cJSON *original, cJSON *patch) {

    if (cJSON_IsArray(original) && cJSON_IsArray(patch)) {
        apply_patch_array(original, patch);
        return;
    }

    cJSON *key = NULL;
    cJSON_ArrayForEach(key, patch) {
        cJSON *patch_value = cJSON_GetObjectItem(patch, key->string);
        cJSON *original_value = cJSON_GetObjectItem(original, key->string);

        if (original_value == NULL) {
            cJSON_AddItemToObject(original, key->string, cJSON_Duplicate(patch_value, 1));
        } else if (cJSON_IsObject(patch_value) && cJSON_IsObject(original_value)) {
            apply_patch(original_value, patch_value);
        } else if (cJSON_IsArray(patch_value) && cJSON_IsArray(original_value)) {
            //cambio en array requiere su propia funcion porque se tratan como tipos separados y su funcioniento difiere
            apply_patch_array(original_value, patch_value);
        } else {
            //maneja nuevos valores de llaves y el caso donde ambos items tienen tipo distinto
            cJSON_ReplaceItemInObject(original, key->string, cJSON_Duplicate(patch_value, 1));
        }
    }
}

FILE * create_empty_file_copy(char * file_sufix, const char *original_file_filepath, char *file_copy_filepath_placeholder){
    const char * copy = "_copy";
    char* filepath_sufix= (char*)malloc(strlen(copy) + strlen(file_sufix) +1);  
   
    if (filepath_sufix == NULL) {
        return NULL;
    }
    strcpy(filepath_sufix, copy);
    strcat(filepath_sufix, file_sufix);
    printf("\nFilepathx sufix es igual a : %s", filepath_sufix);

    if (!strstr(original_file_filepath, file_sufix)){
        perror("\n Error: no se logro actualizar el archivo porque no cumple el formato especificado");
        return NULL;
    }
    char * before = remove_sufix(original_file_filepath, file_sufix);
    
    if (before==NULL){
        return NULL;
    }

    char * file_copy_filepath = (char*)malloc(strlen(before) + strlen(filepath_sufix) +1);

    if(file_copy_filepath==NULL){
        perror("\nError: no se pudo alocar memoria para crear el filepath de la copia");
        return false;
    } 
    strcpy(file_copy_filepath, before);
    strcat(file_copy_filepath, filepath_sufix);
    strcpy(file_copy_filepath_placeholder, file_copy_filepath);

    FILE *file_copy = fopen(file_copy_filepath, "w");
    return file_copy;
}


bool update_file_content(char * file_sufix, const char * original_file_filepath, const char * updated_content){

    char file_copy_filepath[1024];  
    if (file_sufix == NULL) {
        perror("\nError:no se aporto el tipo de archivo a actualizar ");
        return false;
    }
    FILE *file_to_update = create_empty_file_copy(file_sufix, original_file_filepath,file_copy_filepath);


    if (file_to_update == NULL ) {
        perror("\nError: no se pudo abrir el archivo de copia para actualiar el archivo original");
        return false;
    }
    int bytes_written = fwrite(updated_content, sizeof(char), strlen(updated_content), file_to_update);

    if (bytes_written != strlen(updated_content)){
        perror("\nError: no se pudo escribir completamente los bytes necesarios");
        fclose(file_to_update);
        return false;
    }

    fclose(file_to_update);
    remove(original_file_filepath);

    printf("\nBoth files are %s y %s", file_copy_filepath, original_file_filepath);

    if (rename(file_copy_filepath, original_file_filepath)==-1) {
        perror("\nError: no se pudo renombrar el archivo");
        return false;
    }
    return true;
}

bool update_text_file(const char * original_file_filepath, const char * updated_content){

    if (original_file_filepath == NULL || updated_content == NULL || strcmp(original_file_filepath,"")==0) {
        perror("\nError: argumento nulo aportado en funcion updated_text_file");
        return false;
    }
    
    char * file_sufix = ".txt";
    bool resolved = update_file_content(file_sufix, original_file_filepath, updated_content); 
    return resolved;

}


char *update_json(FILE *original_file, const char *updated_content) {
    char *original_content = read_file(original_file);
    printf("\nOriginal content: %s", original_content);
    cJSON *original_json = NULL;

    // Handle empty original content
    if (strcmp(original_content, "") == 0) {
        printf("\nOriginal content is empty, creating an empty JSON object.\n");
        // Create an empty JSON object
        char *empty_json_string = "{}";
        original_json = cJSON_Parse(empty_json_string);
        if (original_json == NULL) {
            printf("\nError: Failed to create an empty JSON object.\n");
            free(original_content);
            return NULL;
        }
    } else {
        // Parse the original content
        original_json = cJSON_Parse(original_content);
        if (original_json == NULL) {
            printf("\nError: Failed to parse the original JSON.\n");
            free(original_content);
            return NULL;
        }
    }

    // Parse the update content (patch)
    cJSON *update_json = cJSON_Parse(updated_content);
    if (update_json == NULL) {
        printf("\nError: Failed to parse the JSON patch.\n");
        cJSON_Delete(original_json);
        free(original_content);
        return NULL;
    }

    // Apply the patch to the original JSON (or empty JSON if it was empty)
    apply_patch(original_json, update_json);

    // Serialize the updated JSON object to a string
    char *updated_json_string = cJSON_Print(original_json);
    printf("Updated JSON:\n%s\n", updated_json_string);

    // Clean up
    free(original_content);
    cJSON_Delete(original_json);
    cJSON_Delete(update_json);

    return updated_json_string;  // Return the updated JSON string
}


bool update_json_file(FILE *file_to_update, const char * updated_content, const char * original_file_filepath){
    if (original_file_filepath == NULL || updated_content == NULL || file_to_update==NULL || strcmp(original_file_filepath,"")==0)  {
        perror("\nError: argumento nulo aportado en funcion updated_text_file");
        return false;
    }
    const char *updated_json_string = update_json(file_to_update, updated_content);
    if (updated_json_string != NULL) {
        char * file_sufix = ".json";
        bool resolve = update_file_content(file_sufix, original_file_filepath, updated_json_string);
        return resolve;
    } 
    return false;
}
