/*
 * libomegle
 *
 * libomegle is the property of its developers.  See the COPYRIGHT file
 * for more details.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OMEGLE_CONNECTION_H
#define OMEGLE_CONNECTION_H

#include "libomegle.h"

/*
 * This is a bitmask.
 */
typedef enum
{
	OM_METHOD_GET  = 0x0001,
	OM_METHOD_POST = 0x0002,
	OM_METHOD_SSL  = 0x0004
} OmegleMethod;

typedef struct _OmegleConnection OmegleConnection;
struct _OmegleConnection {
	OmegleAccount *oma;
	OmegleMethod method;
	gchar *hostname;
	gchar *url;
	GString *request;
	OmegleProxyCallbackFunc callback;
	gpointer user_data;
	char *rx_buf;
	size_t rx_len;
	PurpleProxyConnectData *connect_data;
	PurpleSslConnection *ssl_conn;
	int fd;
	guint input_watcher;
	gboolean connection_keepalive;
	time_t request_time;
};

void om_connection_destroy(OmegleConnection *omconn);
void om_post_or_get(OmegleAccount *oma, OmegleMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		OmegleProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive);

#endif /* OMEGLE_CONNECTION_H */
