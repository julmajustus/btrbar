/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tags.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:22:40 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/25 03:44:24 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TAGS_H
#define TAGS_H

#include "blocks.h"
#include <stddef.h>


typedef struct {
	char		name[MAX_LABEL_LEN];
	const char	*icon; 
	int			occupied;
	int			active;
	int			urgent;
	int			x0, x1;
} tag_t;

int		init_tags(tag_t *tags);
void	free_tags(tag_t *tags);

#endif
