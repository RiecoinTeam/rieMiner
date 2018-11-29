dnl  AMD64 mpn_mod_1s_4p

dnl  Contributed to the GNU project by Torbjorn Granlund.

dnl  Copyright 2009-2012, 2014 Free Software Foundation, Inc.

dnl  This file is derived from the GNU MP Library.
dnl
dnl  The GNU MP Library is free software; you can redistribute it and/or modify
dnl  it under the terms of either:
dnl
dnl    * the GNU Lesser General Public License as published by the Free
dnl      Software Foundation; either version 3 of the License, or (at your
dnl      option) any later version.
dnl
dnl  or
dnl
dnl    * the GNU General Public License as published by the Free Software
dnl      Foundation; either version 2 of the License, or (at your option) any
dnl      later version.
dnl
dnl  or both in parallel, as here.
dnl
dnl  The GNU MP Library is distributed in the hope that it will be useful, but
dnl  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
dnl  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl  for more details.
dnl
dnl  You should have received copies of the GNU General Public License and the
dnl  GNU Lesser General Public License along with the GNU MP Library.  If not,
dnl  see https://www.gnu.org/licenses/.

define(`R32',
        `ifelse($1,`%rax',`%eax',
                $1,`%rbx',`%ebx',
                $1,`%rcx',`%ecx',
                $1,`%rdx',`%edx',
                $1,`%rsi',`%esi',
                $1,`%rdi',`%edi',
                $1,`%rbp',`%ebp',
                $1,`%r8',`%r8d',
                $1,`%r9',`%r9d',
                $1,`%r10',`%r10d',
                $1,`%r11',`%r11d',
                $1,`%r12',`%r12d',
                $1,`%r13',`%r13d',
                $1,`%r14',`%r14d',
                $1,`%r15',`%r15d')')
define(`R8',
        `ifelse($1,`%rax',`%al',
                $1,`%rbx',`%bl',
                $1,`%rcx',`%cl',
                $1,`%rdx',`%dl',
                $1,`%rsi',`%sil',
                $1,`%rdi',`%dil',
                $1,`%rbp',`%bpl',
                $1,`%r8',`%r8b',
                $1,`%r9',`%r9b',
                $1,`%r10',`%r10b',
                $1,`%r11',`%r11b',
                $1,`%r12',`%r12b',
                $1,`%r13',`%r13b',
                $1,`%r14',`%r14b',
                $1,`%r15',`%r15b')')

define(`PROLOGUE',
`       .globl   $1
        .type	$1,function
$1:
')

define(C, `
dnl')

define(`L',
`.L$1')

define(ALIGN,
`.align eval($1), 0x90')

C	     cycles/limb
C AMD K8,K9	 3
C AMD K10	 3
C Intel P4	15.5
C Intel core2	 5
C Intel corei	 4
C Intel atom	23
C VIA nano	 4.75

C Compute a % p for 4 values of p, where a is arbitrary length > 2^256 and p < 2^32
C
C On entry:
C rdi: uint64_t* a
C rsi: uint64_t  n (length of a in 64-bit limbs)
C rdx: uint32_t* ps (array of 4 p values, each shifted left by clz(p))
C rcx: uint32_t  cnt (the shift amount referenced above, which must be the same for each p)
C r8:  uint64_t* cps (array of 4 precomputed invert_limb values correspeonding to each p)
C
C During operation:
C r11, xmm11: B1modb
C rbx, xmm12: B2modb
C r10, xmm13: B3modb
C r13, xmm14: B4modb
C r12, xmm15: B5modb
C xmm0, xmm1: ps
C xmm2, xmm3: cps
C xmm4: cnt

	.text
	ALIGN(16)
PROLOGUE(rie_mod_1s_4p_4times)

	vpcmpeqd	%xmm15, %xmm15, %xmm15
	vpsrld		$31, %xmm15, %xmm14
	vpsllq		$32, %xmm14, %xmm14

	shl	$1, %rsi
	xor	%rax, %rax
	cmp	%rax, -4(%rdi,%rsi,4)
	jnz	L(even)
	sub	$1, %rsi
L(even):

	mov	$32, %r9
	sub	%rcx, %r9
	vpmovzxdq	(%rdx), %xmm0
	vpmovzxdq	8(%rdx), %xmm1
	vmovdqu		(%r8), %xmm2
	vmovdqu		16(%r8), %xmm3
	vpsrlq		$32, %xmm2, %xmm2
	vpsrlq		$32, %xmm3, %xmm3
	vmovq		%rcx, %xmm4
	vmovq		%r9, %xmm5
	vpor		%xmm14, %xmm2, %xmm6
	vpor		%xmm14, %xmm3, %xmm7
	vpsrlq		%xmm5, %xmm6, %xmm6	C ((bi >> (GMP_LIMB_BITS-cnt)) | (CNST_LIMB(1) << cnt));
	vpsrlq		%xmm5, %xmm7, %xmm7
	vpxor		%xmm5, %xmm5, %xmm5
	vpsubd		%xmm0, %xmm5, %xmm8	C -b
	vpsubd		%xmm1, %xmm5, %xmm9
	vpmulld		%xmm8, %xmm6, %xmm6
	vpmulld		%xmm9, %xmm7, %xmm7
	vshufps		$136, %xmm7, %xmm6, %xmm11  
	vpsrld		%xmm4, %xmm11, %xmm11	C B1modb

C xmm6, xmm7: B1modb
C xmm8, xmm9: Q
C xmm15: all 1s

	vpmuludq	%xmm2, %xmm6, %xmm8
	vpmuludq	%xmm3, %xmm7, %xmm9
	vpsrlq		$32, %xmm8, %xmm5
	vpsrlq		$32, %xmm9, %xmm10
	vpaddd		%xmm6, %xmm5, %xmm5
	vpaddd		%xmm7, %xmm10, %xmm10
	vpxor		%xmm15, %xmm5, %xmm5
	vpxor		%xmm15, %xmm10, %xmm10
	vpmulld		%xmm0, %xmm5, %xmm5
	vpmulld		%xmm1, %xmm10, %xmm10
	vpmaxud		%xmm8, %xmm5, %xmm6
	vpmaxud		%xmm9, %xmm10, %xmm7
	vpcmpeqd	%xmm5, %xmm6, %xmm6
	vpcmpeqd	%xmm10, %xmm7, %xmm7
	vpand		%xmm0, %xmm6, %xmm6
	vpand		%xmm1, %xmm7, %xmm7
	vpaddd		%xmm6, %xmm5, %xmm6
	vpaddd		%xmm7, %xmm10, %xmm7
	vshufps		$136, %xmm7, %xmm6, %xmm12  
	vpsrld		%xmm4, %xmm12, %xmm12	C B2modb

	vpmuludq	%xmm2, %xmm6, %xmm8
	vpmuludq	%xmm3, %xmm7, %xmm9
	vpsrlq		$32, %xmm8, %xmm5
	vpsrlq		$32, %xmm9, %xmm10
	vpaddd		%xmm6, %xmm5, %xmm5
	vpaddd		%xmm7, %xmm10, %xmm10
	vpxor		%xmm15, %xmm5, %xmm5
	vpxor		%xmm15, %xmm10, %xmm10
	vpmulld		%xmm0, %xmm5, %xmm5
	vpmulld		%xmm1, %xmm10, %xmm10
	vpmaxud		%xmm8, %xmm5, %xmm6
	vpmaxud		%xmm9, %xmm10, %xmm7
	vpcmpeqd	%xmm5, %xmm6, %xmm6
	vpcmpeqd	%xmm10, %xmm7, %xmm7
	vpand		%xmm0, %xmm6, %xmm6
	vpand		%xmm1, %xmm7, %xmm7
	vpaddd		%xmm6, %xmm5, %xmm6
	vpaddd		%xmm7, %xmm10, %xmm7
	vshufps		$136, %xmm7, %xmm6, %xmm13  
	vpsrld		%xmm4, %xmm13, %xmm13	C B3modb

	vpmuludq	%xmm2, %xmm6, %xmm8
	vpmuludq	%xmm3, %xmm7, %xmm9
	vpsrlq		$32, %xmm8, %xmm5
	vpsrlq		$32, %xmm9, %xmm10
	vpaddd		%xmm6, %xmm5, %xmm5
	vpaddd		%xmm7, %xmm10, %xmm10
	vpxor		%xmm15, %xmm5, %xmm5
	vpxor		%xmm15, %xmm10, %xmm10
	vpmulld		%xmm0, %xmm5, %xmm5
	vpmulld		%xmm1, %xmm10, %xmm10
	vpmaxud		%xmm8, %xmm5, %xmm6
	vpmaxud		%xmm9, %xmm10, %xmm7
	vpcmpeqd	%xmm5, %xmm6, %xmm6
	vpcmpeqd	%xmm10, %xmm7, %xmm7
	vpand		%xmm0, %xmm6, %xmm6
	vpand		%xmm1, %xmm7, %xmm7
	vpaddd		%xmm6, %xmm5, %xmm6
	vpaddd		%xmm7, %xmm10, %xmm7
	vshufps		$136, %xmm7, %xmm6, %xmm14  
	vpsrld		%xmm4, %xmm14, %xmm14	C B4modb

	vpmuludq	%xmm2, %xmm6, %xmm8
	vpmuludq	%xmm3, %xmm7, %xmm9
	vpsrlq		$32, %xmm8, %xmm5
	vpsrlq		$32, %xmm9, %xmm10
	vpaddd		%xmm6, %xmm5, %xmm5
	vpaddd		%xmm7, %xmm10, %xmm10
	vpxor		%xmm15, %xmm5, %xmm5
	vpxor		%xmm15, %xmm10, %xmm10
	vpmulld		%xmm0, %xmm5, %xmm5
	vpmulld		%xmm1, %xmm10, %xmm10
	vpmaxud		%xmm8, %xmm5, %xmm6
	vpmaxud		%xmm9, %xmm10, %xmm7
	vpcmpeqd	%xmm5, %xmm6, %xmm6
	vpcmpeqd	%xmm10, %xmm7, %xmm7
	vpand		%xmm0, %xmm6, %xmm6
	vpand		%xmm1, %xmm7, %xmm7
	vpaddd		%xmm6, %xmm5, %xmm6
	vpaddd		%xmm7, %xmm10, %xmm7
	vshufps		$136, %xmm7, %xmm6, %xmm15  
	vpsrld		%xmm4, %xmm15, %xmm15	C B5modb

	mov     R32(%rsi), R32(%r10)
	and     $3, R32(%r10)
	je      L(b0)
	cmp     $2, R32(%r10)
	jc      L(b1)
	je      L(b2)

L(b3):	lea	-12(%rdi,%rsi,4), %rdi
	vmovd		4(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm11, %xmm2
	vpsrldq		$8, %xmm11, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

	vmovd		(%rdi), %xmm2
	vpshufd		$68, %xmm2, %xmm2
	vmovdqa		%xmm2, %xmm3
	vpaddq		%xmm0, %xmm2, %xmm6
	vpaddq		%xmm1, %xmm3, %xmm7
	
	vmovd		8(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm12, %xmm2
	vpsrldq		$8, %xmm12, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1
	
	jmp	L(m0)

	ALIGN(8)
L(b0):	lea	-16(%rdi,%rsi,4), %rdi
	vmovd		4(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm11, %xmm2
	vpsrldq		$8, %xmm11, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

	vmovd		(%rdi), %xmm2
	vpshufd		$68, %xmm2, %xmm2
	vmovdqa		%xmm2, %xmm3
	vpaddq		%xmm0, %xmm2, %xmm4
	vpaddq		%xmm1, %xmm3, %xmm5
	
	vmovd		8(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm12, %xmm2
	vpsrldq		$8, %xmm12, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

	vpaddq		%xmm0, %xmm4, %xmm6
	vpaddq		%xmm1, %xmm5, %xmm7

	vmovd		12(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm13, %xmm2
	vpsrldq		$8, %xmm13, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

	jmp	L(m0)

	ALIGN(8)
L(b1):	lea	-4(%rdi,%rsi,4), %rdi
	vmovd		(%rdi), %xmm2
	vpshufd         $68, %xmm2, %xmm4
	vmovdqa         %xmm4, %xmm5
	jmp	L(m1)

	ALIGN(8)
L(b2):	lea	-8(%rdi,%rsi,4), %rdi
	vmovq		(%rdi), %xmm2
	vpshufd         $68, %xmm2, %xmm4
	vmovdqa         %xmm4, %xmm5
	jmp	L(m1)

C xmm6, xmm7: ph/pl
C xmm4, xmm5: rh/rl

	ALIGN(16)
L(top):	vmovd		-12(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm11, %xmm2
	vpsrldq		$8, %xmm11, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

	vmovd		-16(%rdi), %xmm2
	vpshufd		$68, %xmm2, %xmm2
	vmovdqa		%xmm2, %xmm3
	vpaddq		%xmm0, %xmm2, %xmm6
	vpaddq		%xmm1, %xmm3, %xmm7
	
	vmovd		-8(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm12, %xmm2
	vpsrldq		$8, %xmm12, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

	vpaddq		%xmm0, %xmm6, %xmm6
	vpaddq		%xmm1, %xmm7, %xmm7

	vmovd		-4(%rdi), %xmm0
	vpshufd		$0, %xmm0, %xmm0
	vmovdqa		%xmm0, %xmm1
	vpmovzxdq	%xmm13, %xmm2
	vpsrldq		$8, %xmm13, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1
	sub	$16, %rdi

	vpaddq		%xmm0, %xmm6, %xmm6
	vpaddq		%xmm1, %xmm7, %xmm7

	vpmovzxdq	%xmm14, %xmm2
	vpsrldq		$8, %xmm14, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpmuludq	%xmm4, %xmm2, %xmm0
	vpmuludq	%xmm5, %xmm3, %xmm1
	vpaddq		%xmm0, %xmm6, %xmm6
	vpaddq		%xmm1, %xmm7, %xmm7

	vpmovzxdq	%xmm15, %xmm2
	vpsrldq		$8, %xmm15, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpsrlq		$32, %xmm4, %xmm0
	vpsrlq		$32, %xmm5, %xmm1
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1

L(m0):	vpaddq		%xmm0, %xmm6, %xmm4
	vpaddq		%xmm1, %xmm7, %xmm5
L(m1):	sub	$4, %rsi
	ja	L(top)

L(end):	vpmovzxdq	%xmm11, %xmm2
	vpsrldq		$8, %xmm11, %xmm3
	vpmovzxdq	%xmm3, %xmm3
	vpsrlq		$32, %xmm4, %xmm0
	vpsrlq		$32, %xmm5, %xmm1
	vpmuludq	%xmm0, %xmm2, %xmm0
	vpmuludq	%xmm1, %xmm3, %xmm1
	vpcmpeqd	%xmm2, %xmm2, %xmm2
	vpsrlq		$32, %xmm2, %xmm2
	vpand		%xmm2, %xmm1, %xmm3 C cl
	vpand		%xmm2, %xmm0, %xmm2
	vblendps	$10, %xmm0, %xmm4, %xmm4 C rh from multiplication result
	vblendps	$10, %xmm1, %xmm5, %xmm5 C rl stays same as before
	vpaddq		%xmm2, %xmm4, %xmm4
	vpaddq		%xmm3, %xmm5, %xmm5

	vmovq		%rcx, %xmm0
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsrld		$31, %xmm1, %xmm1
	vpsllq		$32, %xmm1, %xmm1
	vpsllq		%xmm0, %xmm4, %xmm4
	vpsllq		%xmm0, %xmm5, %xmm5
	vpsrlq		$32, %xmm4, %xmm6 C xmm4/xmm5 have rl
	vpsrlq		$32, %xmm5, %xmm7 C xmm6/xmm7 have r

	vmovdqu		(%r8), %xmm2
	vmovdqu		16(%r8), %xmm3
	vpsrlq		$32, %xmm2, %xmm2
	vpsrlq		$32, %xmm3, %xmm3

	vpmuludq	%xmm6, %xmm2, %xmm2
	vpmuludq	%xmm7, %xmm3, %xmm3 C xmm2/xmm3 have qh/ql
	
	vpaddd		%xmm1, %xmm4, %xmm4
	vpaddd		%xmm1, %xmm5, %xmm5
	vpaddq		%xmm4, %xmm2, %xmm2
	vpaddq		%xmm5, %xmm3, %xmm3

	vpsrlq		$32, %xmm2, %xmm6
	vpsrlq		$32, %xmm3, %xmm7
	vpmovzxdq	(%rdx), %xmm0
	vpmovzxdq	8(%rdx), %xmm1
	vpmulld		%xmm0, %xmm6, %xmm6
	vpmulld		%xmm1, %xmm7, %xmm7
	vpsubd		%xmm6, %xmm4, %xmm4
	vpsubd		%xmm7, %xmm5, %xmm5
	vpmaxud		%xmm2, %xmm4, %xmm6
	vpmaxud		%xmm3, %xmm5, %xmm7
	vpcmpeqd	%xmm4, %xmm6, %xmm6
	vpcmpeqd	%xmm5, %xmm7, %xmm7
	vpand		%xmm0, %xmm6, %xmm6
	vpand		%xmm1, %xmm7, %xmm7
	vpaddd		%xmm6, %xmm4, %xmm4
	vpaddd		%xmm7, %xmm5, %xmm5
	vpmaxud		%xmm4, %xmm0, %xmm6
	vpmaxud		%xmm5, %xmm1, %xmm7
	vpcmpeqd	%xmm0, %xmm6, %xmm6
	vpcmpeqd	%xmm1, %xmm7, %xmm7
	vpandn		%xmm0, %xmm6, %xmm6
	vpandn		%xmm1, %xmm7, %xmm7
	vpsubd		%xmm6, %xmm4, %xmm4
	vpsubd		%xmm7, %xmm5, %xmm5

	vmovd		%xmm4, R32(%rax)
	ret