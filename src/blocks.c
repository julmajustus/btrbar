/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   blocks.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/23 23:35:22 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 21:47:35 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#define _POSIX_C_SOURCE 200809L
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

int
run_cmd(const char *cmd, char *out, size_t len)
{
	FILE *f = popen(cmd, "r");
	if (!f)
		return -1;

	if (fgets(out, len, f) == NULL) {
		pclose(f);
		return -1;
	}
	size_t L = strlen(out);
	if (L && out[L-1] == '\n')
		out[L-1] = '\0';

	return pclose(f) == 0 ? 0 : -1;
}

int
read_file(const char *cmd, char *out, size_t len)
{
	FILE *f = fopen(cmd, "r");
	if (!f)
		return -1;

	if (fgets(out, len, f) == NULL) {
		fclose(f);
		return -1;
	}
	size_t L = strlen(out);
	if (L && out[L-1] == '\n')
		out[L-1] = '\0';

	return fclose(f) == 0 ? 0 : -1;
}

int
update_block(block_t *b, uint64_t update_time)
{
	char body[MAX_LABEL_LEN];
	
	if (b->type == BLK_FUNC) {
		b->get_label(body, sizeof(body));
	} 
	else if (b->type == BLK_TEMP) {
		if (read_file(b->cmd, body, sizeof(body)) != 0)
			memcpy(body, "err", 4);
		else
		snprintf(body, MAX_LABEL_LEN, "%d°C", atoi(body) / 1000);
	}
	else if (b->type == BLK_VOL) {
		if (run_cmd(b->cmd, body, sizeof(body)) != 0)
			memcpy(body, "err", 4);
		else
		snprintf(body, MAX_LABEL_LEN, "%.0f%%", atof(body) * 100);
	}
	// cmd script blocks
	else {
		if (!b->cmd)
			memcpy(body, "", 1);
		else if (run_cmd(b->cmd, body, sizeof(body)) != 0)
			memcpy(body, "err", 4);
	}

	b->last_update_ms = update_time;

	if (strcmp(b->label, body) != 0) {
		size_t len = strnlen(body, MAX_LABEL_LEN-1);
		memcpy(b->label, body, len);
		b->label[len] = '\0';
		b->needs_redraw = 1;
		return 1;
	}
	return 0;
}

void
handle_click(block_t *blocks, size_t n, int x, int button)
{
	// fprintf(stderr, "Check X: %d\n", x);
	for (size_t i = 0; i < n; i++) {
		if (x >= blocks[i].x0 && x < blocks[i].x1) {
			if (blocks[i].on_click)
				blocks[i].on_click(&blocks[i], button);
			blocks[i].needs_redraw = 1;
			break;
		}
	}
}


void
handle_scroll(block_t *blocks, size_t n, int x, int axis, int amt)
{
	(void)axis;
	for (size_t i = 0; i < n; i++) {
		if (x >= blocks[i].x0 && x < blocks[i].x1) {
			if (blocks[i].on_scroll)
				blocks[i].on_scroll(&blocks[i], amt);
			blocks[i].needs_redraw = 1;
			break;
		}
	}
}

int
init_blocks(block_t *blocks)
{
	for (int i = 0; i < N_BLOCKS ; i++) {
		blocks[i].type = blocks_cfg[i].type;
		blocks[i].cmd = blocks_cfg[i].cmd;
		blocks[i].prefix = blocks_cfg[i].prefix;
		blocks[i].get_label = blocks_cfg[i].get_label;
		blocks[i].pfx_color = blocks_cfg[i].pfx_color;
		blocks[i].fg_color = blocks_cfg[i].fg_color;
		blocks[i].bg_color = blocks_cfg[i].bg_color;
		blocks[i].on_click = blocks_cfg[i].on_click;
		blocks[i].on_scroll = blocks_cfg[i].on_scroll;
		blocks[i].align = blocks_cfg[i].align;
		blocks[i].interval_ms = blocks_cfg[i].interval_ms * 1000;
		blocks[i].last_update_ms = 0;
		blocks[i].x0 = blocks[i].x1 = 0;
		update_block(&blocks[i], 0);
	}
	return 0;
}

void
free_blocks(block_t *blocks)
{
	free(blocks);
	blocks = NULL;
}
