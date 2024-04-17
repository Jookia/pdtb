section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

extern getenv_ ; char* getenv(char* name);
extern logGeneral
extern logIncoming
extern logOutgoing
extern parseMessage

extern prefix
extern prefixLen

section _TEXT

; readAuth
readAuth:
    push bp
    mov bp, sp
    push bx
    push cx
    push dx
    mov ax, authEnv
    call getenv_
    cmp ax, 0
    jne .haveEnv
    mov ax, -1
    jmp .done
.haveEnv:
    mov dx, ax
    mov ax, 0x3D00
    int 0x21
    jnc .readFile ; could open file
    mov ax, -2
    jmp .done
.readFile:
    mov bx, ax ; bx = file handle
    mov dx, authBuffer
    mov cx, authBufferMax
    mov ax, 0x3F00
    int 0x21
    mov [authBufferLen], ax
    mov dx, 0
    jnc .doneClose
    mov dx, -3
.doneClose:
    mov ax, 0x3E00
    int 0x21
    mov ax, dx
.done:
    pop dx
    pop cx
    pop bx
    pop bp
    ret


; first message sent
; returns si, cx
global dispatchInit
dispatchInit:
    mov [botState], word idleState
    call readAuth
    cmp ax, 0
    je .success
    mov si, authFailMessage
    mov cx, authFailMessageLen
    jmp logGeneral
.success:
    mov si, initMessage
    mov cx, initMessageLen
    call logGeneral
    mov si, authBuffer
    mov cx, [authBufferLen]
    ret

; si = line
; cx = length
; returns cx, re-uses lineBuffer
; clobbers si
idleState:
    call logIncoming
    cmp [si], word 'PI'
    jne .notPing
    mov [si+1], byte 'O'
    jmp logOutgoing
    .notPing:
    call parseMessage
    mov si, [prefix]
    mov cx, [prefixLen]
    call logIncoming
    mov cx, 0
    ret

; si = line
; cx = length
; returns cx, re-uses lineBuffer
; clobbers si
global dispatchLine
dispatchLine:
    jmp [botState]

; last message sent
; returns si, cx
global dispatchExit
dispatchExit:
    mov si, exitMessage
    mov cx, exitMessageLen
    call logGeneral
    mov si, quitMessage
    mov cx, quitMessageLen
    jmp logOutgoing

section CONST2

initMessage: db "Sending login credentials...",0x0d,0x0a
initMessageLen: equ $ - initMessage
authFailMessage: db "Unable to open BOT_AUTH!",0x0d,0x0a
authFailMessageLen: equ $ - authFailMessage
exitMessage: db "Quitting time",0x0d,0x0a
exitMessageLen: equ $ - exitMessage
quitMessage: db "QUIT pdtb https://twitch.tv/jookia2",0x0d,0xa
quitMessageLen equ $ - quitMessage

authEnv: db "BOT_AUTH",0x00

section _BSS

authBuffer: resb 512
authBufferMax equ $ - authBuffer
authBufferLen: resb 2

botState: resb 4
