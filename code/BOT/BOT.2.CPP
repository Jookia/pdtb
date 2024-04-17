#include <bios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"

/*
	BUG: mTCP will hang when dropping packets larger than its MTU
	This is because it can't ACK these packets and thus the stream hangs.
	Instead it should send RST or something.
	BUG: SLIRP proxies the TCP connection and will re-pack the TCP
	stream according to the TCP window size (four times the MSS) and using
	its own MTU as the cap rather than the MSS.
	COMBINED ISSUE: Using a smaller MTU in mTCP will cause the window
	to be set to something larger than its MTU and cause SLIRP to send
	packets the size of its MTU, larger than ours.
	WORKAROUND: Set mTCP's MTU to SLIRP's MTU to avoid SLIRP making
	packets too large for our uses.
*/

TcpSocket *sock;
uint8_t* recvPacket = (uint8_t*)0;
TcpBuffer* sendBuffer = (TcpBuffer*)0; /* sizeof(TcpBuffer)+TcpSocketMgr::MSS_to_advertise */

/* user customziable stuff */
int setupSocket(void) {
	Utils::parseEnv();
	Utils::initStack(1, TCP_SOCKET_RING_SIZE);
	sock = TcpSocketMgr::getSocket();
	IpAddr_t serverAddr = { 10, 0, 2, 2 };
	uint16_t srcPort = rand() + 1024;
	uint16_t dstPort = 6667;
	int timeout = 500; /* milliseconds */
	return sock->connect(srcPort, serverAddr, dstPort, timeout);
}

void cleanupSocket(void) {
    printf("cleaning up socket...\n");
	sock->close();
	TcpSocketMgr::freeSocket(sock);
	Utils::endStack();
	fclose(TrcStream);
}

extern "C" {

int recvNewPacket(void) {
    if(recvPacket != 0) {
        Buffer_free(recvPacket);
    }
    while((recvPacket = (uint8_t*)sock->incoming.dequeue()) == 0) {
        sleep(1); // DEBUG
        printf("getting new packets...\n");
	    if(bioskey(1) && bioskey(0) == (45 << 8)) {
		    fprintf(stdout, "Alt-X hit, quitting\n");
		    return 0;
	    }
        PACKET_PROCESS_SINGLE;
	    Arp::driveArp();
	    Tcp::drivePackets();
	    if(sock->isRemoteClosed() != 0) {
		    fprintf(stdout, "Remote closed, quitting\n");
	    	return 0;
	    }
	}
	IpHeader *ip = (IpHeader*)(recvPacket + sizeof(EthHeader));
	TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
	uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
	uint16_t len = ip->payloadLen() - tcp->getTcpHlen();
    printf("new packet len %i\n", len);
	return len;
}

uint8_t* currentRecvPacket(void) {
	IpHeader *ip = (IpHeader*)(recvPacket + sizeof(EthHeader));
	TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
	uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
    return userData;
}

uint8_t* currentSendBuffer(int len) {
    int max_len = TcpSocketMgr::MSS_to_advertise;
    if(len > max_len) {
        printf("currentSendBuffer: too big\n");
        return 0;
    }
    printf("currentSendBuffer: size %i\n", len);
    uint8_t* userData = ((uint8_t*)sendBuffer + sizeof(TcpBuffer));
    return userData;
}

int sendNewBuffer(uint8_t* buffer, int len) {
    int max_len = TcpSocketMgr::MSS_to_advertise;
    if(len > max_len) {
        printf("sendBuffer: too big\n");
        return 2;
    }
    TcpBuffer* buf = (TcpBuffer*)((uint8_t*)buffer - sizeof(TcpBuffer));
	buf->dataLen = len;
	int ret = sock->enqueue(buf);
    sendBuffer = TcpBuffer::getXmitBuf();
	switch(ret) {
	    case TCP_RC_GOOD:
            printf("sendBuffer: sent\n");
            PACKET_PROCESS_SINGLE;
	        Arp::driveArp();
	        Tcp::drivePackets();
	        return 0;
	    case TCP_RC_BAD:
            printf("sendBuffer: bad\n");
	        return 1;
	    case -2:
            printf("sendBuffer: too big 2\n");
	        return 2;
	    default:
            printf("sendBuffer: wtf\n");
	        return 3;
    }
}
    
extern int asmRun(void);

}

int main(void) {
	int rc = 1;
	if(setupSocket()) {
		fprintf(stdout, "Unable to connect to host\n");
		cleanupSocket();
		return 1;
	}
    sendBuffer = TcpBuffer::getXmitBuf();
	rc = asmRun();
	cleanupSocket();
	return rc;
}
