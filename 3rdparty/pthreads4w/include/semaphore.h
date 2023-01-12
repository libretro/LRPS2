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
 * When building the library, you should define PTW32_BUILD so that
 * the variables/functions are exported correctly. When using the library,
 * do NOT define PTW32_BUILD, and then the variables/functions will
 * be imported correctly.
 */
#if !defined(PTW32_STATIC_LIB)
#  if defined(PTW32_BUILD)
#    define PTW32_DLLPORT __declspec (dllexport)
#  else
#    define PTW32_DLLPORT __declspec (dllimport)
#  endif
#else
#  define PTW32_DLLPORT
#endif

/*
 * This is a duplicate of what is in the autoconf config.h,
 * which is only used when building the pthread-win32 libraries.
 */

#if !defined(PTW32_CONFIG_H)
#  if defined(WINCE)
#    define NEED_ERRNO
#    define NEED_SEM
#  endif
#  if defined(__MINGW64__)
#    define HAVE_STRUCT_TIMESPEC
#  endif
#endif

/*
 *
 */

#if PTW32_SEMAPHORE_LEVEL >= PTW32_SEMAPHORE_LEVEL_MAX
#if defined(NEED_ERRNO)
#include "need_errno.h"
#else
#include <errno.h>
#endif
#endif /* PTW32_SEMAPHORE_LEVEL >= PTW32_SEMAPHORE_LEVEL_MAX */

#define _POSIX_SEMAPHORES

#if defined(__cplusplus)
extern "C"
{
#endif				/* __cplusplus */

typedef struct sem_t_ * sem_t;

PTW32_DLLPORT int __cdecl sem_init (sem_t * sem,
			    int pshared,
			    unsigned int value);

PTW32_DLLPORT int __cdecl sem_destroy (sem_t * sem);

PTW32_DLLPORT int __cdecl sem_wait (sem_t * sem);

PTW32_DLLPORT int __cdecl sem_timedwait (sem_t * sem,
				 const struct timespec * abstime);

PTW32_DLLPORT int __cdecl sem_post (sem_t * sem);

PTW32_DLLPORT int __cdecl sem_post_multiple (sem_t * sem,
				     int count);

PTW32_DLLPORT int __cdecl sem_getvalue (sem_t * sem,
				int * sval);

#if defined(__cplusplus)
}				/* End of extern "C" */
#endif				/* __cplusplus */

#undef PTW32_SEMAPHORE_LEVEL
#undef PTW32_SEMAPHORE_LEVEL_MAX

#endif				/* !SEMAPHORE_H */
