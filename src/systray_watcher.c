/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   systray_watcher.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/26 02:08:20 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/02 02:22:56 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dbus/dbus-shared.h"
#define _POSIX_C_SOURCE 200809L
#include "systray_watcher.h"
#include <wayland-util.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *introspect_xml =
	"<node>"
	"<interface name=\"org.kde.DBus.Introspectable\">"
	"<method name=\"Introspect\">"
	"<arg name=\"xml_data\" type=\"s\" direction=\"out\"/>"
	"</method>"
	"</interface>"
	"<interface name=\"" SNW_IFACE "\">"
	"<method name=\"RegisterStatusNotifierItem\">"
	"<arg name=\"service\" type=\"s\" direction=\"in\"/>"
	"</method>"
	"<signal name=\"StatusNotifierItemRegistered\">"
	"<arg name=\"service\" type=\"s\"/>"
	"</signal>"
	"<signal name=\"StatusNotifierItemUnregistered\">"
	"<arg name=\"service\" type=\"s\"/>"
	"</signal>"
	"<property name=\"RegisteredStatusNotifierItems\" type=\"as\" access=\"read\"/>"
	"</interface>"
	"</node>";

static void
emit_sni_signal(DBusConnection *conn,
				const char    *signal_name,
				const char    *service)
{
	DBusMessage *m = dbus_message_new_signal(SNW_PATH, SNW_IFACE, signal_name);
	if (!m)
		return;
	dbus_message_append_args(m,
						  DBUS_TYPE_STRING, &service,
						  DBUS_TYPE_INVALID);
	dbus_connection_send(conn, m, NULL);
	dbus_message_unref(m);
}

static DBusHandlerResult
handle_introspect(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	(void)user_data;
	DBusMessage *reply = dbus_message_new_method_return(msg);
	if (!reply) return DBUS_HANDLER_RESULT_NEED_MEMORY;
	dbus_message_append_args(reply,
						  DBUS_TYPE_STRING, &introspect_xml,
						  DBUS_TYPE_INVALID);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_get_prop(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const char *iface, *prop;
	DBusError err;
	DBusMessage *reply;
	DBusMessageIter root, variant, array;
	systray_t *tray = user_data;

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err,
							DBUS_TYPE_STRING, &iface,
							DBUS_TYPE_STRING, &prop,
							DBUS_TYPE_INVALID))
	{
		dbus_error_free(&err);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (strcmp(iface, SNW_IFACE) != 0 ||
		strcmp(prop, "RegisteredStatusNotifierItems") != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_iter_init_append(reply, &root);

	dbus_message_iter_open_container(&root,
								  DBUS_TYPE_VARIANT, "as", &variant);

	dbus_message_iter_open_container(&variant,
								  DBUS_TYPE_ARRAY, "s", &array);

	for (size_t i = 0; i < tray->n_items; i++) {
		const char *svc = tray->items[i].service;
		dbus_message_iter_append_basic(&array,
								 DBUS_TYPE_STRING, &svc);
	}

	dbus_message_iter_close_container(&variant, &array);
	dbus_message_iter_close_container(&root,    &variant);

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_register_item(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const char *object_path;
	DBusMessage *reply;
	systray_t *tray = user_data;

	if (!dbus_message_get_args(msg, NULL,
							DBUS_TYPE_STRING, &object_path,
							DBUS_TYPE_INVALID)) {
		reply = dbus_message_new_error(msg,
								 DBUS_ERROR_INVALID_ARGS,
								 "Expected (s)");
		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	const char *service = dbus_message_get_sender(msg);

	tray_handle_item_added(tray, service, object_path);

	emit_sni_signal(conn, "StatusNotifierItemRegistered", object_path);

	reply = dbus_message_new_method_return(msg);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
name_owner_changed_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	systray_t *tray = user_data;

	if (dbus_message_is_signal(msg, "org.freedesktop.DBus", "NameOwnerChanged"))
	{
		const char *name, *old, *new_;
		if (dbus_message_get_args(msg, NULL,
							DBUS_TYPE_STRING, &name,
							DBUS_TYPE_STRING, &old,
							DBUS_TYPE_STRING, &new_,
							DBUS_TYPE_INVALID))
		{
			if (*new_ == '\0') {
				tray_handle_item_removed(tray, name);
				emit_sni_signal(conn,
					"StatusNotifierItemUnregistered",
					name);
			}
		}
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult
snw_path_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	if (dbus_message_is_method_call(
		msg, "org.kde.DBus.Introspectable", "Introspect"))
		return handle_introspect(conn, msg, user_data);

	if (dbus_message_is_method_call(
		msg, "org.freedesktop.DBus.Properties", "Get"))
		return handle_get_prop(conn, msg, user_data);

	if (dbus_message_is_method_call(
		msg, SNW_IFACE, "RegisterStatusNotifierItem"))
		return handle_register_item(conn, msg, user_data);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable snw_vtable = {
	.message_function = snw_path_handler,
};

int
systray_watcher_start(DBusConnection *conn, systray_t *tray)
{
	DBusError err;
	int ret;

	dbus_error_init(&err);
	const char *watcher_names[] = {
		"org.freedesktop.StatusNotifierWatcher",
		"org.kde.StatusNotifierWatcher",
		"org.ayatana.StatusNotifierWatcher",
	};

	for (size_t i = 0; i < sizeof watcher_names/sizeof *watcher_names; i++) {
		ret = dbus_bus_request_name(
			conn, watcher_names[i],
			DBUS_NAME_FLAG_REPLACE_EXISTING|
			DBUS_NAME_FLAG_DO_NOT_QUEUE,
			&err);
		if (ret < 0) {
			fprintf(stderr, "cannot own %s: %s\n",
		   watcher_names[i], err.message);
			dbus_error_free(&err);
		}
	}

	if (!dbus_connection_register_object_path(
		conn, SNW_PATH, &snw_vtable, tray))
	{
		fprintf(stderr, "Failed to register %s\n", SNW_PATH);
		return -1;
	}
	dbus_bus_add_match(conn,
					"type='signal',"
					"sender='org.freedesktop.DBus',"
					"interface='org.freedesktop.DBus',"
					"member='NameOwnerChanged'", &err);
	dbus_connection_add_filter(conn, name_owner_changed_filter, tray, NULL);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr,"systray: match error: %s\n", err.message);
		dbus_error_free(&err);
	}
	dbus_error_free(&err);
	return 0;
}

void
systray_watcher_stop(DBusConnection *conn)
{
	dbus_connection_unregister_object_path(conn, SNW_PATH);
	dbus_bus_release_name(conn, "org.freedesktop.StatusNotifierWatcher", NULL);
	dbus_bus_release_name(conn, "org.kde.StatusNotifierWatcher", NULL);
	dbus_bus_release_name(conn, "org.ayatana.StatusNotifierWatcher", NULL);
}
