#ifndef INCLUDED_GLOBALS_H
#define INCLUDED_GLOBALS_H

#include <malloc.h>
#include <stdio.h>
#include <pspgu.h>
#include "libs/std.h"
#include "support.h"
#include "render.h"
#include "pic.h"

//This file is licensed under the GPL V2
#include "gpl.txt"

/* messageBox for the UI Thread to message the Network Thread */
extern SceUID networkMessagebox;

/* messageBox for the protocol thread (Telnet / IRC / SSH) */
extern SceUID protocolMessagebox;

#define protocolSend(sendtext, sender) {\
	ProtocolMsg* AMsg = malloc(sizeof(ProtocolMsg));\
	AMsg->from = sender;\
	AMsg->length = strlen(sendtext);\
	AMsg->text = strdup(sendtext);\
	sceKernelSendMbx(protocolMessagebox, AMsg);\
	}

#define protocolSendBin(senddata, len, sender) {\
	ProtocolMsg* AMsg = malloc(sizeof(ProtocolMsg));\
	AMsg->from = sender;\
	AMsg->length = len;\
	AMsg->text = malloc(len+1);\
	memcpy(AMsg->text, senddata, len);\
	AMsg->text[len] = '\0';\
	sceKernelSendMbx(protocolMessagebox, AMsg);\
	}
//the \0 extra byte is for irc, makes it easyer for text only protocols.

#define networkSend(sendtext) {\
		NetworkMsg* AMsg = malloc(sizeof(NetworkMsg));\
		AMsg->text = strdup(sendtext);\
		AMsg->length = strlen(AMsg->text);\
		sceKernelSendMbx(networkMessagebox, AMsg);\
	}

#define networkSendBin(senddata, len) {\
		NetworkMsg* AMsg = malloc(sizeof(NetworkMsg));\
		AMsg->text = malloc(len);\
		memcpy(AMsg->text, senddata, len);\
		AMsg->length = len;\
		sceKernelSendMbx(networkMessagebox, AMsg);\
	}

#define multiselect(x,y,picks,size,selected,message); {\
		while (!done)\
		{\
			char msBuffer[100];\
			SceCtrlData pad;\
			bool onepressed = false;\
			int loop;\
				/*Print out current selection*/\
			renderGoto(0,3);\
			renderMain(message"\r\n", COLOR_WHITE);\
			for (loop = 0; loop < size; loop++)\
			{\
				if (selected == loop)\
				{ sprintf(msBuffer,"> %s\r\n",picks[loop].name); renderMain(msBuffer, COLOR_WHITE); }\
				else\
				{ sprintf(msBuffer,"  %s\r\n",picks[loop].name); renderMain(msBuffer, COLOR_WHITE); }\
			}\
				/*now loop on input, let it fall through to redraw, if it is X then break*/\
			while (!onepressed)/*While havent pressed a key*/\
			{\
				sceCtrlReadBufferPositive(&pad, 1); \
				onepressed = ( (pad.Buttons & PSP_CTRL_CROSS) ||\
								(pad.Buttons & PSP_CTRL_UP) ||\
								(pad.Buttons & PSP_CTRL_DOWN));\
			}\
			/*Find the key and change based on it*/\
			if (pad.Buttons & PSP_CTRL_CROSS) done = true;\
			if (pad.Buttons & PSP_CTRL_UP) selected = (selected + size - 1) % size;\
			if (pad.Buttons & PSP_CTRL_DOWN) selected = (selected+1) % size;\
			while (onepressed)/*Wait for Key Release*/\
			{\
				sceCtrlReadBufferPositive(&pad, 1); \
				onepressed = ( (pad.Buttons & PSP_CTRL_CROSS) ||\
								(pad.Buttons & PSP_CTRL_UP) ||\
								(pad.Buttons & PSP_CTRL_DOWN));\
			}\
		}\
	}


/* Structures for the messages */
typedef struct _NetworkMsg {
	struct _MyMessage *link;	/* For internal use by the kernel */
	char* text;					/* Data to send */
	int length;					/* size of data */
} NetworkMsg;

typedef struct _RenderMsg {
	struct _RenderMsg *link;	/* For internal use by the kernel */
	char* text;					/* Text to Render */
	int color;					/* color of text */
	int bgcolor;				/* background color of text */
	int flags;					/* Extra flags to do funky things */
	int extra1;					/* Used for some flags */
	int extra2;					/* Used for some flags */
} RenderMsg;
/*	flags:
	0 -> default, print normally.
	1 -> reset screen (ignores any text sent)
	2 -> set x,y position to extra1,extra2
	3 -> Redraw the current frame
	4 -> Print a string at extra1,extra2 (ignores line wrap)
	5 -> move the screen up/down by extra1 pixels (positive = page moves up)
*/

typedef struct _ProtocolMsg {
	struct _RenderMsg *link;	/* For internal use by the kernel */
	char* text;					/* Text to Render */
	int length;					/* length of text field if from network*/
	int from;					/* who the msg is from */
} ProtocolMsg;
#define FROM_NETWORK 0
#define FROM_USER 1
#define FROM_USER_CODE 2 /* a keycode from the user, arrow keys etc are sent with this */
#define FROM_CONFIG 3

//Input thread types (TODO MOVE TO OWN FILE)
int charBasedInput(SceSize args, void *argp);
int lineBasedInput(SceSize args, void *argp);

#endif
