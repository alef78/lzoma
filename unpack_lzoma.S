	.file	"unpack.S"
	.section	.text.unlikely,"ax",@progbits
	.text
	.align 16
	.globl	unpack
	.type	unpack, @function
unpack:
	pushl	%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	28(%esp), %esi # uncompressed bytes num
	movl	20(%esp), %edx # input buffer
	decl	%esi
	movl	24(%esp), %edi # edi = output buffer
        pushl   %edi # save output buffer start in stack
	xorl	%ebp, %ebp
	movb    $128,%al
	incl    %ebp # ebp = offset = -1
.copyletter:
	movb	(%edx), %cl   # cl = *src
	inc     %edx          # src++
	movb	%cl, (%edi)   # *dst = cl
	orl     $-1, %ebx     # ebx = len = -1
	inc     %edi          # dst++
	decl	%esi    # left--
	js	.unpack_ret
.checkleft: 
	addb    %al,%al
	jnz	.nonextbit
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.nonextbit:
.checkifletterorlz:
	jnc	.copyletter
.unpack_lz:
	incl	%ebx
	push %esi # save left
	jnz	.load_ofs
	addb    %al,%al
	jnz	.L9
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.L9:
	# ebx==0 at this point
	jnc	.load_len
.load_ofs:
	push %edi # save dst, we need edi as temp register
	xor     %ebp,%ebp
	movzxb	(%edx), %ecx
	mov	$256, %ebx
	subl	8(%esp), %edi # here (%esp) is pushed edi, 4(%esp) is left

        inc     %ebp
	inc     %edx
	cmpl	%ebx, %edi
	#movl	breaklz, %esi
	#movl    $256,%esi
	movl    %ebx,%esi
	#movb    $2,%ah
	jnle	.loop_ofs
	#movb    $10,%ah
	movl	%ebp, %ebx
	xorl	%ecx, %ecx
	dec     %edx
.loop_ofs:
	subl	%ebx, %edi
	cmpl    %edi,%ebx
	jns	.ofs_last_bit
	cmpl	%esi, %ebx
	#dec     %ah
	#jnz	.notbreakofs
	jle	.notbreakofs
	addb    %al,%al
	jnz	.L15
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.L15:
	jnc	.ofs_final_calc
	addl	%ebx, %ebp
	sall	$3, %esi
	#mov     $3,%ah
	subl	%ebx, %edi
.notbreakofs:
	addl	%ebx, %edi
	addb    %al,%al
	jnz	.L18
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.L18:
	adcl    %ecx,%ecx
	addl	%ebx, %ebx
	jmp	.loop_ofs
.ofs_last_bit:
	subl    %edi,%ebx
	cmpl	%ebx, %ecx
	jl	.ofs_final_calc
	addb    %al,%al
	jnz	.L21
	mov	(%edx), %al
	incl	%edx
	adcb    %al,%al
.L21:
	jnc     .ofs_final_calc
	addl    %edi,%ecx
.ofs_final_calc:
	popl    %edi # restore dst
	addl	%ecx, %ebp
	cmpl	$0xD00, %ebp
	sbb     %ebx,%ebx
	cmpl	$0x40000, %ebp
	sbb     $-2,%ebx
	#negl	%ebp
.load_len:
	addb    %al,%al
	jnz	.len2b
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.len2b:
	jc	.lenlonger
	addb    %al,%al
	jnz	.len2b2
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.len2b2:
	adcl    $2,%ebx
	jmp	.copy_match

.lenlonger:
	addl	$3, %ebx
	movl	$1, %esi
	xorl	%ecx, %ecx
.lenloop:
	addl	%esi, %ebx
	addl	%esi, %esi
	addb    %al,%al
	jnz	.L26
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.L26:
	adcl    %ecx,%ecx

	addb    %al,%al
	jnz	.L29
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.L29:
	jc	.lenloop
	addl	%ecx, %ebx
.copy_match:
	movl	%edi, %esi
	movl	%ebx, %ecx
	subl	%ebp, %esi
	rep movsb
	popl    %esi
	subl	%ebx, %esi # left-=len
	jns	.checkleft
.unpack_ret:
	popl    %edi # just smallest way to dec esp

	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	unpack, .-unpack
