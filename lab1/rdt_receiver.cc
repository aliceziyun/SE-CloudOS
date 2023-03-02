/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or
 *       reordering.  You will need to enhance it to deal with all these
 *       situations.  In this implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "checksum.h"

int now_ackNumber = 0;
int receiver_current_capacity = 0;
std::vector<packet> receiver_window;
std::vector<int> receiver_ack_table;

checksum_tool *receiver_cksum_tool;

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    receiver_window.resize(WINDOW_SIZE);
    receiver_ack_table.resize(WINDOW_SIZE);

    receiver_cksum_tool = new checksum_tool();
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* read sequence number*/
    int *seq_num = (int *)(pkt->data + CHECK_SUMSIZE + PAY_LOADSIZE);

    /* check sum */
    unsigned short *check_sum = (unsigned short *)(pkt->data);
    int buf_size = pkt->data[CHECK_SUMSIZE] + PAY_LOADSIZE + SEQ_NUMSIZE;
    bool res = receiver_cksum_tool->verify_checksum(pkt->data + CHECK_SUMSIZE, buf_size, *check_sum);
    if(!res){
        // fprintf(stdout, "%d checksum doesn't pass\n", *seq_num);
        return;
    }

    /* send the ack back to sender again*/
    if (*seq_num < now_ackNumber)
    {
        Receiver_ToLowerLayer(pkt);
        return;
    }

    /* put the package into buffer*/
    int index = (*seq_num - now_ackNumber) % WINDOW_SIZE;
    // fprintf(stdout, "receive sequence num %d and index %d\n", *seq_num, index);

    receiver_window[index] = *pkt;
    if (receiver_ack_table[index] != 1)
    {
        receiver_ack_table[index] = 1;
        receiver_current_capacity++;
    }

    if (receiver_current_capacity == WINDOW_SIZE)
    {
        // fprintf(stdout, "submit all \n");
        submitPakage(WINDOW_SIZE);
    }
    else
    {
        int i = 0;
        for (; i < receiver_current_capacity; i++)
        {
            if (receiver_ack_table[i] != 1)
                break;
        }
        submitPakage(i);
    }
}

void submitPakage(int number)
{
    while (number > 0)
    {
        packet sub_pkt = receiver_window.front();
        Receiver_ToLowerLayer(&sub_pkt); // send the ack info

        receiver_window.erase(receiver_window.begin());
        receiver_ack_table.erase(receiver_ack_table.begin());
        receiver_window.resize(WINDOW_SIZE);
        receiver_ack_table.push_back(0);
        receiver_current_capacity--;
        now_ackNumber++;

        // int *seq_num = (int *)(sub_pkt.data + CHECK_SUMSIZE + PAY_LOADSIZE);
        // fprintf(stdout, "submit the %dth package \n", *seq_num);

        /* construct a message and deliver to the upper layer */
        struct message *msg = (struct message *)malloc(sizeof(struct message));
        ASSERT(msg != NULL);

        /* read header*/
        msg->size = sub_pkt.data[CHECK_SUMSIZE];

        /* sanity check in case the packet is corrupted */
        if (msg->size < 0)
            msg->size = 0;
        if (msg->size > RDT_PKTSIZE - HEAD_SIZE)
            msg->size = RDT_PKTSIZE - HEAD_SIZE;

        msg->data = (char *)malloc(msg->size);
        ASSERT(msg->data != NULL);
        memcpy(msg->data, sub_pkt.data + HEAD_SIZE, msg->size);

        Receiver_ToUpperLayer(msg);

        /* don't forget to free the space */
        if (msg->data != NULL)
            free(msg->data);
        if (msg != NULL)
            free(msg);

        number--;
    }
}
