section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

section _TEXT

; call convention:
; - ah,al: input/output regs
; - bp: scratch register
; - bx: len left in read buffer
; - cx: len left in write buffer <- PRESERVE ACROSS CALLS
; - si: read buffer
; - di: write buffer <- PRESERVE ACROSS CALLS
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

%macro set_parse_buffer 2 ; buf len
    mov [%1], di
    mov [%2], cx
    clc
%endmacro

%macro sub_parse_buffer_len 1 ; len
    sub [%1], cx
    clc
%endmacro

; HIGH LEVEL PRIMITIVES

; Copies a possibly empty word until a space or CR
global copyWord
copyWord:
    do_peek
    cmp al, ' '
    clc
    je .return
    cmp al, 0xd ; \r
    clc
    je .return
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

peekCR:
    do_peek
    cmp al, 0xd ; \r
    clc
.return:
    ret

parseCRLF:
    do_read
    cmp al, 0xd ; \r
    jne .return
    do_read
    cmp al, 0xa ; \n
    jne .return
    clc
    ret
.return: ; Error case only
    stc
    ret

; Copies data until CR
copyToCR:
    do_peek
    cmp al, 0xd ; \r
    clc
    je .return
    do_copy
    jmp copyToCR
.return:
    ret

parseTags:
    set_parse_buffer tagsBuffer, tagsBufferLen
    mov ah, '@'
    do_parse parseWordIf
    sub_parse_buffer_len tagsBufferLen
.return:
    ret

parsePrefix:
    set_parse_buffer prefixBuffer, prefixBufferLen
    mov ah, ':'
    do_parse parseWordIf
    sub_parse_buffer_len prefixBufferLen
.return:
    ret

parseCmd:
    set_parse_buffer cmdBuffer, cmdBufferLen
    do_parse copyWord
    sub_parse_buffer_len cmdBufferLen
.return:
    ret

; bp is current param in paramsList
parseParams:
    inc word [paramsListLen]
    cmp word [paramsListLen], paramsListMax
    jl .enoughParams
    stc
    ret
.enoughParams:
    mov [bp], di
    mov [bp+2], cx
    do_parse skipSpace
    ; Figure out if this is one long param
    do_peek
    cmp al, ':'
    clc
    je .parseLong
.parseShort:
    do_parse copyWord
    jmp .done
.parseLong:
    do_read ; Skip :
    do_parse copyToCR
.done:
    sub [bp+2], cx
    add bp, paramsListEntryLen
    clc
    do_parse peekCR
    jne parseParams
.return:
    ret

parseMessage:
    do_parse parseTags
    do_parse parsePrefix
    do_parse parseCmd
    mov bp, paramsList
    mov [paramsListLen], word 0
    do_parse parseParams
    do_parse parseCRLF
.return: ; Only used for errors
    ret

; si: string to parse
; cx: string length
; returns cf = set on failure
global doParse
doParse:
    push ax
    push bx
    push di ; used for write buffer
    push bp ; extra variable
    mov bx, cx ; bx is used for read len
    mov di, parseBuffer
    mov cx, parseBufferLenMax
    call parseMessage
    jc .return
    cmp bx, 0
    je .return
    stc
.return:
    pop bp
    pop di
    pop bx
    pop ax
    ret

; ax = param num, starting at 0
; return si and cx, ax
; sets si to NULL 
global paramAt
paramAt: 
    cmp [paramsListLen], ax
    jg .valid
    mov si, 0
    mov cx, 0
    ret
.valid:
    mov si, paramsList
    cmp ax, 0
    jz .done
.loop:
    dec ax
    add si, paramsListEntryLen
    cmp ax, 0
    jnz .loop
.done:
    mov cx, [si+2]
    mov si, [si]
    ret

section CONST2

section DATA

global parseBuffer
global parseBufferLen
global tagsBuffer
global tagsBufferLen
global prefixBuffer
global prefixBufferLen
global cmdBuffer
global cmdBufferLen
global paramsList
global paramsListLen

parseBuffer: resb 512
parseBufferLenMax equ $ - parseBuffer
parseBufferLen: resb 2
tagsBuffer: resb 2
tagsBufferLen: resb 2
prefixBuffer: resb 2
prefixBufferLen: resb 2
cmdBuffer: resb 2
cmdBufferLen: resb 2

paramsListEntryLen equ 4 ; buffer pointer + len
paramsList: resb 16
paramsListMax equ ($ - paramsList) / paramsListEntryLen
paramsListLen: resb 2
