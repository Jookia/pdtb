section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

extern currentRecvPacket_ ; uint8_t* currentRecvPacket(void)
extern recvNewPacket_ ; int recvNewPacket(void)
extern currentSendBuffer_ ; uint8_t* currentSendBuffer(int len)
extern sendNewBuffer_ ; int sendNewBuffer(uint8_t* packet, int len)
extern dispatchLine
extern dispatchInit
extern dispatchExit

section _TEXT

; si = packet
; cx = len
; rets 0 if good, positive int if bad
sendPacket:
    push es
    mov ax, cx
    mov dx, cx
    call currentSendBuffer_
    cmp ax, 0
    jne .continue
    mov ax, 10
    pop es
    ret
.continue:
    mov di, ax
    rep movsb
    call sendNewBuffer_
    pop es
    ret

; Prelude
global asmRun_
; Prologue
asmRun_:
    push es
    push ds
    pop es
    push bx
    push cx
    push dx
    push di
    push si
; ax, bx, cx = scratch
; si = packetBuffer
; bx = packetBufferLen
; di = lineBuffer
; dx = lineBufferLen
stateStart:
    call dispatchInit
    cmp ax, 0
    jnz stateEnd
    call sendPacket
    cmp ax, 0
    jnz stateEnd
    mov di, lineBuffer
    mov dx, lineBufferLen
stateGetPacket:
    push es
    call recvNewPacket_ ; Blocking call
    pop es
    cmp ax, 0 ; Check if packet is empty
    jz stateDisconnect ; No more packet, bail
    cmp ax, -1 ; Check if quit requested
    jz stateQuit
    ; Get new packet address
    mov bx, ax
    push es
    call currentRecvPacket_ ; Get packet pointer
    pop es
    mov si, ax
stateAppendLine:
    mov ax, 0
    cmp bx, 0
    jz stateGetPacket
    cmp dx, 0
    jz stateSkipLine
.copyLoop:
    lodsb
    dec bx
    stosb
    dec dx
    cmp al, 0x0a
    je stateDispatch
    jmp stateAppendLine
stateSkipLine:
    mov cx, bx
    mov al, 0x0a
    xchg di, si
    repne scasb
    xchg di, si
    mov bx, cx
    cmp cx, 0
    jz stateGetPacket
    mov di, lineBuffer
    mov dx, lineBufferLen
    jmp stateAppendLine
stateDispatch:
    mov di, lineBuffer
    mov cx, lineBufferLen
    sub cx, dx
    xchg di, si
    call dispatchLine
    cmp cx, 0
    je .finish
    call sendPacket
    cmp ax, 0
    jnz stateEnd
.finish:
    xchg di, si
    mov di, lineBuffer
    mov dx, lineBufferLen
    jmp stateAppendLine
stateQuit:
    call dispatchExit
    call sendPacket
stateDisconnect:
    mov ax, 5
stateEnd:
; Epilogue
asmQuit:
    pop si
    pop di
    pop dx
    pop cx
    pop bx
    pop es
    ret


section CONST2

; unused

section _BSS

lineBuffer: resb 512
lineBufferLen equ $ - lineBuffer
