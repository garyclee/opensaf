/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.z
 *
 * Author(s): GoAhead Software
 *
 */

#include "mds_dt.h"
#include "mds_log.h"
#include "base/ncssysf_def.h"
#include "base/ncssysf_tsk.h"
#include "base/ncssysf_mem.h"
#include "mds_dt_tcp.h"
#include "mds_dt_tcp_disc.h"
#include "mds_dt_tcp_trans.h"
#include "mds_core.h"
#include "base/osaf_utility.h"

#include <sys/poll.h>
#include <poll.h>

#define MDS_PROT_TCP 0xA0
#define MDTM_FRAG_HDR_PLUS_LEN_2_TCP                                           \
	(2 + MDS_SEND_ADDRINFO_TCP + MDTM_FRAG_HDR_LEN_TCP)

/* Defines regarding to the Send and receive buff sizes */
#define MDS_HDR_LEN_TCP                                                        \
	25 /* Mds_prot-4bit, Mds_version-2bit , Msg prior-2bit, Hdr_len-16bit, \
	      Seq_no-32bit, Enc_dec_type-2bit, Msg_snd_type-6bit,              \
	      Pwe_id-16bit, Sndr_vdest_id-16bit, Sndr_svc_id-16bit,            \
	      Rcvr_vdest_id-16bit, Rcvr_svc_id-16bit, Exch_id-32bit,           \
	      App_Vers-16bit node_name_len-8bit, dynamic value                                             \
	      node_name-<_POSIX_HOST_NAME_MAX>bits */

#define MDTM_FRAG_HDR_LEN_TCP                                                  \
	8 /* Msg Seq_no-32bit, More Frag-1bit, Frag_num-15bit, Frag_size-16bit \
	   */
#define MDS_SEND_ADDRINFO_TCP                                                  \
	22 /* Identifier-32bit, Version-8bit, message type-8bit,               \
	      DestNodeid-32bit, DestProcessid-32bit, SrcNodeid-32bit,          \
	      SrcProcessid-32bit */

#define SUM_MDS_HDR_PLUS_MDTM_HDR_PLUS_LEN_TCP                                 \
	((2 + MDS_SEND_ADDRINFO_TCP + MDTM_FRAG_HDR_LEN_TCP + MDS_HDR_LEN_TCP))

#define MDTM_MAX_SEND_PKT_SIZE_TCP                                             \
	(MDS_DIRECT_BUF_MAXSIZE +                                              \
	 SUM_MDS_HDR_PLUS_MDTM_HDR_PLUS_LEN_TCP) /* Includes the 30 header     \
						    bytes(2+8+20) */

uint32_t mdtm_global_frag_num_tcp;
extern struct pollfd pfd[2];
extern pid_t mdtm_pid;

static uint32_t mds_mdtm_process_recvdata(uint32_t rcv_bytes, uint8_t *buffer);

/**
 * Function contains the logic to add the message to the queue based on counter
 *
 * @param send_buffer , bufferlen
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t mds_sock_send(uint8_t *tcp_buffer, uint32_t bufflen)
{
	ssize_t send_len = 0;
	send_len = send(tcp_cb->DBSRsock, tcp_buffer, bufflen, MSG_NOSIGNAL);

	/* message send failed */
	if ((send_len == -1) || (send_len != bufflen)) {
		LOG_ER("Failed to Send  Message bufflen :%d err :%s", bufflen,
		       strerror(errno));
		return NCSCC_RC_FAILURE;
	}
	return NCSCC_RC_SUCCESS;
}

/**
 * Function contains the logic to add the header to the sending message
 *
 * @param buffer req len
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
static uint32_t mdtm_add_mds_hdr_tcp(uint8_t *buffer, MDTM_SEND_REQ *req,
				     uint32_t len)
{
	uint8_t prot_ver = 0, enc_snd_type = 0;
	uint16_t zero_16 = 0;
	uint32_t zero_32 = 0;

	uint32_t xch_id = 0;
	uint16_t mds_hdr_len_tcp;
	int version = req->msg_arch_word & 0x7;
	if (version > 1) {
		mds_hdr_len_tcp =
		    (MDS_HDR_LEN_TCP + gl_mds_mcm_cb->node_name_len);
	} else {
		/* Old MDS_HDR_LEN = 24 */
		mds_hdr_len_tcp = (MDS_HDR_LEN_TCP - 1);
	}

	uint8_t *ptr;
	ptr = buffer;

	prot_ver |= MDS_PROT_TCP | MDS_VERSION | ((uint8_t)(req->pri - 1));
	enc_snd_type = (req->msg.encoding << 6);
	enc_snd_type |= (uint8_t)req->snd_type;

	/* Message length */
	ncs_encode_16bit(&ptr, (len - 2));

	/* Identifier and message type */
	ncs_encode_32bit(&ptr, (uint32_t)MDS_IDENTIFIRE);
	ncs_encode_8bit(&ptr, (uint8_t)MDS_SND_VERSION);
	ncs_encode_8bit(&ptr, (uint8_t)MDS_MDTM_DTM_MESSAGE_TYPE);

	/* TCP header */
	ncs_encode_32bit(&ptr,
			 (uint32_t)m_MDS_GET_NODE_ID_FROM_ADEST(req->adest));
	ncs_encode_32bit(&ptr,
			 (uint32_t)m_MDS_GET_PROCESS_ID_FROM_ADEST(req->adest));
	ncs_encode_32bit(&ptr, tcp_cb->node_id);
	ncs_encode_32bit(&ptr, mdtm_pid);

	/* Frag header */
	ncs_encode_32bit(&ptr, zero_32);
	ncs_encode_16bit(&ptr, zero_16);
	ncs_encode_16bit(&ptr, zero_16);
	/* MDS HDR */
	ncs_encode_8bit(&ptr, prot_ver);
	ncs_encode_16bit(
	    &ptr, (uint16_t)mds_hdr_len_tcp); /* Will be updated if any
						 additional options are being
						 added at the end */
	ncs_encode_32bit(&ptr, req->svc_seq_num);
	ncs_encode_8bit(&ptr, enc_snd_type);
	ncs_encode_16bit(&ptr, req->src_pwe_id);
	ncs_encode_16bit(&ptr, req->src_vdest_id);
	ncs_encode_16bit(&ptr, req->src_svc_id);
	ncs_encode_16bit(&ptr, req->dest_vdest_id);
	ncs_encode_16bit(&ptr, req->dest_svc_id);

	switch (req->snd_type) {
	case MDS_SENDTYPE_SNDRSP:
	case MDS_SENDTYPE_SNDRACK:
	case MDS_SENDTYPE_SNDACK:
	case MDS_SENDTYPE_RSP:
	case MDS_SENDTYPE_REDRSP:
	case MDS_SENDTYPE_REDRACK:
	case MDS_SENDTYPE_REDACK:
	case MDS_SENDTYPE_RRSP:
	case MDS_SENDTYPE_ACK:
	case MDS_SENDTYPE_RACK:
		xch_id = req->xch_id;
		break;

	default:
		xch_id = 0;
		break;
	}

	ncs_encode_32bit(&ptr, xch_id);
	ncs_encode_16bit(&ptr, req->msg_fmt_ver); /* New field */
	if (version > 1) {
		ncs_encode_8bit(&ptr,
				gl_mds_mcm_cb->node_name_len); /* New field 1 */
		ncs_encode_octets(
		    &ptr, (uint8_t *)gl_mds_mcm_cb->node_name,
		    gl_mds_mcm_cb->node_name_len); /* New field 2 */
	}
	return NCSCC_RC_SUCCESS;
}

/**
 * Function contains the logic to add the fragmented header to the sending
 * message
 *
 * @param buffer req len
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
static uint32_t mdtm_fill_frag_hdr_tcp(uint8_t *buffer, MDTM_SEND_REQ *req,
				       uint32_t len)
{
	uint16_t zero_16 = 0;
	uint32_t zero_32 = 0;

	uint8_t *ptr;
	ptr = buffer;

	/* Message length */
	ncs_encode_16bit(&ptr, (len - 2));

	/* Identifier and message type */
	ncs_encode_32bit(&ptr, (uint32_t)MDS_IDENTIFIRE);
	ncs_encode_8bit(&ptr, (uint8_t)MDS_SND_VERSION);
	ncs_encode_8bit(&ptr, (uint8_t)MDS_MDTM_DTM_MESSAGE_TYPE);

	/* TCP header */
	ncs_encode_32bit(&ptr,
			 (uint32_t)m_MDS_GET_NODE_ID_FROM_ADEST(req->adest));
	ncs_encode_32bit(&ptr,
			 (uint32_t)m_MDS_GET_PROCESS_ID_FROM_ADEST(req->adest));
	ncs_encode_32bit(&ptr, tcp_cb->node_id);
	ncs_encode_32bit(&ptr, mdtm_pid);

	/* Frag header */
	ncs_encode_32bit(&ptr, zero_32);
	ncs_encode_16bit(&ptr, zero_16);
	ncs_encode_16bit(&ptr, zero_16);

	return NCSCC_RC_SUCCESS;
}

/**
 * Function contains the logic to add the fragmented header to the sending
 * message
 *
 * @param buf_ptr len seq_num frag_byte
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
static uint32_t mdtm_add_frag_hdr_tcp(uint8_t *buf_ptr, uint16_t len,
				      uint32_t seq_num, uint16_t frag_byte)
{
	/* Add the FRAG HDR to the Buffer */
	uint8_t *data;
	data = buf_ptr;
	ncs_encode_32bit(&data, seq_num);
	ncs_encode_16bit(&data, frag_byte);
	ncs_encode_16bit(&data, len -
				    32); /* Here 32 means , 2-len , 22- iden,
					    ver, msg, dst node, pid, src node,
					    src process , 8 frag header */
	return NCSCC_RC_SUCCESS;
}

/**
 * Function conatins the logic to frag and send the messages
 *
 * @param req seq_num id
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
static uint32_t mdtm_frag_and_send_tcp(MDTM_SEND_REQ *req, uint32_t seq_num,
				       MDS_MDTM_PROCESSID_MSG id)
{
	USRBUF *usrbuf;
	uint32_t len = 0;
	uint16_t len_buf = 0;
	uint8_t *p8;
	uint16_t i = 1;
	uint16_t frag_val = 0;
	uint32_t sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp;
	uint32_t mdtm_max_send_pkt_size_tcp;
	int version = req->msg_arch_word & 0x7;
	if (version > 1) {

		sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp =
		    (SUM_MDS_HDR_PLUS_MDTM_HDR_PLUS_LEN_TCP +
		     gl_mds_mcm_cb->node_name_len);
		mdtm_max_send_pkt_size_tcp =
		    (MDTM_MAX_SEND_PKT_SIZE_TCP + gl_mds_mcm_cb->node_name_len);

	} else {
		sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp =
		    (SUM_MDS_HDR_PLUS_MDTM_HDR_PLUS_LEN_TCP - 1);
		mdtm_max_send_pkt_size_tcp = (MDTM_MAX_SEND_PKT_SIZE_TCP - 1);
	}

	switch (req->msg.encoding) {
	case MDS_ENC_TYPE_FULL:
		usrbuf = req->msg.data.fullenc_uba.start;
		break;

	case MDS_ENC_TYPE_FLAT:
		usrbuf = req->msg.data.flat_uba.start;
		break;

	default:
		return NCSCC_RC_SUCCESS;
	}

	len = m_MMGR_LINK_DATA_LEN(usrbuf); /* Getting total len */

	if (len > (32767 * MDS_DIRECT_BUF_MAXSIZE)) { /* We have 15 bits for
							 frag number so 2( pow
							 15) -1=32767 */
		m_MDS_LOG_CRITICAL(
		    "MDTM: App. is trying to send data more than MDTM Can fragment and send, Max size is =%d\n",
		    32767 * MDS_DIRECT_BUF_MAXSIZE);
		m_MMGR_FREE_BUFR_LIST(usrbuf);
		return NCSCC_RC_FAILURE;
	}

	while (len != 0) {
		if (len > MDS_DIRECT_BUF_MAXSIZE) {
			if (i == 1) {
				len_buf = mdtm_max_send_pkt_size_tcp;
				frag_val = MORE_FRAG_BIT | i;
			} else {
				if ((len + MDTM_FRAG_HDR_PLUS_LEN_2_TCP) >
				    mdtm_max_send_pkt_size_tcp) {
					len_buf = mdtm_max_send_pkt_size_tcp;
					frag_val = MORE_FRAG_BIT | i;
				} else {
					len_buf =
					    len + MDTM_FRAG_HDR_PLUS_LEN_2_TCP;
					frag_val = NO_FRAG_BIT | i;
				}
			}
		} else {
			len_buf = len + MDTM_FRAG_HDR_PLUS_LEN_2_TCP;
			frag_val = NO_FRAG_BIT | i;
		}
		{
			uint8_t *body = NULL;
			body = calloc(1, len_buf);
			if (i == 1) {
				p8 = (uint8_t *)m_MMGR_DATA_AT_START(
				    usrbuf,
				    (len_buf -
				     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp),
				    (char
					 *)(body +
					    sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp));

				if (p8 !=
				    (body +
				     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp))
					memcpy(
					    (body +
					     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp),
					    p8,
					    (len_buf -
					     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp));

				if (NCSCC_RC_SUCCESS !=
				    mdtm_add_mds_hdr_tcp(body, req, len_buf)) {
					m_MDS_LOG_ERR(
					    "MDTM: frg MDS hdr addition failed\n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}

				if (NCSCC_RC_SUCCESS !=
				    mdtm_add_frag_hdr_tcp((body + 24), len_buf,
							  seq_num, frag_val)) {
					m_MDS_LOG_ERR(
					    "MDTM: Frag hdr addition failed\n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}
				m_MDS_LOG_DBG(
				    "MDTM: Sending msg with Service Seqno=%d, Fragment Seqnum=%d, frag_num=%d,to Dest_id=<0x%08x:%u>",
				    req->svc_seq_num, seq_num, frag_val,
				    id.node_id, id.process_id);

				if (NCSCC_RC_SUCCESS !=
				    mds_sock_send(body, len_buf)) {
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}

				m_MMGR_REMOVE_FROM_START(
				    &usrbuf,
				    len_buf -
					sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp);
				free(body);
				len = len -
				      (len_buf -
				       sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp);
			} else {
				p8 = (uint8_t *)m_MMGR_DATA_AT_START(
				    usrbuf,
				    len_buf - MDTM_FRAG_HDR_PLUS_LEN_2_TCP,
				    (char *)(body +
					     MDTM_FRAG_HDR_PLUS_LEN_2_TCP));
				if (p8 != (body + MDTM_FRAG_HDR_PLUS_LEN_2_TCP))
					memcpy(
					    (body +
					     MDTM_FRAG_HDR_PLUS_LEN_2_TCP),
					    p8,
					    len_buf -
						MDTM_FRAG_HDR_PLUS_LEN_2_TCP);

				if (NCSCC_RC_SUCCESS !=
				    mdtm_fill_frag_hdr_tcp(body, req,
							   len_buf)) {
					m_MDS_LOG_ERR(
					    "MDTM: Frag hdr addition failed\n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}

				if (NCSCC_RC_SUCCESS !=
				    mdtm_add_frag_hdr_tcp((body + 24), len_buf,
							  seq_num, frag_val)) {
					m_MDS_LOG_ERR(
					    "MDTM: Frag hde addition failed\n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}
				m_MDS_LOG_DBG(
				    "MDTM: Sending message with Service Seqno=%d, Fragment Seqnum=%d, frag_num=%d, TO Dest_id=<0x%08x:%u>",
				    req->svc_seq_num, seq_num, frag_val,
				    id.node_id, id.process_id);

				if (NCSCC_RC_SUCCESS !=
				    mds_sock_send(body, len_buf)) {
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}

				m_MMGR_REMOVE_FROM_START(
				    &usrbuf,
				    (len_buf - MDTM_FRAG_HDR_PLUS_LEN_2_TCP));
				free(body);
				len = len -
				      (len_buf - MDTM_FRAG_HDR_PLUS_LEN_2_TCP);
				if (len == 0)
					break;
			}
		}
		i++;
		frag_val = 0;
	}
	return NCSCC_RC_SUCCESS;
}

/**
 * Function contains the logic to send the message
 *
 * @param req
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t mds_mdtm_send_tcp(MDTM_SEND_REQ *req)
{
	uint32_t status = 0;
	uint32_t sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp;
	int version = req->msg_arch_word & 0x7;
	if (version > 1) {
		sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp =
		    (SUM_MDS_HDR_PLUS_MDTM_HDR_PLUS_LEN_TCP +
		     gl_mds_mcm_cb->node_name_len);
	} else {
		sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp =
		    (SUM_MDS_HDR_PLUS_MDTM_HDR_PLUS_LEN_TCP - 1);
	}

	if (req->to == DESTINATION_SAME_PROCESS) {
		MDS_DATA_RECV recv;
		memset(&recv, 0, sizeof(recv));

		recv.dest_svc_hdl = (MDS_SVC_HDL)
		    m_MDS_GET_SVC_HDL_FROM_PWE_ID_VDEST_ID_AND_SVC_ID(
			req->dest_pwe_id, req->dest_vdest_id, req->dest_svc_id);
		recv.src_svc_id = req->src_svc_id;
		recv.src_pwe_id = req->src_pwe_id;
		recv.src_vdest = req->src_vdest_id;
		recv.exchange_id = req->xch_id;
		recv.src_adest = tcp_cb->adest;
		recv.snd_type = req->snd_type;
		recv.msg = req->msg;
		recv.pri = req->pri;
		recv.msg_fmt_ver = req->msg_fmt_ver;
		recv.src_svc_sub_part_ver = req->src_svc_sub_part_ver;
		strncpy((char *)recv.src_node_name,
			(char *)gl_mds_mcm_cb->node_name,
			gl_mds_mcm_cb->node_name_len);

		/* This is exclusively for the Bcast ENC and ENC_FLAT case */
		if (recv.msg.encoding == MDS_ENC_TYPE_FULL) {
			ncs_dec_init_space(&recv.msg.data.fullenc_uba,
					   recv.msg.data.fullenc_uba.start);
			recv.msg_arch_word = req->msg_arch_word;
		} else if (recv.msg.encoding == MDS_ENC_TYPE_FLAT) {
			/* This case will not arise, but just to be on safe side
			 */
			ncs_dec_init_space(&recv.msg.data.flat_uba,
					   recv.msg.data.flat_uba.start);
		} else {
			/* Do nothing for the DIrect buff and Copy case */
		}

		status = mds_mcm_ll_data_rcv(&recv);

		return status;

	} else {
		MDS_MDTM_PROCESSID_MSG id;
		USRBUF *usrbuf;

		uint32_t frag_seq_num = 0;

		id.node_id = m_MDS_GET_NODE_ID_FROM_ADEST(req->adest);
		id.process_id = m_MDS_GET_PROCESS_ID_FROM_ADEST(req->adest);

		frag_seq_num = ++mdtm_global_frag_num_tcp;

		/* Only for the ack and not for any other message */
		if (req->snd_type == MDS_SENDTYPE_ACK ||
		    req->snd_type == MDS_SENDTYPE_RACK) {
			uint8_t len = sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp;
			uint8_t buffer_ack[len];

			/* Add mds_hdr */
			if (NCSCC_RC_SUCCESS !=
			    mdtm_add_mds_hdr_tcp(buffer_ack, req, len)) {
				return NCSCC_RC_FAILURE;
			}

			/* Add frag_hdr */
			if (NCSCC_RC_SUCCESS !=
			    mdtm_add_frag_hdr_tcp(&buffer_ack[24], len,
						  frag_seq_num, 0)) {
				return NCSCC_RC_FAILURE;
			}

			m_MDS_LOG_DBG(
			    "MDTM: Sending message with Service Seqno=%d, TO Dest_id=<0x%08x:%u> ",
			    req->svc_seq_num, id.node_id, id.process_id);

			if (NCSCC_RC_SUCCESS !=
			    mds_sock_send(buffer_ack, len)) {
				return NCSCC_RC_FAILURE;
			}

			return NCSCC_RC_SUCCESS;
		}

		if (MDS_ENC_TYPE_FLAT == req->msg.encoding) {
			usrbuf = req->msg.data.flat_uba.start;
		} else if (MDS_ENC_TYPE_FULL == req->msg.encoding) {
			usrbuf = req->msg.data.fullenc_uba.start;
		} else {
			usrbuf = NULL; /* This is because, usrbuf is used only
					  in the above two cases and if it has
					  come here, it means, it is a direct
					  send. Direct send will not use the
					  USRBUF */
		}

		switch (req->msg.encoding) {
		case MDS_ENC_TYPE_CPY:
			/* will not present here */
			break;

		case MDS_ENC_TYPE_FLAT:
		case MDS_ENC_TYPE_FULL: {
			uint32_t len = 0;
			len = m_MMGR_LINK_DATA_LEN(
			    usrbuf); /* Getting total len */

			m_MDS_LOG_INFO(
			    "MDTM: User Sending Data lenght=%d From svc_id = %s(%d) to svc_id = %s(%d)\n",
			    len, get_svc_names(req->src_svc_id),
			    req->src_svc_id, get_svc_names(req->dest_svc_id),
			    req->dest_svc_id);

			if (len > MDS_DIRECT_BUF_MAXSIZE) {
				/* Packet needs to be fragmented and send */
				status = mdtm_frag_and_send_tcp(
				    req, frag_seq_num, id);
				return status;

			} else {
				uint8_t *p8;
				uint8_t *body = NULL;
				body = calloc(
				    1,
				    len +
					sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp);

				p8 = (uint8_t *)m_MMGR_DATA_AT_START(
				    usrbuf, len,
				    (char
					 *)(body +
					    sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp));

				if (p8 !=
				    (body +
				     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp))
					memcpy(
					    (body +
					     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp),
					    p8, len);

				if (NCSCC_RC_SUCCESS !=
				    mdtm_add_mds_hdr_tcp(
					body, req,
					(len +
					 sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp))) {
					m_MDS_LOG_ERR(
					    "MDTM: Unable to add the mds Hdr to the send msg\n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}

				if (NCSCC_RC_SUCCESS !=
				    mdtm_add_frag_hdr_tcp(
					(body + 24),
					(len +
					 sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp),
					frag_seq_num, 0)) {
					m_MDS_LOG_ERR(
					    "MDTM: Unable to add the frag Hdr to the send msg\n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}

				m_MDS_LOG_DBG(
				    "MDTM: Sending message with Service Seqno=%d, TO Dest_id=<0x%08x:%u> ",
				    req->svc_seq_num, id.node_id,
				    id.process_id);

				if (NCSCC_RC_SUCCESS !=
				    mds_sock_send(
					body,
					(len +
					 sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp))) {
					m_MDS_LOG_ERR(
					    "MDTM: Unable to send the msg \n");
					m_MMGR_FREE_BUFR_LIST(usrbuf);
					free(body);
					return NCSCC_RC_FAILURE;
				}
				m_MMGR_FREE_BUFR_LIST(usrbuf);
				free(body);
				return NCSCC_RC_SUCCESS;
			}
		} break;

		case MDS_ENC_TYPE_DIRECT_BUFF: {
			if (req->msg.data.buff_info.len >
			    (MDTM_MAX_DIRECT_BUFF_SIZE -
			     sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp)) {
				m_MDS_LOG_CRITICAL(
				    "MDTM: Passed pkt len is more than the single send direct buff\n");
				mds_free_direct_buff(
				    req->msg.data.buff_info.buff);
				return NCSCC_RC_FAILURE;
			}

			m_MDS_LOG_INFO(
			    "MDTM: User Sending Data len=%d From svc_id = %s(%d) to svc_id = %s(%d)\n",
			    req->msg.data.buff_info.len,
			    get_svc_names(req->src_svc_id), req->src_svc_id,
			    get_svc_names(req->dest_svc_id), req->dest_svc_id);

			uint8_t *body = NULL;
			body =
			    calloc(1, (req->msg.data.buff_info.len +
				       sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp));

			if (NCSCC_RC_SUCCESS !=
			    mdtm_add_mds_hdr_tcp(
				body, req,
				(req->msg.data.buff_info.len +
				 sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp))) {
				m_MDS_LOG_ERR(
				    "MDTM: Unable to add the mds Hdr to the send msg\n");
				free(body);
				mds_free_direct_buff(
				    req->msg.data.buff_info.buff);
				return NCSCC_RC_FAILURE;
			}
			if (NCSCC_RC_SUCCESS !=
			    mdtm_add_frag_hdr_tcp(
				(body + 24),
				req->msg.data.buff_info.len +
				    sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp,
				frag_seq_num, 0)) {
				m_MDS_LOG_ERR(
				    "MDTM: Unable to add the frag Hdr to the send msg\n");
				free(body);
				mds_free_direct_buff(
				    req->msg.data.buff_info.buff);
				return NCSCC_RC_FAILURE;
			}
			memcpy((body + sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp),
			       req->msg.data.buff_info.buff,
			       req->msg.data.buff_info.len);

			if (NCSCC_RC_SUCCESS !=
			    mds_sock_send(
				body,
				(req->msg.data.buff_info.len +
				 sum_mds_hdr_plus_mdtm_hdr_plus_len_tcp))) {
				m_MDS_LOG_ERR(
				    "MDTM: Unable to send the msg \n");
				free(body);
				mds_free_direct_buff(
				    req->msg.data.buff_info.buff);
				return NCSCC_RC_FAILURE;
			}

			/* If Direct Send is bcast it will be done at bcast
			 * function */
			if (req->snd_type == MDS_SENDTYPE_BCAST ||
			    req->snd_type == MDS_SENDTYPE_RBCAST) {
				/* Dont free Here */
			} else {
				mds_free_direct_buff(
				    req->msg.data.buff_info.buff);
			}
			free(body);
			return NCSCC_RC_SUCCESS;
		} break;

		default:
			m_MDS_LOG_ERR("MDTM: Encoding type not supported\n");
			return NCSCC_RC_SUCCESS;
			break;
		}
		return NCSCC_RC_SUCCESS;
	}
	return NCSCC_RC_FAILURE;
}

void mdtm_process_poll_recv_data_tcp(void)
{
	TRACE_ENTER();
	if (0 == tcp_cb->bytes_tb_read) {
		if (0 == tcp_cb->num_by_read_for_len_buff) {
			uint8_t *data;
			ssize_t recd_bytes = 0;

			/*******************************************************/
			/* Receive all incoming data on this socket */
			/*******************************************************/

			recd_bytes = recv(tcp_cb->DBSRsock, tcp_cb->len_buff, 2,
					  MSG_NOSIGNAL);
			if (0 == recd_bytes) {
				syslog(
				    LOG_ERR,
				    "MDTM:SOCKET recd_bytes :%zd, conn lost with dh server",
				    recd_bytes);
				close(tcp_cb->DBSRsock);
				return;
			} else if (2 == recd_bytes) {
				uint16_t local_len_buf = 0;

				data = tcp_cb->len_buff;
				local_len_buf = ncs_decode_16bit(&data);
				tcp_cb->buff_total_len = local_len_buf;
				tcp_cb->num_by_read_for_len_buff = 2;

				if (NULL == (tcp_cb->buffer = calloc(
						 1, (local_len_buf + 1)))) {
					/* Length + 2 is done to reuse the same
					   buffer while sending to other nodes
					 */
					syslog(
					    LOG_ERR,
					    "MDTM:SOCKET Memory allocation failed in dtm_intranode_processing");
					return;
				}
				recd_bytes =
				    recv(tcp_cb->DBSRsock, tcp_cb->buffer,
					 local_len_buf, 0);
				if (recd_bytes < 0) {
					return;
				} else if (0 == recd_bytes) {
					syslog(
					    LOG_ERR,
					    "MDTM:SOCKET = %zd, conn lost with dh server",
					    recd_bytes);
					close(tcp_cb->DBSRsock);
					return;
				} else if (local_len_buf > recd_bytes) {
					/* can happen only in two cases, system
					 * call interrupt or half data, */
					TRACE(
					    "MDTM:SOCKET less data recd, recd bytes = %zd, actual len = %d",
					    recd_bytes, local_len_buf);
					tcp_cb->bytes_tb_read =
					    tcp_cb->buff_total_len - recd_bytes;
					return;
				} else if (local_len_buf == recd_bytes) {
					/* Call the common rcv function */
					mds_mdtm_process_recvdata(
					    tcp_cb->buff_total_len,
					    tcp_cb->buffer);
					tcp_cb->bytes_tb_read = 0;
					tcp_cb->buff_total_len = 0;
					tcp_cb->num_by_read_for_len_buff = 0;
					free(tcp_cb->buffer);
					tcp_cb->buffer = NULL;
					return;
				} else {
					assert(0);
				}
			} else {
				/* we had recd some bytes */
				if (recd_bytes < 0) {
					/* This can happen due to system call
					 * interrupt */
					return;
				} else if (1 == recd_bytes) {
					/* We recd one byte of the length part
					 */
					tcp_cb->num_by_read_for_len_buff =
					    recd_bytes;
				} else {
					assert(0);
				}
			}
		} else if (1 == tcp_cb->num_by_read_for_len_buff) {
			ssize_t recd_bytes = 0;

			recd_bytes =
			    recv(tcp_cb->DBSRsock, &tcp_cb->len_buff[1], 1, 0);
			if (recd_bytes < 0) {
				/* This can happen due to system call interrupt
				 */
				return;
			} else if (1 == recd_bytes) {
				/* We recd one byte(remaining) of the length
				 * part */
				uint8_t *data = tcp_cb->len_buff;
				tcp_cb->num_by_read_for_len_buff = 2;
				tcp_cb->buff_total_len =
				    ncs_decode_16bit(&data);
				return;
			} else if (0 == recd_bytes) {
				syslog(
				    LOG_ERR,
				    "MDTM:SOCKET = %zd, conn lost with dh server",
				    recd_bytes);
				close(tcp_cb->DBSRsock);
				return;
			} else {
				assert(0); /* This should never occur */
			}
		} else if (2 == tcp_cb->num_by_read_for_len_buff) {
			ssize_t recd_bytes = 0;

			if (NULL == (tcp_cb->buffer = calloc(
					 1, (tcp_cb->buff_total_len + 1)))) {
				/* Length + 2 is done to reuse the same buffer
				   while sending to other nodes */
				syslog(
				    LOG_ERR,
				    "MDTM:SOCKET Memory allocation failed in dtm_internode_processing");
				return;
			}
			recd_bytes = recv(tcp_cb->DBSRsock, tcp_cb->buffer,
					  tcp_cb->buff_total_len, 0);
			if (recd_bytes < 0) {
				return;
			} else if (0 == recd_bytes) {
				syslog(
				    LOG_ERR,
				    "MDTM:SOCKET = %zd, conn lost with dh server",
				    recd_bytes);
				close(tcp_cb->DBSRsock);
				return;
			} else if (tcp_cb->buff_total_len > recd_bytes) {
				/* can happen only in two cases, system call
				 * interrupt or half data, */
				TRACE(
				    "MDTM:SOCKET less data recd, recd bytes = %zd, actual len = %d",
				    recd_bytes, tcp_cb->buff_total_len);
				tcp_cb->bytes_tb_read =
				    tcp_cb->buff_total_len - recd_bytes;
				return;
			} else if (tcp_cb->buff_total_len == recd_bytes) {
				/* Call the common rcv function */
				mds_mdtm_process_recvdata(
				    tcp_cb->buff_total_len, tcp_cb->buffer);
				tcp_cb->bytes_tb_read = 0;
				tcp_cb->buff_total_len = 0;
				tcp_cb->num_by_read_for_len_buff = 0;
				free(tcp_cb->buffer);
				tcp_cb->buffer = NULL;
				return;
			} else {
				assert(0);
			}
		} else {
			assert(0);
		}

	} else {
		/* Partial data already read */
		ssize_t recd_bytes = 0;

		recd_bytes = recv(tcp_cb->DBSRsock,
				  &tcp_cb->buffer[(tcp_cb->buff_total_len -
						   tcp_cb->bytes_tb_read)],
				  tcp_cb->bytes_tb_read, 0);
		if (recd_bytes < 0) {
			return;
		} else if (0 == recd_bytes) {
			syslog(LOG_ERR,
			       "MDTM:SOCKET = %zd, conn lost with dh server",
			       recd_bytes);
			close(tcp_cb->DBSRsock);
			return;
		} else if (tcp_cb->bytes_tb_read > recd_bytes) {
			/* can happen only in two cases, system call interrupt
			 * or half data, */
			TRACE(
			    "MDTM:SOCKET less data recd, recd bytes = %zd, actual len = %d",
			    recd_bytes, tcp_cb->bytes_tb_read);
			tcp_cb->bytes_tb_read =
			    tcp_cb->bytes_tb_read - recd_bytes;
			return;
		} else if (tcp_cb->bytes_tb_read == recd_bytes) {
			/* Call the common rcv function */
			mds_mdtm_process_recvdata(tcp_cb->buff_total_len,
						  tcp_cb->buffer);
			tcp_cb->bytes_tb_read = 0;
			tcp_cb->buff_total_len = 0;
			tcp_cb->num_by_read_for_len_buff = 0;
			free(tcp_cb->buffer);
			tcp_cb->buffer = NULL;
		} else {
			assert(0);
		}
	}
	TRACE_LEAVE();
	return;
}

/**
 * Main rcv function
 *
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t mdtm_process_recv_events_tcp(void)
{

	pfd[0].fd = tcp_cb->DBSRsock;
	pfd[1].fd = tcp_cb->tmr_fd;
	/*
	   STEP 1: Poll on the DBSRsock to get the events
	   if data is received process the received data
	   if discovery events are received , process the discovery events
	 */
	while (1) {
		int pollres;

		pfd[0].events = POLLIN;
		pfd[1].events = POLLIN;

		pfd[0].revents = pfd[1].revents = 0;

		pollres = poll(pfd, 2, MDTM_TCP_POLL_TIMEOUT);

		if (pollres > 0) { /* Check for EINTR and discard */
			osaf_mutex_lock_ordie(&gl_mds_library_mutex);

			/* Check for Socket Read operation */
			if (pfd[0].revents & POLLIN) {
				m_MDS_LOG_INFO(
				    "MDTM: Processing pollin events\n");
				mdtm_process_poll_recv_data_tcp();
			}

			if ((pfd[0].revents & (POLLNVAL | POLLERR)) != 0) {
				m_MDS_LOG_INFO(
				    "MDTM: poll failed, revents = %hd\n",
				    pfd[0].revents);
				pfd[0].fd = -1;
			}

			if (pfd[1].revents & POLLIN) {
				m_MDS_LOG_INFO(
				    "MDTM: Processing Timer mailbox events\n");

				/* Check if destroy-event has been processed */
				if (mds_tmr_mailbox_processing() ==
				    NCSCC_RC_DISABLED) {
					/* Quit ASAP. We have acknowledge that
					   MDS thread can be destroyed. Note
					   that the destroying thread is waiting
					   for the MDS_UNLOCK, before proceeding
					   with pthread-cancel and pthread-join
					 */
					osaf_mutex_unlock_ordie(
					    &gl_mds_library_mutex);

					/* N O T E : No further system calls
					   etc. This is to ensure that the
					   pthread-cancel & pthread-join, do not
					   get blocked. */
					return NCSCC_RC_SUCCESS; /* Thread quit
								  */
				}
			}
			osaf_mutex_unlock_ordie(&gl_mds_library_mutex);
		}
	}
}

/**
 * Rcv thread processing
 *
 * @param rcv_bytes, buff_in
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
static uint32_t mds_mdtm_process_recvdata(uint32_t rcv_bytes, uint8_t *buff_in)
{
	PW_ENV_ID pwe_id;
	MDS_SVC_ID svc_id;
	V_DEST_RL role;
	NCSMDS_SCOPE_TYPE scope;
	MDS_VDEST_ID vdest;
	NCS_VDEST_TYPE policy = 0;
	MDS_SVC_HDL svc_hdl;
	MDS_SVC_PVT_SUB_PART_VER svc_sub_part_ver;
	MDS_SVC_ARCHWORD_TYPE archword_type;
	MDTM_LIB_TYPES msg_type;
	SaUint16T mds_version;
	SaUint32T mds_indentifire;
	uint32_t server_type;
	uint32_t server_instance_lower;
	NODE_ID node_id;
	uint32_t process_id;
	uint64_t ref_val;
	MDS_DEST adest = 0;
	uint32_t src_nodeid, src_process_id;
	uint32_t buff_dump = 0;
	uint64_t tcp_id;
	uint8_t *buffer = buff_in;

	mds_indentifire = ncs_decode_32bit(&buffer);
	mds_version = ncs_decode_8bit(&buffer);
	if ((MDS_RCV_IDENTIFIRE != mds_indentifire) ||
	    (MDS_RCV_VERSION != mds_version)) {
		m_MDS_LOG_ERR(
		    "MDTM: Malformed pkt, version or identifer mismatch");
		return NCSCC_RC_FAILURE;
	}
	msg_type = ncs_decode_8bit(&buffer);

	switch (msg_type) {

	case MDTM_LIB_UP_TYPE:
	case MDTM_LIB_DOWN_TYPE: {

		server_type = ncs_decode_32bit(&buffer);
		server_instance_lower = ncs_decode_32bit(&buffer);
		(void)ncs_decode_32bit(&buffer);
		ref_val = ncs_decode_64bit(&buffer);
		node_id = ncs_decode_32bit(&buffer);
		process_id = ncs_decode_32bit(&buffer);

		svc_id = (uint16_t)(server_type & MDS_EVENT_MASK_FOR_SVCID);
		vdest = (MDS_VDEST_ID)server_instance_lower;
		archword_type = (MDS_SVC_ARCHWORD_TYPE)(
		    (server_instance_lower & MDS_ARCHWORD_MASK) >>
		    (LEN_4_BYTES - MDS_ARCHWORD_BITS_LEN));
		svc_sub_part_ver = (MDS_SVC_PVT_SUB_PART_VER)(
		    (server_instance_lower & MDS_VER_MASK) >>
		    (LEN_4_BYTES - MDS_VER_BITS_LEN - MDS_ARCHWORD_BITS_LEN));

		pwe_id = (server_type >> MDS_EVENT_SHIFT_FOR_PWE) &
			 MDS_EVENT_MASK_FOR_PWE;
		policy = (server_instance_lower & MDS_POLICY_MASK) >>
			 (LEN_4_BYTES - MDS_ARCHWORD_BITS_LEN -
			  MDS_VER_BITS_LEN - VDEST_POLICY_LEN);
		role = (server_instance_lower & MDS_ROLE_MASK) >>
		       (LEN_4_BYTES - MDS_ARCHWORD_BITS_LEN - MDS_VER_BITS_LEN -
			VDEST_POLICY_LEN - ACT_STBY_LEN);
		scope =
		    (server_instance_lower & MDS_SCOPE_MASK) >>
		    (LEN_4_BYTES - MDS_ARCHWORD_BITS_LEN - MDS_VER_BITS_LEN -
		     VDEST_POLICY_LEN - ACT_STBY_LEN - MDS_SCOPE_LEN);
		scope = scope + 1;

		m_MDS_LOG_INFO("MDTM: Received SVC event");

		if (NCSCC_RC_SUCCESS !=
		    mdtm_get_from_ref_tbl(ref_val, &svc_hdl)) {
			m_MDS_LOG_INFO(
			    "MDTM: No entry in ref tbl so dropping the recd event");
			return NCSCC_RC_FAILURE;
		}

		if (role == 0)
			role = V_DEST_RL_ACTIVE;
		else
			role = V_DEST_RL_STANDBY;

		if (policy == 0)
			policy = NCS_VDEST_TYPE_MxN;
		else
			policy = NCS_VDEST_TYPE_N_WAY_ROUND_ROBIN;

		adest = (uint64_t)node_id << 32;
		adest |= process_id;

		if (msg_type == MDTM_LIB_UP_TYPE) {
			if (NCSCC_RC_SUCCESS !=
			    mds_mcm_svc_up(pwe_id, svc_id, role, scope, vdest,
					   policy, adest, 0, svc_hdl, ref_val,
					   svc_sub_part_ver, archword_type)) {
				m_MDS_LOG_ERR(
				    "SVC-UP Event processsing failed for SVC id = %d, subscribed by SVC id = %d",
				    svc_id,
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl));
				return NCSCC_RC_FAILURE;
			}
		}

		if (msg_type == MDTM_LIB_DOWN_TYPE) {
			if (NCSCC_RC_SUCCESS !=
			    mds_mcm_svc_down(pwe_id, svc_id, role, scope, vdest,
					     policy, adest, 0, svc_hdl, ref_val,
					     svc_sub_part_ver, archword_type)) {
				m_MDS_LOG_ERR(
				    "MDTM: SVC-DOWN Event processsing failed for SVC id = %d, subscribed by SVC id = %d\n",
				    svc_id,
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl));
				return NCSCC_RC_FAILURE;
			}
		}
	}

	break;

	case MDTM_LIB_NODE_UP_TYPE:
	case MDTM_LIB_NODE_DOWN_TYPE:

	{
		uint16_t addr_family; /* Indicates V4 or V6 */
		char node_ip[INET6_ADDRSTRLEN];
		char node_name[_POSIX_HOST_NAME_MAX];
		node_id = ncs_decode_32bit(&buffer);
		ref_val = ncs_decode_64bit(&buffer);

		if (NCSCC_RC_SUCCESS !=
		    mdtm_get_from_ref_tbl(ref_val, &svc_hdl)) {
			m_MDS_LOG_INFO(
			    "MDTM: No entry in ref tbl so dropping the recd event");
			return NCSCC_RC_FAILURE;
		}

		if (msg_type == MDTM_LIB_NODE_UP_TYPE) {
			addr_family = ncs_decode_8bit(&buffer);
			memset(node_ip, 0, INET6_ADDRSTRLEN);
			memset(node_name, 0, _POSIX_HOST_NAME_MAX);
			memcpy(node_ip, (uint8_t *)buffer, INET6_ADDRSTRLEN);
			buffer = buffer + INET6_ADDRSTRLEN;
			memcpy(node_name, (uint8_t *)buffer,
			       _POSIX_HOST_NAME_MAX);
			m_MDS_LOG_INFO(
			    "MDTM: NODE_UP for node_name:%s, node_ip:%s, node_id:%u addr_family:%d msg_type:%d",
			    node_name, node_ip, node_id, addr_family, msg_type);
			mds_mcm_node_up(svc_hdl, node_id, node_ip, addr_family,
					node_name);
		} else if (msg_type == MDTM_LIB_NODE_DOWN_TYPE) {
			m_MDS_LOG_INFO(
			    "MDTM: NODE_DOWN  node_id:%u msg_type:%d", node_id,
			    msg_type);
			/* TBD if required this can be AF_INET or AF_INET6
			   for now to distinguished between TCP & TIPC
			   hardcoding to AF_INET in case of TIPC  we receive
			   this as AF_TIPC */
			addr_family = AF_INET; /* AF_INET or AF_INET6 */
			mds_mcm_node_down(svc_hdl, node_id, addr_family);
		}
	} break;

	case MDTM_LIB_MESSAGE_TYPE: {

		(void)ncs_decode_32bit(&buffer);
		(void)ncs_decode_32bit(&buffer);
		src_nodeid = ncs_decode_32bit(&buffer);
		src_process_id = ncs_decode_32bit(&buffer);

		tcp_id = ((uint64_t)src_nodeid) << 32;
		tcp_id |= src_process_id;

		mdtm_process_recv_data(&buff_in[22], rcv_bytes - 22, tcp_id,
				       &buff_dump);
	}

	break;

	default:
		syslog(
		    LOG_CRIT,
		    "MDTM: Message format is not correct something is wrong !!");
		assert(0);
	}
	return NCSCC_RC_SUCCESS;
}
