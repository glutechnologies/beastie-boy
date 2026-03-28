#include "pcap_writer.h"
#include <stdio.h>
#include <pcap/pcap.h>
#include <sys/time.h>

void write_pcap_packet(const char *filename, const void *data, size_t len) {
	static pcap_t *pcap = NULL;
	static pcap_dumper_t *dumper = NULL;
	static int initialized = 0;
	struct pcap_pkthdr header;

	if (!initialized) {
		pcap = pcap_open_dead(DLT_EN10MB, 65535);
		dumper = pcap_dump_open(pcap, filename);
		initialized = 1;
	}
	header.caplen = len;
	header.len = len;
	gettimeofday(&header.ts, NULL);
	pcap_dump((unsigned char *)dumper, &header, data);
	pcap_dump_flush(dumper);
}
