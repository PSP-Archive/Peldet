#include "../global.h"
#include "telnet.h"

void addDataToCache(char* text);
bool parseFirst();
bool parseSecond();
char* getUserNick(char* nickString);


char newLine[3] = {'\r','\n','\0'};

//Variables filled in by the setting string
char* yournick;
char* chan;

/*
	Connection status on IRC
	0 = Not connected
	1 = Connecting (regging nick etc)
	2 = Connected
	3 = in Chan
*/
int status = 0;

//Buffer an incoming line
char lineBuffer[1000];
int bufferPos = 0;


/* A thread which implements the irc protocol */
int ircProtocol(SceSize args, void *argp)
{
	ProtocolMsg* data;
	int error;
	
	lineBuffer[0] = '\0';
	
	while (1)
	{
		//for in FROMUSER
		char talkbuffer[200];
	
		error = sceKernelReceiveMbx(protocolMessagebox, (void*)&data, NULL);
		if (error < 0)
		{
			//OH NO ERROR :(
			//tell renderer an error i guess....
			renderMain("IRC IS DIED OHES NOES\r\n", COLOR_RED);
			return -1;
		}
		switch(data->from)
		{
		//This will only happen once and will be the first thing we see
		case FROM_CONFIG:
		{
			//Split the nick and chan
			sprintf(talkbuffer, "_%s_\r\n", data->text);
			renderMain(talkbuffer, COLOR_BLUE);
			char* space = strchr(data->text, ' ');
			
			space[0] = '\0';
			yournick = strdup(data->text);
			chan = strdup(space+1);
			
			//and send it to the server
			sprintf(talkbuffer, "NICK %s\r\nUSER %s PELDET SERVERADDR :%s\r\n", yournick, yournick, yournick);
			networkSend(talkbuffer);
			break;
		}
		case FROM_NETWORK:
			addDataToCache(data->text);
//			renderMain(data->text, COLOR_WHITE);
			break;
		case FROM_USER:
					//GIANT HAXOR IN THE CODE
					//TODO SCRAP AND RECODE THIS BIT ;)
					//DONOT EVEN PLAY WITH THIS CODE IT IS BAD
			
			if (data->text[0] == '/') //Command
			{
				int len = strlen(data->text);
				if (len >= 2 && data->text[1] == 'q') // QUIT /q lol byes
				{
					if (len > 2)
						sprintf(talkbuffer, "QUIT :%s\r\n", data->text+3);
					else
						sprintf(talkbuffer, "QUIT :Was using peldet -> localhost.geek.nz/telnet/\r\n");
					networkSend(talkbuffer);
					break;
				}
				
				if (len >= 3 && data->text[1] == 'm' && data->text[2] != 'e') // MSG  /m joe lol hi joe
				{
					char* firstspace = strchr(data->text, ' ');
					char* secondspace;
					char* target;
					if (firstspace == NULL) goto err;
					secondspace = strchr(firstspace+1, ' ');
					if (secondspace == NULL) goto err;
					
					secondspace[0] = '\0';
					target = strdup(firstspace+1);
					secondspace[0] = ' ';
					sprintf(talkbuffer, "PRIVMSG %s :%s\r\n", target, secondspace+1);
					networkSend(talkbuffer);
					
					sprintf(talkbuffer, "*%s* %s\r\n", target, secondspace+1);
					renderMain(talkbuffer, COLOR_PURPLE);
					free(target);
					break;
					
					err:
					renderMain("You fail at /m!\r\n", COLOR_RED);
					break;
				}
				
				if (len >= 3 && data->text[1] == 'm' && data->text[2] == 'e') // ACT  /me punches j00
				{
					char* firstspace = strchr(data->text, ' ');
					if (firstspace == NULL) goto err2;
					
					sprintf(talkbuffer, "PRIVMSG %s :%cACTION %s%c\r\n", chan, 1, firstspace+1, 1);
					networkSend(talkbuffer);
					
					sprintf(talkbuffer, "* %s %s\r\n", yournick, firstspace+1);
					renderMain(talkbuffer, COLOR_GREEN);
					
					break;
					
					err2:
					renderMain("You fail at /me!\r\n", COLOR_RED);
					break;
				}
				
				//UNKNOWN, send raw!
				sprintf(talkbuffer, "%s\r\n", data->text+1);
				networkSend(talkbuffer);
				sprintf(talkbuffer, "RAW: %s\r\n", data->text+1);
				renderMain(talkbuffer, COLOR_RED);
			}
			else //plain text
			{
				
				//print to screen
				sprintf(talkbuffer, "<%s> %s\r\n", yournick, data->text);
				renderMain(talkbuffer, COLOR_GREY);
				
				//send to server
				sprintf(talkbuffer, "PRIVMSG %s :%s\r\n", chan, data->text);
				networkSend(talkbuffer);
			}
			break;
		}
		free(data->text);
		free(data);
	}
	
	//Should never happen
	return 0;
}


/*
	Adds data from the network into a cache, 
	when it hits a \n it calls the correct function to handle the command
*/
void addDataToCache(char* text)
{
	int a; 
	for (a = 0; a < strlen(text); a++)
	{
		lineBuffer[bufferPos] = text[a];
		bufferPos++;
		if (text[a] == '\n')
		{
			lineBuffer[bufferPos] = '\0';
			
			//Try parse 
			if (!parseFirst())
			{
				if (!parseSecond())
				{
					//Unable to parse, print raw
					renderMain("UNK:", COLOR_RED);
					renderMain(lineBuffer, COLOR_RED);
				}
			}
	
			//Reset the buffer
			lineBuffer[0] = '\0';
			bufferPos = 0;
		}
	}
	lineBuffer[bufferPos] = '\0';
}

bool parseFirst()
{
	//Copy the first string to first
	char* firstspaceStr = strchr(lineBuffer, ' ');
	if (firstspaceStr == NULL) return false;
	firstspaceStr[0] = '\0';
	char* first = strdup(lineBuffer);
	firstspaceStr[0] = ' ';
	
		//PING
	if (strcmp(first, "PING") == 0)
	{
		lineBuffer[1] = 'O';
		networkSend(lineBuffer);
		renderMain("Ping? Pong!\r\n", COLOR_YELLOW);
		free (first);
		return true;
	}
	
	if (!strcmp(first, "NOTICE")) //NOTICE : this server is liek leet pwnz0r!
	{
		renderMain(lineBuffer, COLOR_YELLOW);
		free (first);
		return true;
	}
	
	free (first);
	return false;
}

	//this will find your nick
	//this finds the important message
	//Skip ' '
	//skip any leading ':'
#define printFourthOnwards(color) {\
	char* third = strchr(secondSpace+1, ' ');\
	third++;\
	if (third[0] == ':') third++;\
	renderMain(third, color);\
}

bool parseSecond()
{
	//Copy the first string to first
	char* firstSpace = strchr(lineBuffer, ' ');
	if (firstSpace == NULL) return false;
	firstSpace[0] = '\0';
	char* first = strdup(lineBuffer);
	firstSpace[0] = ' ';

	//Copy the 2nd string to second
	char* secondSpace = strchr(firstSpace+1, ' ');
	if (secondSpace == NULL) return false;
	secondSpace[0] = '\0';
	char* second = strdup(firstSpace+1);
	secondSpace[0] = ' ';
	
	int secNum = atoi(second);
	if (secNum != 0) //It is a number
	{
		switch(secNum)
		{
		//Things we dont really care about
		
		//Things we chose to ignore completely
		case 333: //Someoneerather set the topic, TODO ACTUALLY USE THIS
		case 366://end of /names list TODO USE THIS
			free(first);
			free(second);
			return true;
		//Straight message from the server to pass on.
		case 1:
		case 2:
		case 3:
		case 4://ServerName ServerVersion something something something
		case 5://Settings such supported by the server (We should probally do something with these one day...)
		if (status == 0)//Any of these mean that we are now connnected, so do our onjoin bits
		{
			status = 1; //We are connected yay!
			printFourthOnwards(COLOR_YELLOW);
			
			sprintf(lineBuffer, "JOIN %s\r\n", chan);
			networkSend(lineBuffer);
			
			free(first);
			free(second);
			return true;
		}
		case 251://User Invisible Servers
		case 252://ops online
		case 253://unknown connections
		case 254://channels formed
		case 255://i have X clients and Y servers (local server)
		case 375://pre MOTD (Start of MOTD?)
		case 372://do XXXX to read the MOTD
		case 376://end of MOTD
		//For now these things are just being told to the user, 
		//we dont really want to do anything with them
			printFourthOnwards(COLOR_YELLOW);
			free(first);
			free(second);
			return true;
		
		//Error messages
		case 404://cannot send to channel
		case 421://unknown command
			printFourthOnwards(COLOR_RED);
			free(first);
			free(second);
			return true;
		//Things we need to do something about
		
		case 332: //Channel topic is...
		{
			//Get the chan
			char *thirdSpace = strchr(secondSpace+1, ' ');
			if (thirdSpace == NULL)
			{
				renderMain("MALFORMED 332 CHANTOPIC\r\n", COLOR_RED);
				free(first);
				free(second);
				return false;
			}
			char *fourthSpace = strchr(thirdSpace+1, ' ');
			if (fourthSpace == NULL)
			{
				renderMain("MALFORMED 332 CHANTOPIC\r\n", COLOR_RED);
				free(first);
				free(second);
				return false;
			}
			//Extract the chan name
			fourthSpace[0] = '\0';
			char* channame = strdup(thirdSpace+1);
			fourthSpace[0] = ' ';
			if (fourthSpace[1] == ':') fourthSpace++;
			sprintf(lineBuffer, "-:- topic in %s is %s", channame, fourthSpace+1);
			renderMain(lineBuffer, COLOR_BLUE);
			free(channame);
			free(first);
			free(second);
			return true;
		}
		case 353: //People in ... is .....
		{
			free(first);
			free(second);
			
			//find 4thspace
			char* space = strchr(lineBuffer, ' ');
			if (space == NULL) goto err353;
			space = strchr(space+1, ' ');
			if (space == NULL) goto err353;
			space = strchr(space+1, ' ');
			if (space == NULL) goto err353;
			space = strchr(space+1, ' ');
			if (space == NULL) goto err353;
			
			//and 5thspace
			char* fifthspace = strchr(space+1, ' ');
			if (fifthspace == NULL) goto err353;
			
			//print out..
			fifthspace[0] = '\0';
			sprintf(lineBuffer, "People in %s: %s", space+1, fifthspace+2);
			renderMain(lineBuffer, COLOR_BLUE);
			return true;
			
			err353:
				renderMain("BAD 353(Topic) LINE\r\n", COLOR_RED);
				return true;
		}
		case 433: //Nickname in use
			//TODO
			renderMain("ERROR: Nickname is in use, type: /nick new_nick to attempt a nickchange", COLOR_RED);
			return true;
		}
	}
	else if (!strcmp(second, "NOTICE")) //Notice Message
	{
		printFourthOnwards(COLOR_YELLOW);
		free(first);
		free(second);
		return true;

	}
	else if (!strcmp(second, "PART")) //Part message
	{
		char* thirdSpace = strchr(secondSpace+1, ' ');
		if (thirdSpace == NULL)
		{
			renderMain("MALFORMED PART\r\n", COLOR_RED);
			free(first);
			free(second);
			return false;
		}
		thirdSpace[0] = '\0';
		char* third = strdup(secondSpace+1);
		thirdSpace[0] = ' ';

		//get usernick
		char* partnick = getUserNick(first);

		sprintf(lineBuffer, "-:- %s has left %s\r\n", partnick, third);
		renderMain(lineBuffer, COLOR_BLUE);
		free(first);
		free(second);
		free(partnick);
		return true;
	}
	else if (!strcmp(second, "JOIN")) //Join message
	{
		//get usernick
		char* joinnick = getUserNick(first);

		sprintf(lineBuffer, "-:- %s has joined %s", joinnick, secondSpace+1);
		renderMain(lineBuffer, COLOR_BLUE);
		free(first);
		free(second);
		free(joinnick);
		return true;
	}
	else if (!strcmp(second, "QUIT")) //Quit message
	{
		//get usernick
		char* partnick = getUserNick(first);

		if (secondSpace[1] == ':')
			secondSpace++;
			
		//Strip any \r\n
		char* test = strchr(secondSpace+1, '\n');
		if (test != NULL) test[0] = '\0';
		test = strchr(secondSpace+1, '\r');
		if (test != NULL) test[0] = '\0';
		
		sprintf(lineBuffer, "-:- %s has quit IRC (%s)\r\n", partnick, secondSpace+1);
		renderMain(lineBuffer, COLOR_BLUE);
		free(first);
		free(second);
		free(partnick);
		return true;
	}
	else if (!strcmp(second, "MODE")) //Mode message
	{
		char* thirdSpace = strchr(secondSpace+1, ' ');
		if (thirdSpace == NULL)
		{
			renderMain("MALFORMED MODE\r\n", COLOR_RED);
			free(first);
			free(second);
			return false;
		}
		thirdSpace[0] = '\0';
		char* third = strdup(secondSpace+1);
		thirdSpace[0] = ' ';

		//get usernick
		char* modenick = getUserNick(first);

		//TODO THIS IS IN BAD STYLE, REDO THE #define printFourth....
		sprintf(lineBuffer, "-:- %s sets mode %s ", modenick, third);
		renderMain(lineBuffer, COLOR_BLUE);
		printFourthOnwards(COLOR_BLUE);
		free(first);
		free(second);
		free(modenick);
		return true;
	}
	else if (!strcmp(second, "TOPIC")) //Topic Change message
	{
		char* thirdSpace = strchr(secondSpace+1, ' ');
		if (thirdSpace == NULL)
		{
			renderMain("MALFORMED TOPIC\r\n", COLOR_RED);
			free(first);
			free(second);
			return false;
		}
		thirdSpace[0] = '\0';
		char* third = strdup(secondSpace+1);
		thirdSpace[0] = ' ';

		//get usernick
		char* modenick = getUserNick(first);

		//TODO THIS IS IN BAD STYLE, REDO THE #define printFourth....
		sprintf(lineBuffer, "-:- %s sets %s topic to ", modenick, third);
		renderMain(lineBuffer, COLOR_BLUE);
		printFourthOnwards(COLOR_BLUE);
		free(first);
		free(second);
		free(modenick);
		return true;
	}

	else if (!strcmp(second, "INVITE")) //Invite message
	{
		//get usernick
		char* invitenick = getUserNick(first);

		//TODO THIS IS IN BAD STYLE, REDO THE #define printFourth....
		sprintf(lineBuffer, "-:- %s invites you to ", invitenick);
		renderMain(lineBuffer, COLOR_BLUE);
		printFourthOnwards(COLOR_BLUE);
		free(first);
		free(second);
		free(invitenick);
		return true;
	}
	else if (!strcmp(second, "PRIVMSG")) //Private message
	{
			//Get the place they are messaging (chan or 
		char* thirdSpace = strchr(secondSpace+1, ' ');
		if (thirdSpace == NULL)
		{
			renderMain("MALFORMED PRIVMSG\r\n", COLOR_RED);
			free(first);
			free(second);
			return false;
		}
		thirdSpace[0] = '\0';
		char* sendTarget = strdup(secondSpace+1);
		thirdSpace[0] = ' ';

		//get usernick
		char* privatenick = getUserNick(first);
		
		//Test and alter if its an action
		char* third = strchr(secondSpace+1, ' ');
		third++;
		if (third[0] == ':') third++;
		if (third[0] == 1)//ACTION OR VERSION OR something else???
		{
			third++;
			char x = third[6];
			third[6] = '\0';
			if (!strcmp(third, "ACTION")) //is an ACTION
			{
				third[6] = x;
				char* endmark = strchr(third, 1);
				if (endmark == NULL) //Should of been an action but its not :(
				{
					free(first);
					free(second);
					free(privatenick);
					return false;
				}
				endmark[0] = '\r';
				endmark[1] = '\n';
				endmark[2] = '\0';
				
				int color;
				
				if (!strcmp(sendTarget,yournick)) //PM Action
					color = COLOR_PURPLE;
				else
					color = COLOR_GREEN;
				sprintf(lineBuffer, "* %s %s", privatenick, third+7); //+7 to skip ACTION
				renderMain(lineBuffer, color);
				free(first);
				free(second);
				free(privatenick);
				return true;
			}
			else if (!strcmp(third, "VERSIO"))//is a version request, 6 chars as we only have 6
			{
				//send a version reply :)
//		char* privatenick = getUserNick(first);
				sprintf(lineBuffer, "NOTICE %s :%cVERSION peldet 0.8b on PSP :)%c\r\n", privatenick, 1, 1);
				networkSend(lineBuffer);
				sprintf(lineBuffer, "-:- Version Request from %s\r\n", privatenick);
				renderMain(lineBuffer, COLOR_GREEN);
				
				free(first);
				free(second);
				free(privatenick);
				return true;
			}
		}
		
		//TODO THIS IS IN BAD STYLE, REDO THE #define printFourth....
		if (!strcmp(sendTarget,yournick))
		{
			sprintf(lineBuffer, "[%s] ", privatenick); //Message to you
			renderMain(lineBuffer, COLOR_PURPLE);
			printFourthOnwards(COLOR_PURPLE);
		}
		else
		{
			sprintf(lineBuffer, "<%s> ", privatenick); //Chan Message
			renderMain(lineBuffer, COLOR_GREY);
			printFourthOnwards(COLOR_GREY);
		}
		free(first);
		free(second);
		free(privatenick);
		return true;
	}

	
	free(first);
	free(second);
	return false;
}

//Parses a :WiZ!jto@tolsun.oulu.fi string to find their nick
char* getUserNick(char* nickString)
{
	if (nickString[0] == ':') nickString++;
	char* exPlace = strchr(nickString, '!');

	if (exPlace == NULL) return nickString;	//Probally a server or something...
	
	exPlace[0] = '\0';
	char* thenick = strdup(nickString);
	exPlace[0] = '!';
	return thenick;
}
