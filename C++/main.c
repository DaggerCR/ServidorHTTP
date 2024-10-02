    /*
        http://localhost:8082/
        http://localhost:8081/../JSON/response.json

        gcc -o main main.cpp
        ./main

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
#include "file_operations.h"

#define PORT 8081

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
    void handle_get_request(int new_socket, char request[]);
    void handle_post_request(int new_socket,char pRequest[]);
    void handle_put_request(int new_socket,char pRequest[]);
    void handle_update_request(int new_socket, char pRequest[]);

    int main() 
    {
        int server_fd;

        //abrir el servidor a conecciones 
        if (open_server(&server_fd)) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }; 

        int salir = 0;
        while (true) 
        {
            int new_socket;
            struct sockaddr_in client_address;
            socklen_t client_address_len = sizeof(client_address);

            // Aceptar una conexión entrante
            new_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
            if (new_socket < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            handle_client_request(new_socket);
            
        }
        return 0;
    }


    int open_server(int * server_fd) {
        struct sockaddr_in server_address; 
        
        // Crear el socket
        *server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (*server_fd == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // // Since the tester restarts your program quite often, setting SO_REUSEADDR
        // // ensures that we don't run into 'Address already in use' errors
        
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

    void * handle_client_request(int new_socket) {

        //Buffer del cliente 
        //
        //
        char request[4000] = {0};
        // Leer la solicitud del cliente
        read(new_socket, request, sizeof(request));//ERROR
        
        printf("\r\nRequest recibida: \r\n%s \r\n\r\n", request);
    
        // Determinar si es una solicitud GET o POST
        if (strncmp(request, "GET", 3) == 0) {
            handle_get_request(new_socket, request);
            printf("\nGET request manejada\n");
        } else if (strncmp(request, "POST", 4) == 0) {
            handle_post_request(new_socket, request);
            printf("\nPOST request manejada\n");
        }
         else if (strncmp(request, "PUT", 3) == 0) {
            handle_put_request(new_socket, request);
            printf("\nPUT request manejada\n");
        }
         else if (strncmp(request, "PATCH", 5) == 0) {
            handle_update_request(new_socket, request);
            printf("\nUPDATE request manejada\n");
        }
         else if (strncmp(request, "DELETE", 6) == 0) {
            handle_delete_request(new_socket, request);
            printf("\nDELETE request manejada\n");
        }
        printf("\nCerrando la conexion del cliente\n");
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

    // Manejo de GET request
    void handle_get_request(int new_socket, char request[]) {
        FILE *file;
        char file_response[1024] = {0};
        const char *file_path =  get_file_path(request);
        const char * content_type = get_content_type(pRequest);

        if (!validate_file_type(new_socket, content_type, file_path)){
            return;
        }

        file = fopen(file_path, "r");
        
        if (file == NULL) {
            send_response(new_socket, HTTP_NOT_FOUND, CONTENT_TYPE_TEXT, RESPONSE_NOT_FOUND);  
            return;
        }
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);
        send_response(new_socket, HTTP_OK,content_type, file_response);  
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


    // Manejo de GET request
    void handle_delete_request(int new_socket, char request[]) {
        FILE *file;
        char file_response[1024] = {0};
        const char *file_path =  get_file_path(request);
        const char * content_type = get_content_type(pRequest);

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
