/* $Id: USBLCD.c,v 1.7 2002/08/19 09:11:34 reinelt Exp $
 *
 * Driver for USBLCD ( see http://www.usblcd.de )
 * This Driver is based on HD44780.c
 *
 * Copyright 2002 by Robin Adams, Adams IT Services ( info@usblcd.de )
 * Copyright 1999, 2000 by Michael Reinelt (reinelt@eunet.at)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * $Log: USBLCD.c,v $
 * Revision 1.7  2002/08/19 09:11:34  reinelt
 * changed HD44780 to use generic bar functions
 *
 * Revision 1.6  2002/08/19 07:52:19  reinelt
 * corrected type declaration of (*defchar)()
 *
 * Revision 1.5  2002/08/19 07:36:29  reinelt
 *
 * finished bar.c, USBLCD is the first driver that uses the generic bar functions
 *
 * Revision 1.4  2002/08/19 04:41:20  reinelt
 * introduced bar.c, moved bar stuff from display.h to bar.h
 *
 * Revision 1.3  2002/08/18 08:11:11  reinelt
 * USBLCD buffered I/O
 *
 * Revision 1.2  2002/08/17 14:14:21  reinelt
 *
 * USBLCD fixes
 *
 * Revision 1.0  2002/07/08 12:16:10  radams
 *
 * first version of the USBLCD driver
 *
 */

/* 
 *
 * exported fuctions:
 *
 * struct LCD UDBLCD[]
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <sys/time.h>

#include "debug.h"
#include "cfg.h"
#include "display.h"
#include "bar.h"
#include "udelay.h"

#define GET_HARD_VERSION	1
#define GET_DRV_VERSION		2

#define XRES 5
#define YRES 8
#define CHARS 8

static LCD Lcd;

int usblcd_file;

static char *Port=NULL;
static char Txt[4][40];

static unsigned char  Buffer[1024];
static unsigned char *BufPtr;

static void USBLCD_send ()
{
  // struct timeval now, end;
  // gettimeofday (&now, NULL);
  
  write(usblcd_file,Buffer,BufPtr-Buffer);

  // gettimeofday (&end, NULL);
  // debug ("send %d: %d usec (%d usec/byte)", BufPtr-Buffer, 1000000*(end.tv_sec-now.tv_sec)+end.tv_usec-now.tv_usec, (1000000*(end.tv_sec-now.tv_sec)+end.tv_usec-now.tv_usec)/(BufPtr-Buffer));

  BufPtr=Buffer;

}


static void USBLCD_command (unsigned char cmd)
{
  *BufPtr++='\0';
  *BufPtr++=cmd;
}

static void USBLCD_write (char *string, int len)
{
  while (len--) {
    if(*string==0) *BufPtr++=*string;
    *BufPtr++=*string++;
  }
}


static int USBLCD_open (void)
{
  char buf[128];
  int major,minor;

  usblcd_file=open(Port,O_WRONLY);
  if (usblcd_file==-1) {
    error ("USBLCD: open(%s) failed: %s", Port, strerror(errno));
    return -1;
  }
  
  memset(buf,0,sizeof(buf));
  if (ioctl(usblcd_file,GET_DRV_VERSION, buf)!=0) {
    error ("USBLCD: ioctl() failed, could not get Driver Version!");
    return -2;
  }
  debug("Driver Version: %s",buf);

  if (sscanf(buf,"USBLCD Driver Version %d.%d",&major,&minor)!=2) {
    error("USBLCD: could not read Driver Version!");
    return -4;
  }
  if (major!=1) {
    error("USBLCD: Driver Version not supported!");
    return -4;
  }

  memset(buf,0,sizeof(buf));
  if (ioctl(usblcd_file,GET_HARD_VERSION, buf)!=0) {
    error ("USBLCD: ioctl() failed, could not get Hardware Version!");
    return -3;
  }
  debug("Hardware Version: %s",buf);

  if (sscanf(buf,"%d.%d",&major,&minor)!=2) {
    error("USBLCD: could not read Hardware Version!");
    return -4;
  };
  if (major!=1) {
    error("USBLCD: Hardware Version not supported!");
    return -4;
  }
  
  BufPtr=Buffer;

  USBLCD_command (0x29); // 8 Bit mode, 1/16 duty cycle, 5x8 font
  USBLCD_command (0x08); // Display off, cursor off, blink off
  USBLCD_command (0x0c); // Display on, cursor off, blink off
  USBLCD_command (0x06); // curser moves to right, no shift

  return 0;
}

static void USBLCD_define_char (int ascii, char *buffer)
{
  USBLCD_command (0x40|8*ascii);
  USBLCD_write (buffer, 8);
}


int USBLCD_clear (void)
{
  int row, col;

  for (row=0; row<Lcd.rows; row++) {
    for (col=0; col<Lcd.cols; col++) {
      Txt[row][col]='\t';
    }
  }

  bar_clear();

  USBLCD_command (0x01); // clear display

  return 0;
}


int USBLCD_init (LCD *Self)
{
  int rows=-1, cols=-1 ;
  char *port,*s ;

  if (Port) {
    free(Port);
    Port=NULL;
  }
  if ((port=cfg_get("Port"))==NULL || *port=='\0') {
    error ("USBLCD: no 'Port' entry in %s", cfg_file());
    return -1;
  }
  if (port[0]=='/') {
    Port=strdup(port);
  } else {
    Port=(char *)malloc(5/*/dev/ */+strlen(port)+1);
    sprintf(Port,"/dev/%s",port);
  }

  debug ("using device %s ", Port);

  s=cfg_get("Size");
  if (s==NULL || *s=='\0') {
    error ("USBLCD: no 'Size' entry in %s", cfg_file());
    return -1;
  }
  if (sscanf(s,"%dx%d",&cols,&rows)!=2 || rows<1 || cols<1) {
    error ("USBLCD: bad size '%s'",s);
    return -1;
  }

  Self->rows=rows;
  Self->cols=cols;
  Lcd=*Self;
  
  if (USBLCD_open()!=0)
    return -1;
  
  bar_init(rows, cols, XRES, YRES, CHARS);
  bar_add_segment(  0,  0,255, 32); // ASCII  32 = blank
  bar_add_segment(255,255,255,255); // ASCII 255 = block

  USBLCD_clear();
  USBLCD_send();

  return 0;
}


void USBLCD_goto (int row, int col)
{
  int pos=(row%2)*64+(row/2)*20+col;
  USBLCD_command (0x80|pos);
}


int USBLCD_put (int row, int col, char *text)
{
  char *p=&Txt[row][col];
  char *t=text;
  
  while (*t && col++<=Lcd.cols) {
    *p++=*t++;
  }
  return 0;
}


int USBLCD_bar (int type, int row, int col, int max, int len1, int len2)
{
  return bar_draw (type, row, col, max, len1, len2);
}


int USBLCD_flush (void)
{
  char buffer[256];
  char *p;
  int c, row, col;
  
  bar_process(USBLCD_define_char);

  for (row=0; row<Lcd.rows; row++) {
    for (col=0; col<Lcd.cols; col++) {
      c=bar_peek(row, col);
      if (c!=-1) {
	Txt[row][col]=(char)c;
      }
    }
    for (col=0; col<Lcd.cols; col++) {
      if (Txt[row][col]=='\t') continue;
      USBLCD_goto (row, col);
      for (p=buffer; col<Lcd.cols; col++, p++) {
	if (Txt[row][col]=='\t') break;
	*p=Txt[row][col];
      }
      USBLCD_write (buffer, p-buffer);
    }
  }

  USBLCD_send();

  return 0;
}


int USBLCD_quit (void)
{
  USBLCD_send();
  debug ("closing port %s", Port);
  close(usblcd_file);
  return 0;
}


LCD USBLCD[] = {
  { name: "USBLCD",
    rows:  0,
    cols:  0,
    xres:  XRES,
    yres:  YRES,
    bars:  BAR_L | BAR_R | BAR_U | BAR_D | BAR_H2,
    gpos:  0,
    init:  USBLCD_init,
    clear: USBLCD_clear,
    put:   USBLCD_put,
    bar:   USBLCD_bar,
    gpo:   NULL,
    flush: USBLCD_flush,
    quit:  USBLCD_quit 
  },
  { NULL }
};