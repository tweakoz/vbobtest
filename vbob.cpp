#include "vbob.h"

const char* timingtable[] = {
    "ML_TIMING_NONE",
    "ML_TIMING_UNKNOWN",
    "ML_TIMING_525",
    "ML_TIMING_625",
    "ML_TIMING_525_SQ_PIX",
    "ML_TIMING_625_SQ_PIX",
    "ML_TIMING_1125_1920x1080_60p",
    "ML_TIMING_1125_1920x1080_5994p",
    "ML_TIMING_1125_1920x1080_50p",
    "ML_TIMING_1125_1920x1080_60i",
    "ML_TIMING_1125_1920x1080_5994i",
    "ML_TIMING_1125_1920x1080_50i",
    "ML_TIMING_1125_1920x1080_30p",
    "ML_TIMING_1125_1920x1080_2997p",
    "ML_TIMING_1125_1920x1080_25p",
    "ML_TIMING_1125_1920x1080_24p",
    "ML_TIMING_1125_1920x1080_2398p",
    "ML_TIMING_1125_1920x1080_24PsF",
    "ML_TIMING_1125_1920x1080_2398PsF",
    "ML_TIMING_1125_1920x1080_30PsF",
    "ML_TIMING_1125_1920x1080_2997PsF",
    "ML_TIMING_1125_1920x1080_25PsF",
    "ML_TIMING_1250_1920x1080_50p",
    "ML_TIMING_1250_1920x1080_50i",
    "ML_TIMING_1125_1920x1035_60i",
    "ML_TIMING_1125_1920x1035_5994i",
    "ML_TIMING_750_1280x720_60p",
    "ML_TIMING_750_1280x720_5994p",
    "ML_TIMING_525_720x483_5994p",
};

MLint32 freeBuffers( void** buffers, MLint32 maxBuffers )
{
  int i;

  for (i = 0; i < maxBuffers; i++) 
    if (buffers[i]) 
      free(buffers[i]);
  return 0;
}

/*--------------------------------------------------------------allocateBuffers
 * Allocate an array of image buffers with specified alignment and size
 * wrapper around virtualAlloc/memalign
 */
MLint32 allocateBuffers(void** buffers,
    MLint32 imageSize,
    MLint32 maxBuffers,
    MLint32 memAlignment)
{
    printf("imageSize<%d>\n", int(imageSize));
    printf("maxBuffers<%d>\n", int(maxBuffers));
    printf("memAlignment<%d>\n", int(memAlignment));

    int i;

    for (i = 0; i < maxBuffers; i++) {
        buffers[i] = memalign(memAlignment, imageSize);
        if (buffers[i] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return -1;
        }
        /*
     * Here we touch the buffers, forcing the buffer memory to be mapped
     * this avoids the need to map the buffers the first time they're used.
     * We could go the extra step and mpin them, but choose not to here,
     * trying a simpler approach first.
     */
        memset(buffers[i], 0, imageSize);
    }
    return 0;
}

/*-------------------------------------------------------------------------*/
MLstatus event_wait(MLwaitable pathWaitHandle)
{

#ifdef ML_OS_NT
    if (WaitForSingleObject(pathWaitHandle, INFINITE) != WAIT_OBJECT_0) {
        fprintf(stderr, "Error waiting for reply\n");
        return ML_STATUS_INTERNAL_ERROR;
    }
    return ML_STATUS_NO_ERROR;

#else /* ML_OS_UNIX */
    for (;;) {
        fd_set fdset;
        int rc;

        FD_ZERO(&fdset);
        FD_SET(pathWaitHandle, &fdset);

        rc = select(pathWaitHandle + 1, &fdset, NULL, NULL, NULL);
        if (rc < 0) {
            fprintf(stderr, "select: %s\n", OSERROR);
            return ML_STATUS_INTERNAL_ERROR;
        }
        if (rc == 1) {
            return ML_STATUS_NO_ERROR;
        }
        /* anything else, loop again */
    }
#endif
}

void dparams(MLint64 openPath, MLpv* controls)
{
    MLpv* p;
    char buff[256];

    for (p = controls; p->param != ML_END; p++) {
        MLint32 size = sizeof(buff);
        MLstatus stat = mlPvToString(openPath, p, buff, &size);

        if (stat != ML_STATUS_NO_ERROR) {
            fprintf(stderr, "mlPvToString: %s\n", mlStatusName(stat));
        } else {
            fprintf(stderr, "\t%s", buff);
            if (p->length != 1)
                fprintf(stderr, " (length %d)", p->length);
            fprintf(stderr, "\n");
        }
    }
}
