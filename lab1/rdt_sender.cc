/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include "rdt_sender.h"
#include "checksum.h"

int now_seqNumber = 0;
int now_ack = 0;
int sender_current_capacity = 0;

std::vector<packet> sender_window;
std::vector<int> sender_ack_table;

std::vector<packet> buffer;

checksum_tool *sender_cksum_tool;

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    // freopen("freopen.out", "w", stdout);     //debug
    sender_ack_table.resize(WINDOW_SIZE + 1);
    sender_cksum_tool = new checksum_tool();
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* maximum payload size */
    int maxpay_loadsize = RDT_PKTSIZE - HEAD_SIZE;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size - cursor > maxpay_loadsize)
    {
        /* fill in the packet */

        /* pay load size */
        pkt.data[CHECK_SUMSIZE] = maxpay_loadsize;

        /* sequence num and ack num*/
        // fprintf(stdout, "sender sequence num1 %d \n", now_seqNumber);
        memcpy(pkt.data + CHECK_SUMSIZE + PAY_LOADSIZE, &now_seqNumber, SEQ_NUMSIZE);
        now_seqNumber++;

        /* pay load */
        memcpy(pkt.data + HEAD_SIZE, msg->data + cursor, maxpay_loadsize);

        /* last generate checksum*/
        unsigned short checksum = sender_cksum_tool->gen_checksum(pkt.data + CHECK_SUMSIZE , RDT_PKTSIZE - CHECK_SUMSIZE);
        memcpy(pkt.data, &checksum, CHECK_SUMSIZE);
        // fprintf(stdout, "%dth can can need %d \n", now_seqNumber-1 ,checksum);

        /* add it to the buffer */
        buffer.push_back(pkt);

        /* move the cursor */
        cursor += maxpay_loadsize;
    }

    /* send out the last packet */
    if (msg->size > cursor)
    {
        /* fill in the packet */

        /* pay load size */
        pkt.data[CHECK_SUMSIZE] = msg->size - cursor;

        /* sequence num size*/
        // fprintf(stdout, "sender sequence num2 %d \n", now_seqNumber);
        memcpy(pkt.data + CHECK_SUMSIZE + PAY_LOADSIZE, &now_seqNumber, SEQ_NUMSIZE);
        now_seqNumber++;

        /* pay load */
        memcpy(pkt.data + HEAD_SIZE, msg->data + cursor, pkt.data[CHECK_SUMSIZE]);

        /* last generate checksum*/
        unsigned short checksum = sender_cksum_tool->gen_checksum(pkt.data + CHECK_SUMSIZE , pkt.data[CHECK_SUMSIZE] + PAY_LOADSIZE + SEQ_NUMSIZE);
        memcpy(pkt.data, &checksum, CHECK_SUMSIZE);
        // fprintf(stdout, "%dth can can need %d \n", now_seqNumber-1 ,checksum);

        /* add it to the buffer */
        buffer.push_back(pkt);
    }

    sendPacket();
}

void sendPacket()
{
    /* the window has capacity to send*/
    while (buffer.size() > 0 && sender_current_capacity < WINDOW_SIZE)
    {
        packet pkt_to_send = buffer.front();

        sender_window.push_back(pkt_to_send);
        buffer.erase(buffer.begin());
        sender_current_capacity++;

        Sender_ToLowerLayer(&pkt_to_send);
    }
    if (sender_current_capacity == WINDOW_SIZE && !Sender_isTimerSet()) // full and hasn't use a timer
    { 
        Sender_StartTimer(EXPIRE_TIME);
    }
    if (sender_current_capacity == 0 && buffer.size() == 0) // all package have been sent
    {
        // fprintf(stdout, "send all package \n");
        Sender_StopTimer();
    }
}

/* event handler, called when a packet is passed from the lower layer at the sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    /*check sum*/
    unsigned short *check_sum = (unsigned short *)(pkt->data);
    int buf_size = pkt->data[CHECK_SUMSIZE] + PAY_LOADSIZE + SEQ_NUMSIZE;
    bool res = sender_cksum_tool->verify_checksum(pkt->data + CHECK_SUMSIZE, buf_size, *check_sum);
    if(!res){
        // fprintf(stdout, "%d checksum doesn't pass\n", *seq_num);
        return;
    }

    int *seq_num = (int *)(pkt->data + CHECK_SUMSIZE + PAY_LOADSIZE);
    if (*seq_num == now_ack)    // the package is the smallest in window
    { 
        sender_ack_table[0] = 1;
        int i = 0;
        for (; i <= WINDOW_SIZE; i++)
        {
            if (sender_ack_table[0] == 1)
            {
                sender_window.erase(sender_window.begin());
                sender_ack_table.erase(sender_ack_table.begin());
                sender_ack_table.push_back(0);
                sender_current_capacity--;
            }
            else
                break;
        }
        now_ack += i;
        sendPacket();
    }
    else if (*seq_num > now_ack && *seq_num < (now_ack + WINDOW_SIZE))  // the package is in the window
    { 
        int index = (*seq_num - now_ack) % WINDOW_SIZE;
        if (sender_ack_table[index] == 0)
        {
            sender_ack_table[index] = 1;
        }
    }
    if (buffer.size() == 0 && sender_current_capacity == 0)     //last package has been acked
        return;

    else if(!Sender_isTimerSet())
        Sender_StartTimer(EXPIRE_TIME);
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    // fprintf(stdout, "expire ack num %d\n", now_ack);

    packet pkt = *(sender_window.begin());
    Sender_ToLowerLayer(&pkt);  //may lose

    Sender_StartTimer(EXPIRE_TIME);     //you have to restart a timer because the above pkt may lose
}
