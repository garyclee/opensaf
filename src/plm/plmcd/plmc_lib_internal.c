/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2009 The OpenSAF Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 * Author(s): Hewlett-Packard Development Company, L.P.
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "base/logtrace.h"
#include "plm/plmcd/plmc_lib_internal.h"
#include "plmc_cmds.h"

extern int plmc_cmd_string_to_enum(char *cmd);
extern char *plmc_get_listening_ip_addr(char *plmc_config_file);

static thread_entry *HEAD = NULL;
static thread_entry *TAIL = NULL;

static pthread_mutex_t td_list_lock; /* List-wide lock */

static int epoll_fd;

FILE *plmc_lib_debug;

char *plmc_config_file = 0;
PLMC_config_data config;
pthread_t tcp_listener_id = 0, udp_listener_id = 0;
int tcp_listener_stop_fd = -1, udp_listener_stop_fd = -1;

cb_functions callbacks;

/* returns pointer to thread entry on success, NULL otherwise */
thread_entry *find_thread_entry(const char *ee_id)
{
	thread_entry *curr_entry, *retval = NULL;

	/*** Begin Critical Section ***/
	if (pthread_mutex_lock(&td_list_lock) != 0) {
		syslog(LOG_ERR, "error: plmc library could not lock thread "
				"list");
		return (NULL);
	}

	curr_entry = HEAD;
	while (curr_entry != NULL) {
		if (strcmp(curr_entry->thread_d.ee_id, ee_id) == 0) {
			retval = curr_entry;
			break;
		} else
			curr_entry = curr_entry->next;
	}

	if (pthread_mutex_unlock(&td_list_lock) != 0) {
		syslog(LOG_ERR, "error: plmc library could not unlock thread "
				"list");
		return (NULL);
	}
	/*** End Critical Section ***/

	return (retval);
}

static thread_entry * find_thread_entry_by_socket(int sockfd)
{
	thread_entry *entry = 0;

	TRACE_ENTER2("fd: %i", sockfd);

	do {
		thread_entry *curr_entry = 0;

		if (pthread_mutex_lock(&td_list_lock) != 0) {
			syslog(LOG_ERR,
				"error: plmc library could not lock thread "
				"list");
			break;
		}

		curr_entry = HEAD;
		while (curr_entry) {
		if (curr_entry->thread_d.socketfd == sockfd) {
			entry = curr_entry;
			break;
		} else
			curr_entry = curr_entry->next;
		}

		if (pthread_mutex_unlock(&td_list_lock) != 0) {
			syslog(LOG_ERR,
				"error: plmc library could not unlock thread "
				"list");
			break;
		}
	} while (false);

	TRACE_LEAVE2("ee_id: %s", entry ? entry->thread_d.ee_id : "none");
	return entry;
}

/* returns pointer to new thread entry on success, NULL otherwise */
static thread_entry *create_thread_entry(const char *ee_id, int sock)
{
	thread_entry *new_entry = NULL;

	TRACE_ENTER2("ee_id: %s fd: %i", ee_id, sock);

	new_entry = calloc(1, sizeof(thread_entry));
	if (new_entry == NULL) {
		syslog(LOG_ERR, "Out of memory");
		return (NULL);
	} else {
		/* Initialize the contents of the thread data portion */
		pthread_mutex_init(&(new_entry->thread_d.td_lock), NULL);

		/* We need a copy of the ee_id
		 *because we are running in our own thread
		 */
		strncpy(new_entry->thread_d.ee_id, ee_id,
			PLMC_EE_ID_MAX_LENGTH);
		new_entry->thread_d.ee_id[PLMC_EE_ID_MAX_LENGTH - 1] = '\0';
		new_entry->thread_d.socketfd = sock;
		new_entry->thread_d.command[0] = '\0';
		new_entry->thread_d.callback = NULL;
	}

	/*** Begin Critical Section ***/
	if (pthread_mutex_lock(&td_list_lock) != 0) {
		syslog(LOG_ERR, "plmc_lib: create thread_entry encountered an "
				"error locking the list");
		free(new_entry);
		return (NULL);
	}

	/* Add new entry to the end of the linked list */
	if (HEAD == NULL) {
		/* This new entry is the only entry in the list */
		new_entry->previous = NULL;
		new_entry->next = NULL;
		HEAD = new_entry;
		TAIL = new_entry;
	} else {
		/* We know there is already an entry in the list,
		 * so update TAIL
		 */
		TAIL->next = new_entry;
		new_entry->previous = TAIL;
		new_entry->next = NULL;
		TAIL = new_entry;
	}

	if (pthread_mutex_unlock(&td_list_lock) != 0) {
		syslog(LOG_ERR, "plmc_lib: create threa_entry encountered an "
				"error unlocking the list");
		free(new_entry);
		return (NULL);
	}
	/*** End Critical Section ***/

	TRACE_LEAVE();
	return (new_entry);
}

/* returns 0 on success, 1 on error */
static int delete_thread_entry(const char *ee_id)
{
	thread_entry *entry_to_delete;

	TRACE_ENTER2("ee_id: %s", ee_id);

	/* Make sure we have this entry in the list. */
	entry_to_delete = find_thread_entry(ee_id);
	if (entry_to_delete == NULL) {
		return (1);
	}

	/*** Begin Critical Section ***/
	if (pthread_mutex_lock(&td_list_lock) != 0) {
		syslog(LOG_ERR, "plmc_lib: delete_thread_entry encountered an "
				"error locking the list");
		return (1);
	}

	/* First, fix up the list - then free the memory for this entry */
	if (entry_to_delete->previous != NULL) {
		/* entry_to_delete is not the HEAD */
		entry_to_delete->previous->next = entry_to_delete->next;
		/* We need to update TAIL if entry_to_delete is also the
		 * last entry
		 */
		if (entry_to_delete == TAIL) {
			TAIL = entry_to_delete->previous;
		} else {
			entry_to_delete->next->previous =
			    entry_to_delete->previous;
		}
	} else {
		/* entry_to_delete is the HEAD */
		HEAD = entry_to_delete->next;
		if (entry_to_delete->next != NULL) {
			/* entry_to_delete is not the last entry */
			entry_to_delete->next->previous = NULL;
		} else {
			/* update TAIL */
			TAIL = NULL;
		}
	}

	if (pthread_mutex_unlock(&td_list_lock) != 0) {
		syslog(LOG_ERR, "plmc_lib: delete_thread_entry encountered an "
				"error unlocking the list");
		return (1);
	}
	/*** End Critical Section ***/

	pthread_mutex_destroy(&(entry_to_delete->thread_d.td_lock));
	free(entry_to_delete);

	TRACE_LEAVE();

	return (0);
}

/* delete all thread entries */
void delete_all_thread_entries(void)
{
	thread_entry *curr_entry, *next_entry = NULL;
	int sockfd;
	char result[PLMC_MAX_TCP_DATA_LEN];

	TRACE_ENTER();

	/*** Begin Critical Section ***/
	if (pthread_mutex_lock(&td_list_lock) != 0) {
		LOG_ER("plmc_lib: delete_thread_entry encountered an "
				"error locking the list");
		TRACE_LEAVE();
		return;
	}

	curr_entry = HEAD;
	while (curr_entry != NULL) {
		sockfd = curr_entry->thread_d.socketfd;
		next_entry = curr_entry->next;
		pthread_mutex_destroy(&(curr_entry->thread_d.td_lock));

		if (sockfd != 0 && recv(sockfd, result, PLMC_MAX_TCP_DATA_LEN,
					MSG_PEEK | MSG_DONTWAIT) != 0) {
			close(sockfd);
			TRACE("closed socket");
		}

		free(curr_entry);
		curr_entry = next_entry;
	}
	TAIL = NULL;
	HEAD = NULL;

	if (pthread_mutex_unlock(&td_list_lock) != 0) {
		LOG_ER("plmc_lib: delete_thread_entry encountered an "
				"error unlocking the list");
		TRACE_LEAVE();
		return;
	}
	/*** End Critical Section ***/
	TRACE_LEAVE();
}

static char *PLMC_action_msg[] = {"Action not defined", "Closing client socket",
				  "Exiting thread", "Destroying library",
				  "Ignoring error"};

static char *PLMC_error_msg[] = {"Misc error",
				 "A system call failed",
				 "Lost connection with client",
				 "Message from client invalid",
				 "Internal timeout while waiting for client",
				 "Can't locate a valid configuration file",
				 "Callback return non-zero",
				 "Internal plmc library error"};

/*********************************************************************
 *	This function takes care of shutting down and closing a socket
 *
 *	Parameters:
 *	socketfd - The socket file descriptor
 *
 *	Returns:
 *	This function return 0 if successful and a 1 if unsuccessful
 *********************************************************************/

int close_socket(int sockfd)
{
	if (close(sockfd) != 0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a call "
				"to close()");
		return 1;
	}
	return 0;
}

/*********************************************************************
 *	This function sends and error to PLMSv using the registered
 *	callback function (see plmc_initialize)
 *
 *	Parameters:
 *	error - An integer that represents the error (see plmc_lib.h)
 *	action - An integer that represents the action taken to deal
 *	with this error (see plmc_lib.h)
 *
 *	Returns:
 *	This function return 0 if successful and a 1 if unsuccessful
 *********************************************************************/

int send_error(int error, int action, char *ee_id, PLMC_cmd_idx cmd_enum)
{

	plmc_lib_error myerr;

	myerr.ecode = error;
	myerr.acode = action;
	strncpy(myerr.errormsg, PLMC_error_msg[error],
		PLMC_MAX_ERROR_MSG_LENGTH);
	myerr.errormsg[PLMC_MAX_ERROR_MSG_LENGTH - 1] = '\0';
	strncpy(myerr.action, PLMC_action_msg[action], PLMC_MAX_ACTION_LENGTH);
	myerr.action[PLMC_MAX_ACTION_LENGTH - 1] = '\0';
	if (ee_id != NULL) {
		strncpy(myerr.ee_id, ee_id, PLMC_EE_ID_MAX_LENGTH);
		myerr.ee_id[PLMC_EE_ID_MAX_LENGTH - 1] = '\0';
	} else {
		strcpy(myerr.ee_id, "");
	}
	myerr.cmd_enum = cmd_enum;
	if ((callbacks.err_cb(&myerr)) != 0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during the "
				"error callback call to PLMSv");
		return (1);
	}
	return (0);
}

/*********************************************************************
 *	This function takes care of extracting the essential from
 *	the incoming UDP Message
 *	This function will use this to populate a struct of the type
 *	udp_msg
 *
 *	Parameters:
 *	parsed - a pointer to the udp_msg struct (see plmc_lib.h)
 *	incoming - a pointer to the char that contains the message
 *
 *	Returns:
 *	This function return 0 if successful and a 1 if unsuccessful
 *********************************************************************/

int parse_udp(udp_msg *parsed, char *incoming)
{
	char *tmpstring;
	int i;
#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "parse_udp was called with: %s\n", incoming);
	fflush(plmc_lib_debug);
#endif
	tmpstring = incoming;
	tmpstring = strstr(tmpstring, " ");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 0)
		tmpstring = tmpstring + 1;
	else
		return 1;
	i = 0;
	while (tmpstring[i] != ':') {
		i++;
		if (i >= strlen(tmpstring) || i > PLMC_EE_ID_MAX_LENGTH) {
			return 1;
		}
	}
	strncpy(parsed->ee_id, tmpstring, i - 1);
	parsed->ee_id[i - 1] = '\0';

	tmpstring = strstr(tmpstring, ":");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 1)
		tmpstring = tmpstring + 2;
	else
		return 1;
	i = 0;
	while (tmpstring[i] != ':') {
		i++;
		if (i >= strlen(tmpstring) || i > PLMC_CMD_RESULT_MAX_LENGTH) {
			return 1;
		}
	}
	strncpy(parsed->msg, tmpstring, i - 1);
	parsed->msg[i - 1] = '\0';

	tmpstring = strstr(tmpstring, ":");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 1)
		tmpstring = tmpstring + 2;
	else
		return 1;
	strncpy(parsed->os_info, tmpstring, PLMC_CMD_RESULT_MAX_LENGTH);
	parsed->os_info[PLMC_CMD_RESULT_MAX_LENGTH - 1] = '\0';
	if (strcmp(parsed->msg, "EE_INSTANTIATING") == 0)
		parsed->msg_idx = EE_INSTANTIATING;
	else if (strcmp(parsed->msg, "EE_TERMINATED") == 0)
		parsed->msg_idx = EE_TERMINATED;
	else if (strcmp(parsed->msg, "EE_INSTANTIATED") == 0)
		parsed->msg_idx = EE_INSTANTIATED;
	else if (strcmp(parsed->msg, "EE_TERMINATING") == 0)
		parsed->msg_idx = EE_TERMINATING;

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "parse_udp ee_id is: %s\n", parsed->ee_id);
	fprintf(plmc_lib_debug, "parse_udp msg is: %s\n", parsed->msg);
	fprintf(plmc_lib_debug, "parse_udp os_info is: %s\n", parsed->os_info);
	fflush(plmc_lib_debug);
#endif
	return 0;
}

/*********************************************************************
 *	This function takes care of extracting the essential from
 *	the incoming TCP Message
 *	An example of an incomin message would be
 *	"plmc_d PLMC_SA_PLM_ADMIN_UNLOCK ack 0020f : 0020f". This function
 *	will use this to populate a struct of the type tcp_msg
 *
 *	Parameters:
 *	parsed - a pointer to the tcp_msg struct
 *	incoming - a pointer to the char that contains the message
 *
 *	Returns:
 *	This function return 0 if successful and a 1 if unsuccessful
 *********************************************************************/

static int parse_tcp(tcp_msg *parsed, const char *incoming)
{
	const char *tmpstring = incoming;
	int i;

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "parse_tcp_msg was called with: %s\n",
		incoming);
	fflush(plmc_lib_debug);
#endif
	tmpstring = strstr(tmpstring, " ");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 0)
		tmpstring = tmpstring + 1;
	else
		return 1;
	i = 0;
	while (tmpstring[i] != ' ') {
		i++;
		if (i >= strlen(tmpstring) || i > PLMC_CMD_NAME_MAX_LENGTH) {
			return 1;
		}
	}
	strncpy(parsed->cmd, tmpstring, i);
	parsed->cmd[i] = '\0';
	tmpstring = strstr(tmpstring, " ");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 0)
		tmpstring = tmpstring + 1;
	else
		return 1;
	tmpstring = strstr(tmpstring, " ");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 0)
		tmpstring = tmpstring + 1; /* Beginning of ee_id */
	else
		return 1;
	i = 0;
	while (tmpstring[i] != ':') {
		i++;
		if (i >= strlen(tmpstring) || i > PLMC_EE_ID_MAX_LENGTH) {
			return 1;
		}
	}
	strncpy(parsed->ee_id, tmpstring, i - 1);
	parsed->ee_id[i - 1] = '\0';
	tmpstring = strstr(tmpstring, ":");
	if (tmpstring == NULL)
		return 1;
	if (strlen(tmpstring) > 1)
		tmpstring = tmpstring + 2;
	else
		return 1;
	strncpy(parsed->result, tmpstring, PLMC_CMD_RESULT_MAX_LENGTH);
	parsed->result[PLMC_CMD_RESULT_MAX_LENGTH - 1] = '\0';

	parsed->cmd_enum = plmc_cmd_string_to_enum(parsed->cmd);

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "parse_tcp ee_id is: %s\n", parsed->ee_id);
	fprintf(plmc_lib_debug, "parse_tcp cmd is: %s\n", parsed->cmd);
	fprintf(plmc_lib_debug, "parse_tcp result is: %s\n", parsed->result);
	fflush(plmc_lib_debug);
#endif

	return 0;
}

static int handle_plmc_response(int fd, const char *recv_mesg)
{
	int result = 0;

	do {
		tcp_msg tcpmsg;
		thread_entry *tentry = 0;

#ifdef PLMC_LIB_DEBUG
		fprintf(plmc_lib_debug,
			"plmc_tcp_listener got response back: "
			"%s\n",
			recv_mesg);
		fflush(plmc_lib_debug);
#endif

		if (parse_tcp(&tcpmsg, recv_mesg) != 0) {
			syslog(LOG_ERR, "plmc_lib: Invalid TCP message "
					"during GET_EE_ID");
			send_error(PLMC_LIBERR_MSG_INVALID,
				   PLMC_LIBACT_CLOSE_SOCKET, NULL,
				   PLMC_GET_ID_CMD);
			result = -1;
			break;
		}

		tentry = find_thread_entry(tcpmsg.ee_id);

		if (!tentry) {
			tentry = create_thread_entry(tcpmsg.ee_id, fd);

			if (!tentry) {
				syslog(LOG_ERR,
					"unable to create thread entry for EE: "
					"%s",
				tcpmsg.ee_id);
				assert(false);
			}
		}

		/* Send callback */
		if (tentry->thread_d.callback) {
			if (tentry->thread_d.callback(&tcpmsg) != 0) {
				PLMC_cmd_idx cmd_enum =
					plmc_cmd_string_to_enum(
							tentry->thread_d.command);
				syslog(LOG_ERR,
					"plmc_lib: encountered an error during "
					"the result callback call to PLMSv");
				send_error(PLMC_LIBERR_ERR_CB,
						PLMC_LIBACT_IGNORING,
						tentry->thread_d.ee_id,
						cmd_enum);
			}
		}

		if (tcpmsg.cmd_enum == PLMC_GET_ID_CMD) {
			if ((callbacks.connect_cb(tcpmsg.ee_id,
							PLMC_CONN_CB_MSG)) != 0) {
				send_error(PLMC_LIBERR_ERR_CB,
						PLMC_LIBACT_IGNORING,
						tcpmsg.ee_id,
						PLMC_GET_ID_CMD);
				result = -1;
			}
		}
	} while (false);

	return result;
}

/**********************************************************************
 *	plmc_client_mgr - This is the thread that will be started
 *	by a library call when PLMSv wants to issue a command to a
 *	specific PLMc. This thread will hang around till PLMc returns
 *	a response to the command or times out. After it gets a message
 *	back from PLMc it will issue a callback to the thread that
 *	invoked the command
 *
 *	Parameters:
 *	argument => The incoming data struct for the thread. should
 *	be casted to thread_entry
 *
 *	Returns:
 *	This function returns a *void
 ***********************************************************************/

void plmc_client_mgr(thread_entry *tentry)
{
	char command[PLMC_CMD_NAME_MAX_LENGTH] = { 0 };
	char ee_id[PLMC_EE_ID_MAX_LENGTH] = { 0 };
	int sockfd, retval;
	thread_data *tdata = &(tentry->thread_d);

	do {
		sockfd = tentry->thread_d.socketfd;
		strncpy(ee_id, tdata->ee_id, PLMC_EE_ID_MAX_LENGTH);

		/* There is a new command, copy it locally */
		strncpy(command, tdata->command, PLMC_CMD_NAME_MAX_LENGTH);
		command[PLMC_CMD_NAME_MAX_LENGTH - 1] = '\0';

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug,
		"plmc_client_mgr got a new command %s for "
		"ee_id %s\n",
		command, ee_id);
	fflush(plmc_lib_debug);
#endif

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "plmc_client_mgr: Sending command to client\n");
	fflush(plmc_lib_debug);
#endif
		/* Send the command to PLMc */
		retval = send(sockfd, command, strlen(command), 0);
		if (retval <= 0) {
			syslog(LOG_ERR,
				"plmc_lib: Client Mgr encountered an error "
				"during a call to send(). Leaving cleanup for "
				"connection mgr");
			syslog(LOG_ERR, "plmc_lib: errno is %d", errno);
		}
	} while (false);

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "plmc_client_mgr: Done with command, "
				"exiting\n");
	fflush(plmc_lib_debug);
#endif
}

/**********************************************************************
 *	plmc_udp_listener - This is the thread that is responsible for
 *	dealing with incoming UDP message
 *
 *	Parameters:
 *	argument => The incoming data struct for the thread.
 *	This is not being used
 *
 *	Returns:
 *	This function returns a *void
 ***********************************************************************/

void *plmc_udp_listener(void *arguments)
{
	int sockfd, true_value = true;
	struct sockaddr_in servaddr, cliaddr;
	struct in_addr inp;
	char *match_ip;
	bool done = false;
	struct pollfd fds[2];

	TRACE_ENTER();

	/* Get the socket */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		syslog(LOG_ERR, "plmc_lib: encountered an error opening a "
				"socket");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		if (pthread_cancel(tcp_listener_id) != 0) {
			send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
				   PLMC_LIBACT_IGNORING, NULL, PLMC_NOOP_CMD);
			syslog(LOG_ERR, "plmc_lib: encountered an error "
					"canceling tcp_listener");
		}
		pthread_exit((void *)NULL);
	}

	if (setsockopt(sockfd,
			SOL_SOCKET,
			SO_REUSEADDR,
			&true_value,
			sizeof(int)) < 0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a "
				"call to setsockopt()");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES, PLMC_LIBACT_IGNORING,
			   NULL, PLMC_NOOP_CMD);
	}

	if (setsockopt(sockfd,
			SOL_SOCKET,
			SO_REUSEPORT,
			&true_value,
			sizeof(int)) < 0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a "
				"call to setsockopt() REUSEPORT");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES, PLMC_LIBACT_IGNORING,
			   NULL, PLMC_NOOP_CMD);
	}

	match_ip = plmc_get_listening_ip_addr(plmc_config_file);
	if (strlen(match_ip) == 0) {
		syslog(LOG_ERR, "plmc_udp_listener cannot match available "
				"network insterfaces with IPs of controllers "
				"specified in the plmcd.conf file");
		send_error(PLMC_LIBERR_NO_CONF, PLMC_LIBACT_DESTROY_LIBRARY,
			   NULL, PLMC_NOOP_CMD);
		pthread_exit((void *)NULL);
	}
	inet_pton(AF_INET, match_ip, &inp);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inp.s_addr;
	servaddr.sin_port = htons(atoi(config.udp_broadcast_port));
	if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ==
	    -1) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a "
				"call to bind()");
		syslog(LOG_ERR, "plmc_lib: errnor is %d", errno);
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		if (pthread_cancel(tcp_listener_id) != 0) {
			send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
				   PLMC_LIBACT_IGNORING, NULL, PLMC_NOOP_CMD);
			syslog(LOG_ERR, "plmc_lib: encountered an error "
					"canceling tcp_listener");
		}
		pthread_exit((void *)NULL);
	}

	fds[0].fd      = sockfd;
	fds[0].events  = POLLIN;
	fds[0].revents = 0;

	fds[1].fd      = udp_listener_stop_fd;
	fds[1].events  = POLLIN;
	fds[1].revents = 0;

	while (!done) {
		int res = poll(fds, sizeof(fds) / sizeof(struct pollfd), -1);

		if (res < 0) {
			if (errno == EINTR)
				continue;
			else
				LOG_ER("poll failed: %i", errno);
		}

		if (fds[0].revents & POLLIN) {
			char mesg[PLMC_MAX_UDP_DATA_LEN];
			udp_msg udpmsg;
			socklen_t addr_length = sizeof(cliaddr);
			int msg_length = recvfrom(sockfd,
					mesg,
					PLMC_MAX_UDP_DATA_LEN,
					0,
				       	(struct sockaddr *)&cliaddr,
					&addr_length);
			mesg[msg_length] = '\0';
#ifdef PLMC_LIB_DEBUG
			fprintf(plmc_lib_debug,
				"plmc_udp_listener got a new message: "
				"%s\n",
				mesg);
			fflush(plmc_lib_debug);
#endif

			/* Get the struct setup */
			if (parse_udp(&udpmsg, mesg) != 0) {
				syslog(LOG_ERR,
					"plmc_lib: UDP message invalid %s",
					mesg);
				send_error(PLMC_LIBERR_MSG_INVALID,
						PLMC_LIBACT_IGNORING,
						NULL,
						PLMC_NOOP_CMD);
				continue;
			}
			if ((callbacks.udp_cb(&udpmsg)) != 0) {
				syslog(LOG_ERR,
					"plmc_lib: encountered an error during "
					"the UDP callback call to PLMSv");
				send_error(PLMC_LIBERR_ERR_CB,
						PLMC_LIBACT_IGNORING,
						udpmsg.ee_id,
						PLMC_NOOP_CMD);
				continue;
			}
		}

		if (fds[1].revents & POLLIN) {
			TRACE("udp_listener thread received term request");

			if (close(sockfd) < 0) {
				LOG_ER("closing sockfd in udp_listener thread "
						"failed: %i", errno);
			}

			if (close(udp_listener_stop_fd) < 0) {
				LOG_ER("closing stop fd in udp_listener thread "
						"failed: %i", errno);
			}

			done = true;
		}
	}

	TRACE_LEAVE();
	return 0;
}

static void accept_new_connection(int listen_fd)
{
	do {
		struct epoll_event ev = { 0 };
		struct sockaddr_in cliaddr;
		unsigned int sin_size = sizeof(struct sockaddr_in);
		int optlen, retval;

		int conn_fd = accept4(listen_fd,
			(struct sockaddr *)&cliaddr,
			&sin_size,
			SOCK_NONBLOCK);

		if (conn_fd < 0) {
			syslog(LOG_ERR, "plmc_lib: encoutered a problem in "
					"the accept() call");
			send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
				   PLMC_LIBACT_IGNORING, NULL, PLMC_NOOP_CMD);
			break;
		}

		if (config.so_keepalive) {
                        /* Set SO_KEEPALIVE */
                        optlen = sizeof(config.so_keepalive);
                        if (setsockopt(conn_fd, SOL_SOCKET, SO_KEEPALIVE,
                                       &config.so_keepalive, optlen) < 0) {
				syslog(LOG_ERR,
					"setsockopt SO_KEEPALIVE failed error:%s",
					strerror(errno));
				break;
                        }

			/* Set TCP_KEEPIDLE */
			optlen = sizeof(config.tcp_keepidle_time);
                        if (setsockopt(conn_fd, SOL_TCP, TCP_KEEPIDLE,
                                       &config.tcp_keepidle_time, optlen) < 0) {
                                syslog(LOG_ERR,
                                       "setsockopt TCP_KEEPIDLE "
                                       "failed "
                                       "error:%s",
                                       strerror(errno));
				break;
                        }

                        /* Set TCP_KEEPINTVL */
                        optlen = sizeof(config.tcp_keepalive_intvl);
                        if (setsockopt(conn_fd, SOL_TCP, TCP_KEEPINTVL,
                                       &config.tcp_keepalive_intvl, optlen) < 0) {
                                syslog(LOG_ERR,
                                       "setsockopt TCP_KEEPINTVL failed"
                                       " error:%s",
                                       strerror(errno));
				break;
                        }

                        /* Set TCP_KEEPCNT */
                        optlen = sizeof(config.tcp_keepalive_probes);
                        if (setsockopt(conn_fd, SOL_TCP, TCP_KEEPCNT,
                                       &config.tcp_keepalive_probes, optlen) < 0) {
                                syslog(LOG_ERR,
                                       "setsockopt TCP_KEEPCNT  failed"
                                       " error:%s",
                                       strerror(errno));
				break;
                        }

			optlen = sizeof(config.tcp_user_timeout);
			if (setsockopt(conn_fd,
						IPPROTO_TCP,
						TCP_USER_TIMEOUT,
						&config.tcp_user_timeout,
						optlen) < 0) {
				syslog(LOG_ERR,
						"setting tcp user timeout "
						"failed: %s", strerror(errno));
				break;
			}
		}

		/* Send a request for the ee_id to the client */
		retval = send(conn_fd, PLMC_cmd_name[PLMC_GET_ID_CMD],
			      strlen(PLMC_cmd_name[PLMC_GET_ID_CMD]), 0);
		if (retval < 0) {
			syslog(LOG_ERR, "plmc_lib: encountered an error "
					"during a call to send(): %d", errno);
			send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
				   PLMC_LIBACT_CLOSE_SOCKET, NULL,
				   PLMC_GET_ID_CMD);

			if (close_socket(conn_fd) != 0) {
				syslog(LOG_ERR,
				       "plmc_lib: encountered an "
				       "error during a call to close_socket");
				send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
					   PLMC_LIBACT_CLOSE_SOCKET, NULL,
					   PLMC_GET_ID_CMD);
			}

			break;
		}

		/* add the socket descriptor to the polling set */
		ev.events = EPOLLIN | EPOLLRDHUP;
		ev.data.fd = conn_fd;

		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) < 0) {
			syslog(LOG_ERR, "epoll_ctl failed: %d", errno);
			assert(false);
		}
	} while (false);
}

/**********************************************************************
 *	plmc_tcp_listener - This is the thread that is responsible for
 *	dealing with new connections over the TCP channel. For
 *	Each new connection it will get the ee_id from the connecting
 *	client and store the information in the global list.
 *
 *	Parameters:
 *	argument => The incoming data struct for the thread. should
 *	be casted to cb_functions.
 *
 *	Returns:
 *	This function returns a *void
 ***********************************************************************/

void *plmc_tcp_listener(void *arguments)
{
	int sockfd, true_value = 1;
	struct sockaddr_in servaddr;
	struct in_addr inp;
	struct epoll_event ev = { 0 };
	char *match_ip;
	bool done = false;

	pthread_attr_t udp_attr;

	TRACE_ENTER();

	/* Get the socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		syslog(LOG_ERR, "plmc_lib: encountered an error opening "
				"a socket");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		pthread_exit((void *)NULL);
	}

	/* Set some socket options, not sure we need this */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true_value, sizeof(int)) <
	    0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a "
				"call to setsockopt()");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES, PLMC_LIBACT_IGNORING,
			   NULL, PLMC_NOOP_CMD);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &true_value, sizeof(int)) <
	    0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a "
				"call to setsockopt() REUSEPORT");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES, PLMC_LIBACT_IGNORING,
			   NULL, PLMC_NOOP_CMD);
	}

	/* Set up the IP stuff */
	match_ip = plmc_get_listening_ip_addr(plmc_config_file);
	if (strlen(match_ip) == 0) {
		syslog(LOG_ERR, "plmc_tcp_listener cannot match available "
				"network insterfaces with IPs of controllers "
				"specified in the plmcd.conf file");
		send_error(PLMC_LIBERR_NO_CONF, PLMC_LIBACT_DESTROY_LIBRARY,
			   NULL, PLMC_NOOP_CMD);
		pthread_exit((void *)NULL);
	}
	inet_pton(AF_INET, match_ip, &inp);

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(config.tcp_plms_listening_port));
	servaddr.sin_addr.s_addr = inp.s_addr;
	bzero(&(servaddr.sin_zero), 8);

	/* Socket Address to server */
	if (bind(sockfd, (struct sockaddr *)&servaddr,
		 sizeof(struct sockaddr)) == -1) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a "
				"call to bind()");
		syslog(LOG_ERR, "plmc_lib: errnor is %d", errno);
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		close(sockfd);
		pthread_exit((void *)NULL);
	}

	/* Spec max # queued up plmc connections */
	if (listen(sockfd, SOMAXCONN) < 0) {
		syslog(LOG_ERR, "plmc_lib: encountered an error during a call "
				"to listen()");
		syslog(LOG_ERR, "plmc_lib: errnor is %d", errno);
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		close(sockfd);
		pthread_exit((void *)NULL);
	}

#ifdef PLMC_LIB_DEBUG
	fprintf(plmc_lib_debug, "plmc_tcp_listener starting the UDP thread\n");
	fflush(plmc_lib_debug);
#endif
	udp_listener_stop_fd = eventfd(0, EFD_NONBLOCK);

	if (udp_listener_stop_fd < 0) {
		LOG_ER("eventfd failed: %i for udp_listener thread", errno);
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		close(sockfd);
		pthread_exit(0);
	}

	/* Create the UDP thread */
	pthread_attr_init(&udp_attr);

	if (pthread_create(&(udp_listener_id), &udp_attr, plmc_udp_listener,
			   NULL) != 0) {
		syslog(LOG_ERR, "plmc_lib: Could not create the UDP thread");
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		close(sockfd);
		close(udp_listener_stop_fd);
		pthread_attr_destroy(&udp_attr);
		pthread_exit((void *)NULL);
	}

	pthread_attr_destroy(&udp_attr);

	epoll_fd = epoll_create1(EPOLL_CLOEXEC);

	if (epoll_fd < 0) {
		syslog(LOG_ERR, "epoll_create1 failed: %d", errno);
		send_error(PLMC_LIBERR_SYSTEM_RESOURCES,
			   PLMC_LIBACT_DESTROY_LIBRARY, NULL, PLMC_NOOP_CMD);
		close(sockfd);
		close(udp_listener_stop_fd);
		pthread_exit(0);
	}

	/* add the listen fd */
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
		syslog(LOG_ERR, "epoll_ctl failed: %d", errno);
		assert(false);
	}

	/* add the term fd */
	ev.events = EPOLLIN;
	ev.data.fd = tcp_listener_stop_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_listener_stop_fd, &ev) < 0) {
		syslog(LOG_ERR, "epoll_ctl failed: %d", errno);
		assert(false);
	}

	while (!done) {
		struct epoll_event events[128];
		int poll_ret;
		do {
			poll_ret = epoll_wait(epoll_fd,
						&events[0],
						sizeof(events) /
							sizeof(events[0]),
						-1);
		} while (poll_ret < 0 && errno == EINTR);

		/* Check to see if the poll call failed. */
		if (poll_ret < 0) {
			syslog(LOG_ERR, "epoll_wait() failed: %d", errno);
			break;
		}

		for (int i = 0; i < poll_ret; ++i) {
			int close_conn = 0;

			if (events[i].data.fd == sockfd) {
				accept_new_connection(sockfd);
			} else if (events[i].data.fd == tcp_listener_stop_fd) {
				TRACE("tcp_listener thread received term "
						"request");
				delete_all_thread_entries();
				if (close(sockfd) < 0) {
					LOG_ER("error closing "
						"tcp_listener_thread socket: "
						"%i", errno);
				}

				if (close(epoll_fd) < 0) {
					LOG_ER("error closing epoll socket: %i",
							errno);
				}

				if (close(tcp_listener_stop_fd) < 0) {
					LOG_ER("error closing "
						"tcp_listener_stop_fd socket: "
						"%i",
						errno);
				}

				done = true;
				break;
			} else {
				if (events[i].events & EPOLLIN) {
					char recv_mesg[PLMC_MAX_TCP_DATA_LEN] =
						{ 0 };

					int bytes_received =
						recv(events[i].data.fd,
							recv_mesg,
							PLMC_MAX_TCP_DATA_LEN,
							0);

					if (bytes_received < 0) {
						TRACE("recv failed: %d for fd: "
							"%i",
							errno,
							events[i].data.fd);
						send_error(PLMC_LIBERR_TIMEOUT,
								PLMC_LIBACT_CLOSE_SOCKET,
								NULL,
								PLMC_GET_ID_CMD);
						close_conn = 1;
					} else if (bytes_received == 0) {
						TRACE("client closed "
							"connection fd: %i",
							events[i].data.fd);
						close_conn = 1;
					}

					if (!close_conn) {
						handle_plmc_response(
							events[i].data.fd,
							recv_mesg);
					}
				}

				if (events[i].events & (EPOLLHUP |
							EPOLLRDHUP |
							EPOLLERR)) {
					TRACE("HUP for fd: %i",
						events[i].data.fd);
					close_conn = 1;
				}

				if (close_conn) {
					thread_entry *entry =
						find_thread_entry_by_socket(
							events[i].data.fd);

					if (!entry) {
						/* if client never responded to
						 * GET_ID we could get here */
						TRACE("unable to find thread "
							"entry for socket: %d",
							events[i].data.fd);
					}

					close_conn = 0;

					if (close_socket(events[i].data.fd) != 0) {
						syslog(LOG_ERR,
							"closing socket failed: "
							"%d",
							errno);
					}

					if (entry) {
						if (callbacks.connect_cb(
							entry->thread_d.ee_id,
							PLMC_DISCONN_CB_MSG) !=
								0) {
							send_error(
								PLMC_LIBERR_SYSTEM_RESOURCES,
								PLMC_LIBACT_CLOSE_SOCKET,
								entry->thread_d.ee_id,
								PLMC_GET_ID_CMD);
						}

						delete_thread_entry(
							entry->thread_d.ee_id);
					}
				}
			}
		}
	}

	TRACE_LEAVE();
	return 0;
}
