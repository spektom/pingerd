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
	return !_logfile ? -1 : 0;
}

void log_close() {
	if (_logfile) {
		fclose(_logfile);
		_logfile = NULL;
	}
}

void _log_printf(const char* severity, const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsnprintf(_logbuf, sizeof(_logbuf), fmt, args);
	va_end(args);

	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	strftime(_timebuf, sizeof(_timebuf), "%F %T%z", t);

	fprintf(_logfile ? _logfile : stderr, "%s [%s]:\t%s\n", _timebuf, severity, _logbuf);
	if (_logfile) {
		fflush(_logfile);
	}
}

