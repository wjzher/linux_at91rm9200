#ifndef _SED1335_H_
#define _SED1335_H_

#define SETSEDSYS	0x40
#define SEDMWRITE	0x42
#define SEDMREAD	0x43
#define SEDCRSW		0x46
#define SEDCRSR		0x47
#define SEDSCROLL	0x44
#define SEDCSRDIR	0x4C
#define SEDHDOT		0x5A
#define SEDOVLAY	0x5B
#define SEDOFF		0x58
#define SEDON		0x59
#define SEDSAD1		0
#define SEDSAD2		0x4B0
#define SEDSAD3		0x2A30
#define SEDSIZE		320 * 240

#define SED1335_DATA_ADDR 0x40000000
#define SED1335_CMD_ADDR 0x40000001
#define SED1335_MAJOR 139
#define SMC3_ADDR 0xffffff7c

#define SETCURSOR	0xC8
#define RDCURSOR	0xC5
#define INITSED		0xC7
#define CLEARSED		0xC9
#define SETCURSORUP		0xCA
#define SETCURSORDOWN	0xCB
#define SETCURSORLEFT	0xCC
#define SETCURSORRIGHT	0xCD
#define SETOVLAY		0xC6

struct sed_data {
	unsigned short addr;
	char *data;
};

#endif
