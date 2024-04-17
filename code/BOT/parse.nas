section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

section _TEXT

; <message>  ::= ['@' <tags> SPACE] [':' <prefix> <SPACE> ] <command> <params> <crlf>
; param can be :<anything><cr><lf>...
; param can be word1 :<anything><cr><lf>...
; param can be word1 word2 :<anything><cr><lf>...

; format to print nicely:
; - prefix
; - command
; - param1
; - param2
; - param3
; - param4

; si is string
; cx is length
; returns nothing yet
global parseMessage
parseMessage:
    jcxz .return
    push ax
    push bx
    push si
    push di
    push cx
    mov di, si
    ; di is the next byte AFTER the string
    add si, cx
    call parsePrefix
    cmp di, si
    je .return
.return:
    pop cx
    pop di
    pop si
    pop bx
    pop ax
    ret

; ax is clobbered
; bx is clobbered
; si is string start
; di is string end
parsePrefix:
    cmp byte [di], ':'
    je .isPrefix
    mov word [prefixLen], 0
    ret
.isPrefix:
    inc di ; skip ':'
    ; set prefix
    mov bx, di
    mov [prefix], bx
.skipContent:
    cmp di, si
    je .return
    scasb
    mov al, byte [di]
    inc di
    cmp al, ' '
    jne .skipContent
    sub bx, di
    neg bx
    dec bx ; we're 1 ahead
    mov [prefixLen], bx
.skipSpaces:
    cmp di, si
    je .return
    mov al, byte [di]
    inc di
    cmp al, ' '
    je .skipSpaces
.return:
    ret

section CONST2

;authEnv: db "BOT_AUTH",0x00

section _BSS

global prefix
prefix: resb 4
global prefixLen
prefixLen: resb 4
