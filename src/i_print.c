#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <i_print.h>

int loglevel = LOGD;

void prlog(int level, const char *fmt, ...)
{
	char buf[8192];
	va_list ap;
	time_t ltime; /* calendar time */
	struct tm local_tm;

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

	ltime = time(NULL);
	memset(&local_tm, 0, sizeof(local_tm));
	struct tm *t = localtime_r(&ltime, &local_tm);
	char time_fmt[64] = { 0 };
	strftime(time_fmt, sizeof(time_fmt), "%Y-%m-%dT%H:%M:%S", t);
	(void)fprintf(stderr, "[%zu][%s][%s] %s\n", pthread_self(), time_fmt,
				  prefix, buf);
	va_end(ap);
}
