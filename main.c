#include <stdio.h>

#include <i_mem.h>

int main(int argc, char *argv[])
{
	iallocator_init();

	void *t1 = imalloc(100);
	void *t2 = imalloc(21);

	ifree(t2);

	t1 = irealloc(t1, 101);
	t1 = irealloc(t1, 99);
	ifree(t1);

	printf("%zu\n", iamount_of());

	return 1;
}
