#ifndef _PINGERD_CONF_H_
#define _PINGERD_CONF_H_

/**
 * Pinger daemon configuration
 */
struct pingerd_conf {
	char* config_file;
	int num_connections;
	int report_freq;
	char* reports_dir;
	char* hosts_file;
	char* log_file;
	char** hosts;
	int hosts_num;
};

extern struct pingerd_conf conf;

int init_config();
int read_config();
int free_config();

#endif /* _PINGERD_CONF_H_ */
