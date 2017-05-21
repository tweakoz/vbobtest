/***************************************************************************
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
 * Sample video output application
 * 
 ****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#ifndef	ML_OS_IRIX
#define	oserror()	errno
#endif
#define	OSERROR	strerror(oserror())

#ifdef	ML_OS_NT
#include <ML/getopt.h>
#include <io.h>
#define	memalign(alignment,size)	malloc(size)	// FIXME
#else
#include <unistd.h>
#endif

#include <ML/ml.h>
#include <ML/ml_private.h>
#include <ML/mlu.h>

int debug = 0;
char *Usage =	"\n"
"usage: %s <-d device> <-j jack> [-r file] [-w file] [-gD] [control] [control=value] ...\n"
"where:\n"
    "\t-d <device name>\n"
    "\t-j <jack name>\n"
    "\t-r <file>        read control settings from <file>\n"
    "\t-w <file>        write control settings to <file>\n"
    "\t-g               get all control values after the set operation\n"
    "\t-D               turn on debugging\n"
    "\tcontrol          control name:  run mlquery -d device -j jack\n"
    "\tvalue            control value: run mlquery -d device -j jack -v control\n"
    "\t                 (control by itself displays that control value)\n"
    "\t                 (control=value sets control to that value)\n\n"
    "\tfor jack name: run mlquery to find device names\n"
    "\tthen run mlquery -d device to get jack names\n"
    "\tdevice name syntax is: device[:unit]\n"
    "\tjack name syntax is:  [device[:unit].]jack\n\n"
    ;

int  print_controls( MLint64 openPath, FILE *f );
void flush_cmds( MLopenid openPath, char* rw_mode, char **cmds, int count );
void dparams( MLint64 openPath, MLpv *controls, char *header )
{
    MLpv *p; char buff[256];

    fprintf( stderr, "%s\n", header );
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


/*----------------------------------------------------------------------main */

int main(int argc, char **argv)
{
    char*	devName = NULL;
    char*	jackName = NULL;
    char*	readFile = NULL;
    char*	writeFile = NULL;
    int		c;
    int		get = 0;
    MLint64	devId=0;
    MLint64	jackId=0;
    MLint64	pathId=0;
    MLopenid	openPath;

    /* --- get command line args --- */
    while ((c = getopt(argc, argv, "Dd:ghj:r:w:")) != EOF) {
	switch (c) {
	    case 'D':
		debug++;
		break;

	    case 'd':
		devName = optarg;
		break;

	    case 'g':
		get++;
		break;

	    case 'j':
		jackName = optarg;
		break;

	    case 'r':
		readFile = optarg;
		break;

	    case 'w':
		writeFile = optarg;
		break;

	    case 'h':
	    default:
		fprintf(stderr, Usage, argv[0]);
		exit(1);
	}
    }

    if( !devName && !jackName ) {
	fprintf( stderr, "Need at least a device or jack name!\n" );
	fprintf( stderr, Usage );
	exit(1);
    }

    if( devName ) {
	if( mluFindDeviceByName( ML_SYSTEM_LOCALHOST, devName, &devId )) {
	    fprintf( stderr, "Cannot find device %s.\n", devName );
	    exit(1);
	}
    }
    if( jackName ) {
	if( mluFindJackByName( devId, jackName, &jackId )) {
	    fprintf( stderr, "Cannot find jack %s.\n", jackName );
	    exit(1);
	}
    }
    else if( mluFindFirstInputJack( devId, &jackId ) &&
	     mluFindFirstOutputJack( devId, &jackId ) ) {
	fprintf(stderr, "Cannot find an input or output jack on %s\n",
			devName? devName : "this system" );
	exit(1);
    }

    if( debug ) {
	fprintf( stderr, " devId 0x%" FORMAT_LLX "\njackId 0x%" FORMAT_LLX "\n",
		devId, jackId );
    }

    if( mluFindPathFromJack( jackId, &pathId, NULL ) &&
	mluFindPathToJack(   jackId, &pathId, NULL )) {
	fprintf( stderr, "Cannot find a path to/from %s\n", jackName );
	exit(1);
    }
    if( mlOpen( pathId, NULL, &openPath ) ) {
	fprintf( stderr, "Cannot open path\n" );
	exit(1);
    }

    {
	char *read_cmds[ 300 ];
	char *write_cmds[ 300 ];
	int read_cmd_index = 0;
	int write_cmd_index = 0;

	if( readFile ) {
	    FILE *f = fopen( readFile, "ro" );
	    if( f ) {
		char line[ 200 ];
		if( debug ) {
		    fprintf( stderr, "File %s Controls\n", readFile );
		}
		while( fgets( line, sizeof( line ), f ) > 0 ) {
		    if( debug ) {
			fprintf( stderr, "%s", line );
		    }
		    if(( write_cmds[ write_cmd_index++ ] = strdup( line )) == NULL ) {
			fprintf( stderr, "strdup error\n" );
			exit( 2 );
		    }
		}
		fclose( f );
	    }
	    else {
		fprintf( stderr, "can't open %s\n", readFile );
	    }
	}

	while( optind < argc ) {
	    char *p = argv[ optind++ ];

	    if( strchr( p, '=' ) ) {	// write (put) value
		if( read_cmd_index ) {
		    flush_cmds( openPath, "read", read_cmds, read_cmd_index );
		    read_cmd_index = 0;
		}
		if(( write_cmds[ write_cmd_index++ ] = strdup( p )) == NULL ) {
		    fprintf( stderr, "strdup error\n" );
		    exit( 2 );
		}
	    }
	    else {	// read (get) value
		if( write_cmd_index ) {
		    flush_cmds( openPath, "write", write_cmds, write_cmd_index );
		    write_cmd_index = 0;
		}
		if(( read_cmds[ read_cmd_index++ ] = strdup( p )) == NULL ) {
		    fprintf( stderr, "strdup error\n" );
		    exit( 2 );
		}
	    }
	}

	// flush remaining controls
	if( write_cmd_index ) {
	    flush_cmds( openPath, "write", write_cmds, write_cmd_index );
	}
	if( read_cmd_index ) {
	    flush_cmds( openPath, "read", read_cmds, read_cmd_index );
	}

	// display or write control values to a file?
	if( get ) {
	    print_controls( openPath, stdout );
	}
	if( writeFile ) {
	    FILE *f = fopen( writeFile, "wo" );
	    if( f ) {
		print_controls( openPath, f );
		if( debug ) {
		    fprintf( stderr, "Controls written to %s\n", writeFile );
		}
		fclose( f );
	    }
	    else {
		fprintf( stderr, "can't open %s\n", writeFile );
	    }
	}
    }
    return 0;
}

#include <ctype.h>

void flush_cmds( MLopenid openPath, char * rw_mode, char **cmds, int count )
{
    MLpv msg[ 300 ], *mp = msg;
    int i = 0;

    while( i < count ) {
	char *p = cmds[ i++ ];
	MLint32 size = strlen( p );
	MLint64 data[ 512 ];

	if( debug > 1 ) {
	    fprintf( stderr, "process %s\n", p );
	}
	mlPvFromString( openPath, p, &size, mp++, (MLbyte*)data, sizeof( data ));
    }
    mp->param = ML_END;

    if( strcmp( rw_mode, "write" ) == 0 ) {
	if( mlSetControls( openPath, msg ) != ML_STATUS_NO_ERROR || debug ) {
	    dparams( openPath, msg, "Set Controls" );
	}
    }
    else {
	mlGetControls( openPath, msg );
	dparams( openPath, msg, "Get Controls" );
    }
}

int print_controls( MLint64 openPath, FILE *f )
{
    /* First make a list of all the parameters we need to write */
    MLpv* devCap;
    MLpv* paramIds;
    char paramBuffer[60], valueBuffer[60];
    int i;

    if( mlGetCapabilities(openPath, &devCap)) {
	fprintf( stderr, "Unable to get device capabilities");
	return -1;
    }

    paramIds = mlPvFind(devCap, ML_PARAM_IDS_INT64_ARRAY);
    if( paramIds == NULL ) {
	fprintf( stderr, "Unable to find param id list");
	return -1;
    }

    for(i=0; i < paramIds->length; i++) {
	MLint32 class = ML_PARAM_GET_CLASS( paramIds->value.pInt64[i] );
	if( class == ML_CLASS_VIDEO ||
	    class == ML_CLASS_IMAGE ||
	    class == ML_CLASS_AUDIO ) {

	    int vs, ps;
	    MLpv* paramCap;
	    MLpv controls[2];
	    MLstatus stat;

	    if( paramIds->value.pInt64[i] == ML_VIDEO_UST_INT64 ||
		paramIds->value.pInt64[i] == ML_VIDEO_MSC_INT64 ||
		paramIds->value.pInt64[i] == ML_VIDEO_ASC_INT64 ||
		paramIds->value.pInt64[i] == ML_AUDIO_UST_INT64 ||
		paramIds->value.pInt64[i] == ML_AUDIO_MSC_INT64 ||
		paramIds->value.pInt64[i] == ML_AUDIO_ASC_INT64 ) {

		continue;
	    }

	    controls[0].param = paramIds->value.pInt64[i];
	    controls[1].param = ML_END;

	    // if we're writing to a file, we should only save those params that
	    // can actually be set...
	    if( f != stdout ) {
		if( mlPvGetCapabilities( openPath, controls[0].param, &paramCap )) {
		    fprintf( stderr, "Unable to get param 0x%" FORMAT_LLX " capabilities",
				      controls[0].param );
		    return -1;
		}
		{
		    MLpv *pv = mlPvFind( paramCap, ML_PARAM_ACCESS_INT32 );
		    if( !pv || !( pv->value.int32 & ML_ACCESS_W )) {
			continue;
		    }
		}
	    }

	    if( stat = mlGetControls(openPath, controls)) {
		if( debug ) {
		    char pname[64] = "";
		    int size = sizeof(pname);
		    mlPvParamToString(openPath, controls, pname, &size);
		    fprintf(stderr, "Unable to get param %s: %s",
				     pname, mlStatusName( stat ));
		    continue;
		}
	    }
	    ps = sizeof(paramBuffer)-1;
	    if( mlPvParamToString(openPath, controls, paramBuffer, &ps) ) {
		fprintf(stderr,
		    "Unable to convert param 0x%" FORMAT_LLX " to string",
			controls[0].param);
		continue;
	    }
	    vs = sizeof(valueBuffer)-1;
	    if( mlPvValueToString(openPath, controls, valueBuffer, &vs) ) {
		fprintf(stderr,
		    "Unable to convert %s value %d (0x%x) to string",
			paramBuffer, controls[0].value.int32,
				     controls[0].value.int32 );
		continue;
	    }
	    if( ps + vs < 72 ) {
		fprintf( f, "\t%s = %s\n", paramBuffer, valueBuffer );
	    }
	    else {
		fprintf( f, "\t%s =\n\t\t%s", paramBuffer, valueBuffer );
	    }
	}
    }
    mlFreeCapabilities( devCap );
    return 0;
}
