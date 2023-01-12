add_library(pthread
   pthread.c
   config.h
   context.h
   implement.h
   include/pthread.h
   include/sched.h
   include/semaphore.h
)

target_include_directories(pthread PRIVATE . include)
