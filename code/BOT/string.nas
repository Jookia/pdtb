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
; - cx: len left in write buffer
; - si: read buffer
; - di: write buffer
; error checking: check that cx isn't full after writing

; writes a single character
global writeChar
writeChar:
    cmp cx, 0
    jz .return
    stosb
    dec cx
.return:
    ret

; writes a string, truncating to smallest value
global writeStr
writeStr:
    cmp cx, bx
    jnc .notrunc
    mov bx, cx
.notrunc:
    sub cx, bx
    xchg bx, cx
    rep movsb
    xchg bx, cx
    ret

; writes an unsigned number
; ax = number
; bx = padding, may be 0. pads up to 5 digits
global writeNum
writeNum:
    push dx ; remainder
    push bp ; div table pointer
    mov si, 4 ; place minus 1
    dec bx ; padding for place minus 1
    mov bp, .divTable
.loop:
    mov dx, 0
    cs div word [bp]
    cmp ax, 0
    jnz .write
    cmp bx, si
    jl .skip
.write:
    add ax, 0x30 ; '0'
    call writeChar
.skip:
    mov ax, dx
    add bp, 2
    dec si
    jnz .loop
    add ax, 0x30 ; '0'
    call writeChar
    pop bp
    pop dx
    mov ax, 0
    mov bx, 0
    mov si, 0
    ret
.divTable:
    dw 10000
    dw 1000
    dw 100
    dw 10

; writes a signed number
; ax = number
; bx = padding, may be 0. pads up to 5 digits
global writeNumSigned
writeNumSigned:
    cmp ax, 0
    jge writeNum
    mov si, ax
    mov ax, 0x2D
    call writeChar
    mov ax, si
    neg ax
    jmp writeNum

section CONST2
section _BSS
