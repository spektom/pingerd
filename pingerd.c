#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "pingerd_conf.h"
#include "pingerd.h"

#define HELP "USAGE: %s [options]\n"\
		"Where options are:\n"\
		"  -h                Show this help message\n"\
		"  -c <config file>  Alternative configuration file (default: /etc/pingerd.conf)\n\n"

FILE* logfile;

int server_start() {
	if (read_config() == -1) {
		return -1;
	}

	logfile = fopen(conf.log_file, "a");
	if (logfile == NULL) {
		perror(conf.log_file);
		return -1;
	}
	getc(stdin);

	fclose(logfile);
}

int main(int argc, char** argv) {
	int c;
	opterr = 0;
	logfile = stderr;
	init_config();

	while ((c = getopt(argc, argv, "hc:")) != -1) {
		switch (c) {
			case 'c':
				conf.config_file = optarg;
				break;
			case 'h':
				printf(HELP, argv[0]);
				return 0;
			default:
				fprintf(stderr, HELP, argv[0]);
				return 1;
		}
	}
	
	server_start();
	free_config();
}

