/*
gcc -arch i386 -O3 -o SetDisplay SetDisplay.c -framework Cocoa

SetDisplay.c

This tool sets the display resolution.  This tool exists so that we
can set the display to a setting when the display manager says it can't,
mainly, when a Mac starts up and the monitor is off, or if the Mac is on
a KVM.

Copyright (c) 2007 University of Utah Student Computing Labs.
All Rights Reserved.

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appears in all copies and
that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of The University
of Utah not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission. This software is supplied as is without expressed or
implied warranties of any kind.

USAGE:
You can give it arguments or not.  If you don't give it arguments, it will use:

1024 x 768, 32 bits per pixel, and 75 htz

If you give it 4 arguments, they should be: width, height, bits per pixel, and refresh rate.

Like so:

SetDisplay 1024 768 32 75

WARNING:
In testing garbage values, I did get this to tool to change the display so that absolutely
nothing displayed.  I don't remember what I did to get that.  And I can't duplicate it anymore.
I could remotely control it, so I could fix it.  But beware, if you can't remotely control the
machine and fix bad display settings, well, you might have to resort to drastic measures to fix
a mac with bad display settings (like boot off of a different disk and delete the display
preferences.

In other words, this tool will change the display settings regardless of what the display
manager says is possible.  So be careful!!!

*/

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_DISPLAYS 32

typedef struct
{
	size_t width;
	size_t height;
	size_t bitsPerPixel;
	CGRefreshRate refresh;
} displayMode;

displayMode myModeStruct;

static int numberForKey( CFDictionaryRef desc, CFStringRef key )
{
	CFNumberRef value;
	int num = 0;

	if ( (value = CFDictionaryGetValue(desc, key)) == NULL )
		return 0;
	CFNumberGetValue(value, kCFNumberIntType, &num);
	return num;
}

static void printDispDesc( CFDictionaryRef desc )
{
	char * msg;
	if ( CFDictionaryGetValue(desc, kCGDisplayModeUsableForDesktopGUI) == kCFBooleanTrue )
		msg = "Supports Aqua GUI";
	else
		msg = "Not for Aqua GUI";

	printf( "\t%d x %d,\t%d BPP,\t%d Hz\t(%s)\n",
			numberForKey(desc, kCGDisplayWidth),
			numberForKey(desc, kCGDisplayHeight),
			numberForKey(desc, kCGDisplayBitsPerPixel),
			numberForKey(desc, kCGDisplayRefreshRate),
			msg );
}

static void printShortDispDesc( CFDictionaryRef desc, int showOnlyAqua )
{
	int aqua;
	if ( CFDictionaryGetValue(desc, kCGDisplayModeUsableForDesktopGUI) == kCFBooleanTrue )
		aqua = 1;
	else
		aqua = 0;

	if ( showOnlyAqua == 1 ) {
		if ( aqua == 1 )
			printf( "%d %d %d %d Usable\n",
					numberForKey(desc, kCGDisplayWidth),
					numberForKey(desc, kCGDisplayHeight),
					numberForKey(desc, kCGDisplayBitsPerPixel),
					numberForKey(desc, kCGDisplayRefreshRate)
					);
	} else {
		printf( "%d %d %d %d %s\n",
				numberForKey(desc, kCGDisplayWidth),
				numberForKey(desc, kCGDisplayHeight),
				numberForKey(desc, kCGDisplayBitsPerPixel),
				numberForKey(desc, kCGDisplayRefreshRate),
				(aqua) ? "Usable" : "Nonusable"
				);	
	}
}

static void findClosestModeForDisplay( CGDirectDisplayID display, int verbose)
{
	CFDictionaryRef mode;
	boolean_t exactMatch;

	if ( verbose == 1 )
		printf( "Looking for chosest match to %ld x %ld, %ld Bits Per Pixel, %ld Hz\n", myModeStruct.width, myModeStruct.height, myModeStruct.bitsPerPixel, (long int)myModeStruct.refresh );
	mode = CGDisplayBestModeForParametersAndRefreshRate(display, myModeStruct.bitsPerPixel, myModeStruct.width, myModeStruct.height, myModeStruct.refresh, &exactMatch);
	if ( verbose == 1 )
		printf ("The chosen mode:\n");
	
	if ( verbose == 1 )
		printDispDesc( mode );
	else
		printShortDispDesc( mode, 0 );
	
	if( !exactMatch && verbose == 1 )
	{
		printf( "Not an exact match\n" );
	}
}

static void findExactModeForDisplay( CGDirectDisplayID display, int verbose)
{
	CFIndex index, count;
	CFArrayRef modes;
	CFDictionaryRef mode;

	modes = CGDisplayAvailableModes (display);
	count = CFArrayGetCount (modes);
	for (index = 0; index < count; index++)
	{
		mode = CFArrayGetValueAtIndex( modes, index );
		if ( numberForKey(mode, kCGDisplayWidth) == myModeStruct.width &&
				numberForKey(mode, kCGDisplayHeight) == myModeStruct.height &&
				numberForKey(mode, kCGDisplayRefreshRate) == myModeStruct.refresh &&
				numberForKey(mode, kCGDisplayBitsPerPixel) == myModeStruct.bitsPerPixel)
		{
			if ( verbose == 1 )
				printDispDesc( mode );
			else
				printShortDispDesc( mode, 0 );
			return;
		}
	}
}

static void allModesForDisplay( CGDirectDisplayID display, int verbose)
{
	CFIndex index, count;
	CFArrayRef modes;
	CFDictionaryRef mode;
	printf( "------ All modes for display ------\n" );
	modes = CGDisplayAvailableModes (display);
	count = CFArrayGetCount (modes);
	for (index = 0; index < count; index++)
	{
		mode = CFArrayGetValueAtIndex( modes, index );
		printShortDispDesc( mode, verbose );
	}
	printf( "------------\n" );
}

static void highestModeForDisplay( CGDirectDisplayID display, displayMode *highestMode, int verbose )
{
	CFIndex index, count;
	CFArrayRef modes;
	CFDictionaryRef mode;
	
	highestMode->width = 0;
	highestMode->height = 0;
	highestMode->bitsPerPixel = 0;
	highestMode->refresh = 0;

	modes = CGDisplayAvailableModes (display);
	count = CFArrayGetCount (modes);
	for (index = 0; index < count; index++)
	{
		mode = CFArrayGetValueAtIndex( modes, index );
		if ( numberForKey(mode, kCGDisplayWidth) > highestMode->width )
			highestMode->width = numberForKey(mode, kCGDisplayWidth);
		if ( numberForKey(mode, kCGDisplayHeight) > highestMode->height )
			highestMode->height = numberForKey(mode, kCGDisplayHeight);
		if ( numberForKey(mode, kCGDisplayRefreshRate) > highestMode->refresh )
			highestMode->refresh = numberForKey(mode, kCGDisplayRefreshRate);
		if ( numberForKey(mode, kCGDisplayBitsPerPixel) > highestMode->bitsPerPixel )
			highestMode->bitsPerPixel = numberForKey(mode, kCGDisplayBitsPerPixel);
	}
	if ( verbose == 1 )
		printf( "Highest mode for display: %ld x %ld, %ld Bits Per Pixel, %ld Hz\n", highestMode->width, highestMode->height, highestMode->bitsPerPixel, (long int)highestMode->refresh );
	else
		printf( "%ld %ld %ld %ld\n", highestMode->width, highestMode->height, highestMode->bitsPerPixel, (long int)highestMode->refresh );
}

static void setdisplay( CGDirectDisplayID display, int mirroringOnOff, int verbose )
{
	CGError err;
	CGDisplayConfigRef configRef;
	CFDictionaryRef mode;
	boolean_t exactMatch;

	CGBeginDisplayConfiguration(&configRef);
	mode = CGDisplayBestModeForParametersAndRefreshRate(display, myModeStruct.bitsPerPixel, myModeStruct.width, myModeStruct.height, myModeStruct.refresh, &exactMatch);
	err = CGConfigureDisplayMode(configRef, display, mode);
	
	if ( mirroringOnOff == 1 ) {
		CGConfigureDisplayMirrorOfDisplay(configRef, display, kCGNullDirectDisplay);
	} else if ( mirroringOnOff == 2 ) {
		CGDirectDisplayID mainDisplay = CGMainDisplayID();
		if ( display != mainDisplay ) {
			CGConfigureDisplayMirrorOfDisplay(configRef, display, mainDisplay);
		}
	}

	CGCompleteDisplayConfiguration( configRef, kCGConfigurePermanently );
	if ( err != CGDisplayNoErr )
	{
		printf( "Oops!  Mode switch failed?!?? (%d)\n", err );
	}
}

static void usage()
{
	printf( "SetDisplay [-acvxyz] [-w WIDTH] [-h HEIGHT] [-b BPP] [-r REFRESH] | [WIDTH HEIGHT BPP REFRESH]\n" );
	printf( " -a Show all possible matches (resolution not changed)\n" );
	printf( " -c Show closest match (resolution not changed)\n" );
	printf( " -M Mirroring on\n" );
	printf( " -m Mirroring off\n" );
	printf( " -v Verbose\n" );
	printf( " -x Show exact match (resolution not changed)\n" );
	printf( " -y Show and set highest possible resolution\n" );
	printf( " -z Show highest possible resolution (resolution not changed)\n" );
	printf( " No args default to 1024 768 32 75\n" );
	printf( " Values set with flags override values not set with flags (those last values are for legacy)\n" );
	exit(1);
}

int main(int argc, char **argv)
{
	CGDirectDisplayID displays[MAX_DISPLAYS];
	CGDisplayCount numDisplays;
	CGDisplayCount ii;
	CGDisplayErr err;
	int cc;
	int verbose = 0;
	int shouldFindHighest = 0;
	int shouldFindExact = 0;
	int shouldShowAll = 0;
	int shouldFindClosest = 0;
	int mirroringOnOff = 0;
	int shouldSetDisplay = 1;

	opterr = 0;

	myModeStruct.width = 1024;
	myModeStruct.height = 768;
	myModeStruct.bitsPerPixel = 32;
	myModeStruct.refresh = 75;

	while ((cc = getopt (argc, argv, "ab:ch:Mmr:vw:xyz")) != -1) {
		//printf ("Options %c\n", cc);
		switch (cc)
			{
			case 'w':
				myModeStruct.width = atoi(optarg);
				break;
			case 'h':
				myModeStruct.height = atoi(optarg);
				break;
			case 'b':
				myModeStruct.bitsPerPixel = atoi(optarg);
				break;
			case 'r':
				myModeStruct.refresh = atoi(optarg);
				break;
			case 'a':
				shouldShowAll = 1;
				shouldSetDisplay = 0;
				break;
			case 'c':
				shouldFindClosest = 1;
				shouldSetDisplay = 0;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'x':
				shouldFindExact = 1;
				shouldSetDisplay = 0;
				break;
			case 'y':
				shouldFindHighest = 1;
				break;
			case 'z':
				shouldFindHighest = 1;
				shouldSetDisplay = 0;
				break;
			case 'M':
				mirroringOnOff = 2;
				break;
			case 'm':
				mirroringOnOff = 1;
				break;
			case '?':
				usage();
				break;
			//default:
				//abort ();
			}
	}
	
	if ( argc > optind )
	{
		if ( argc == 4+optind )
		{
			myModeStruct.width = atoi(argv[0+optind]);
			myModeStruct.height = atoi(argv[1+optind]);
			myModeStruct.bitsPerPixel = atoi(argv[2+optind]);
			myModeStruct.refresh = atoi(argv[3+optind]);
		} else {
			usage();
		}
	}

	if ( verbose == 1 )
		printf( "Width: %d Height: %d BitsPerPixel: %d Refresh rate: %d\n", (int)myModeStruct.width, (int)myModeStruct.height, (int)myModeStruct.bitsPerPixel, (int)myModeStruct.refresh );

	//err = CGGetActiveDisplayList(MAX_DISPLAYS,
	err = CGGetOnlineDisplayList(MAX_DISPLAYS,
 								displays,
 								&numDisplays);
	if ( err != CGDisplayNoErr )
	{
		printf("Cannot get displays (%d)\n", err);
		exit( 1 );
	}

	if ( verbose == 1 )
		printf( "%d online display(s) found\n", (int)numDisplays );
		
	for (ii = 0; ii < numDisplays; ii++)
	{
		CGDisplayModeRef originalMode;
		if ( verbose == 1 && ! shouldShowAll )
			printf( "------------------------------------\n");
		originalMode = CGDisplayCopyDisplayMode( displays[ii] );
		if ( originalMode == NULL )
		{
			printf( "Display 0x%x is invalid\n", (unsigned int)displays[ii]);
			return 1;
		}
		if ( verbose == 1 )
			printf( "Display 0x%x\n", (unsigned int)displays[ii]);

		if ( shouldShowAll == 1 )
			allModesForDisplay( displays[ii], verbose );

		if ( shouldFindClosest == 1 )
			findClosestModeForDisplay( displays[ii], verbose );

		if ( shouldFindExact == 1 )
			findExactModeForDisplay( displays[ii], verbose );

		if ( shouldFindHighest == 1 )
			highestModeForDisplay( displays[ii], &myModeStruct, verbose );

		if ( shouldSetDisplay == 1 )
			setdisplay( displays[ii], mirroringOnOff, verbose );

	}
	exit(0);
}
