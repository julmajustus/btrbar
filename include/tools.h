/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tools.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:48:14 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 21:02:57 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TOOLS_H
#define TOOLS_H

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include "bar.h"

void	noop();
int64_t	current_time_ms(void);

#endif
