#ifndef _LOGFILE_H_
#define _LOGFILE_H_

extern int log_open(const char* filename);
extern void log_close();

extern void log_error(const char* fmt, ...);
extern void log_warn(const char* fmt, ...);
extern void log_info(const char* fmt, ...);
extern void log_debug(const char* fmt, ...);

#endif /* _LOGFILE_H_ */
