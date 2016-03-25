/*
	Hello World example made by Aurelio Mannara for ctrulib
	This code was modified for the last time on: 12/12/2014 21:00 UTC+1
*/

#ifndef NO_3DS
#include <3ds.h>
#else
#include <stdint.h>
typedef uint32_t u32;
#endif

#include <cstdio>
#include <iostream>

#include <stdlib.h>
#include <math.h>

extern "C" {

#include "../test.h"

extern "C" char *dtoa(double d, int mode, int ndigits,
			int *decpt, int *sign, char **rve);

char* _dtoa_r(struct _reent *ptr,
	double _d,
	int mode,
	int ndigits,
	int *decpt,
	int *sign,
	char **rve);

}

int main(int argc, char **argv) {
#ifndef NO_3DS
	gfxInitDefault();

	//Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
	consoleInit(GFX_TOP, NULL);
#else
#warning Compiling with NO_3DS!
#endif

printf("Hello!\n");

#if 0

	//Move the cursor to row 15 and column 19 and then prints "Hello World!"
	//To move the cursor you have to print "\x1b[r;cH", where r and c are respectively
	//the row and column where you want your cursor to move
	//The top screen has 30 rows and 50 columns
	//The bottom screen has 30 rows and 40 columns
	printf("\x1b[15;19HHello World!\n\n");

#endif

//test();
//while(1);

int mode = 0;
int ndigits = 2;
int decpt;
int sign;
char* buffer = (char*)malloc(128);

#ifndef NO_3DS
{

#if 0
static struct _reent re;

buffer = _dtoa_r(&re, 0.4, mode, ndigits, &decpt, &sign, &buffer);
printf("Got '%s'\n", buffer);

buffer = _dtoa_r(&re, 0.2, mode, ndigits, &decpt, &sign, &buffer);
printf("Got '%s'\n", buffer);
#endif

}
#endif

buffer = dtoa(0.2, mode, ndigits, &decpt, &sign, &buffer);
printf("Got '%s' (decpt: %d)\n", buffer, decpt);
while(1);

Bigint Sd;
Sd.wds = 1;
Sd.x[0] = 0xF00;
Bigint* S = &Sd;

int s5 = 0x7FFFFFFF;
S = pow5mult_nl (S, s5);

printf("Bye!");
while(1);

asm("nop\nnop\nnop\nnop\n");
while(1) {
  float sf = 1.0f;
  u32 f = *(u32*)&sf;
  for(unsigned int i = 0; i < 32; i++) {
    f = 1 << i;
    printf("%d: W %f\n", i, *(float*)&f);
  }
}

while(1);

#if 0

#if 0
  std::cout << "Works! " << 0.4f << std::endl;
  std::cout << "Crash! " << 0.3f << std::endl;
#else
//  printf("Works! %f", 0.4f);
//  printf("Crash! %f", 0.1375f);
#endif

	printf("\x1b[29;11HPress Start to exit.");

	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_START) break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}

	gfxExit();
#endif
	return 0;
}
