#include "pcap_writer.h"

#include <stdio.h>
#include <pcap/pcap.h>
#include <sys/time.h>

static pcap_t *pcap = NULL;
static pcap_dumper_t *dumper = NULL;
static int initialized = 0;

int write_pcap_packet(const char *filename, const void *data, size_t len) {
	struct pcap_pkthdr header;

	if (!initialized) {
		pcap = pcap_open_dead(DLT_EN10MB, 65535);
		if (pcap == NULL) {
			fprintf(stderr, "pcap_open_dead failed\n");
			return -1;
		}
		dumper = pcap_dump_open(pcap, filename);
		if (dumper == NULL) {
			fprintf(stderr, "pcap_dump_open(%s): %s\n", filename, pcap_geterr(pcap));
			pcap_close(pcap);
			pcap = NULL;
			return -1;
		}
		initialized = 1;
	}
	header.caplen = len;
	header.len = len;
	gettimeofday(&header.ts, NULL);
	pcap_dump((unsigned char *)dumper, &header, data);
	return 0;
}

int flush_pcap_writer(void)
{
	if (!initialized || dumper == NULL) {
		return 0;
	}

	return pcap_dump_flush(dumper);
}

void close_pcap_writer(void)
{
	if (dumper != NULL) {
		pcap_dump_flush(dumper);
		pcap_dump_close(dumper);
		dumper = NULL;
	}

	if (pcap != NULL) {
		pcap_close(pcap);
		pcap = NULL;
	}

	initialized = 0;
}
