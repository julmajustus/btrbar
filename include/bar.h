/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bar.h                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 10:13:41 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 01:55:22 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BAR_H
#define BAR_H

#include "dwl-ipc-unstable-v2.h"
#include "stb_truetype.h"
#include "config.h"
#include "blocks.h"
#include "tags.h"

#include <stddef.h>
#include <stdint.h>
#include <wayland-client.h>
#include <dbus-1.0/dbus/dbus.h>


#define MAX_OUTPUTS 4

typedef struct systray_t systray_t;
typedef struct bar_manager_t bar_manager_t;

enum run_status {
	RUN_OK = 0,
	RUN_TERM = 1,
	RUN_WAYLAND_DISCONNECT = -2
};

typedef struct {
	uint8_t						  cur_buf;
	struct wl_buffer			  *buffers[2];
	uint32_t					  *argb_buf;
	uint32_t					  *argb_bufs[2]; 

	block_t						  local_blocks[N_BLOCKS];
	block_inst_t				  block_inst[N_BLOCKS];
	tag_t						  *tags;
	systray_t					  *tray;
	uint8_t						  needs_redraw;

	uint32_t					  last_x;
	uint32_t					  last_y;

	struct wl_display			  *display;
	struct wl_registry			  *registry;
	struct wl_compositor		  *compositor;
	struct zwlr_layer_shell_v1	  *layer_shell;
	struct wl_surface			  *surface;
	struct zwlr_layer_surface_v1  *layer_surface;
	struct wl_callback			  *frame_cb;
	struct zdwl_ipc_manager_v2	  *ipc_mgr;
	struct zdwl_ipc_output_v2	  *ipc_out;
	struct wl_seat				  *seat;
	struct wl_pointer			  *pointer;
	struct wl_output			  *output;
	uint32_t					  output_id;
	uint32_t					  width;
	uint32_t					  height;

	struct wl_shm				  *shm;
	struct wl_shm_pool			  *shm_pool;
	int							  shm_fd;
	size_t						  pool_size;
	stbtt_fontinfo				  font;
	uint8_t						  is_focused;
	uint8_t						  is_clicked;
	bar_manager_t				  *bar_manager;
}	bar_t;


typedef struct {
	char						  *service;
	char						  *path;
	char						  *menu_path;
	char						  *iface;
	uint32_t					  width, height;
	uint32_t					  *pixels;
	struct wl_buffer			  *buffer;
	int							  shm_fd;
	uint32_t					  x0;
	uint32_t					  x1;
	systray_t					  *tray;
} tray_item_t;

typedef struct MenuItem {
	int32_t						  id;
	char						  *label;
	uint8_t						  enabled;
	uint8_t						  visible;
	uint8_t						  toggle_state;
	uint8_t						  has_submenu;
	struct MenuItem				  **children;
	uint32_t					  n_children;
}	MenuItem;

typedef struct PopupMenu {
	uint8_t						  active;
	uint32_t					  x, y, w, h;
	uint32_t					  *argb_buf;
	int							  shm_fd;
	struct wl_buffer			  *wl_buf;      
	struct wl_surface			  *surf;
	struct zwlr_layer_surface_v1  *layer;
	MenuItem					  **items;
	uint32_t						  n_items;
	MenuItem					  *parent; 
	uint32_t						  parent_index;
	tray_item_t					  *parent_item;
	uint8_t						  highlighted;
	uint8_t						  is_inside_menu;
}	PopupMenu;

// Tray context
struct  systray_t{
	DBusConnection				  *conn;
	tray_item_t					  items[MAX_TRAY_ITEMS];
	uint32_t						  n_items;
	bar_t						  *bar;
	bar_manager_t				  *manager;
	PopupMenu					  menu;
};

struct bar_manager_t {
	struct wl_display			  *display;
	struct wl_registry			  *registry;
	struct wl_compositor		  *compositor;
	struct zwlr_layer_shell_v1	  *layer_shell;
	struct zdwl_ipc_manager_v2	  *ipc_mgr;
	struct wl_seat				  *seat;
	struct wl_shm				  *shm;
	bar_t						  *bars[MAX_OUTPUTS];
	uint8_t						  n_bars;

	block_t						  shared_blocks[N_BLOCKS];
	const block_cfg_t			  *block_cfg;
	systray_t					  *tray;

	unsigned char				  *ttf_buffer;
	stbtt_fontinfo				  font;
};

#include "systray.h"

int		init_manager(bar_manager_t *m);
int		init_bar(bar_manager_t *m, struct wl_output *wlo, uint32_t id);
int		bar_blocks_init(bar_t *b, bar_manager_t *m);
int		run(bar_manager_t *m);
void	cleanup(bar_manager_t *m);

#endif
