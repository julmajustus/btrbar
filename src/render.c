/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:23:38 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/02 00:28:20 by julmajustus      ###   ########.fr       */
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
free_font(bar_t *b)
{
	free(b->ttf_buffer);
}

int
init_font(bar_t *b)
{
	FILE *fp = fopen(FONT, "rb");
	if (!fp) { perror("font file"); exit(1); }
	fseek(fp, 0, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	b->ttf_buffer = malloc(sz);
	if (!b->ttf_buffer) {
		fclose(fp);
		return -1;
	}
	if (fread(b->ttf_buffer, 1, sz, fp) == 0) {
		if (ferror(fp))
			fclose(fp);
		return -1;
	}
	fclose(fp);

	if (!stbtt_InitFont(&b->font, b->ttf_buffer, stbtt_GetFontOffsetForIndex(b->ttf_buffer,0))) {
		free_font(b);
		return -1;
	}
	return 0;
}


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
blend_glyph(uint32_t *argb_buf, int buf_w, int buf_h, int x, int y, int w, int h, unsigned char *bitmap, uint32_t color)
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
draw_text(bar_t *b, uint32_t *buf, int buf_w, int buf_h, const char *text, int x, int y, uint32_t color)
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
		unsigned char *bitmap = malloc(w * h);
		stbtt_MakeGlyphBitmap(&b->font, bitmap, w, h, w, scale, scale, glyph);

		blend_glyph(buf, buf_w, buf_h, pen_x + x0, y + baseline + y0, w, h, bitmap, color);
		free(bitmap);
		pen_x += (int)(adv * scale);
	}
}

int
text_width_px(bar_t *b, const char *text)
{
	if (!text)
		return 0;
	float scale = stbtt_ScaleForPixelHeight(&b->font, F_SIZE);
	int width = 0;
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
tags_width_px(bar_t *b)
{
	int tx = 0;;

	for (size_t i = 0; i < N_TAGS; i++) {
		tag_t *t = &b->tags[i];
		int txt_w = text_width_px(b, t->icon);
		t->x0 = tx;
		t->x1 = tx + txt_w + TAG_ICON_PADDING;

		tx += txt_w + TAG_ICON_PADDING;
	}
	return tx;
}

static int
draw_tag(bar_t *b, int tx, int y)
{
	for (size_t i = 0; i < N_TAGS; i++) {
		tag_t *t = &b->tags[i];
		int txt_w = text_width_px(b, t->icon);
		t->x0 = tx;
		t->x1 = tx + txt_w + TAG_ICON_PADDING;

		uint32_t fg = t->active   ? TAG_FG_ACTIVE
			: t->urgent? TAG_FG_URGENT
			: t->occupied? TAG_FG_OCCUPIED
			: TAG_FG_EMPTY;
		draw_rect(b->argb_buf, b->width, b->height, tx, 0, t->x1, b->height, TAG_BG_COLOR);

		int x_txt = tx;
		draw_text(b, b->argb_buf, b->width, b->height, t->icon, x_txt, y, fg);

		tx += txt_w + TAG_ICON_PADDING;
	}
	return tx;
}

static void
draw_systray(bar_t *b, int x0) {
	systray_t *tray = b->tray;
	uint32_t *bar_px = b->argb_buf;
	int x = x0;

	for (size_t i = 0; i < tray->n_items; i++) {
		tray_item_t *it = &tray->items[i];
		// fprintf(stderr, "Check service: %s\n path: %s\n width: %d height: %d\n pixels: %d\n", it->service, it->path, it->width, it->height, it->pixels);
		if (!it->pixels)
			continue;

		for (uint32_t yy = 0; yy < it->height && yy < b->height; yy++) {
			uint32_t *dst = bar_px + yy * b->width + x;
			uint32_t *src = it->pixels + yy * it->width;
			for (uint32_t xx = 0; xx < it->width; xx++) {
				uint32_t sp = src[xx];
				uint8_t  sa = sp >> 24;
				if (sa == 0)
					continue;
				uint32_t dp = dst[xx];
				float    fa = sa/255.0f;
				uint8_t dr = dp >> 16, dg = dp >> 8, db = dp;
				uint8_t sr = sp >> 16, sg = sp >> 8, sb = sp;
				uint8_t rr = dr + (int)((sr - dr)*fa);
				uint8_t gg = dg + (int)((sg - dg)*fa);
				uint8_t bb = db + (int)((sb - db)*fa);
				dst[xx] = (0xFFu << 24) | (rr << 16) | (gg << 8) |bb;
			}
		}
		it->x0 = x;
		x += it->width + TRAY_ICON_PADDING;
		it->x1 = x;
	}
}

void
render_bar(bar_t *b)
{
	b->argb_buf = b->argb_bufs[b->cur_buf];

	draw_rect(b->argb_buf, b->width, b->height, 0, 0, b->width, b->height, BG_COLOR);

	int char_h = b->height;
	int y = (b->height - char_h)/2;
	int left_x = EDGE_PADDING;
	int right_x = b->width - EDGE_PADDING;
	int tx = 0;

	static int widths[N_BLOCKS];
	int total_center_w = 0;

	for (size_t i = 0; i < N_BLOCKS; i++) {
		if (TAGS && b->blocks[i].type == BLK_TAG) {
			widths[i] = tags_width_px(b);
		}
		else if (b->blocks[i].type == BLK_TRAY) {
			widths[i] = (b->tray->n_items * b->height) + (b->tray->n_items * TRAY_ICON_PADDING);
		}
		else {
			const char *prefix = b->blocks[i].prefix ?: "";
			int prefix_w = text_width_px(b, prefix);
			int label_w  = text_width_px(b, b->blocks[i].label);
			widths[i]    = prefix_w + label_w + BLOCK_PADDING * 2;
		}

		if (b->blocks[i].align == ALIGN_CENTER) {
			total_center_w += widths[i];
		}
	}

	int center_x = (b->width - total_center_w) / 2;

	for (int i = 0; i < N_BLOCKS; i++) {
		int w  = widths[i];
		int bx;

		switch (b->blocks[i].align) {
			case ALIGN_LEFT:
				bx = left_x;
				left_x += w;
				break;
			case ALIGN_RIGHT:
				bx = right_x - w;
				right_x -= w;
				break;
			case ALIGN_CENTER:
			default:
				bx = center_x;
				center_x += w;
				break;
		}

		b->blocks[i].x0 = bx;
		b->blocks[i].x1 = bx + w;

		// fprintf(stderr, "Block check: %d: %s%s dimensions: x0: %d x1: %d\n", i, b->blocks[i].prefix, b->blocks[i].label, b->blocks[i].x0, b->blocks[i].x1);
		if (TAGS && b->blocks[i].type == BLK_TAG) {
			tx = draw_tag(b, bx, y) + bx;
			continue;
		}
		else if (b->blocks[i].type == BLK_TRAY) {
			draw_systray(b, bx + BLOCK_PADDING);
			tx = bx;
			continue;
		}
		draw_rect(b->argb_buf, b->width, b->height, bx, 0, w, b->height, b->blocks[i].bg_color);

		const char *prefix = b->blocks[i].prefix ?: "";
		int prefix_w = text_width_px(b, prefix);
		tx = bx + BLOCK_PADDING;
		if (*prefix) {
			draw_text(b, b->argb_buf, b->width, b->height, prefix, tx, y, b->blocks[i].pfx_color);
			tx += prefix_w;
		}

		draw_text(b, b->argb_buf, b->width, b->height, b->blocks[i].label, tx, y, b->blocks[i].fg_color);
	}

	struct wl_buffer *buf = b->buffers[b->cur_buf];
	wl_surface_attach(b->surface, buf, 0, 0);
	wl_surface_damage(b->surface, 0, 0, b->width, b->height);
	wl_surface_commit(b->surface);
	b->cur_buf ^= 1;
}
