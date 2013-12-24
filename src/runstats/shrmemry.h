#ifndef _SHRMEMRY_INCLUDED /* allow multiple inclusions */
#define _SHRMEMRY_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* Shared memory functions managed by this module ....
   objects allocated by these calls cannot be freed by
   C Runtime library malloc/free and vise versa */

void * SHmalloc(size_t size);

void SHfree(void * ptr);

void * SHrealloc(void *ptr, size_t size);

void * SHstrdup(char * str);


/* Create shared memory with specified name of specified
   size.
   RETURN: TRUE if created.
    */
int CreateSharedMemory(char * shMemName, unsigned size);

/* Get access to shared memory as a client.  return
   base of shared memory in 'pVoid' */
int GetSharedMemory(char * shMemName, unsigned size, void ** pVoid);

void ReleaseSharedMemory();

#ifdef __cplusplus
}
#endif

#endif /* _SHRMEMRY_INCLUDED */

