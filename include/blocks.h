/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   blocks.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/23 23:37:36 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/29 23:29:39 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_LABEL_LEN 256

typedef enum {
	BLK_FUNC,
	BLK_SCRIPT,
	BLK_TEMP,
	BLK_VOL,
	BLK_TAG,
	BLK_LAYOUT,
	BLK_TITLE,
	BLK_TRAY
}	blk_type_t;

typedef enum {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
}	align_t;

typedef struct block_t block_t;

struct  block_t {
	blk_type_t	type;
	const char	*cmd;
	char		*prefix;
	void		(*get_label)(char *buf, size_t bufsz);
	uint32_t	pfx_color;
	uint32_t	fg_color;
	uint32_t	bg_color;
	void		(*on_click)(block_t *block, int button);
	void		(*on_scroll)(block_t *block, int amt);
	align_t		align;
	uint32_t	interval_ms;
	int64_t		last_update_ms;
	int			x0, x1;
	char		label[MAX_LABEL_LEN];
};

// block functions
void	_clock(char *buf, size_t bufsz);
void	cpu_usage(char *buf, size_t bufsz);
void	mem_usage_simple(char *buf, size_t bufsz);
void	mem_usage_full(char *buf, size_t bufsz);

// click handlers
void	vol_click(block_t *block, int button);
void	clock_click(block_t *block, int button);
void	vol_scroll(block_t *block, int amt);
void	clock_scroll(block_t *block, int amt);
void	power_consumption_click(block_t *block, int button);

// util functions
int		run_cmd(const char *cmd, char *out, size_t len);
int		read_file(const char *cmd, char *out, size_t len);
int		init_blocks(block_t *blocks);
void	free_blocks(block_t *blocks);
void	update_block(block_t *block, uint64_t update_time);
void	handle_click(block_t *blocks, size_t n_blocks, int x, int y);
void	handle_scroll(block_t *blocks, size_t n, int x, int axis, int amt);

#endif
