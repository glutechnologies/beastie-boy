#include "boy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple execució de comandes vppctl per configurar memif i SPAN
void run_cmd(const char *cmd) {
	printf("Exec: %s\n", cmd);
	int ret = system(cmd);
	if (ret != 0) {
		fprintf(stderr, "Error executant: %s\n", cmd);
	}
}

int boy_run(void) {
	// Paràmetres d'exemple
	int memif_id = 0;
	const char *if_phys = "xe0";
	char cmd[256];

	// Crea la memif
	snprintf(cmd, sizeof(cmd), "vppctl create interface memif id %d master", memif_id);
	run_cmd(cmd);
	snprintf(cmd, sizeof(cmd), "vppctl set interface state memif%d up", memif_id);
	run_cmd(cmd);
	// Configura SPAN/mirror
	snprintf(cmd, sizeof(cmd), "vppctl span add interface %s destination memif%d", if_phys, memif_id);
	run_cmd(cmd);
	printf("Configuració completada.\n");
	return 0;
}
