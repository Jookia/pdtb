section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

extern snprintf_ ; int sprintf(char *str, size_t size, const char *format, ...)
extern getenv_ ; char* getenv(char *name);

section _TEXT

; int openLog(void)
global openLog_
openLog_:
    push bp
    mov bp, sp
    push bx
    push cx
    push dx
    mov ax, botlogEnv
    call getenv_
    cmp ax, 0
    jne .haveEnv
    mov ax, 5
    jmp .done
.haveEnv:
    mov dx, ax
    mov ax, 0x5B00
    mov cx, 0
    int 0x21
    jnc .haveFile
    cmp ax, 0x50 ; already exists
    jne .done
    mov ax, 0x3D01
    int 0x21
    jc .done ; couldn't open existing file
.haveFile:
    ; ax is file handle
    mov bx, ax
    mov [logHandle], bx
    mov ax, 0x4202
    mov cx, 0
    mov dx, 0
    int 0x21
    ; ignore failure, we tried our best
    mov ax, 0
.done:
    pop dx
    pop cx
    pop bx
    pop bp
    ret

; void closeLog(void)
global closeLog_
closeLog_:
    push bp
    push bx
    mov bp, sp
    mov bx, [logHandle]
    mov ax, 0x3E00
    int 0x21
    pop bx
    pop bp
    ret

; int printf(const char *format, ...)
global logPrintf_
logPrintf_:
    pop ax
    mov [printfRet], ax
    mov [printfCx], cx
    mov [printfSi], si
    push logBufferLen
    push logBuffer
    call snprintfNice
    add sp, 4
    mov si, ax
    call logGeneral
    mov cx, [printfCx]
    mov si, [printfSi]
    jmp [printfRet]

; ax = line
; cx = len
writeLog:
    push ax
    push bx
    push dx
    mov dx, ax
    mov bx, [logHandle]
    mov ax, 0x4000
    int 21h
    mov bx, 1
    mov ax, 0x4000
    int 21h
    pop dx
    pop bx
    pop ax
    ret

writeSpace:
    push ax
    push cx
    push 0x20
    mov ax, sp
    mov cx, 1
    call writeLog
    add sp, 2
    pop cx
    pop ax
    ret

; arguments on stack according to snprintf
; returns ax = buffer
; returns cx = safe count
snprintfNice:
    pop cx ; ret
    call snprintf_
    push cx
    push bp
    mov bp, sp
    mov cx, ax
    cmp cx, 0
    jg .didWrite
    mov cx, 0
    jmp .noTruncate
.didWrite:
    mov ax, [bp+6]  ; snprintf length
    cmp cx, ax
    jl .noTruncate
    mov cx, ax
.noTruncate: 
    mov ax, [bp+4] ; snprintf buffer
    pop bp
    ret

; si = line
; cx = length
; returns nothing
global logGeneral
logGeneral:
    push ax
    push bx
    push dx
    push cx
    call createTimestamp
    call writeLog
    call writeSpace
    pop cx
    mov ax, si
    call writeLog
    pop dx
    pop bx
    pop ax
    ret

; si = line
; cx = length
; returns nothing
global logIncoming
logIncoming:
    push ax
    push cx
    push bp
    mov bp, sp
    push cx
    call createTimestamp
    call writeLog
    call writeSpace
    pop cx
    push si
    push cx
    push cx
    push printFormatIncoming
    push logBufferLen
    push logBuffer
    call snprintfNice
    call writeLog
    mov sp, bp
    pop bp
    pop cx
    pop ax
    ret

; si = line
; cx = length
; returns nothing
global logOutgoing
logOutgoing:
    push ax
    push cx
    push bp
    mov bp, sp
    push cx
    call createTimestamp
    call writeLog
    call writeSpace
    pop cx
    push si
    push cx
    push cx
    push printFormatOutgoing
    push logBufferLen
    push logBuffer
    call snprintfNice
    call writeLog
    mov sp, bp
    pop bp
    pop cx
    pop ax
    ret

; returns null-terminated time in ax, cx
createTimestamp:
    push dx
    push bp
    mov bp, sp
    mov ax, 0x2C00
    int 0x21
    mov ah, 0
    mov al, dh
    push ax
    mov al, cl
    push ax
    mov al, ch
    push ax
    mov ax, 0x2A00
    int 0x21
    mov ah, 0
    mov al, dh
    push ax
    mov al, dl
    push ax
    push cx
    push timeFormat
    push timeBufferLen
    push timeBuffer
    call snprintfNice
    mov sp, bp
    pop bp
    pop dx
    ret

section CONST2

printFormatIncoming: db "INCOMING %i: %.*s",0x00
printFormatOutgoing: db "OUTGOING %i: %.*s",0x00
timeFormat: db "%.4u-%.2u-%.2uT%.2u:%.2u:%.2u",0x00
botlogEnv: db "BOT_LOG",0x00

section _BSS

logHandle: resb 2

timeBuffer: resb 20
timeBufferLen equ $ - timeBuffer

logBuffer: resb 512
logBufferLen equ $ - logBuffer

printfRet: resb 2
printfCx: resb 2
printfSi: resb 2
