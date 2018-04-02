#ifndef _IPRINT_H
#define _IPRINT_H

#define LOGD 1
#define LOGI 2
#define LOGW 3
#define LOGC 4


void prlog(int level, const char *fmt, ...);

#endif
