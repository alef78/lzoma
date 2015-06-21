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
	lodsl
	adcl    %eax,%eax
.L9:
	# ebx==0 at this point
	jnc	.load_len
.load_ofs:
	push %edi # save dst, we need edi as temp register
	xor     %ebp,%ebp # ofs=0
	movzxb	(%esi), %ecx # res=*src
	mov	$512, %ebx # x=256
	subl	8(%esp), %edi # here (%esp) is pushed edi, 4(%esp) is left
        inc     %ebp # ofs++
	inc     %esi
	#movl	$48, %edx
	lea     47(%ebp),%edx
	cmp     $652630,%edi
	ja      .low
	mov     $60,%dl
	cmp     $49549,%edi
	ja      .low
	mov     $80,%dl
.low:
.loop_ofs:
	addl    %edi,%edx # top+=total
	cmpl    %edx,%ebx # cmp total+top,x
	jns     .ofs_last_bit
	subl    %edi,%edx # top-=total
	test    $0x055ffc00,%ebx
	jz      .noshift
	leal    (%edx,%edx,8),%edx # top*=9
	shrl    $3,%edx  # top>>=3
.noshift:
	cmp     %edx,%ecx # cmp top,res
	jl      .ofs_final_calc # if res<top
	subl    %edx,%ebp # ofs-=top
	addl    %edx,%edi # total+=top
	addl    %edx,%edx # top+=top
	addl    %eax,%eax
	jnz	.L18
	lodsl
	adcl    %eax,%eax
.L18:
	adcl    %ecx,%ecx
	addl	%ebx, %ebx # x+=x
	jmp	.loop_ofs

.ofs_last_bit:
	subl    %edi,%ebx # x-=total
	cmpl	%ebx, %ecx # cmp x,res
	jl	.ofs_final_calc # if res<x goto final
	addl    %eax,%eax
	jnz	.L21
	lodsl
	adcl    %eax,%eax
.L21:
	adcl    %ecx,%ecx
	subl    %ebx,%ecx # res-=x
.ofs_final_calc:
	popl    %edi # restore dst
	addl	%ecx, %ebp # ofs+=res
	cmpl	$5400, %ebp
	sbb     %ebx,%ebx
	cmpl	$0x60000, %ebp
	sbb     $-2,%ebx
.load_len:
	addl    %eax,%eax
	jnz	.len2b
	lodsl
	adcl    %eax,%eax
.len2b:
	jc	.lenlonger
	addl    %eax,%eax
	jnz	.len2b2
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
	lodsl
	adcl    %eax,%eax
.L26:
	adcl    %ecx,%ecx

	addl    %eax,%eax
	jnz	.L29
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
