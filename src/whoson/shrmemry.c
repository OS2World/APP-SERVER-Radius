
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define INCL_DOSMEMMGR
#define INCL_DOSERRORS
#include <os2.h>

static PVOID heapBase;
static int memorySubset=0;


void * SHmalloc(size_t size){
  PVOID newBlock;
  APIRET rc;

  /* Allocate extra to store block size */
  int blockSize=size + sizeof(int);
  rc=DosSubAllocMem(heapBase, &newBlock, blockSize);
  if (rc==0) {
    int * intBlock=newBlock;
    /* Store block size for free call */
    *intBlock=blockSize;
    intBlock++;
    return (intBlock);
  } else {
    return (0);
  }

}

void SHfree(void * ptr) {
  int * intBlock=ptr;
  int size;
  APIRET rc;

  intBlock--;
  size=*intBlock;
  rc=DosSubFreeMem(heapBase, intBlock, size);
}



void * SHrealloc(void *oldPtr, size_t size) {

  int * intBlock;
  int oldSize;
  int copySize;
  char * newPtr=SHmalloc(size);

  if (newPtr) {
    /* Determine size of old block to copy */
    intBlock=oldPtr;
    intBlock--;
    oldSize=*intBlock;
    copySize=min(oldSize, size);
    memcpy(newPtr, oldPtr, copySize);
    SHfree(oldPtr);
  }
  return (newPtr);
}



void * SHstrdup(char * str) {
  char * newPtr=SHmalloc(strlen(str) + 1);
  if (newPtr) {
    strcpy(newPtr, str);
  }
  return (newPtr);
}




int CreateSharedMemory(char * shMemName, unsigned heapSize) {

  int created=0;
  APIRET rc          = NO_ERROR;  /* Return code                          */

  rc = DosAllocSharedMem(&heapBase, shMemName, heapSize,
                           PAG_COMMIT |    /* Commit memory                 */
                           PAG_WRITE );    /* Allocate memory as read/write */
  if (rc==ERROR_ALREADY_EXISTS) {
    rc=DosGetNamedSharedMem(&heapBase, shMemName,
          PAG_WRITE);
  }

  if (rc==0) {
    memset(heapBase, 0, heapSize);
    rc = DosSubSetMem
      ( heapBase, DOSSUB_INIT | DOSSUB_SERIALIZE, heapSize );
    if (rc==0) {
      created=1;
    }
  }

  memorySubset=created;
  return (created);
}



int GetSharedMemory(char * shMemName, unsigned heapSize, void ** pVoid){

  int created;
  APIRET rc;

  created=0;

  rc = DosAllocSharedMem(&heapBase, shMemName, heapSize,
                PAG_WRITE | PAG_COMMIT);
  if (rc==0) {
    memset(heapBase, 0, heapSize);
    created=1;
  } else if (rc==ERROR_ALREADY_EXISTS) {
    rc=DosGetNamedSharedMem(&heapBase, shMemName,
                PAG_READ);
    created = (rc==0);
  }
  if (created) {
    *pVoid=heapBase;
  }
  return (created);
}


void ReleaseSharedMemory() {
  APIRET rc;

  if (heapBase) {
    if (memorySubset) {
      rc = DosSubUnsetMem(heapBase);
    }
    rc=DosFreeMem(heapBase);
    heapBase=0;
  }

}


