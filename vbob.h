#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define OSERROR strerror(oserror())

#include <unistd.h>

#include <ML/ml.h>
#include <ML/mlu.h>

#include <string>
#include <vector>

extern const char* timingtable[];

MLint32 freeBuffers( void** buffers, MLint32 maxBuffers );

MLint32 allocateBuffers(void** buffers, MLint32 imageSize,
    MLint32 maxBuffers, MLint32 memAlignment);

MLstatus event_wait(MLwaitable pathWaitHandle);

void dparams(MLint64 openPath, MLpv* controls);

#define   setB(cp,id,val,len,mlen)\
        cp->param = id; \
        cp->value.pByte = (MLbyte*) val; \
        cp->length = len; \
        cp->maxLength = mlen; \
        cp++
