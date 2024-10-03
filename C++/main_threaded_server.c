
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

pthread_t thread_pool[THREAD_POOL_SIZE];  // Pool de hilos

// Estructura para manejar las solicitudes de cliente
typedef struct {
    int socket_fd;
} client_request_t;

// Cola de solicitudes pendientes
client_request_t request_queue[THREAD_POOL_SIZE];
int request_count = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void *handle_client_request(void *arg);
void *thread_function(void *arg);
void enqueue_request(int new_socket);
int dequeue_request();

int open_server(int *server_fd);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if (open_server(&server_fd) < 0) {
        fprintf(stderr, "Error al abrir el servidor");
        exit(EXIT_FAILURE);
    }

    // Inicializamos el pool de hilos
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }

    printf("Servidor escuchando en el puerto %d...", PORT);

    while (true) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("Error en accept");
            exit(EXIT_FAILURE);
        }
        enqueue_request(new_socket);  // Añadir la solicitud a la cola
    }

    close(server_fd);
    return 0;
}

// Función para manejar las solicitudes de cliente
void *handle_client_request(void *arg) {
    int client_socket = *(int *)arg;
    char request[RESPONSE_BUFFER_SIZE] = {0};

    read(client_socket, request, sizeof(request)); // Leer la solicitud del cliente
        printf("\r\nRequest recibida: \n \r\n%s \r\n\r\n", request);
    
        // Determinar si es una solicitud GET o POST u otro
        if (strncmp(request, "GET", 3) == 0) 
        {
            handle_get_request(client_socket, request);
            printf("\nGET request manejada\n");
        } 
        else 
            if (strncmp(request, "POST", 4) == 0) {
                handle_post_request(client_socket, request);
                printf("\nPOST request manejada\n");
            }
            else 
                if (strncmp(request, "PUT", 3) == 0) {
                    handle_put_request(client_socket, request);
                    printf("\nPUT request manejada\n");
                }
                else 
                    if (strncmp(request, "PATCH", 5) == 0) {
                        handle_update_request(client_socket, request);
                        printf("\nUPDATE request manejada\n");
                    }
                    else 
                        if (strncmp(request, "DELETE", 6) == 0) {
                            handle_delete_request(client_socket, request);
                            printf("\nDELETE request manejada\n");
                        }
        printf("\nSolicitud atendida    , cerrando socket...\n");

    close(client_socket);
    return NULL;
}

// Función para los hilos del pool
void *thread_function(void *arg) {
    while (true) {
        int client_socket = dequeue_request();  // Obtener una solicitud de la cola
        if (client_socket != -1) {
            handle_client_request(&client_socket);
        }
    }
    return NULL;
}

// Añadir una solicitud a la cola
void enqueue_request(int new_socket) {
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

// Abrir el servidor en el puerto especificado
int open_server(int *server_fd) {
    struct sockaddr_in address;

    if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(*server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        return -1;
    }

    if (listen(*server_fd, 10) < 0) {
        perror("Error en listen");
        return -1;
    }

    return 0;
}
