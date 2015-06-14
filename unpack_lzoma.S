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
	#movl    %edx,rdtsc2
	leal	-12(%esp), %esp
	movl	40(%esp), %eax
	movl	32(%esp), %edx
	decl	%eax
	movl	36(%esp), %edi
	movl	%eax, (%esp)
	xorl	%eax, %eax
	decl    %eax
	movl	%eax, 8(%esp)
	movb    $128,%al
	movb    (%edx),%ah
.copyletter:
	#movb	(%edx), %cl   # cl = *src
	inc     %edx          # src++
	movb	%ah, (%edi)   # *dst = cl
	inc     %edi          # dst++
	movb    (%edx),%ah
	orl     $-1, %ebx     # ebx = len = -1
	decl	(%esp)    # left--
	js	.unpack_ret
.checkleft:
	addb    %al,%al
	jnz	.nonextbit
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.nonextbit:
.checkifletterorlz:
	jnc	.copyletter
	movl	%edi, 4(%esp) # dst = edi
.unpack_lz:
	incl	%ebx
	jnz	.load_ofs
	addb    %al,%al
	jnz	.L9
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L9:
	# ebx==0 at this point
	jnc	.load_len
.load_ofs:
	movl	breaklz, %esi
	subl	$out_buf, %edi
	xorl	%ecx, %ecx
	cmpl	$255, %edi
	movl	%ecx, 8(%esp)
	movl	$1, %ebx
	jle	.loop_ofs
	movb	%ah, %cl
	mov	$256, %ebx
	inc     %edx
	movb    (%edx),%ah
.loop_ofs:
	subl	%ebx, %edi
	movl	%ebx, %ebp
	subl	%edi, %ebp
	jns	.ofs_last_bit
	cmpl	%esi, %ebx
	jl	.L14
	addb    %al,%al
	jnz	.L15
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L15:
	jnc	.ofs_final_calc
	addl	%ebx, 8(%esp)
	subl	%ebx, %edi
	sall	$3, %esi
.L14:
	addl	%ebx, %edi
	addl	%ebx, %ebx
	addb    %al,%al
	jnz	.L18
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L18:
	adcl    %ecx,%ecx
	jmp	.loop_ofs
.ofs_last_bit:
	cmpl	%ebp, %ecx
	movl	%edx, %ebx
	jl	.ofs_final_calc
	#addl	%ecx, %edi
	addb    %al,%al
	jnz	.L21
	mov	%ah, %al
	incl	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L21:
	#cmovc	%edi, %ecx
	jnc     .ofs_final_calc
	addl    %edi,%ecx
.ofs_final_calc:
	movl	8(%esp), %esi
	xorl	%ebx, %ebx
	leal	1(%esi,%ecx), %edi
	cmpl	$3328, %edi
	setge	%bl
	negl	%edi
	movl	%edi, 8(%esp)
.load_len:
	addb    %al,%al
	jnz	.len2b
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.len2b:
	jc	.lenlonger
	addb    %al,%al
	jnz	.len2b2
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.len2b2:
	adcl    $2,%ebx
	jmp	.copy_match

.lenlonger:
	addl	$4, %ebx
	movl	$1, %edi
	xorl	%ecx, %ecx
	movl	(%esp), %esi
	cmpl	%edi, %esi
	jle	.copy_match
	cmpl	$2, %esi
	je	.L38
.L32:
	addl	%edi, %edi
	subl	%edi, %esi
	addb    %al,%al
	jnz	.L26
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L26:
	adcl    %ecx,%ecx

	movl	%edi, %ebp # ebp = edi
	subl	%esi, %ebp
	jns	.L28
	addb    %al,%al
	jnz	.L29
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L29:
	jnc	.L31
	addl	%edi, %ebx
	jmp	.L32
.L28:
	cmpl	%ebp, %ecx
	jl	.L31
.L38:
	#addl	%ecx, %esi
	addb    %al,%al
	jnz	.L33
	mov	%ah, %al
	inc	%edx
	adcb    %al,%al
	movb    (%edx),%ah
.L33:
	#cmovc	%esi, %ecx
	jnc     .L31
	addl    %esi,%ecx
.L31:
	addl	%ecx, %ebx
.copy_match:
	movl	4(%esp), %edi
	movl	8(%esp), %esi
	movl	%ebx, %ecx
	addl	%edi, %esi
	rep movsb
	subl	%ebx, (%esp) # left-=len
	movl	%edi, 4(%esp)
	jns	.checkleft
.unpack_ret:
        rdtsc
	subl rdtsc1,%eax
	#sbbl rdtsc2,%edx
	
	leal	12(%esp), %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	unpack, .-unpack
