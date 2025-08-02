/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   systray_watcher.h                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/26 02:07:20 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/29 23:49:50 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef SYSTRAY_WATCHER_H
#define SYSTRAY_WATCHER_H

#include <dbus-1.0/dbus/dbus.h>
#include "systray.h"

#define SNW_NAME  "org.kde.StatusNotifierWatcher"
#define SNW_PATH  "/StatusNotifierWatcher"
#define SNW_IFACE "org.kde.StatusNotifierWatcher"

int		systray_watcher_start(DBusConnection *conn, systray_t *tray);
void	systray_watcher_stop(DBusConnection *conn);

#endif
