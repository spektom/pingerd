#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include "log.h"

FILE* _logfile = NULL;
char _logbuf[1024];
char _timebuf[64];

int log_open(const char* filename) {
	log_close();
	_logfile = fopen(filename, "a");
	return _logfile ? 0 : -1;
}

void log_close() {
	if (_logfile) {
		fclose(_logfile);
		_logfile = NULL;
	}
}

void _log_printf(const char* severity, const char * fmt, va_list args) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	strftime(_timebuf, sizeof(_timebuf), "%F %T%z", t);

	vsnprintf(_logbuf, sizeof(_logbuf), fmt, args);
	va_end(args);

	fprintf(_logfile ? _logfile : stderr, "%s [%s]:\t%s\n", _timebuf, severity, _logbuf);
	if (_logfile) {
		fflush(_logfile);
	}
}

void log_error(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	_log_printf("Error", fmt, args);
}

void log_warn(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	_log_printf("Warning", fmt, args);
}

void log_info(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	_log_printf("Info", fmt, args);
}

void log_debug(const char* fmt, ...) {
#ifdef DEBUG
	va_list args;
	va_start(args, fmt);
	_log_printf("Debug", fmt, args);
#endif
}

