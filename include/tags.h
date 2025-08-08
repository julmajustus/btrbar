/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tags.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:22:40 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 02:03:29 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TAGS_H
#define TAGS_H

#include "blocks.h"
#include <stddef.h>


typedef struct {
	char		name[MAX_LABEL_LEN];
	const char	*icon; 
	uint8_t		occupied;
	uint8_t		active;
	uint8_t		urgent;
	uint32_t	x0, x1;
} tag_t;

int		init_tags(tag_t *tags);
void	free_tags(tag_t *tags);

#endif
