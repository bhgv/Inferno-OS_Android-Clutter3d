/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Red Hat, Inc.
 */

/* This doesn't implement full server-side NTLM, and it mostly doesn't
 * even test that the client is doing the crypto/encoding/etc parts of
 * NTLM correctly. It only tests that the right message headers get
 * set in the right messages.
 */

#include "test-utils.h"

typedef enum {
	NTLM_UNAUTHENTICATED,
	NTLM_RECEIVED_REQUEST,
	NTLM_SENT_CHALLENGE,
	NTLM_AUTHENTICATED_ALICE,
	NTLM_AUTHENTICATED_BOB
} NTLMServerState;

#define NTLM_REQUEST_START "TlRMTVNTUAABAAAA"
#define NTLM_RESPONSE_START "TlRMTVNTUAADAAAA"

#define NTLM_CHALLENGE "TlRMTVNTUAACAAAADAAMADAAAAABAoEAASNFZ4mrze8AAAAAAAAAAGIAYgA8AAAARABPAE0AQQBJAE4AAgAMAEQATwBNAEEASQBOAAEADABTAEUAUgBWAEUAUgAEABQAZABvAG0AYQBpAG4ALgBjAG8AbQADACIAcwBlAHIAdgBlAHIALgBkAG8AbQBhAGkAbgAuAGMAbwBtAAAAAAA="

#define NTLM_RESPONSE_USER(response) ((response)[102] == 'E' ? NTLM_AUTHENTICATED_ALICE : NTLM_AUTHENTICATED_BOB)

static void
clear_state (gpointer connections, GObject *ex_connection)
{
	g_hash_table_remove (connections, ex_connection);
}

static void
server_callback (SoupServer *server, SoupMessage *msg,
		 const char *path, GHashTable *query,
		 SoupClientContext *client, gpointer data)
{
	GHashTable *connections = data;
	SoupSocket *socket;
	const char *auth;
	NTLMServerState state, required_user = 0;
	gboolean auth_required, not_found = FALSE;
	gboolean basic_allowed = TRUE, ntlm_allowed = TRUE;

	if (msg->method != SOUP_METHOD_GET) {
		soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
		return;
	}

	if (!strncmp (path, "/alice", 6))
		required_user = NTLM_AUTHENTICATED_ALICE;
	else if (!strncmp (path, "/bob", 4))
		required_user = NTLM_AUTHENTICATED_BOB;
	else if (!strncmp (path, "/either", 7))
		;
	else if (!strncmp (path, "/basic", 6))
		ntlm_allowed = FALSE;
	else if (!strncmp (path, "/noauth", 7))
		basic_allowed = ntlm_allowed = FALSE;
	auth_required = ntlm_allowed || basic_allowed;

	if (strstr (path, "/404"))
		not_found = TRUE;

	socket = soup_client_context_get_socket (client);
	state = GPOINTER_TO_INT (g_hash_table_lookup (connections, socket));
	auth = soup_message_headers_get_one (msg->request_headers,
					     "Authorization");

	if (auth) {
		if (!strncmp (auth, "NTLM ", 5)) {
			if (!strncmp (auth + 5, NTLM_REQUEST_START,
				      strlen (NTLM_REQUEST_START))) {
				state = NTLM_RECEIVED_REQUEST;
				/* If they start, they must finish, even if
				 * it was unnecessary.
				 */
				auth_required = ntlm_allowed = TRUE;
				basic_allowed = FALSE;
			} else if (state == NTLM_SENT_CHALLENGE &&
				   !strncmp (auth + 5, NTLM_RESPONSE_START,
					     strlen (NTLM_RESPONSE_START))) {
				state = NTLM_RESPONSE_USER (auth + 5);
			} else
				state = NTLM_UNAUTHENTICATED;
		} else if (basic_allowed && !strncmp (auth, "Basic ", 6)) {
			gsize len;
			char *decoded = (char *)g_base64_decode (auth + 6, &len);

			if (!strncmp (decoded, "alice:password", len) &&
			    required_user != NTLM_AUTHENTICATED_BOB)
				auth_required = FALSE;
			else if (!strncmp (decoded, "bob:password", len) &&
				 required_user != NTLM_AUTHENTICATED_ALICE)
				auth_required = FALSE;
			g_free (decoded);
		}
	}

	if (ntlm_allowed && state > NTLM_SENT_CHALLENGE &&
	    (!required_user || required_user == state))
		auth_required = FALSE;

	if (auth_required) {
		soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED);

		if (basic_allowed && state != NTLM_RECEIVED_REQUEST) {
			soup_message_headers_append (msg->response_headers,
						     "WWW-Authenticate",
						     "Basic realm=\"ntlm-test\"");
		}

		if (ntlm_allowed && state == NTLM_RECEIVED_REQUEST) {
			soup_message_headers_append (msg->response_headers,
						     "WWW-Authenticate",
						     "NTLM " NTLM_CHALLENGE);
			state = NTLM_SENT_CHALLENGE;
		} else if (ntlm_allowed) {
			soup_message_headers_append (msg->response_headers,
						     "WWW-Authenticate", "NTLM");
			soup_message_headers_append (msg->response_headers,
						     "Connection", "close");
		}
	} else {
		if (not_found)
			soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
		else {
			soup_message_set_response (msg, "text/plain",
						   SOUP_MEMORY_STATIC,
						   "OK\r\n", 4);
			soup_message_set_status (msg, SOUP_STATUS_OK);
		}
	}

	g_hash_table_insert (connections, socket, GINT_TO_POINTER (state));
	g_object_weak_ref (G_OBJECT (socket), clear_state, connections);
}

static void
authenticate (SoupSession *session, SoupMessage *msg,
	      SoupAuth *auth, gboolean retrying, gpointer user)
{
	if (!retrying)
		soup_auth_authenticate (auth, user, "password");
}

typedef struct {
	gboolean got_ntlm_prompt;
	gboolean got_basic_prompt;
	gboolean sent_ntlm_request;
	gboolean got_ntlm_challenge;
	gboolean sent_ntlm_response;
	gboolean sent_basic_response;
} NTLMState;

static void
prompt_check (SoupMessage *msg, gpointer user_data)
{
	NTLMState *state = user_data;
	const char *header;

	header = soup_message_headers_get_list (msg->response_headers,
						"WWW-Authenticate");
	if (header && strstr (header, "Basic "))
		state->got_basic_prompt = TRUE;
	if (header && strstr (header, "NTLM") &&
	    !strstr (header, NTLM_CHALLENGE))
		state->got_ntlm_prompt = TRUE;
}

static void
challenge_check (SoupMessage *msg, gpointer user_data)
{
	NTLMState *state = user_data;
	const char *header;

	header = soup_message_headers_get_list (msg->response_headers,
						"WWW-Authenticate");
	if (header && !strncmp (header, "NTLM ", 5))
		state->got_ntlm_challenge = TRUE;
}

static void
request_check (SoupMessage *msg, gpointer user_data)
{
	NTLMState *state = user_data;
	const char *header;

	header = soup_message_headers_get_one (msg->request_headers,
					       "Authorization");
	if (header && !strncmp (header, "NTLM " NTLM_REQUEST_START,
				strlen ("NTLM " NTLM_REQUEST_START)))
		state->sent_ntlm_request = TRUE;
}

static void
response_check (SoupMessage *msg, gpointer user_data)
{
	NTLMState *state = user_data;
	const char *header;

	header = soup_message_headers_get_one (msg->request_headers,
					       "Authorization");
	if (header && !strncmp (header, "NTLM " NTLM_RESPONSE_START,
				strlen ("NTLM " NTLM_RESPONSE_START)))
		state->sent_ntlm_response = TRUE;
	if (header && !strncmp (header, "Basic ", 6))
		state->sent_basic_response = TRUE;
}

static void
do_message (SoupSession *session, SoupURI *base_uri, const char *path,
	    gboolean get_ntlm_prompt, gboolean do_ntlm,
	    gboolean get_basic_prompt, gboolean do_basic,
	    guint status_code)
{
	SoupURI *uri;
	SoupMessage *msg;
	NTLMState state = { FALSE, FALSE, FALSE, FALSE };

	uri = soup_uri_new_with_base (base_uri, path);
	msg = soup_message_new_from_uri ("GET", uri);
	soup_uri_free (uri);

	g_signal_connect (msg, "got_headers",
			  G_CALLBACK (prompt_check), &state);
	g_signal_connect (msg, "got_headers",
			  G_CALLBACK (challenge_check), &state);
	g_signal_connect (msg, "wrote-headers",
			  G_CALLBACK (request_check), &state);
	g_signal_connect (msg, "wrote-headers",
			  G_CALLBACK (response_check), &state);

	soup_session_send_message (session, msg);
	debug_printf (1, "  %-10s -> ", path);

	if (state.got_ntlm_prompt) {
		debug_printf (1, " NTLM_PROMPT");
		if (!get_ntlm_prompt) {
			debug_printf (1, "???");
			errors++;
		}
	} else if (get_ntlm_prompt) {
		debug_printf (1, " no-ntlm-prompt???");
		errors++;
	}

	if (state.got_basic_prompt) {
		debug_printf (1, " BASIC_PROMPT");
		if (!get_basic_prompt) {
			debug_printf (1, "???");
			errors++;
		}
	} else if (get_basic_prompt) {
		debug_printf (1, " no-basic-prompt???");
		errors++;
	}

	if (state.sent_ntlm_request) {
		debug_printf (1, " REQUEST");
		if (!do_ntlm) {
			debug_printf (1, "???");
			errors++;
		}
	} else if (do_ntlm) {
		debug_printf (1, " no-request???");
		errors++;
	}

	if (state.got_ntlm_challenge) {
		debug_printf (1, " CHALLENGE");
		if (!do_ntlm) {
			debug_printf (1, "???");
			errors++;
		}
	} else if (do_ntlm) {
		debug_printf (1, " no-challenge???");
		errors++;
	}

	if (state.sent_ntlm_response) {
		debug_printf (1, " NTLM_RESPONSE");
		if (!do_ntlm) {
			debug_printf (1, "???");
			errors++;
		}
	} else if (do_ntlm) {
		debug_printf (1, " no-ntlm-response???");
		errors++;
	}

	if (state.sent_basic_response) {
		debug_printf (1, " BASIC_RESPONSE");
		if (!do_basic) {
			debug_printf (1, "???");
			errors++;
		}
	} else if (do_basic) {
		debug_printf (1, " no-basic-response???");
		errors++;
	}

	debug_printf (1, " -> %s", msg->reason_phrase);
	if (msg->status_code != status_code) {
		debug_printf (1, "???");
		errors++;
	}
	debug_printf (1, "\n");

	g_object_unref (msg);
}

static void
do_ntlm_round (SoupURI *base_uri, gboolean use_ntlm, const char *user)
{
	SoupSession *session;
	gboolean alice = !g_strcmp0 (user, "alice");
	gboolean bob = !g_strcmp0 (user, "bob");
	gboolean alice_via_ntlm = use_ntlm && alice;
	gboolean bob_via_ntlm = use_ntlm && bob;
	gboolean alice_via_basic = !use_ntlm && alice;

	session = soup_test_session_new (SOUP_TYPE_SESSION_ASYNC, NULL);
	if (use_ntlm)
		soup_session_add_feature_by_type (session, SOUP_TYPE_AUTH_NTLM);

	if (user) {
		g_signal_connect (session, "authenticate",
				  G_CALLBACK (authenticate), (char *)user);
	}

	/* 1. Server doesn't request auth, so both get_ntlm_prompt and
	 * get_basic_prompt are both FALSE, and likewise do_basic. But
	 * if we're using NTLM we'll try that even without the server
	 * asking.
	 */
	do_message (session, base_uri, "/noauth",
		    FALSE, use_ntlm,
		    FALSE, FALSE,
		    SOUP_STATUS_OK);

	/* 2. Server requires auth as Alice, so it will request that
	 * if we didn't already authenticate the connection to her in
	 * the previous step. If we authenticated as Bob in the
	 * previous step, then we'll just immediately get a 401 here.
	 * So in no case will we see the client try to do_ntlm.
	 */
	do_message (session, base_uri, "/alice",
		    !alice_via_ntlm, FALSE,
		    !alice_via_ntlm, alice_via_basic,
		    alice ? SOUP_STATUS_OK :
		    SOUP_STATUS_UNAUTHORIZED);

	/* 3. Server still requires auth as Alice, but this URI
	 * doesn't exist, so Alice should get a 404, but others still
	 * get 401. Alice-via-NTLM is still authenticated, and so
	 * won't get prompts, and Alice-via-Basic knows at this point
	 * to send auth without it being requested, so also won't get
	 * prompts. But Bob/nobody will.
	 */
	do_message (session, base_uri, "/alice/404",
		    !alice, bob_via_ntlm,
		    !alice, alice_via_basic,
		    alice ? SOUP_STATUS_NOT_FOUND :
		    SOUP_STATUS_UNAUTHORIZED);

	/* 4. Should be exactly the same as #3, except the status code */
	do_message (session, base_uri, "/alice",
		    !alice, bob_via_ntlm,
		    !alice, alice_via_basic,
		    alice ? SOUP_STATUS_OK :
		    SOUP_STATUS_UNAUTHORIZED);

	/* 5. This path requires auth as Bob; Alice-via-NTLM will get
	 * an immediate 401 and not try to reauthenticate.
	 * Alice-via-Basic will get a 401 and then try to do Basic
	 * (and fail). Bob-via-NTLM will try to do NTLM right away and
	 * succeed.
	 */
	do_message (session, base_uri, "/bob",
		    !bob_via_ntlm, bob_via_ntlm,
		    !bob_via_ntlm, alice_via_basic,
		    bob ? SOUP_STATUS_OK :
		    SOUP_STATUS_UNAUTHORIZED);

	/* 6. Back to /alice. Somewhat the inverse of #5; Bob-via-NTLM
	 * will get an immediate 401 and not try again, Alice-via-NTLM
	 * will try to do NTLM right away and succeed. Alice-via-Basic
	 * still knows about this path, so will try Basic right away
	 * and succeed.
	 */
	do_message (session, base_uri, "/alice",
		    !alice_via_ntlm, alice_via_ntlm,
		    !alice_via_ntlm, alice_via_basic,
		    alice ? SOUP_STATUS_OK :
		    SOUP_STATUS_UNAUTHORIZED);

	/* 7. Server accepts Basic auth from either user, but not NTLM.
	 * Since Bob-via-NTLM is unauthenticated at this point, he'll try
	 * NTLM before realizing that the server doesn't support it.
	 */
	do_message (session, base_uri, "/basic",
		    FALSE, bob_via_ntlm,
		    TRUE, user != NULL,
		    user != NULL ? SOUP_STATUS_OK :
		    SOUP_STATUS_UNAUTHORIZED);

	/* 8. Server accepts Basic or NTLM from either user.
	 * Alice-via-NTLM is still authenticated at this point from #6,
	 * and Bob-via-NTLM is authenticated from #7, so neither
	 * of them will do anything.
	 */
	do_message (session, base_uri, "/either",
		    !use_ntlm, FALSE,
		    !use_ntlm, !use_ntlm && user != NULL,
		    user != NULL ? SOUP_STATUS_OK :
		    SOUP_STATUS_UNAUTHORIZED);

	soup_test_session_abort_unref (session);
}

static void
do_ntlm_tests (SoupURI *base_uri)
{
	debug_printf (1, "Round 1: Non-NTLM Connection, no auth\n");
	do_ntlm_round (base_uri, FALSE, NULL);
	debug_printf (1, "Round 2: NTLM Connection, user=alice\n");
	do_ntlm_round (base_uri, TRUE, "alice");
	debug_printf (1, "Round 3: NTLM Connection, user=bob\n");
	do_ntlm_round (base_uri, TRUE, "bob");
	debug_printf (1, "Round 4: Non-NTLM Connection, user=alice\n");
	do_ntlm_round (base_uri, FALSE, "alice");
}

int
main (int argc, char **argv)
{
	GMainLoop *loop;
	SoupServer *server;
	GHashTable *connections;
	SoupURI *uri;

	test_init (argc, argv, NULL);

	server = soup_test_server_new (FALSE);
	connections = g_hash_table_new (NULL, NULL);
	soup_server_add_handler (server, NULL,
				 server_callback, connections, NULL);

	loop = g_main_loop_new (NULL, TRUE);

	uri = soup_uri_new ("http://127.0.0.1/");
	soup_uri_set_port (uri, soup_server_get_port (server));
	do_ntlm_tests (uri);
	soup_uri_free (uri);

	g_main_loop_unref (loop);

	soup_test_server_quit_unref (server);
	test_cleanup ();
	g_hash_table_destroy (connections);

	return errors != 0;
}
