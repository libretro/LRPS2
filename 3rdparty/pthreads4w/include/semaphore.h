#if !defined( SEMAPHORE_H )
#define SEMAPHORE_H

#undef PTW32_SEMAPHORE_LEVEL

#if defined(_POSIX_SOURCE)
#define PTW32_SEMAPHORE_LEVEL 0
/* Early POSIX */
#endif

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309
#undef PTW32_SEMAPHORE_LEVEL
#define PTW32_SEMAPHORE_LEVEL 1
/* Include 1b, 1c and 1d */
#endif

#if defined(INCLUDE_NP)
#undef PTW32_SEMAPHORE_LEVEL
#define PTW32_SEMAPHORE_LEVEL 2
/* Include Non-Portable extensions */
#endif

#define PTW32_SEMAPHORE_LEVEL_MAX 3

#if !defined(PTW32_SEMAPHORE_LEVEL)
#define PTW32_SEMAPHORE_LEVEL PTW32_SEMAPHORE_LEVEL_MAX
/* Include everything */
#endif

#if defined(__GNUC__) && ! defined (__declspec)
# error Please upgrade your GNU compiler to one that supports __declspec.
#endif

/*
 * This is a duplicate of what is in the autoconf config.h,
 * which is only used when building the pthread-win32 libraries.
 */

#if !defined(PTW32_CONFIG_H)
#  if defined(__MINGW64__)
#    define HAVE_STRUCT_TIMESPEC
#  endif
#endif

#define _POSIX_SEMAPHORES

#if defined(__cplusplus)
extern "C"
{
#endif				/* __cplusplus */

typedef struct sem_t_ * sem_t;

int __cdecl sem_init (sem_t * sem,
			    int pshared,
			    unsigned int value);

int __cdecl sem_destroy (sem_t * sem);

int __cdecl sem_wait (sem_t * sem);

int __cdecl sem_timedwait (sem_t * sem,
				 const struct timespec * abstime);

int __cdecl sem_post (sem_t * sem);

int __cdecl sem_post_multiple (sem_t * sem,
				     int count);

int __cdecl sem_getvalue (sem_t * sem,
				int * sval);

#if defined(__cplusplus)
}				/* End of extern "C" */
#endif				/* __cplusplus */

#undef PTW32_SEMAPHORE_LEVEL
#undef PTW32_SEMAPHORE_LEVEL_MAX

#endif				/* !SEMAPHORE_H */
