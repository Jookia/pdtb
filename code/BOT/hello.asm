section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
GROUP DGROUP CONST2

extern currentRecvPacket_ ; uint8_t* currentRecvPacket(void)
extern recvNewPacket_ ; int recvNewPacket(void)
extern sendNewBuffer_ ; int sendNewBuffer(uint8_t* packet, int len)
extern printf_ ; int printf(const char *format, ...)

section _TEXT

global asmRun_
asmRun_:
; Entry
    push ax
    push bx
    push cx
    push dx
    push bp
    push di
    push si
.waitPacket:
    call recvNewPacket_
    cmp ax, 0
    jz .noMore
; Get new packet address
    mov dx, ax
    call currentRecvPacket_
; ax = packet, dx = size
; Terminate packet for printing
    mov bx, ax
    mov si, dx
    mov [ds:bx+si], byte 0
; Print packet, preserve registers
    push ax
    push dx
    push printFormat
    call printf_
    pop ax
    pop dx
    pop ax
; Send packet back
    call sendNewBuffer_
    jmp .waitPacket
.noMore:
; Exit
    pop si
    pop di
    pop bp
    pop dx
    pop cx
    pop bx
    pop ax
    mov ax, 0
    ret

section CONST2

printFormat: db "INCOMING (%i): %s",0x0a,0x00
