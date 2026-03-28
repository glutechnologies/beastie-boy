#pragma once

#include <stddef.h>

// Declaracions per a l'escriptura de fitxers pcap
void write_pcap_packet(const char *filename, const void *data, size_t len);
