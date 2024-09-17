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

#define PORT 8081

    int  open_server(int * server_fd);
    void *handle_client(int new_socket);
    char *getFilePath(char *pRequesr);
    void format_response(char * response_buffer, char *status_code, char *content_type, int content_length, char * file_response);
    void handle_get(int new_socket, char request[]);
    void handle_post(int new_socket,char pRequest[]);

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

            handle_client(new_socket);
            
        }
        
        close(server_fd);
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

    void * handle_client(int new_socket) {

        //Buffer del cliente 
        //
        //
        char request[4000] = {0};
        // Leer la solicitud del cliente
        read(new_socket, request, sizeof(request));//ERROR
        
        printf("\r\nRequest recibida: \r\n%s \r\n\r\n", request);
    
        // Determinar si es una solicitud GET o POST
        if (strncmp(request, "GET", 3) == 0) {
            handle_get(new_socket, request);
            printf("\nGET request manejada\n");
        } else if (strncmp(request, "POST", 4) == 0) {
            handle_post(new_socket, request);
            printf("\nPOST request manejada\n");
        }
        printf("\nCerrando la conexion del cliente\n");
        close(new_socket);
    }


    char * getFilePath(char request[]){
        char *file_path = strtok(request, " ");
        file_path = strtok(NULL, " ?");
        //mover puntero hacia delante elimina el "/" inicial
        return ++file_path;
    } 

    void format_response (char * response_buffer, char *status_code, char *content_type, int content_length, char * file_response) {

        sprintf(response_buffer, "HTTP/1.1 %s \r\n "
                "Content-Type: %s\r\n"
                " Content-Length: %d\r\n\r\n"
                " %s",
                status_code, content_type,content_length,file_response);
    }

    // Manejo de GET request
    void handle_get(int new_socket, char request[]) {
        FILE *file;
        char file_response[1024] = {0};
        char response_buffer[2048];
        const char *file_path =  getFilePath(request);
        file = fopen(file_path, "r");
        
        
        
        if (file == NULL) {
            format_response(response_buffer, "404  Not Found", "application/json", 8, "Not Found");
            // Enviar la respuesta al cliente
            send(new_socket, response_buffer, strlen(response_buffer), 0);
            return;
        }
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);

        // Formar la respuesta HTTP
        format_response(response_buffer, "200 OK", "application/json", strlen(file_response), file_response);

        // Enviar la respuesta al cliente
        send(new_socket, response_buffer, strlen(response_buffer), 0);
    }


    // Manejo de POST request
    void handle_post(int new_socket, char pRequest[]) {
        char *file_start;
        FILE *file;
        char request[4000] = {0};
        char response_buffer[2048];
	    char file_response[1024] = {0};
        // Encontrar el inicio del JSON en la solicitud POST (después del doble salto de línea)
        file_start = strstr(pRequest, "\r\n\r\n");
        file_start += 4;  // Saltar el doble salto de línea
	    printf("Mensaje por escribir:\r\n%s\r\n", file_start);
        if (strlen(file_start) > 0)  {
	    // Agarrar el path del archivo
	    const char *file_path =  getFilePath(pRequest);
        	
        // Abrir el archivo response.JSON para escribir el nuevo contenido
        file = fopen(file_path, "w"); //w para sobre escribir, a para agregar al final
        if (file == NULL) {
            perror("No se pudo abrir el archivo JSON para escribir");
            exit(EXIT_FAILURE);
        }
    
        // Escribir el contenido del JSON en el archivo
        fwrite(file_start, sizeof(char), strlen(file_start), file);
		fclose(file);
	    
	    //GET CONTENT TYPE
	    pRequest = strtok(NULL, "\r\n");
	    pRequest = strtok(NULL, "\r\n");
	    pRequest = strstr(pRequest, ": ");
	    pRequest = pRequest+2;
	    
        // Responder al cliente (POSTMAN)con un mensaje de éxito
        file = fopen(file_path, "r");
        fread(file_response, sizeof(char), sizeof(file_response), file);
        fclose(file);

        // Formar la respuesta HTTP
        format_response(response_buffer, "201 Created", pRequest, strlen(file_response), file_response);

        // Enviar la respuesta al cliente
        send(new_socket, response_buffer, strlen(response_buffer), 0);
        }
        else
            printf("\nJSON msj NULO");
    }

