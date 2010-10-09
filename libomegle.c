/*
 * libfacebook
 *
 * libfacebook is the property of its developers.  See the COPYRIGHT file
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

#include "libomegle.h"
#include "om_connection.h"

#include <json-glib/json-glib.h>

static void om_got_events(OmegleAccount *oma, gchar *response, gsize len,
		gpointer userdata);

/******************************************************************************/
/* PRPL functions */
/******************************************************************************/

static const char *om_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "omegle";
}

static GList *om_statuses(PurpleAccount *account)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, NULL, FALSE, FALSE, FALSE);
	types = g_list_append(types, status);

	return types;
}

static void om_login(PurpleAccount *account)
{
	PurpleBuddy *bud;
	OmegleAccount *oma;
	
	//make sure there's an Omegle buddy on the buddy list
	bud = purple_find_buddy(account, "omegle");
	if (bud == NULL)
	{
		bud = purple_buddy_new(account, "omegle", "Omegle");
		purple_blist_add_buddy(bud, NULL, NULL, NULL);
	}
	purple_prpl_got_user_status(account, "omegle", purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);
	
	
	oma = g_new0(OmegleAccount, 1);
	oma->account = account;
	oma->pc = purple_account_get_connection(account);
	oma->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	oma->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	account->gc->proto_data = oma;
	
	//No such thing as a login
	purple_connection_set_state(purple_account_get_connection(account), PURPLE_CONNECTED);
	
}

static void om_close(PurpleConnection *pc)
{
	OmegleAccount *oma;
	GList *ims;
	
	g_return_if_fail(pc != NULL);
	g_return_if_fail(pc->proto_data != NULL);
	
	ims = purple_get_ims();
	//TODO: Loop through all im's and disconnect them all
	
	oma = pc->proto_data;
	
	while (oma->conns != NULL)
		om_connection_destroy(oma->conns->data);
	while (oma->dns_queries != NULL) {
		PurpleDnsQueryData *dns_query = oma->dns_queries->data;
		oma->dns_queries = g_slist_remove(oma->dns_queries, dns_query);
		purple_dnsquery_destroy(dns_query);
	}

	g_hash_table_destroy(oma->cookie_table);
	g_hash_table_destroy(oma->hostname_ip_cache);
	
	g_free(oma);
}

static void om_convo_closed(PurpleConnection *pc, const char *who)
{
	OmegleAccount *oma;
	gchar *postdata;
	
	oma = pc->proto_data;
	postdata = g_strdup_printf("id=%s", purple_url_encode(who));
	
	om_post_or_get(oma, OM_METHOD_POST, NULL, "/disconnect",
				postdata, NULL, NULL, FALSE);
				
	g_free(postdata);
}

static void om_fetch_events(OmegleAccount *oma, gchar *who)
{
	gchar *postdata;
	
	postdata = g_strdup_printf("id=%s", purple_url_encode(who));
	
	om_post_or_get(oma, OM_METHOD_POST, NULL, "/events",
				postdata, om_got_events, who, FALSE);
				
	g_free(postdata);
}

static void om_got_events(OmegleAccount *oma, gchar *response, gsize len,
		gpointer userdata)
{
	//[["waiting"], ["connected"]]
	gchar *who = userdata;
	const gchar *message;
	const gchar *event_type;
	JsonParser *parser;
	JsonNode *rootnode, *currentnode;
	JsonArray *array, *current;
	guint i;

	purple_debug_info("omegle", "got events: %s\n", response?response:"(null)");
	
	if (!response || g_str_equal(response, "null"))
	{
		g_free(who);
		return;
	}
	
	parser = json_parser_new();
	json_parser_load_from_data(parser, response, len, NULL);
	rootnode = json_parser_get_root(parser);
	if (!rootnode)
	{
		g_object_unref(parser);
		return;
	}
	array = json_node_get_array(rootnode);
	
	for(i=0; i<json_array_get_length(array); i++)
	{
		currentnode = json_array_get_element(array, i);
		current = json_node_get_array(currentnode);
		event_type = json_node_get_string(json_array_get_element(current, 0));
		if (!event_type)
		{
			continue;
		} else if (g_str_equal(event_type, "waiting")) {
			serv_got_im(oma->pc, who, "Looking for someone you can chat with. Hang on.", PURPLE_MESSAGE_SYSTEM, time(NULL));
		} else if (g_str_equal(event_type, "connected")) {
			serv_got_im(oma->pc, who, "You're now chatting with a random stranger. Say hi!", PURPLE_MESSAGE_SYSTEM, time(NULL));
		} else if (g_str_equal(event_type, "gotMessage")) {
			//[["gotMessage","message goes here"]]
			message = json_node_get_string(json_array_get_element(current, 1));
			if (message)
				serv_got_im(oma->pc, who, message, PURPLE_MESSAGE_RECV, time(NULL));
		} else if (g_str_equal(event_type, "typing")) {
			serv_got_typing(oma->pc, who, 10, PURPLE_TYPING);
		} else if (g_str_equal(event_type, "stoppedTyping")) {
			serv_got_typing(oma->pc, who, 10, PURPLE_TYPED);
		} else if (g_str_equal(event_type, "strangerDisconnected")) {
			serv_got_im(oma->pc, who, "Your conversational partner has disconnected", PURPLE_MESSAGE_SYSTEM, time(NULL));
		}
	}
	
	om_fetch_events(oma, g_strdup(who));
	
	g_free(who);
	g_object_unref(parser);
}

static void om_start_im_cb(OmegleAccount *oma, gchar *response, gsize len,
		gpointer userdata)
{
	gchar *id;
	
	//This should come back with an ID that we pass around
	id = g_strdup(response);
	purple_str_strip_char(id, '"');
	
	//Start the event loop
	om_fetch_events(oma, g_strdup(id));
	
	g_free(id);
}

static void om_start_im(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	OmegleAccount *oma;
	PurpleConnection *pc;
	
	if(!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return;
	buddy = (PurpleBuddy *) node;
	if (!buddy)
		return;
	pc = purple_account_get_connection(buddy->account);
	oma = pc->proto_data;
	
	om_post_or_get(oma, OM_METHOD_POST, NULL, "/start",
				NULL, om_start_im_cb, NULL, FALSE);
}

static GList *om_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	PurpleBuddy *buddy;
	
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		buddy = (PurpleBuddy *)node;
		
		act = purple_menu_action_new(_("_Start random IM"),
				PURPLE_CALLBACK(om_start_im),
				NULL, NULL);
		m = g_list_append(m, act);
	}
	return m;
}

static unsigned int om_send_typing(PurpleConnection *pc, const gchar *name,
		PurpleTypingState state)
{
	gchar *postdata;
	OmegleAccount *oma = pc->proto_data;
	gchar *url;

	g_return_val_if_fail(oma != NULL, 0);

	if (state == PURPLE_TYPING)
	{
		url = "/typing";	
	} else if (state == PURPLE_TYPED)
	{
		url = "/stoppedtyping";
	} else {
		return 0;
	}
	
	postdata = g_strdup_printf("id=%s", purple_url_encode(name));
	
	om_post_or_get(oma, OM_METHOD_POST, NULL, url, postdata, NULL, NULL, FALSE);
	
	g_free(postdata);
	
	return 10;
}

static int om_send_im(PurpleConnection *pc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	OmegleAccount *oma;
	gchar *encoded_name;
	gchar *encoded_message;
	gchar *postdata;
	
	oma = pc->proto_data;
	encoded_name = g_strdup(purple_url_encode(who));
	encoded_message = g_strdup(purple_url_encode(message));
	
	postdata = g_strdup_printf("id=%s&msg=%s", encoded_name, encoded_message);
	
	om_post_or_get(oma, OM_METHOD_POST, NULL, "/send", postdata, NULL, NULL, FALSE);

	g_free(postdata);
	g_free(encoded_name);
	g_free(encoded_message);

	return strlen(message);
}

/******************************************************************************/
/* Plugin functions */
/******************************************************************************/

static gboolean plugin_load(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;

	option = purple_account_option_string_new("Server", "host", "bajor.omegle.com");
	prpl_info->protocol_options = g_list_append(
		prpl_info->protocol_options, option);
		
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static void plugin_init(PurplePlugin *plugin)
{

}

static PurplePluginProtocolInfo prpl_info = {
	/* options */
	OPT_PROTO_NO_PASSWORD,

	NULL,                   /* user_splits */
	NULL,                   /* protocol_options */
	NO_BUDDY_ICONS,         /* icon_spec */
	om_list_icon,           /* list_icon */
	NULL,                   /* list_emblems */
	NULL,                   /* status_text */
	NULL,                   /* tooltip_text */
	om_statuses,            /* status_types */
	om_node_menu,           /* blist_node_menu */
	NULL,                   /* chat_info */
	NULL,                   /* chat_info_defaults */
	om_login,               /* login */
	om_close,               /* close */
	om_send_im,             /* send_im */
	NULL,                   /* set_info */
	om_send_typing,         /* send_typing */
	NULL,                   /* get_info */
	NULL,                   /* set_status */
	NULL,                   /* set_idle */
	NULL,                   /* change_passwd */
	NULL,                   /* add_buddy */
	NULL,                   /* add_buddies */
	NULL,                   /* remove_buddy */
	NULL,                   /* remove_buddies */
	NULL,                   /* add_permit */
	NULL,                   /* add_deny */
	NULL,                   /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,                   /* set_permit_deny */
	NULL,                   /* join_chat */
	NULL,                   /* reject chat invite */
	NULL,                   /* get_chat_name */
	NULL,                   /* chat_invite */
	NULL,                   /* chat_leave */
	NULL,                   /* chat_whisper */
	NULL,                   /* chat_send */
	NULL,                   /* keepalive */
	NULL,                   /* register_user */
	NULL,                   /* get_cb_info */
	NULL,                   /* get_cb_away */
	NULL,                   /* alias_buddy */
	NULL,                   /* group_buddy */
	NULL,                   /* rename_group */
	NULL,                   /* buddy_free */
	om_convo_closed,        /* convo_closed */
	purple_normalize_nocase,/* normalize */
	NULL,                   /* set_buddy_icon */
	NULL,                   /* remove_group */
	NULL,                   /* get_cb_real_name */
	NULL,                   /* set_chat_topic */
	NULL,                   /* find_blist_chat */
	NULL,                   /* roomlist_get_list */
	NULL,                   /* roomlist_cancel */
	NULL,                   /* roomlist_expand_category */
	NULL,                   /* can_receive_file */
	NULL,                   /* send_file */
	NULL,                   /* new_xfer */
	NULL,                   /* offline_message */
	NULL,                   /* whiteboard_prpl_ops */
	NULL,                   /* send_raw */
	NULL,                   /* roomlist_room_serialize */
	NULL,                   /* unregister_user */
	NULL,                   /* send_attention */
	NULL,                   /* attention_types */
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 5
	sizeof(PurplePluginProtocolInfo), /* struct_size */
	NULL,                   /* get_account_text_table */
#else
	(gpointer) sizeof(PurplePluginProtocolInfo)
#endif
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	2,						/* major_version */
	3, 						/* minor version */
	PURPLE_PLUGIN_PROTOCOL, 			/* type */
	NULL, 						/* ui_requirement */
	0, 						/* flags */
	NULL, 						/* dependencies */
	PURPLE_PRIORITY_DEFAULT, 			/* priority */
	OMEGLE_PLUGIN_ID,				/* id */
	"Omegle", 					/* name */
	OMEGLE_PLUGIN_VERSION, 			/* version */
	N_("Omegle Protocol Plugin"), 		/* summary */
	N_("Omegle Protocol Plugin"), 		/* description */
	"Eion Robb <eionrobb@gmail.com>", 		/* author */
	"http://pidgin-omegle.googlecode.com/",	/* homepage */
	plugin_load, 					/* load */
	plugin_unload, 					/* unload */
	NULL, 						/* destroy */
	NULL, 						/* ui_info */
	&prpl_info, 					/* extra_info */
	NULL, 						/* prefs_info */
	NULL, 					/* actions */

							/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(facebook, plugin_init, info);
