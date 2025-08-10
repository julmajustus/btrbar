/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tags.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:23:49 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/29 23:22:25 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config.h"
#include "util.h"
#include "tags.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


int
init_tags(tag_t *tags)
{

	for (long i = 0; i < ASIZE(tag_icons); i++) {
		snprintf(tags[i].name, MAX_LABEL_LEN, "%zu", i + 1);

		if (i < ASIZE(tag_icons) && tag_icons[i])
			tags[i].icon = tag_icons[i];
		else
			tags[i].icon = tags[i].name;

		tags[i].occupied = 0;
		tags[i].active   = 0;
		tags[i].urgent   = 0;
		tags[i].x0 = tags[i].x1 = 0;
	}
	return 0;
}

void
free_tags(tag_t *tags)
{
	free(tags);
	tags = NULL;
}
