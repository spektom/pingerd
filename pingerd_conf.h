#ifndef PINGERD_CONF_H
#define PINGERD_CONF_H

/**
 * Pinger daemon configuration
 */
struct pingerd_conf_s {
	char* config_file;
	int num_connections;
	int report_freq;
	int packet_size;
	int packets_count;
	char* reports_dir;
	char* hosts_file;
	char* log_file;
	char** hosts;
	int hosts_num;
};

extern struct pingerd_conf_s conf;

void init_config();
int read_config();
int reload_config();
void free_config();

#endif /* PINGERD_CONF_H */
