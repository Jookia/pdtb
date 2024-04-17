section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

extern getenv_ ; char* getenv(char* name);
extern logGeneral
extern parseMessage

extern writeStr
extern writeChar

extern doParse
extern paramAt
extern cmdBuffer
extern cmdBufferLen

section _TEXT

; readAuth
readAuth:
    push bp
    mov bp, sp
    push es
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
    mov dx, outBuffer
    mov cx, outBufferMax
    mov ax, 0x3F00
    int 0x21
    mov [outBufferLen], ax
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
    pop es
    pop bp
    ret

; si = line
; cx = length
global logIncoming
logIncoming:
    push cx
    push ax
    push bx
    dec cx ; Remove new line
    mov ax, incomingTag
    mov bx, incomingTagLen
    call logGeneral
    pop bx
    pop ax
    pop cx
    ret

; si = line
; cx = length
global logOutgoing
logOutgoing:
    push cx
    push ax
    push bx
    mov ax, outgoingTag
    mov bx, outgoingTagLen
    dec cx ; Remove new line
    call logGeneral
    pop bx
    pop ax
    pop cx
    ret

; si = line
; cx = length
logLogic:
    push ax
    push bx
    mov ax, logicTag
    mov bx, logicTagLen
    call logGeneral
    pop bx
    pop ax
    ret

; first message sent
; returns si, cx
global dispatchInit
dispatchInit:
    mov [botState], word startState
    call readAuth
    cmp ax, 0
    je .success
    mov si, authFailMessage
    mov cx, authFailMessageLen
    jmp logLogic
.success:
    mov si, initMessage
    mov cx, initMessageLen
    call logLogic
    mov si, outBuffer
    mov cx, [outBufferLen]
    ret

; calls code in the calling convention for string building
; !make sure to preserve di and cx!
buildBuffer:
    push ax
    push bp
    push bx
    push di
    mov di, outBuffer
    mov cx, outBufferLen
    call [botState]
    mov bx, cx
    mov si, outBuffer
    mov cx, outBufferLen
    sub cx, bx
    pop di
    pop bx
    pop bp
    pop ax
    ret

writePong:
    mov si, pongCmd
    mov bx, pongCmdLen
    call writeStr
    mov ax, 0x20 ; SPACE
    call writeChar
    mov ax, 0x3A ; :
    call writeChar
    xchg bx, cx
    mov ax, 0
    call paramAt
    xchg bx, cx
    call writeStr
    mov ax, 0x0D ; CARRIAGE RETURN
    call writeChar
    mov ax, 0x0A ; LINE FEED
    call writeChar
    ret

writeHello:
    mov si, privmsgCmd
    mov bx, privmsgCmdLen
    call writeStr
    mov ax, 0x20 ; SPACE
    call writeChar
    xchg bx, cx
    mov ax, 0
    call paramAt
    xchg bx, cx
    call writeStr
    mov ax, 0x20 ; SPACE
    call writeChar
    mov ax, 0x3A ; :
    call writeChar
    mov si, helloText
    mov bx, helloTextLen
    call writeStr
    mov ax, 0x0D ; CARRIAGE RETURN
    call writeChar
    mov ax, 0x0A ; LINE FEED
    call writeChar
    ret

; Wait for 004 command
startState:
    mov si, [cmdBuffer]
    cmp [si], word '00'
    jne .not004
    cmp [si+2], byte '4'
    jne .not004
    mov si, startCmd
    mov bx, startCmdLen
    call writeStr
    mov [botState], word idleState
.not004:
    ret

; Manage day to day bot commands
idleState:
    mov si, [cmdBuffer]
    cmp [si], word 'PI'
    je writePong
    xchg bx, cx
    mov ax, 1
    call paramAt
    xchg bx, cx
    cmp [si], word '>H'
    jne .done
    cmp [si+2], word 'EL'
    jne .done
    cmp [si+4], word 'LO'
    je writeHello
.done:
    ret

; si = line
; cx = length
; clobbers si and cx
global dispatchLine
dispatchLine:
    call logIncoming
    call doParse
    jc .badParse
    call buildBuffer
    cmp cx, 0
    je .returnEmpty
    call logOutgoing
    ret
.badParse:
    mov si, badParseMessage
    mov cx, badParseMessageLen
    call logLogic
.returnEmpty:
    mov cx, 0
    ret

; last message sent
; returns si, cx
global dispatchExit
dispatchExit:
    mov si, exitMessage
    mov cx, exitMessageLen
    call logLogic
    mov si, quitMessage
    mov cx, quitMessageLen
    jmp logOutgoing

section CONST2

incomingTag: db "INCOMING"
incomingTagLen equ $ - incomingTag
outgoingTag: db "OUTGOING"
outgoingTagLen equ $ - outgoingTag
logicTag: db "LOGIC"
logicTagLen: equ $ - logicTag

badParseMessage: db "Couldn't parse message"
badParseMessageLen: equ $ - badParseMessage
initMessage: db "Sending login credentials..."
initMessageLen: equ $ - initMessage
authFailMessage: db "Unable to open BOT_AUTH!"
authFailMessageLen: equ $ - authFailMessage

exitMessage: db "Quitting time",0x0d,0x0a
exitMessageLen: equ $ - exitMessage
quitMessage: db "QUIT pdtb https://twitch.tv/jookia2",0x0d,0xa
quitMessageLen equ $ - quitMessage

pongCmd: db "PONG"
pongCmdLen equ $ - pongCmd
privmsgCmd: db "PRIVMSG"
privmsgCmdLen equ $ - privmsgCmd
startCmd: db "CAP REQ :twitch.tv/membership twitch.tv/tags twitch.tv/commands",0x0d,0x0a,"JOIN #jookia2",0x0d,0x0a
startCmdLen equ $ - startCmd
helloText: db "HELLO THERE"
helloTextLen equ $ - helloText

authEnv: db "BOT_AUTH",0x00

section _BSS

outBuffer: resb 512
outBufferMax equ $ - outBuffer
outBufferLen: resb 2

botState: resb 4
