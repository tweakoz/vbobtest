
#include "sgilicense.h"
#include "vbob.h"

/****************************************************************************
 *
 * Sample video output application
 * 
 ****************************************************************************/

MLint32 fillBuffers(const std::string& fileName, void** buffers, MLint32 imageSize,
    MLint32 maxBuffers);

const char* Usage = "usage: %s -d <device> -f <filename> [options]\n"
                    "options:\n"
                    "    -b #             number buffers to allocate (and preroll)\n"
                    "    -c #             count of buffers to transfer (0 = indefinitely)\n"
                    "    -d <device name> (run mlquery to find device names)\n"
                    "    -f <filename>    save pixels in 'filename'\n"
                    "                     (use '%%d' in filename to make seperate image files)\n"
                    "                     (use .ppm extension to make PPM files)\n"
                    "    -j <jack name>   (run mlquery to find jack names)\n"
                    "    -s <timing>      set video standard\n"
                    "    -v <line #>      place vitc on line # (-2 to dynamically move)\n"
                    "    -D               turn on debugging\n"
                    "\n"
                    "available timings:\n"
                    "     480i (or) 486i (or) 487i (or) NTSC\n"
                    "     576i (or) PAL\n"
                    "    1080i (or) SMPTE274/29I\n"
                    "    1080p24 (or) SMPTE274/24P\n";



/*-------------------------------------------------------------------------*/
int next_word(FILE* f)
{
    if (f) {
        int rewound = 0;
        union {
            char line[80];
            int word;
        } data;

        while (rewound < 2) {
            while (fgets(data.line, sizeof(data.line), f)) {
                if (strlen(data.line) == 5) {
                    data.line[4] = '\0';
                    return data.word;
                }
            }
            rewind(f);
            rewound++;
        }
    }
    return 0x01020304;
}



/*-------------------------------------------------------------------------main
 */
int main(int argc, char** argv)
{
    MLint32 imageSize;

    MLint32 transferred_buffers = 0;
    MLint32 requested_buffers = 1000;
    MLint32 maxBuffers = 10;
    void** buffers;

    int c;
    int debug = 0;
    char* jackName = NULL;
    std::string fileName;

    MLUTimeCode tc;
    FILE* wordfile;
    MLint32 display_vitc = -5,
            vitc_inited = 0,
            vitc_line,
            vitc_line_max,
            user_data;
    MLint32 i;
    MLint64 devId = 0;
    MLint64 jackId = 0;
    MLint64 pathId = 0;
    MLint32 memAlignment;
    MLopenid openPath;
    MLwaitable pathWaitHandle;

    MLint32 timing = -1, input_timing = -1;
    char* timing_option = NULL;
    char* devName;

    /* --- get command line args --- */
    while ((c = getopt(argc, argv, "b:c:d:Df:hj:s:v:")) != EOF) {
        switch (c) {
        case 'b':
            maxBuffers = atoi(optarg);
            break;

        case 'c':
            requested_buffers = atoi(optarg);
            break;

        case 'd':
            devName = optarg;
            break;

        case 'D':
            debug++;
            break;

        case 'f':
            fileName = optarg;
            break;

        case 'j':
            jackName = optarg;
            break;

        case 's':
            timing_option = optarg;
            if (!strcmp(optarg, "NTSC") || !strcmp(optarg, "480i") || !strcmp(optarg, "486i")) {
                timing = ML_TIMING_525;
            } else if (!strcmp(optarg, "PAL") || !strcmp(optarg, "576i")) {
                timing = ML_TIMING_625; /* PAL */
            } else if (!strcmp(optarg, "SMPTE274/29I") || !strcmp(optarg, "1080i")) {
                timing = ML_TIMING_1125_1920x1080_5994i; /* HD */
            } else if (!strcmp(optarg, "SMPTE274/24P") || !strcmp(optarg, "1080p24")) {
                timing = ML_TIMING_1125_1920x1080_24p; /* HD */
            } else {
                fprintf(stderr, Usage, argv[0]);
                exit(0);
            }
            break;

        case 'v':
            display_vitc = atoi(optarg);
            vitc_line = display_vitc;
            user_data = 0x01020304;
            if (vitc_line == -2) {
                vitc_line = 4;
            }
            memset(&tc, 0, sizeof(tc));
            wordfile = fopen("/usr/lib/dict/words", "ro");
            break;

        case 'h':
        default:
            fprintf(stderr, Usage, argv[0]);
            exit(0);
        }
    }

    buffers = (void**)malloc(sizeof(void*) * maxBuffers);

    /*
    * Find out about this system
    */

    if (mluFindDeviceByName(ML_SYSTEM_LOCALHOST, devName, &devId)) {
        fprintf(stderr, "Cannot find device named:%s.\n", devName);
        return -1;
    }

    if (jackName == NULL) {
        if (mluFindFirstOutputJack(devId, &jackId)) {
            fprintf(stderr, "Cannot find a suitable output jack.\n");
            return -1;
        }
    } else {
        if (mluFindJackByName(devId, jackName, &jackId)) {
            fprintf(stderr, "Cannot find jack: %s.\n", jackName);
            return -1;
        }
    }

    if (mluFindPathToJack(jackId, &pathId, &memAlignment)) {
        fprintf(stderr, "Cannot find a path to jack\n");
        return -1;
    }

    if (mlOpen(pathId, NULL, &openPath)) {
        fprintf(stderr, "Cannot open path\n");
        goto SHUTDOWN;
    }

    /* 
     * Set the path parameters.  In this case, we wish to
     * transfer high-definition or standard-definition,
     * we want the data to be stored with 8-bits-per-component,
     * and we want it to have an RGB colorspace in memory.
     * We also wish to be notified of sequence-lost events
     * (these correspond to cases where the application was
     * unable to send enough buffers quickly enough to keep
     * the video hardware busy).
     */
    {
        MLpv controls[30], *cp;

        /* Define an array of events for use later (in this case,
     * there is only a single event, but we'll code it so we
     * add more events by just adding them to this array)
     */
        MLint32 events[] = { ML_EVENT_VIDEO_SEQUENCE_LOST };

        /* clear pv variables - clears length (error) flags */
        memset(controls, 0, sizeof(controls));

/* Video parameters, describing the signal at the jack */
#define setV(cp, id, val, typ) \
    cp->param = id;            \
    cp->value.typ = val;       \
    cp->length = 1;            \
    cp++

        /* if timing not specified, see if we can discover the
     * input timing */
        if (timing == -1) {
            cp = controls;
            setV(cp, ML_VIDEO_SIGNAL_PRESENT_INT32, timing, int32);
            setV(cp, ML_END, 0, int32);
            if (mlGetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
                fprintf(stderr, "Couldn't get controls on path (a)\n");
                dparams(openPath, controls);
                return -1;
            }
            input_timing = controls[0].value.int32;
            fprintf(stderr, "Output timing present = %s\n",
                timingtable[input_timing]);
        }
        if (timing == -1) {
            if (controls[0].value.int32 != ML_TIMING_NONE && controls[0].value.int32 != ML_TIMING_UNKNOWN) {
                timing = input_timing;
            }
        }

        /* if we can't discover the timing, then just get what it's
     * currently set to and use that. */
        if (timing == -1) {
            cp = controls;
            setV(cp, ML_VIDEO_TIMING_INT32, timing, int32);
            setV(cp, ML_END, 0, int32);
            auto mlerr = mlGetControls(openPath, controls);
            printf("MLERR<%d>\n", int(mlerr));
            if (mlerr != ML_STATUS_NO_ERROR) {
                fprintf(stderr, "Couldn't get controls on path (b)\n");
                dparams(openPath, controls);
                return -1;
            }
            timing = controls[0].value.int32;
        }

        /*if( input_timing != ML_TIMING_UNKNOWN && input_timing != timing ) {
        fprintf(stderr, "Cannot set requested timing = %s\n",
                timingtable[timing]);
        exit(1);
    }*/
        if (ML_TIMING_UNKNOWN == timing) {
            fprintf(stderr, "Cannot set requested timing = %s\n",
                timingtable[timing]);
            exit(1);
        }

        /* now set the timing and video precision */
        cp = controls;
        setV(cp, ML_VIDEO_TIMING_INT32, timing, int32);
        setV(cp, ML_VIDEO_PRECISION_INT32, 10, int32);
        setV(cp, ML_END, 0, int32);
        if (mlSetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
            fprintf(stderr, "Couldn't set controls on path\n");
            dparams(openPath, controls);
            return -1;
        }

        if (debug) {
            printf(" Timing %d\n", timing);
        }

        /* now set remainder of controls */
        cp = controls;

        /* the following controls are dependent on TIMING, we'll use
     * a utility routine to fill in the actual values */
        (cp++)->param = ML_VIDEO_COLORSPACE_INT32;
        (cp++)->param = ML_VIDEO_WIDTH_INT32;
        (cp++)->param = ML_VIDEO_HEIGHT_F1_INT32;
        (cp++)->param = ML_VIDEO_HEIGHT_F2_INT32;
        (cp++)->param = ML_VIDEO_START_X_INT32;
        (cp++)->param = ML_VIDEO_START_Y_F1_INT32;
        (cp++)->param = ML_VIDEO_START_Y_F2_INT32;

        /* Image parameters, describing the signal in memory */
        /* these too are dependent on TIMING */
        (cp++)->param = ML_IMAGE_WIDTH_INT32;
        (cp++)->param = ML_IMAGE_HEIGHT_1_INT32;
        (cp++)->param = ML_IMAGE_HEIGHT_2_INT32;
        (cp++)->param = ML_IMAGE_TEMPORAL_SAMPLING_INT32;
        (cp++)->param = ML_IMAGE_DOMINANCE_INT32;

        /* set timing dependent parameters */
        (cp)->param = ML_END;
        mluComputePathParamsFromTiming(timing, controls,
            MLU_TIMING_IGNORE_INVALID);

        /* the following are variable - set for our need to display on gfx */
        setV(cp, ML_IMAGE_COMPRESSION_INT32, ML_COMPRESSION_UNCOMPRESSED, int32);
        setV(cp, ML_IMAGE_COLORSPACE_INT32, ML_COLORSPACE_RGB_601_FULL, int32);
        setV(cp, ML_IMAGE_SAMPLING_INT32, ML_SAMPLING_444, int32);
        setV(cp, ML_IMAGE_PACKING_INT32, ML_PACKING_8, int32);
        setV(cp, ML_IMAGE_INTERLEAVE_MODE_INT32,
            ML_INTERLEAVE_MODE_INTERLEAVED, int32);

        /* did the user request VITC processing? */
        if (display_vitc != -5) {
            MLint32 v = display_vitc;
            if (v == -2) {
                v = 4;
            }
            setV(cp, ML_VITC_LINE_NUMBER_INT32, v, int32);
            if (timing == ML_TIMING_525) {
                vitc_line_max = 19;
            } else if (timing == ML_TIMING_625) {
                vitc_line_max = 21;
            } else {
                vitc_line_max = 25; /* no idea what this should be */
            }
        }

        /* Device events, describing which events we want sent to us */
        (cp)->param = ML_DEVICE_EVENTS_INT32_ARRAY;
        (cp)->value.pInt32 = events;
        (cp)->length = sizeof(events) / sizeof(MLint32);
        (cp++)->maxLength = sizeof(events) / sizeof(MLint32);

        /* That's everything, now signal the end of the list */
        (cp)->param = ML_END;

        /* adjust sizes for interleaved capture */
        {
            MLpv* sp = mlPvFind(controls, ML_VIDEO_START_Y_F1_INT32);
            MLpv* vp = mlPvFind(controls, ML_VIDEO_HEIGHT_F1_INT32);
            MLpv* ip = mlPvFind(controls, ML_IMAGE_HEIGHT_1_INT32);

            /* adjust video/image sizes to be what the user requested */
            if (timing_option) {
                if (strcmp(timing_option, "487i") == 0) {
                    sp[0].value.int32 = 20;
                    vp[0].value.int32 = 244;
                    vp[1].value.int32 = 243;
                    ip[0].value.int32 = 487;
                    ip[1].value.int32 = 0;
                } else if (strcmp(timing_option, "486i") == 0) {
                    sp[0].value.int32 = 21;
                    vp[0].value.int32 = 243;
                    vp[1].value.int32 = 243;
                    ip[0].value.int32 = 486;
                    ip[1].value.int32 = 0;
                } else if (strcmp(timing_option, "480i") == 0) {
                    sp[0].value.int32 = 21;
                    vp[0].value.int32 = 240;
                    vp[1].value.int32 = 240;
                    ip[0].value.int32 = 480;
                    ip[1].value.int32 = 0;
                } else {
                    ip[0].value.int32 += ip[1].value.int32;
                    ip[1].value.int32 = 0;
                }
            } else {
                ip[0].value.int32 += ip[1].value.int32;
                ip[1].value.int32 = 0;
            }
        }

        /* now send the completed params to the video device */
        if (mlSetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
            fprintf(stderr, "Couldn't set controls on path\n");
            dparams(openPath, controls);
            return -1;
        }

        /* display param values to user */
        if (debug) {
            dparams(openPath, controls);
        }
    }

    if (mluGetImageBufferSize(openPath, &imageSize)) {
        fprintf(stderr, "Couldn't get buffer size\n");
        return -1;
    }

    if (allocateBuffers(buffers, imageSize, maxBuffers, memAlignment)) {
        fprintf(stderr, "Cannot allocate memory for image buffers\n");
        return -1;
    }

    if (fillBuffers(fileName, buffers, imageSize, maxBuffers)) {
        fprintf(stderr, "Cannot fill memory for image buffers\n");
        return -1;
    }

    /* Preroll - send buffers to the path in preparation
    * for beginning the transfer.
    */
    printf("Prerolling %d buffers: ", maxBuffers);
    fflush(stdout);
    for (i = 0; i < maxBuffers; i++) {
        MLpv msg[8], *pv = msg;

        setB(pv, ML_IMAGE_BUFFER_POINTER, buffers[i], imageSize, imageSize);
        setV(pv, ML_VIDEO_MSC_INT64, 0, int64);
        setV(pv, ML_VIDEO_UST_INT64, 0, int64);

        if (display_vitc != -5) {
            MLint32 timecode;

            MLU_TC_TYPE tc_type;
            if (!vitc_inited) {
                if (timing == ML_TIMING_525) {
                    tc_type = MLU_TC_2997_4FIELD_DROP;
                } else {
                    tc_type = MLU_TC_25_ND;
                }
                mluTCFromSeconds(&tc, tc_type, (double)0.0);
            }
            mluTCPack(&timecode, &tc);
            if (debug > 1) {
                printf(" VITC %02d:%02d:%02d%s%02d -> 0x%08x\n",
                    tc.hours, tc.minutes, tc.seconds,
                    // display a ":" for even field, "." for odd field
                    // (just like sony monitors ;-)
                    tc.evenField ? ":" : ".",
                    tc.frames, timecode);
            }
            setV(pv, ML_VITC_LINE_NUMBER_INT32, vitc_line, int32);
            setV(pv, ML_VITC_TIMECODE_INT32, timecode, int32);
            setV(pv, ML_VITC_USERDATA_INT32, user_data, int32);

            mluTCAddFrames(&tc, &tc, 1, NULL);
        }
        setV(pv, ML_END, 0, int64);

        if (mlSendBuffers(openPath, msg)) {
            fprintf(stderr, "Error sending buffers.\n");
            goto SHUTDOWN;
        }
        printf(".");
        fflush(stdout);
    }
    printf("done.\n");

    /* 
    * Now start the video transfer
    */
    printf("Begin transfer %d buffers\n", requested_buffers);
    fflush(stdout);
    if (mlBeginTransfer(openPath)) {
        fprintf(stderr, "Error beginning transfer.\n");
        goto SHUTDOWN;
    }

    /* We can either poll mlReceiveMessage, or allow a wait
    * variable to be signalled.  Polling is undesirable - its
    * much better to wait for an event - that way the OS can
    * swap us out and make full use of the processor until the
    * event occurs.
    */
    if (mlGetReceiveWaitHandle(openPath, &pathWaitHandle)) {
        fprintf(stderr, "Cannot get wait handle\n");
        goto SHUTDOWN;
    }

    while (requested_buffers == 0 || transferred_buffers < requested_buffers) {
        MLstatus status = event_wait(pathWaitHandle);
        if (status != ML_STATUS_NO_ERROR) {
            exit(1);
        }

        /* 
       * Let's see what reply message is ready...
       */
        {
            MLint32 messageType;
            MLpv* message;

            if (mlReceiveMessage(openPath, &messageType, &message)) {
                fprintf(stderr, "\nUnable to receive reply message\n");
                goto SHUTDOWN;
            }

            switch (messageType) {
            case ML_BUFFERS_COMPLETE: {
                /* 
           * Here we could do something with the result of the
           * transfer.
           *
           * MLbyte* theBuffer = message[0].value.pByte;
           * MLint64 theMSC    = message[1].value.int64;
           * MLint64 theUST    = message[2].value.int64;
           */
                transferred_buffers++;
                printf("\r  transfer %d complete!  ", transferred_buffers);
                fflush(stdout);

                /* 
           * Send the buffer back to the path so another field can
           * be transferred from it.
           */
                if (display_vitc != -5) {
                    MLpv* p = mlPvFind(message, ML_VITC_TIMECODE_INT32);
                    MLpv* d = mlPvFind(message, ML_VITC_USERDATA_INT32);
                    MLpv* l = mlPvFind(message, ML_VITC_LINE_NUMBER_INT32);
                    mluTCAddFrames(&tc, &tc, 1, NULL);
                    mluTCPack(&p->value.int32, &tc);
                    if (d) {
                        if (display_vitc == -2 && (transferred_buffers % 5) == 0) {
                            // rotate word
                            user_data = next_word(wordfile);
                        }
                        d->value.int32 = user_data;
                    }
                    if (l && display_vitc == -2 && (transferred_buffers % 10) == 0) {
                        if (++l->value.int32 > vitc_line_max) {
                            l->value.int32 = 4;
                        }
                        printf(" vitc line %d ", l->value.int32);
                        fflush(stdout);
                    }
                }
                mlSendBuffers(openPath, message);
            } break;
            case ML_BUFFERS_FAILED: {
                /* 
           * Here we could do something with the result of the
           * failed transfer.
           *
           * MLbyte* theBuffer = message[0].value.pByte;
           */

                fprintf(stderr, "\nTransfer failed, aborting.\n");
            }
                goto SHUTDOWN;
            case ML_EVENT_VIDEO_SEQUENCE_LOST:
                fprintf(stderr, "\nWarning, missed field\n");
                break;
            case ML_EVENT_QUEUE_OVERFLOW:
                fprintf(stderr, "\nEvent queue overflowed, aborting\n");
                goto SHUTDOWN;

            default:
                fprintf(stderr, "\nSomething went wrong - message is %s\n",
                    mlMessageName(messageType));
                goto SHUTDOWN;
            }
            fflush(stdout);
        }
    }

SHUTDOWN:
    printf("\nShutdown\n");

    mlEndTransfer(openPath);
    mlClose(openPath);

    freeBuffers(buffers, maxBuffers);

    return 0;
}


/*----------------------------------------------------------------fill buffers
 * Fill each buffer with a recognizable pattern
 */

MLint32 fillBuffersFromBytes(void** buffers, MLint32 imageSize,MLint32 maxBuffers)
{
    for (int i = 0; i < maxBuffers; i++) {
        MLbyte* p;
        int x, y;

        for (p = (MLbyte*)buffers[i]; p < (MLbyte*)buffers[i] + imageSize / 5;) {
            *p++ = 0x0;
            *p++ = 0x0;
            *p++ = 0x0;
        }

        for (; p < (MLbyte*)buffers[i] + imageSize * 2 / 5;) {
            *p++ = (MLbyte)0xe0;
            *p++ = (MLbyte)0x00;
            *p++ = (MLbyte)0x00;
        }

        for (; p < (MLbyte*)buffers[i] + imageSize * 3 / 5;) {
            *p++ = (MLbyte)0x00;
            *p++ = (MLbyte)0xe0;
            *p++ = (MLbyte)0x0;
        }

        for (; p < (MLbyte*)buffers[i] + imageSize * 4 / 5;) {
            *p++ = (MLbyte)0x00;
            *p++ = (MLbyte)0x00;
            *p++ = (MLbyte)0xe0;
        }

        for (; p < (MLbyte*)buffers[i] + imageSize;) {
            *p++ = (MLbyte)0xe0;
            *p++ = (MLbyte)0xe0;
            *p++ = (MLbyte)0xe0;
        }

        for (y = 0; y < 100; y++)
            for (x = 0; x < i * 10 + 100; x++)
                *((MLbyte*)buffers[i] + ((y * 720) + x) * 3) = (MLbyte)0x80;

        printf(".");
        fflush(stdout);    
    }
    return 0;
}

MLint32 fillBuffersFromFile(const std::string& fileName, 
                            void** buffers, 
                            MLint32 imageSize,
                            MLint32 maxBuffers )
{
    int fd;
    FILE* fp;
    bool imagefiles = false; //(strchr(fileName, '%') != NULL);
    bool ppmfile = (fileName.find(".ppm") != std::string::npos);

    if (!imagefiles) {
        if (ppmfile) {
            fp = fopen(fileName.c_str(), "rb");
            if (fp < 0) {
                perror(fileName.c_str());
                return -1;
            }
        } else {
            fd = open(fileName.c_str(), O_RDONLY, 0666);
            if (fd < 0) {
                perror(fileName.c_str());
                return -1;
            }
        }
    }

    for (int i = 0; i < maxBuffers; i++) {
        int rc;
        if (imagefiles) {
            char fn[100];
            sprintf(fn, fileName.c_str(), i);
            if (ppmfile) {
                fp = fopen(fn, "rb");
                if (fp < 0) {
                    perror(fn);
                    return -1;
                }
            } else {
                fd = open(fn, O_CREAT | O_RDONLY, 0666);
                if (fd < 0) {
                    perror(fn);
                    return -1;
                }
            }
        }
        if (ppmfile) {
            int readPPMfile(FILE * fp, char* buffer, int buffersize);
            rc = readPPMfile(fp, (char*)buffers[i], imageSize);
            if (rc != 1) {
                perror("fread");
                return -1;
            }
        } else {
            rc = read(fd, buffers[i], imageSize);
            if (rc != imageSize) {
                perror("read");
                return -1;
            }
        }

        if (imagefiles) {
            if (ppmfile) {
                fclose(fp);
            } else {
                close(fd);
            }
        }
        printf(".");
        fflush(stdout);
    }    
    return 0;
}

MLint32 fillBuffers(const std::string& fileName, void** buffers, MLint32 imageSize,
    MLint32 maxBuffers)
{
    printf("Filling buffers: ");

    if (fileName.length())
        printf("from %s: ", fileName.c_str());

    fflush(stdout);

    if (0 == fileName.length()) 
        fillBuffersFromBytes(buffers,imageSize,maxBuffers);
    else 
        fillBuffersFromFile(fileName,buffers,imageSize,maxBuffers);

    printf("done.\n");
    return 0;
}

static int _readInt(FILE* fptr, int* result)
{
    int c;

    while ((c = fgetc(fptr)) > 0) {
        switch (c) {
        case '#': /* ignore rest of comment line */
        {
            char line[71];
            fgets(line, 71, fptr);
            continue;
        }
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            continue;
        default:
            ungetc(c, fptr);
            return fscanf(fptr, "%d", result) == 1;
        }
    }
    return -1;
}

int readPPMfile(FILE* fp, char* buffer, int buffersize)
{
    char filetype[3];
    int maxValue, imageWidth, imageHeight, isize;
    int rc;

    if (fgets(filetype, 3, fp) < 0) {
        fprintf(stderr, "can't get filetype\n");
        exit(2);
    }
    if ((filetype[0] != 'P' && filetype[0] != 'S') || filetype[1] != '6') {
        fprintf(stderr, "not a ppm file: %s\n", filetype);
        exit(2);
    }

    _readInt(fp, &imageWidth);
    _readInt(fp, &imageHeight);
    _readInt(fp, &maxValue);
    fgetc(fp); /* skip over single whitespace char after maxValue */

    isize = imageWidth * imageHeight * 3;

    if (isize > buffersize) {
        fprintf(stderr, "Error: image size %d buffer size %d\n",
            isize, buffersize);
        fprintf(stderr, "imageWidth %d imageHeight %d maxValue %d\n",
            imageWidth, imageHeight, maxValue);
        exit(2);
    }
    if ((rc = fread(buffer, isize, 1, fp)) != 1) {
        fprintf(stderr, "fread returns %d\n", rc);
        exit(2);
    }
    if (isize < buffersize) {
        memset(&buffer[isize], 0, buffersize - isize);
    }

    return rc;
}
