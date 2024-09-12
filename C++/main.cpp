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

int main(int argc, char const *argv[]) 
{
    int server_fd; //
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int puerto = 8082;
    char buffer[2048] = {0};
    
    // 1. Crear el socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configurar la dirección y puerto
    address.sin_family = AF_INET;          // IPv4
    address.sin_addr.s_addr = INADDR_ANY;  // Aceptar conexiones en cualquier interfaz
    address.sin_port = htons(puerto);        // Escuchar en el puerto 

    // 3. Vincular el socket con la dirección y puerto
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 4. Empezar a escuchar en el puerto
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    else
        printf("Servidor escuchando en el puerto %d...\n",puerto);

    // 5. Aceptar una conexión entrante
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    else{
        printf("Conexión aceptada\n");
    }

    // 6. Leer datos del cliente
    read(new_socket, buffer, sizeof(buffer));
    printf("Mensaje del cliente: %s\n", buffer);

    // 7. Enviar una respuesta al cliente
    const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 13\n\nHola usuario";
    send(new_socket, response, strlen(response), 0);

    

    // Cerrar los sockets
    close(new_socket);
    close(server_fd);

    printf("\n\nFin de la transmisión\n");

    return 0;
}

