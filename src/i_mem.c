#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <i_atomic.h>

#define ALIGNMENT0 sizeof(union alignment0)
#define OUTPTR0(ptr) (((char*)ptr) + ALIGNMENT0)
#define INPTR0(ptr) (((char*)ptr) - ALIGNMENT0)

I_ATOMIC_DECLARE(size_t, amountof);

union alignment0 {
	size_t size;
	void *ptr;
	double dbl;
};

size_t iamount_of(void)
{
	return I_ATOMIC_GET(amountof);
}

void *trace_malloc(size_t size)
{
	void *chunk = malloc(size + ALIGNMENT0);

	if (!chunk)
		return chunk;
	I_ATOMIC_ADD(amountof, size);
	*(size_t*)chunk = size;
	return OUTPTR0(chunk);
}

void *trace_calloc(size_t size)
{
	void *chunk = calloc(1, size + ALIGNMENT0);

	if (!chunk)
		return chunk;
	I_ATOMIC_ADD(amountof, size);
	*(size_t*)chunk = size;
	return OUTPTR0(chunk);
}

void *trace_realloc(void *ptr, size_t size)
{
	size_t old_size = 0;

	if (ptr) {
		ptr = INPTR0(ptr);
		old_size = *(size_t*)ptr;
	}
	ptr = realloc(ptr, size + ALIGNMENT0);
	if (!ptr)
		return NULL;
	*(size_t*)ptr = size;

	I_ATOMIC_SUB(amountof, old_size);
	I_ATOMIC_ADD(amountof, size);
	return OUTPTR0(ptr);
}

char *trace_strdup(const char *s)
{
	int l = strlen(s) + 1;
	char *d = (char *)trace_calloc(l);

	memcpy(d, s, l);
	return d;
}

void trace_ifree(void **pp)
{
	if (pp != NULL && *pp != NULL) {
		void *ptr = *pp;

		ptr = INPTR0(ptr);
		I_ATOMIC_SUB(amountof, *(size_t*)ptr);

		free(ptr);
		*pp = NULL;
	}
}

void trace_allocator_init(void)
{
	I_ATOMIC_INIT(amountof);
}
