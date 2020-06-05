#ifndef _GPIOB_H_
#define _GPIOB_H_

#define GPIOB_MAJOR 135
#define GPIOB_IRQ 3
#define GPIOC_IRQ 4

#define KEYUP		0x1
#define KEYDOWN		0x10
#define KEYLEFT		0x2
#define KEYRIGHT	0x8
#define KEYCANCLE	0x20
#define KEYOK		0x4
#define KEYONOFF	0x80

#define CLEARKEY 0xEA
#define SETBIT	0xEB
#define CLEARBIT	0xEC
#define ENIOB		0xED
#define DISIOB		0xEE
#define GETNET	0xEF
#define GETHATL 0xF0
#define DISABLEKEY 0xF1
#define ENABLEKEY 0xF2

#endif
