section _TEXT class=CODE group=DGROUP

extern currentRecvPacket_ ; uint8_t* currentRecvPacket(void)
extern recvNewPacket_ ; int recvNewPacket(void)
extern sendPacket_ ; int sendPacket(uint8_t* packet, int len)
extern printf_

global asmRun_
asmRun_:
    push bp
.asmLoop:
    call recvNewPacket_
    cmp ax, 0
    jz .noNew
    mov dx, ax
    call currentRecvPacket_
    push ax
    push dx
    push printFormat
    call printf_
    ; ax = buf, dx = len
    ; call sendPacket_
    jmp .asmLoop
.noNew:
    pop bp
    mov ax, 0
    ret

printFormat: db "INCOMING (%i): %s",0x0a,0x00

;copyLoop:
;  c = *inData
;  inData++
;  if c is \n:
;    print it
;  *lineBuffer = c
;  lineBuffer++
;  if lineBuffer out of range: 
;    what do?
