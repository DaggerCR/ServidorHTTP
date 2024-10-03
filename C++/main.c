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

#define HTTP_OK "200 OK"
#define HTTP_CREATED "201 Created"
#define HTTP_NO_CONTENT "204 No Content"
#define HTTP_NOT_FOUND "404 Not Found"
#define HTTP_BAD_REQUEST "400 Bad Request"

#define CONTENT_TYPE_JSON "application/json"
#define CONTENT_TYPE_TEXT "text/plain"

#define RESPONSE_NOT_FOUND "Not Found"
#define RESPONSE_ERROR "Error"
#define RESPONSE_BUFFER_SIZE 2048

    int  open_server(int * server_fd);
    void *handle_client_request(int new_socket);
    char *get_file_path(char *pRequesr);

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
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);

        if (open_server(&server_fd) < 0) {
            fprintf(stderr, "Error al abrir el servidor");
            exit(EXIT_FAILURE);
        }

        // Pool de hilos
        for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
            pthread_create(&thread_pool[i], NULL, thread_function, NULL);
        }

        printf("Servidor escuchando en el puerto %d...", PORT);

        while (true) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (new_socket < 0) {
                perror("Error al abrir el socket");
                exit(EXIT_FAILURE);
            }
            enqueue_request(new_socket);  // se pone la solicitud en la cola de hilos
        }
    
    }

    // Función para los hilos del pool
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
        printf("\n Añadiendo al pool de hilos la solicitud...");
        pthread_mutex_lock(&queue_mutex);

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

        printf("Servidor escuchando en el puerto %d...\n", PORT);
        return 0;
    }

    /**
     * Método principal para manejar las solicitudes del cliente
     * new_socket: socket donde se alojará la solicitud 
     */
    void * handle_client_request(int new_socket) {
        char request[4000] = {0}; //Buffer del cliente, contiene la solicitud completa del cliente
        read(new_socket, request, sizeof(request)); // Leer la solicitud del cliente
        printf("\r\nRequest recibida: \n \r\n%s \r\n\r\n", request);
    
        // Determinar si es una solicitud GET o POST u otro
        if (strncmp(request, "GET", 3) == 0) 
        {
            handle_get_request(new_socket, request);
            printf("\nGET request manejada\n");
        } 
        else 
            if (strncmp(request, "POST", 4) == 0) {
                handle_post_request(new_socket, request);
                printf("\nPOST request manejada\n");
            }
            else 
                if (strncmp(request, "PUT", 3) == 0) {
                    handle_put_request(new_socket, request);
                    printf("\nPUT request manejada\n");
                }
                else 
                    if (strncmp(request, "PATCH", 5) == 0) {
                        handle_update_request(new_socket, request);
                        printf("\nUPDATE request manejada\n");
                    }
                    else 
                        if (strncmp(request, "DELETE", 6) == 0) {
                            handle_delete_request(new_socket, request);
                            printf("\nDELETE request manejada\n");
                        }
        printf("\nSolicitud atendida    , cerrando socket...\n");
        close(new_socket);
    }

    void send_response(int socket, const char* status_code, const char* content_type, const char* content) {
        char response_buffer[RESPONSE_BUFFER_SIZE];
        snprintf(response_buffer, sizeof(response_buffer),"HTTP/1.1 %s \r\n "
                "Content-Type: %s \r\n"
                " Content-Length: %zu \r\n\r\n"
                " %s",
                status_code, content_type,strlen(content), content);
        send(socket, response_buffer, strlen(response_buffer), 0);
    }

    char * get_file_path(char request[]){
        char *file_path = strtok(request, " ");
        file_path = strtok(NULL, " ?");
        //mover puntero hacia delante elimina el "/" inicial
        return ++file_path;
    } 

    char * get_content_type(char *request){
        if (request == NULL){
            return NULL;
        }
	    request = strtok(NULL, "\r\n");
	    request = strtok(NULL, "\r\n");
	    request = strstr(request, ": ");
	    request = request+2;
        printf("\nSe recibio un ocntenido de tipo: %s", request);
        if (!strcmp(request, CONTENT_TYPE_TEXT)){
            return CONTENT_TYPE_TEXT;
        } else if (!strcmp(request, CONTENT_TYPE_JSON)) {
            return CONTENT_TYPE_JSON;
        }
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
    
    char *trim_query(char *file_response, char *query){
    	//busca el key del query
    	char *query_key = strtok(query, "=");
    	// busca el value del query
    	// "&" si hay, " " si no hay
    	char *query_value = strtok(NULL, "& ");
    	char *json = strchr(file_response, '{');
    	//busco si hay { para ver si es un json o txt
    	if (json==NULL){
    	    //es un text file
    	    //devuelve la oracion donde esta el key
    	    char *response = file_response;
    	    size_t final_length = strlen(response);
    	    //Tokenizo cada oracion
    	    response = strtok(file_response, "\n");
    	    //mientras no se acaben los tokens
    	    while(response!=NULL){ 
    	       //si la palabra se encuentra en la oracion
    	       if(strstr(response, query_key)!=NULL){
    	          return response;
    	       }
    	       //Key was not found, look in the next sentence
    	       response = strtok(NULL, "\n");
    	   }
    	}
    	else{
    	   char *response = strstr(file_response, "{");
    	   size_t final_length = strlen(response);
    	   //while it is inside the boundaries of the file string
    	   while(response<file_response+final_length){
    	   	int braket_counter = 0;
    	   	char *open = response;
    	   	char *close = response;
    	   	size_t length = strlen(response);
    	   	//find the } that matches the first {
    	   	for (int i = 0; i<length; i++){
    	   	   if (response[i] == '{') braket_counter++;
    	   	   if (response[i] == '}') {
    	   	      braket_counter--;
    	   	      close = response + i;
    	   	   }
    	   	   if (braket_counter == 0) break;
    	   	   else close = response + i;
    	   	}
    	   	// a {} was found or the whole file will be taken into account
    	   	if (response!=NULL && close !=NULL && close>response){
    	   	   //copy instead of trim in case it is not the correct {}
    	   	   size_t length = close - response +1;  
		   char *respond_copy = strdup(response);
		   strncpy(respond_copy, response, length);
	           respond_copy[length] = '\0';  // Null-terminate the result string
	           // searches key and value inside { }
	           char *found_key = strstr(respond_copy, ("\"%s\"", query_key));
	           if (found_key != NULL) {
	              //key value was found
	              if (query_value != NULL){
	              	 //query value was assigned
	              	 //have to check if it matches too
	              	 char *found_value = strstr(respond_copy, query_value);
	              	 if (found_value!=NULL){
	              	    //value was found
	              	    return respond_copy;
	              	 }
	              }
	              else{
	                 //query value was not assigned
	                 return respond_copy;
	              }
	           }
    	   	
    	   	}
    	   	// {} did not match, try in next { }
    	   	response = close + 1;
    	   }
    	}
    	//the query did not match the text
    	return "{}";
    }


    // Manejo de GET request
    void handle_get_request(int new_socket, char request[]) {
        FILE *file;
        char file_response[1024] = {0};
        char *message_type = HTTP_OK;
        
        //Check if there are queries, prior to file managment
        bool query_exist = query_exists(request);
        char *query;
        if (query_exist){
           query = get_query(request);
        }
        
        //Buscar archivo
        const char *file_path =  get_file_path(request);
        printf("\n Archivo a buscar: %s",file_path);
        file = fopen(file_path, "r");
        
        //Si el archivo no existe
        if (file == NULL) {
            send_response(new_socket, HTTP_NOT_FOUND, CONTENT_TYPE_TEXT, RESPONSE_NOT_FOUND);  
            printf("\n Archivo no encontrado");
            return;
        }
        
        //Si el archivo si existe
        //Check what type is the file 
        char *content_type = CONTENT_TYPE_TEXT;
        if(strstr(file_path, ".json") != NULL)
        {
           printf("\n Archivo encontrado"); 
           content_type = CONTENT_TYPE_JSON;
        }
        
        //Read whole file and store it in file_response
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        
        //If query, trim file_response and only return the query or {} if not found 
        if (query_exist){
           strcpy(file_response, trim_query(file_response, query));
        }
        
        //if file is {}, then query was not found
        if(!strcmp(file_response, "{}")) {
            message_type = HTTP_NOT_FOUND;
        }

        //if file is {}, then query was not found
        if(!strcmp(file_response, "")){
            message_type =  HTTP_NO_CONTENT;
        }

        send_response(new_socket, HTTP_OK, content_type, file_response);   
    }

    // Manejo de POST request
    void handle_post_request(int new_socket, char pRequest[]) {
        char *file_start;
        FILE *file;
	    char file_response[1024] = {0};
        // Encontrar el inicio del JSON en la solicitud POST (después del doble salto de línea)
        file_start = strstr(pRequest, "\r\n\r\n");
        file_start += 4;  // Saltar el doble salto de línea
	    printf("Mensaje por escribir:\r\n%s\r\n", file_start);
        
        // Agarrar el path del archivo
        const char *file_path =  get_file_path(pRequest);
        const char * content_type = get_content_type(pRequest);

        if (!validate_file_type(new_socket, content_type, file_path)){
            return;
        }
        
        if (strlen(file_start) ==  0 && !strcmp(content_type, CONTENT_TYPE_JSON))  {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);  
            return;
        }
        // Abrir el archivo response.JSON para escribir el nuevo contenido
        file = fopen(file_path, "r"); //w para sobre escribir, a para agregar al final
        if (file == NULL) {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);  
            return;
        }
        fclose(file);
        fopen(file_path, "w");
        // Escribir el contenido del JSON en el archivo
        fwrite(file_start, sizeof(char), strlen(file_start), file);
		fclose(file);

        // Responder al cliente (POSTMAN)con un mensaje de éxito
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        send_response(new_socket, HTTP_OK, content_type, file_response);  
    }
    
    // Manejo de PUT request
    void handle_put_request(int new_socket, char pRequest[]) {
        char *file_start;
        FILE *file;
	char file_response[1024] = {0};
	char confirmation_message[15];
        // Agarrar el path del archivo
	
        // Encontrar el inicio del JSON en la solicitud POST (después del doble salto de línea)
        file_start = strstr(pRequest, "\r\n\r\n");
        file_start += 4;  // Saltar el doble salto de línea
        printf("Mensaje por escribir:\r\n%s\r\n", file_start);


        const char *file_path =  get_file_path(pRequest);
        const char * content_type = get_content_type(pRequest);

        if (!validate_file_type(new_socket, content_type, file_path)){
            return;
        }

        if (strlen(file_start) <=  0 && !strcmp(content_type, CONTENT_TYPE_JSON))  {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);  
            return;
        }
	    
        //Ver si el archivo existe, de lo contrario se creara
        file = fopen(file_path, "r"); 
        if (file){
           snprintf(confirmation_message, sizeof(confirmation_message), "200 OK");
           fclose(file); //luego se abre en modo escritura
        }
        else {
           file = fopen(file_path, "w"); 
           snprintf(confirmation_message, sizeof(confirmation_message), "201 Created");
           fclose(file); //luego se abre en modo escritura
        }
        
        // Abrir el archivo response.JSON para escribir el nuevo contenido
        file = fopen(file_path, "w"); //w para sobre escribir, a para agregar al final
        if (file == NULL) {
            perror("Error: no se pudo abrir el archivo JSON para escribir");
            exit(EXIT_FAILURE);
        }
    
        // Escribir el contenido del JSON en el archivo
        fwrite(file_start, sizeof(char), strlen(file_start), file);
		fclose(file);
	    
        // Responder al cliente (POSTMAN)con un mensaje de éxito
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        
        send_response(new_socket, HTTP_OK, CONTENT_TYPE_TEXT, file_response);  
    }

    void handle_update_request(int new_socket, char pRequest[]) {
        char *file_start;
        FILE *file;
	    char file_response[1024] = {0};
	
        // Encontrar el inicio del contenido en la solicitud update (después del doble salto de línea)
        file_start = strstr(pRequest, "\r\n\r\n");
        file_start += 4;  // Saltar el doble salto de línea
	    printf("Contenido de la actualizacion:\r\n%s\r\n", file_start);
        const char *file_path =  get_file_path(pRequest);
        const char * content_type = get_content_type(pRequest);

        printf("\nFilepath es: %s", file_path);

        if (!validate_file_type(new_socket, content_type, file_path)){
            return;
        }

        if (strlen(file_start) <=  0 && !strcmp(content_type, CONTENT_TYPE_JSON))  {
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);  
            return;
        }
        file = fopen(file_path, "r"); 
        if (file == NULL){
            send_response(new_socket, HTTP_NOT_FOUND, content_type, RESPONSE_NOT_FOUND);  
            return;
        }

        printf("\nContenido es: %s", pRequest);
        bool resolve=false;

        if(strstr(file_path, ".json")){
            file = fopen(file_path, "r"); 
            resolve = update_json_file(file, file_start, file_path);
            fclose(file);
        } else if (strstr(file_path, ".txt")){
            resolve = update_text_file(file_path, file_start);
        }

        if (!resolve){
            send_response(new_socket, HTTP_BAD_REQUEST, content_type, RESPONSE_ERROR);  
            return;
        }

        // Responder al cliente (POSTMAN)con un mensaje de éxito
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);

        send_response(new_socket, HTTP_OK, CONTENT_TYPE_TEXT, file_response);  
    }


    // Manejo de DELETE request
    void handle_delete_request(int new_socket, char request[]) {
        FILE *file;
        char file_response[1024] = {0};
        const char *file_path =  get_file_path(request);
        const char * content_type = get_content_type(request);
        if (!validate_file_type(new_socket, content_type, file_path)){
            return;
        }
	
        file = fopen(file_path, "r");
        if (file == NULL) {
            send_response(new_socket, HTTP_NOT_FOUND, CONTENT_TYPE_TEXT, RESPONSE_NOT_FOUND);  
            return;
        }
        remove(file_path);
        fclose(file);
        
        send_response(new_socket, HTTP_NO_CONTENT, content_type, "");  
    }
