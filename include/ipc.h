/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ipc.h                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:35:05 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/24 20:18:20 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IPC_H
#define IPC_H

#include "dwl-ipc-unstable-v2.h"

extern const struct wl_output_listener output_listener;
extern const struct zdwl_ipc_manager_v2_listener ipc_manager_listener;
extern const struct zdwl_ipc_output_v2_listener ipc_output_listener;

#endif
