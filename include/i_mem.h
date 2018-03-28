#ifndef _I_MEM_H
#define _I_MEM_H

#include <stdio.h>

#define iallocator_init trace_allocator_init
#define imalloc trace_malloc
#define icalloc trace_calloc
#define irealloc trace_realloc
#define istrdup trace_strdup
#define ifree(p) trace_ifree((void **)&(p))

void *trace_malloc(size_t size);
void *trace_calloc(size_t size);
void *trace_realloc(void *ptr, size_t size);
char *trace_strdup(const char *s);
void trace_ifree(void **pp);
size_t iamount_of(void);
void trace_allocator_init(void);

#endif
