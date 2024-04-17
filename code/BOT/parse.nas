section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

section _TEXT

; take in a string
; fill up the following values:
; - param1
; - param2
; - param3
; - param4

; call convention:
; - ah,al: input/output regs
; - bx: len left in read buffer
; - cx: len left in write buffer
; - si: read buffer
; - di: write buffer
; - cf: whether we 'failed'
; low level char:
;   peek: read char to ax, no si increment, check parse len
;   read: read char to ax, yes si increment, len mod
;   copy: copy character to read buffer, len mod, check parse len + read len
; high level:
;   copyWord: input: max len, returns length of word, set z if overflow
;   skipSpace: read spaces until no more

; LOW LEVEL STUFF

global peek
peek:
    cmp bx, 0
    jz .error
    mov al, [si]
    clc
    ret
.error:
    stc
    ret

global read
read:
    cmp bx, 0
    jz .error
    mov al, [si]
    inc si
    dec bx
    clc
    ret
.error:
    stc
    ret

global copy
copy:
    cmp bx, 0
    jz .error
    cmp cx, 0
    jz .error
    mov al, [si]
    mov [di], al
    inc si
    inc di
    dec bx
    dec cx
    clc
    ret
.error:
    stc
    ret

; MACROS

%macro do_parse 1
    call %1
    jc .return
%endmacro

%macro do_expect 1
    cmp al, %1
    clc
    jne .return
%endmacro

%macro do_expect_not 1
    cmp al, %1
    clc
    je .return
%endmacro

%define do_peek do_parse peek
%define do_read do_parse read
%define do_copy do_parse copy

%macro set_parse_buffer 3 ; buf max len
    mov di, %1
    mov cx, %2
    mov [%3], cx
%endmacro

%macro sub_parse_buffer_len 1 ; max len
    sub [%1], cx
%endmacro

; HIGH LEVEL PRIMITIVES

; Copies a possibly empty word until a space
global copyWord
copyWord:
    do_peek
    do_expect_not ' '
    do_copy
    jmp copyWord
.return:
    ret

; Skips any spaces
global skipSpace
skipSpace:
    do_peek
    do_expect ' '
    do_read
    jmp skipSpace
.return:
    ret

global parseWordIf
parseWordIf:
    do_peek
    cmp al, ah
    clc
    jne .return
    do_read ; Skip the character
    do_parse copyWord
    do_parse skipSpace
.return:
    ret

; IRC SYNTAX

parseTags:
    set_parse_buffer tagsBuffer, tagsBufferLenMax, tagsBufferLen
    mov ah, '@'
    do_parse parseWordIf
    sub_parse_buffer_len tagsBufferLen
.return:
    ret

parsePrefix:
    set_parse_buffer prefixBuffer, prefixBufferLenMax, prefixBufferLen
    mov ah, ':'
    do_parse parseWordIf
    sub_parse_buffer_len prefixBufferLen
.return:
    ret

parseCmd:
    set_parse_buffer cmdBuffer, cmdBufferLenMax, cmdBufferLen
    do_parse copyWord
    sub_parse_buffer_len cmdBufferLen
.return:
    ret

parseMessage:
    do_parse parseTags
    do_parse parsePrefix
    do_parse parseCmd
    do_parse skipSpace
    do_read
    cmp al, 0xd ; \r
    clc
    jne .return
    ret
.return: ; Only used for errors
    stc
    ret

; si: string to parse
; cx: string length
; returns cf = set on failure
global doParse
doParse:
    push ax
    push bx
    push di ; used for write buffer
    mov bx, cx ; bx is used for read len
    call parseMessage
    pop di
    pop bx
    pop ax
    ret

section CONST2

section _BSS

global tagsBuffer
global tagsBufferLen
global prefixBuffer
global prefixBufferLen
global cmdBuffer
global cmdBufferLen

tagsBuffer: resb 32
tagsBufferLenMax: equ $ - tagsBuffer
tagsBufferLen: resb 2
prefixBuffer: resb 32
prefixBufferLenMax: equ $ - prefixBuffer
prefixBufferLen: resb 2
cmdBuffer: resb 8
cmdBufferLenMax: equ $ - cmdBuffer
cmdBufferLen: resb 2
