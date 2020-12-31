// WiFi Simple test app .03

//This file is licensed under the GPL V2
#include "gpl.txt"

#include "global.h"

#include <pspwlan.h>
#include <psppower.h>
#include "libs/nlh.h" // net lib helper
#include "libs/p_sprint.h"
#include "protocol/protocols.h"

//////////////////////////////////////////////////////////////////////

//NOTE: kernel mode module flag and kernel mode thread are both required
PSP_MODULE_INFO("PELDET", 0x1000, 1, 1); /* 0x1000 REQUIRED!!! */
PSP_MAIN_THREAD_ATTR(0); /* 0 REQUIRED!!! */
PSP_MAIN_THREAD_STACK_SIZE_KB(32); /* smaller stack for kernel thread */

/* messageBox for the UI Thread to message the Network Thread */
SceUID networkMessagebox;
/* messageBox for the networkProtocol thread (Telnet / IRC / SSH) */
SceUID protocolMessagebox;

int user_main(SceSize args, void* argp);
void networkThread(const char* szMyIPAddr);

//Function that returns a line of input up to 41 chars long
char* getSingleLine();

//Bits for resolver
int  m_ResolverId;
char m_ResolverBuffer[1024]; /** Could be smaller, no idea */


/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
	sceKernelExitGame();

	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();
	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x10, 0xFA0, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}
	
	return thid;
}


void initScreen()
{
	/* init the p-sprint main screen */
//	pspDebugScreenInit();
	drawLine(0,242,479,242,0x00000FFF);
}

PspDebugStackTrace st[3];

/* Example custom exception handler */
void MyExceptionHandler(PspDebugRegBlock *regs)
{
	/* Do normal initial dump, setup screen etc */
	pspDebugScreenInit();

	/* I always felt BSODs were more interesting that white on black */
	pspDebugScreenSetBackColor(0x00FF0000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	pspDebugScreenPrintf("Exception Details:\n");
	pspDebugDumpException(regs);
	pspDebugScreenPrintf("\nStack Trace:\n");
	
	int size = pspDebugGetStackTrace2(regs, st, 3);
	int a;
	for (a = 0; a < size; a++)
	{
		printf("%i)  %x | %x\n", a, st[a].call_addr, st[a].func_addr);
	}
	sceKernelDelayThread(10*1000*1000);	// 10sec
}

int main(void)
{
	// Kernel mode thread
	pspDebugScreenInit();
	

	if (nlhLoadDrivers(&module_info) != 0)
	{
		printf("Driver load error\n");
		return 0;
	}
	pspDebugInstallErrorHandler(MyExceptionHandler);
//	scePowerSetClockFrequency(333, 333, 166);
	
	// create user thread, tweek stack size here if necessary
	SceUID thid = sceKernelCreateThread("User Mode Thread", user_main,
			0x11, // default priority
			128 * 1024, // stack size (256KB is regular default) HALVED
			PSP_THREAD_ATTR_USER, NULL);

	// start user thread, then wait for it to do everything else
	sceKernelStartThread(thid, 0, NULL);
	sceKernelWaitThreadEnd(thid, NULL);
	
	sceKernelExitGame();
	return 0;
}

//////////////////////////////////////////////////////////////////////


int user_main(SceSize args, void* argp)
{
	// user mode thread does all the real work
	u32 err;
	int connectionConfig = -1;
	char buffer[200];
	SetupCallbacks(); //This must be ran from a userspace thread, not sure why :O
	sceKernelDelayThread(100000); // Give the Thread time to run (0.1Sec)
	
	//Initialize the Gu
	sceGuInit();
	sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);
	sceGuDispBuffer(480,272,(void*)0x88000,512);
	sceGuDepthBuffer((void*)0x110000,512);
	sceGuOffset(2048 - (480/2),2048 - (272/2));
	sceGuViewport(2048,2048,480,272);
	sceGuDepthRange(0xc350,0x2710);
	sceGuScissor(0,0,480,272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(0,0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	
	//Prepare Mailboxes for communication between threads
	networkMessagebox = sceKernelCreateMbx("NT-MB", 0, 0);
//	renderMessagebox = sceKernelCreateMbx("UI-MB", 0, 0);
	protocolMessagebox = sceKernelCreateMbx("PT-MB", 0, 0);
	
	//Boot the render thread, TODO: possibly change the priority, it seems fine though...	
//	SceUID renderthid = sceKernelCreateThread("Render Thread", renderThread, 0x11, 128*1024, PSP_THREAD_ATTR_USER, NULL);
//	sceKernelStartThread(renderthid, 0, NULL);
	renderInit();
	
	renderMain("peldet (PSP Telnet and IRC)\r\n", COLOR_WHITE);
	renderMain("Coded by Danzel, Using Wifi code from PSPPet and P-Sprint from Arwin\r\n", COLOR_WHITE);
	renderMain("\r\n", COLOR_WHITE);
		
	// nlhInit must be called from user thread for DHCP to work
	err = nlhInit();
	if (err != 0)
	{
		renderMain("WiFi Init error\r\n", COLOR_WHITE);
		sprintf(buffer,"nlhInit returns $%x\r\n", err);
		renderMain(buffer, COLOR_WHITE);
		renderMain("Please check Network Settings\r\n", COLOR_WHITE);
		sceKernelDelayThread(10*1000000); // 10sec to read before exit
		goto close_net;
	}
	while (sceWlanGetSwitchState() != 1)	//switch = off
	{
		renderMain("Turn the wifi switch on!\r\n", COLOR_RED);
		sceKernelDelayThread(1000*1000);
	}
	
	// enumerate connections
#define MAX_PICK 10
	struct
	{
		int index;
		char name[64];
	} picks[MAX_PICK];
	int pick_count = 0;

	int iNetIndex;
	for (iNetIndex = 1; iNetIndex < 100; iNetIndex++) // skip the 0th connection
	{
		if (sceUtilityCheckNetParam(iNetIndex) != 0)
			break;  // no more
		sceUtilityGetNetParam(iNetIndex, 0, picks[pick_count].name);
		picks[pick_count].index = iNetIndex;
		pick_count++;
		if (pick_count >= MAX_PICK)
			break;  // no more room
	}

	if (pick_count == 0)
	{
		renderMain("No connections\r\n", COLOR_WHITE);
		renderMain("Please try Network Settings\r\n", COLOR_WHITE);
		sceKernelDelayThread(10*1000000); // 10sec to read before exit
		goto close_net;
	}

	sprintf(buffer,"Found %d possible connections\r\n", pick_count);
	renderMain(buffer, COLOR_WHITE);
	iNetIndex = 0;
	if (pick_count > 1)
	{
		bool done = 0;
		
		sceCtrlSetSamplingCycle(0);
		sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

		multiselect(0,3,picks,pick_count,iNetIndex,"Choose a connection and press X");
	}

	connectionConfig = picks[iNetIndex].index;
	
	// Connect
	sprintf(buffer, "using connection #%d '%s'\r\n", connectionConfig, picks[iNetIndex].name);
	renderMain(buffer, COLOR_WHITE);
	
connectWifi:
	err = sceNetApctlConnect(connectionConfig);
	if (err != 0)
	{
		sprintf(buffer, "sceNetApctlConnect returns $%x\r\n", err);
		renderMain(buffer, COLOR_RED);
		sceKernelDelayThread(4*1000000); // 4sec to read before exit
		goto close_net;
	}

	// Report status while waiting for connection to access point
	int stateLast = -1;
	renderMain("Connecting To Wifi...\r\n", COLOR_WHITE);
	while (1)
	{
		int state;
		err = sceNetApctlGetState(&state);
		if (err != 0)
		{
			sprintf(buffer, "sceNetApctlGetState returns $%x\r\n", err);
			renderMain(buffer, COLOR_WHITE);
			sceKernelDelayThread(10*1000000); // 10sec to read before exit
			goto close_connection;
		}
		if (state != stateLast)
		{
			if (stateLast == 2 && state == 0)
			{
				renderMain("  Connecting to wifi Failed, Retrying...\r\n", COLOR_WHITE);
				sceKernelDelayThread(500*1000); // 500ms
				goto connectWifi;
			}
			sprintf(buffer, "  connection state %d of 4\r\n", state);
			renderMain(buffer, COLOR_WHITE);
			stateLast = state;
		}
		if (state == 4)
			break;  // connected

		// wait a little before polling again
		sceKernelDelayThread(50*1000); // 50ms
	}

	// connected, get my IPADDR and run telnet
	char szMyIPAddr[32];
	if (sceNetApctlGetInfo(8, szMyIPAddr) != 0)
		strcpy(szMyIPAddr, "unknown IP address");
	networkThread(szMyIPAddr);

	// all done

close_connection:
	err = sceNetApctlDisconnect();
	if (err != 0)
	{
		sprintf(buffer, "sceNetApctlDisconnect returns $%x\r\n", err);
		renderMain(buffer, COLOR_RED);
	}

close_net:

	nlhTerm();

	renderMain("Program exiting\r\n", COLOR_RED);
	sceKernelDelayThread(2*1000000); // 2sec to read any final status

	return 0;
}

struct sockaddr_in stringTosockaddr(char* input)
{
	struct sockaddr_in addrListen;
	
	addrListen.sin_family = AF_INET;
	addrListen.sin_addr[0] = 0;
	addrListen.sin_addr[1] = 0;
	addrListen.sin_addr[2] = 0;
	addrListen.sin_addr[3] = 0;
	addrListen.sin_port = 0;


	renderMain("Chose:", COLOR_RED);
	renderMain(input, COLOR_RED);
	renderMain("\r\n", COLOR_RED);
	
	bool foundcolon = 0;
	int colonpos = 0;
	for (;colonpos < strlen(input); colonpos++)
	{
		if (input[colonpos] == ':')
		{
			foundcolon = 1;
			break;
		}
	}
	if (!foundcolon)
	{
		renderMain("Unable to find a colon, did you forget the port!\r\n", COLOR_RED);
		renderMain("Gonna crash out now...\r\n", COLOR_RED);
		return addrListen;
	}
	
	//temporarily make colonpos = '\0'
	input[colonpos] = '\0';
	
	struct in_addr addr;

	/* RC Let's try aton first in case the address is in dotted numerical form */
	memset(&addr, 0, sizeof(struct in_addr));
	int rc = sceNetInetInetAton(input, &addr);
	if (rc == 0)
	{
		/* That didn't work!, it must be a hostname, let's try the resolver... */
		renderMain("Resolving Host\r\n", COLOR_WHITE);
		rc = sceNetResolverStartNtoA(m_ResolverId, input, &addr, 2, 3);
		if (rc < 0)
		{
			renderMain("Could not resolve host!\r\n", COLOR_RED);
			return addrListen;
		}
	}
	
	memcpy(&addrListen.sin_addr, &addr, sizeof(struct in_addr));
	
	input[colonpos] = ':';
	//now parse out the port after the :
	colonpos++;
	while( input[colonpos] >= '0' && input[colonpos] <= '9')
	{
		addrListen.sin_port *= 10;
		addrListen.sin_port += input[colonpos] - '0';
		colonpos++;
	}
	
	addrListen.sin_port = htons(addrListen.sin_port);
	
	return addrListen;
}

//Ask the user for an address to connect to and return a corresponding sockaddr_in
struct sockaddr_in getConnectionFromUser()
{
	struct sockaddr_in addrListen;

	#define MAX_FAVORITES 10
	//Show Favorites and allow Selection
	struct
	{
		char addr[100];	//Hopefully these limits are okay, really we should be reallocing to get them
		char name[100];
		char* extra;	//FREE THIS LATER PLZ! TODO
		int protocol;
	} picks[MAX_FAVORITES];
	
	int pick_count = 0;
	int picked = 0;
	bool done = 0;
	FILE* fd;
	
	sprintf(picks[0].name,"Manually Enter Details");
	pick_count++;
	
	fd = fopen("ms0:/peldetfav.txt","r");
	if(fd == 0)
	{
		renderMain("No Favorites Found", COLOR_WHITE);
		picked = 0;
	}
	else
	{
		int x = 1;
		for (;x< MAX_FAVORITES && (!feof(fd)); x++)
		{
			char inBuffer[400];
			char* resget = fgets( inBuffer, 400-1, fd);	//Grab a line
			
			if (strlen(inBuffer) <2 || resget==0) break; //HACK
			
			//Grab the bit before the space (Address hopefully)
			char* firstspace = strchr(inBuffer, ' ');
			if (firstspace == NULL)
			{
				renderMain("FIRSTSPACE", COLOR_RED);
				goto badfavorite;
			}
			
			firstspace[0] = '\0';
			sprintf(picks[pick_count].addr,"%s", inBuffer);
			
			//Now get the second string, protocol 
			char* secondspace = strchr(firstspace+1, ' ');
			if (secondspace == NULL)
			{
				renderMain("SECONDSPACE", COLOR_RED);
				goto badfavorite;
			}
			secondspace[0] = '\0';
			
			//Find the protocol matching the string
			int a;
			bool found = false;
			for (a = 0; a < protocols_count; a++)
			{
				if (!strcmp(protocols[a].name, firstspace+1))//found it
				//Take copy of the extra string with the length depicted by the protocol
				{
					//keep the protocol array pos
					picks[pick_count].protocol = a;
					char* spacepos = secondspace;
					found = true;
					
					//advance through that many spaces
					if (protocols[a].stringCount > 0)
					{
						int ppos;
						for (ppos = 0; ppos < protocols[a].stringCount; ppos++)
						{
							spacepos = strchr(spacepos+1, ' ');
							if (spacepos == NULL)
							{
								renderMain("NULLINSPACEPOS", COLOR_RED);
								goto badfavorite;
							}
						}
						spacepos[0] = '\0';
						picks[pick_count].extra = strdup(secondspace+1);
//						sprintf(picks[x].name, "%i %s", spacepos-inBuffer, secondspace+1);
						spacepos[0] = ' ';
						
						//need to update secondspace to the end of the area :)
						secondspace = spacepos;
					}
					else
						picks[pick_count].extra = strdup("");

				}
			}
			if (!found)
			{
				renderMain("NOTFOUNDPROTO", COLOR_RED);
				goto badfavorite;
			}
			
			//And the bit after the space goes into name
//			sprintf(picks[pick_count].name, "%s|%s", picks[x].extra, secondspace+1);
			sprintf(picks[pick_count].name, "%s", secondspace+1);
			
			//Strip any \r\n from the end ;)
			char* rembr = strchr(picks[pick_count].name,'\r');
			if (rembr != NULL) rembr[0] = '\0';
			rembr = strchr(picks[pick_count].name,'\n');
			if (rembr != NULL) rembr[0] = '\0';
			
			pick_count++; //Add one;
			
			continue;
			
			badfavorite:
			{
				renderMain("BAD FAVORITE: ", COLOR_RED);
				renderMain(inBuffer, COLOR_WHITE);
				renderMain("\r\n", COLOR_RED);
				continue;
			}
		}
		fclose(fd);
		
		multiselect(0,3,picks,pick_count,picked, "Choose a favorite to connect to and press X");
	}
	if (picked != 0)
	{
		addrListen = stringTosockaddr(picks[picked].addr);
		
		//Launch the protocol thread for that favorite
		SceUID protocolthid = sceKernelCreateThread("Protocol Thread", protocols[picks[picked].protocol].protocolMain, 0x11, 128*1024, PSP_THREAD_ATTR_USER, NULL);
		sceKernelStartThread(protocolthid, 0, NULL);
		
		// fire up its interface Thread (TODO: priority fine?)
		SceUID interfaceThid = sceKernelCreateThread("interfaceThread", protocols[picks[picked].protocol].inputThread, 11, 8192, THREAD_ATTR_USER, 0);
		sceKernelStartThread(interfaceThid, 0, NULL);
		
		
		//Now send the extra bits in a message to the protocol.
		//we should send them in the sceKernelStartThread call but I cant get it to go yet...
		protocolSend(picks[picked].extra, FROM_CONFIG);
		return addrListen;
	}
	//If new then...
	renderMain("Input the ip and port to connect to in the form AAA.BBB.CCC.DDD:PORT\r\nor some.web.address.com:PORT\r\n", COLOR_WHITE);
	
	/* main loop waiting for input from pad*/
	while(1)
	{
		char* inputStr = getSingleLine();
		if (pic_isEnabled())
		{
			pic_disable();
		}
				
		addrListen = stringTosockaddr(inputStr);
		p_spSetActiveGroup(P_SP_KEYGROUP_DEFAULT);
		
		//TODO Get Protocol From user (need to get strings too)
		int selected = 0;
		
		//Now launch that protocol
		SceUID protocolthid = sceKernelCreateThread("Protocol Thread", protocols[selected].protocolMain, 0x11, 128*1024, PSP_THREAD_ATTR_USER, NULL);
		sceKernelStartThread(protocolthid, 0, NULL);

		// fire up its interface Thread (TODO: priority fine?)
		SceUID interfaceThid = sceKernelCreateThread("interfaceThread", protocols[selected].inputThread, 11, 8192, THREAD_ATTR_USER, 0);
		sceKernelStartThread(interfaceThid, 0, NULL);

		
		return addrListen;
	}

	//UM DAMN HOW DID WE GET HERE
	return addrListen;
}


void networkThread(const char* szMyIPAddr)
{
	u32 err;
//	SceUID interfaceThid; //Thread ID of the Interface
	char buffer[200];
	
	renderReset();
	renderMain("Connected to Wifi! Where shall we Connect to?\r\n", COLOR_WHITE);
	renderMain("\r\n", COLOR_WHITE);

	{
		//Create a Socket
		SOCKET sockListen;
		struct sockaddr_in addrListen;
		int error;

		sockListen = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
		if (sockListen <= 0)
		{
			sprintf(buffer, "socket returned $%x\r\n", sockListen);
			renderMain(buffer, COLOR_RED);
			goto done;
		}

		int rc = sceNetResolverCreate(&m_ResolverId, m_ResolverBuffer, sizeof(m_ResolverBuffer));
		if (rc < 0)
		{
			sprintf(buffer, "sceNetResolverCreate $%x\r\n", rc);
			renderMain(buffer, COLOR_RED);
			goto done;
		}
		
			//Place we are connecting :)
		
		//Get CON IP From User
		addrListen = getConnectionFromUser();
		if (addrListen.sin_port == 0)	//user gave invalid details
		{
			goto done;
		}
			
		sprintf(buffer, "Connecting to: %d %d %d %d : %d\r\n", addrListen.sin_addr[0], addrListen.sin_addr[1], addrListen.sin_addr[2], addrListen.sin_addr[3], htons(addrListen.sin_port));
		renderMain(buffer, COLOR_WHITE);
		
		err = sceNetInetConnect(sockListen, &addrListen, sizeof(addrListen));
		if (err != 0)
		{
			renderMain("Unable to connect!\r\n", COLOR_RED);
			
			switch(sceNetInetGetErrno())
			{
			case 0x74:
				renderMain("  Connection Timed out\r\n", COLOR_RED);
				break;
			case 0x6f:
				renderMain("  Connection Refused\r\n", COLOR_RED);
				break;
			default:
				sprintf(buffer, "connect returned $%x\r\n", err);
				renderMain(buffer, COLOR_RED);
				sprintf(buffer, "  errno=$%x\r\n", sceNetInetGetErrno());
				renderMain(buffer, COLOR_RED);
			}
			goto done;
		}
		
		//disable nonblock maybe
		//This bit thanks to XOSs pspvnc :) you guys rock.
		#define IPPROTO_TCP     6
		#define TCP_NODELAY     0x0001
		u32 one = 1;
	    sceNetInetSetsockopt(sockListen, IPPROTO_TCP, TCP_NODELAY,(char *)&one, sizeof(one));
		
		//Alter Socket options to have a timeout
		u32 timeout = 1000000; // in microseconds == 1 sec
		err = sceNetInetSetsockopt(sockListen, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
		
		
		if (err != 0) //Failure
		{
			renderMain("set SO_RCVTIMEO failed\r\n", COLOR_RED);
			u32 data;
			int len;
			err = sceNetInetGetsockopt(sockListen, SOL_SOCKET, SO_RCVTIMEO,
				(char*)&data, &len);
			if (err == 0 && len == 4)
			{
				sprintf(buffer,"Get SO_RCVTIMEO = %d ($%x)\r\n", data, data);
				renderMain(buffer, COLOR_WHITE);
			}
		}
		
		//We are all Connected, 
		
		
		
		while (1)
		{
				//Deal with Messages in the Box
			NetworkMsg* data;
			error = sceKernelPollMbx(networkMessagebox, (void*)&data);
			if(error < 0) {
				/* Nothing Arived */
			} else {
				//Received a Message
				//Send it over the Network.
				int j;
				j = sceNetInetSend(sockListen, data->text, data->length, 0);
				
				if (j < 0) renderMain("SEND FAILED!!!!!!!!!\r\n", COLOR_RED);
				
				free(data->text);
				free(data);
			}

			//Receive anything on the Socket
			char buffer[10000];
			int cch;

			cch = sceNetInetRecv(sockListen, (u8*)buffer, sizeof(buffer)-1, 0);
			if (cch == 0)
				break;      // connection closed

			if (cch < 0)
			{
				int errno = sceNetInetGetErrno();
				if (errno == 11)
				{
					// recv timeout, have a sleep
					sceKernelDelayThread(1000);
				}
			}
			else
			{
				//successfull receive
				buffer[cch] = '\0';
				
				protocolSendBin(buffer, cch, FROM_NETWORK);
			}
			
			if (sceWlanDevIsPowerOn() != 1) //power is off
			{
				renderMain("WLAN has lost power, psp went to sleep?\r\n", COLOR_RED);
				break;
			}
			
		}
		
		//Connection is lost
		
		renderMode(RENDER_SLOW); //reset incase protocol changed it
		
		//Post a message to the interface
		renderMain("Connection Closed\r\n", COLOR_RED);
		
		err = sceNetInetClose(sockListen);
		if (err != 0)
		{
			sprintf(buffer,"closesocket returned $%x\n", err);
			renderMain(buffer, COLOR_RED);
		}
		
		//Wait for the message to be processed
/*		SceKernelMbxInfo info;		HACK HACK HACK, do we still need this
		info.size = sizeof(info);
		do
		{
			sceKernelDelayThread(3000);
			sceKernelReferMbxStatus(renderMessagebox, &info);
		} while (info.numMessages > 0);
	*/	
		//now give the user a 2 seconds to read
		sceKernelDelayThread(2*1000000);

done:
		//Try close the socket incase it isnt
		err = sceNetInetClose(sockListen);
		
	}
}



int lineBasedInput(SceSize args, void *argp)
{
	renderReset();
	initScreen();

	/* main loop waiting for input from pad*/
	while (1)
	{
		char* inputStr = getSingleLine();
		protocolSend(inputStr, FROM_USER);
		
		//Clear the input area..
		renderPutString(0,26,0x0FFFFFFF,"                                        ");
		
		//Clear the input string
		free(inputStr);
	}
	return 0;
}


//Blocking Call that gets a single line of input from the user.
//Maximum of 40 characters input.
char* getSingleLine()
{
	int inputChar = 0;
	char inputStr[41];
	struct p_sp_Key myKey;
	int prevgroup = 1;
	int iBlink = 0;
	
	inputStr[0] = '\0';
	
	while(1)
	{
		//See if we should redraw the pic
		SceCtrlData pad;
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_RTRIGGER)
		{
			//Flip the Screen
			if (pic_isEnabled())
				pic_disable();
			else
				pic_enable();
			
			//Spinlock until released HACK
			while (pad.Buttons & PSP_CTRL_RTRIGGER)
			{
				sceCtrlReadBufferPositive(&pad, 1);
				sceKernelDelayThread(5000);//5ms
			}
		}
		//Move the screens
		#define DEADZONESIZE 8
		#define MODIFYER 10
		if (pad.Ly > (128+DEADZONESIZE))
		{
			renderMove((pad.Ly-128)/MODIFYER);
		}
		else if (pad.Ly < (128-DEADZONESIZE))
		{
			renderMove((pad.Ly-128)/MODIFYER);
		}
		
		//We dont want p_sprint to know if L/R are pressed as we use them.
		pad.Buttons &= ~(PSP_CTRL_RTRIGGER | PSP_CTRL_LTRIGGER);
		
		if(p_spReadKeyEx(&myKey, pad.Buttons)) //If we read a letter
		{
			//Special Case Keys
			if (myKey.keychar == 13) //Enter
			{
				//Return the Inputted String
				return strdup(inputStr);
			}
			else if (myKey.keychar == 8) //BackSpace
			{
				if (inputChar != 0)
				{
					inputChar--;
					inputStr[inputChar] = '\0';
					renderPutString(inputChar,26,COLOR_WHITE,"  ");
				}
			}
			else //Normal char (Todo - Other special chars?)
			{
				if (inputChar != 40 && myKey.keychar != 0)
				{
					//Add the character to our buffer and draw it on the screens!
					inputStr[inputChar] = myKey.keychar;
					
					char str[3];
					str[0] = myKey.keychar;
					str[1] = ' ';
					str[2] = '\0';
					renderPutString(inputChar, 26, COLOR_WHITE, str);
					inputChar++;
					inputStr[inputChar] = '\0';
				}
			}
		}
		
		if(myKey.keygroup!=prevgroup)//Changing KeyGroup
		{
			char* keyGroupNames[] = {
				"Alphabet        ",
				"Numbers & F-Keys",
				"Control keys    ",
				"Custom 1        ",
				"Custom 2        ",
				"Custom 3        "
				};
			if (myKey.keygroup >= 0 && myKey.keygroup <= 5)
				renderPutString(50,26,COLOR_WHITE,keyGroupNames[myKey.keygroup]);
			prevgroup = myKey.keygroup;
		} 
		
		iBlink++;
		if(iBlink<20)
		{
			renderPutString(inputChar,26,COLOR_WHITE,"_");
		}
		else if(iBlink<40)
		{
			renderPutString(inputChar,26,COLOR_WHITE," ");
		}
		else
		{
			iBlink=0;
		}
		sceKernelDelayThread(3000);
	}
	
	//CANT REACH HERE
	return "";
}


/*
	An input thread for getting single characters at a time.
	current assumes that the renderer sets mode to fast (as telnet does)
*/
int charBasedInput(SceSize args, void *argp)
{
	struct p_sp_Key myKey;
	int prevgroup = 1;
	
	renderReset();
	initScreen();
	
	
	while(1)
	{
		//See if we should redraw the pic
		SceCtrlData pad;
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_RTRIGGER)
		{
			//Flip the Screen
			if (pic_isEnabled())
				pic_disable();
			else
				pic_enable();
			
			//Spinlock until released HACK
			while (pad.Buttons & PSP_CTRL_RTRIGGER)
			{
				sceCtrlReadBufferPositive(&pad, 1);
				sceKernelDelayThread(5000);//5ms
			}
		}
		//Move the screens
		#define DEADZONESIZE 8
		#define MODIFYER 10
		if (pad.Ly > (128+DEADZONESIZE))
		{
			renderMove((pad.Ly-128)/MODIFYER);
		}
		else if (pad.Ly < (128-DEADZONESIZE))
		{
			renderMove((pad.Ly-128)/MODIFYER);
		}
		
		
		//We dont want p_sprint to know if L/R are pressed as we use them.
		pad.Buttons &= ~(PSP_CTRL_RTRIGGER | PSP_CTRL_LTRIGGER);
		
		if(p_spReadKeyEx(&myKey, pad.Buttons)) //If we read a letter
		{
			if (myKey.keychar == 0)
			{
/*				char buffer[10];
				sprintf(buffer, "::%i::", myKey.keycode);
				renderMain(buffer, COLOR_WHITE);*/
				char send[1];
				send[0] = myKey.keycode;
				//send to protocol
				protocolSendBin(send, 1, FROM_USER_CODE);

			}
			else
			{
				char send[1];
				send[0] = myKey.keychar;
				//send to protocol
				protocolSendBin(send, 1, FROM_USER);
			}
		}
		
		if(myKey.keygroup!=prevgroup)//Changing KeyGroup
		{
			char* keyGroupNames[] = {
				"Alphabet        ",
				"Numbers & F-Keys",
				"Control keys    ",
				"Custom 1        ",
				"Custom 2        ",
				"Custom 3        "
				};
			if (myKey.keygroup >= 0 && myKey.keygroup <= 5)
			{
				renderPutString(50,26,COLOR_WHITE,keyGroupNames[myKey.keygroup]);
				renderRedraw();
			}
			prevgroup = myKey.keygroup;
		} 
		sceKernelDelayThread(3000);
	}
	
	//CANT REACH HERE
	return 0;
}
