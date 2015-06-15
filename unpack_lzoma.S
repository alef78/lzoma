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
	movl	28(%esp), %edx # uncompressed bytes num
	movl	20(%esp), %esi # input buffer
	decl	%edx
	movl	24(%esp), %edi # edi = output buffer
        pushl   %edi # save output buffer start in stack
	xorl	%ebp, %ebp
	movl    $0x80000000,%eax
	incl    %ebp # ebp = offset = -1
.copyletter:
	movsb
	#movb	(%esi), %cl   # cl = *src
	#inc     %esi          # src++
	#movb	%cl, (%edi)   # *dst = cl
	#inc     %edi          # dst++
	orl     $-1, %ebx     # ebx = len = -1
	decl	%edx    # left--
	js	.unpack_ret
.checkleft: 
	addl    %eax,%eax
	jnz	.nonextbit
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.nonextbit:
.checkifletterorlz:
	jnc	.copyletter
.unpack_lz:
	incl	%ebx
	push %edx # save left
	jnz	.load_ofs
	addl    %eax,%eax
	jnz	.L9
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.L9:
	# ebx==0 at this point
	jnc	.load_len
.load_ofs:
	push %edi # save dst, we need edi as temp register
	xor     %ebp,%ebp
	movzxb	(%esi), %ecx
	mov	$256, %ebx
	subl	8(%esp), %edi # here (%esp) is pushed edi, 4(%esp) is left

        inc     %ebp
	inc     %esi
	cmpl	%ebx, %edi
	#movl	breaklz, %edx
	#movl    $256,%edx
	movl    %ebx,%edx
	jnle	.loop_ofs
	movl	%ebp, %ebx
	xorl	%ecx, %ecx
	dec     %esi
.loop_ofs:
	subl	%ebx, %edi
	cmpl    %edi,%ebx
	jns	.ofs_last_bit
	cmpl	%edx, %ebx
	jle	.notbreakofs
	addl    %eax,%eax
	jnz	.L15
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.L15:
	jnc	.ofs_final_calc
	addl	%ebx, %ebp
	sall	$3, %edx
	subl	%ebx, %edi
.notbreakofs:
	addl	%ebx, %edi
	addl    %eax,%eax
	jnz	.L18
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.L18:
	adcl    %ecx,%ecx
	addl	%ebx, %ebx
	jmp	.loop_ofs
.ofs_last_bit:
	subl    %edi,%ebx
	cmpl	%ebx, %ecx
	jl	.ofs_final_calc
	addl    %eax,%eax
	jnz	.L21
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
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
	addl    %eax,%eax
	jnz	.len2b
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.len2b:
	jc	.lenlonger
	addl    %eax,%eax
	jnz	.len2b2
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.len2b2:
	adcl    $2,%ebx
	jmp	.copy_match

.lenlonger:
	addl	$3, %ebx
	movl	$1, %edx
	xorl	%ecx, %ecx
.lenloop:
	addl	%edx, %ebx
	addl	%edx, %edx
	addl    %eax,%eax
	jnz	.L26
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.L26:
	adcl    %ecx,%ecx

	addl    %eax,%eax
	jnz	.L29
	#mov	(%esi), %eax
	#lea	4(%esi),%esi
	lodsl
	adcl    %eax,%eax
.L29:
	jc	.lenloop
	addl	%ecx, %ebx
.copy_match:
        pushl    %esi
	movl	%edi, %esi
	movl	%ebx, %ecx
	subl	%ebp, %esi
	rep movsb
	popl    %esi
	popl    %edx
	subl	%ebx, %edx # left-=len
	jns	.checkleft
.unpack_ret:
	popl    %edi # just smallest way to dec esp

	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	unpack, .-unpack
