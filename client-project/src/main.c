/*
 * client.c
 *
 * UDP Client - Esonero Reti di Calcolatori
 * Migrazione da TCP a UDP
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

// Funzione di gestione errori
void errorhandler(char *errorMessage) {
    printf("%s\n", errorMessage);
}

// Cleanup Winsock
void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}
// ====================== VALIDAZIONE ======================

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
// ====================== SERIALIZZAZIONE ======================

// Serializza richiesta client → buffer
int serialize_request(weather_request_t *req, char *buffer) {
    int offset = 0;
    memcpy(buffer + offset, &req->type, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, req->city, sizeof(req->city));
    offset += sizeof(req->city);
    return offset;
}

// Deserializza risposta server ← buffer
int deserialize_response(weather_response_t *res, char *buffer) {
    int offset = 0;

    // status
    uint32_t net_status;
    memcpy(&net_status, buffer + offset, sizeof(uint32_t));
    res->status = ntohl(net_status);
    offset += sizeof(uint32_t);

    // type
    memcpy(&res->type, buffer + offset, sizeof(char));
    offset += sizeof(char);

    // value (float con network byte order)
    uint32_t temp;
    memcpy(&temp, buffer + offset, sizeof(uint32_t));
    temp = ntohl(temp);
    memcpy(&res->value, &temp, sizeof(float));
    offset += sizeof(uint32_t);

    return offset;
}

// ====================== MAIN CLIENT ======================

 int main(int argc, char *argv[]) {
#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != 0) {
        printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    int port = SERVER_PORT;
    char ip[32] = DEFAULT_IP;
    char request_string[128] = "";
    int richiesta = 0;

    weather_request_t request;
    memset(&request, 0, sizeof(request));

    // Parsing argomenti
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            strncpy(ip, argv[++i], 31);
        } else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            strncpy(request_string, argv[++i], 127);
            richiesta = 1;
        }
    }

    if (!richiesta) {
        printf("Uso: %s [-s server] [-p port] -r \"tipo città\"\n", argv[0]);
        clearwinsock();
        return -1;
    }

    // Parsing richiesta
    int i = 0;
    while (request_string[i] == ' ' && request_string[i] != '\0') i++;
    request.type = request_string[i];
    i++;

    //controllo che il primo token sia un carattere singolo
    if (request_string[i] != ' ' && request_string[i] != '\0') {
    	printf("Richiesta non valida \n");
    	clearwinsock();
    	return -1;
    }

    //salta eventuali spazi dopo il tipo
    while (request_string[i] == ' ' && request_string[i] != '\0') i++;
    strncpy(request.city, &request_string[i], 63);
    request.city[63] = '\0';

    // Validazione lunghezza città
    if (strlen(request.city) > 63) {
        printf("Errore: nome città troppo lungo (max 63 caratteri).\n");
        clearwinsock();
        return -1;
    }


    if (!is_valid_city(request.city)) {
        printf("Richiesta non valida\n");
        clearwinsock();
        return -1;
    }


    //Validazione tipo

    if (request.type != TYPE_TEMPERATURE &&
    		request.type != TYPE_HUMIDITY &&
			request.type != TYPE_WIND &&
			request.type != TYPE_PRESSURE) {
    	printf("Richiesta non valida\n");
    	clearwinsock ();
    	return -1;
    }
    // Creazione socket UDP
    int c_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c_socket < 0) {
        errorhandler("Creazione del socket UDP fallita.\n");
        clearwinsock();
        return -1;
    }

    // Configurazione indirizzo server
    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_addr.s_addr = inet_addr(ip);
    sad.sin_port = htons(port);

    // Serializzazione richiesta
    char buffer[BUFFER_SIZE];
    int req_len = serialize_request(&request, buffer);

    // Invio richiesta
    if (sendto(c_socket, buffer, req_len, 0, (struct sockaddr*)&sad, sizeof(sad)) != req_len) {
        errorhandler("sendto() ha inviato un numero di byte diverso dall'atteso");
        closesocket(c_socket);
        clearwinsock();
        return -1;
    }

    // Ricezione risposta
    struct sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    int bytesRecv = recvfrom(c_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&fromAddr, &fromLen);
    if (bytesRecv <= 0) {
        errorhandler("recvfrom() fallita");
        closesocket(c_socket);
        clearwinsock();
        return -1;
    }

    // Deserializzazione risposta

    weather_response_t response;
    deserialize_response(&response, buffer);

    // Risoluzione DNS per output
    char host[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&fromAddr, sizeof(fromAddr), host, sizeof(host), NULL, 0, NI_NAMEREQD) != 0) {
        strcpy(host, ip); // fallback
    }

    // Stampa risultato
    printf("Ricevuto risultato dal server %s (ip %s). ", host, ip);
    if (response.status == STATUS_SUCCESS) {
        request.city[0] = toupper(request.city[0]);
        switch (response.type) {
            case TYPE_TEMPERATURE:
                printf("%s: Temperatura = %.1f°C\n", request.city, response.value);
                break;
            case TYPE_HUMIDITY:
                printf("%s: Umidità = %.1f%%\n", request.city, response.value);
                break;
            case TYPE_WIND:
                printf("%s: Vento = %.1f km/h\n", request.city, response.value);
                break;
            case TYPE_PRESSURE:
                printf("%s: Pressione = %.1f hPa\n", request.city, response.value);
                break;
            default:
                printf("Tipo sconosciuto ricevuto.\n");
        }
    } else if (response.status == STATUS_CITY_UNAVAILABLE) {
        printf("Città non disponibile\n");
    } else if (response.status == STATUS_INVALID_REQUEST) {
        printf("Richiesta non valida\n");
    } else {
        printf("Errore sconosciuto\n");
    }

    // Chiusura socket
    closesocket(c_socket);
    clearwinsock();
    return 0;
}
