#ifndef _I_ATOMIC_H
#define _I_ATOMIC_H

#include <pthread.h>

#define __spinlock						pthread_spinlock_t
#define __spin_lock(spin)				pthread_spin_lock(spin)
#define __spin_trylock(spin)			pthread_spin_trylock(spin)
#define __spin_unlock(spin)				pthread_spin_unlock(spin)
#define __spin_init(spin, spin_attr)	pthread_spin_init(spin, spin_attr)
#define __spin_destroy(spin)			pthread_spin_destroy(spin)

#define I_ATOMIC_DECLARE(type, name) \
	type name ## __atomic__; \
	__spinlock name ## __lock__

#define I_ATOMIC_EXTERN(type, name) \
	extern type name ## __atomic__; \
	extern __spinlock name ## __lock__

#define I_ATOMIC_DECL_AND_INIT(type, name) \
	type name ## __atomic__ = 0; \
	__spinlock name ## __lock__; \
	__spin_init(&(name ## __lock__), 0)

#define I_ATOMIC_INIT(name) \
	do { \
		__spin_init(&(name ## __lock__), 0); \
		(name ## __atomic__) = 0; \
	} while(0)

#define I_ATOMIC_RESET(name) \
	do { \
		(name ## __atomic__) = 0; \
	} while(0)

#define I_ATOMIC_DESTROY(name) \
	do { \
		__spin_destroy(&(name ## __lock__)); \
	} while (0)

#define I_ATOMIC_ADD(name, val) \
	({\
	 typeof(name ## __atomic__) var; \
	 do { \
	 __spin_lock(&(name ## __lock__)); \
	 (name ## __atomic__) += (val); \
	 var = (name ## __atomic__); \
	 __spin_unlock(&(name ## __lock__)); \
	 } while(0); \
	 var ; \
	 })

#define I_ATOMIC_SUB(name, val) \
	({ \
	 typeof(name ## __atomic__) var; \
	 do { \
	 __spin_lock(&(name ## __lock__)); \
	 (name ## __atomic__) -= (val); \
	 var = (name ## __atomic__); \
	 __spin_unlock(&(name ## __lock__)); \
	 } while(0); \
	 var ; \
	 })

#define I_ATOMIC_AND(name, val) \
	do { \
		__spin_lock(&(name ## __lock__)); \
		(name ## __atomic__) &= (val); \
		__spin_unlock(&(name ## __lock__)); \
	} while(0)

#define I_ATOMIC_OR(name, val) \
	do { \
		__spin_lock(&(name ## __lock__)); \
		(name ## __atomic__) |= (val); \
		__spin_unlock(&(name ## __lock__)); \
	} while(0)

#define I_ATOMIC_NAND(name, val) \
	do { \
		__spin_lock(&(name ## __lock__)); \
		(name ## __atomic__) = ~(name ## __atomic__) & (val); \
		__spin_unlock(&(name ## __lock__)); \
	} while(0)

#define I_ATOMIC_XOR(name, val) \
	do { \
		__spin_lock(&(name ## __lock__)); \
		(name ## __atomic__) ^= (val); \
		__spin_unlock(&(name ## __lock__)); \
	} while(0)

#define I_ATOMIC_GET(name) \
	({ \
	 typeof(name ## __atomic__) var; \
	 do { \
	 __spin_lock(&(name ## __lock__)); \
	 var = (name ## __atomic__); \
	 __spin_unlock(&(name ## __lock__)); \
	 } while (0); \
	 var; \
	 })

#define I_ATOMIC_SET(name, val) \
	({       \
	 typeof(name ## __atomic__) var; \
	 do { \
	 __spin_lock(&(name ## __lock__)); \
	 var = (name ## __atomic__) = val; \
	 __spin_unlock(&(name ## __lock__)); \
	 } while (0); \
	 var; \
	 })

#define I_ATOMIC_CAS(name, cmpval, newval) \
	({ \
	 char r = 0; \
	 do { \
	 __spin_lock((name ## __lock__)); \
	 if (*(name ## __atomic__) == (cmpval)) { \
	 *(name ## __atomic__) = (newval); \
	 r = 1; \
	 } \
	 __spin_unlock((name ## __lock__)); \
	 } while(0); \
	 r; \
	 })

#endif
