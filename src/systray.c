/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   systray.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/25 14:59:08 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/02 02:57:01 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define _POSIX_C_SOURCE 200809L
#include "render.h"
#include "systray.h"
#include "systray_watcher.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

static MenuItem *parse_menu_item(DBusMessageIter *iter);
static void popup_menu_show(tray_item_t *it, bar_t *bar);
static void build_full_menu_tree(systray_t *tray, DBusMessageIter *arr);

static void
tray_trigger_event(tray_item_t *it, int id)
{
	char path[1024];
	snprintf(path, sizeof(path), "%s/Menu", it->path);
	DBusMessage *msg = dbus_message_new_method_call(
		it->service, path,
		"com.canonical.dbusmenu", "Event");
	DBusMessageIter args;
	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &id);
	const char *event = "clicked";
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &event);
	DBusMessageIter var;
	dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "s", &var);
	const char *data = "";
	dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &data);
	dbus_message_iter_close_container(&args, &var);
	uint32_t timestamp = (uint32_t)time(NULL);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &timestamp);

	dbus_connection_send(it->tray->conn, msg, NULL);
	dbus_connection_flush(it->tray->conn);
	dbus_message_unref(msg);
}

static void
tray_menu_reply(DBusPendingCall *pending, void *user_data)
{
	tray_item_t *it = user_data;
	systray_t *tray = it->tray;

	DBusMessage *reply = dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	if (!reply)
		return;

	if (tray->menu.active)
		popup_menu_hide(tray);

	DBusMessageIter root;
	dbus_message_iter_init(reply, &root);
	if (dbus_message_iter_get_arg_type(&root)==DBUS_TYPE_UINT32)
		dbus_message_iter_next(&root);
	DBusMessageIter struct_iter;
	dbus_message_iter_recurse(&root, &struct_iter);
	dbus_message_iter_next(&struct_iter);
	dbus_message_iter_next(&struct_iter);
	if (dbus_message_iter_get_arg_type(&struct_iter)==DBUS_TYPE_ARRAY) {
		DBusMessageIter arr;
		dbus_message_iter_recurse(&struct_iter, &arr);
		build_full_menu_tree(tray, &arr);
	}
	dbus_message_unref(reply);

	popup_menu_show(it, tray->bar);
}

static MenuItem *
parse_menu_item(DBusMessageIter *struct_iter)
{
	DBusMessageIter iter;
	dbus_message_iter_recurse(struct_iter, &iter);

	MenuItem *mi = calloc(1,sizeof *mi);
	if (!mi)
		return NULL;

	dbus_message_iter_get_basic(&iter, &mi->id);
	dbus_message_iter_next(&iter);

	mi->visible      = 1;
	mi->enabled      = 1;
	mi->label        = NULL;
	mi->toggle_state = -1;
	mi->has_submenu  = 0;

	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
		DBusMessageIter arr;
		dbus_message_iter_recurse(&iter, &arr);

		while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_DICT_ENTRY) {
			DBusMessageIter member, val;
			const char *key;

			dbus_message_iter_recurse(&arr, &member);
			dbus_message_iter_get_basic(&member, &key);
			dbus_message_iter_next(&member);

			dbus_message_iter_recurse(&member, &val);
			int vtype = dbus_message_iter_get_arg_type(&val);

			if (strcmp(key, "visible") == 0 &&
				vtype == DBUS_TYPE_BOOLEAN) {
				dbus_message_iter_get_basic(&val, &mi->visible);

			} else if (strcmp(key, "enabled") == 0 &&
				vtype == DBUS_TYPE_BOOLEAN) {
				dbus_message_iter_get_basic(&val, &mi->enabled);

			} else if (strcmp(key, "label") == 0 &&
				vtype == DBUS_TYPE_STRING) {
				dbus_message_iter_get_basic(&val, &mi->label);
				mi->label = strdup(mi->label);

			} else if (strcmp(key, "toggle-type") == 0 &&
				vtype == DBUS_TYPE_STRING) {
				const char *tt;
				dbus_message_iter_get_basic(&val, &tt);

			} else if (strcmp(key, "toggle-state") == 0 &&
				vtype == DBUS_TYPE_INT32) {
				dbus_message_iter_get_basic(&val, &mi->toggle_state);

			} else if (strcmp(key, "children-display") == 0 &&
				vtype == DBUS_TYPE_STRING) {
				const char *cd;
				dbus_message_iter_get_basic(&val, &cd);
				if (strcmp(cd, "submenu") == 0) {
					mi->has_submenu = 1;
				}
			}
			dbus_message_iter_next(&arr);
		}
	}

	dbus_message_iter_next(&iter);
	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
		DBusMessageIter carr;
		dbus_message_iter_recurse(&iter, &carr);

		size_t cnt = 0;
		for (DBusMessageIter tmp = carr;
		dbus_message_iter_get_arg_type(&tmp) == DBUS_TYPE_VARIANT;
		dbus_message_iter_next(&tmp))
		{
			DBusMessageIter sin;
			dbus_message_iter_recurse(&tmp, &sin);
			if (dbus_message_iter_get_arg_type(&sin) == DBUS_TYPE_STRUCT)
				cnt++;
		}

		if (cnt) {
			mi->children   = calloc(cnt, sizeof *mi->children);
			mi->n_children = cnt;
			size_t idx = 0;
			for (; dbus_message_iter_get_arg_type(&carr) == DBUS_TYPE_VARIANT;
			dbus_message_iter_next(&carr))
			{
				DBusMessageIter sin;
				dbus_message_iter_recurse(&carr, &sin);
				if (dbus_message_iter_get_arg_type(&sin)==DBUS_TYPE_STRUCT) {
					mi->children[idx++] = parse_menu_item(&sin);
				}
			}
		}
	}

	return mi;
}

static void
build_full_menu_tree(systray_t *tray, DBusMessageIter *arr)
{
	size_t cnt = 0;
	for (DBusMessageIter tmp = *arr;
	dbus_message_iter_get_arg_type(&tmp)==DBUS_TYPE_VARIANT;
	dbus_message_iter_next(&tmp))
	{
		DBusMessageIter sin;
		dbus_message_iter_recurse(&tmp, &sin);
		if (dbus_message_iter_get_arg_type(&sin)==DBUS_TYPE_STRUCT)
			cnt++;
	}
	if (!cnt)
		return;

	tray->menu.items   = calloc(cnt, sizeof *tray->menu.items);
	tray->menu.n_items = cnt;

	size_t idx = 0;
	for (; dbus_message_iter_get_arg_type(arr)==DBUS_TYPE_VARIANT;
	dbus_message_iter_next(arr))
	{
		DBusMessageIter sin;
		dbus_message_iter_recurse(arr,&sin);
		if (dbus_message_iter_get_arg_type(&sin)==DBUS_TYPE_STRUCT)
			tray->menu.items[idx++] = parse_menu_item(&sin);
	}
	tray->menu.parent   = NULL;
	tray->menu.active   = 0;
}

void
free_menu_tree(MenuItem **items, size_t n)
{
	if (!items)
		return;
	for (size_t i = 0; i < n; i++) {
		MenuItem *mi = items[i];
		free(mi->label);

		free_menu_tree(mi->children, mi->n_children);

		free(mi);
	}
	free(items);
}

static int
create_shm_file(size_t size)
{
	char tmpl[] = "/tmp/btrbar-shm-XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0)
		return -1;
	unlink(tmpl);
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static struct wl_buffer *
create_shm_buffer(bar_t *b, uint32_t w, uint32_t h, uint32_t *pixels, int *fd_out)
{
	int stride = w * 4;
	size_t size = stride * h;
	int fd = create_shm_file(size);
	if (fd < 0)
		return NULL;
	void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	memcpy(mem, pixels, size);
	struct wl_shm_pool *pool = wl_shm_create_pool(b->shm, fd, size);
	struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	munmap(mem, size);
	*fd_out = fd;
	return buf;
}

static void
layer_configure(void *data,
				struct zwlr_layer_surface_v1 *layer_surf,
				uint32_t serial,
				uint32_t width,
				uint32_t height)
{
	(void)width, (void)height;
	zwlr_layer_surface_v1_ack_configure(layer_surf, serial);

	bar_t *b = data;

	systray_render_popup(b);
	wl_surface_attach(b->tray->menu.surf, b->tray->menu.wl_buf, 0, 0);
	wl_surface_damage(b->tray->menu.surf, 0, 0, b->tray->menu.w, b->tray->menu.h);
	wl_surface_commit(b->tray->menu.surf);
}

static void
layer_closed(void *data,
			 struct zwlr_layer_surface_v1 *layer_surf)
{
	(void)data, (void)layer_surf;
}

static const struct zwlr_layer_surface_v1_listener layer_listeners = {
	.configure = layer_configure,
	.closed    = layer_closed,
};

static void
popup_menu_show(tray_item_t *it, bar_t *b)
{
	PopupMenu *m = &b->tray->menu;
	int pad = 8;
	int line_h = F_SIZE + 4;
	int max_w = 0;
	for (size_t i=0;i<m->n_items;i++) {
		int tw = text_width_px(b, m->items[i]->label);
		max_w = max_w < tw ? tw : max_w;
	}
	m->w = max_w + pad * 2;
	m->h = line_h * m->n_items;

	int x = b->last_x;
	int y = b->height + 10;
	if (x + m->w > b->width)
		x = b->width - m->w;
	m->x = x;
	m->y = y;

	if (m->argb_buf)
		free(m->argb_buf);
	m->argb_buf = calloc(m->w * m->h, sizeof *m->argb_buf);

	if (m->layer)
		zwlr_layer_surface_v1_destroy(m->layer);
	if (m->surf)
		wl_surface_destroy(m->surf);
	m->surf = wl_compositor_create_surface(b->compositor);
	m->layer = zwlr_layer_shell_v1_get_layer_surface(
		b->layer_shell, m->surf,
		NULL,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		"systray-menu"
	);
	zwlr_layer_surface_v1_set_size(m->layer, m->w, m->h);
	zwlr_layer_surface_v1_set_anchor(m->layer,
								  ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
								  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_margin(m->layer, b->height + 10, 0, 0, 0);
	zwlr_layer_surface_v1_set_exclusive_zone(m->layer, -1);
	zwlr_layer_surface_v1_add_listener(m->layer, &layer_listeners, b);
	wl_surface_commit(m->surf);
	m->active = 1;
	m->parent_item = it;
	m->highlighted = -1;
	m->is_inside_menu = 0;
}

void
popup_menu_hide(systray_t *tray)
{
	PopupMenu *m = &tray->menu;
	if (!m->active)
		return;

	if (m->wl_buf) {
		wl_buffer_destroy(m->wl_buf);
		m->wl_buf = NULL;
	}
	if (m->layer) {
		zwlr_layer_surface_v1_destroy(m->layer);
		m->layer = NULL;
	}
	if (m->argb_buf) {
		free(m->argb_buf);
		m->argb_buf = NULL;
	}
	if (m->surf) {
		wl_surface_destroy(m->surf);
		m->surf = NULL;
	}
	if (m->shm_fd >= 0) {
		close(m->shm_fd);
		m->shm_fd = -1;
	}

	free_menu_tree(tray->menu.items, tray->menu.n_items);
	tray->menu.items = NULL;
	tray->menu.n_items = 0;
	m->active          = 0;
	m->highlighted     = -1;
	m->is_inside_menu  = 0;
}

void
systray_render_popup(bar_t *b)
{
	systray_t *tray = b->tray;
	PopupMenu *m = &tray->menu;
	if (!m->active)
		return;
	draw_rect(m->argb_buf, m->w, m->h, 0, 0, m->w, m->h, TRAY_MENU_BG_COLOR);

	int line_h = F_SIZE + 4;
	for (size_t i = 0; i < m->n_items; i++) {
		MenuItem *it = m->items[i];
		int ty = i * line_h + 2;
		int is_hover = ((int)i == m->highlighted);
		if (is_hover) {
			draw_rect(m->argb_buf, m->w, m->h, 0, ty, m->w, line_h,
			 TRAY_MENU_HOVER_BG_COLOR);
		}
		if (it->label)
			draw_text(b, m->argb_buf, m->w, m->h, it->label, 8, ty, it->enabled ? TRAY_MENU_FG_COLOR : TRAY_MENU_DISABLED_FG_COLOR);
		if (it->has_submenu) {
			const char *arrow = "\u25B6";
			int aw = text_width_px(b, arrow);
			draw_text(b, m->argb_buf, m->w, m->h,
			 arrow,
			 m->w - 8 - aw, ty,
			 TRAY_MENU_FG_COLOR
			 );
		}
	}

	if (m->wl_buf)
		wl_buffer_destroy(m->wl_buf);
	m->wl_buf = create_shm_buffer(b, m->w, m->h, m->argb_buf, &m->shm_fd);
	wl_surface_attach(m->surf, m->wl_buf, 0, 0);
	wl_surface_damage(m->surf, 0, 0, m->w, m->h);
	wl_surface_commit(m->surf);
}

int
systray_handle_popup_click(bar_t *b){
	systray_t *tray = b->tray;
	PopupMenu *m = &tray->menu;
	if (!m->active || !m->is_inside_menu)
		return 0;

	int line_h = F_SIZE + 4;
	int idx = b->last_y / line_h;
	if (idx < 0 || (size_t)idx >= m->n_items) {
		popup_menu_hide(b->tray);
		return 1;
	}
	MenuItem *mi = m->items[idx];
	if (mi->has_submenu && mi->n_children > 0) {
		m->items   = mi->children;
		m->n_items = mi->n_children;
		m->parent  = mi;
		popup_menu_show(m->parent_item, b);
		wl_display_roundtrip(b->display);
		systray_render_popup(b);
		return 1;
	} else {
		tray_trigger_event(m->parent_item, mi->id);
		popup_menu_hide(b->tray);
		return 1;
	}
}

void
tray_get_menu_layout(tray_item_t *it, systray_t *tray)
{
	char path[1024];
	snprintf(path, sizeof(path), "%s/Menu", it->path);
	DBusMessage *msg = dbus_message_new_method_call(
		it->service, path,
		"com.canonical.dbusmenu", "GetLayout"
	);
	DBusMessageIter args, arr;
	int32_t pid=0, depth=-1;
	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &pid);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &depth);
	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &arr);
	dbus_message_iter_close_container(&args, &arr);

	DBusPendingCall *pc = NULL;
	dbus_connection_send_with_reply(tray->conn, msg, &pc, -1);
	dbus_pending_call_set_notify(pc, tray_menu_reply, it, NULL);
	dbus_connection_flush(tray->conn);
	dbus_message_unref(msg);
}

void
tray_left_click(tray_item_t *it)
{
	tray_trigger_event(it, 0);
}

static void
tray_fetch_icon(tray_item_t *it, systray_t *tray)
{
	static const char *sn_item_ifaces[] = {
		"org.kde.StatusNotifierItem",
		"org.freedesktop.StatusNotifierItem",
		"org.ayatana.StatusNotifierItem",
	};
	DBusError err;
	dbus_error_init(&err);

	for (size_t i = 0; i < 3; i++) {
		const char *iface = sn_item_ifaces[i];
		DBusMessage *msg, *reply;

		msg = dbus_message_new_method_call(
			it->service, it->path,
			"org.freedesktop.DBus.Properties", "Get"
		);
		const char *prop = "IconPixmap";
		dbus_message_append_args(msg,
						   DBUS_TYPE_STRING, &iface,
						   DBUS_TYPE_STRING, &prop,
						   DBUS_TYPE_INVALID
						   );
		reply = dbus_connection_send_with_reply_and_block(tray->conn, msg, -1, &err);
		dbus_message_unref(msg);

		if (reply) {
			// fprintf(stderr, "Got reply for IconPixmap scan for interface: %s\n", iface);

			DBusMessageIter var, arr;
			if (dbus_message_iter_init(reply, &var) &&
				dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_VARIANT)
			{
				dbus_message_iter_recurse(&var, &arr);
				if (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_ARRAY) {
					DBusMessageIter entry;
					dbus_message_iter_recurse(&arr, &entry);

					int best_width = 0;
					int best_height = 0;
					const uint8_t *best_pixels = NULL;
					int best_n_bytes = 0;

					while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRUCT) {
						DBusMessageIter struct_iter, bytes_iter;
						dbus_message_iter_recurse(&entry, &struct_iter);

						int width, height, n_bytes;
						const uint8_t *raw;

						dbus_message_iter_get_basic(&struct_iter, &width);
						dbus_message_iter_next(&struct_iter);

						dbus_message_iter_get_basic(&struct_iter, &height);
						dbus_message_iter_next(&struct_iter);

						if (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_ARRAY) {
							dbus_message_iter_recurse(&struct_iter, &bytes_iter);
							dbus_message_iter_get_fixed_array(&bytes_iter, &raw, &n_bytes);

							if (width > best_width && height > best_height && raw && n_bytes > 0) {
								best_width = width;
								best_height = height;
								best_pixels = raw;
								best_n_bytes = n_bytes;
							}
						}

						dbus_message_iter_next(&entry);
					}

					if (best_pixels) {
						it->width  = best_width;
						it->height = best_height;
						it->pixels = malloc(best_n_bytes);
						if (it->pixels) {
							memcpy(it->pixels, best_pixels, best_n_bytes);
						}
					}
				}
			}
			dbus_message_unref(reply);
			if (it->pixels) {
				it->iface = strdup(iface);
				goto got_pixels;
			}
		}

		msg = dbus_message_new_method_call(
			it->service, it->path,
			"org.freedesktop.DBus.Properties", "Get"
		);
		prop = "IconName";
		dbus_message_append_args(msg,
						   DBUS_TYPE_STRING, &iface,
						   DBUS_TYPE_STRING, &prop,
						   DBUS_TYPE_INVALID
						   );
		reply = dbus_connection_send_with_reply_and_block(tray->conn, msg, -1, &err);
		dbus_message_unref(msg);

		if (reply) {
			DBusMessageIter var, child;
			const char *icon_name = NULL;
			if (dbus_message_iter_init(reply, &var) &&
				dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_VARIANT)
			{
				dbus_message_iter_recurse(&var, &child);
				if (dbus_message_iter_get_arg_type(&child) == DBUS_TYPE_STRING)
					dbus_message_iter_get_basic(&child, &icon_name);
			}
			dbus_message_unref(reply);

			// fprintf(stderr, "Check icon name on fetch icons: %s\n", icon_name);
			if (icon_name) {
				char pathbuf[PATH_MAX];
				char *paths[] = {
					"/usr/share/icons/hicolor/32x32/apps",
					"/usr/share/icons/hicolor/48x48/apps",
					"/usr/share/icons/hicolor/64x64/apps",
					"/usr/share/icons/hicolor/scalable/apps",
					"/usr/share/pixmaps",
					"/usr/local/share/icons/hicolor",
					"~/.icons/hicolor"
				};
				size_t n_paths = sizeof(paths)/sizeof(paths[0]);

				for (size_t i = 0; i < n_paths; i++) {
					size_t dir_len  = strlen(paths[i]);
					size_t name_len = strlen(icon_name);
					// suffix len (4)
					if (dir_len + 1 + name_len + 4 + 1 > sizeof(pathbuf))
						continue;

					snprintf(pathbuf, sizeof pathbuf, "%s/%s.png", paths[i], icon_name);
					if (access(pathbuf, F_OK) == 0)
						break;
				}
				int x, y, n;
				unsigned char *data = stbi_load(pathbuf, &x, &y, &n, 4);
				if (data) {
					float scale = (float)tray->bar->height / (float)y;
					int new_width = (int)(x * scale);
					int new_height = tray->bar->height;

					unsigned char *resized_data = malloc(new_width * new_height * 4);

					if (resized_data) {
						stbir_resize_uint8_linear(
							data, x, y, 0,
							resized_data, new_width, new_height, 0,
							(stbir_pixel_layout)4);

						stbi_image_free(data);

						it->width = new_width;
						it->height = new_height;
						it->pixels = malloc(new_width * new_height * 4);

						if (it->pixels) {
							for (int i = 0; i < new_width * new_height; i++) {
								uint8_t r = resized_data[4 * i + 0];
								uint8_t g = resized_data[4 * i + 1];
								uint8_t b = resized_data[4 * i + 2];
								uint8_t a = resized_data[4 * i + 3];
								it->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
							}
						}

						free(resized_data);
					}
					else 
					stbi_image_free(data);
				}
			}
		}
		if (it->pixels) {
			it->iface = strdup(iface);
			goto got_pixels;
		}
	}

got_pixels:
	if (it->pixels) {
		it->buffer = create_shm_buffer(
			tray->bar,
			it->width, it->height,
			it->pixels,
			&it->shm_fd
		);
	}
	dbus_error_free(&err);

}

void
tray_handle_item_added(systray_t *tray, const char *service, const char *object_path)
{
	// fprintf(stderr, "In tray_handle_item_added: service=%s path=%s\n",
	// service, object_path);

	if (tray->n_items >= MAX_TRAY_ITEMS)
		return;

	tray_item_t *it = &tray->items[tray->n_items++];
	it->service = strdup(service);
	it->path    = strdup(object_path);
	it->iface   = NULL;
	it->pixels  = NULL;
	it->buffer  = NULL;
	it->shm_fd  = -1;
	it->tray = tray;

	tray_fetch_icon(it, tray);
}

void
tray_handle_item_removed(systray_t *tray, const char *service)
{
	for (size_t i = 0; i < tray->n_items; i++) {
		if (strcmp(tray->items[i].service, service) == 0) {
			tray_item_t *it = &tray->items[i];
			if (it->buffer) {
				wl_buffer_destroy(it->buffer);
				it->buffer = NULL;
			}
			free(it->pixels);
			it->pixels = NULL;

			free(it->service);
			it->service = NULL;
			free(it->path);
			it->path = NULL;

			if (it->shm_fd >= 0) {
				close(it->shm_fd);
				it->shm_fd = -1;
			}
			memmove(it, it+1, sizeof(*it) * (tray->n_items - i - 1));
			tray->n_items--;
			break;
		}
	}
}

int
systray_init(bar_t *b)
{
	systray_t *tray = calloc(1, sizeof(*tray));
	if (!tray)
		return -1;
	tray->bar  = b;
	b->tray    = tray;

	DBusError err;
	dbus_error_init(&err);
	tray->conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	// fprintf(stderr, "In systray_init tray connection: %p\n", tray->conn);
	if (!tray->conn) {
		fprintf(stderr, "Failed DBus connect: %s\n", err.message);
		dbus_error_free(&err);
		free(tray);
		return -1;
	}
	dbus_connection_set_exit_on_disconnect(tray->conn, FALSE);
	if (systray_watcher_start(tray->conn, tray) < 0) {
		fprintf(stderr, "systray: watcher startup failed, disabling tray\n");
	}
	dbus_error_free(&err);
	return 0;
}

void
systray_handle(bar_t *b)
{
	dbus_connection_read_write_dispatch(b->tray->conn, 0);
}

void
systray_cleanup(bar_t *b)
{
	systray_t *tray = b->tray;
	free_menu_tree(tray->menu.items, tray->menu.n_items);
	for (size_t i = 0; i < tray->n_items; i++) {
		tray_item_t *it = &tray->items[i];
		if (it->buffer) {
			wl_buffer_destroy(it->buffer);
			it->buffer = NULL;
		}
		if (it->pixels) {
			free(it->pixels);
			it->pixels = NULL;
		}
		if (it->iface) {
			free(it->iface);
			it->iface = NULL;
		}
		if (it->service) {
			free(it->service);
			it->service = NULL;
		}
		if (it->path) {
			free(it->path);
			it->path = NULL;
		}
		if (it->shm_fd >= 0)
			close(it->shm_fd);
	}
	systray_watcher_stop(tray->conn);
	dbus_connection_unref(tray->conn);
	dbus_shutdown();

	free(tray);
	b->tray = NULL;
}
