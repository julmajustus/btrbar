/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:23:38 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/11 17:45:59 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "blocks.h"
#include <stdint.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "render.h"
#include "bar.h"
#include "config.h"
#include "systray.h"

#include <stdio.h>
#include <stdlib.h>


void
free_font(bar_manager_t *m)
{
	free(m->ttf_buffer);
}

int
init_font(bar_manager_t *m)
{
	FILE *fp = fopen(FONT, "rb");
	if (!fp) {
		perror("font file");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	m->ttf_buffer = malloc(sz);
	if (!m->ttf_buffer) {
		fclose(fp);
		return -1;
	}
	if (fread(m->ttf_buffer, 1, sz, fp) == 0) {
		if (ferror(fp))
			fclose(fp);
		return -1;
	}
	fclose(fp);

	if (!stbtt_InitFont(&m->font, m->ttf_buffer, stbtt_GetFontOffsetForIndex(m->ttf_buffer,0))) {
		free_font(m);
		return -1;
	}
	return 0;
}

static void
frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	(void)time;
	bar_t *b = data;
	if (cb)
		wl_callback_destroy(cb);
	b->frame_cb = NULL;
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_done
};

void
draw_rect(uint32_t *buf, int buf_w, int buf_h, int x0, int y0, int w, int h, uint32_t color)
{
	// fprintf(stderr, "draw_rect(%d,%d,%d,%d,0x%08x)\n", x0, y0, w, h, color);
	if (x0 < 0) {
		w += x0;
		x0 = 0;
	}
	if (y0 < 0) {
		h += y0;
		y0 = 0;
	}
	if (x0 + w > buf_w)
		w = buf_w  - x0;
	if (y0 + h > buf_h)
		h = buf_h  - y0;
	if (w <= 0 || h <= 0)
		return;

	uint32_t r_buf[w];
	static int r_buf_w = 0;
	if (r_buf_w < w) {
		r_buf_w = w;
	}
	for (int i = 0; i < w; i++)
		r_buf[i] = color;

	for (int row = 0; row < h; row++)
		memcpy(buf + (y0 + row) * buf_w + x0, r_buf, w * sizeof *r_buf);
}

static void
blend_glyph(uint32_t *argb_buf, int buf_w, int buf_h,
			int x, int y, int w, int h, unsigned char *bitmap, uint32_t color)
{
	uint8_t src_a = (color >> 24) & 0xFF;
	uint8_t src_r = (color >> 16) & 0xFF;
	uint8_t src_g = (color >> 8 ) & 0xFF;
	uint8_t src_b = (color >> 0 ) & 0xFF;

	for (int row = 0; row < h; ++row) {
		int by = y + row;
		if (by < 0 || by >= buf_h)
			continue;
		for (int col = 0; col < w; ++col) {
			int bx = x + col;
			if (bx < 0 || bx >= buf_w)
				continue;
			int idx = by * buf_w + bx;

			uint8_t glyph_alpha = bitmap[row * w + col];
			float alpha = (glyph_alpha / 255.0f) * (src_a / 255.0f);

			// in memory { B, G, R, A }
			uint8_t *dst = (uint8_t *)&argb_buf[idx];

			dst[0] = (uint8_t)(src_b * alpha + dst[0] * (1.0f - alpha));
			dst[1] = (uint8_t)(src_g * alpha + dst[1] * (1.0f - alpha));
			dst[2] = (uint8_t)(src_r * alpha + dst[2] * (1.0f - alpha));
			dst[3] = (uint8_t)(src_a * alpha + dst[3] * (1.0f - alpha));
		}
	}
}

static uint32_t
next_utf8(const char **s)
{
	const unsigned char *us = (const unsigned char *)(*s);
	uint32_t cp;
	if (us[0] < 0x80) {
		cp = us[0];
		*s += 1;
	}
	else if ((us[0] & 0xE0) == 0xC0) {
		cp = ((us[0] & 0x1F) << 6) | (us[1] & 0x3F);
		*s += 2;
	}
	else if ((us[0] & 0xF0) == 0xE0) {
		cp = ((us[0] & 0x0F) << 12) | ((us[1] & 0x3F) << 6) | (us[2] & 0x3F);
		*s += 3;
	}
	else if ((us[0] & 0xF8) == 0xF0) {
		cp = ((us[0] & 0x07) << 18) | ((us[1] & 0x3F) << 12) | ((us[2] & 0x3F) << 6) | (us[3] & 0x3F);
		*s += 4;
	}
	else {
		cp = 0;
		*s += 1; }
	return cp;
}

void
draw_text(bar_t *b, uint32_t *buf, int buf_w, int buf_h,
		  const char *text, int x, int y, uint32_t color)
{
	// fprintf(stderr, "draw_text('%s',%d,%d,0x%08x)\n", text, x, y, color);
	float scale = stbtt_ScaleForPixelHeight(&b->font, F_SIZE);
	int ascent;
	stbtt_GetFontVMetrics(&b->font, &ascent, 0, 0);
	int baseline = (int)(ascent * scale);

	int pen_x = x;
	const char *s = text;
	while (*s) {
		uint32_t cp = next_utf8(&s);
		int glyph = stbtt_FindGlyphIndex(&b->font, cp);
		int adv, lsb;
		stbtt_GetGlyphHMetrics(&b->font, glyph, &adv, &lsb);

		int x0, y0, x1, y1;
		stbtt_GetGlyphBitmapBox(&b->font, glyph, scale, scale, &x0, &y0, &x1, &y1);
		int w = x1 - x0, h = y1 - y0;
		unsigned char bitmap[w * h];
		stbtt_MakeGlyphBitmap(&b->font, bitmap, w, h, w, scale, scale, glyph);

		blend_glyph(buf, buf_w, buf_h, pen_x + x0, y + baseline + y0, w, h, bitmap, color);
		pen_x += (int)(adv * scale);
	}
}

int
text_width_px(bar_t *b, const char *text)
{
	if (!text)
		return 0;
	float scale = stbtt_ScaleForPixelHeight(&b->font, F_SIZE);
	uint32_t width = 0;
	const char *s = text;
	while (*s) {
		uint32_t cp = next_utf8(&s);
		int glyph = stbtt_FindGlyphIndex(&b->font, cp);
		int adv, lsb;
		stbtt_GetGlyphHMetrics(&b->font, glyph, &adv, &lsb);
		width += (int)(adv * scale);
	}
	return width;
}

static int
block_width_px(bar_t *b, const block_inst_t *bi)
{
	const block_t *block = bi->block;

	if (TAGS && block->type == BLK_TAG) {
		int tx = 0;
		for (uint8_t i = 0; i < ASIZE(tag_icons); i++) {
			int txt_w = text_width_px(b, b->tags[i].icon);
			tx += txt_w + TAG_ICON_PADDING;
		}
		return tx;
	}
	if (TRAY && block->type == BLK_TRAY) {
		int icons = b->tray ? (int)b->tray->n_items : 0;
		return icons ? icons * b->height + icons * TRAY_ICON_PADDING : 0;
	}

	int prefix_w = text_width_px(b, block->prefix ? block->prefix : "");
	int label_w  = text_width_px(b, block->label);
	return prefix_w + label_w + 4 + BLOCK_PADDING * 2;
}

static void draw_tags(bar_t *b, int x, int y)
{
	int tx = x;
	for (uint8_t i = 0; i < ASIZE(tag_icons); i++) {
		tag_t *t = &b->tags[i];
		uint32_t txt_w = text_width_px(b, t->icon);
		t->x0 = tx;
		t->x1 = tx + txt_w + TAG_ICON_PADDING;

		uint32_t fg = t->active   ? TAG_FG_ACTIVE
			: t->urgent  ? TAG_FG_URGENT
			: t->occupied? TAG_FG_OCCUPIED
			: TAG_FG_EMPTY;
		draw_rect(b->argb_buf, b->width, b->height,
			tx, 0, t->x1 - tx, b->height, TAG_BG_COLOR);
		draw_text(b, b->argb_buf, b->width, b->height, t->icon, tx, y, fg);
		tx += txt_w + TAG_ICON_PADDING;
	}
}

static void
draw_tray(bar_t *b, block_inst_t *bi, int x0)
{
	if (!TRAY || !b->tray)
		return;
	bi->local.tray.n = 0;

	int x = x0;
	for (uint8_t i = 0; i < b->tray->n_items; i++) {
		tray_item_t *it = &b->tray->items[i];
		if (!it->pixels)
			continue;
		for (uint32_t yy = 0; yy < it->height && yy < b->height; yy++) {
			uint32_t *dst = b->argb_buf + yy * b->width + x;
			uint32_t *src = it->pixels + yy * it->width;
			for (uint32_t xx = 0; xx < it->width; xx++) {
				uint32_t sp = src[xx];
				uint8_t  sa = sp >> 24;
				if (!sa)
					continue;
				uint32_t dp = dst[xx];
				float fa = sa / 255.0f;
				uint8_t dr = dp >> 16, dg = dp >> 8, db = dp;
				uint8_t sr = sp >> 16, sg = sp >> 8, sb = sp;
				dst[xx] = (0xFFu << 24)
					| ((uint8_t)(dr + (sr - dr)*fa) << 16)
					| ((uint8_t)(dg + (sg - dg)*fa) <<  8)
					|  (uint8_t)(db + (sb - db)*fa);
			}
		}
		if (bi->local.tray.n < MAX_TRAY_ITEMS) {
			tray_span_t *span = &bi->local.tray.spans[bi->local.tray.n++];
			span->x0 = x;
			x += it->width + TRAY_ICON_PADDING;
			span->x1 = x;
			span->item_index = i;
		}
		else 
		x += it->width + TRAY_ICON_PADDING;
	}
}

void
render_bar(bar_t *b)
{
	if (!b->needs_redraw)
		return;

	b->argb_buf = b->argb_bufs[b->cur_buf];

	draw_rect(b->argb_buf, b->width, b->height, 0, 0, b->width, b->height, BG_COLOR);

	int y = 0;

	for (uint8_t i = 0; i < ASIZE(blocks_cfg); ++i) {
		block_inst_t *bi = &b->block_inst[i];
		if (bi->seen_version != bi->block->version) {
			bi->seen_version = bi->block->version;
		}
	}

	for (int align = ALIGN_LEFT; align <= ALIGN_RIGHT; align++) {
		int right = (align == ALIGN_RIGHT);
		int x = right ? (b->width - EDGE_PADDING)
			: (align == ALIGN_LEFT ? EDGE_PADDING : b->width / 2);
		int start = right ? ASIZE(blocks_cfg) - 1 : 0;
		int end   = right ? -1 : ASIZE(blocks_cfg);
		int step  = right ? -1 : 1;

		for (int i = start; i != end; i += step) {
			block_inst_t *bi = &b->block_inst[i];
			if (bi->block->align != (align_t)align)
				continue;

			int w = block_width_px(b, bi);
			int new_x0 = right ? x - w : x;
			int new_x1 = right ? x : x + w;

			bi->old_x0 = bi->x0; bi->old_x1 = bi->x1;
			bi->x0 = new_x0; bi->x1 = new_x1;
			bi->current_width = w;

			draw_rect(b->argb_buf, b->width, b->height,
			 bi->x0, 0, w, b->height, bi->block->bg_color);

			if (TAGS && bi->block->type == BLK_TAG)
				draw_tags(b, bi->x0, y);
			else if (TRAY && bi->block->type == BLK_TRAY)
				draw_tray(b, bi, bi->x0 + BLOCK_PADDING);
			else {
				uint32_t tx = bi->x0 + BLOCK_PADDING;
				if (bi->block->prefix) {
					draw_text(b, b->argb_buf, b->width, b->height,
			   bi->block->prefix, tx + 2, y, bi->block->pfx_color);
					tx += text_width_px(b, bi->block->prefix);
				}
				draw_text(b, b->argb_buf, b->width, b->height,
			  bi->block->label, tx + 2, y, bi->block->fg_color);
			}

			x = right ? new_x0 : new_x1;
		}
	}

	wl_surface_attach(b->surface, b->buffers[b->cur_buf], 0, 0);
	wl_surface_damage(b->surface, 0, 0, INT32_MAX, INT32_MAX);
	if (b->frame_cb)
		wl_callback_destroy(b->frame_cb);
	b->frame_cb = wl_surface_frame(b->surface);
	wl_callback_add_listener(b->frame_cb, &frame_listener, b);
	wl_surface_commit(b->surface);

	b->cur_buf ^= 1;
	b->needs_redraw ^= 1;
}
