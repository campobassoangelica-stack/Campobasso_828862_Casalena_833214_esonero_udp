/*
 * server.c
 *
 * UDP Server - Esonero Reti di Calcolatori
 * Migrazione da TCP a UDP
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#if defined WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define closesocket close
#endif

#include "protocol.h"

// Lista città supportate
static const char *SUPPORTED_CITIES[] = {
    "Bari", "Roma", "Milano", "Napoli", "Torino",
    "Palermo", "Genova", "Bologna", "Firenze", "Venezia"
};

// Funzioni di utilità
void errorhandler(char *errorMessage) { printf("%s\n", errorMessage); }
void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

// Generazione random float
float random_float(float min, float max) {
    float scale = rand() / (float) RAND_MAX;
    return min + scale * (max - min);
}

// Funzioni meteo
float get_temperature(void) { return random_float(-10.0, 40.0); }
float get_humidity(void)    { return random_float(20.0, 100.0); }
float get_wind(void)        { return random_float(0.0, 100.0); }
float get_pressure(void)    { return random_float(950.0, 1050.0); }

//validazione città
int is_valid_city(const char *city) {
    if (city == NULL || city[0] == '\0') return 0;
    for (int j = 0; city[j] != '\0'; j++) {
        unsigned char uc = (unsigned char) city[j];
        if (!isalpha(uc) && uc != ' ') {
            return 0; // carattere non valido
        }
    }
    return 1;
}
// Validazione richiesta
void valida(weather_request_t *req, weather_response_t *resp) {
	//controllo tipo
    if(req->type != TYPE_TEMPERATURE && req->type != TYPE_HUMIDITY &&
       req->type != TYPE_WIND && req->type != TYPE_PRESSURE) {
        resp->status = STATUS_INVALID_REQUEST;
        return;
    }

    //controllo città vuota
        if (req->city[0] == '\0') {
            resp->status = STATUS_INVALID_REQUEST;
            return;
        }
//controllo caratteri non validi
    if (!is_valid_city(req->city)) {
            resp->status = STATUS_INVALID_REQUEST;
            return;
        }


    //controllo città supportata
    int flag = 1;
    for (int i = 0; i < 10; i++) {
        if(strcasecmp(req->city, SUPPORTED_CITIES[i]) == 0) {
            flag = 0;
            break;
        }
    }

    if(flag == 1) {
        resp->status = STATUS_CITY_UNAVAILABLE;
    } else {
        resp->status = STATUS_SUCCESS;
    }
}

// Serializzazione risposta → buffer
int serialize_response(weather_response_t *res, char *buffer) {
    int offset = 0;
//Serializzazione status
    uint32_t net_status = htonl(res->status);
    memcpy(buffer + offset, &net_status, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    //serializzazione type
    memcpy(buffer + offset, &res->type, sizeof(char));
    offset += sizeof(char);

    //Conversione float con union
    union { float f; uint32_t i; } conv;
    conv.f = res->value;
    uint32_t temp = htonl(conv.i);
    memcpy (buffer + offset, &temp, sizeof(uint32_t));
    offset += sizeof(uint32_t); //aggiorna offset
    return offset;
}


// Deserializzazione richiesta ← buffer
int deserialize_request(weather_request_t *req, char *buffer) {
    int offset = 0;
    memcpy(&req->type, buffer + offset, sizeof(char));
    offset += sizeof(char);
    memcpy(req->city, buffer + offset, sizeof(req->city));
    req->city[63] = '\0';
    offset += sizeof(req->city);
    return offset;
}

int main(int argc, char *argv[]) {
#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != 0) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    int port = SERVER_PORT;
    if (argc > 2 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    // Creazione socket UDP
    int my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket < 0) {
        errorhandler("socket creation failed.\n");
        clearwinsock();
        return -1;
    }

    // Configurazione indirizzo server
    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_addr.s_addr = htonl(INADDR_ANY);
    sad.sin_port = htons(port);

    // Binding
    if (bind(my_socket, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
        errorhandler("bind() failed.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    srand(time(NULL));
    printf("Server UDP in ascolto sulla porta %d...\n", port);

    // Loop infinito
    while (1) {
        struct sockaddr_in cad;
        int client_len = sizeof(cad);
        char buffer[BUFFER_SIZE];

        // Ricezione richiesta
        int bytesRecv = recvfrom(my_socket, buffer, BUFFER_SIZE, 0,
                                 (struct sockaddr*)&cad, &client_len);
        if (bytesRecv <= 0) {
            errorhandler("recvfrom() failed.\n");
            continue;
        }

        weather_request_t request;
        deserialize_request(&request, buffer);

        // Risoluzione DNS per log
        char host[NI_MAXHOST];
        if (getnameinfo((struct sockaddr*)&cad, client_len, host, sizeof(host),
                        NULL, 0, NI_NAMEREQD) != 0) {
            strcpy(host, inet_ntoa(cad.sin_addr));
        }

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               host, inet_ntoa(cad.sin_addr), request.type, request.city);

        // Prepara risposta
        weather_response_t response;
        valida(&request, &response);

        if(response.status == STATUS_SUCCESS) {
            switch (request.type) {
                case TYPE_TEMPERATURE: response.value = get_temperature(); break;
                case TYPE_HUMIDITY:    response.value = get_humidity(); break;
                case TYPE_WIND:        response.value = get_wind(); break;
                case TYPE_PRESSURE:    response.value = get_pressure(); break;
            }
            response.type = request.type;
        } else {
            response.type = '\0';
            response.value = 0.0;
        }

        // Serializza e invia risposta
        int resp_len = serialize_response(&response, buffer);
        if (sendto(my_socket, buffer, resp_len, 0,
                   (struct sockaddr*)&cad, client_len) != resp_len) {
            errorhandler("sendto() failed.\n");
        }
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
