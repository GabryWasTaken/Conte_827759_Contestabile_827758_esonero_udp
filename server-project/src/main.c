/*
 * main.c
 *
 * UDP Server - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP server
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
#include <time.h>
#include <ctype.h> // Per tolower
#include "protocol.h"

#define NO_ERROR 0

// Helper per string comparison case insensitive portabile
int case_insensitive_compare(const char *s1, const char *s2) {
#if defined WIN32
    return stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

// Implementazione funzioni generazione dati
float get_temperature(void) {
    // Range: -10.0 to 40.0
    return -10.0f + ((float)rand() / RAND_MAX) * 50.0f;
}
float get_humidity(void) {
    // Range: 20.0 to 100.0
    return 20.0f + ((float)rand() / RAND_MAX) * 80.0f;
}
float get_wind(void) {
    // Range: 0.0 to 100.0
    return ((float)rand() / RAND_MAX) * 100.0f;
}
float get_pressure(void) {
    // Range: 950.0 to 1050.0
    return 950.0f + ((float)rand() / RAND_MAX) * 100.0f;
}

int main(int argc, char *argv[]) {

	// Implement server logic
    srand((unsigned int)time(NULL)); // Inizializza random seed

    int port = SERVER_PORT;
    int i;
    // Parsing argomenti server
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

#if defined WIN32
	// Initialize Winsock
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
	if (result != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		return 0;
	}
#endif

	int my_socket;
    struct sockaddr_in server_addr;

	// Create socket (UDP = SOCK_DGRAM)
	my_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (my_socket < 0) {
        printf("Error creating socket\n");
        return -1;
    }

    // Configure server address
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// Bind socket
	if (bind(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error binding socket\n");
        closesocket(my_socket);
        return -1;
    }

    printf("Server in ascolto sulla porta %d\n", port);

	// Implement communication logic

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    // Buffer
    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];

    // Lista città supportate
    const char *supported_cities[] = {
        "Bari", "Roma", "Milano", "Napoli", "Torino",
        "Palermo", "Genova", "Bologna", "Firenze", "Venezia"
    };
    int num_cities = 10;

	while (1) {
        // Ricevi datagramma
#if defined WIN32
        int recv_len = recvfrom(my_socket, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len);
#else
        int recv_len = recvfrom(my_socket, recv_buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
#endif

        if (recv_len > 0) {

            // DESERIALIZZAZIONE MANUALE (Richiesta)
            weather_request_t req;
            memset(&req, 0, sizeof(req));
            int offset = 0;

            // Leggi Type (1 byte)
            if (offset < recv_len) {
                req.type = recv_buffer[offset];
                offset += 1;
            }

            // Leggi City (copia stringa sicura)
            if (offset < recv_len) {
                strncpy(req.city, recv_buffer + offset, sizeof(req.city) - 1);
            }

            // Risoluzione DNS inversa del Client per logging
            char client_name[256];
            struct hostent *client_host = gethostbyaddr((char*)&client_addr.sin_addr, 4, AF_INET);
            if (client_host) {
                strcpy(client_name, client_host->h_name);
            } else {
                strcpy(client_name, "unknown");
            }

            printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
                   client_name, inet_ntoa(client_addr.sin_addr),
                   req.type, req.city);

            // Validazione
            int city_found = 0;
            int type_valid = 0;
            int invalid_chars = 0;

            // Validazione caratteri speciali o tabulazioni nella città
            const char *forbidden = "\t!@#$%^&*()+=[]{}\\|<>";
            if (strpbrk(req.city, forbidden) != NULL) {
                invalid_chars = 1;
            }

            // Check Type
            if (req.type == 't' || req.type == 'h' || req.type == 'w' || req.type == 'p') {
                type_valid = 1;
            }

            // Check City in list
            for (int j = 0; j < num_cities; j++) {
                if (case_insensitive_compare(req.city, supported_cities[j]) == 0) {
                    city_found = 1;
                    break;
                }
            }

            weather_response_t resp;
            memset(&resp, 0, sizeof(resp));

            if (!type_valid || invalid_chars) {
                resp.status = STATUS_INVALID_REQUEST;
                resp.type = req.type;
                resp.value = 0.0f;
            } else if (!city_found) {
                resp.status = STATUS_CITY_UNAVAILABLE;
                resp.type = req.type;
                resp.value = 0.0f;
            } else {
                resp.status = STATUS_SUCCESS;
                resp.type = req.type;

                switch(req.type) {
                    case 't': resp.value = get_temperature(); break;
                    case 'h': resp.value = get_humidity(); break;
                    case 'w': resp.value = get_wind(); break;
                    case 'p': resp.value = get_pressure(); break;
                }
            }

            // SERIALIZZAZIONE MANUALE (Risposta)
            offset = 0;

            // 1. Status (host to network)
            uint32_t net_status = htonl(resp.status);
            memcpy(send_buffer + offset, &net_status, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // 2. Type (char)
            memcpy(send_buffer + offset, &resp.type, sizeof(char));
            offset += sizeof(char);

            // 3. Value (float to network order via uint32)
            uint32_t net_val_tmp;
            memcpy(&net_val_tmp, &resp.value, sizeof(float));
            net_val_tmp = htonl(net_val_tmp);
            memcpy(send_buffer + offset, &net_val_tmp, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // Invia Risposta
            sendto(my_socket, send_buffer, offset, 0, (struct sockaddr*)&client_addr, client_len);
        }
	}

	printf("Server terminated.\n");

	closesocket(my_socket);
	clearwinsock();
	return 0;
} // main end
