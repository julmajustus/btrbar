/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bar.h                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 10:13:41 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 20:53:21 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BAR_H
#define BAR_H

#include "stb_truetype.h"
#include <stddef.h>
#include <stdint.h>
#include <wayland-client.h>
#include "config.h"
#include "blocks.h"
#include "tags.h"
#include <dbus-1.0/dbus/dbus.h>

#define MAX_TRAY_ITEMS 64
#define MAX_OUTPUTS 4
typedef struct systray_t systray_t;

typedef struct {
	int							  cur_buf;
	struct wl_buffer			  *buffers[2];
	uint32_t					  *argb_buf;
	uint32_t					  *argb_bufs[2]; 

	uint8_t						  needs_redraw;
	block_t						  *blocks;
	tag_t						  *tags;

	int							  last_x;
	int							  last_y;
	systray_t					  *tray;
	struct wl_display			  *display;
	struct wl_registry			  *registry;
	struct wl_compositor		  *compositor;
	struct zwlr_layer_shell_v1	  *layer_shell;
	struct wl_surface			  *surface;
	struct zwlr_layer_surface_v1  *layer_surface;

	struct zdwl_ipc_manager_v2	  *ipc_mgr;
	struct zdwl_ipc_output_v2	  *ipc_out;
	struct wl_seat				  *seat;
	struct wl_pointer			  *pointer;
	struct wl_output			  *outputs[MAX_OUTPUTS];
	size_t						  n_outputs;
	uint32_t					  width;
	uint32_t					  height;

	struct wl_shm				  *shm;
	struct wl_shm_pool			  *shm_pool;
	int							  shm_fd;
	size_t						  pool_size;
	unsigned char				  *ttf_buffer;
	stbtt_fontinfo				  font;
}	bar_t;

typedef struct {
    char						  *service;
    char						  *path;
	char						  *iface;
    uint32_t					  width, height;
    uint32_t					  *pixels;
    struct wl_buffer			  *buffer;
    int							  shm_fd;
	int							  x0;
	int							  x1;
    systray_t					  *tray;
} tray_item_t;

typedef struct MenuItem {
    int32_t						  id;
    char						  *label;
    int							  enabled;
    int							  visible;
    int							  toggle_state;
    int							  has_submenu;
    struct MenuItem				  **children;
    size_t						  n_children;
}	MenuItem;

typedef struct PopupMenu {
    int							  active;
    uint32_t					  x, y, w, h;
	uint32_t					  *argb_buf;
    int							  shm_fd;
    struct wl_buffer			  *wl_buf;      
    struct wl_surface			  *surf;
    struct zwlr_layer_surface_v1  *layer;
    MenuItem					  **items;
    size_t						  n_items;
	MenuItem					  *parent; 
	size_t						  parent_index;
	tray_item_t					  *parent_item;
	int							  highlighted;
	int							  is_inside_menu;
}	PopupMenu;

// Tray context
struct  systray_t{
    DBusConnection				  *conn;
    tray_item_t					  items[MAX_TRAY_ITEMS];
    size_t						  n_items;
    bar_t						  *bar;
	PopupMenu					  menu;
};

#include "systray.h"

int		bar_init(bar_t *bar);
void	run(bar_t *bar);
void	cleanup(bar_t *bar);

#endif
