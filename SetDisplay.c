/*
gcc -arch i386 -O3 -o SetDisplay SetDisplay.c -framework Cocoa

SetDisplay.c

Requires Mac OS X 10.6 or later

This tool sets the display resolution.  This tool exists so that we
can set the display to a setting when the display manager says it can't,
mainly, when a Mac starts up and the monitor is off, or if the Mac is on
a KVM.

Copyright (c) 2014 The University of Utah
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
	double refresh;
} displayMode;

displayMode myModeStruct;

size_t displayBitsPerPixel( CGDisplayModeRef mode )
{
	size_t depth = 0;
	
	CFStringRef pixEnc = CGDisplayModeCopyPixelEncoding(mode);
	if(CFStringCompare(pixEnc, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		depth = 32;
	else if(CFStringCompare(pixEnc, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		depth = 16;
	else if(CFStringCompare(pixEnc, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		depth = 8;
	
	return depth;
}

static void printShortDispDesc( CGDisplayModeRef modeRef, int showOnlyAqua )
{
	int aqua;
	if ( CGDisplayModeGetIODisplayModeID(modeRef) )
		aqua = 1;
	else
		aqua = 0;
	size_t width = CGDisplayModeGetWidth(modeRef);
	size_t height = CGDisplayModeGetHeight(modeRef);
	size_t bpp = displayBitsPerPixel(modeRef);
	double refreshrate = CGDisplayModeGetRefreshRate(modeRef);
	if ( showOnlyAqua == 1 ) {
		if ( aqua == 1 )
			printf( "%zu %zu %zu %lg Usable\n", width, height, bpp, refreshrate );
	} else {
		printf( "%zu %zu %zu %lg %s\n", width, height, bpp, refreshrate, (aqua) ? "Usable" : "Nonusable" );
	}
}

/////////////////

static void allModesForDisplay( CGDirectDisplayID display, int verbose)
{
	CFIndex index, count;
	CFArrayRef dictModes;
	CGDisplayModeRef modeRef;
	dictModes = CGDisplayCopyAllDisplayModes (display, NULL);
	count = CFArrayGetCount (dictModes);
	printf( "------ All modes for display ------\n" );
	for (index = 0; index < count; index++)
	{
		modeRef = (CGDisplayModeRef)CFArrayGetValueAtIndex( dictModes, index );
		printShortDispDesc( modeRef, verbose );
	}
	printf( "-----------------------------------\n" );
}

CGDisplayModeRef modeForDisplay( CGDirectDisplayID display, int scanType, displayMode findMode )
{
	CGDisplayModeRef matchingModeRef;
	displayMode matchingModeStruct;
	matchingModeStruct.width = 0;
	matchingModeStruct.height = 0;
	matchingModeStruct.bitsPerPixel = 0;
	matchingModeStruct.refresh = 0;
	int d_width = INT_MAX/2;
	int d_height = INT_MAX/2;
	int d_bpp = INT_MAX;
	int d_refresh = INT_MAX;
	CFIndex index, count;
	CFArrayRef dictModes;
	CGDisplayModeRef modeRef;
	dictModes = CGDisplayCopyAllDisplayModes (display, NULL);
	count = CFArrayGetCount (dictModes);
	for (index = 0; index < count; index++)
	{
		modeRef = (CGDisplayModeRef)CFArrayGetValueAtIndex( dictModes, index );
		size_t width = CGDisplayModeGetWidth(modeRef);
		size_t height = CGDisplayModeGetHeight(modeRef);
		size_t bpp = displayBitsPerPixel(modeRef);
		double refreshrate = CGDisplayModeGetRefreshRate(modeRef);
		if ( scanType == 0 ) {
			// Exact
			if ( width == findMode.width && height == findMode.height && bpp == findMode.bitsPerPixel /* && refreshrate == findMode.refresh */ ) {
				matchingModeStruct.width = width;
				matchingModeStruct.height = height;
				matchingModeStruct.bitsPerPixel = bpp;
				matchingModeStruct.refresh = refreshrate;
				matchingModeRef = modeRef;
				index = count; // EXIT LOOP
			}
		} else if ( scanType == 1 ) {
			// Closest
			int dw = abs(width - findMode.width);
			int dh = abs(height - findMode.height);
			int db = abs(bpp - findMode.bitsPerPixel);
			int dr = abs(refreshrate - findMode.refresh);

//			printf( "%zu %zu %zu %lg\n", matchingModeStruct.width, matchingModeStruct.height, matchingModeStruct.bitsPerPixel, matchingModeStruct.refresh );

			if ( dw == d_width && dh == d_height ) {
				if ( db <= d_bpp && dr <= d_refresh ) {
					matchingModeStruct.bitsPerPixel = bpp;
					matchingModeStruct.refresh = refreshrate;
					d_width = dw;
					d_height = dh;
					d_bpp = db;
					d_refresh = dr;
					matchingModeRef = modeRef;
				}
			} else if ( dw + dh <= d_width + d_height ) {
				matchingModeStruct.width = width;
				matchingModeStruct.height = height;
				matchingModeStruct.bitsPerPixel = bpp;
				matchingModeStruct.refresh = refreshrate;
				d_width = dw;
				d_height = dh;
				d_bpp = db;
				d_refresh = dr;
				matchingModeRef = modeRef;
			}
		}
	}
	printf( "%zu %zu %zu %lg\n", matchingModeStruct.width, matchingModeStruct.height, matchingModeStruct.bitsPerPixel, matchingModeStruct.refresh );
	return matchingModeRef;
}

static void setdisplay( CGDirectDisplayID display, CGDisplayModeRef modeRef, int mirroringOnOff, int verbose )
{
	CGError err;
	CGDisplayConfigRef configRef;
	boolean_t exactMatch;
	CGBeginDisplayConfiguration(&configRef);

	err = CGConfigureDisplayWithDisplayMode( configRef, display, modeRef, NULL );

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
	printf( " -c Show closest match\n" );
	printf( " -M Mirroring on\n" );
	printf( " -m Mirroring off\n" );
	printf( " -n Do not change the resolution\n" );
	printf( " -v Verbose\n" );
	printf( " -x Show exact match\n" );
	printf( " -z Show highest possible resolution\n" );
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
	int shouldFindClosest = 1;
	int mirroringOnOff = 0;
	int shouldSetDisplay = 1;

	opterr = 0;

	myModeStruct.width = 1024;
	myModeStruct.height = 768;
	myModeStruct.bitsPerPixel = 32;
	myModeStruct.refresh = 75;

	while ((cc = getopt (argc, argv, "ab:ch:Mmnr:vw:xz")) != -1) {
		//printf ("Options %c\n", cc);
		switch (cc)
			{
			case 'a':
				shouldShowAll = 1;
				shouldSetDisplay = 0;
				break;
			case 'b':
				myModeStruct.bitsPerPixel = atoi(optarg);
				break;
			case 'c':
				shouldFindClosest = 1;
				break;
			case 'h':
				myModeStruct.height = atoi(optarg);
				break;
			case 'm':
				mirroringOnOff = 1;
				break;
			case 'M':
				mirroringOnOff = 2;
				break;
			case 'n':
				shouldSetDisplay = 0;
				break;
			case 'r':
				myModeStruct.refresh = atoi(optarg);
				break;
			case 'v':
				verbose = 1;
				break;
			case 'w':
				myModeStruct.width = atoi(optarg);
				break;
			case 'x':
				shouldFindExact = 1;
				shouldFindClosest = 0;
				break;
			case 'z':
				shouldFindHighest = 1;
				shouldFindClosest = 0;
				break;

			case '?':
				usage();
				break;
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
		printf( "Width: %zu Height: %zu BitsPerPixel: %zu Refresh rate: %lg\n", myModeStruct.width, myModeStruct.height, myModeStruct.bitsPerPixel, myModeStruct.refresh );

	//err = CGGetActiveDisplayList(MAX_DISPLAYS, displays, &numDisplays); // active only
	err = CGGetOnlineDisplayList(MAX_DISPLAYS, displays, &numDisplays); // active, mirrored, or sleeping
	if ( err != CGDisplayNoErr )
	{
		printf("Cannot get displays (%d)\n", err);
		exit( 1 );
	}

	if ( verbose == 1 )
		printf( "%d online display(s) found\n", (int)numDisplays );

	CGDisplayModeRef modeRef;
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

		if ( shouldShowAll == 1 ) {

			allModesForDisplay( displays[ii], verbose );

		} else {

			if ( shouldFindExact == 1 ) {

				printf( "------ Exact mode for display -----\n" );
				modeRef = modeForDisplay( displays[ii], 0, myModeStruct );
				printf( "-----------------------------------\n" );

			} else if ( shouldFindHighest == 1 ) {

				printf( "----- Highest mode for display ----\n" );
				myModeStruct.width = INT_MAX;
				myModeStruct.height = INT_MAX;
				myModeStruct.bitsPerPixel = INT_MAX;
				myModeStruct.refresh = INT_MAX;
				modeRef = modeForDisplay( displays[ii], 1, myModeStruct );
				printf( "-----------------------------------\n" );

			} else if ( shouldFindClosest == 1 ) {

				printf( "----- Closest mode for display ----\n" );
				modeRef = modeForDisplay( displays[ii], 1, myModeStruct );
				printf( "-----------------------------------\n" );

			}

			if ( shouldSetDisplay == 1 )
				setdisplay( displays[ii], modeRef, mirroringOnOff, verbose );

		}

	}
	exit(0);
}
