add_library(z
   adler32.c
   compress.c
   crc32.c
   deflate.c
   gzclose.c
   gzlib.c
   gzread.c
   gzwrite.c
   infback.c
   inffast.c
   inflate.c
   inftrees.c
   trees.c
   uncompr.c
   zutil.c


   crc32.h
   deflate.h
   inffast.h
   inffixed.h
   inflate.h
   inftrees.h
   trees.h
   zconf.h
   zlib.h
   zutil.h
)

target_compile_definitions(z PRIVATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_WARNINGS)
if(WIN32)
   target_compile_definitions(z PRIVATE -D__WIN32__ -DWIN32 -D_WINDOWS -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
endif()
