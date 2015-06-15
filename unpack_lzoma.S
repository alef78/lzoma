	.file	"unpack.S"
.data
rdtsc1:	 .word 0
rdtsc2:  .word 0
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
	rdtsc
	movl    %eax,rdtsc1
	movl	28(%esp), %eax # uncompressed bytes
	movl	20(%esp), %edx # input buffer
	decl	%eax
	movl	24(%esp), %edi # edi = output buffer
        pushl   %edi # save output buffer start in stack
        pushl   %eax # (%esp) = bytes left -1
	xor	%ebp, %ebp
	movb    $128,%al
	decl    %ebp # ebp = offset = -1
.copyletter:
	movb	(%edx), %cl   # cl = *src
	inc     %edx          # src++
	movb	%cl, (%edi)   # *dst = cl
	inc     %edi          # dst++
	orl     $-1, %ebx     # ebx = len = -1
	decl	(%esp)    # left--
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
	push %edi # save dst, we need edi as temp register
#.unpack_lz:
	incl	%ebx
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
	movl	breaklz, %esi
	subl	8(%esp), %edi # here (%esp) is pushed edi, 4(%esp) is left
	xorl	%ecx, %ecx
	movl	$1, %ebx
	cmpl	$256, %edi
	movl	%ebx, %ebp
	jle	.loop_ofs
	mov	(%edx), %cl
	mov	$256, %ebx
	inc     %edx
.loop_ofs:
	subl	%ebx, %edi
	cmpl    %edi,%ebx
	jns	.ofs_last_bit
	cmpl	%esi, %ebx
	jl	.L14
	addb    %al,%al
	jnz	.L15
	mov	(%edx), %al
	inc	%edx
	adcb    %al,%al
.L15:
	jnc	.ofs_final_calc
	addl	%ebx, %ebp
	subl	%ebx, %edi
	sall	$3, %esi
.L14:
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
	addl	%ecx, %ebp
	xorl	%ebx, %ebx
	cmpl	$0xD00, %ebp
	sbb     $-1,%ebx
	cmpl	$0x40000, %ebp
	sbb     $-1,%ebx
	negl	%ebp
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
	movl	$1, %edi
	xorl	%ecx, %ecx
.lenloop:
	addl	%edi, %ebx
	addl	%edi, %edi
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
	popl    %edi # restore dst
	movl	%ebp, %esi
	movl	%ebx, %ecx
	addl	%edi, %esi
	rep movsb
	subl	%ebx, (%esp) # left-=len
	jns	.checkleft
.unpack_ret:
        rdtsc
	subl rdtsc1,%eax
	
	popl    %edi # just smallest way to dec esp
	popl    %ebp

	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	unpack, .-unpack
