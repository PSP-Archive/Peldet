#include "telnet.h"
#include "irc.h"

#define protocols_count 3

struct {
	void* protocolMain;
	int stringCount; //Amount of additional strings that this protocol needs
	char* name;
	void* inputThread; //Type of input thread the protocol uses
} protocols[] =
{
	{ telnetProtocol, 0, "telnet",  charBasedInput },
	{ telnetProtocol, 0, "telnetmud", lineBasedInput },
	{ ircProtocol,    2, "irc",     lineBasedInput }
};
