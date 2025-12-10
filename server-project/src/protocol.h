/*
 * protocol.h
 *
 * Shared header file for UDP client and server
 * Contains protocol definitions, data structures, constants and function prototypes
 */
#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

// Costanti condivise
#define SERVER_PORT 56700
#define BUFFER_SIZE 512
#define DEFAULT_IP "127.0.0.1"

// Tipi di dati meteo
#define TYPE_TEMPERATURE 't'
#define TYPE_HUMIDITY    'h'
#define TYPE_WIND        'w'
#define TYPE_PRESSURE    'p'

// Codici di stato
#define STATUS_SUCCESS           0
#define STATUS_CITY_UNAVAILABLE  1
#define STATUS_INVALID_REQUEST   2

// Struttura richiesta client → server
typedef struct {
    char type;
    char city[64];
} weather_request_t;

// Struttura risposta server → client
typedef struct {
    unsigned int status;
    char type;
    float value;
} weather_response_t;

// Prototipi di funzioni comuni
void errorhandler(char *errorMessage);
void clearwinsock(void);

// Prototipi funzioni di generazione dati
float get_temperature(void);
float get_humidity(void);
float get_wind(void);
float get_pressure(void);

// Prototipi funzioni di serializzazione
int serialize_request(weather_request_t *req, char *buffer);
int deserialize_request(weather_request_t *req, char *buffer);
int serialize_response(weather_response_t *res, char *buffer);
int deserialize_response(weather_response_t *res, char *buffer);

#endif /* PROTOCOL_H_ */
