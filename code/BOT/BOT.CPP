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

TcpSocket *sock = (TcpSocket*)0;
uint8_t* recvPacket = (uint8_t*)0;
TcpBuffer* sendBuffer = (TcpBuffer*)0;

void __interrupt __far handler_ctrlbrk(void) {
}
void __interrupt __far handler_ctrlc(void) {
}

/* user customziable stuff */
int setupSocket(void) {
  int rc = Utils::parseEnv();
  if(rc) {
    printf("setupSocket: parseEnv failed, rc %i\n", rc);
    return rc;
  }
  rc = Utils::initStack(1, TCP_SOCKET_RING_SIZE, handler_ctrlbrk, handler_ctrlc);
  if(rc) {
    printf("setupSocket: initStack failed, rc %i\n", rc);
    return rc;
  }
  sock = TcpSocketMgr::getSocket();
  if(!sock) {
    printf("setupSocket: getSocket failed, rc %i\n", rc);
    return 1;
  }
  IpAddr_t serverAddr = { 10, 0, 2, 2 };
  uint16_t srcPort = rand() + 1024;
  uint16_t dstPort = 6667;
  int timeout = 5000; /* milliseconds */
  return sock->connect(srcPort, serverAddr, dstPort, timeout);
}

void cleanupSocket(void) {
  sock->close();
  TcpSocketMgr::freeSocket(sock);
  Utils::endStack();
}

extern "C" {

uint8_t* currentRecvPacket(void) {
  IpHeader *ip = (IpHeader*)(recvPacket + sizeof(EthHeader));
  TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
  uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
  return userData;
}

int recvNewPacket(void) {
  if(recvPacket != 0) {
    Buffer_free(recvPacket);
  }
  while((recvPacket = (uint8_t*)sock->incoming.dequeue()) == 0) {
    PACKET_PROCESS_SINGLE;
    Arp::driveArp();
    Tcp::drivePackets();
    if(sock->isRemoteClosed() != 0) {
      fprintf(stdout, "recvNewPacket: Remote closed, quitting\n");
      return 0;
    }
    if(bioskey(1) && bioskey(0) == (45 << 8)) {
      fprintf(stdout, "recvNewPacket: Alt-X hit, quitting\n");
      return 0;
    }
  }
  IpHeader *ip = (IpHeader*)(recvPacket + sizeof(EthHeader));
  TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
  uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
  uint16_t len = ip->payloadLen() - tcp->getTcpHlen();
  printf("recvNewPacket: New packet, len %i\n", len);
  return len;
}

uint8_t* currentSendBuffer(int len) {
  int max_len = TcpSocketMgr::MSS_to_advertise;
  /* TODO: check if sendBuffer is NUL */
  if(len > max_len) {
    printf("sendNewBuffer: Packet too big, %i > %i\n", len, max_len);
    return 0;
  }
  uint8_t* userData = ((uint8_t*)sendBuffer + sizeof(TcpBuffer));
  return userData;
}

int sendNewBuffer(uint8_t* buffer, int len) {
  int max_len = TcpSocketMgr::MSS_to_advertise;
  if(len > max_len) {
    printf("sendNewBuffer: Packet too big, %i > %i\n", len, max_len);
    return 2;
  }
  TcpBuffer* buf = (TcpBuffer*)((uint8_t*)buffer - sizeof(TcpBuffer));
  buf->dataLen = len;
  int ret = sock->enqueue(buf);
  /* TODO: confirm that enqueue takes the xmitbuf */
  sendBuffer = TcpBuffer::getXmitBuf();
  switch(ret) {
    case TCP_RC_GOOD:
      printf("sendNewBuffer: Sent packet, len %i\n", len);
      PACKET_PROCESS_SINGLE;
      Arp::driveArp();
      Tcp::drivePackets();
      return 0;
    case TCP_RC_BAD:
      printf("sendNewBuffer: Error TCP_RC_BAD\n");
      return 1;
    case -2:
      printf("sendNewBuffer: Too big\n");
      return 2;
    default:
      printf("sendNewBuffer: Unknown error\n");
      return 3;
  }
}
    
extern int asmRun(void);

}

int main(void) {
  int rc;
  if(setupSocket()) {
    fprintf(stdout, "main: Unable to connect to host\n");
    return 1;
  }
  sendBuffer = TcpBuffer::getXmitBuf();
  printf("main: Assembly starting\n");
  rc = asmRun();
  printf("main: Assembly done (rc %i), cleaning up\n", rc);
  cleanupSocket();
  return rc;
}
