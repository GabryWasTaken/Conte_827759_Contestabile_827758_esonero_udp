/*
 * main.c
 *
 * UDP Client - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP client
 * portable across Windows, Linux and macOS.
 */

#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "protocol.h"

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

int main(int argc, char *argv[]) {

	// Implement client logic

#if defined WIN32
	// Initialize Winsock
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
	if (result != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		return 0;
	}
#endif

    // Variabili per argomenti e connessione
    char *server_host = "localhost"; // Default host cambiato per supportare nomi DNS
    int port = SERVER_PORT;
    char *request_arg = NULL;
    int my_socket;
    struct sockaddr_in server_addr;
    struct hostent *host; // Per risoluzione DNS

    // Parsing argomenti da riga di comando
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_host = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            request_arg = argv[++i];
        }
    }

    if (request_arg == NULL) {
        printf("Errore: Parametro -r obbligatorio.\n");
        printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", argv[0]);
        clearwinsock();
        return -1;
    }

    // Controllo caratteri di tabulazione (richiesto da specifica)
    if (strchr(request_arg, '\t') != NULL) {
        printf("Errore: La richiesta contiene caratteri di tabulazione.\n");
        clearwinsock();
        return -1;
    }

    // Parsing della stringa di richiesta (es. "t bari" o "p Reggio Calabria")
    weather_request_t req;
    memset(&req, 0, sizeof(req)); // Pulizia memoria

    // Validazione formato "città tipo"
    char *first_space = strchr(request_arg, ' ');
    if (first_space == NULL) {
        printf("Errore: Formato richiesta invalido. Manca lo spazio.\n");
        clearwinsock();
        return -1;
    }

    // Calcolo lunghezza del primo token (deve essere 1 char)
    int type_len = first_space - request_arg;
    if (type_len != 1) {
        printf("Errore: Il tipo deve essere un singolo carattere.\n");
        clearwinsock();
        return -1;
    }

    req.type = request_arg[0];
    char *city_ptr = first_space + 1;

    // Controllo lunghezza città (max 63 caratteri + terminatore)
    if (strlen(city_ptr) >= 64) {
        printf("Errore: Nome città troppo lungo (max 63 caratteri).\n");
        clearwinsock();
        return -1;
    }

    strncpy(req.city, city_ptr, sizeof(req.city) - 1);

    // Risoluzione DNS del server (nome o IP)
    host = gethostbyname(server_host);
    if (host == NULL) {
        printf("Errore: Impossibile risolvere il server %s.\n", server_host);
        clearwinsock();
        return -1;
    }

	// Create socket (UDP = SOCK_DGRAM)
	my_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (my_socket < 0) {
        printf("Errore nella creazione del socket.\n");
        clearwinsock();
        return -1;
    }

	// Configure server address
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr*) host->h_addr);

	// Implement communication logic

    char send_buffer[BUFFER_SIZE];
    int offset = 0;

    // 1. Serializza Type (1 byte)
    memcpy(send_buffer + offset, &req.type, sizeof(char));
    offset += sizeof(char);

    // 2. Serializza City (lunghezza variabile + null terminator)
    int city_len = strlen(req.city) + 1;
    memcpy(send_buffer + offset, req.city, city_len);
    offset += city_len;

    // 1. Invio Richiesta (sendto per UDP)
	if (sendto(my_socket, send_buffer, offset, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Errore nell'invio dei dati.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // 2. Ricezione Risposta
    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    int from_len = sizeof(from_addr);

#if defined WIN32
    int n = recvfrom(my_socket, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr*)&from_addr, &from_len);
#else
    int n = recvfrom(my_socket, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr*)&from_addr, (socklen_t*)&from_len);
#endif

    if (n <= 0) {
        printf("Errore nella ricezione dei dati.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // DESERIALIZZAZIONE MANUALE (Server -> Client)
    weather_response_t resp;
    offset = 0;

    // 1. Deserializza Status (unsigned int, Network Byte Order)
    uint32_t net_status;
    memcpy(&net_status, recv_buffer + offset, sizeof(uint32_t));
    resp.status = ntohl(net_status);
    offset += sizeof(uint32_t);

    // 2. Deserializza Type (char)
    memcpy(&resp.type, recv_buffer + offset, sizeof(char));
    offset += sizeof(char);

    // 3. Deserializza Value (float, gestito come uint32 per ntohl)
    uint32_t net_value;
    memcpy(&net_value, recv_buffer + offset, sizeof(uint32_t));
    net_value = ntohl(net_value);
    memcpy(&resp.value, &net_value, sizeof(float));

    // 3. Visualizzazione Output
    // Risoluzione inversa per ottenere il nome del server dall'IP
    char server_name[256];
    struct hostent *server_host_entry = gethostbyaddr((char*)&from_addr.sin_addr, 4, AF_INET);

    if (server_host_entry != NULL) {
        strcpy(server_name, server_host_entry->h_name);
    } else {
        // Fallback sull'IP se il nome non è risolvibile
        strcpy(server_name, inet_ntoa(from_addr.sin_addr));
    }

    // Formato richiesto: Ricevuto risultato dal server <nome> (ip <ip>).
    printf("Ricevuto risultato dal server %s (ip %s). ", server_name, inet_ntoa(from_addr.sin_addr));

    if (resp.status != STATUS_SUCCESS) {
        if (resp.status == STATUS_CITY_UNAVAILABLE) {
            printf("Città non disponibile\n");
        } else {
            printf("Richiesta non valida\n");
        }
    } else {
        // Successo: formatta in base al tipo

        if (strlen(req.city) > 0) {
            req.city[0] = toupper(req.city[0]);
        }

        switch (resp.type) {
            case 't':
                printf("%s: Temperatura = %.1f°C\n", req.city, resp.value);
                break;
            case 'h':
                printf("%s: Umidità = %.1f%%\n", req.city, resp.value);
                break;
            case 'w':
                printf("%s: Vento = %.1f km/h\n", req.city, resp.value);
                break;
            case 'p':
                printf("%s: Pressione = %.1f hPa\n", req.city, resp.value);
                break;
            default:
                printf("Tipo di risposta sconosciuto.\n");
                break;
        }
    }

	// Close socket
	closesocket(my_socket);

	clearwinsock();
	return 0;
} // main end
