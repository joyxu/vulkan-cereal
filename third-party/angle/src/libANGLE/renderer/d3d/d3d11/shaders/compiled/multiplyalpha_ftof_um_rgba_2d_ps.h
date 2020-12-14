#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 9.29.952.3111
//
//
//   fxc /nologo /E PS_FtoF_UM_RGBA_2D /T ps_4_0 /Fh
//    compiled\multiplyalpha_ftof_um_rgba_2d_ps.h MultiplyAlpha.hlsl
//
//
// Resource Bindings:
//
// Name                                 Type  Format         Dim Slot Elements
// ------------------------------ ---------- ------- ----------- ---- --------
// Sampler                           sampler      NA          NA    0        1
// TextureF                          texture  float4          2d    0        1
//
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue Format   Used
// -------------------- ----- ------ -------- -------- ------ ------
// SV_POSITION              0   xyzw        0      POS  float
// TEXCOORD                 0   xy          1     NONE  float   xy
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue Format   Used
// -------------------- ----- ------ -------- -------- ------ ------
// SV_TARGET                0   xyzw        0   TARGET  float   xyzw
//
ps_4_0
dcl_sampler s0, mode_default
dcl_resource_texture2d (float,float,float,float) t0
dcl_input_ps linear v1.xy
dcl_output o0.xyzw
dcl_temps 2
sample r0.xyzw, v1.xyxx, t0.xyzw, s0
lt r1.x, l(0.000000), r0.w
div r1.yzw, r0.xxyz, r0.wwww
movc o0.xyz, r1.xxxx, r1.yzwy, r0.xyzx
mov o0.w, r0.w
ret
// Approximately 6 instruction slots used
#endif

const BYTE g_PS_FtoF_UM_RGBA_2D[] = {
    68,  88,  66,  67,  214, 130, 152, 233, 185, 254, 78,  247, 13,  195, 81,  12, 126, 13,  130,
    154, 1,   0,   0,   0,   200, 2,   0,   0,   5,   0,   0,   0,   52,  0,   0,  0,   220, 0,
    0,   0,   52,  1,   0,   0,   104, 1,   0,   0,   76,  2,   0,   0,   82,  68, 69,  70,  160,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   2,   0,   0,   0,   28, 0,   0,   0,
    0,   4,   255, 255, 0,   1,   0,   0,   109, 0,   0,   0,   92,  0,   0,   0,  3,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   1,   0,
    0,   0,   1,   0,   0,   0,   100, 0,   0,   0,   2,   0,   0,   0,   5,   0,  0,   0,   4,
    0,   0,   0,   255, 255, 255, 255, 0,   0,   0,   0,   1,   0,   0,   0,   13, 0,   0,   0,
    83,  97,  109, 112, 108, 101, 114, 0,   84,  101, 120, 116, 117, 114, 101, 70, 0,   77,  105,
    99,  114, 111, 115, 111, 102, 116, 32,  40,  82,  41,  32,  72,  76,  83,  76, 32,  83,  104,
    97,  100, 101, 114, 32,  67,  111, 109, 112, 105, 108, 101, 114, 32,  57,  46, 50,  57,  46,
    57,  53,  50,  46,  51,  49,  49,  49,  0,   171, 171, 73,  83,  71,  78,  80, 0,   0,   0,
    2,   0,   0,   0,   8,   0,   0,   0,   56,  0,   0,   0,   0,   0,   0,   0,  1,   0,   0,
    0,   3,   0,   0,   0,   0,   0,   0,   0,   15,  0,   0,   0,   68,  0,   0,  0,   0,   0,
    0,   0,   0,   0,   0,   0,   3,   0,   0,   0,   1,   0,   0,   0,   3,   3,  0,   0,   83,
    86,  95,  80,  79,  83,  73,  84,  73,  79,  78,  0,   84,  69,  88,  67,  79, 79,  82,  68,
    0,   171, 171, 171, 79,  83,  71,  78,  44,  0,   0,   0,   1,   0,   0,   0,  8,   0,   0,
    0,   32,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   3,   0,   0,  0,   0,   0,
    0,   0,   15,  0,   0,   0,   83,  86,  95,  84,  65,  82,  71,  69,  84,  0,  171, 171, 83,
    72,  68,  82,  220, 0,   0,   0,   64,  0,   0,   0,   55,  0,   0,   0,   90, 0,   0,   3,
    0,   96,  16,  0,   0,   0,   0,   0,   88,  24,  0,   4,   0,   112, 16,  0,  0,   0,   0,
    0,   85,  85,  0,   0,   98,  16,  0,   3,   50,  16,  16,  0,   1,   0,   0,  0,   101, 0,
    0,   3,   242, 32,  16,  0,   0,   0,   0,   0,   104, 0,   0,   2,   2,   0,  0,   0,   69,
    0,   0,   9,   242, 0,   16,  0,   0,   0,   0,   0,   70,  16,  16,  0,   1,  0,   0,   0,
    70,  126, 16,  0,   0,   0,   0,   0,   0,   96,  16,  0,   0,   0,   0,   0,  49,  0,   0,
    7,   18,  0,   16,  0,   1,   0,   0,   0,   1,   64,  0,   0,   0,   0,   0,  0,   58,  0,
    16,  0,   0,   0,   0,   0,   14,  0,   0,   7,   226, 0,   16,  0,   1,   0,  0,   0,   6,
    9,   16,  0,   0,   0,   0,   0,   246, 15,  16,  0,   0,   0,   0,   0,   55, 0,   0,   9,
    114, 32,  16,  0,   0,   0,   0,   0,   6,   0,   16,  0,   1,   0,   0,   0,  150, 7,   16,
    0,   1,   0,   0,   0,   70,  2,   16,  0,   0,   0,   0,   0,   54,  0,   0,  5,   130, 32,
    16,  0,   0,   0,   0,   0,   58,  0,   16,  0,   0,   0,   0,   0,   62,  0,  0,   1,   83,
    84,  65,  84,  116, 0,   0,   0,   6,   0,   0,   0,   2,   0,   0,   0,   0,  0,   0,   0,
    2,   0,   0,   0,   2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  1,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,
    0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   1,   0,   0,   0,   0,  0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0};
