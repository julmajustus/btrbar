/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:25:41 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/02 04:09:33 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_H
#define CONFIG_H

#include "blocks.h"
#define BAR_WIDTH			1920
#define BAR_HEIGHT			30

// bar colors
#define BG_COLOR			0xff0a0d0f
#define FG_COLOR			0xffbbbbbb
#define L_GREEN				0xff82df94
#define GREEN				0xff00ff44
#define GREY				0xff343a40
#define CYAN				0xff9ee9ea
#define PURPLE				0xffe49186
#define PINK				0xffff22aa

// enable built in dwl blocks
#define LAYOUT				1
#define TITLE				1
#define TAGS				1
#define TRAY				1

// tags
#define N_TAGS				17
#define TAG_FG_ACTIVE   	0xff9ee0ea
#define TAG_FG_OCCUPIED 	0xffbbbbbb
#define TAG_FG_EMPTY    	0xff343a40
#define TAG_FG_URGENT   	0xffE49186
#define TAG_BG_COLOR    	0xff0a0d0f
#define TAG_ICON_PADDING	14
#define TAG_WIDTH			24

static const char * const tag_icons[N_TAGS] = {
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	""
};

// tray

#define TRAY_ICON_PADDING	6
#define TRAY_MENU_W			200
// center 0, top_left 1, top_right 3
#define TRAY_MENU_LOCATION	3
#define TRAY_MENU_BG_COLOR	0xff0a0d0f
#define TRAY_MENU_HOVER_BG_COLOR	0xff0a6f0f
#define TRAY_MENU_FG_COLOR	0xffbbbbbb
#define TRAY_MENU_DISABLED_FG_COLOR 0xffE49186
// font
#define FONT "/usr/share/fonts/TTF/Iosevka Bold Nerd Font Complete.ttf" 
// fontsize
#define F_SIZE				30

// padding between blocks px
#define BLOCK_PADDING		4
// screen edge padding px
#define EDGE_PADDING		8

// block count
#define N_BLOCKS			12

/* block config */
static const block_t blocks_cfg[N_BLOCKS] = {

	{ 
		BLK_TRAY,		/*type*/ 
		"",				/*cmd*/
		"",				/*prefix*/
		NULL,			/*builtin function*/
		0,				/*pfx color*/
		0,				/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		0,				/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_SCRIPT,		/*type*/ 
		"~/.local/bin/sb/sb-nettraf", /*cmd*/
		"  ",			/*prefix*/
		NULL,			/*builtin function*/
		0xff992244,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		5*1000,			/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_VOL,		/*type*/ 
		"wpctl get-volume @DEFAULT_AUDIO_SINK@ | tr -d 'Volume: '", /*cmd*/
		"  ",			/*prefix*/
		NULL,			/*builtin function*/
		L_GREEN,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		vol_click,		/*on click*/
		vol_scroll,		/*on click*/
		ALIGN_RIGHT,	/*align*/
		90*1000,		/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_FUNC,		/*type*/ 
		NULL,			/*cmd*/
		"  ",			/*prefix*/
		mem_usage_simple,	/*builtin function*/
		0xff00ff44,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		60*1000,		/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_SCRIPT,		/*type*/ 
		"/home/juftuf/.local/bin/get_cpu_and_gpu_power_usage", /*cmd*/
		" ",			/*prefix*/
		NULL,			/*builtin function*/
		L_GREEN,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		power_consumption_click, /*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		7*1000,			/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_TEMP,		/*type*/ 
		"/sys/class/drm/card0/device/hwmon/hwmon3/temp1_input", /*cmd*/
		"  ",			/*prefix*/
		NULL,			/*builtin function*/
		0xffe6ffff,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		10*1000,		/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_TEMP,		/*type*/ 
		"/sys/class/hwmon/hwmon5/temp1_input", /*cmd*/
		" ",			/*prefix*/
		NULL,			/*builtin function*/
		0xffe6ffff,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		10*1000,		/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_FUNC,		/*type*/ 
		NULL,			/*cmd*/
		"  ",			/*prefix*/
		cpu_usage,		/*builtin function*/
		0xfffbb0b0,		/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		1*1000,			/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_FUNC,		/*type*/ 
		NULL,			/*cmd*/
		"  ",			/*prefix*/
		_clock,			/*builtin function*/
		CYAN,			/*pfx color*/
		FG_COLOR,		/*fg color*/
		BG_COLOR,		/*bg color*/
		clock_click,	/*on click*/
		NULL,			/*on scroll*/
		ALIGN_RIGHT,	/*align*/
		30*1000,		/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_TAG,		/*type*/ 
		"",				/*cmd*/
		"",				/*prefix*/
		NULL,			/*builtin function*/
		0,				/*pfx color*/
		0,				/*fg color*/
		0,				/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_LEFT,		/*align*/
		0,				/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_LAYOUT,		/*type*/ 
		"",				/*cmd*/
		"",				/*prefix*/
		NULL,			/*builtin function*/
		0,				/*pfx color*/
		CYAN,			/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_LEFT,		/*align*/
		0,				/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},

	{ 
		BLK_TITLE,		/*type*/ 
		"",				/*cmd*/
		"",				/*prefix*/
		NULL,			/*builtin function*/
		0,				/*pfx color*/
		L_GREEN,		/*fg color*/
		BG_COLOR,		/*bg color*/
		NULL,			/*on click*/
		NULL,			/*on scroll*/
		ALIGN_LEFT,		/*align*/
		0,				/*update interval ms*/
		0,				/*do not change */
		0,				/*do not change */
		0,				/*do not change */
		"",				/*do not change */
	},
};

#endif
