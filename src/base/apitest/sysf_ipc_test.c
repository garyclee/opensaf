/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2021 The OpenSAF Foundation
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
 * Author(s): Ericsson AB
 *
 */

#include "base/apitest/basetest.h"
#include "base/ncs_main_papi.h"
#include "base/sysf_ipc.h"
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>

int no_of_msgs_sent;
pthread_mutex_t lock;
SYSF_MBX mbox;

typedef struct message_ {
	struct message_ *next;
	NCS_IPC_PRIORITY prio;
	int seq_no;
} Message;

static bool mbox_clean(NCSCONTEXT arg, NCSCONTEXT msg)
{
	Message *curr;

	/* clean the entire mailbox */
	for (curr = (Message *)msg; curr;) {
		Message *temp = curr;
		curr = curr->next;

		free(temp);
	}
	return true;
}

static int send_msg(NCS_IPC_PRIORITY prio, int seq_no)
{
	Message *msg = NULL;

	msg = (Message *)calloc(1, sizeof(Message));
	msg->prio = prio;
	msg->seq_no = seq_no;
	return m_NCS_IPC_SEND(&mbox, msg, prio);
}

static bool recv_msg(NCS_IPC_PRIORITY prio, int seq_no)
{
	Message *msg;

	bool is_success = false;

	msg = (Message *)(ncs_ipc_non_blk_recv(&mbox));
	if (msg == NULL) {
		fprintf(stderr, "ncs_ipc_non_blk_recv failed\n");
	} else {
		is_success = msg->prio == prio && msg->seq_no == seq_no;
		free(msg);
	}
	return is_success;
}

static void *message_sender()
{
	srand(time(NULL));

	Message *msg = NULL;
	for (int i = 0; i < 60; ++i) {
		int prio = (random() % 3) + 1;
		msg = (Message *)calloc(1, sizeof(Message));
		msg->prio = (NCS_IPC_PRIORITY)prio;
		msg->seq_no = i;

		int rc = m_NCS_IPC_SEND(&mbox, msg, msg->prio);
		assert(rc == NCSCC_RC_SUCCESS && "m_NCS_IPC_SEND failed");

		pthread_mutex_lock(&lock);
		no_of_msgs_sent++;
		pthread_mutex_unlock(&lock);

		sched_yield();
	}
	return NULL;
}

static void *message_receiver()
{
	NCS_SEL_OBJ mbox_fd;
	struct pollfd fds;
	bool done = false;
	Message *msg;
	int no_of_msgs_received = 0;

	mbox_fd = ncs_ipc_get_sel_obj(&mbox);

	fds.fd = mbox_fd.rmv_obj;
	fds.events = POLLIN;

	while (!done) {
		int rc = poll(&fds, 1, -1);

		if (rc == -1) {
			if (errno == EINTR) {
				continue;
			}
			assert(rc == 0);
		}

		if (fds.revents & POLLIN) {
			while ((msg = (Message *)(ncs_ipc_non_blk_recv(
				    &mbox))) != NULL) {
				no_of_msgs_received++;

				if (msg->seq_no == 4711) {
					done = true;
				}
				free(msg);
			}
		}
	}

	rc_validate(no_of_msgs_received, no_of_msgs_sent);
	return NULL;
}

void test_send_receive_message(void)
{
	Message *msg;
	int rc = NCSCC_RC_SUCCESS;

	if ((rc = ncs_leap_startup()) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "ncs_leap_startup failed\n");
		test_validate(rc, NCSCC_RC_SUCCESS);
		goto leap_shutdown;
	}

	if ((rc = m_NCS_IPC_CREATE(&mbox)) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_CREATE failed\n");
		test_validate(rc, NCSCC_RC_SUCCESS);
		goto ipc_release;
	}

	if ((rc = m_NCS_IPC_ATTACH(&mbox)) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_ATTACH failed\n");
		test_validate(rc, NCSCC_RC_SUCCESS);
		goto ipc_detach;
	}

	NCS_IPC_PRIORITY prio[] = {
	    NCS_IPC_PRIORITY_LOW, NCS_IPC_PRIORITY_NORMAL,
	    NCS_IPC_PRIORITY_HIGH, NCS_IPC_PRIORITY_VERY_HIGH};
	// send messages
	for (size_t i = 0; i < 8; i++) {
		if ((rc = send_msg(prio[i / 2], i % 2 == 0 ? 1 : 2)) !=
		    NCSCC_RC_SUCCESS) {
			test_validate(rc, NCSCC_RC_SUCCESS);
			goto tear_down;
		}
	}

	// receive messages
	for (size_t i = 8; i > 0; i--) {
		if (!recv_msg(prio[(i - 1) / 2], i % 2 == 0 ? 1 : 2)) {
			test_validate(false, true);
			goto tear_down;
		}
	}

	msg = (Message *)(ncs_ipc_non_blk_recv(&mbox));
	bool is_null = msg == NULL ? true : false;
	test_validate(is_null, true);

tear_down:
ipc_detach:
	if (m_NCS_IPC_DETACH(&mbox, mbox_clean, 0) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_DETACH failed\n");
	}

ipc_release:
	if (m_NCS_IPC_RELEASE(&mbox, 0) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_RELEASE failed\n");
	}

leap_shutdown:
	ncs_leap_shutdown();
}

void test_thread_send_receive_message(void)
{
	pthread_t sndr_thread[5];
	pthread_t msg_receiver;
	int rc = NCSCC_RC_SUCCESS;

	if ((rc = ncs_leap_startup()) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "ncs_leap_startup failed\n");
		test_validate(rc, NCSCC_RC_SUCCESS);
		goto leap_shutdown;
	}

	if ((rc = m_NCS_IPC_CREATE(&mbox)) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_CREATE failed\n");
		test_validate(rc, NCSCC_RC_SUCCESS);
		goto ipc_release;
	}

	if ((rc = m_NCS_IPC_ATTACH(&mbox)) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_ATTACH failed\n");
		test_validate(rc, NCSCC_RC_SUCCESS);
		goto ipc_detach;
	}

	srand(time(NULL));
	assert(!pthread_create(&msg_receiver, NULL, message_receiver, NULL));

	for (int i = 0; i < 5; ++i) {
		assert(!pthread_create(&sndr_thread[i], NULL, message_sender,
				       NULL));
	}

	for (int i = 0; i < 5; ++i) {
		pthread_join(sndr_thread[i], NULL);
	}

	sched_yield();

	no_of_msgs_sent++;

	send_msg(NCS_IPC_PRIORITY_LOW, 4711);

	pthread_join(msg_receiver, NULL);

ipc_detach:
	if (m_NCS_IPC_DETACH(&mbox, mbox_clean, 0) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_DETACH failed\n");
	}

ipc_release:
	if (m_NCS_IPC_RELEASE(&mbox, 0) != NCSCC_RC_SUCCESS) {
		fprintf(stderr, "m_NCS_IPC_RELEASE failed\n");
	}

leap_shutdown:
	ncs_leap_shutdown();
}

__attribute__((constructor)) static void sysf_ipc_test_constructor(void)
{
	test_suite_add(1, "SysfIpcTest");
	test_case_add(1, test_send_receive_message,
		      "Tests send and receive messages");
	test_case_add(1, test_thread_send_receive_message,
		      "Tests threads sending and receiving messages");
}
