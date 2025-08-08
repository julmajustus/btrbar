/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   systray_watcher.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/26 02:08:20 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 01:34:58 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dbus/dbus-shared.h"
#include <stdint.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "systray_watcher.h"
#include <wayland-util.h>
#include <stdlib.h>
#include <stdio.h>

static const char *SNW_NAMES[] = {
	"org.freedesktop.StatusNotifierWatcher",
	"org.kde.StatusNotifierWatcher",
	"org.ayatana.StatusNotifierWatcher"
};

#define N_SNW_NAMES (sizeof SNW_NAMES / sizeof *SNW_NAMES)
#define SNW_PATH "/StatusNotifierWatcher"

static const char *SNI_IFACE = "org.kde.StatusNotifierItem";
static const char *PROP_IFACE = "org.freedesktop.DBus.Properties";

static const char *introspect_xml =
	"<node>"
	"  <interface name=\"org.freedesktop.DBus.Introspectable\">"
	"    <method name=\"Introspect\">"
	"      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>"
	"    </method>"
	"  </interface>"
	"  <interface name=\"org.freedesktop.StatusNotifierWatcher\">"
	"    <method name=\"RegisterStatusNotifierHost\"><arg name=\"service\" type=\"s\" direction=\"in\"/></method>"
	"    <method name=\"RegisterStatusNotifierItem\"><arg name=\"service\" type=\"s\" direction=\"in\"/></method>"
	"    <signal name=\"StatusNotifierHostRegistered\"><arg name=\"service\" type=\"s\"/></signal>"
	"    <signal name=\"StatusNotifierHostUnregistered\"><arg name=\"service\" type=\"s\"/></signal>"
	"    <signal name=\"StatusNotifierItemRegistered\"><arg name=\"service\" type=\"s\"/></signal>"
	"    <signal name=\"StatusNotifierItemUnregistered\"><arg name=\"service\" type=\"s\"/></signal>"
	"	 <property name=\"IsStatusNotifierHostRegistered\" type=\"b\" access=\"read\"/>"
	"	 <property name=\"RegisteredStatusNotifierItems\" type=\"as\" access=\"read\"/>"
	"  </interface>"
	"  <interface name=\"org.freedesktop.DBus.Properties\">"
	"    <method name=\"Get\">"
	"      <arg name=\"interface\" type=\"s\" direction=\"in\"/>"
	"      <arg name=\"property\" type=\"s\" direction=\"in\"/>"
	"      <arg name=\"value\" type=\"v\" direction=\"out\"/>"
	"    </method>"
	"    <method name=\"GetAll\">"
	"      <arg name=\"interface\" type=\"s\" direction=\"in\"/>"
	"      <arg name=\"properties\" type=\"a{sv}\" direction=\"out\"/>"
	"    </method>"
	"  </interface>"
	"</node>";

static void
emit_watcher_signal(DBusConnection *conn, const char *signal_name,
					const char *service, const char *obj_path)
{
	// fprintf(stderr, "Check emit_watcher_signal: signal_name: %s service: %s object_path: %s\n", signal_name, service, obj_path);
	char full[1024];
	snprintf(full, sizeof full, "%s%s", service, obj_path);
	for (uint32_t i = 0; i < N_SNW_NAMES; i++) {
		DBusMessage *m = dbus_message_new_signal(
			SNW_PATH, SNW_NAMES[i], signal_name);
		const char *arg = full;
		dbus_message_append_args(m, DBUS_TYPE_STRING, &arg, DBUS_TYPE_INVALID);
		dbus_connection_send(conn, m, NULL);
		dbus_message_unref(m);
	}
}

static DBusHandlerResult
handle_register_host(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	(void)user_data;
	const char *service;
	dbus_message_get_args(msg, NULL,
					   DBUS_TYPE_STRING, &service,
					   DBUS_TYPE_INVALID);
	DBusMessage *reply = dbus_message_new_method_return(msg);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);

	emit_watcher_signal(conn, "StatusNotifierHostRegistered",
					 service, service[0]=='/' ? service : "");
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_introspect(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	(void)user_data;
	DBusMessage *reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspect_xml, DBUS_TYPE_INVALID);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_get_prop(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const char *iface, *prop;
	DBusError err;
	DBusMessageIter root, variant, array;
	systray_t *tray = user_data;

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &iface,
							DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID)) {
		dbus_error_free(&err);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	if (strcmp(prop, "IsStatusNotifierHostRegistered") == 0) {
		dbus_bool_t yes = TRUE;
		DBusMessage *reply = dbus_message_new_method_return(msg);
		dbus_message_iter_init_append(reply, &root);
		dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT, "b", &variant);
		dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &yes);
		dbus_message_iter_close_container(&root, &variant);
		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	if (strcmp(prop, "RegisteredStatusNotifierItems") != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	int is_watcher_iface = 0;
	for (uint32_t i = 0; i < N_SNW_NAMES; i++) {
		if (strcmp(iface, SNW_NAMES[i]) == 0) {
			is_watcher_iface = 1;
			break;
		}
	}
	if (!is_watcher_iface)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessage *reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &root);
	dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT, "as", &variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &array);
	for (size_t i = 0; i < tray->n_items; i++) {
		char full[1024];
		snprintf(full, sizeof full, "%s%s", tray->items[i].service, tray->items[i].path);
		dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &full);
	}
	dbus_message_iter_close_container(&variant, &array);
	dbus_message_iter_close_container(&root, &variant);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_get_all_props(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const char *iface;
	DBusError err;
	DBusMessageIter root, array, entry, variant, str_array;
	systray_t *tray = user_data;

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID)) {
		dbus_error_free(&err);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	int is_watcher_iface = 0;
	for (uint32_t i = 0; i < N_SNW_NAMES; i++) {
		if (strcmp(iface, SNW_NAMES[i]) == 0) {
			is_watcher_iface = 1;
			break;
		}
	}
	if (!is_watcher_iface)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessage *reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &root);
	dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY,"{sv}", &array);
	dbus_message_iter_open_container(&array, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	const char *key = "RegisteredStatusNotifierItems";
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &str_array);
	for (size_t i = 0; i < tray->n_items; i++) {
		char full[1024];
		snprintf(full, sizeof full, "%s%s", tray->items[i].service, tray->items[i].path);
		dbus_message_iter_append_basic(&str_array, DBUS_TYPE_STRING, &full);
	}
	dbus_message_iter_close_container(&variant, &str_array);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(&root, &array);

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}


static const char *
fetch_object_path_property(DBusConnection *conn, const char *bus, const char *obj_path,
						   const char *iface, const char *prop)
{
	DBusMessage *m, *r;
	DBusError err;
	DBusMessageIter iter, variant;
	const char *out_path = NULL;
	const char *ret = NULL;

	dbus_error_init(&err);

	m = dbus_message_new_method_call(bus, obj_path, PROP_IFACE, "Get");
	if (!m)
		goto out;

	dbus_message_append_args(m, DBUS_TYPE_STRING,  &iface,
						  DBUS_TYPE_STRING,  &prop, DBUS_TYPE_INVALID);

	r = dbus_connection_send_with_reply_and_block(conn, m, 1000, &err);
	dbus_message_unref(m);
	if (!r || dbus_error_is_set(&err)) {
		fprintf(stderr, "Failed to Get %s.%s: %s\n", iface, prop,
		  err.message ? err.message : "(no error)");
		dbus_error_free(&err);
		goto out;
	}

	if (dbus_message_iter_init(r, &iter) &&
		dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT)
	{
		dbus_message_iter_recurse(&iter, &variant);
		if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_OBJECT_PATH)
		{
			dbus_message_iter_get_basic(&variant, &out_path);
			ret = out_path;
		}
	}
	dbus_message_unref(r);

out:
	return ret;}

static DBusHandlerResult
handle_register_item(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const char *service = NULL;
	const char *object_path = NULL;
	const char *menu_path = NULL;
	DBusMessageIter iter;
	systray_t *tray = user_data;

	if (dbus_message_iter_init(msg, &iter)) {
		service = dbus_message_get_sender(msg);
		do {
			int t = dbus_message_iter_get_arg_type(&iter);
			if (t == DBUS_TYPE_STRING)
				dbus_message_iter_get_basic(&iter, &object_path);
			else if (t == DBUS_TYPE_OBJECT_PATH)
				dbus_message_iter_get_basic(&iter, &object_path);
			if (strcmp(service, object_path) == 0)
				object_path = NULL;
		} while (dbus_message_iter_next(&iter));
	}
	if (!service)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (!object_path)
		object_path = "/StatusNotifierItem";

	// fprintf(stderr, "\n\nRegister: service=%s path=%s\n", service, object_path);

	menu_path = fetch_object_path_property(conn, service, object_path, SNI_IFACE, "Menu");
	// fprintf(stderr, "Check service: %s menupath: %s\n\n", service, menu_path);
	if (!menu_path)
		menu_path = "/StatusNotifierItem";

	tray_handle_item_added(tray, service, object_path, menu_path);


	DBusMessage *reply = dbus_message_new_method_return(msg);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);

	// fprintf(stderr, "Check service: %s object_path: %s\n",service, object_path);
	emit_watcher_signal(conn, "StatusNotifierItemRegistered", service, object_path);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
name_owner_changed_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	systray_t *tray = user_data;
	if (dbus_message_is_signal(msg, "org.freedesktop.DBus", "NameOwnerChanged")) {
		const char *name;
		const char *old_owner;
		const char *new_owner;
		DBusError err;
		dbus_error_init(&err);

		if (dbus_message_get_args(msg, &err,
							DBUS_TYPE_STRING, &name,
							DBUS_TYPE_STRING, &old_owner,
							DBUS_TYPE_STRING, &new_owner,
							DBUS_TYPE_INVALID)) {
			if (old_owner && *old_owner != '\0' && (!new_owner || *new_owner == '\0')) {
				tray_handle_item_removed(tray, name);
				emit_watcher_signal(conn, "StatusNotifierItemUnregistered", name, "/StatusNotifierItem");
			}
		} 
		else 
		dbus_error_free(&err);
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult
snw_path_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	if (dbus_message_is_method_call(msg,
								 "org.freedesktop.DBus.Introspectable", "Introspect")) {
		return handle_introspect(conn, msg, user_data);
	}
	if (dbus_message_is_method_call(msg,
								 "org.freedesktop.DBus.Properties", "Get")) {
		return handle_get_prop(conn, msg, user_data);
	}
	if (dbus_message_is_method_call(msg,
								 "org.freedesktop.DBus.Properties", "GetAll")) {
		return handle_get_all_props(conn, msg, user_data);
	}
	for (uint32_t i = 0; i < N_SNW_NAMES; i++) {
		if (dbus_message_is_method_call(msg,
								  SNW_NAMES[i], "RegisterStatusNotifierHost")) {
			return handle_register_host(conn, msg, user_data);
		}
		if (dbus_message_is_method_call(msg,
								  SNW_NAMES[i], "RegisterStatusNotifierItem")) {
			return handle_register_item(conn, msg, user_data);
		}
	}
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
	for (uint32_t i = 0; i < N_SNW_NAMES; i++) {
		ret = dbus_bus_request_name(conn, SNW_NAMES[i],
							  DBUS_NAME_FLAG_REPLACE_EXISTING |
							  DBUS_NAME_FLAG_DO_NOT_QUEUE,
							  &err);
		if (ret < 0) {
			fprintf(stderr, "Cannot own %s: %s\n", SNW_NAMES[i], err.message);
			dbus_error_free(&err);
		}
	}

	if (!dbus_connection_register_object_path(conn, SNW_PATH, &snw_vtable, tray)) {
		fprintf(stderr, "Failed to register %s\n", SNW_PATH);
		return -1;
	}

	dbus_bus_add_match(conn, "type='signal'," "sender='org.freedesktop.DBus',"
					"interface='org.freedesktop.DBus'," "member='NameOwnerChanged'", &err);
	dbus_connection_add_filter(conn, name_owner_changed_filter, tray, NULL);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "systray: match error: %s\n", err.message);
		dbus_error_free(&err);
	}
	dbus_error_free(&err);
	return 0;
}

void
systray_watcher_stop(DBusConnection *conn)
{
	dbus_connection_unregister_object_path(conn, SNW_PATH);
	for (uint32_t i = 0; i < N_SNW_NAMES; i++)
		dbus_bus_release_name(conn, SNW_NAMES[i], NULL);
}
