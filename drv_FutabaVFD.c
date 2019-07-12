/* $Id: drv_FutabaVFD.c -1   $
 * $URL: https://ssl.bulix.org/svn/lcd4linux/trunk/drv_FutabaVFD.c $
 *
 * A driver to run Futaba VFD M402SD06GL with LCD4Linux on parallel port
 *
 * Copyright (C) 2005 Michael Reinelt <michael@reinelt.co.at>
 * Copyright (C) 2005, 2006, 2007 The LCD4Linux Team <lcd4linux-devel@users.sourceforge.net>
 * Copyright (C) 2012 Marcus Menzel <codingmax@gmx-topmail.de>
 *
 * This file is part of LCD4Linux.
 *
 * LCD4Linux is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * LCD4Linux is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* 
 *
 * exported fuctions:
 *
 * struct DRIVER drv_FutabaVFD
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "debug.h"
#include "cfg.h"
#include "qprintf.h"
#include "udelay.h"
#include "plugin.h"
#include "widget.h"
#include "widget_text.h"
#include "widget_icon.h"
#include "widget_bar.h"
#include "drv.h"

#include "drv_generic_text.h"
#include "drv_generic_parport.h"

#include "timer.h"
#include "action.h"


#define NAME "FutabaVFD"
#define RELEASE NAME " 0.0.2"

#define PWR_ON 1
#define PWR_OFF 0

static unsigned char SIGNAL_WR;
static unsigned char SIGNAL_SELECT;
static unsigned char SIGNAL_TEST;	/*test mode not implemented */
static unsigned char SIGNAL_BUSY;
static unsigned char SIGNAL_PWR_ON;
static unsigned char SIGNAL_PWR_5V;

static unsigned char SIGNAL_PWR_5V;
static unsigned char SIGNAL_PWR_5V;

static int pwrState = PWR_OFF;
static int dim = 0;		/* brightness 0..3 */

static char *displayBuffer = NULL;
static Action *firstAction = NULL;

static void my_reset(void);

static void my_set_pwr_state(int newState)
{

    if (pwrState != newState) {
	pwrState = newState;
	if (pwrState == PWR_ON)
	    my_reset();
	action_trigger(firstAction, (pwrState == PWR_ON) ? "poweron" : "poweroff");
    }
}


/* sending one byte over the wire 
 * returns PWR_ON if VFD powered and write ok 
 * else PWR_OFF
 */
static int my_writeChar_help(const unsigned char c)
{

    int i;
    unsigned char status;

    drv_generic_parport_data(c);
    drv_generic_parport_control(SIGNAL_WR | SIGNAL_SELECT, 0);
    ndelay(60);
    drv_generic_parport_control(SIGNAL_WR, 0xFF);

    /*wait <=60ns till busy */
    for (i = 0; i < 60; i++) {
	status = drv_generic_parport_status();
	if (!(status & SIGNAL_PWR_ON))
	    return PWR_OFF;
	if (!(status & SIGNAL_BUSY))	/* signal inverted */
	    break;
	ndelay(1);
    }

    ndelay(60 - i);
    drv_generic_parport_control(SIGNAL_SELECT, 0xFF);
    drv_generic_parport_control(SIGNAL_WR, 0);

    /*wait max 32000ns till not busy */
    for (i = 0; i < 32000; i++) {
	status = drv_generic_parport_status();
	if (!(status & SIGNAL_PWR_ON))
	    return PWR_OFF;
	if ((status & SIGNAL_BUSY))	/* signal inverted */
	    break;
	ndelay(1);
    }

    ndelay(210);
    return PWR_ON;
}


static int my_writeChar(const unsigned char c)
{

    if (pwrState == PWR_ON)
	my_set_pwr_state(my_writeChar_help(c));

    return pwrState;
}


static void my_goto(const unsigned char pos)
{
    my_writeChar(0x10);
    my_writeChar(pos);
}


static void my_brightness(const int brightness)
{
    if (brightness < 0)
	dim = 0;
    else if (brightness > 3)
	dim = 3;
    else
	dim = brightness;
    unsigned char val = (dim < 3) ? (1 + dim) * 0x20 : 0xFF;
    my_writeChar(0x04);
    my_writeChar(val);
}


static void my_reset(void)
{
    my_writeChar(0x1F);		/*reset */
    my_writeChar(0x14);		/*hide cursor */
    my_writeChar(0x11);		/*DC1 - normal display */
    my_brightness(dim);

    if (displayBuffer == NULL)
	return;

    int r, c;
    for (r = 0; r < DROWS; r++) {
	my_goto(r * DCOLS);
	for (c = 0; c < DCOLS; c++)
	    if (my_writeChar(displayBuffer[r * DCOLS + c]) != PWR_ON)
		return;
    }
}


static int my_open(const char *section)
{

    if (drv_generic_parport_open(section, NAME) != 0) {
	error("%s: could not initialize parallel port!", NAME);
	return -1;
    }

    /* hardwired */

    if ((SIGNAL_WR = drv_generic_parport_hardwire_ctrl("WR", "STROBE")) == 0xFF)
	return -1;
    if ((SIGNAL_SELECT = drv_generic_parport_hardwire_ctrl("SEL", "SLCTIN")) == 0xFF)
	return -1;
    if ((SIGNAL_TEST = drv_generic_parport_hardwire_ctrl("TEST", "AUTOFD")) == 0xFF)
	return -1;
    if ((SIGNAL_PWR_5V = drv_generic_parport_hardwire_ctrl("PWR_5V", "INIT")) == 0xFF)
	return -1;
    if ((SIGNAL_BUSY = drv_generic_parport_hardwire_status("BUSY", "BUSY")) == 0xFF)
	return -1;
    if ((SIGNAL_PWR_ON = drv_generic_parport_hardwire_status("PWR_ON", "PAPEROUT")) == 0xFF)
	return -1;

    /* set all signals but WR to high */
    drv_generic_parport_control(SIGNAL_SELECT | SIGNAL_TEST | SIGNAL_PWR_5V, 0xFF);
    drv_generic_parport_control(SIGNAL_WR, 0);

    /* set direction: write */
    drv_generic_parport_direction(0);

    return 0;
}


static void my_write(const int row, const int col, const char *data, const int len)
{
    if (displayBuffer != NULL)
	memcpy(displayBuffer + (DCOLS * row + col), data, len);

    my_goto(row * DCOLS + col);
    int i;
    for (i = 0; i < len; i++)
	if (my_writeChar(*data++) != PWR_ON)
	    break;
}


static void my_defchar(const int ascii, const unsigned char *matrix)
{
    return;			// no defchars
}


/* start text mode display */
static int my_start(const char *section)
{

    int brightness;
    int rows = -1, cols = -1;
    char *s;


    s = cfg_get(section, "Size", NULL);
    if (s == NULL || *s == '\0') {
	error("%s: no '%s.Size' entry from %s", NAME, section, cfg_source());
	return -1;
    }
    if (sscanf(s, "%dx%d", &cols, &rows) != 2 || rows < 1 || cols < 1) {
	error("%s: bad %s.Size '%s' from %s", NAME, section, s, cfg_source());
	free(s);
	return -1;
    }

    DROWS = rows;
    DCOLS = cols;


    const char *names[] = { "poweron", "poweroff", NULL };
    action_init_multiple(section, names, &firstAction);


    /* open communication with the display */
    if (my_open(section) < 0)
	return -1;

    if (cfg_number(section, "brightness", 0, 0, 255, &brightness) > 0)
	dim = brightness;

    return 0;
}



/****************************************/
/***            plugins               ***/
/****************************************/

static void plugin_brightness(RESULT * result, const int argc, RESULT * argv[])
{

    double d = dim;

    switch (argc) {
    case 0:
	break;
    case 1:
	my_brightness(R2N(argv[0]));
	d = dim;
	break;
    default:
	error("LCD::brightness(): wrong number of parameters.");
	d = -1;
    }
    SetResult(&result, R_NUMBER, &d);
}

static void plugin_power(RESULT * result)
{

    double res = (pwrState == PWR_ON) ? 1 : 0;
    SetResult(&result, R_NUMBER, &res);
}

/****************************************/
/***        exported functions        ***/
/****************************************/


/* list models */
int my_list(void)
{
    printf("Futaba VFD M402SD06GL");
    return 0;
}


static void my_pwr_check_callback(void *null)
{

    int newState = (drv_generic_parport_status() & SIGNAL_PWR_ON)
	? PWR_ON : PWR_OFF;

    my_set_pwr_state(newState);
}


/* initialize driver & display */
/* use this function for a text display */
int my_init(const char *section, const int quiet)
{
    info("Init %s %s.", NAME, RELEASE);

    WIDGET_CLASS wc;

    XRES = 5;			/* pixel width of one char  */
    YRES = 7;			/* pixel height of one char  */
    GOTO_COST = 2;		/* number of bytes a goto command requires */

    /* real worker functions */
    drv_generic_text_real_write = my_write;
    drv_generic_text_real_defchar = my_defchar;

    /* start display */
    int ret;
    if ((ret = my_start(section)) != 0)
	return ret;

    /* initialize generic text driver */
    if ((ret = drv_generic_text_init(section, NAME)) != 0)
	return ret;

    /* initialize generic icon driver */
    if ((ret = drv_generic_text_icon_init()) != 0)
	return ret;

    /* initialize generic bar driver */
    if ((ret = drv_generic_text_bar_init(1)) != 0)
	return ret;

    /* add fixed chars to the bar driver */
    drv_generic_text_bar_add_segment(0, 0, 255, '_');
    drv_generic_text_bar_add_segment(255, 255, 255, 0x7F);	/* full dot */

    /* register text widget */
    wc = Widget_Text;
    wc.draw = drv_generic_text_draw;
    widget_register(&wc);

    /* register icon widget */
    wc = Widget_Icon;
    wc.draw = drv_generic_text_icon_draw;
    widget_register(&wc);

    /* register bar widget */
    wc = Widget_Bar;
    wc.draw = drv_generic_text_bar_draw;
    widget_register(&wc);

    /* register plugins */
    AddFunction("LCD::brightness", -1, plugin_brightness);
    AddFunction("LCD::power", 0, plugin_power);

    my_reset();			/* clear display */

    if (!quiet && drv_generic_text_greet(NULL, NULL)) {
	sleep(3);
    }

    displayBuffer = malloc(DROWS * DCOLS * sizeof(char));
    if (displayBuffer != NULL)
	memset(displayBuffer, 0x20, DROWS * DCOLS);
    else
	return -1;

    timer_add(my_pwr_check_callback, NULL, 100, 0);

    return 0;
}


/* close driver & display */
/* use this function for a text display */
int my_quit(const int quiet)
{
    info("%s: shutting down.", NAME);

    timer_remove(my_pwr_check_callback, NULL);

    free(displayBuffer);
    displayBuffer = NULL;

    action_free_all(firstAction);
    firstAction = NULL;

    my_reset();
    my_write(0, 0, "Goodbye...", 10);

    drv_generic_text_quit();
    debug("closing connection");
    drv_generic_parport_close();
    return (0);
}

/* CHECKED */
DRIVER drv_FutabaVFD = {
    .name = NAME,
    .list = my_list,
    .init = my_init,
    .quit = my_quit,
};
