/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   input.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:57:57 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/03 23:03:32 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "input.h"
#include "bar.h"
#include "blocks.h"
#include "config.h"
#include "ipc.h"
#include "render.h"
#include "tools.h"
#include "systray.h"

#include <stdio.h>
#include <stdlib.h>

static void
pointer_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *surf, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)p, (void)serial, (void)sx, (void)sy;

	bar_t *b = data;
	if (TRAY && surf == b->tray->menu.surf) {
		b->tray->menu.is_inside_menu = 1;
		return;
	}
}

static void
pointer_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *surf)
{
	(void)p, (void)serial;

	bar_t *b = data;
	if (TRAY && surf == b->tray->menu.surf)
		popup_menu_hide(b->tray);
}

static void
pointer_motion(void *data, struct wl_pointer *p, uint32_t t, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)p, (void)t, (void)sy;
	bar_t *b = data;

	b->last_x = wl_fixed_to_int(sx);
	b->last_y = wl_fixed_to_int(sy);

	if (TRAY) {
		PopupMenu *m = &b->tray->menu;
		if (!m->active)
			return;
		int line_h = F_SIZE + 4;
		int idx = b->last_y / line_h;
		if (idx < 0)
			idx = 0;

		if (idx >= (int)m->n_items)
			idx = m->n_items - 1;

		if (idx != m->highlighted && b->tray->menu.is_inside_menu != 0) {
			m->highlighted = idx;
			systray_render_popup(b);
		}
	}
}

static void
pointer_button(void *data, struct wl_pointer *p, uint32_t time, uint32_t serial, uint32_t button, uint32_t state)
{
	if (state != WL_POINTER_BUTTON_STATE_PRESSED)
		return;
	(void)p, (void)time, (void)serial;
	bar_t *b = data;
	if (TRAY) {
		if (b->tray->menu.active) {
			if (systray_handle_popup_click(b)) {
				return;
			}
		}

		for (size_t i = 0; i < b->tray->n_items; i++) {
			tray_item_t *it = &b->tray->items[i];
			if (b->last_x >= it->x0 && b->last_x < it->x1 && !b->tray->menu.active) {
				if (button == 272) // left click
					tray_left_click(it);
				else if (button == 273) {  // right click
					tray_get_menu_layout(it, b->tray);
				}
				return;
			}
		}
	}

	if (TAGS) {
		for (size_t i = 0; i < N_TAGS; i++) {
			if (b->last_x >= b->tags[i].x0 && b->last_x < b->tags[i].x1) {
				uint32_t mask = 1u << i;
				zdwl_ipc_output_v2_set_tags(b->ipc_out, mask, 0);
				for (int i = 0; i < N_BLOCKS; i++) {
					if (b->blocks[i].type == BLK_TAG)
						b->blocks[i].needs_redraw = 1;
					b->needs_redraw = 1;
					wl_display_roundtrip(b->display);
				}
				return;
			}
		}
	}
	handle_click(b->blocks, N_BLOCKS, b->last_x, button);
	b->needs_redraw = 1;
	wl_display_roundtrip(b->display);
}

static void
pointer_axis(void *data, struct wl_pointer *p, uint32_t t, uint32_t axis, wl_fixed_t value)
{
	(void)p, (void)t;
	bar_t *b = data;
	int amt = wl_fixed_to_int(value);
	handle_scroll(b->blocks, N_BLOCKS, b->last_x, axis, amt);
	b->needs_redraw = 1;
	wl_display_roundtrip(b->display);
}

const struct wl_pointer_listener pointer_listener = {
	.enter  = pointer_enter,
	.leave  = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis   = pointer_axis,
	.frame  = noop,
	.axis_source  = noop,
	.axis_stop  = noop,
	.axis_discrete  = noop,
};
