	.file	"asm_gen.cpp"
	.intel_syntax noprefix
# GNU C++20 (Ubuntu 13.3.0-6ubuntu2~24.04) version 13.3.0 (x86_64-linux-gnu)
#	compiled by GNU C version 13.3.0, GMP version 6.3.0, MPFR version 4.2.1, MPC version 1.3.1, isl version isl-0.26-GMP

# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed: -march=znver4 -mmmx -mpopcnt -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -msse4a -mno-fma4 -mno-xop -mfma -mbmi -mbmi2 -maes -mpclmul -mavx512vl -mavx512dq -mavx512cd -mno-avx512er -mno-avx512pf -mavx512vbmi -mavx512ifma -mno-avx5124vnniw -mno-avx5124fmaps -mavx512vpopcntdq -mavx512vbmi2 -mgfni -mvpclmulqdq -mavx512vnni -mavx512bitalg -mno-avx512vp2intersect -mno-3dnow -madx -mabm -mno-cldemote -mclflushopt -mclwb -mclzero -mcx16 -mno-enqcmd -mf16c -mfsgsbase -mfxsr -mno-hle -msahf -mno-lwp -mlzcnt -mmovbe -mno-movdir64b -mno-movdiri -mmwaitx -mno-pconfig -mpku -mno-prefetchwt1 -mprfchw -mno-ptwrite -mrdpid -mrdrnd -mrdseed -mno-rtm -mno-serialize -mno-sgx -msha -mshstk -mno-tbm -mno-tsxldtrk -mvaes -mno-waitpkg -mwbnoinvd -mxsave -mxsavec -mxsaveopt -mxsaves -mno-amx-tile -mno-amx-int8 -mno-amx-bf16 -mno-uintr -mno-hreset -mno-kl -mno-widekl -mno-avxvnni -mno-avx512fp16 -mno-avxifma -mno-avxvnniint8 -mno-avxneconvert -mno-cmpccxadd -mno-amx-fp16 -mno-prefetchi -mno-raoint -mno-amx-complex --param=l1-cache-size=32 --param=l1-cache-line-size=64 --param=l2-cache-size=1024 -mtune=znver4 -mavx2 -mavx512f -mavx512bw -mavx512bf16 -masm=intel -O3 -std=c++20 -ffast-math -fasynchronous-unwind-tables -fstack-protector-strong -fstack-clash-protection -fcf-protection
	.text
	.p2align 4
	.globl	franklin::int32_pipeline_loop(int const*, int const*, int*, unsigned long)
	.type	franklin::int32_pipeline_loop(int const*, int const*, int*, unsigned long), @function
franklin::int32_pipeline_loop(int const*, int const*, int*, unsigned long):
.LFB10600:
	.cfi_startproc
	endbr64	
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:20:     while (offset + step <= num_elements) {
	cmp	rcx, 7	# num_elements,
	jbe	.L5	#,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:20:     while (offset + step <= num_elements) {
	mov	eax, 8	# _5,
	.p2align 4
	.p2align 3
.L3:
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avx2intrin.h:121:   return (__m256i) ((__v8su)__A + (__v8su)__B);
	vmovdqa	ymm1, YMMWORD PTR -32[rsi+rax*4]	# tmp100, MEM[(const __m256i * {ref-all})snd_ptr_12(D) + -32B + _23 * 4]
	vpaddd	ymm0, ymm1, YMMWORD PTR -32[rdi+rax*4]	# tmp93, tmp100, MEM[(const __m256i * {ref-all})fst_ptr_10(D) + -32B + _23 * 4]
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avxintrin.h:923:   *__P = __A;
	vmovdqa	YMMWORD PTR -32[rdx+rax*4], ymm0	# MEM[(__m256i * {ref-all})out_ptr_18(D) + -32B + _23 * 4], tmp93
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:20:     while (offset + step <= num_elements) {
	add	rax, 8	# _5,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:20:     while (offset + step <= num_elements) {
	cmp	rcx, rax	# num_elements, _5
	jnb	.L3	#,
	vzeroupper
.L5:
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:40: }
	ret	
	.cfi_endproc
.LFE10600:
	.size	franklin::int32_pipeline_loop(int const*, int const*, int*, unsigned long), .-franklin::int32_pipeline_loop(int const*, int const*, int*, unsigned long)
	.p2align 4
	.globl	franklin::bf16_pipeline_loop(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long)
	.type	franklin::bf16_pipeline_loop(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long), @function
franklin::bf16_pipeline_loop(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long):
.LFB10601:
	.cfi_startproc
	endbr64	
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:51:     while (offset + step <= num_elements) {
	cmp	rcx, 7	# num_elements,
	jbe	.L11	#,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:51:     while (offset + step <= num_elements) {
	mov	eax, 8	# _5,
	.p2align 4
	.p2align 3
.L9:
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avx2intrin.h:463:   return (__m256i) __builtin_ia32_pmovsxwd256 ((__v8hi)__X);
	vpmovsxwd	ymm1, XMMWORD PTR -16[rdi+rax*2]	# MEM[(const __m128i * {ref-all})fst_ptr_10(D) + -16B + _30 * 2], MEM[(const __m128i * {ref-all})fst_ptr_10(D) + -16B + _30 * 2]
	vpmovsxwd	ymm0, XMMWORD PTR -16[rsi+rax*2]	# MEM[(const __m128i * {ref-all})snd_ptr_12(D) + -16B + _30 * 2], MEM[(const __m128i * {ref-all})snd_ptr_12(D) + -16B + _30 * 2]
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avx2intrin.h:684:   return (__m256i)__builtin_ia32_pslldi256 ((__v8si)__A, __B);
	vpslld	ymm1, ymm1, 16	# tmp102, MEM[(const __m128i * {ref-all})fst_ptr_10(D) + -16B + _30 * 2],
	vpslld	ymm0, ymm0, 16	# tmp106, MEM[(const __m128i * {ref-all})snd_ptr_12(D) + -16B + _30 * 2],
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avxintrin.h:149:   return (__m256) ((__v8sf)__A + (__v8sf)__B);
	vaddps	ymm0, ymm0, ymm1	# tmp109, tmp106, tmp102
# /home/remoteaccess/code/franklin/container/column.hpp:210:     __m128bh result = _mm256_cvtneps_pbh(fp32_reg);
	vcvtneps2bf16	xmm0, ymm0	# tmp108, tmp109
# /usr/lib/gcc/x86_64-linux-gnu/13/include/emmintrin.h:736:   *__P = __B;
	vmovdqa	XMMWORD PTR -16[rdx+rax*2], xmm0	# MEM[(__m128i * {ref-all})out_ptr_18(D) + -16B + _30 * 2], tmp108
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:51:     while (offset + step <= num_elements) {
	add	rax, 8	# _5,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:51:     while (offset + step <= num_elements) {
	cmp	rcx, rax	# num_elements, _5
	jnb	.L9	#,
	vzeroupper
.L11:
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:71: }
	ret	
	.cfi_endproc
.LFE10601:
	.size	franklin::bf16_pipeline_loop(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long), .-franklin::bf16_pipeline_loop(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long)
	.p2align 4
	.globl	franklin::test_int32_inline(int const*, int const*, int*, unsigned long)
	.type	franklin::test_int32_inline(int const*, int const*, int*, unsigned long), @function
franklin::test_int32_inline(int const*, int const*, int*, unsigned long):
.LFB10604:
	.cfi_startproc
	endbr64	
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:82:     while (offset + step <= num_elements) {
	cmp	rcx, 7	# n,
	jbe	.L16	#,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:82:     while (offset + step <= num_elements) {
	mov	eax, 8	# _11,
	.p2align 4
	.p2align 3
.L14:
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avx2intrin.h:121:   return (__m256i) ((__v8su)__A + (__v8su)__B);
	vmovdqa	ymm1, YMMWORD PTR -32[rdi+rax*4]	# tmp100, MEM[(const __m256i * {ref-all})fst_2(D) + -32B + _24 * 4]
	vpaddd	ymm0, ymm1, YMMWORD PTR -32[rsi+rax*4]	# tmp93, tmp100, MEM[(const __m256i * {ref-all})snd_3(D) + -32B + _24 * 4]
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avxintrin.h:923:   *__P = __A;
	vmovdqa	YMMWORD PTR -32[rdx+rax*4], ymm0	# MEM[(__m256i * {ref-all})out_4(D) + -32B + _24 * 4], tmp93
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:82:     while (offset + step <= num_elements) {
	add	rax, 8	# _11,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:82:     while (offset + step <= num_elements) {
	cmp	rcx, rax	# n, _11
	jnb	.L14	#,
	vzeroupper
.L16:
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:119: }
	ret	
	.cfi_endproc
.LFE10604:
	.size	franklin::test_int32_inline(int const*, int const*, int*, unsigned long), .-franklin::test_int32_inline(int const*, int const*, int*, unsigned long)
	.p2align 4
	.globl	franklin::test_bf16_inline(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long)
	.type	franklin::test_bf16_inline(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long), @function
franklin::test_bf16_inline(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long):
.LFB10605:
	.cfi_startproc
	endbr64	
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:102:     while (offset + step <= num_elements) {
	cmp	rcx, 7	# n,
	jbe	.L21	#,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:102:     while (offset + step <= num_elements) {
	mov	eax, 8	# _11,
	.p2align 4
	.p2align 3
.L19:
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avx2intrin.h:463:   return (__m256i) __builtin_ia32_pmovsxwd256 ((__v8hi)__X);
	vpmovsxwd	ymm1, XMMWORD PTR -16[rdi+rax*2]	# MEM[(const __m128i * {ref-all})fst_2(D) + -16B + _31 * 2], MEM[(const __m128i * {ref-all})fst_2(D) + -16B + _31 * 2]
	vpmovsxwd	ymm0, XMMWORD PTR -16[rsi+rax*2]	# MEM[(const __m128i * {ref-all})snd_3(D) + -16B + _31 * 2], MEM[(const __m128i * {ref-all})snd_3(D) + -16B + _31 * 2]
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avx2intrin.h:684:   return (__m256i)__builtin_ia32_pslldi256 ((__v8si)__A, __B);
	vpslld	ymm1, ymm1, 16	# tmp102, MEM[(const __m128i * {ref-all})fst_2(D) + -16B + _31 * 2],
	vpslld	ymm0, ymm0, 16	# tmp106, MEM[(const __m128i * {ref-all})snd_3(D) + -16B + _31 * 2],
# /usr/lib/gcc/x86_64-linux-gnu/13/include/avxintrin.h:149:   return (__m256) ((__v8sf)__A + (__v8sf)__B);
	vaddps	ymm0, ymm0, ymm1	# tmp109, tmp106, tmp102
# /home/remoteaccess/code/franklin/container/column.hpp:210:     __m128bh result = _mm256_cvtneps_pbh(fp32_reg);
	vcvtneps2bf16	xmm0, ymm0	# tmp108, tmp109
# /usr/lib/gcc/x86_64-linux-gnu/13/include/emmintrin.h:736:   *__P = __B;
	vmovdqa	XMMWORD PTR -16[rdx+rax*2], xmm0	# MEM[(__m128i * {ref-all})out_4(D) + -16B + _31 * 2], tmp108
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:102:     while (offset + step <= num_elements) {
	add	rax, 8	# _11,
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:102:     while (offset + step <= num_elements) {
	cmp	rcx, rax	# n, _11
	jnb	.L19	#,
	vzeroupper
.L21:
# /home/remoteaccess/code/franklin/container/asm_gen.cpp:125: }
	ret	
	.cfi_endproc
.LFE10605:
	.size	franklin::test_bf16_inline(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long), .-franklin::test_bf16_inline(franklin::bf16 const*, franklin::bf16 const*, franklin::bf16*, unsigned long)
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
