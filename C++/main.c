    /*
        http://localhost:8081/

        Ejemplo de GET:
        http://localhost:8081/../files/test_file.json

        gcc main.c file_operations.c -o server -I/usr/local/include/cjson -L/usr/local/lib -lcjson
        ./server

        Archivo principal del servidor
        Integrantes:
            - Daniel Sequeira Retana
            - Josi Marín
            - Emanuel R
    */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include "file_operations.h"

#define PORT 8081
#define THREAD_POOL_SIZE 8

#define NUM_FILE_MUTEXES 256 

#define HTTP_OK "200 OK"
#define HTTP_CREATED "201 Created"
#define HTTP_NO_CONTENT "204 No Content"
#define HTTP_BAD_REQUEST "400 Bad Request"
#define HTTP_NOT_FOUND "404 Not Found"
#define HTTP_FORBIDDEN "403 Forbidden"
#define HTTP_INTERNAL_SERVER_ERROR "500 Internal Server Error"

#define CONTENT_TYPE_JSON "application/json"
#define CONTENT_TYPE_TEXT "text/plain"

#define RESPONSE_NOT_FOUND "Not Found"
#define RESPONSE_ERROR "Error"
#define RESPONSE_ACCESS_DENIED "Access Dennied"
#define RESPONSE_BUFFER_SIZE 2048

    int  open_server(int * server_fd);
    void accept_connections(int server_fd); 
    void *handle_client_request(int new_socket);

    //funciones de ayuda para los request 
    char *get_file_path(char *pRequesr);
    bool validate_file_existence(int new_socket, const char *file_path);
    bool validate_request(int new_socket, const char *content_type, const char *file_path, const char *content);
    bool validate_file_path(int new_socket, const char *file_path);
    char *extract_request_body(char *pRequest);


    // funciones para el manejo de request
    void handle_get_request(int new_socket, char request[]);
    void handle_post_request(int new_socket,char pRequest[]);
    void handle_put_request(int new_socket,char pRequest[]);
    void handle_update_request(int new_socket, char pRequest[]);
    void handle_delete_request(int new_socket, char pRequest[]);
    
    //funciones auxiliares para el get con queries
    bool query_exists(char request[]);
    char *get_query(char request[]);
    char *trim_query(char *file_response, char *query);
    char *find_key_in_text_file(char *file_response, char *query_key);
    char *find_key_in_json(char *file_response, char *query_key, char *query_value);
    char *extract_json_block(char *response);
    char *create_copy_of_block(char *response, char *close);
    bool is_key_in_block(char *block, char *query_key, char *query_value);


    // Lista de rutas de directorios válidos
    const char *server_end_points[] = {
        "../files/",
        "../public/"
        // Añadir más directorios válidos según sea necesario
    };
    const int num_end_points = sizeof(server_end_points) / sizeof(server_end_points[0]);

    //manejo de mutex
    pthread_mutex_t file_mutexes[NUM_FILE_MUTEXES]; 
    // Manejo de hilos
    pthread_t thread_pool[THREAD_POOL_SIZE];  // Pool de hilos
    
    int request_count = 0;
    pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

    //funciones para el manejo de hilos
    void *handle_client_request_threads(void *arg);
    void *thread_function(void *arg);
    void enqueue_request(int new_socket);
    int dequeue_request();
    
    // struct para las solicitudes 
    typedef struct 
    {
        int socket_fd;
    }client_request_t;

    // Cola de solicitudes pendientes
    client_request_t request_queue[THREAD_POOL_SIZE]; 

    int main() 
    {
        
        int server_fd, new_socket;

        if (open_server(&server_fd) < 0) {
            fprintf(stderr, "Error al abrir el servidor");
            exit(EXIT_FAILURE);
        }

        //hashtable de mutex para archivos
        for (int i = 0; i < NUM_FILE_MUTEXES; ++i) {
            pthread_mutex_init(&file_mutexes[i], NULL);
        }

        // Pool de hilos
        for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
            pthread_create(&thread_pool[i], NULL, thread_function, NULL);
        }
        printf("\n╔═════════════════════════════════════════╗");
        printf("\n║ Servidor escuchando en el puerto %d...║", PORT);
        printf("\n╚═════════════════════════════════════════╝");

        accept_connections(server_fd);

        // Limpieza
        for (int i = 0; i < NUM_FILE_MUTEXES; ++i) {
            pthread_mutex_destroy(&file_mutexes[i]);
            printf("\nMutex destruido");
        }
    }

    unsigned int hash_function(const char *file_path) {
        unsigned int hash = 0;
        while (*file_path) {
            hash = (hash * 31) + *file_path++;  // Use a simple hashing algorithm
        }
        return hash % NUM_FILE_MUTEXES;  // Map the hash to one of the mutexes in the array
    }

    pthread_mutex_t* get_file_mutex(const char *file_path) {
        unsigned int index = hash_function(file_path);
        return &file_mutexes[index];
    }


    // Función para procesar los hilos del pool
    void *thread_function(void *arg) {
        while (true) {
            int client_socket = dequeue_request();  // Obtener una solicitud de la cola
            if (client_socket != -1) {
                handle_client_request(client_socket);
            }
        }
        return NULL;
    }

    /**
     * Método que encola las solicitudes de los clientes
     */
    void enqueue_request(int new_socket) {
        
        printf("\n\nAñadiendo a la cola del pool de hilos el socket %d...",new_socket);
        pthread_mutex_lock(&queue_mutex);

        printf("\nSocket %d añadido a la cola en la posición %d",new_socket,request_count);
        request_queue[request_count].socket_fd = new_socket;
        request_count++;

        pthread_cond_signal(&queue_cond);
        pthread_mutex_unlock(&queue_mutex);
    }

    // Obtener una solicitud de la cola
    int dequeue_request() {
        pthread_mutex_lock(&queue_mutex);

        while (request_count == 0) {
            pthread_cond_wait(&queue_cond, &queue_mutex);  // Esperar hasta que haya una solicitud
        }

        int client_socket = request_queue[0].socket_fd;
        printf("\nAtendiendo la solicitud del socket: %d",client_socket);

        // Desplazar la cola hacia adelante
        for (int i = 0; i < request_count - 1; ++i) {
            request_queue[i] = request_queue[i + 1];
        }
        request_count--;

        pthread_mutex_unlock(&queue_mutex);
        return client_socket;
    }

    /**
     * Función para intentar abrir el servidor
     * server_fd:  puntero que almancena la dirección del servidor
     */
    int open_server(int * server_fd) {
        struct sockaddr_in server_address; 
        
        // Crear el socket
        *server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (*server_fd == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }
        
        int reuse = 1;
        if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            printf("SO_REUSEADDR failed: %s \n", strerror(errno));
            return 1;
        }

        // Configurar la dirección y puerto
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(PORT);

        // Vincular el socket
        if (bind(*server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        // Escuchar conexiones
        if (listen(*server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        return 0;
    }

    // Nueva función que maneja la aceptación de conexiones
    void accept_connections(int server_fd) {
        int new_socket;
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);

        while (true) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (new_socket < 0) {
                perror("Error al abrir el socket");
                exit(EXIT_FAILURE);
            }
            enqueue_request(new_socket);  // se pone la solicitud en la cola de hilos
        }
    }

    /**
     * Método principal para manejar las solicitudes del cliente
     * new_socket: socket donde se alojará la solicitud 
     */
    void * handle_client_request(int new_socket) {
        char request[4000] = {0}; //Buffer del cliente, contiene la solicitud completa del cliente
        read(new_socket, request, sizeof(request)); // Leer la solicitud del cliente
        printf("\n\n╔═════════════════════════════════════════════════╗");
        printf("\n    Datos del Request recibidos del socket %d: \n\n%s",new_socket, request);
        printf("\n╚══════════════════════════════════════════════════╝");

        // Determinar si es una solicitud GET o POST u otro
        if (strncmp(request, "GET", 3) == 0) 
        {
            handle_get_request(new_socket, request);
            printf("\nGET request completado del socket: %d\n",new_socket);
        } 
        else 
            if (strncmp(request, "POST", 4) == 0) {
                handle_post_request(new_socket, request);
                printf("\nPOST request completado del socket: %d\n",new_socket);
            }
            else 
                if (strncmp(request, "PUT", 3) == 0) {
                    handle_put_request(new_socket, request);
                    printf("\nPUT request completado del socket: %d\n",new_socket);;
                }
                else 
                    if (strncmp(request, "PATCH", 5) == 0) {
                        handle_update_request(new_socket, request);
                        printf("\nUPDATE request completado del socket: %d\n",new_socket);
                    }
                    else 
                        if (strncmp(request, "DELETE", 6) == 0) {
                            handle_delete_request(new_socket, request);
                            printf("\nDELETE request completado del socket: %d\n",new_socket);
                        }
        printf("\nSolicitud atendida, cerrando socket %d...\n",new_socket);
        close(new_socket);
    }

    void send_response(int socket, const char* status_code, const char* content_type, const char* content) {
        char response_buffer[RESPONSE_BUFFER_SIZE];

        // Validar si el status_code es uno de los definidos
        if (strcmp(status_code, HTTP_OK) != 0 &&
            strcmp(status_code, HTTP_CREATED) != 0 &&
            strcmp(status_code, HTTP_NO_CONTENT) != 0 &&
            strcmp(status_code, HTTP_NOT_FOUND) != 0 &&
            strcmp(status_code, HTTP_BAD_REQUEST) != 0 &&
            strcmp(status_code, HTTP_INTERNAL_SERVER_ERROR) != 0 &&
            strcmp(status_code, HTTP_FORBIDDEN) != 0) 
        {
            // Si no es uno de los códigos definidos, establece el código por defecto
            status_code = HTTP_INTERNAL_SERVER_ERROR;
        }
    

        if (content == NULL || strlen(content) == 0) {
            snprintf(response_buffer, sizeof(response_buffer),
                     "HTTP/1.1 %s \r\n"
                     "Content-Length: 0\r\n"
                    "\r\n",
                    status_code);
        } else {
            snprintf(response_buffer, sizeof(response_buffer),
                     "HTTP/1.1 %s \r\n"
                     "Content-Type: %s\r\n"
                    "Content-Length: %zu\r\n\r\n"
                    "%s",
                    status_code, content_type, strlen(content), content);
        }
        // Enviar respuesta al cliente
        ssize_t bytes_sent = send(socket, response_buffer, strlen(response_buffer), 0);
        if (bytes_sent == -1) {
            perror("Error al enviar respuesta");
            close(socket);  // Cierra el socket antes de salir
            exit(EXIT_FAILURE);  // Termina el servidor de forma segura
        }}

    char * get_file_path(char request[]){
        char *file_path = strtok(request, " ");
        file_path = strtok(NULL, " ?");
        //mover puntero hacia delante elimina el "/" inicial
        return ++file_path;
    } 

char *get_content_type(char *request) {
    if (request == NULL) {
        return NULL;
    }

    printf("\nRequest inside get content type is: %s", request);

    // Copia el request para evitar modificar el original con strtok
    char *request_copy = strdup(request);
    if (request_copy == NULL) {
        perror("Error al duplicar la solicitud");
        return NULL;
    }

    char *line = strtok(request_copy, "\r\n");
    while (line != NULL) {
        printf("\n LIne! : %s", line); 
        // Busca la línea que contiene "Content-Type:"
        if (strstr(line, "Content-Type:") != NULL) {
            // Encuentra el inicio del valor del tipo de contenido
            char *content_type = strstr(line, ": ");
            if (content_type != NULL) {
                content_type += 2;  // Saltar el ": " para llegar al valor
                
                // Copiar el tipo de contenido a una nueva cadena
                char *content_type_copy = strdup(content_type);
                if (content_type_copy == NULL) {
                    perror("Error al asignar memoria para el tipo de contenido");
                    free(request_copy);
                    return NULL;
                }
                
                printf("\nSe recibió un contenido de tipo: %s", content_type_copy);

                // Verifica si el tipo de contenido es JSON o texto
                if (strcmp(content_type_copy, CONTENT_TYPE_TEXT) == 0 || strcmp(content_type_copy, CONTENT_TYPE_JSON) == 0) {
                    free(request_copy);  // Liberar la copia de la solicitud
                    return content_type_copy;  // Retornar el tipo de contenido
                }

                // Si no es JSON o texto, liberar la memoria y retornar NULL
                free(content_type_copy);
                break;
            }
        } else {
        }
        // Obtener la siguiente línea
        line = strtok(NULL, "\r\n");
    }
    printf("\nfin del while");

    free(request_copy);
    return NULL;
}

    bool validate_file_type(int new_socket, const char *content_type, const char *file_path) {
        bool is_valid = false;
        printf("\nContent_type es igual a : %s", content_type);
        if (!strcmp(content_type, CONTENT_TYPE_JSON)) {
            if (strstr(file_path, ".json")) {
                is_valid = true;
            } else {
                printf("\nNO es un application/json");
            }
        } else if (!strcmp(content_type, CONTENT_TYPE_TEXT)) {
            if (strstr(file_path, ".txt")) {
                is_valid = true;
            } else {
                printf("\nNO es un text/plain");
            }
        } else {
            printf("\nNO es tipo valido");
        }

        if (!is_valid) {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);
        }

        return is_valid;
    }


    // Nueva función para validar si un archivo existe
    bool validate_file_existence(int new_socket, const char *file_path) {
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
            send_response(new_socket, HTTP_NOT_FOUND, CONTENT_TYPE_TEXT, RESPONSE_NOT_FOUND);
            printf("\n Archivo no encontrado");
            return false;
        }
        fclose(file);
        return true;
    }

bool validate_file_path(int new_socket, const char *file_path) {
    bool is_valid = false;

    // Extraer la parte del directorio del file_path
    char directory_path[1024];
    strcpy(directory_path, file_path);

    // Encontrar la última ocurrencia de '/' para aislar el directorio
    char *last_slash = strrchr(directory_path, '/');
    if (last_slash != NULL) {
        *(last_slash + 1) = '\0';  // Terminar el string después del último '/'
    } else {
        // Si no hay '/', no es un camino válido
        send_response(new_socket, HTTP_FORBIDDEN, CONTENT_TYPE_TEXT, RESPONSE_ACCESS_DENIED); 
        printf("\n Acceso denegado para el archivo: %s", file_path);
        return false;
    }

    // Comparar directory_path con los endpoints válidos
    for (int i = 0; i < num_end_points; ++i) {
        if (strstr(directory_path, server_end_points[i]) == directory_path) {
            // Asegurar que no se acceda a un subdirectorio no permitido
            if (strlen(directory_path) <= strlen(server_end_points[i])) {
                is_valid = true;
                break;
            }
        }
    }

    // Si el directorio no es válido, enviar un error de acceso denegado
    if (!is_valid) {
        const char *error_message = "Access Denied";
        send_response(new_socket, "403 Forbidden", CONTENT_TYPE_TEXT, error_message);
        printf("\n Acceso denegado para el directorio: %s", directory_path);
    }

    return is_valid;
}

    // Nueva función para validar la solicitud
    bool validate_request(int new_socket, const char *content_type, const char *file_path, const char * content) {
        if (content_type == NULL || file_path == NULL) {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);
            return false;
        }
        if (!validate_file_type(new_socket, content_type, file_path)) {
            return false;
        }

        if (strcmp(content_type, CONTENT_TYPE_JSON) == 0 && (content  == NULL || strlen(content) == 0)) {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);
            return false;
        }

        return true;
    }


    // Nueva función para extraer el cuerpo de la solicitud
    char *extract_request_body(char *pRequest) {
        char *file_start = strstr(pRequest, "\r\n\r\n");
        return file_start ? file_start + 4 : NULL;
    }

    // Funciones auxiliares para GET
    bool query_exists(char request[]){
        char request_copy[1024] = {0};
        //hago una copia para no danar el path
        strcpy(request_copy, request);
        //obtengo el path
        char *query = strtok (request_copy, " ");
        query = strtok(NULL, " ");
        char *question_mark = strchr(query, '?');
        //si hay un ? es porque hay queries, los path no pueden contener ?
    	if (question_mark == NULL) {
            return false;
    	}
    	return true; 
    }
    
    char *get_query(char request[]){
    	char request_copy[1024]={0};
    	strcpy(request_copy, request);
    	//hacer una copia para no danar el path
    	char *query = strtok(request_copy," ");
    	query = strtok(NULL, " ");
    	//busca si en el path hay un ?
    	query = strchr(query, '?'); 
    	//quita el ? para devolver solo los queries
    	return query+1; 
    }
    
    
    // Función principal refactorizada
    char *trim_query(char *file_response, char *query) {
        char *query_key = strtok(query, "=");
        char *query_value = strtok(NULL, "& ");
        char *json = strchr(file_response, '{');

        if (json == NULL) {
            // Archivo de texto
            return find_key_in_text_file(file_response, query_key);
        } else {
            // Archivo JSON
            return find_key_in_json(file_response, query_key, query_value);
        }
    }

    // Función para encontrar el key en un archivo de texto
    char *find_key_in_text_file(char *file_response, char *query_key) {
        char *response = strtok(file_response, "\n");
        while (response != NULL) {
            if (strstr(response, query_key) != NULL) {
                return response;
            }
            response = strtok(NULL, "\n");
        }
        return "{}"; // No se encontró el key
    }

    // Función para encontrar el key en un archivo JSON
    char *find_key_in_json(char *file_response, char *query_key, char *query_value) {
        char *response = extract_json_block(file_response);
        while (response != NULL) {
            char *close = strchr(response, '}');
            if (close != NULL) {
                char *block_copy = create_copy_of_block(response, close);
                if (is_key_in_block(block_copy, query_key, query_value)) {
                    return block_copy;
                }
                free(block_copy);
            }
            response = close + 1;
        }
        return "{}"; // No se encontró el key
    }   

    // Extraer el bloque JSON completo
    char *extract_json_block(char *response) {
        return strstr(response, "{");
    }

    // Crear una copia del bloque JSON
    char *create_copy_of_block(char *response, char *close) {
        size_t length = close - response + 1;
        char *block_copy = (char *)malloc(length + 1);
        strncpy(block_copy, response, length);
        block_copy[length] = '\0';  // Asegurar la terminación del string
        return block_copy;
    }

    // Comprobar si el key y valor están en el bloque JSON
    bool is_key_in_block(char *block, char *query_key, char *query_value) {
        char key_search[256];
        snprintf(key_search, sizeof(key_search), "\"%s\"", query_key);
        char *found_key = strstr(block, key_search);
    
        if (found_key != NULL) {
            if (query_value != NULL) {
                return strstr(block, query_value) != NULL;
            }
            return true; // Solo se buscaba el key
        }
        return false; // No se encontró el key
    }

    // Refactorización de las funciones handle
    void handle_get_request(int new_socket, char request[]) {
        FILE *file;
        char file_response[1024] = {0};
        char *message_type = HTTP_OK;

        // Check if there are queries, prior to file management
        bool query_exist = query_exists(request);
        char *query = query_exist ? get_query(request) : NULL;

        // Buscar archivo
        const char *file_path = get_file_path(request);
        printf("\n\nEl socket %d está haciendo get del archivo: %s",new_socket,file_path);

        pthread_mutex_t *file_mutex = get_file_mutex(file_path);
        pthread_mutex_lock(file_mutex);

        if (!validate_file_path(new_socket, file_path) || !validate_file_existence(new_socket, file_path)) {
            pthread_mutex_unlock(file_mutex);
            return;
        }

        // Check file type
        char *content_type = strstr(file_path, ".json") ? CONTENT_TYPE_JSON : CONTENT_TYPE_TEXT;

        // Read whole file and store it in file_response
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);

        pthread_mutex_unlock(file_mutex);

        // If query, trim file_response and only return the query or {} if not found 
        if (query_exist) {
            strcpy(file_response, trim_query(file_response, query));
        }

        // If file is {}, then query was not found
        if (!strcmp(file_response, "{}")) {
            message_type = HTTP_NOT_FOUND;
        }

        // If file is empty
        if (!strcmp(file_response, "")) {
            message_type = HTTP_NO_CONTENT;
        }

        send_response(new_socket, message_type, content_type, file_response);
    }

    void handle_post_request(int new_socket, char pRequest[]) {
        char file_response[1024] = {0};

        const char *content_type = get_content_type(pRequest);
        const char *file_start = extract_request_body(pRequest);
        const char *file_path = get_file_path(pRequest);


        pthread_mutex_t *file_mutex = get_file_mutex(file_path);
        pthread_mutex_lock(file_mutex);

        // Validate request
        if (!validate_file_path(new_socket, file_path) || !validate_request(new_socket, content_type, file_path, file_start) || !validate_file_existence(new_socket, file_path)) {
            pthread_mutex_unlock(file_mutex);
            return;
        }

         // Write content to file
        FILE *file = fopen(file_path, "w");
        if (file == NULL) {
            pthread_mutex_unlock(file_mutex);  // Unlock mutex before returning
            send_response(new_socket, HTTP_INTERNAL_SERVER_ERROR, content_type, RESPONSE_ERROR);
            return;
        }
        
        fwrite(file_start, sizeof(char), strlen(file_start), file);
        fclose(file);

        // Respond with success message
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        pthread_mutex_unlock(file_mutex);  // Unlock mutex before returning
        send_response(new_socket, HTTP_OK, content_type, file_response);
    }

    void handle_put_request(int new_socket, char pRequest[]) {
        char file_response[1024] = {0};

        const char *content_type = get_content_type(pRequest);
        const char *file_start = extract_request_body(pRequest);
        const char *file_path = get_file_path(pRequest);

        pthread_mutex_t *file_mutex = get_file_mutex(file_path);
        pthread_mutex_lock(file_mutex);
        // Validate request
        if (!validate_file_path(new_socket, file_path) || !validate_request(new_socket, content_type, file_path, file_start)) {
            pthread_mutex_unlock(file_mutex);
            return;
        }

        // Open file for writing (create if it doesn't exist)
        FILE *file = fopen(file_path, "w");
        if (file == NULL) {
            pthread_mutex_unlock(file_mutex);
            send_response(new_socket, HTTP_INTERNAL_SERVER_ERROR, content_type, RESPONSE_ERROR);
            return;
        }

        // Write content to file
        fwrite(file_start, sizeof(char), strlen(file_start), file);
        fclose(file);

        // Respond with success message
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        pthread_mutex_unlock(file_mutex);
        send_response(new_socket, HTTP_OK, content_type, file_response);
    }

    void handle_update_request(int new_socket, char pRequest[]) {
        char file_response[1024] = {0};

        const char *content_type = get_content_type(pRequest);
        const char *file_start = extract_request_body(pRequest);
        const char *file_path = get_file_path(pRequest);


        pthread_mutex_t *file_mutex = get_file_mutex(file_path);
        pthread_mutex_lock(file_mutex);

        // Validate request
        if (!validate_file_path(new_socket, file_path) || !validate_request(new_socket, content_type, file_path, file_start)) {
            pthread_mutex_unlock(file_mutex);
            return;
        }

        // Validate file existence
        if (!validate_file_existence(new_socket, file_path)) {
            pthread_mutex_unlock(file_mutex);
            return;
        }

        bool resolve = false;

        // Handle JSON and text file updates
        if (strstr(file_path, ".json")) {
            FILE *file = fopen(file_path, "r");
            resolve = update_json_file(file, file_start, file_path);
            fclose(file);
        } else if (strstr(file_path, ".txt")) {
            resolve = update_text_file(file_path, file_start);
        }

        // If update failed, send a bad request response
        if (!resolve) {
            pthread_mutex_unlock(file_mutex);
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);
            return;
        }

        // Respond with success message
        FILE *file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        pthread_mutex_unlock(file_mutex);
        send_response(new_socket, HTTP_OK, content_type, file_response);
    }


    void handle_delete_request(int new_socket, char request[]) {
        const char *file_path = get_file_path(request);
        const char *content_type = get_content_type(request);

        pthread_mutex_t *file_mutex = get_file_mutex(file_path);
        pthread_mutex_lock(file_mutex);

        if (!validate_file_path(new_socket, file_path) || !validate_file_existence(new_socket, file_path)) {
            return;
        }

        remove(file_path);
        send_response(new_socket, HTTP_NO_CONTENT, content_type, NULL);
    }




