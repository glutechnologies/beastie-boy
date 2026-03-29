#pragma once

#include <stddef.h>

// Declaracions per a l'escriptura de fitxers pcap
int write_pcap_packet(const char *filename, const void *data, size_t len);
int flush_pcap_writer(void);
void close_pcap_writer(void);
