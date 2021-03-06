/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/*******************************************************************************
 *
 *  Filename:      btif_profile_queue.c
 *
 *  Description:   Bluetooth remote device connection queuing implementation.
 *
 ******************************************************************************/

#include <hardware/bluetooth.h>

#define LOG_TAG "BTIF_QUEUE"
#include "btif_common.h"
#include "btif_profile_queue.h"
#include "gki.h"

/*******************************************************************************
**  Local type definitions
*******************************************************************************/

typedef struct connect_node_tag
{
    bt_bdaddr_t bda;
    uint16_t uuid;
    uint16_t busy;
    void *p_cb;
    struct connect_node_tag *p_next;
} __attribute__((packed))connect_node_t;


/*******************************************************************************
**  Static variables
*******************************************************************************/

static connect_node_t *connect_queue;


/*******************************************************************************
**  Queue helper functions
*******************************************************************************/

static void queue_int_add(connect_node_t *p_param)
{
    connect_node_t *p_list = connect_queue;
    connect_node_t *p_node = GKI_getbuf(sizeof(connect_node_t));
    ASSERTC(p_node != NULL, "Failed to allocate new list node", 0);

    memcpy(p_node, p_param, sizeof(connect_node_t));

    if (connect_queue == NULL)
    {
        connect_queue = p_node;
        return;
    }

    while (p_list->p_next)
        p_list = p_list->p_next;
    p_list->p_next = p_node;
}

static void queue_int_advance()
{
    connect_node_t *p_head = connect_queue;
    if (connect_queue == NULL)
        return;

    connect_queue = connect_queue->p_next;
    GKI_freebuf(p_head);
}

static bt_status_t queue_int_connect_next()
{
    connect_node_t* p_head = connect_queue;

    if (p_head == NULL)
        return BT_STATUS_FAIL;

    /* If the queue is currently busy, we return  success anyway,
     * since the connection has been queued... */
    if (p_head->busy != FALSE)
        return BT_STATUS_SUCCESS;

    p_head->busy = TRUE;
    return (*(btif_connect_cb_t*)p_head->p_cb)(&p_head->bda);
}

static void queue_check_connect(connect_node_t *p_param)
{
    BTIF_TRACE_VERBOSE2("%s, UUID : 0x%x", __FUNCTION__, p_param->uuid);
    connect_node_t *p_head = connect_queue;
    connect_node_t *p_temp = p_head;
    if (connect_queue == NULL)
        return;
    // Check for deletion of first node.
    if ((p_param->uuid == connect_queue->uuid))
    {
            BTIF_TRACE_VERBOSE0("Matched Connect req with uuid for single node");
            connect_queue = connect_queue->p_next;
            GKI_freebuf(p_head);
            return;
    }
    p_head = connect_queue->p_next;
    // parse list for common node and delete that node
    while (p_head != NULL)
    {
        if ((p_param->uuid == p_head->uuid))
        {
            BTIF_TRACE_VERBOSE0("Matched Connect req with uuid");
            // delete this node
            p_temp->p_next = p_head->p_next;
            GKI_freebuf(p_head);
            break;
        }
        p_temp = p_head;
        p_head = p_head->p_next;
    }
}

static void queue_int_handle_evt(UINT16 event, char *p_param)
{

    BTIF_TRACE_VERBOSE2("%s, Event : 0x%x", __FUNCTION__, event);
    switch(event)
    {
        case BTIF_QUEUE_CONNECT_EVT:
            queue_int_add((connect_node_t*)p_param);
            break;

        case BTIF_QUEUE_ADVANCE_EVT:
            queue_int_advance();
            break;
        case BTIF_QUEUE_PENDING_CONECT_EVT:
            queue_int_add((connect_node_t*)p_param);
            return;
            break;
        case BTIF_QUEUE_CHECK_CONNECT_REQ:
            queue_check_connect((connect_node_t*)p_param);
            break;
        default:
            BTIF_TRACE_VERBOSE0("BTIF_QUEUE_PENDING_CONECT_ADVANCE_EVT");
            break;
    }

    queue_int_connect_next();
}

/*******************************************************************************
**
** Function         btif_queue_connect
**
** Description      Add a new connection to the queue and trigger the next
**                  scheduled connection.
**
** Returns          BT_STATUS_SUCCESS if successful
**
*******************************************************************************/
bt_status_t btif_queue_connect(uint16_t uuid, const bt_bdaddr_t *bda,
                        btif_connect_cb_t *connect_cb, uint8_t queue_connect)
{
    connect_node_t node;
    memset(&node, 0, sizeof(connect_node_t));
    memcpy(&(node.bda), bda, sizeof(bt_bdaddr_t));
    node.uuid = uuid;
    node.p_cb = connect_cb;
    return btif_transfer_context(queue_int_handle_evt, queue_connect,
                          (char*)&node, sizeof(connect_node_t), NULL);
}

/*******************************************************************************
**
** Function         btif_queue_advance
**
** Description      Clear the queue's busy status and advance to the next
**                  scheduled connection.
**
** Returns          void
**
*******************************************************************************/
void btif_queue_advance()
{
    btif_transfer_context(queue_int_handle_evt, BTIF_QUEUE_ADVANCE_EVT,
                          NULL, 0, NULL);
}
/*******************************************************************************
**
** Function         btif_queue_pending_retry
**
** Description      Advance to the next scheduled connection.
**
** Returns          void
**
*******************************************************************************/
void btif_queue_pending_retry()
{
    BTIF_TRACE_VERBOSE0("btif_queue_pending_retry");
    btif_transfer_context(queue_int_handle_evt, BTIF_QUEUE_PENDING_CONECT_ADVANCE_EVT,
                          NULL, 0, NULL);
}


/*******************************************************************************
**
** Function         btif_queue_release
**
** Description      Free up all the queue nodes and set the queue head to NULL
**
** Returns          void
**
*******************************************************************************/
void btif_queue_release()
{
    connect_node_t *current = connect_queue;

    while (current != NULL)
    {
         connect_node_t *next = current->p_next;
         GKI_freebuf(current);
         current = next;
    }

    connect_queue = NULL;
}
/*******************************************************************************
**
** Function         btif_queue_remove_connect
**
** Description      Remove connection request from connect Queue
**                  when connect request for same UUID is received
**                  from app.
**
** Returns          void
**
*******************************************************************************/
void btif_queue_remove_connect(uint16_t uuid, uint8_t check_connect_req)
{
    connect_node_t node;
    memset(&node, 0, sizeof(connect_node_t));
    node.uuid = uuid;
    btif_transfer_context(queue_int_handle_evt, check_connect_req,
                      (char*)&node, sizeof(connect_node_t), NULL);
}

