/*
    http://localhost:8082/

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
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080

void handle_get(int new_socket);
void handle_post(int new_socket,char request[4000]);

int main() 
{
    int server_fd, new_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);
    char request[4000] = {0};

    // Crear el socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección y puerto
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Vincular el socket
    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d...\n", PORT);

    int salir =0;
    while (true) 
    {
        // Aceptar una conexión entrante
        new_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
        if (new_socket < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Leer la solicitud del cliente
        read(new_socket, request, sizeof(request));

        // Determinar si es una solicitud GET o POST
        if (strncmp(request, "GET", 3) == 0) {
            printf("\nGET request recibida\n");
            handle_get(new_socket);
        } else if (strncmp(request, "POST", 4) == 0) {
            handle_post(new_socket,request);
        }
        close(new_socket);
        
        
    }
    
    close(server_fd);
    return 0;
}

// Manejo de GET request
void handle_get(int new_socket) {
    FILE *json_file;
    char json_response[1024] = {0};
    const char *json_file_path = "../JSON/response.json";

    // Abrir y leer el archivo JSON
    json_file = fopen(json_file_path, "r");
    if (json_file == NULL) {
        perror("No se pudo abrir el archivo JSON");
        exit(EXIT_FAILURE);
    }
    fread(json_response, sizeof(char), sizeof(json_response), json_file);
    fclose(json_file);

    // Formar la respuesta HTTP
    char response[2048];
    sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\nContent-Length: %ld\n\n%s", strlen(json_response), json_response);

    // Enviar la respuesta al cliente
    send(new_socket, response, strlen(response), 0);
}

// Manejo de POST request
void handle_post(int new_socket, char pRequest[4000]) {
    char *json_start;
    FILE *json_file;
    const char *json_file_path = "../JSON/response.json";
    char request[4000] = {0};

    // Leer el cuerpo del POST 
    read(new_socket, request, sizeof(request));

    // Encontrar el inicio del JSON en la solicitud POST (después del doble salto de línea)
    json_start = strstr(pRequest, "\r\n\r\n");
    if (json_start != NULL) {
        json_start += 4;  // Saltar el doble salto de línea

        // Abrir el archivo response.JSON para escribir el nuevo contenido
        json_file = fopen(json_file_path, "w");
        if (json_file == NULL) {
            perror("No se pudo abrir el archivo JSON para escribir");
            exit(EXIT_FAILURE);
        }

        // Escribir el contenido del JSON en el archivo
        fwrite(json_start, sizeof(char), strlen(json_start), json_file);
        fclose(json_file);

        // Responder al cliente con un mensaje de éxito
        const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 19\n\nJSON guardado con éxito";
        send(new_socket, response, strlen(response), 0);
    }
    else
        printf("\njson start nulo");

}
