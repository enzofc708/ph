#include "nds/g3d/sbc.h"
#include "nds/gfx.h"

extern void func_020056b4();
extern void func_02018298();
extern G3d_Texture *G3d_GetTexture(G3d_ContainerHeader *filePtr);
extern u32 G3d_TexGetBlock1LenSHR3(G3d_Texture *tex);
extern u32 G3d_TexGetBlock2LenSHR3(G3d_Texture *tex);
extern u32 G3d_TexGetBlock4LenSHR3(G3d_Texture *tex);
extern u32 (*data_02057514)(u32, u32, u32);
extern u32 (*data_0205750c)(u32, u32, u32);
extern u32 (*data_02057510)(u32);
extern u32 (*data_02057518)(u32);
extern void func_02018cd8(G3d_Texture *tex, u32 var1, u32 var2);
extern void func_02018db0(G3d_Texture *tex, u32 var1);
extern void func_02018cec(G3d_Texture *tex, u32 var1);
extern void func_02018db8(G3d_Texture *tex, u32 var1);
extern unk32 func_0201e4bc(G3d_ContainerHeader *filePtr);
extern u32 func_02019380(unk32 param1, const G3d_Texture *tex);

const s8 G3D_SbcCmdLen[256] = {1,  1,  3,  2,  2,  2,  4,  2,  2,  0,  9,  1,  3,  3,  -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, 2,  -1, 5,  3,  3,  0,  -1, 1,  -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, 2,  -1, 5,  3,  3,  -1, -1, -1, -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, -1, -1, 6,  4,  4,  -1, -1, -1, -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

void G3d_GetCurrentMtx(Mat4x3p *mtx1, Mat3p *mtx2) {
    FlushGfxQueue();

    GFX_FIFO_MTX_MODE     = 0;
    GFX_FIFO_MTX_PUSH     = 0;
    GFX_FIFO_MTX_IDENTITY = 0;

    if (mtx1) {
        Mat4p tmp;
        while (func_0200598c(&tmp))
            ;
        Mat4p_CopyToMat4x3p(&tmp, mtx1);
    }

    if (mtx2) {
        while (func_020059bc(mtx2))
            ;
    }

    GFX_FIFO_MTX_POP  = 1;
    GFX_FIFO_MTX_MODE = 2;
}

THUMB void G3D_func_0201b248(void) {
    func_020056b4();
    func_02018298();
    REG_GFX_STAT = REG_GFX_STAT & 0x3fffffff | 0x80000000; // GFX FIFO IRQ = Empty
}

ARM int G3D_GetSbcCmdLength(const u8 *c) {
    int cmdLen;

    cmdLen = G3D_SbcCmdLen[*c];

    if (cmdLen < 0) {
        return -1;
    } else if (cmdLen == 0) {
        if (*c == G3D_SBC_CMD_SKN) {
            return *(c + 2) * 3 + 3;
        } else {
            return -1;
        }
    } else {
        return cmdLen;
    }
}

const u8 *G3D_FindSbcCmd(const u8 *currentCmd, u8 cmd) {
    int x;

    cmd &= G3D_SBC_CMD_MASK;
    while ((x = (*currentCmd & G3D_SBC_CMD_MASK)) != G3D_SBC_CMD_END) {
        if (cmd == x) {
            return currentCmd;
        } else {
            int cmdLen = G3D_GetSbcCmdLength(currentCmd);
            currentCmd += cmdLen;
        }
    }
    return NULL;
}

const u8 *G3D_GetParentBoneId(int *parentId, const u8 *cmd, u32 boneId) {
    const u8 *tmp = cmd;

    while ((tmp = G3D_FindSbcCmd(tmp, G3D_SBC_CMD_MTP))) {
        if (*(tmp + 1) == boneId) {
            *parentId = *(tmp + 2);
            return tmp;
        }

        {
            int cmdLen = G3D_GetSbcCmdLength(tmp);
            tmp += cmdLen;
        }
    }
    return NULL;
}

int G3D_GetChildBonesIds(u8 *ids, const u8 *cmd, u32 boneId) {
    const u8 *tmp = cmd;
    int num       = 0;

    while ((tmp = G3D_FindSbcCmd(tmp, G3D_SBC_CMD_MTP))) {
        if (*(tmp + 2) == boneId && *(tmp + 2) != *(tmp + 1)) {
            *(ids + num++) = *(tmp + 1);
        }

        {
            int cmdLen = G3D_GetSbcCmdLength(tmp);
            tmp += cmdLen;
        }
    }
    return num;
}

u32 G3D_func_0201b3c4(void *filePtr) {
    u8 *stamp  = (u8 *) filePtr;
    u32 failed = 0;

    switch (*(u32 *) &stamp[0]) {
        case '0XTB': // BTX0
        case '0DMB': // BMD0
        {
            G3d_Texture *tex;
            u32 lenB1, lenB2, lenB4;
            u32 var11 = 1;
            u32 var21 = 1;
            u32 var31 = 1;
            u32 var1, var2, var3;

            tex = G3d_GetTexture((G3d_ContainerHeader *) filePtr);
            if (tex) {
                lenB1 = G3d_TexGetBlock1LenSHR3(tex);
                lenB2 = G3d_TexGetBlock2LenSHR3(tex);
                lenB4 = G3d_TexGetBlock4LenSHR3(tex);

                if (lenB1 > 0) {
                    var1 = (*data_02057514)(lenB1, 0, 0);
                    if (var1 == 0) {
                        var11 = 0;
                    }
                } else {
                    var1 = 0;
                }

                if (lenB2 > 0) {
                    var2 = (*data_02057514)(lenB2, 1, 0);
                    if (var2 == 0) {
                        var21 = 0;
                    }
                } else {
                    var2 = 0;
                }

                if (lenB4 > 0) {
                    var3 = (*data_0205750c)(lenB4, tex->mUnk_20 & 0x8000, 0);
                    if (var3 == 0) {
                        var31 = 0;
                    }
                } else {
                    var3 = 0;
                }

                if (!var11 || !var21 || !var31) {
                    (*data_02057510)(var3);
                    (*data_02057518)(var2);
                    (*data_02057518)(var1);
                    return 0;
                }

                func_02018cd8(tex, var1, var2);
                func_02018db0(tex, var3);
                func_02018cec(tex, 1);
                func_02018db8(tex, 1);
            }

            if (*(u32 *) &stamp[0] == '0DMB') {
                unk32 mdlSet = func_0201e4bc((G3d_ContainerHeader *) filePtr);
                if (tex) {
                    (void) func_02019380(mdlSet, tex);
                }
            }
        }
            return 1;
            break;
        case '0ACB': // BCA0
        case '0AVB': // BVA0
        case '0AMB': // BMA0
        case '0PTB': // BTP0
        case '0ATB': // BTA0
            return 1;
            break;
        default:
            return 0;
            break;
    };
}

// s32 G3d_WorldToLocalPos(const Vec3p* worldPos, int* px, int* py) {

//     const Mat4p* proj;
//     const Mat4x3p* camera;
//     Vec3p tmp;
//     Vec3p vec;
//     q20 w;
//     q32 invW;
//     int x1, y1, x2, y2;
//     int dx, dy;
//     int rval;

//     proj = NNS_G3dGlbGetProjectionMtx();
//     camera = NNS_G3dGlbGetCameraMtx();

//     MTX_MultVec43(pWorld, camera, &tmp);

//     w = (fx32)(((fx64)tmp.x * proj->_03 +
//                 (fx64)tmp.y * proj->_13 +
//                 (fx64)tmp.z * proj->_23) >> FX32_SHIFT);
//     w += proj->_33;

//     FX_InvAsync(w);

//     vec.x = (fx32)(((fx64)tmp.x * proj->_00 +
//                     (fx64)tmp.y * proj->_10 +
//                     (fx64)tmp.z * proj->_20) >> FX32_SHIFT);
//     vec.x += proj->_30;

//     vec.y = (fx32)(((fx64)tmp.x * proj->_01 +
//                     (fx64)tmp.y * proj->_11 +
//                     (fx64)tmp.z * proj->_21) >> FX32_SHIFT);
//     vec.y += proj->_31;

//     invW = FX_GetInvResultFx64c();

//     // conversion to normalized screen coordinate type
//     vec.x = (FX_Mul32x64c(vec.x, invW) + FX32_ONE) / 2;
//     vec.y = (FX_Mul32x64c(vec.y, invW) + FX32_ONE) / 2;

//     // the local coordinate origin is being converted to a normalized screen coordinate type
//     if (vec.x < 0 || vec.y < 0 || vec.x > FX32_ONE || vec.y > FX32_ONE)
//     {
//         // When off-screen
//         rval = -1;
//     }
//     else
//     {
//         rval = 0;
//     }

//     NNS_G3dGlbGetViewPort(&x1, &y1, &x2, &y2);
//     dx = x2 - x1;
//     dy = y2 - y1;

//     *px = x1 + ((vec.x * dx + FX32_HALF) >> FX32_SHIFT);
//     *py = 191 - y1 - ((vec.y * dy + FX32_HALF) >> FX32_SHIFT);

//     return rval;
// }