/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:14:52 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/29 23:25:21 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */
#include "bar.h"
#include <stdlib.h>

int
main(void)
{
	bar_t *b = calloc(1, sizeof(*b));

	if (bar_init(b) == -1)
		return 1;

	run(b);
	cleanup(b);
	free(b);

	return 0;
}
