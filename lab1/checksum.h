#ifndef _CHECKSUM_H_
#define _CHECKSUM_H_
//checksum tool
class checksum_tool
{
public:
    checksum_tool(/* args */){};
    ~checksum_tool(){};

    unsigned short gen_checksum(char *buf, int n);
    bool verify_checksum(char *buf, int n, unsigned short checksum);
};

#endif