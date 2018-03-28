#ifndef _I_MACRO_H
#define _I_MACRO_H

#define I_COUNT_OF(x) \
	((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#endif
