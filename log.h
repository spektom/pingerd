#ifndef LOG_H
#define LOG_H

extern int log_open(const char* filename);
extern void log_close();

#define MAKESTR(x) #x

#define log_error(fmt,...) _log_printf("Error", fmt, ## __VA_ARGS__)
#define log_warn(fmt,...) _log_printf("Warning", fmt, ## __VA_ARGS__)
#define log_info(fmt,...) _log_printf("Info", fmt, ## __VA_ARGS__)
#define log_fatal(fmt,...) do { \
	_log_printf("Fatal", __FILE__ ":" MAKESTR(__LINE__) fmt, ## __VA_ARGS__); \
	exit(-1); \
} while(0);

#ifdef DEBUG
# define log_debug(fmt,...) _log_printf("Debug", fmt, ## __VA_ARGS__)
#else
# define log_debug(fmt,...) (void)0
#endif

#endif /* LOG_H */
