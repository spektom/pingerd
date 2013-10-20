#define _GNU_SOURCE
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "log.h"
#include "pingerd_conf.h"

#define HELP "USAGE: %s [options]\n"\
		"Where options are:\n"\
		"  -h                Show this help message\n"\
		"  -c <config file>  Alternative configuration file (default: /etc/pingerd.conf)\n\n"

#define	DEFDATALEN	(64 - 8)	/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(65536 - 60 - 8)/* max packet size */
#define	MAXWAIT		10		/* max seconds to wait for response */
#define	NROUTES		9		/* number of record route slots */

struct job_s {
	char* hostname;
	struct gaicb* dns_req;
	struct addrinfo* addr_info;
	int socket;
	u_char* packet;
	u_int packet_len;
	time_t ping_start;
	int n_transmitted;
	int n_received;
};

/* Pinger jobs */
struct job_s** jobs = NULL;
int jobs_num = 0;

/* Output packet buffer */
u_char outpack[MAXPACKET];

/* Process id to identify our packets */
int ident; 

/* Current DNS resolution requests in process */
struct gaicb** dns_reqs = NULL;

void free_jobs() {
	if (jobs) {
		int i;
		for (i = 0; i < jobs_num; ++i) {
			struct job_s* job = jobs[i];
			if (job->dns_req) {
				freeaddrinfo(job->dns_req->ar_result);
				free(job->dns_req);
			}
			if (job->addr_info) {
				free(job->addr_info);
			}
			if (job->packet) {
				free(job->packet);
			}
			free(job);
		}
		free(jobs);
		jobs = NULL;
		jobs_num = 0;
	}
}

/**
 * Initialize new jobs queue from configuration
 */
void init_jobs() {
	free_jobs();
	jobs_num = conf.hosts_num;
	if (!(jobs = calloc(jobs_num, sizeof(struct job_s*)))) {
		log_fatal("can't allocate memory");
	}
	int i;
	for (i = 0; i < jobs_num; ++i) {
		struct job_s* job = calloc(1, sizeof(struct job_s));
		if (!job) {
			log_fatal("can't allocate memory");
		}
		job->hostname = conf.hosts[i];
		jobs[i] = job;
	}
}

/**
 * Async DNS resolution callback
 */
void resolve_hosts_handler() {
	log_debug("DNS resolution has completed");

	int i;
	for (i = 0; i < jobs_num; ++i) {
		struct job_s* job = jobs[i];
		if (!job->dns_req->ar_result) {
			log_warn("Unable to resolve host: %s", job->hostname);
		} else {
			job->addr_info = calloc(1, sizeof(struct addrinfo));
			if (!job->addr_info) {
				log_fatal("can't allocate memory");
			}
			memcpy(job->addr_info, job->dns_req->ar_result, sizeof(struct addrinfo));
			freeaddrinfo(job->dns_req->ar_result);
		}
		free(job->dns_req);
		job->dns_req = NULL;
	}
	free(dns_reqs);
	dns_reqs = NULL;

	log_debug("Scheduling pinger after %d minutes", conf.report_freq);
	alarm(conf.report_freq);
}

/**
 * Resolve hosts asynchronously
 */
int resolve_hosts() {
	int i;
	if (dns_reqs) {
		// Other DNS resolution is in process
		for (i = 0; i < jobs_num; ++i) {
			gai_cancel(dns_reqs[i]);
		}
		free(dns_reqs);
	}

	init_jobs();
	if (!(dns_reqs = calloc(jobs_num, sizeof(struct gaicb*)))) {
		log_fatal("can't allocate memory");
	}

	for (i = 0; i < jobs_num; ++i) {
		struct job_s* job = jobs[i];
		if (!(job->dns_req = calloc(1, sizeof(struct gaicb)))) {
			log_fatal("can't allocate memory");
		}
		job->dns_req->ar_name = job->hostname;
		dns_reqs[i] = job->dns_req;
	}

	struct sigevent sigev;
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIGRTMIN;

	struct sigaction sact;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = SA_RESETHAND;
    sact.sa_handler = resolve_hosts_handler;
    sigaction(SIGRTMIN, &sact, NULL);
	
	log_debug("Starting DNS resolution");

	int ret;
	if ((ret = getaddrinfo_a(GAI_NOWAIT, dns_reqs, jobs_num, &sigev)) != 0) {
		log_error("getaddrinfo_a(): %s", gai_strerror(ret));
		return -1;
	}
	return 0;
}

void init_packet(struct job_s* job) {
	job->packet_len = conf.packet_size + MAXIPLEN + MAXICMPLEN;
    if (!(job->packet = (u_char*) malloc(job->packet_len))) {
		log_fatal("can't allocate memory");
    }
}

/**
 * Checksum routine for Internet Protocol family headers (C Version)
 */
int in_cksum(u_short* addr, int len) {
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}

void send_packet(struct job_s* job) {
	register struct icmp *icp;

	icp = (struct icmp *)outpack;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = job->n_transmitted++;
	icp->icmp_id = ident;

	gettimeofday((struct timeval *)&outpack[8], (struct timezone *)NULL);
	register int cc = conf.packet_size + 8;			/* skips ICMP portion */

	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	int i = sendto(job->socket, (char *)outpack, cc, 0, job->addr_info->ai_addr, job->addr_info->ai_addrlen);

	if (i < 0 || i != cc)  {
		if (i < 0) {
			log_error("Can't transmit packet to %s: %s", job->hostname, strerror(errno));
		}
		log_warn("%s: wrote %d chars, ret=%d\n", job->hostname, cc, i);
	}
}

void pinger() {
	log_debug("Re-scheduling pinger after %d minutes", conf.report_freq);
	alarm(conf.report_freq);
}

void server_reload() {
	log_debug("Deleting pinger schedule");
	alarm(0);

	if (reload_config() != -1) {
		resolve_hosts();
	}
}

void server_stop(int signum) {
	log_info("%s - closing pingerd", strsignal(signum));
	log_close();
	free_config();
	free_jobs();
	exit(0);
}

void setup_signal_handlers() {
	// Reload pingerd when SIGHUP is received:
	struct sigaction reload_sact;
    sigemptyset(&reload_sact.sa_mask);
    reload_sact.sa_flags = 0;
    reload_sact.sa_handler = server_reload;
    sigaction(SIGHUP, &reload_sact, NULL);

	// Setup clean exit handlers:
	struct sigaction stop_sact;
    sigemptyset(&stop_sact.sa_mask);
    stop_sact.sa_flags = 0;
    stop_sact.sa_handler = server_stop;
    sigaction(SIGINT, &stop_sact, NULL);
    sigaction(SIGTERM, &stop_sact, NULL);

	// Pinger timer
	struct sigaction ping_sact;
    sigemptyset(&ping_sact.sa_mask);
    ping_sact.sa_flags = 0;
    ping_sact.sa_handler = pinger;
    sigaction(SIGALRM, &ping_sact, NULL);
}

int server_start() {
	if (read_config() == -1) {
		return -1;
	}
	if (log_open(conf.log_file) == -1) {
		perror(conf.log_file);
		return -1;
	}

	log_info("Starting pingerd daemon");
	resolve_hosts();

	setup_signal_handlers();

	while(1) {}
	return 0;
}

int main(int argc, char** argv) {
	int c;
	opterr = 0;
	ident = getpid() & 0xFFFF;
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

