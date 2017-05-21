/*
 *
 * License Applicability. Except to the extent portions of this file are 
 * made subject to an alternative license as permitted in the SGI Free 
 * Software License B, Version 1.1 (the "License"), the contents of this 
 * file are subject only to the provisions of the License. You may not use 
 * this file except in compliance with the License. You may obtain a copy 
 * of the License at Silicon Graphics, Inc., attn: Legal Services, 
 * 1600 Amphitheatre Parkway, Mountain View, CA 94043-1351, or at: 
 * 
 * http://oss.sgi.com/projects/FreeB
 * 
 * Note that, as provided in the License, the Software is distributed 
 * on an "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND 
 * CONDITIONS DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED 
 * WARRANTIES AND CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
 * 
 * OpenML ML Library, 1.0, 12/13/2001, developed by Silicon Graphics, Inc. 
 * ML1.0 is Copyright (c) 2001 Silicon Graphics, Inc. Copyright in any 
 * portions created by third parties is as indicated elsewhere herein. 
 * All Rights Reserved.
 *  
 ***************************************************************************/

/****************************************************************************
 *
 * Sample video input to gfx application
 * 
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>

#include <errno.h>
#ifndef	ML_OS_IRIX
#define	oserror()	errno
#endif
#define	OSERROR	strerror(oserror())

#ifdef	ML_OS_NT
#include <ML/getopt.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include <ML/ml.h>
#include <ML/mlu.h>

#include <X11/X.h>   /* X */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <GL/glx.h>  /* OpenGL */
#include <GL/gl.h>
#include <GL/glu.h>

#ifdef	ML_OS_IRIX
#include <audio.h>
#endif

#ifndef	FORMAT_LLX
    #ifdef ML_OS_UNIX
	#define FORMAT_LLX "llx"
	#define FORMAT_LLD "lld"
    #endif
    #ifdef ML_OS_NT
	#define FORMAT_LLX "I64x"
	#define FORMAT_LLD "I64d"
    #endif
#endif

/* 
 * Define a few convenience routines
 */
MLint32 allocateBuffers( void** buffers, MLint32 imageSize,
			 MLint32 maxBuffers, MLint32 memAlignment);

MLint32 freeBuffers( void** buffers, MLint32 maxBuffers );

static void	createWindowAndContext(Window *win, GLXContext *ctx,int *visualAttr,int width, int height, char name[]);

/* 
 * And a few global variables
 */
Display	        *dpy;
Window	        window;

char *Usage = 
    "usage: %s -d <device name> [options]\n"
    "options:\n"
    #ifdef	ML_OS_IRIX
    "  -a <audio device> send embedded audio to <device> [or '-']\n"
    #endif
    "  -b #             number buffers to allocate (and preroll)\n"
    "  -c #             count of buffers to transfer (0 = indefinitely)\n"
    "  -d <device name> (run mlquery to find device names)\n"
    "  -j <jack name>   (run mlquery to find jack names)\n"
    "  -s <timing>      set video standard\n"
    "  -v <line #>      enable vitc display (use -2 to not specify line number)\n"
    "  -D               turn on debugging\n"
    "\n"
    "available timings:\n"
    "    480i (or) 486i (or) 487i (or) NTSC\n"
    "    576i (or) PAL\n"
    "   1080i (or) SMPTE274/29I\n"
    "   1080p24 (or) SMPTE274/24P\n"
    ;

static void dparams( MLint64 openPath, MLpv *controls )
{
    MLpv *p; char buff[256];

    for( p = controls; p->param != ML_END; p++ ) {
	MLint32 size = sizeof( buff );
	MLstatus stat = mlPvToString( openPath, p, buff, &size );

	if( stat != ML_STATUS_NO_ERROR ) {
	    fprintf( stderr, "mlPvToString: %s\n", mlStatusName( stat ));
	}
	else {
	    fprintf( stderr, "\t%s", buff );
	    if( p->length != 1 ) fprintf( stderr, " (length %d)", p->length );
	    fprintf( stderr, "\n" );
	}
    }
}

#ifdef	ML_OS_IRIX
ALport openAudioContext( char *device )
{
    ALport p;
    ALconfig theconfig = ALnewconfig();

    if( device && device[0] != '-' ) {
	int i = alGetResourceByName( AL_SYSTEM, device, AL_DEVICE_TYPE );
	if (i != 0) {
	    alSetDevice(theconfig, i);
	}
	else {
	    fprintf( stderr, "invalid device name %s\n", device );
	    exit(1);
	}
    }

    p = alOpenPort("dropout","w",theconfig);
    if (!p) {
	fprintf(stderr, "openport failed: %s\n",alGetErrorString(oserror()));
	exit(-1);
    }

    return p;
}
#endif

char *timingtable[] = {
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

/*-------------------------------------------------------------------------*/
MLstatus event_wait( MLwaitable pathWaitHandle )
{

#ifdef	ML_OS_NT
    if(WaitForSingleObject(pathWaitHandle, INFINITE) != WAIT_OBJECT_0) {
	fprintf(stderr, "Error waiting for reply\n");
	return ML_STATUS_INTERNAL_ERROR;
    }
    return ML_STATUS_NO_ERROR;

#else	/* ML_OS_UNIX */
    for( ;; ) {
	fd_set fdset;
	int rc;

	FD_ZERO( &fdset);
	FD_SET( pathWaitHandle, &fdset );

	rc = select( pathWaitHandle+1, &fdset, NULL, NULL, NULL );
	if( rc < 0 ) {
	    fprintf( stderr, "select: %s\n", OSERROR );
	    return ML_STATUS_INTERNAL_ERROR;
	}
	if( rc == 1 ) {
	    return ML_STATUS_NO_ERROR;
	}
	/* anything else, loop again */
    }
#endif
}

/*-------------------------------------------------------------------------main
 */
int main(int argc, char **argv)
{
  int c;
  int debug=0; // debug level, 0=no debug output

  MLint32 imageWidth;
  MLint32 imageHeight;
  MLint32 timing = -1, input_timing = -1;
  char *timing_option = NULL;
#ifdef	ML_OS_IRIX
  char *audio_device = NULL;
  ALport audio_handle = NULL;
  MLint32 abuffsize = 9000;
#endif

  MLint32 transferred_buffers = 0;
  MLint32 requested_buffers = 600;
  MLint32 maxBuffers = 10;

  void** buffers;
  void** abuffers;
  char *devName = NULL;
  char *jackName = NULL;

  MLint32	display_vitc = 0, vitc_inited = 0;
  MLUTimeCode tc;

  MLint32 i;
  MLint64 devId=0;
  MLint64 jackId=0;
  MLint64 pathId=0;
  MLint32 memAlignment;
  MLopenid openPath;
  MLwaitable pathWaitHandle;
  MLint32 imageSize;

    int interactive = 0;
  GLXContext ctx;
  int visualAttr[] = {	GLX_RGBA,
  			GLX_RED_SIZE, 8,
			GLX_GREEN_SIZE, 8,
			GLX_BLUE_SIZE, 8,
			GLX_DOUBLEBUFFER,
		      None};

  /* --- get command line args --- */
  while ((c = getopt(argc, argv, "a:b:c:d:ij:s:v:Dh")) != EOF) {
      switch (c) {
#ifdef	ML_OS_IRIX
	case 'a':
	    audio_device = optarg;
	    break;
#endif

	case 'b':
	    maxBuffers = atoi(optarg);
	    break;

	case 'c':
	    requested_buffers = atoi(optarg);
	    break;

	case 'd':
	  devName = optarg;
	  break;
    
	case 'i':
	    interactive = 1;
	    break;

	case 'j':
	  jackName = optarg;
	  break;
    
	case 's':
	  timing_option = optarg;
          if(!strcmp(optarg, "NTSC") || 
	     !strcmp(optarg, "480i") ||
	     !strcmp(optarg, "486i") ||
	     !strcmp(optarg, "487i")) {
	    timing = ML_TIMING_525;
          }
	  else if(!strcmp(optarg, "PAL") || !strcmp(optarg, "576i")) {
	    timing = ML_TIMING_625; /* PAL */
          }
	  else if(!strcmp(optarg, "SMPTE274/29I") ||
		  !strcmp(optarg, "1080i")) {
	    timing = ML_TIMING_1125_1920x1080_5994i; /* HD */
          }
	  else if(!strcmp(optarg, "SMPTE274/24P") ||
		  !strcmp(optarg, "1080p24")) {
	    timing = ML_TIMING_1125_1920x1080_24p; /* HD */
	  }
	  else {
	      fprintf(stderr, Usage, argv[0]);
	      exit(0);
	  }
	  break;
    
	case 'v':
	    display_vitc = atoi( optarg );
	    memset( &tc, 0, sizeof( tc ));
	    break;

	case 'D':
	  debug++;
	  break;
    
	case 'h':
	default:
	  fprintf(stderr, Usage, argv[0]);
	  exit(0);
	}
    }

#ifdef	ML_OS_IRIX
  if( audio_device ) {
    audio_handle = openAudioContext( audio_device );
  }
#endif
  buffers = (void**) malloc( sizeof( void * ) * maxBuffers );
  abuffers = (void**) malloc( sizeof( void * ) * maxBuffers );

    // open requested path...

    if( devName ) {
	if( mluFindDeviceByName( ML_SYSTEM_LOCALHOST, devName, &devId )) {
	  fprintf(stderr, "Cannot find device '%s'\n", devName);
	  return -1;
	}
    }

    if( mluFindJackByName( devId, jackName, &jackId )) {
	fprintf(stderr, "Cannot find jack '%s'\n", jackName);
	return -1;
    }

    if( mluFindPathFromJack( jackId, &pathId, &memAlignment )) {
	fprintf(stderr, "Cannot find a path from jack '%s.%s'\n",
		devName, jackName);
	return -1;
    }

    if( mlOpen( pathId, NULL, &openPath ) ) {
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
	MLpv controls[21], *cp;

	/* Define an array of events for use later (in this case,
	 * there is only a single event, but we'll code it so we
	 * add more events by just adding them to this array)
	 */
	MLint32 events[] = { ML_EVENT_VIDEO_SEQUENCE_LOST };

	/* clear pv variables - clears length (error) flags */
	memset( controls, 0, sizeof( controls ));

	/* Video parameters, describing the signal at the jack */
	#define	setV(cp,id,val)	cp->param = id; \
				cp->value.int32 = val; \
				cp->length = 1; \
				cp++

	/* if timing not specified, see if we can discover the
	 * input timing */
	cp = controls;
	setV( cp, ML_VIDEO_SIGNAL_PRESENT_INT32, timing );
	setV( cp, ML_END, 0 );
	if( mlGetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
	    fprintf(stderr, "Couldn't get controls on path\n");
	    dparams( openPath, controls );
	    return -1;
	}
	input_timing = controls[0].value.int32;
	fprintf(stderr, "Input Timing Present = %s\n",
			    timingtable[input_timing]);
	
    printf( "wtfa\n");
    if( timing == -1 ) {
	    if( controls[0].value.int32 != ML_TIMING_NONE &&
		controls[0].value.int32 != ML_TIMING_UNKNOWN )
        {
		  timing = input_timing;
	    }
	}
    printf( "wtfb\n");

	/* if we can't discover the timing, then just get what it's
	 * currently set to and use that. */
	if( timing == -1 ) {
	    cp = controls;
	    setV( cp, ML_VIDEO_TIMING_INT32, timing );
	    setV( cp, ML_END, 0 );
	    if( mlGetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
		fprintf(stderr, "Couldn't get controls on path\n");
		dparams( openPath, controls );
		return -1;
	    }
	    timing = controls[0].value.int32;
	}
	if( input_timing != timing ) {
	    fprintf(stderr, "Cannot set requested timing = %s\n",
			    timingtable[timing]);
	    exit(1);
	}

	/* now set the timing and video precision */
	

    cp = controls;
	setV( cp, ML_VIDEO_TIMING_INT32, timing );
	//setV( cp, ML_VIDEO_PRECISION_INT32, 10 );
	setV( cp, ML_END, 0 );
	if( mlSetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
	    fprintf(stderr, "Couldn't set controls on path\n");
	    dparams( openPath, controls );
	    return -1;
	}

    if(debug) {
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
	(cp  )->param = ML_END;
	mluComputePathParamsFromTiming( timing, controls,
					MLU_TIMING_IGNORE_INVALID );

	/* the following are variable - set for our need to display on gfx */
	setV( cp, ML_IMAGE_COMPRESSION_INT32, ML_COMPRESSION_UNCOMPRESSED );
	setV( cp, ML_IMAGE_COLORSPACE_INT32, ML_COLORSPACE_RGB_601_FULL );
	setV( cp, ML_IMAGE_SAMPLING_INT32, ML_SAMPLING_444 );
	setV( cp, ML_IMAGE_PACKING_INT32, ML_PACKING_8 );
	setV( cp, ML_IMAGE_INTERLEAVE_MODE_INT32,
		  ML_INTERLEAVE_MODE_INTERLEAVED );

	/* honor user request for VITC processing */
	if( display_vitc && display_vitc != -2 ) {
	    setV( cp, ML_VITC_LINE_NUMBER_INT32, display_vitc );
	}

	/* Device events, describing which events we want sent to us */
	(cp  )->param = ML_DEVICE_EVENTS_INT32_ARRAY;
	(cp  )->value.pInt32 = events;
	(cp  )->length=sizeof(events)/sizeof(MLint32);
	(cp++)->maxLength=sizeof(events)/sizeof(MLint32);

	/* That's everything, now signal the end of the list */
	(cp  )->param = ML_END;

	/* adjust sizes for interleaved capture */
	{
        auto x = 1;
	    MLpv *sp  = mlPvFind( controls, ML_VIDEO_START_Y_F1_INT32 );
	    MLpv *vp  = mlPvFind( controls, ML_VIDEO_HEIGHT_F1_INT32 );
	    MLpv *ip  = mlPvFind( controls, ML_IMAGE_HEIGHT_1_INT32 );

	    /* adjust video/image sizes to be what the user requested */
	    if( timing_option ) {
		if( strcmp( timing_option, "487i") == 0 ) {
		    sp[0].value.int32 = 20;
		    vp[0].value.int32 = 244;
		    vp[1].value.int32 = 243;
		    ip[0].value.int32 = 487;
		    ip[1].value.int32 = 0;
		}
		else if( strcmp( timing_option, "486i") == 0 ) {
		    sp[0].value.int32 = 21;
		    vp[0].value.int32 = 243;
		    vp[1].value.int32 = 243;
		    ip[0].value.int32 = 486;
		    ip[1].value.int32 = 0;
		}
		else if( strcmp( timing_option, "480i") == 0 ) {
		    sp[0].value.int32 = 21;
		    vp[0].value.int32 = 240;
		    vp[1].value.int32 = 240;
		    ip[0].value.int32 = 480;
		    ip[1].value.int32 = 0;
		}
		else {
		    ip[0].value.int32 += ip[1].value.int32;
		    ip[1].value.int32 = 0;
		}
	    }
	    else {
		ip[0].value.int32 += ip[1].value.int32;
		ip[1].value.int32 = 0;
	    }
	}

	/* now send the completed params to the video device */
	if( mlSetControls(openPath, controls) != ML_STATUS_NO_ERROR ) {
	    fprintf(stderr, "Couldn't set controls on path\n");
	    dparams( openPath, controls );
	    return -1;
	}

	/* and finally, get the image size so we can setup the display window */
	cp = controls;
	(cp++)->param = ML_IMAGE_WIDTH_INT32;
	(cp++)->param = ML_IMAGE_HEIGHT_1_INT32;
	(cp++)->param = ML_IMAGE_HEIGHT_2_INT32;
	(cp  )->param = ML_END;
	if( mlGetControls(openPath, controls) != ML_STATUS_NO_ERROR) {
	    fprintf(stderr, "Couldn't get controls on path\n");
	    dparams( openPath, controls );
	    return -1;
	}
	imageWidth  = controls[0].value.int32;
	imageHeight = controls[1].value.int32 + controls[2].value.int32;

	/* display param values to user */
	if( debug ) {
	    dparams( openPath, controls );
	}
    }

  if( mluGetImageBufferSize(openPath, &imageSize ) )
    {
      fprintf(stderr, "Couldn't get buffer size\n");
      return -1;
    }

  if( allocateBuffers( buffers, imageSize, maxBuffers, memAlignment)) 
    {
      fprintf(stderr, "Cannot allocate memory for image buffers\n");
      return -1;
    }

#ifdef	ML_OS_IRIX
  if( audio_handle &&
      allocateBuffers( abuffers, abuffsize, maxBuffers, memAlignment)) 
    {
      fprintf(stderr, "Cannot allocate memory for audio buffers\n");
      return -1;
    }
#endif

  /*
   * Initialize graphics
   */
  createWindowAndContext(&window,&ctx,visualAttr, imageWidth, imageHeight,
			 argv[0]);
  XSelectInput(dpy,window,ButtonPressMask|KeyPressMask);
  glLoadIdentity();
  glViewport(0, 0, imageWidth, imageHeight);
  glOrtho(0, imageWidth, 0, imageHeight, -1, 1);
  glShadeModel(GL_FLAT);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_BLEND);		
  glDisable(GL_DEPTH_TEST);	
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glPixelZoom(1.0, -1.0);
      
  /* 
   * We can either poll mlReceiveMessage, or allow a wait
   * variable to be signalled.  Polling is undesirable - its
   * much better to wait for an event - that way the OS can
   * swap us out and make full use of the processor until the
   * event occurs.
   */
  if ( mlGetReceiveWaitHandle(openPath, &pathWaitHandle) ) 
    {
      fprintf(stderr, "Cannot get wait handle\n");
      goto SHUTDOWN;
    }

  /* Preroll - send buffers to the path in preparation
   * for beginning the transfer.
   */
  for (i = 0; i < maxBuffers; i++) 
    {
      MLpv msg[10], *p = msg;
      #define	setB(cp,id,val,len,mlen)\
				cp->param = id; \
				cp->value.pByte = val; \
				cp->length = len; \
				cp->maxLength = mlen; \
				cp++

      setB( p, ML_IMAGE_BUFFER_POINTER, (MLbyte*) buffers[i], 0, imageSize );
      setV( p, ML_VIDEO_MSC_INT64, 0 );
      setV( p, ML_VIDEO_UST_INT64, 0 );
#ifdef	ML_OS_IRIX
      if( audio_handle ) {
	  setB( p, ML_AUDIO_BUFFER_POINTER, (MLbyte*) abuffers[i], 0, abuffsize );
      }
#endif

      if( display_vitc ) {
	  MLint32 timecode;
	  MLU_TC_TYPE tc_type;
	  if( !vitc_inited ) {
	      if( timing == ML_TIMING_525 ) {
		  tc_type = MLU_TC_2997_4FIELD_DROP;
	      }
	      else {
		  tc_type = MLU_TC_25_ND;
	      }
	      mluTCFromSeconds( &tc, tc_type, (double)0.0 );
	  }
	  mluTCPack( &timecode, &tc );
	  setV( p, ML_VITC_TIMECODE_INT32, timecode );
	  setV( p, ML_VITC_USERDATA_INT32, 'DATA' );
      }
      setV( p, ML_END, 0 );

      if( mlSendBuffers(openPath, msg) ) 
	{ 
	  fprintf(stderr, "Error sending buffers.\n");
	  goto SHUTDOWN;
	}
    }

  /* 
   * Now start the video transfer
   */
  if (mlBeginTransfer(openPath) )
    { 
      fprintf(stderr, "Error beginning transfer.\n");
      goto SHUTDOWN;
    }

  /* We can either poll mlReceiveMessage, or allow a wait
   * variable to be signalled.  Polling is undesirable - its
   * much better to wait for an event - that way the OS can
   * swap us out and make full use of the processor until the
   * event occurs.
   */
  if ( mlGetReceiveWaitHandle(openPath, &pathWaitHandle) ) 
    {
      fprintf(stderr, "Cannot get wait handle\n");
      goto SHUTDOWN;
    }

    while (requested_buffers == 0 || transferred_buffers < requested_buffers) {

	MLstatus status = event_wait( pathWaitHandle );
	if( status != ML_STATUS_NO_ERROR ) {
	    exit( 1 );
	}

      /* 
       * Let's see what reply message is ready...
       */
      {
	MLint32 messageType;
	MLpv* message;

	if( mlReceiveMessage(openPath, &messageType, &message ))
	  { 
	    fprintf(stderr, "Unable to receive reply message\n");
	    goto SHUTDOWN;
	  }

	switch (messageType) 
	  {
	  case ML_BUFFERS_COMPLETE:
	    {
	      /* 
               * Here we could do something with the result of the
	       * transfer.
               */
	      union { MLint32 word; char bytes[8]; } userdata = {0};
	      MLbyte* theBuffer = message[0].value.pByte;
	      MLint64 theMSC    = message[1].value.int64;
	      MLint64 theUST    = message[2].value.int64;
	      MLpv *pv = mlPvFind( message, ML_VITC_TIMECODE_INT32 );
	      MLpv *uv = mlPvFind( message, ML_VITC_USERDATA_INT32 );
	      MLpv line[] = { { ML_VITC_INCOMING_LINE_NUMBER_INT32 },
			    { ML_END } };

	      assert( message[0].param == ML_IMAGE_BUFFER_POINTER );
	      transferred_buffers++;

	      if( uv ) {
	        userdata.word = uv->value.int32;
	      }
	      if( pv ) {
		mluTCUnpack( &tc, pv->value.int32 );
		mlGetControls( openPath, line );

		if( debug > 1 ) {
		  printf("VITC %s %02d:%02d:%02d%s%02d on line %d\n", 
		      userdata.bytes, tc.hours, tc.minutes, tc.seconds,
		      // display a ":" for even field, "." for odd field 
		      // (just like sony monitors ;-)
		      tc.evenField? ":" : ".", tc.frames,
		      line[0].value.int32 );
		}
	      }

	      if (debug > 1 ) {
		printf("  transfer %d complete: length:%d, MSC:%" FORMAT_LLD ", UST:%" FORMAT_LLD "\n",
		     transferred_buffers, message[0].length, theMSC, theUST);
	      }
	      else if( debug && pv ) {
		  printf("\r  %s %02d:%02d:%02d%s%02d line %d  ", 
		      userdata.bytes, tc.hours, tc.minutes, tc.seconds,
		      // display a ":" for even field, "." for odd field 
		      // (just like sony monitors ;-)
		      tc.evenField? ":" : ".", tc.frames,
		      line[0].value.int32 );
		  fflush( stdout );
	      }
	      else if( (transferred_buffers%10) == 0 ) {
		fprintf( stderr, "." ); fflush( stderr );
	      }

	      /* First see if we're running behind, if so, then
		 don't draw this frame */
	      {
		int count;
		mlGetReceiveMessageCount(openPath, &count);
		if( count < 2 )
		  {
		    /*glRasterPos2i( 0, 0);*/
		    glRasterPos2i( 0, imageHeight-1);
	  
		    glDrawPixels(imageWidth, imageHeight, 
				 GL_RGB, GL_UNSIGNED_BYTE,
				 (char *)theBuffer);
	      
		    if( debug >= 3 ) {
		      int *t = &((int*)theBuffer)[1000];
		      printf( "0x%p: 0x%x 0x%x 0x%x\n", t, t[0], t[1], t[2] );
		    }

		    glXSwapBuffers(dpy,window);

		    /* do audio */
#ifdef	ML_OS_IRIX
		    if( audio_handle ) {
		    	/* Assume AUDIO_FORMAT_S16 for now. */
			unsigned short *buf = (unsigned short *) message[3].value.pByte;
			unsigned short tmp;
			int len = (int)message[3].length / sizeof (unsigned short);
			int i;
			for (i=0; i<len; i++)
			{
				tmp = buf[i];
				buf[i] = (tmp >> 8) | (tmp << 8);
			}
			/* Assume stereo for now. */
			alWriteFrames( audio_handle, buf, len / 2);
		    }
#endif
		  }
		else
		  {
		    if (debug>=2)
		      printf(" gfx skipped frame (msg queue cnt=%d)\n",count);
		  }
	      }

	      /* 
               * Send the buffer back to the path so another frame can
	       * be transferred into it.
	       */
	      if( interactive ) {
		char line[200]; gets(line);
	      }
	      mlSendBuffers(openPath, message);
	    }
	    break;
	  case ML_EVENT_VIDEO_SEQUENCE_LOST: 
	    fprintf(stderr,"Warning, dropped a frame");
	    break;
	    
	  default:
	    fprintf(stderr, "Something went wrong - message is %s\n", 
		    mlMessageName(messageType));
	    goto SHUTDOWN;
	  }
	fflush(stdout);
      }
    }

SHUTDOWN:
  fprintf(stderr,"Shutdown\n");

  mlEndTransfer(openPath);
  mlClose(openPath);

  freeBuffers( buffers, maxBuffers );

  return 0;
}

/*--------------------------------------------------------------allocateBuffers
 * Allocate an array of image buffers with specified alignment and size
 */
MLint32 allocateBuffers( void** buffers, 
			 MLint32 imageSize,
			 MLint32 maxBuffers, 
			 MLint32 memAlignment)
{
  int i;

  for (i = 0; i < maxBuffers; i++) 
  {
    buffers[i] = memalign( memAlignment, imageSize );
    if (buffers[i] == NULL )
      {
	fprintf(stderr,"Memory allocation failed\n");
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

/*------------------------------------------------------------------freeBuffers
*/ 
MLint32 freeBuffers( void** buffers, MLint32 maxBuffers )
{
  int i;

  for (i = 0; i < maxBuffers; i++) 
    if (buffers[i]) 
      free(buffers[i]);
  return 0;
}

/* ARGSUSED dpy */
static Bool waitForMapNotify(Display *dpy, XEvent *ev, XPointer arg)
{
    return (ev->type == MapNotify && ev->xmapping.window == *((Window *) arg));
}

/* ARGSUSED dpy */
static int xioerror(Display *dpy)
{
    exit(-1);
    /*NOTREACHED*/
}

/* 
 * create X window and opengl context 
 */
static void createWindowAndContext(Window *win, GLXContext *ctx,
				   int *visualAttr, int xsize, int ysize,
				   char name[])
{
    XSizeHints hints;
    XSetWindowAttributes swa;
    Colormap cmap;
    XEvent event;
    XVisualInfo *vi;

    memset( &hints, 0, sizeof( hints ));
    hints.min_width = xsize/2;
    hints.max_width = 2*xsize;
    hints.base_width = hints.width = xsize;
    hints.min_height = ysize/2;
    hints.max_height = 2*ysize;
    hints.base_height = hints.height = ysize;
    hints.flags = USSize | PMinSize | PMaxSize;

    /* get a connection */
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "Can't connect to display `%s'\n",
                getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
    XSetIOErrorHandler (xioerror);

    vi = glXChooseVisual(dpy, DefaultScreen(dpy), visualAttr);
    if( vi == NULL ) {
	fprintf(stderr, "No matching visual on display `%s'\n",
		getenv("DISPLAY"));
	exit(EXIT_FAILURE);
    }

    /* create a GLX context */
    if ((*ctx = glXCreateContext(dpy, vi, 0, GL_TRUE)) == NULL){
       fprintf(stderr,"Cannot create a context.\n");
       exit(EXIT_FAILURE);
    }
    /* create an empty colormap (required for XCreateWindow) */
    cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                           vi->visual, AllocNone);
    if ((void*)cmap == NULL) {
        fprintf(stderr,"Cannot create a colormap.\n");
        exit(EXIT_FAILURE);
    }
    /* create a window */
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask |KeyPressMask | ExposureMask;

    *win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                        0, 0, xsize, ysize,
                        0, vi->depth, InputOutput, vi->visual,
                        CWBorderPixel | CWColormap | CWEventMask, &swa);
    if ((void*)(*win) == NULL) {
        fprintf(stderr,"Cannot create a window.\n");
        exit(EXIT_FAILURE);
    }
    XStoreName(dpy, *win, name);
    XSetNormalHints(dpy, *win, &hints);
    XMapWindow(dpy, *win);
    XIfEvent(dpy, &event, waitForMapNotify, (XPointer) win);
    /* Connect the GL context to the window */
    if (!glXMakeCurrent(dpy, *win, *ctx)) {
        fprintf(stderr, "Can't make window current to context\n");
        exit(EXIT_FAILURE);
    }
}

