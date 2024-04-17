#include <bios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include "types.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"

TcpSocket *sock = (TcpSocket*)0;
uint8_t* recvPacket = (uint8_t*)0;
TcpBuffer* sendBuffer = (TcpBuffer*)0;

#pragma aux logGeneral "logGeneral"
#pragma aux doLogGeneral = "call logGeneral" parm [ax] [bx] [si] [cx]

extern "C" int asmRun();
extern "C" int openLog();
extern "C" int closeLog();

void logPrintf(const char *format, ...) {
  va_list args;
  char buffer[512];
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  char wrapper[] = "WRAPPER";
  doLogGeneral(wrapper, sizeof(wrapper) - 1, buffer, len);
}

void __interrupt __far handler_ctrlbrk(void) {
}
void __interrupt __far handler_ctrlc(void) {
}

int getEnvironmentDetails(IpAddr_t addr, uint16_t *dstPort) {
  char *details = getenv("BOT_HOST");
  if(!details) {
    return 1;
  }
  unsigned int intaddr[4];
  char canary;
  int scanned = sscanf(details, "%u.%u.%u.%u:%u%c",
    &intaddr[0], &intaddr[1], &intaddr[2], &intaddr[3], dstPort, canary);
  if(scanned != 5) {
    return 2;
  }
  for(int i = 0; i < 4; ++i) {
    unsigned int octet = intaddr[i];
    if(octet <= 255) {
      addr[i] = (uint8_t)octet;
    } else {
      return 3 + i;
    }
  }
  return 0;
}

/* user customziable stuff */
int setupSocket(void) {
  int rc = Utils::parseEnv();
  if(rc) {
    logPrintf("setupSocket: parseEnv failed, rc %i", rc);
    return rc;
  }
  rc = Utils::initStack(1, TCP_SOCKET_RING_SIZE, handler_ctrlbrk, handler_ctrlc);
  if(rc) {
    logPrintf("setupSocket: initStack failed, rc %i", rc);
    return rc;
  }
  sock = TcpSocketMgr::getSocket();
  if(!sock) {
    logPrintf("setupSocket: getSocket failed, rc %i", rc);
    return 1;
  }
  IpAddr_t addr;
  uint16_t dstPort;
  int err = getEnvironmentDetails(addr, &dstPort);
  if(err) {
    logPrintf("setupSocket: Unable to read BOT_HOST, rc %i", rc);
    return 0;
  }
  logPrintf("Host details: %i.%i.%i.%i:%i", 
    addr[0], addr[1], addr[2], addr[3], dstPort);
  uint16_t srcPort = rand() + 1024;
  int timeout = 1000; /* milliseconds */
  return sock->connect(srcPort, addr, dstPort, timeout);
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
      logPrintf("recvNewPacket: Remote closed, quitting");
      return 0;
    }
    if(_bios_keybrd(1) && _bios_keybrd(0) == (45 << 8)) {
      logPrintf("recvNewPacket: Alt-X hit, quitting");
      return -1;
    }
  }
  IpHeader *ip = (IpHeader*)(recvPacket + sizeof(EthHeader));
  TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
  uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
  uint16_t len = ip->payloadLen() - tcp->getTcpHlen();
  logPrintf("recvNewPacket: New packet, len %i", len);
  return len;
}

uint8_t* currentSendBuffer(int len) {
  int max_len = TcpSocketMgr::MSS_to_advertise;
  /* TODO: check if sendBuffer is NUL */
  if(len > max_len) {
    logPrintf("sendNewBuffer: Packet too big, %i > %i", len, max_len);
    return 0;
  }
  uint8_t* userData = ((uint8_t*)sendBuffer + sizeof(TcpBuffer));
  return userData;
}

int sendNewBuffer(uint8_t* buffer, int len) {
  int max_len = TcpSocketMgr::MSS_to_advertise;
  if(len > max_len) {
    logPrintf("sendNewBuffer: Packet too big, %i > %i", len, max_len);
    return 2;
  }
  TcpBuffer* buf = (TcpBuffer*)((uint8_t*)buffer - sizeof(TcpBuffer));
  buf->dataLen = len;
  int ret = sock->enqueue(buf);
  /* TODO: confirm that enqueue takes the xmitbuf */
  sendBuffer = TcpBuffer::getXmitBuf();
  switch(ret) {
    case TCP_RC_GOOD:
      logPrintf("sendNewBuffer: Sent packet, len %i", len);
      PACKET_PROCESS_SINGLE;
      Arp::driveArp();
      Tcp::drivePackets();
      return 0;
    case TCP_RC_BAD:
      logPrintf("sendNewBuffer: Error TCP_RC_BAD");
      return 1;
    case -2:
      logPrintf("sendNewBuffer: Too big");
      return 2;
    default:
      logPrintf("sendNewBuffer: Unknown error");
      return 3;
  }
}
    
}

int main(void) {
  int rc;
  if(rc = openLog()) {
    printf("openLog: Unable to open BOT_LOG (rc %i)\n", rc);
    return 1;
  }
  if(rc = setupSocket()) {
    cleanupSocket();
    logPrintf("main: Unable to connect to host (rc %i)", rc);
    closeLog();
    return 1;
  }
  sendBuffer = TcpBuffer::getXmitBuf();
  logPrintf("main: Assembly starting");
  rc = asmRun();
  logPrintf("main: Assembly done (rc %i), cleaning up", rc);
  cleanupSocket();
  closeLog();
  return rc;
}
