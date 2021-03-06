/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2012 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef PTL_IMPL_H
#define PTL_IMPL_H

#include <mpid_nem_impl.h>
#include <portals4.h>

#define PTL_MAX_EAGER (64*1024UL) /* 64K */

#define PTL_LARGE_THRESHOLD (64*1024UL) /* 64K */

extern ptl_handle_ni_t MPIDI_nem_ptl_ni;
extern ptl_pt_index_t  MPIDI_nem_ptl_pt;
extern ptl_pt_index_t  MPIDI_nem_ptl_get_pt; /* portal for gets by receiver */
extern ptl_pt_index_t  MPIDI_nem_ptl_control_pt; /* portal for MPICH control messages */
extern ptl_handle_eq_t MPIDI_nem_ptl_eq;

extern ptl_handle_md_t MPIDI_nem_ptl_global_md;

#define MPID_NEM_PTL_MAX_OVERFLOW_DATA 32 /* that's way more than we need */
typedef struct MPID_nem_ptl_pack_overflow
{
    MPI_Aint len;
    MPI_Aint offset;
    char buf[MPID_NEM_PTL_MAX_OVERFLOW_DATA];
} MPID_nem_ptl_pack_overflow_t;

typedef int (* event_handler_fn)(const ptl_event_t *e);

#define MPID_NEM_PTL_NUM_CHUNK_BUFFERS 2

typedef struct {
    struct MPID_nem_ptl_pack_overflow overflow[MPID_NEM_PTL_NUM_CHUNK_BUFFERS];
    int noncontig;
    int large;
    ptl_handle_md_t md;
    ptl_handle_me_t me;
    void *chunk_buffer[MPID_NEM_PTL_NUM_CHUNK_BUFFERS];
    MPIDI_msg_sz_t bytes_put;
    int found; /* used in probes with PtlMESearch() */
    event_handler_fn event_handler;
} MPID_nem_ptl_req_area;

/* macro for ptl private in req */
#define REQ_PTL(req) ((MPID_nem_ptl_req_area *)((req)->ch.netmod_area.padding))

#define MPID_nem_ptl_init_req(req_) do {                        \
        int i;                                                  \
        for (i = 0; i < MPID_NEM_PTL_NUM_CHUNK_BUFFERS; ++i) {  \
            REQ_PTL(req_)->overflow[i].len  = 0;                \
            REQ_PTL(req_)->chunk_buffer[i] = NULL;              \
        }                                                       \
        REQ_PTL(req_)->noncontig     = FALSE;                   \
        REQ_PTL(req_)->large         = FALSE;                   \
        REQ_PTL(req_)->md            = PTL_INVALID_HANDLE;      \
        REQ_PTL(req_)->me            = PTL_INVALID_HANDLE;      \
        REQ_PTL(req_)->event_handler = NULL;                    \
    } while (0)

#define MPID_nem_ptl_request_create_sreq(sreq_, errno_, comm_) do {                                             \
        (sreq_) = MPIU_Handle_obj_alloc(&MPID_Request_mem);                                                     \
        MPIU_ERR_CHKANDJUMP1((sreq_) == NULL, mpi_errno, MPI_ERR_OTHER, "**nomem", "**nomem %s", "request");    \
        MPIU_Object_set_ref((sreq_), 2);                                                                        \
        (sreq_)->kind               = MPID_REQUEST_SEND;                                                        \
        MPIR_Comm_add_ref(comm_);                                                                               \
        (sreq_)->comm               = comm_;                                                                    \
        (sreq_)->greq_fns           = NULL;                                                                     \
        MPID_cc_set(&(sreq_)->cc, 1);                                                                           \
        (sreq_)->cc_ptr             = &(sreq_)->cc;                                                             \
        (sreq_)->status.MPI_ERROR   = MPI_SUCCESS;                                                              \
        MPIR_STATUS_SET_CANCEL_BIT((sreq_)->status, FALSE);                                                           \
        (sreq_)->dev.cancel_pending = FALSE;                                                                    \
        (sreq_)->dev.state          = 0;                                                                        \
        (sreq_)->dev.datatype_ptr   = NULL;                                                                     \
        (sreq_)->dev.segment_ptr    = NULL;                                                                     \
                                                                                                                \
        MPID_nem_ptl_init_req(sreq_);                                                                           \
    } while (0)

typedef struct {
    ptl_process_t id;
    ptl_pt_index_t pt;
    ptl_pt_index_t ptg;
    ptl_pt_index_t ptc;
    int id_initialized; /* TRUE iff id and pt have been initialized */
    MPIDI_msg_sz_t num_queued_sends; /* number of reqs for this vc in sendq */
} MPID_nem_ptl_vc_area;

/* macro for ptl private in VC */
#define VC_PTL(vc) ((MPID_nem_ptl_vc_area *)vc->ch.netmod_area.padding)

/* Header bit fields
   bit   field
   ----  -------------------
   63    single/multiple
   62    large/small
   61    ssend
   0-60  length

   Note: This means we support no more than 2^24 processes.
*/
#define NPTL_SSEND             ((ptl_hdr_data_t)1<<61)
#define NPTL_LARGE             ((ptl_hdr_data_t)1<<62)
#define NPTL_MULTIPLE          ((ptl_hdr_data_t)1<<63)

#define NPTL_LENGTH_MASK     (((ptl_hdr_data_t)1<<61)-1)

#define NPTL_HEADER(flags_, length_) ((flags_) | (ptl_hdr_data_t)(length_))
#define NPTL_HEADER_GET_LENGTH(hdr_) ((hdr_) & NPTL_LENGTH_MASK)

#define NPTL_MAX_LENGTH NPTL_LENGTH_MASK

/* The comm_world rank of the sender is stored in the match_bits, but they are
   ignored when matching.
   bit   field
   ----  -------------------
   32-63  Tag
   16-31  Context id
    0-15  Rank
*/

#define NPTL_MATCH_TAG_OFFSET 32
#define NPTL_MATCH_CTX_OFFSET 16
#define NPTL_MATCH_RANK_MASK (((ptl_match_bits_t)(1) << 16) - 1)
#define NPTL_MATCH_CTX_MASK ((((ptl_match_bits_t)(1) << 16) - 1) << NPTL_MATCH_CTX_OFFSET)
#define NPTL_MATCH_TAG_MASK ((((ptl_match_bits_t)(1) << 32) - 1) << NPTL_MATCH_TAG_OFFSET)
#define NPTL_MATCH(tag_, ctx_, rank_) (((ptl_match_bits_t)(tag_) << NPTL_MATCH_TAG_OFFSET) |     \
                                       ((ptl_match_bits_t)(ctx_) << NPTL_MATCH_CTX_OFFSET) |     \
                                       ((ptl_match_bits_t)(rank_)))
#define NPTL_MATCH_IGNORE NPTL_MATCH_RANK_MASK
#define NPTL_MATCH_IGNORE_ANY_TAG (NPTL_MATCH_IGNORE | NPTL_MATCH_TAG_MASK)

#define NPTL_MATCH_GET_RANK(match_bits_) ((match_bits_) & NPTL_MATCH_RANK_MASK)
#define NPTL_MATCH_GET_CTX(match_bits_) (((match_bits_) & NPTL_MATCH_CTX_MASK) >> NPTL_MATCH_CTX_OFFSET)
#define NPTL_MATCH_GET_TAG(match_bits_) ((match_bits_) >> NPTL_MATCH_TAG_OFFSET)

int MPID_nem_ptl_nm_init(void);
int MPID_nem_ptl_nm_finalize(void);
int MPID_nem_ptl_nm_event_handler(const ptl_event_t *e);
int MPID_nem_ptl_sendq_complete_with_error(MPIDI_VC_t *vc, int req_errno);
int MPID_nem_ptl_SendNoncontig(MPIDI_VC_t *vc, MPID_Request *sreq, void *hdr, MPIDI_msg_sz_t hdr_sz);
int MPID_nem_ptl_iStartContigMsg(MPIDI_VC_t *vc, void *hdr, MPIDI_msg_sz_t hdr_sz, void *data, MPIDI_msg_sz_t data_sz,
                                   MPID_Request **sreq_ptr);
int MPID_nem_ptl_iSendContig(MPIDI_VC_t *vc, MPID_Request *sreq, void *hdr, MPIDI_msg_sz_t hdr_sz,
                               void *data, MPIDI_msg_sz_t data_sz);
int MPID_nem_ptl_poll_init(void);
int MPID_nem_ptl_poll_finalize(void);
int MPID_nem_ptl_poll(int is_blocking_poll);
int MPID_nem_ptl_vc_terminated(MPIDI_VC_t *vc);
int MPID_nem_ptl_get_id_from_bc(const char *business_card, ptl_process_t *id, ptl_pt_index_t *pt, ptl_pt_index_t *ptg,
                                ptl_pt_index_t *ptc);
void MPI_nem_ptl_pack_byte(MPID_Segment *segment, MPI_Aint first, MPI_Aint last, void *buf,
                           MPID_nem_ptl_pack_overflow_t *overflow);
int MPID_nem_ptl_unpack_byte(MPID_Segment *segment, MPI_Aint first, MPI_Aint last, void *buf,
                             MPID_nem_ptl_pack_overflow_t *overflow);

/* comm override functions */
int MPID_nem_ptl_recv_posted(struct MPIDI_VC *vc, struct MPID_Request *req);
/* isend is also used to implement send, rsend and irsend */
int MPID_nem_ptl_isend(struct MPIDI_VC *vc, const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
                       MPID_Comm *comm, int context_offset, struct MPID_Request **request);
/* issend is also used to implement ssend */
int MPID_nem_ptl_issend(struct MPIDI_VC *vc, const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
                        MPID_Comm *comm, int context_offset, struct MPID_Request **request);
int MPID_nem_ptl_cancel_send(struct MPIDI_VC *vc,  struct MPID_Request *sreq);
int MPID_nem_ptl_cancel_recv(struct MPIDI_VC *vc,  struct MPID_Request *rreq);
int MPID_nem_ptl_probe(struct MPIDI_VC *vc,  int source, int tag, MPID_Comm *comm, int context_offset, MPI_Status *status);
int MPID_nem_ptl_iprobe(struct MPIDI_VC *vc,  int source, int tag, MPID_Comm *comm, int context_offset, int *flag,
                        MPI_Status *status);
int MPID_nem_ptl_improbe(struct MPIDI_VC *vc,  int source, int tag, MPID_Comm *comm, int context_offset, int *flag,
                         MPID_Request **message, MPI_Status *status);
int MPID_nem_ptl_anysource_iprobe(int tag, MPID_Comm * comm, int context_offset, int *flag, MPI_Status * status);
int MPID_nem_ptl_anysource_improbe(int tag, MPID_Comm * comm, int context_offset, int *flag, MPID_Request **message,
                                   MPI_Status * status);
void MPID_nem_ptl_anysource_posted(MPID_Request *rreq);
int MPID_nem_ptl_anysource_matched(MPID_Request *rreq);
int MPID_nem_ptl_init_id(MPIDI_VC_t *vc);


/* debugging */
const char *MPID_nem_ptl_strerror(int ret);
const char *MPID_nem_ptl_strevent(const ptl_event_t *ev);
const char *MPID_nem_ptl_strnifail(ptl_ni_fail_t ni_fail);
const char *MPID_nem_ptl_strlist(ptl_list_t list);

#define DBG_MSG_PUT(md_, data_sz_, pg_rank_, match_, header_) do {                                                                          \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "PtlPut: md=%s data_sz=%lu pg_rank=%d", md_, data_sz_, pg_rank_));          \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "        tag=%#lx ctx=%#lx rank=%ld match=%#lx",                            \
                                                NPTL_MATCH_GET_TAG(match_), NPTL_MATCH_GET_CTX(match_), NPTL_MATCH_GET_RANK(match_), match_)); \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "        flags=%c%c%c data_sz=%ld header=%#lx",                             \
                                                header_ & NPTL_SSEND ? 'S':' ',                                                             \
                                                header_ & NPTL_LARGE ? 'L':' ',                                                             \
                                                header_ & NPTL_MULTIPLE ? 'M':' ',                                                          \
                                                NPTL_HEADER_GET_LENGTH(header_), header_));                                                 \
    } while(0)

#define DBG_MSG_GET(md_, data_sz_, pg_rank_, match_) do {                                                                                   \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "PtlGet: md=%s data_sz=%lu pg_rank=%d", md_, data_sz_, pg_rank_));          \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "        tag=%#lx ctx=%#lx rank=%ld match=%#lx",                            \
                                                NPTL_MATCH_GET_TAG(match_), NPTL_MATCH_GET_CTX(match_), NPTL_MATCH_GET_RANK(match_), match_)); \
    } while(0)

#define DBG_MSG_MEAPPEND(pt_, pg_rank_, me_, usr_ptr_) do {                                                                                 \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "PtlMEAppend: pt=%s pg_rank=%d me.start=%p me.length=%lu is_IOV=%d usr_ptr=%p", \
                                                pt_, pg_rank_, me_.start, me_.length, me_.options & PTL_IOVEC, usr_ptr_));                  \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "             tag=%#lx ctx=%#lx rank=%ld match=%#lx ignore=%#lx",           \
                                                NPTL_MATCH_GET_TAG(me_.match_bits), NPTL_MATCH_GET_CTX(me_.match_bits),                     \
                                                NPTL_MATCH_GET_RANK(me_.match_bits), me_.match_bits, me_.ignore_bits));                     \
    } while(0)
    
#define DBG_MSG_MESearch(pt_, pg_rank_, me_, usr_ptr_) do {                                                                             \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "PtlMESearch: pt=%s pg_rank=%d usr_ptr=%p", pt_, pg_rank_, usr_ptr_));  \
        MPIU_DBG_MSG_FMT(CH3_CHANNEL, VERBOSE, (MPIU_DBG_FDEST, "             tag=%#lx ctx=%#lx rank=%ld match=%#lx ignore=%#lx",       \
                                                NPTL_MATCH_GET_TAG(me_.match_bits), NPTL_MATCH_GET_CTX(me_.match_bits),                 \
                                                NPTL_MATCH_GET_RANK(me_.match_bits), me_.match_bits, me_.ignore_bits));                 \
    } while(0)
    


#endif /* PTL_IMPL_H */
