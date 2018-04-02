#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>

#include <i_print.h>

int loglevel = LOGD;

void prlog(int level, const char *fmt, ...)
{
	char buf[8192];
	va_list ap;

	if (level < loglevel)
		return;

	va_start(ap, fmt);
	if (fmt != NULL)
		vsnprintf(buf, sizeof(buf), fmt, ap);
	else
		buf[0] = '\0';

	const char *prefix;
	switch (level) {
	case LOGD:
		prefix = "debug";
		break;
	case LOGI:
		prefix = "info";
		break;
	case LOGW:
		prefix = "warn";
		break;
	case LOGC:
		prefix = "crit";
		break;
	default:
		prefix = "???";
		break;
	}
	(void)fprintf(stderr, "[%zu][%s] %s\n", pthread_self(), prefix, buf);
	va_end(ap);
}
