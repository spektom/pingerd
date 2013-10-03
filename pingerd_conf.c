#include <libconfig.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "log.h"
#include "pingerd.h"
#include "pingerd_conf.h"

struct pingerd_conf conf;

void init_config() {
	conf.config_file = "/etc/pingerd.conf";
	conf.num_connections = 100;
	conf.report_freq = 900;
	conf.reports_dir = strdup("/var/log/pingerd/");
	conf.hosts_file = strdup("/etc/pingerd/ip.txt");
	conf.log_file = strdup("/var/log/pingerd.conf");
	conf.hosts = NULL;
	conf.hosts_num = 0;
}

void free_config_hosts() {
	if (conf.hosts) {
		int i;
		for (i = 0; i < conf.hosts_num; ++i) {
			free(conf.hosts[i]);
		}
		free(conf.hosts);
		conf.hosts = NULL;
	}
	conf.hosts_num = 0;
}

void free_config() {
	free_config_hosts();
	free(conf.reports_dir);
	free(conf.hosts_file);
	free(conf.log_file);
}

/**
 * Read hosts database file
 */
int read_hosts() {
	FILE* fp = fopen(conf.hosts_file, "r");
	if (fp == NULL) {
		log_error("%s: %s", conf.hosts_file, strerror(errno));
		return -1;
	}

	free_config_hosts();

	char buf[250];
	char* tok;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		tok = strtok(buf, "\r\n");
		if (strlen (tok) > 0) {
			conf.hosts = (char **) realloc (conf.hosts, sizeof(char *) * (conf.hosts_num+1));
			conf.hosts[conf.hosts_num] = strdup(tok);
			++conf.hosts_num;
		}
	}

	fclose(fp);
	return 0;
}

int reload_config() {
	log_info("Reloading hosts from: %s", conf.hosts_file);
	return read_hosts();
}

/**
 * Re-reads configuration from the given file
 */
int read_config() {
	config_t cfg;
	config_init(&cfg);

	if (config_read_file(&cfg, conf.config_file) == CONFIG_FALSE) {
		if (config_error_type(&cfg) == CONFIG_ERR_FILE_IO) {
			log_error("Configuration file not found: %s", conf.config_file);
			config_destroy(&cfg);
			return -1;
		}
		log_error("%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	int intVal;
	const char* charVal;
	if (config_lookup_int(&cfg, "num_connections", &intVal) == CONFIG_TRUE) {
		conf.num_connections = intVal;
	}
	if (config_lookup_int(&cfg, "report_freq", &intVal) == CONFIG_TRUE) {
		conf.report_freq = intVal;
	}
	if (config_lookup_string(&cfg, "reports_dir", &charVal) == CONFIG_TRUE) {
		free(conf.reports_dir);
		conf.reports_dir = strdup(charVal);
	}
	if (config_lookup_string(&cfg, "hosts_file", &charVal) == CONFIG_TRUE) {
		free(conf.hosts_file);
		conf.hosts_file = strdup(charVal);
	}
	if (config_lookup_string(&cfg, "log_file", &charVal) == CONFIG_TRUE) {
		free(conf.log_file);
		conf.log_file = strdup(charVal);
	}
	config_destroy(&cfg);

	if (read_hosts() == -1) {
		return -1;
	}

	return 0;
}
