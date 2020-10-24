add_library(pthread
   pthread.c
   config.h
   context.h
   implement.h
   need_errno.h
   include/pthread.h
   include/sched.h
   include/semaphore.h
)

target_include_directories(pthread PRIVATE . include)
#target_compile_definitions(pthread PRIVATE -DHAVE_PTW32_CONFIG_H -DPTW32_BUILD_INLINED)

