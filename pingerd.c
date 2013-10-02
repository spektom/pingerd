#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include "config.h"
#include "log.h"
#include "pingerd_conf.h"
#include "pingerd.h"

#define HELP "USAGE: %s [options]\n"\
		"Where options are:\n"\
		"  -h                Show this help message\n"\
		"  -c <config file>  Alternative configuration file (default: /etc/pingerd.conf)\n\n"

struct jobs_st {
	struct gaicb** dns_reqs;
	int hosts_num;
	struct addrinfo** hosts;
};
struct jobs_st jobs = {
	.dns_reqs = NULL,
	.hosts_num = 0,
	.hosts = NULL
};

void free_jobs_dns_reqs() {
	if (jobs.dns_reqs) {
		int i;
		for (i = 0; i < jobs.hosts_num; ++i) {
			freeaddrinfo(jobs.dns_reqs[i]->ar_result);
			free(jobs.dns_reqs[i]);
		}
		free(jobs.dns_reqs);
		jobs.dns_reqs = NULL;
	}
}

void free_jobs_hosts() {
	if (jobs.hosts) {
		int i;
		for (i = 0; i < jobs.hosts_num; ++i) {
			free(jobs.hosts[i]);
		}
		free(jobs.hosts);
		jobs.hosts = NULL;
	}
}

void free_jobs() {
	free_jobs_dns_reqs();
	free_jobs_hosts();
	jobs.hosts_num = 0;
}

void resolve_hosts_handler() {
	signal(SIGUSR1, SIG_DFL);

	log_debug("DNS resolution has completed");
	free_jobs_hosts();
	jobs.hosts = calloc(jobs.hosts_num, sizeof(struct addrinfo*));
	int i;
	for (i = 0; i < jobs.hosts_num; ++i) {
		if (jobs.dns_reqs[i]->ar_result == NULL) {
			log_error("Unable to resolve host: %s", jobs.dns_reqs[i]->ar_name);
			jobs.hosts[i] = NULL;
		} else {
			jobs.hosts[i] = calloc(1, sizeof(struct addrinfo));
			memcpy(jobs.hosts[i], jobs.dns_reqs[i]->ar_result, sizeof(struct addrinfo));
		}
	}
	free_jobs_dns_reqs();
}

int resolve_hosts() {
	int i;
	// Other DNS resolutions is in process:
	if (jobs.dns_reqs) {
		signal(SIGUSR1, SIG_DFL);
		for (i = 0; i < conf.hosts_num; ++i) {
			gai_cancel(jobs.dns_reqs[i]);
		}
		free_jobs_dns_reqs();
	}

	jobs.dns_reqs = calloc(conf.hosts_num, sizeof(struct gaicb*));
	for (i = 0; i < conf.hosts_num; ++i) {
		jobs.dns_reqs[i] = calloc(1, sizeof(struct gaicb));
		jobs.dns_reqs[i]->ar_name = conf.hosts[i];
	}
	jobs.hosts_num = conf.hosts_num;

	struct sigevent sigev;
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIGUSR1;
	signal(SIGUSR1, resolve_hosts_handler);

	int ret;
	if ((ret = getaddrinfo_a(GAI_NOWAIT, jobs.dns_reqs, conf.hosts_num, &sigev)) != 0) {
		log_error("getaddrinfo_a() failed: %s", gai_strerror(ret));
		return -1;
	}
	
	log_debug("Starting DNS resolution");
	return 0;
}

void reload() {
	signal(SIGHUP, reload);
	if (reload_config() != -1) {
		resolve_hosts();
	}
}

int server_start() {
	if (read_config() == -1) {
		return -1;
	}
	if (log_open(conf.log_file) == -1) {
		perror(conf.log_file);
		return -1;
	}

	log_debug("Starting pingerd daemon");
	resolve_hosts();

	// Re-load pingerd when SIGHUP is received:
	signal(SIGHUP, reload);

	getc(stdin);

	log_close();
	free_config();
	free_jobs();

	return 0;
}

int main(int argc, char** argv) {
	int c;
	opterr = 0;
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
	
	return server_start();
}

