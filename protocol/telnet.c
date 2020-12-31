#include "../global.h"
#include "telnet.h"
 
 
//process text and render it like a vt100 (also includes some telnet things)
void vt100Render(char* inputStr, int length);

//render text using current colors and text attributes
void styledRender(char* text);
void styledClear(int clearType); //calls renderClear with correct color

void handleEscSequence(char inputChr);

void handleWill(char inputChr);
void handleWont(char inputChr);
void handleDo(char inputChr);
void handleDont(char inputChr);
void handleSuboption(char inputChr);
void handleSuboptionEnd();

//For use with attributes from a telnet ^[...m sequence
void setDisplayAttribute(int attr);

bool wontEcho = false;


bool cygwinHacksMode = false;	//if we detect connecting to cygwin this is set
//if this is set then backspace is sent of \b instead of \177. other telnet clients dont need to do this but i do :(

//colors and attributes of text
int textColor;
int bgColor;
bool attrReverse = false;
int brightness = BRIGHTNESS_NORMAL;

//what the fonts are set as and what one we are currently using
bool G0_font = FONT_DEFAULT;
bool G1_font = FONT_LINEDRAW;
bool using_gX = 0;

//saved ones (from an esc sequence)
int  saved_textColor;
int  saved_bgColor;
bool saved_attrReverse;
int  saved_brightness;
bool saved_G0_font;
bool saved_G1_font;
bool saved_using_gX;


//cache for replies to network
char networkBuf[200];
int networkBufSize = 0;

void networkBufAdd(char* data, int len)
{
	int a;
	for (a = 0; a < len; a++)
	{
		networkBuf[a+networkBufSize] = data[a];
	}
	networkBufSize += len;
}

//input set things
#define APPLICATION_MODE false /* for both */

#define POSITIONING_MODE true
bool cursorMode = POSITIONING_MODE;

#define NUMERIC_MODE true
bool keypadMode = NUMERIC_MODE;
/*
 *  ESC [ ? 1 h       cursor keys in applications mode
 *  ESC [ ? 1 l       cursor keys in cursor positioning mode
 *  ESC =             keypad keys in applications mode
 *  ESC >             keypad keys in numeric mode
*/

bool remoteEcho = false;	//if true the remote computer will echo back input, so we dont need to


/* A thread which implements the telnet protocol */
int telnetProtocol(SceSize args, void *argp)
{
	ProtocolMsg* data;
	int error;
	
	//send initial hello type message
	char testset[100];
	sprintf(testset, "%c%c%c" "%c%c%c" "%c%c%c" "%c%c%c" "%c%c%c" "%c%c%c" "%c%c%c" "%c%c%c", 
	0xff, 0xfd, 0x03,	//Do Suppress Go Ahead
	0xff, 0xfb, 0x18,	//Will Terminal Type
	0xff, 0xfb, 0x1f,	//Will Negotiate About Window Size
	0xff, 0xfb, 0x20,	//Will Terminal Speed
	0xff, 0xfb, 0x21,	//Will Remote Flow Control	(sorta HACK, we just ignore future messages about it)
	0xff, 0xfb, 0x22,	//Will Line Mode			(sorta HACK, we only can really handle talking to cygwin and others that behave the same)
	0xff, 0xfb, 0x27,	//Will New Environment Option (almost HACK)
	0xff, 0xfd, 0x05	//Do Status
	);
	networkSend(testset);
	
	setDisplayAttribute(0);//reset display
//	renderMode(RENDER_FAST);
	
	while (1)
	{
	
		error = sceKernelReceiveMbx(protocolMessagebox, (void*)&data, NULL);
		if (error < 0)
		{
			//OH NO ERROR :(
			//tell renderer an error i guess....
			renderMain("TELNET IS DIED OHES NOES", COLOR_RED);
			return -1;
		}
		
		switch(data->from)
		{
		case FROM_NETWORK:
			vt100Render(data->text, data->length);
			
			if(networkBufSize != 0)
			{
				networkSendBin(networkBuf, networkBufSize);
				networkBufSize = 0;
			}
			break;
		case FROM_USER:
			{
				if (!wontEcho)
				{
					char tosend[100];
					sprintf(tosend, "%s\r\n", data->text); 
					networkSend(tosend);
					styledRender(tosend);
				}
				else
				{
					if (data->text[0] == '\n' || data->text[0] == '\r')
					{
						if (cygwinHacksMode)	//okay so this isnt actually a cygwin hack as it has been negotiated from linemode, but it is okay for now
						{
							networkSendBin("\r", 1);	//	(CYGWIN HACK)
						}
						else
						{
							networkSendBin("\r\0", 2);
						}
						
						if (!remoteEcho)
							styledRender("\r\n");
					}
					else
					{
						//Need to do some replacement of charcodes when sending, not sure which ;)
						switch(data->text[0])
						{
							case '\b':
								if (cygwinHacksMode)
								{
									networkSendBin("\b", 1);
								}
								else
								{
									networkSendBin("\177", 1);
								}
								if (!remoteEcho)
								{
									char* forscreen = "\b \b";
									styledRender(forscreen);
								}
								break;
							default:
								networkSendBin(data->text, 1);
								if (!remoteEcho)
								{
									char forscreen[2];
									forscreen[0] = data->text[0];
									forscreen[1] = '\0';
									styledRender(forscreen);
								}
						}
						
					}
				}
				break;
			}
		case FROM_USER_CODE:
			{
				char* code = "";
				int len = 0;
				
				switch(data->text[0])
				{
				case 33:	//page up
					code = "\033[5~";
					len = 4;
					break;
				case 34:	//page down
					code = "\033[6~";
					len = 4;
					break;
				case 35:	//end
					code = "\033[F";
					len = 3;
					break;
				case 36:	//home
					code = "\033[H";
					len = 3;
					break;
				case 37:	//left
					if (cursorMode == POSITIONING_MODE)
						code = "\033[D";
					else
						code = "\033OD";
					len = 3;
					break;
				case 38:	//up
					if (cursorMode == POSITIONING_MODE)
						code = "\033[A";
					else
						code = "\033OA";
					len = 3;
					break;
				case 39:	//right
					if (cursorMode == POSITIONING_MODE)
						code = "\033[C";
					else
						code = "\033OC";
					len = 3;
					break;
				case 40:	//down
					if (cursorMode == POSITIONING_MODE)
						code = "\033[B";
					else
						code = "\033OB";
					len = 3;
					break;
				case 45:	//Insert
					code = "\033[2~";
					len = 4;
					break;
				case 46:	//Delete
					code = "\033[3~";
					len = 4;
					break;
				}
				
				networkSendBin(code, len);
				break;
			}
		}
		free(data->text);
		free(data);
	}
	
	//Should never happen
	return 0;
}

//flush the torender Buffer
#define flushBuffer(); {\
	styledRender(torender);\
	torender[0] = '\0';\
	}

/*
	0 = normal char printing
	1 = just read escape character
	2 = Parsing from ^[
	
	3 = Just read a IAC (255), this means we are getting a WILL/WON'T/DO/DON'T
	4 = Read Will
	5 = Read Won't
	6 = Read Do
	7 = Read Don't
	8 = Reading Suboption
	
	9 = Ready for end of suboption
	
	10 = Parsing from ^#	(debug)
	11 = Parsing from ^(	(charset)
	12 = Parsing from ^)	(charset)
*/
int state = 0;

//int current_color = COLOR_WHITE;

char escSequence[200];
int escSequenceLen = 0;

//buffer of text to render, space for more than 2 screens of plain chars
char torender[4096];

void vt100Render(char* inputStr, int length)
{
	torender[0] = '\0';
	
	int a;
	for (a = 0; a < length; a++)
	{
		switch (inputStr[a])	//need to handle esc chars first, so they arent picked up in an esc seq
		{
		case '\n':
		case '\r':
		case '\v':	//(vertical tab)
			{
				strncat(torender, &inputStr[a], 1);
				break;
			}
		case '\b':
			//flush
			flushBuffer();
			//move back 1
			renderGoBy(-1, 0);
			break;
//		case '\000':	//According to a capture from bitchx in ansi mode, this may be newline too :|
			//Not so sure anymore, might remove this later...
//			strncat(torender, "\n", 1);
			break;
		case 14:	// 'shift out'  Switch to alt font G1
			flushBuffer();
			using_gX = 1;
			renderSetFont(G1_font);
			break;
		case 15:	// 'shift in' Switch to normal font G0
			flushBuffer();
			using_gX = 0;
			renderSetFont(G0_font);
			break;
			
		default:
			switch (state)
			{
			case 0:
				switch (inputStr[a])
				{
				case '\033': //Escape Character
					state = 1;
					break;
				case '\377': // 255 (before a will/wont)
					state = 3;
					break;
				default:
					strncat(torender, &inputStr[a], 1);
				}
				break;
			case 1:
				switch (inputStr[a])
				{
				case '[':
					state = 2;
					break;
				case '#':
					state = 10;
					break;
				case '7':	//save cursor pos
					state = 0;
					flushBuffer();
					//send save msg
					renderSaveLoad(SAVE);
					//also save some extras
					saved_textColor = textColor;
					saved_bgColor = bgColor;
					saved_attrReverse = attrReverse;
					saved_brightness = brightness;
					saved_G0_font = G0_font;
					saved_G1_font = G1_font;
					saved_using_gX = using_gX;
					break;
				case '8': //restore cursor pos
					state = 0;
					flushBuffer();
					//send load msg
					renderSaveLoad(LOAD);
					//also load some extras
					textColor   = saved_textColor;
					bgColor     = saved_bgColor;
					attrReverse = saved_attrReverse;
					brightness  = saved_brightness;
					G0_font     = saved_G0_font;
					G1_font     = saved_G1_font;
					using_gX    = saved_using_gX;
					
					if (using_gX == 0)
					{
						renderSetFont(G0_font);
					}
					else
					{
						renderSetFont(G1_font);
					}

					break;
				case 'E':	//New line
					state = 0;
					strncat(torender, "\r\n", 2);
					break;
				case 'D':	//cursor down - (TODO) at bottom of region, scroll up
					state = 0;
					flushBuffer();
					renderGoByMove(0, 1);
					break;
				case 'M':	//cursor up - (TODO) at top of region, scroll down
					state = 0;
					flushBuffer();
					renderGoByMove(0, -1);
					break;
					
				case '>':	//keypad keys in numeric mode *shrug*
				case '=':	//keypad keys in applications mode *shrug*
					state = 0;
					break;
				case '(':
					state = 11;
					break;
				case ')':
					state = 12;
					break;
		//		default:	//Unknown :(
				}
				break;
			case 2:
				//End of Esc Sequence
				if ((inputStr[a] >= 'a' && inputStr[a] <= 'z') || (inputStr[a] >= 'A' && inputStr[a] <= 'Z'))
				{
					escSequence[escSequenceLen] = '\0';
					state = 0;
					
					handleEscSequence(inputStr[a]);
					
					//Processed
					escSequenceLen = 0;
				}
				else
				{
					escSequence[escSequenceLen] = inputStr[a];
					escSequenceLen++;
				}
				break;
			case 3:
				switch(inputStr[a])
				{
				case '\373': //WILL
					state = 4;
					break;
				case '\374': //WON'T
					state = 5;
					break;
				case '\375': //DO
					state = 6;
					break;
				case '\376': //DON'T
					state = 7;
					break;
				case '\372': //suboption begin
					state = 8;
					escSequence[0] = '\0';
					escSequenceLen = 0;
					break;
				default:
					//UNKNOWN OMGOES
					break;
				}
				break;
			case 4: //WILL
				state = 0;
				handleWill(inputStr[a]);
				break;
			case 5: //WON'T
				state = 0;
				handleWont(inputStr[a]);
				break;
			case 6: //DO
				state = 0; //Assume we can reset the state.
				handleDo(inputStr[a]);
				break;
			case 7: //DON'T
				state = 0;
				handleDont(inputStr[a]);
				break;
			case 8: //In Suboption
				handleSuboption(inputStr[a]);
				break;
			case 9: //Suboption escaped thing
				if (inputStr[a] == '\360') //end char
				{
					state = 0;
					handleSuboptionEnd();
				}
				break;
			case 10:
				{
					int q;
					switch(inputStr[a])
					{
					case '8':	//Fill screen with E OMGOES
						flushBuffer();
						renderSaveLoad(SAVE); // HACK, instead use the direct render at position thing?
						renderGoto(0,0);
						for (q = 0; q < 24; q++)
							renderMain("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE", COLOR_WHITE);
						renderSaveLoad(LOAD);
						break;
					}
					state = 0;
					break;
				}
			case 11: // ^(X
				state = 0;
				if (inputStr[a] == 'A' || inputStr[a] == 'B')
					G0_font = FONT_DEFAULT;
				else if (inputStr[a] == '0')
					G0_font = FONT_LINEDRAW;
				
				if (using_gX == 0)
				{
					flushBuffer();
					renderSetFont(G0_font);
				}
				break;
			case 12: // ^)X
				state = 0;
				if (inputStr[a] == 'A' || inputStr[a] == 'B')
					G1_font = FONT_DEFAULT;
				else if (inputStr[a] == '0')
					G1_font = FONT_LINEDRAW;
				
				if (using_gX == 1)
				{
					flushBuffer();
					renderSetFont(G1_font);
				}
				break;
				
				
				//Else who knows what happened, assume its a fuckup and do nothing....
			}
			
		}
		//The great big debug hack lol
/*		{
			char aaa[10];
			sprintf(aaa, "%d(%d) ", inputStr[a], state);
			strcat(torender, aaa);
		}	*/
	}
	
	//FLUSH
	flushBuffer();
	
	//Redraw Screen
	renderRedraw();
}

void handleSuboption(char inputChr)
{
	if (inputChr == '\377')
	{
		state = 9;
		return;
	}
	else
	{
		escSequence[escSequenceLen] = inputChr;
		escSequenceLen++;
		escSequence[escSequenceLen] = '\0';
		return;
	}
}

void handleSuboptionEnd()
{
	if (escSequence[0] == 0x18 && escSequence[1] == 0x01) //Send your terminal Type
	{
		//send suboption terminal type vt100
//		char sendbuf[11] = { 0xff, 0xfa, 0x18, 0x00, 0x56, 0x54, 0x31, 0x30, 0x30, 0xff, 0xf0 };  //vt100
//		char sendbuf[10] = { 0xff, 0xfa, 0x18, 0x00, 0x41, 0x4e, 0x53, 0x49, 0xff, 0xf0 }; //ansi
		char sendbuf[11] = { 0xff, 0xfa, 0x18, 0x00, 0x58, 0x54, 0x45, 0x52, 0x4d, 0xff, 0xf0 }; //xterm
		networkBufAdd(sendbuf, 11);
	} else 
	if (escSequence[0] == 0x20 && escSequence[1] == 0x01) //send your Terminal speed
	{
		char sendbuf[17] = { 0xff, 0xfa, 0x20, 0x00, 0x33, 0x38, 0x34, 0x30, 0x30, 0x2c, 0x33, 0x38, 0x34, 0x30, 0x30, 0xff, 0xf0 }; // 38400,38400 (could use something else)
		networkBufAdd(sendbuf, 17);
	} else
	if (escSequence[0] == 0x22 && escSequence[1] == 0x01 && escSequence[2] == 0x13)	//linemode 0x13
	{
		char sendbuf[10] = { 0xff, 0xfa, 0x22, 0x01, 0x17, 0xff, 0xf0 }; //linemode 0x17
		networkBufAdd(sendbuf, 10);
		cygwinHacksMode = true;   //This seems like a good enough place to set it...
	} else
	if (escSequence[0] == 0x27 && escSequence[1] == 0x01) // new environment option (1)
	{
		char sendbuf[6] = { 0xff, 0xfa, 0x27, 0x00, 0xff, 0xf0 };
		
		networkBufAdd(sendbuf, 6);
	}
	//TODO HANDLE OTHERS...
}

void handleWill(char inputChr)
{
	switch(inputChr)
	{
	case 0x01: //Will ECHO
		{
			remoteEcho = true;
			
			char Msg[3] = { 255, 253, 1 };	//Do Echo
			networkBufAdd(Msg, 3);
			break;
		}
	case 0x03: //Will Suppress Go Ahead
		{
			char Msg[3] = { 0xff, 0xfd, 0x03 };	//Do Suppress go Ahead
			networkBufAdd(Msg, 3);
			break;
		}
	}
}

void handleWont(char inputChr)
{
	switch(inputChr)
	{
	}
}

void handleDo(char inputChr)
{
	switch(inputChr)
	{
	//Things we should do (but dont yet)
	case 0x18: //Terminal Type
	break;	//i dont think we do anything here, only on a 'plz send term type' msg should we do anything?
	
	//Things we need to ignore
	case 0x20: //Terminal Speed
	case 39: //New Environment Option
	break;
	
	//Things we dont do and never will
	case 35: //X Display Location
	case 36: //Do environment Option
		{
			char wontMsg[3];		//wont <what you said>
			wontMsg[0] = 255;
			wontMsg[1] = 252;
			wontMsg[2] = inputChr;
			networkBufAdd(wontMsg, 3);
			break;
		}
	
	case 0x01: //do echo
		{
			renderMode(RENDER_FAST);//change render mode here to let MUD mode work right (HACK)
			wontEcho = true;
			renderDrawCurrentPos(true);
			
			char wontMsg[4];		//Wont echo
			wontMsg[0] = 255;
			wontMsg[1] = 252;
			wontMsg[2] = 1;
			networkBufAdd(wontMsg, 3);
			break;
		}
	case 0x1f: //do Negotiate about window size (send back 80x24)
		{
			char gogo[9];
			gogo[0] = 0xff;
			gogo[1] = 0xfa;
			gogo[2] = 0x1f;
			gogo[3] = 0x00;
			gogo[4] = 0x50;
			gogo[5] = 0x00;
			gogo[6] = 0x18;
			gogo[7] = 0xff;
			gogo[8] = 0xf0;
			networkBufAdd(gogo, 9);
			break;
		}
	case 0x00: //do binary transmission
		{
			char willMsg[3] = { 0xff, 0xfb, 0x00 };
			networkBufAdd(willMsg, 3); //will Binary Transmission
			break;
		}
	case 0x22: //Line Mode
		{
			char lineMsg[54] = {                                              0xff, 0xfa, 0x22, 0x03, 0x01, 	//05
			0x00, 0x00, 0x03, 0x62, 0x03, 0x04, 0x02, 0x0f, 0x05, 0x00, 0x00, 0x07, 0x62, 0x1c, 0x08, 0x02,		//16
			0x04, 0x09, 0x42, 0x1a, 0x0a, 0x02, 0x7f, 0x0b, 0x02, 0x15, 0x0c, 0x02, 0x17, 0x0d, 0x02, 0x12,		//16
			0x0e, 0x02, 0x16, 0x0f, 0x02, 0x11, 0x10, 0x02, 0x13, 0x11, 0x00, 0x00, 0x12, 0x00, 0x00, 0xff,		//16
			0xf0 };                                                                                        		//01
			networkBufAdd(lineMsg, 54);
			break;
		}
	}
}

void handleDont(char inputChr)
{
	switch(inputChr)
	{
	case 0x22: //Linemode
		{
			char wontMsg[3] = { 0xff, 0xfc, 0x22 };	//wont linemode
			networkBufAdd(wontMsg, 3);
			break;
		}
	}
}




void handleEscSequence(char inputChr)
{
	switch(inputChr)
	{
	case 'H':	//goto possition
	case 'f':
		{
			char* ret;
			int x = 0;
			int y = 0;
			
			//sscanf(escSequence, "%d ; %d", &y, &x);
			y = strtol(escSequence, &ret, 10);
			if (ret != escSequence && *ret != '\0') //read something
			{
				ret++;	//hopefully we skip a ';'
				x = strtol(ret, NULL, 10);
			}
			else
				y = 0;
			
			//if they are numbers, we need to -- them
			if (x >= 1) x--;
			if (y >= 1) y--;
			
			flushBuffer();
			renderGoto(x,y);
			break;
		}
	case 'A':	//Move up by X
		{
			int moves = 1;
			moves = atoi(escSequence);
			if (moves == 0) moves = 1;
			flushBuffer();
			renderGoBy(0, -moves);
			break;
		}
	case 'B':	//Move down by X
		{
			int moves = 1;
			moves = atoi(escSequence);
			if (moves == 0) moves = 1;
			flushBuffer();
			renderGoBy(0, moves);
			break;
		}
	case 'C':	//Move Right by X
		{
			int moves = 1;
			moves = atoi(escSequence);
			if (moves == 0) moves = 1;
			flushBuffer();
			renderGoBy(moves, 0);
			break;
		}
	case 'D':	//Move left by X
		{
			int moves = 1;
			moves = atoi(escSequence);
			if (moves == 0) moves = 1;
			flushBuffer();
			renderGoBy(-moves, 0);
			break;
		}
	case 's':	//Save position
		flushBuffer();
		renderSaveLoad(SAVE);
		break;
	case 'u':	//Load Position
		flushBuffer();
		renderSaveLoad(LOAD);
		break;
		
	case 'K':
		switch(escSequence[0])
		{
		case '\0':
		case '0':	//Erase to end of line
			flushBuffer();
			styledClear(CLEAR_TO_EOL);
			break;
		case '1':	//Erase to start of line
			flushBuffer();
			styledClear(CLEAR_TO_SOL);
			break;
		case '2':	//Erase current line
			flushBuffer();
			styledClear(CLEAR_LINE);
			break;
		}
		break;
	case 'J':
		switch(escSequence[0])
		{
		case '\0':
		case '0':	//Erase current line to bottom
			flushBuffer();
			styledClear(CLEAR_TO_EOL);			//HACK - if we are at top/bottom = bad
			renderGoBy(0,1);
			styledClear(CLEAR_LINE_TO_BOTTOM);
			renderGoBy(0,-1);
			break;
		case '1':	//Erase current line to top
			flushBuffer();
			styledClear(CLEAR_TO_SOL);			//HACK - if we are at top/bottom = bad
			renderGoBy(0,-1);
			styledClear(CLEAR_LINE_TO_TOP);
			renderGoBy(0,1);
			break;
		case '2':	//Erase screen
			flushBuffer();
			styledClear(CLEAR_SCREEN);
			break;
		}
		break;
	case 'm':	//Display Attributes
		{
			flushBuffer();
			int attr = 0;
			int a;
			bool setone = true;	//(true so we process a ^[m as a ^[0m, which it is)
			for (a=0; a < escSequenceLen; a++)
			{
				if (escSequence[a] >= '0' && escSequence[a] <= '9')
				{
					attr *= 10;
					attr += (escSequence[a]-'0');
					setone = true;
				}
				else
				{
					if (setone)
						setDisplayAttribute(attr);
					setone = false;
					attr = 0;
				}
			}
			if (setone)
				setDisplayAttribute(attr);
			break;
		}
	case 'h':	//Some setup things
		{
			if (escSequenceLen == 2)
				switch(escSequence[1])
				{
					case '1':	//cursor keys in applications mode
						cursorMode = APPLICATION_MODE;
						break;
					case '5':	//black chars on white BG (TODO should this change bold etc)
						textColor = COLOR_BLACK;
						bgColor   = COLOR_WHITE;
						break;
					case '6':	//turn on region - origin mode	(TODO i dont actually know what this is ment to do, so ill fullscreen it)
						flushBuffer();
						renderSetRegion(default_top_row,default_bot_row);
						break;
				}
			break;
		}
 	case 'l':	//Some un-setup things
		{
			if (escSequenceLen == 2)
				switch(escSequence[1])
				{
					case '1':	//cursor keys in cursor positioning mode
						cursorMode = POSITIONING_MODE;
						break;
					case '5':	//white chars on black BG (TODO should this change bold etc)
						textColor = COLOR_WHITE;
						bgColor   = COLOR_BLACK;
						break;
					case '6':	//turn on region - full screen mode
						flushBuffer();
						renderSetRegion(default_top_row,default_bot_row);
						break;
				}
			break;
		}
	case 'c':	//tell me your terminal type / status
		{
//		 *  ESC [ c           request to identify terminal type
//		 *  ESC [ 0 c         request to identify terminal type
//		 *  ESC Z             request to identify terminal type
			//Always assume its the same thing, send back a report saying we are vt100
			char *tosend = "\033[?1;0c";
			networkSendBin(tosend, 7);
			break;
		}
	case 'r':	//set a scroll region
		{
			//   "12;13"
			int start = default_top_row+1;
			int end = default_bot_row+1;
//			sscanf(escSequence, "%d ; %d", &start, &end);
			char* ret;
			start = strtol(escSequence, &ret, 10);
			if (ret != escSequence && *ret != '\0') //read something
			{
				ret++;	//hopefully we skip a ';'
				end = strtol(ret, NULL, 10);
			}
			else	//if we didnt read anything, we need to reset start to its original value
			{
				start = default_top_row+1;
			}
			
			start--; end--;
			
			flushBuffer();
			renderSetRegion(start, end);
			break;
		}
/*		HACK TODO - part of 'ansi' terminal, BitchX borks hard when on ansi and this is part of an almost fix.
					however! xterm and putty bork with BitchX in ansi mode, so we just wont go there ;)
	case 'S':	//cursor down - (TODO) at bottom of region, scroll up
		state = 0;
		flushBuffer();
		renderScroll(1);
//		renderGoByMove(0, 1);
		break;*/
	}
}

void setDisplayAttribute(int attr)
{
	switch(attr)
	{
	case 0: 		//Reset all attributes
		textColor = COLOR_WHITE;
		bgColor = COLOR_BLACK;
		attrReverse = false;
		brightness = BRIGHTNESS_NORMAL;
		break;
	case 1: 		//Bright
		brightness = BRIGHTNESS_BRIGHT;
		break;
	case 2: 		//Dim
		brightness = BRIGHTNESS_DIM;
		break;
/*	case 4: 		//Underscore	
	case 5: 		//Blink
	case 8: 		//Hidden
*/
	case 7: 		//Reverse
		attrReverse = true;
		break;
	
//		Foreground Colours
	case 30:		//Black
		textColor = COLOR_BLACK;
		break;
	case 31:		//Red
		textColor = COLOR_RED;
		break;
	case 32:		//Green
		textColor = COLOR_GREEN;
		break;
	case 33:		//Yellow
		textColor = COLOR_YELLOW;
		break;
	case 34:		//Blue
		textColor = COLOR_BLUE;
		break;
	case 35:		//Magenta
		textColor = COLOR_MAGENTA;
		break;
	case 36:		//Cyan
		textColor = COLOR_CYAN;
		break;
	case 37:		//White
		textColor = COLOR_WHITE;
		break;
	
//		Background Colours
	case 40:		//Black
		bgColor = COLOR_BLACK;
		break;
	case 41:		//Red
		bgColor = COLOR_RED;
		break;
	case 42:		//Green
		bgColor = COLOR_GREEN;
		break;
	case 43:		//Yellow
		bgColor = COLOR_YELLOW;
		break;
	case 44:		//Blue
		bgColor = COLOR_BLUE;
		break;
	case 45:		//Magenta
		bgColor = COLOR_MAGENTA;
		break;
	case 46:		//Cyan
		bgColor = COLOR_CYAN;
		break;
	case 47:		//White
		bgColor = COLOR_WHITE;
		break;
	}
}

//render text using current colors and text attributes
void styledRender(char* text)
{
	int color1;	//text
	int color2;	//background
	
		//First apply a reverse if needed
	if (attrReverse)
	{
		color1 = bgColor;
		color2 = textColor;
	}
	else
	{
		color1 = textColor;
		color2 = bgColor;
	}
	color2 &= MODIFY_NORMAL;
	
		//and now change brightness if needed
	switch (brightness)
	{
	case BRIGHTNESS_DIM:
		color1 &= MODIFY_DIM;
		break;
	case BRIGHTNESS_NORMAL:
		color1 &= MODIFY_NORMAL;
		break;
	case BRIGHTNESS_BRIGHT:
		if (color1 == COLOR_BLACK)	//special case, bright black = dark grey :|
			color1 = MODIFY_DIM;
		else
			color1 &= MODIFY_BRIGHT;
		break;
		break;
	}
	
	renderMainBG(text, color1, color2);
}

void styledClear(int clearType)
{
	int color2;	//background
	
		//apply a reverse if needed
	if (attrReverse)
		color2 = textColor;
	else
		color2 = bgColor;
	
	color2 &= MODIFY_NORMAL;
	renderClear(clearType, color2);
}
