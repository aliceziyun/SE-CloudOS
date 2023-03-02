#include "checksum.h"

unsigned short checksum_tool::gen_checksum(char *buf, int n)
{
    unsigned long sum;
    for(sum = 0; n > 0; n--){
        sum += *buf;
        buf++;
    }

    while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (unsigned short) ~sum;
}

bool checksum_tool::verify_checksum(char *buf, int n, unsigned short checksum)
{
    unsigned short sum = gen_checksum(buf,n);

    return sum == checksum;
}