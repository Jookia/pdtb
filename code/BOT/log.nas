section _TEXT class=CODE
section CONST2 class=DATA group=DGROUP
section DATA class=DATA group=DGROUP
section _BSS class=BSS group=DGROUP
GROUP DGROUP CONST2 DATA _BSS

extern getenv_ ; char* getenv(char *name);

extern writeNum
extern writeChar
extern writeStr

section _TEXT

; int openLog(void)
global openLog_
openLog_:
    push bp
    mov bp, sp
    push es
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
    pop es
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

; ax = tag
; bx = tag len
; si = line
; cx = length
; returns nothing
global logGeneral
logGeneral:
    push ax
    push si
    push bx
    push cx
    push di
    push si
    push cx
    push ax
    push bx
    mov di, logBuffer
    mov cx, logBufferLen
    call createTimestamp
    mov ax, 0x20 ; SPACE
    call writeChar
    pop bx
    pop si
    call writeStr
    mov ax, 0x20 ; SPACE
    call writeChar
    pop bx
    pop si
    call writeStr
    mov ax, 0x0D ; CARRIAGE RETURn
    call writeChar
    mov ax, 0x0A ; NEW LINE
    call writeChar
    xchg cx, bx
    mov cx, logBufferLen
    sub cx, bx
    mov ax, logBuffer
    call writeLog
    pop di
    pop cx
    pop bx
    pop si
    pop ax
    ret

; input/output: cx/di
createTimestamp:
    push bp
    mov bp, sp
    sub sp, 16
    ; save output string
    mov [bp-14], di
    mov [bp-16], cx
    ; get time
    mov ax, 0x2C00
    int 0x21
    mov ah, 0
    mov al, dh
    mov [bp-2], ax ; second
    mov al, cl
    mov [bp-4], ax ; minute
    mov al, ch
    mov [bp-6], ax ; hour
    ; get date
    mov ax, 0x2A00
    int 0x21
    mov ah, 0
    mov al, dl
    mov [bp-8], ax ; day
    mov al, dh
    mov [bp-10], ax ; month
    mov [bp-12], cx ; year
    mov bx, 4
    ; load output string
    mov di, [bp-14]
    mov cx, [bp-16]
    ; write year
    mov ax, [bp-12]
    call writeNum
    mov ax, '-'
    call writeChar
    ; write month
    mov bx, 2
    mov ax, [bp-10]
    call writeNum
    mov ax, '-'
    call writeChar
    ; write day
    mov bx, 2
    mov ax, [bp-8]
    call writeNum
    mov ax, 'T'
    call writeChar
    ; write hour
    mov bx, 2
    mov ax, [bp-6]
    call writeNum
    mov ax, ':'
    call writeChar
    ; write minute
    mov bx, 2
    mov ax, [bp-4]
    call writeNum
    mov ax, ':'
    call writeChar
    ; write second
    mov bx, 2
    mov ax, [bp-2]
    call writeNum
    mov sp, bp
    pop bp
    ret

section CONST2

botlogEnv: db "BOT_LOG",0x00

section _BSS

logHandle: resb 2

logBuffer: resb 512
logBufferLen equ $ - logBuffer
