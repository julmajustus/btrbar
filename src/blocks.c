/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   blocks.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/23 23:35:22 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 02:05:38 by julmajustus      ###   ########.fr       */
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
	char body[MAX_LABEL_LEN] = {0};

	switch (b->type) {
		case BLK_FUNC:
			if (b->get_label)
				b->get_label(body, sizeof(body));
			break;
		case BLK_TEMP:
			if (read_file(b->cmd, body, sizeof(body)) != 0)
				memcpy(body, "err", 4);
			else 
				snprintf(body, MAX_LABEL_LEN, "%d°C", atoi(body) / 1000);
			break;
		case BLK_VOL:
			if (run_cmd(b->cmd, body, sizeof(body)) != 0)
				memcpy(body, "err", 4);
			else
				snprintf(body, MAX_LABEL_LEN, "%.0f%%", atof(body) * 100);
			break;
		default:
			if (!b->cmd)
				body[0] = '\0';
			else if (run_cmd(b->cmd, body, sizeof(body)) != 0)
				memcpy(body, "err", 4);
			break;
	}

	b->last_update_ms = update_time;

	if (strcmp(b->label, body) != 0) {
		uint32_t len = strnlen(body, MAX_LABEL_LEN - 1);
		memcpy(b->label, body, len);
		b->label[len] = '\0';
		return 1;
	}
	return 0;
}

void
handle_click(block_inst_t *blocks, uint32_t x, int button)
{
	for (uint8_t i = 0; i < N_BLOCKS; i++) {
		block_inst_t *bi = &blocks[i];
		if (x >= bi->x0 && x < bi->x1) {
			if (bi->block->on_click) {
				bi->block->on_click(bi->block, button);
				bi->block->version++;
			}
			break;
		}
	}
}

void
handle_scroll(block_inst_t *blocks, uint32_t x, int axis, int amt)
{
	(void)axis;
	for (uint8_t i = 0; i < N_BLOCKS; i++) {
		block_inst_t *bi = &blocks[i];
		if (x >= bi->x0 && x < bi->x1) {
			if (bi->block->on_scroll) {
				bi->block->on_scroll(bi->block, amt);
				bi->block->version++;
			}
			break;
		}
	}
}
