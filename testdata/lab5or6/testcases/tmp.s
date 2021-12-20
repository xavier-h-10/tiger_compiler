.text
.globl nop
.type nop, @function
.set nop_framesize, 8
nop:
subq $8, %rsp
L12:
movq %rbx, t104
leaq nop_framesize(%rsp), t131 
movq t131, t105
movq %r12, t106
movq %r13, t107
movq %r14, t108
movq %r15, t109
leaq nop_framesize(%rsp), t133 
movq $-8, t134
movq t133, t132
addq t134, t132
movq %rdi, (t132)
leaq L0(%rip), t136
movq t136, %rdi
callq print
movq %rax, t135
movq t135, %rax
movq t104, %rbx
leaq nop_framesize(%rsp), t137 
movq t105, t137
movq t106, %r12
movq t107, %r13
movq t108, %r14
movq t109, %r15
jmp L11
L11:


addq $8, %rsp
retq
.size nop, .-nop
.globl init
.type init, @function
.set init_framesize, 24
init:
subq $24, %rsp
L14:
movq %rbx, t110
leaq init_framesize(%rsp), t138 
movq t138, t111
movq %r12, t112
movq %r13, t113
movq %r14, t114
movq %r15, t115
leaq init_framesize(%rsp), t140 
movq $-8, t141
movq t140, t139
addq t141, t139
movq %rdi, (t139)
movq $0, t142
leaq init_framesize(%rsp), t144 
movq $-16, t145
movq t144, t143
addq t145, t143
movq t142, (t143)
leaq init_framesize(%rsp), t149 
movq $-8, t150
movq t149, t148
addq t150, t148
movq (t148), t151
movq $-8, t152
movq t151, t147
addq t152, t147
movq (t147), t153
movq $1, t154
movq t153, t146
subq t154, t146
leaq init_framesize(%rsp), t156 
movq $-24, t157
movq t156, t155
addq t157, t155
movq t146, (t155)
L1:
leaq init_framesize(%rsp), t159 
movq $-16, t160
movq t159, t158
addq t160, t158
movq (t158), t161
leaq init_framesize(%rsp), t163 
movq $-24, t164
movq t163, t162
addq t164, t162
movq (t162), t165
cmp t165, t161
jle L2
L3:
movq $0, t166
movq t166, %rax
movq t110, %rbx
leaq init_framesize(%rsp), t167 
movq t111, t167
movq t112, %r12
movq t113, %r13
movq t114, %r14
movq t115, %r15
jmp L13
L2:
movq %rax, t170
movq %rdx, t171
leaq init_framesize(%rsp), t173 
movq $-16, t174
movq t173, t172
addq t174, t172
movq (t172), t175
movq $2, t176
movq t175, %rax
imulq t176
movq %rax, t169
movq t170, %rax
movq t171, %rdx
movq $1, t177
movq t169, t168
addq t177, t168
leaq init_framesize(%rsp), t181 
movq $-8, t182
movq t181, t180
addq t182, t180
movq (t180), t183
movq $-16, t184
movq t183, t179
addq t184, t179
movq (t179), t185
movq %rax, t187
movq %rdx, t188
leaq init_framesize(%rsp), t190 
movq $-16, t191
movq t190, t189
addq t191, t189
movq (t189), t192
movq $8, t193
movq t192, %rax
imulq t193
movq %rax, t186
movq t187, %rax
movq t188, %rdx
movq t185, t178
addq t186, t178
movq t168, (t178)
leaq init_framesize(%rsp), t196 
movq $-8, t197
movq t196, t195
addq t197, t195
movq (t195), t198
movq t198, %rdi
callq nop
movq %rax, t194
leaq init_framesize(%rsp), t201 
movq $-16, t202
movq t201, t200
addq t202, t200
movq (t200), t203
movq $1, t204
movq t203, t199
addq t204, t199
leaq init_framesize(%rsp), t206 
movq $-16, t207
movq t206, t205
addq t207, t205
movq t199, (t205)
jmp L1
L13:


addq $24, %rsp
retq
.size init, .-init
.globl bsearch
.type bsearch, @function
.set bsearch_framesize, 8
bsearch:
subq $8, %rsp
L16:
movq %rbx, t119
leaq bsearch_framesize(%rsp), t208 
movq t208, t120
movq %r12, t121
movq %r13, t122
movq %r14, t123
movq %r15, t124
leaq bsearch_framesize(%rsp), t210 
movq $-8, t211
movq t210, t209
addq t211, t209
movq %rdi, (t209)
movq %rsi, t101
movq %rdx, t102
movq %rcx, t103
cmp t102, t101
je L4
L5:
movq %rax, t213
movq %rdx, t214
movq t101, t215
addq t102, t215
movq $2, t216
movq t215, %rax
idivq t216
movq %rax, t212
movq t213, %rax
movq t214, %rdx
movq t212, t116
leaq bsearch_framesize(%rsp), t220 
movq $-8, t221
movq t220, t219
addq t221, t219
movq (t219), t222
movq $-16, t223
movq t222, t218
addq t223, t218
movq (t218), t224
movq %rax, t226
movq %rdx, t227
movq $8, t228
movq t116, %rax
imulq t228
movq %rax, t225
movq t226, %rax
movq t227, %rdx
movq t224, t217
addq t225, t217
movq (t217), t229
cmp t103, t229
jl L6
L7:
leaq bsearch_framesize(%rsp), t232 
movq $-8, t233
movq t232, t231
addq t233, t231
movq (t231), t234
movq t234, %rdi
movq t101, %rsi
movq t116, %rdx
movq t103, %rcx
callq bsearch
movq %rax, t230
movq t230, t117
L8:
movq t117, t118
L9:
movq t118, %rax
movq t119, %rbx
leaq bsearch_framesize(%rsp), t235 
movq t120, t235
movq t121, %r12
movq t122, %r13
movq t123, %r14
movq t124, %r15
jmp L15
L4:
movq t101, t118
jmp L9
L6:
leaq bsearch_framesize(%rsp), t238 
movq $-8, t239
movq t238, t237
addq t239, t237
movq (t237), t240
movq t240, %rdi
movq $1, t242
movq t116, t241
addq t242, t241
movq t241, %rsi
movq t102, %rdx
movq t103, %rcx
callq bsearch
movq %rax, t236
movq t236, t117
jmp L8
L15:


addq $8, %rsp
retq
.size bsearch, .-bsearch
.globl try
.type try, @function
.set try_framesize, 8
try:
subq $8, %rsp
L18:
movq %rbx, t125
leaq try_framesize(%rsp), t244 
movq t244, t126
movq %r12, t127
movq %r13, t128
movq %r14, t129
movq %r15, t130
leaq try_framesize(%rsp), t246 
movq $-8, t247
movq t246, t245
addq t247, t245
movq %rdi, (t245)
leaq try_framesize(%rsp), t250 
movq $-8, t251
movq t250, t249
addq t251, t249
movq (t249), t252
movq t252, %rdi
callq init
movq %rax, t248
leaq try_framesize(%rsp), t255 
movq $-8, t256
movq t255, t254
addq t256, t254
movq (t254), t257
movq t257, %rdi
movq $0, t258
movq t258, %rsi
leaq try_framesize(%rsp), t262 
movq $-8, t263
movq t262, t261
addq t263, t261
movq (t261), t264
movq $-8, t265
movq t264, t260
addq t265, t260
movq (t260), t266
movq $1, t267
movq t266, t259
subq t267, t259
movq t259, %rdx
movq $7, t268
movq t268, %rcx
callq bsearch
movq %rax, t253
movq t253, t243
movq t243, %rdi
callq printi
movq %rax, t269
leaq L10(%rip), t271
movq t271, %rdi
callq print
movq %rax, t270
movq t270, %rax
movq t125, %rbx
leaq try_framesize(%rsp), t272 
movq t126, t272
movq t127, %r12
movq t128, %r13
movq t129, %r14
movq t130, %r15
jmp L17
L17:


addq $8, %rsp
retq
.size try, .-try
.globl tigermain
.type tigermain, @function
.set tigermain_framesize, 16
tigermain:
subq $16, %rsp
L20:
movq $16, t274
leaq tigermain_framesize(%rsp), t276 
movq $-8, t277
movq t276, t275
addq t277, t275
movq t274, (t275)
leaq tigermain_framesize(%rsp), t279 
movq $-16, t280
movq t279, t278
addq t280, t278
movq t278, t273
leaq tigermain_framesize(%rsp), t283 
movq $-8, t284
movq t283, t282
addq t284, t282
movq (t282), t285
movq t285, %rdi
movq $0, t286
movq t286, %rsi
callq init_array
movq %rax, t281
movq t281, t100
movq t100, (t273)
leaq tigermain_framesize(%rsp), t288 
movq t288, %rdi
callq try
movq %rax, t287
jmp L19
L19:


addq $16, %rsp
retq
.size tigermain, .-tigermain
.section .rodata
L0:
.long 0
.string ""
L10:
.long 1
.string "\n"
