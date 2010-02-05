/*===========================================
        GRRLIB 4.0.0
        Code     : NoNameNo
        Additional Code : Crayon
        GX hints : RedShade

Download and Help Forum : http://grrlib.santo.fr
===========================================*/

// Modified by oggzee

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "libpng/pngu/pngu.h"
//#include "lib/libjpeg/jpeglib.h"
#include "GRRLIB.h"
#include <fat.h> 


#define DEFAULT_FIFO_SIZE (256 * 1024)

u32 fb = 0;
static void *xfb[2] = { NULL, NULL};
GXRModeObj *rmode;
void *gp_fifo = NULL;


/**
 * Clear screen with a specific color.
 * @param color the color to use to fill the screen.
 */
inline void GRRLIB_FillScreen(u32 color) {
    GRRLIB_Rectangle(-40, -40, 680, 520, color, 1);
}

/**
 * Draw a dot.
 * @param x specifies the x-coordinate of the dot.
 * @param y specifies the y-coordinate of the dot.
 * @param color the color of the dot.
 */
inline void GRRLIB_Plot(f32 x, f32 y, u32 color) {
    Vector v[] = {{x,y,0.0f}};

    GRRLIB_NPlot(v, color, 1);
}

/**
 *
 * @param v
 * @param color
 * @param n
 */
void GRRLIB_NPlot(Vector v[], u32 color, long n) {
    GRRLIB_GXEngine(v, color, n, GX_POINTS);
}

/**
 * Draw a line.
 * @param x1 start point for line for the x coordinate.
 * @param y1 start point for line for the y coordinate.
 * @param x2 end point for line for the x coordinate.
 * @param y2 end point for line for the x coordinate.
 * @param color line color.
 */
inline void GRRLIB_Line(f32 x1, f32 y1, f32 x2, f32 y2, u32 color) {
    Vector v[] = {{x1,y1,0.0f}, {x2,y2,0.0f}};

    GRRLIB_NGone(v, color, 2);
}

/**
 * Draw a rectangle.
 * @param x specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param y specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param width the width of the rectangle.
 * @param height the height of the rectangle.
 * @param color the color of the rectangle.
 * @param filled true to fill the rectangle with a color.
 */
inline void GRRLIB_Rectangle(f32 x, f32 y, f32 width, f32 height, u32 color, u8 filled) {
    f32 x2 = x+width;
    f32 y2 = y+height;
    Vector v[] = {{x,y,0.0f}, {x2,y,0.0f}, {x2,y2,0.0f}, {x,y2,0.0f}, {x,y,0.0f}};

    if(!filled) {
        GRRLIB_NGone(v, color, 5);
    }
    else{
        GRRLIB_NGoneFilled(v, color, 4);
    }
}

/**
 *
 * @param v
 * @param color
 * @param n
 */
void GRRLIB_NGone(Vector v[], u32 color, long n) {
    GRRLIB_GXEngine(v, color, n, GX_LINESTRIP);
}

/**
 *
 * @param v
 * @param color
 * @param n
 */
void GRRLIB_NGoneFilled(Vector v[], u32 color, long n) {
    GRRLIB_GXEngine(v, color, n, GX_TRIANGLEFAN);
}

/**
 *
 * @param tex
 * @param tilew
 * @param tileh
 * @param tilestart
 */
void GRRLIB_InitTileSet(struct GRRLIB_texImg *tex, unsigned int tilew, unsigned int tileh, unsigned int tilestart) {
    tex->tilew = tilew;
    tex->tileh = tileh;
    tex->nbtilew = tex->w / tilew;
    tex->nbtileh = tex->h / tileh;
    tex->tilestart = tilestart;
}

/**
 * Load a texture from a buffer.
 * @param my_png the PNG buffer to load.
 * @return A GRRLIB_texImg structure filled with PNG informations.
 */
GRRLIB_texImg GRRLIB_LoadTexturePNG(const unsigned char my_png[]) {
    PNGUPROP imgProp;
    IMGCTX ctx = NULL;
    GRRLIB_texImg my_texture;
	int ret;

    my_texture.w = 0;
    my_texture.h = 0;
    my_texture.data = NULL;
    ctx = PNGU_SelectImageFromBuffer(my_png);
	if (ctx == NULL) goto out;
    ret = PNGU_GetImageProperties (ctx, &imgProp);
	if (ret != PNGU_OK) goto out;
    //my_texture.data = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
    my_texture.data = mem_alloc (imgProp.imgWidth * imgProp.imgHeight * 4);
	if (my_texture.data == NULL) goto out;
    ret = PNGU_DecodeTo4x4RGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, my_texture.data, 255);
	if (ret != PNGU_OK) {
		SAFE_FREE(my_texture.data);
		my_texture.data = NULL;
		goto out;
	}

    my_texture.w = imgProp.imgWidth;
    my_texture.h = imgProp.imgHeight;
    GRRLIB_FlushTex(my_texture);
	out:
    if (ctx) PNGU_ReleaseImageContext (ctx);
    return my_texture;
}

#if 0
/**
 * Convert a raw bmp (RGB, no alpha) to 4x4RGBA.
 * @author DrTwox
 * @param src
 * @param dst
 * @param width
 * @param height
*/
static void RawTo4x4RGBA(const unsigned char *src, void *dst, const unsigned int width, const unsigned int height) {
    unsigned int block;
    unsigned int i;
    unsigned int c;
    unsigned int ar;
    unsigned int gb;
    unsigned char *p = (unsigned char*)dst;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            /* Alpha and Red */
            for (c = 0; c < 4; ++c) {
                for (ar = 0; ar < 4; ++ar) {
                    /* Alpha pixels */
                    *p++ = 255;
                    /* Red pixels */
                    *p++ = src[((i + ar) + ((block + c) * width)) * 3];
                }
            }

            /* Green and Blue */
            for (c = 0; c < 4; ++c) {
                for (gb = 0; gb < 4; ++gb) {
                    /* Green pixels */
                    *p++ = src[(((i + gb) + ((block + c) * width)) * 3) + 1];
                    /* Blue pixels */
                    *p++ = src[(((i + gb) + ((block + c) * width)) * 3) + 2];
                }
            }
        } /* i */
    } /* block */
}

/**
 * Load a texture from a buffer.
 * Take Care to have a JPG finnishing by 0xFF 0xD9 !!!!
 * @author DrTwox
 * @param my_jpg the JPEG buffer to load.
 * @return A GRRLIB_texImg structure filled with PNG informations.
 */
GRRLIB_texImg GRRLIB_LoadTextureJPG(const unsigned char my_jpg[]) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    GRRLIB_texImg my_texture;
    int n = 0;
    unsigned int i;

    if((my_jpg[0]==0xff) && (my_jpg[1]==0xd8) && (my_jpg[2]==0xff)) {
        while(1) {
            if((my_jpg[n]==0xff) && (my_jpg[n+1]==0xd9))
                break;
            n++;
        }
        n+=2;
    }

    jpeg_create_decompress(&cinfo);
    cinfo.err = jpeg_std_error(&jerr);
    cinfo.progress = NULL;
    jpeg_memory_src(&cinfo, my_jpg, n);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    unsigned char *tempBuffer = (unsigned char*) malloc(cinfo.output_width * cinfo.output_height * cinfo.num_components);
    JSAMPROW row_pointer[1];
    row_pointer[0] = (unsigned char*) malloc(cinfo.output_width * cinfo.num_components);
    size_t location = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for (i = 0; i < cinfo.image_width * cinfo.num_components; i++) {
            /* Put the decoded scanline into the tempBuffer */
            tempBuffer[ location++ ] = row_pointer[0][i];
        }
    }

    /* Create a buffer to hold the final texture */
    my_texture.data = memalign(32, cinfo.output_width * cinfo.output_height * 4);
    RawTo4x4RGBA(tempBuffer, my_texture.data, cinfo.output_width, cinfo.output_height);

    /* Done - do cleanup and release memory */
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(row_pointer[0]);
    free(tempBuffer);

    my_texture.w = cinfo.output_width;
    my_texture.h = cinfo.output_height;
    GRRLIB_FlushTex(my_texture);

    return my_texture;
}
#endif

/**
 * Print formatted output.
 * @param xpos
 * @param ypos
 * @param tex
 * @param color
 * @param zoom
 * @param text
 * @param ... Optional arguments.
 */
void GRRLIB_PrintBMF(f32 xpos, f32 ypos, GRRLIB_bytemapFont bmf, f32 zoom, const char *text, ...) {
    unsigned int i, j, x, y, n, size;
    char tmp[1024];

    va_list argp;
    va_start(argp, text);
    size = vsprintf(tmp, text, argp);
    va_end(argp);

    GRRLIB_texImg tex_BMfont = GRRLIB_CreateEmptyTexture(640, 480);

    for(i=0; i<size; i++) {
        for(j=0; j<bmf.nbChar; j++) {
            if(tmp[i] == bmf.charDef[j].character) {
                n=0;
                for(y=0; y<bmf.charDef[j].height; y++) {
                    for(x=0; x<bmf.charDef[j].width; x++) {
                        if(bmf.charDef[j].data[n]) {
                            GRRLIB_SetPixelTotexImg(xpos + x + bmf.charDef[j].relx, ypos + y + bmf.charDef[j].rely,
                                tex_BMfont, bmf.palette[bmf.charDef[j].data[n]]);
                            //GRRLIB_Plot(xpos + x + bmf.charDef[j].relx, ypos + y + bmf.charDef[j].rely,
                            //    bmf.palette[bmf.charDef[j].data[n]]);
                        }
                        n++;
                    }
                }
                xpos += bmf.charDef[j].shift + bmf.addSpace;
                break;
            }
        }
    }
    GRRLIB_FlushTex(tex_BMfont);

    GRRLIB_DrawImg(0, 0, tex_BMfont, 0, 1, 1, 0xFFFFFFFF);

    free(tex_BMfont.data);
}

/**
 * Load a ByteMap font structure from a buffer.
 * @param my_bmf the ByteMap font buffer to load.
 * @return A GRRLIB_bytemapFont structure filled with BMF informations.
 */
GRRLIB_bytemapFont GRRLIB_LoadBMF(const unsigned char my_bmf[]) {
    GRRLIB_bytemapFont fontArray;
    int i, j = 1;
    u8 lineheight, usedcolors, highestcolor, nbPalette;
    short int sizeover, sizeunder, sizeinner, numcolpal;
    u16 nbPixels;

    // Initialize everything to zero
    memset(&fontArray, 0, sizeof(fontArray));

    if(my_bmf[0]==0xE1 && my_bmf[1]==0xE6 && my_bmf[2]==0xD5 && my_bmf[3]==0x1A) {
        fontArray.version = my_bmf[4];
        lineheight = my_bmf[5];
        sizeover = my_bmf[6];
        sizeunder = my_bmf[7];
        fontArray.addSpace = my_bmf[8];
        sizeinner = my_bmf[9];
        usedcolors = my_bmf[10];
        highestcolor = my_bmf[11];
        nbPalette = my_bmf[16];
        numcolpal = 3 * nbPalette;
        fontArray.palette = (u32 *)calloc(nbPalette + 1, sizeof(u32));
        for(i=0; i < numcolpal; i+=3) {
            fontArray.palette[j++] = ((((my_bmf[i+17]<<2)+3)<<24) | (((my_bmf[i+18]<<2)+3)<<16) | (((my_bmf[i+19]<<2)+3)<<8) | 0xFF);
        }
        j = my_bmf[17 + numcolpal];
        fontArray.name = (char *)calloc(j + 1, sizeof(char));
        memcpy(fontArray.name, &my_bmf[18 + numcolpal], j);
        j = 18 + numcolpal + j;
        fontArray.nbChar = (my_bmf[j] | my_bmf[j+1]<<8);
        fontArray.charDef = (GRRLIB_bytemapChar *)calloc(fontArray.nbChar, sizeof(GRRLIB_bytemapChar));
        j++;
        for(i=0; i < fontArray.nbChar; i++) {
            fontArray.charDef[i].character = my_bmf[++j];
            fontArray.charDef[i].width = my_bmf[++j];
            fontArray.charDef[i].height = my_bmf[++j];
            fontArray.charDef[i].relx = my_bmf[++j];
            fontArray.charDef[i].rely = my_bmf[++j];
            fontArray.charDef[i].shift = my_bmf[++j];
            nbPixels = fontArray.charDef[i].width * fontArray.charDef[i].height;
            fontArray.charDef[i].data = malloc(nbPixels);
            if(nbPixels && fontArray.charDef[i].data) {
                memcpy(fontArray.charDef[i].data, &my_bmf[++j], nbPixels);
                j += (nbPixels - 1);
            }
        }
    }
    return fontArray;
}

/**
 * Free memory.
 * @param bmf a GRRLIB_bytemapFont structure.
 */
void GRRLIB_FreeBMF(GRRLIB_bytemapFont bmf)
{
    unsigned int i;

    for(i=0; i<bmf.nbChar; i++) {
        free(bmf.charDef[i].data);
    }
    free(bmf.charDef);
    free(bmf.palette);
    free(bmf.name);
}

/**
 * Load a texture from a buffer.
 * @param my_img the JPEG or PNG buffer to load.
 * @return A GRRLIB_texImg structure filled with imgage informations.
 */
GRRLIB_texImg GRRLIB_LoadTexture(const unsigned char my_img[]) {
#if 0
    if(my_img[0]==0xFF && my_img[1]==0xD8 && my_img[2]==0xFF) {
        return(GRRLIB_LoadTextureJPG(my_img));
    }
    else {
        return(GRRLIB_LoadTexturePNG(my_img));
    }
#else
        return(GRRLIB_LoadTexturePNG(my_img));
#endif
}

/**
 * Create an empty texture.
 * @param w width of the new texture to create.
 * @param h height of the new texture to create.
 * @return A GRRLIB_texImg structure newly created.
 */
GRRLIB_texImg GRRLIB_CreateEmptyTexture(unsigned int w, unsigned int h) {
    unsigned int x, y;
    GRRLIB_texImg my_texture;

    my_texture.w = 0;
    my_texture.h = 0;
    my_texture.data = memalign (32, h * w * 4);
	if (my_texture.data == NULL) goto out;
    my_texture.w = w;
    my_texture.h = h;
    // Initialize the texture
    for(y=0; y<h; y++) {
        for(x=0; x<w; x++) {
            GRRLIB_SetPixelTotexImg(x, y, my_texture, 0x00000000);
        }
    }
    GRRLIB_FlushTex(my_texture);
	out:
    return my_texture;
}

/**
 * Draw a texture.
 * @param xpos specifies the x-coordinate of the upper-left corner.
 * @param ypos specifies the y-coordinate of the upper-left corner.
 * @param tex texture to draw.
 * @param degrees angle of rotation.
 * @param scaleX
 * @param scaleY
 * @param color
 */
inline void GRRLIB_DrawImg(f32 xpos, f32 ypos, GRRLIB_texImg tex, float degrees, float scaleX, f32 scaleY, u32 color ) {
    GXTexObj texObj;
    u16 width, height;
    Mtx m, m1, m2, mv;

    GX_InitTexObj(&texObj, tex.data, tex.w, tex.h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	if (degrees == 0.0 && scaleX == 1.0 && scaleY == 1.0) {
	    GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
	}
    GX_LoadTexObj(&texObj, GX_TEXMAP0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
    GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

    width = tex.w * 0.5;
    height = tex.h * 0.5;
    guMtxIdentity (m1);
    guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0);
    Vector axis = (Vector) {0, 0, 1 };
    guMtxRotAxisDeg (m2, &axis, degrees);
    guMtxConcat(m2, m1, m);

    guMtxTransApply(m, m, xpos+width, ypos+height, 0);
    guMtxConcat (GXmodelView2D, m, mv);
    GX_LoadPosMtxImm (mv, GX_PNMTX0);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(-width, -height, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(0, 0);

    GX_Position3f32(width, -height, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(1, 0);

    GX_Position3f32(width, height, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(1, 1);

    GX_Position3f32(-width, height, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(0, 1);
    GX_End();
    GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

/**
 * Draw a textured quad.
 * @param pos Vector array of the 4 points.
 * @param tex The texture to draw.
 * @param color Color in RGBA format.
 */
inline void GRRLIB_DrawImgQuad(Vector pos[4], struct GRRLIB_texImg *tex, u32 color) {
    if (tex == NULL || tex->data == NULL) {
        return;
    }

    GXTexObj texObj;
    Mtx m, m1, m2, mv;

    GX_InitTexObj(&texObj, tex->data, tex->w, tex->h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    //if (GRRLIB_Settings.antialias == false) {
    //    GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    //}

    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    guMtxIdentity(m1);
    guMtxScaleApply(m1, m1, 1, 1, 1.0);
    Vector axis = (Vector) {0, 0, 1};
    guMtxRotAxisDeg (m2, &axis, 0);
    guMtxConcat(m2, m1, m);

    guMtxConcat(GXmodelView2D, m, mv);

    GX_LoadPosMtxImm(mv, GX_PNMTX0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(pos[0].x, pos[0].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(0, 0);

        GX_Position3f32(pos[1].x, pos[1].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(1, 0);

        GX_Position3f32(pos[2].x, pos[2].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(1, 1);

        GX_Position3f32(pos[3].x, pos[3].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(0, 1);
    GX_End();
    GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
}

/**
 * Draw a tile.
 * @param xpos specifies the x-coordinate of the upper-left corner.
 * @param ypos specifies the y-coordinate of the upper-left corner.
 * @param tex texture to draw.
 * @param degrees angle of rotation.
 * @param scaleX
 * @param scaleY
 * @param color
 * @param frame
 */
inline void GRRLIB_DrawTile(f32 xpos, f32 ypos, GRRLIB_texImg tex, float degrees, float scaleX, f32 scaleY, u32 color, int frame) {
    GXTexObj texObj;
    f32 width, height;
    Mtx m, m1, m2, mv;

    // Frame Correction by spiffen
    f32 FRAME_CORR = 0.001f;
    f32 s1 = (((frame%tex.nbtilew))/(f32)tex.nbtilew)+(FRAME_CORR/tex.w);
    f32 s2 = (((frame%tex.nbtilew)+1)/(f32)tex.nbtilew)-(FRAME_CORR/tex.w);
    f32 t1 = (((int)(frame/tex.nbtilew))/(f32)tex.nbtileh)+(FRAME_CORR/tex.h);
    f32 t2 = (((int)(frame/tex.nbtilew)+1)/(f32)tex.nbtileh)-(FRAME_CORR/tex.h);

    GX_InitTexObj(&texObj, tex.data, tex.tilew*tex.nbtilew, tex.tileh*tex.nbtileh, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
    GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

    width = tex.tilew * 0.5f;
    height = tex.tileh * 0.5f;
    guMtxIdentity (m1);
    guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0f);
    Vector axis = (Vector) {0, 0, 1 };
    guMtxRotAxisDeg (m2, &axis, degrees);
    guMtxConcat(m2, m1, m);
    guMtxTransApply(m, m, xpos+width, ypos+height, 0);
    guMtxConcat (GXmodelView2D, m, mv);
    GX_LoadPosMtxImm (mv, GX_PNMTX0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(-width, -height, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s1, t1);

    GX_Position3f32(width, -height,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s2, t1);

    GX_Position3f32(width, height,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s2, t2);

    GX_Position3f32(-width, height,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s1, t2);
    GX_End();
    GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

/**
 * Draw a tile in a quad.
 * @param pos Vector array of the 4 points.
 * @param tex The texture to draw.
 * @param color Color in RGBA format.
 * @param frame Specifies the frame to draw.
 */
inline void GRRLIB_DrawTileQuad(Vector pos[4], struct GRRLIB_texImg *tex, u32 color, int frame) {
    if (tex == NULL || tex->data == NULL) {
        return;
    }

    GXTexObj texObj;
    //Mtx m, m1, m2, mv;

    // Frame Correction by spiffen
    f32 FRAME_CORR = 0.001f;
    f32 s1 = (((frame%tex->nbtilew))/(f32)tex->nbtilew)+(FRAME_CORR/tex->w);
    f32 s2 = (((frame%tex->nbtilew)+1)/(f32)tex->nbtilew)-(FRAME_CORR/tex->w);
    f32 t1 = (((int)(frame/tex->nbtilew))/(f32)tex->nbtileh)+(FRAME_CORR/tex->h);
    f32 t2 = (((int)(frame/tex->nbtilew)+1)/(f32)tex->nbtileh)-(FRAME_CORR/tex->h);

    GX_InitTexObj(&texObj, tex->data, tex->tilew*tex->nbtilew, tex->tileh*tex->nbtileh, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    //if (GRRLIB_Settings.antialias == false) {
    //    GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    //}
    GX_LoadTexObj(&texObj, GX_TEXMAP0);

    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    /*guMtxIdentity(m1);
    guMtxScaleApply(m1, m1, 1, 1, 1.0f);

    Vector axis = (Vector) {0, 0, 1};
    guMtxRotAxisDeg(m2, &axis, 0);
    guMtxConcat(m2, m1, m);

    guMtxConcat(GXmodelView2D, m, mv);

    GX_LoadPosMtxImm(mv, GX_PNMTX0);*/
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(pos[0].x, pos[0].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(s1, t1);

        GX_Position3f32(pos[1].x, pos[1].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(s2, t1);

        GX_Position3f32(pos[2].x, pos[2].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(s2, t2);

        GX_Position3f32(pos[3].x, pos[3].y, 0);
        GX_Color1u32(color);
        GX_TexCoord2f32(s1, t2);
    GX_End();
    //GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
}

/**
 * Print formatted output.
 * @param xpos Specifies the x-coordinate of the upper-left corner of the text.
 * @param ypos Specifies the y-coordinate of the upper-left corner of the text.
 * @param tex The texture containing the character set.
 * @param color Text color in RGBA format. The alpha channel is used to change the opacity of the text.
 * @param zoom This is a factor by which the text size will be increase or decrease.
 * @param text Text to draw.
 * @param ... Optional arguments.
 */
void GRRLIB_Printf(f32 xpos, f32 ypos, struct GRRLIB_texImg tex, u32 color, f32 zoom, const char *text, ...) {
    int i, size;
    char tmp[1024];

    va_list argp;
    va_start(argp, text);
    size = vsprintf(tmp, text, argp);
    va_end(argp);

    for (i = 0; i < size; i++) {
        u8 c = tmp[i]-tex.tilestart;
        GRRLIB_DrawTile(xpos+i*tex.tilew*zoom, ypos, tex, 0, zoom, zoom, color, c);
    }
}

/**
 * Determines whether the specified point lies within the specified rectangle.
 * @param hotx specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param hoty specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param hotw the width of the rectangle.
 * @param hoth the height of the rectangle.
 * @param wpadx specifies the x-coordinate of the point.
 * @param wpady specifies the y-coordinate of the point.
 * @return If the specified point lies within the rectangle, the return value is true otherwise it's false.
 */
bool GRRLIB_PtInRect(int hotx, int hoty, int hotw, int hoth, int wpadx, int wpady) {
    return(((wpadx>=hotx) & (wpadx<=(hotx+hotw))) & ((wpady>=hoty) & (wpady<=(hoty+hoth))));
}

/**
 * Determine whether a specified rectangle lies within another rectangle.
 * @param rect1x Specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param rect1y Specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param rect1w Specifies the width of the rectangle.
 * @param rect1h Specifies the height of the rectangle.
 * @param rect2x Specifies the x-coordinate of the upper-left corner of the rectangle.
 * @param rect2y Specifies the y-coordinate of the upper-left corner of the rectangle.
 * @param rect2w Specifies the width of the rectangle.
 * @param rect2h Specifies the height of the rectangle.
 * @return If the specified rectangle lies within the other rectangle, the return value is true otherwise it's false.
 */
 bool GRRLIB_RectInRect(int rect1x, int rect1y, int rect1w, int rect1h, int rect2x, int rect2y, int rect2w, int rect2h) {
    return ((rect1x >= rect2x) && (rect1y >= rect2y) &&
        (rect1x+rect1w <= rect2x+rect2w) && (rect1y+rect1h <= rect2y+rect2h));
}

/**
 * Determine whether a part of a specified rectangle lies on another rectangle.
 * @param rect1x Specifies the x-coordinate of the upper-left corner of the first rectangle.
 * @param rect1y Specifies the y-coordinate of the upper-left corner of the first rectangle.
 * @param rect1w Specifies the width of the first rectangle.
 * @param rect1h Specifies the height of the first rectangle.
 * @param rect2x Specifies the x-coordinate of the upper-left corner of the second rectangle.
 * @param rect2y Specifies the y-coordinate of the upper-left corner of the second rectangle.
 * @param rect2w Specifies the width of the second rectangle.
 * @param rect2h Specifies the height of the second rectangle.
 * @return If the specified rectangle lies on the other rectangle, the return value is true otherwise it's false.
 */
bool GRRLIB_RectOnRect(int rect1x, int rect1y, int rect1w, int rect1h, int rect2x, int rect2y, int rect2w, int rect2h) {
    return (GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x, rect2y) ||
        GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x+rect2w, rect2y) ||
        GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x+rect2w, rect2y+rect2h) ||
        GRRLIB_PtInRect(rect1x, rect1y, rect1w, rect1h, rect2x, rect2y+rect2h));
}

/**
 * Return the color value of a pixel from a GRRLIB_texImg.
 * @param x specifies the x-coordinate of the pixel in the texture.
 * @param y specifies the y-coordinate of the pixel in the texture.
 * @param tex texture to get the color from.
 * @return The color of a pixel.
 */
u32 GRRLIB_GetPixelFromtexImg(int x, int y, GRRLIB_texImg tex) {
    u8 *truc = (u8*)tex.data;
    u8 r, g, b, a;
    u32 offset;

    // if they're not within the tex then return
    if ( x >= tex.w || x < 0 || y >= tex.h || y < 0) { 
		return 0xFFFFFFFF; 
	}

    offset = (((y >> 2)<<4)*tex.w) + ((x >> 2)<<6) + (((y%4 << 2) + x%4 ) << 1); // Fuckin equation found by NoNameNo ;)

    a=*(truc+offset);
    r=*(truc+offset+1);
    g=*(truc+offset+32);
    b=*(truc+offset+33);

    return((r<<24) | (g<<16) | (b<<8) | a);
}

/**
 * Set the color value of a pixel to a GRRLIB_texImg.
 * @see GRRLIB_FlushTex
 * @param x specifies the x-coordinate of the pixel in the texture.
 * @param y specifies the y-coordinate of the pixel in the texture.
 * @param tex texture to set the color to.
 * @param color the color of the pixel.
 */
void GRRLIB_SetPixelTotexImg(int x, int y, GRRLIB_texImg tex, u32 color) {
    u8 *truc = (u8*)tex.data;
    u32 offset;

    // if they're not within the tex then return
    if ( x >= tex.w || x < 0 || y >= tex.h || y < 0) { 
		return; 
	}

    offset = (((y >> 2)<<4)*tex.w) + ((x >> 2)<<6) + (((y%4 << 2) + x%4 ) << 1); // Fuckin equation found by NoNameNo ;)

    *(truc+offset)=color & 0xFF;
    *(truc+offset+1)=(color>>24) & 0xFF;
    *(truc+offset+32)=(color>>16) & 0xFF;
    *(truc+offset+33)=(color>>8) & 0xFF;
}

/**
 * Writes the contents of a texture in the data cache down to main memory.
 * For performance the CPU holds a data cache where modifications are stored before they get written down to mainmemory.
 * @param tex the texture to flush.
 */
void GRRLIB_FlushTex(GRRLIB_texImg tex)
{
    DCFlushRange(tex.data, tex.w * tex.h * 4);
}

/**
 * Change a texture to gray scale.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture grayscaled destination.
 */
void GRRLIB_BMFX_Grayscale(GRRLIB_texImg texsrc, GRRLIB_texImg texdest) {
    unsigned int x, y;
    u8 gray;
    u32 color;

    for(y=0; y<texsrc.h; y++) {
        for(x=0; x<texsrc.w; x++) {
            color = GRRLIB_GetPixelFromtexImg(x, y, texsrc);

            gray = (((color >> 24 & 0xFF)*77 + (color >> 16 & 0xFF)*150 + (color >> 8 & 0xFF)*28) / (255));

            GRRLIB_SetPixelTotexImg(x, y, texdest,
                ((gray << 24) | (gray << 16) | (gray << 8) | (color & 0xFF)));
        }
    }
}

/**
 * Invert colors of the texture.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture destination.
 */
void GRRLIB_BMFX_Invert(GRRLIB_texImg texsrc, GRRLIB_texImg texdest) {
    unsigned int x, y;
    u32 color;

    for(y=0; y<texsrc.h; y++) {
        for(x=0; x<texsrc.w; x++) {
            color = GRRLIB_GetPixelFromtexImg(x, y, texsrc);

            GRRLIB_SetPixelTotexImg(x, y, texdest,
                ((0xFFFFFF - (color >> 8 & 0xFFFFFF)) << 8)  | (color & 0xFF));
        }
    }
}

/**
 * Flip texture horizontal.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture destination.
 */
void GRRLIB_BMFX_FlipH(GRRLIB_texImg texsrc, GRRLIB_texImg texdest) {
    unsigned int x, y, txtWidth = texsrc.w - 1;

    for(y=0; y<texsrc.h; y++) {
        for(x=0; x<texsrc.w; x++) {
            GRRLIB_SetPixelTotexImg(txtWidth - x, y, texdest,
                GRRLIB_GetPixelFromtexImg(x, y, texsrc));
        }
    }
}

/**
 * Flip texture vertical.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture destination.
 */
void GRRLIB_BMFX_FlipV(GRRLIB_texImg texsrc, GRRLIB_texImg texdest) {
    unsigned int x, y, texHeight = texsrc.h - 1;

    for(y=0; y<texsrc.h; y++) {
        for(x=0; x<texsrc.w; x++) {
            GRRLIB_SetPixelTotexImg(x, texHeight - y, texdest,
                GRRLIB_GetPixelFromtexImg(x, y, texsrc));
        }
    }
}

/**
 * Blur a texture.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture destination.
 * @param factor the blur factor.
 */
void GRRLIB_BMFX_Blur(GRRLIB_texImg texsrc, GRRLIB_texImg texdest, int factor) {
    int numba = (1+(factor<<1))*(1+(factor<<1));

    int x, y;
    int k, l;
    int tmp=0;
    int newr, newg, newb, newa;
    u32 colours[numba];
    u32 thiscol;

    for (x = 0; x < texsrc.w; x++) {
        for (y = 0; y < texsrc.h; y++) {
            newr = 0;
            newg = 0;
            newb = 0;
            newa = 0;

            tmp=0;
            thiscol = GRRLIB_GetPixelFromtexImg(x, y, texsrc);

            for (k = x - factor; k <= x + factor; k++) {
                for (l = y - factor; l <= y + factor; l++) {
                    if (k < 0) { colours[tmp] = thiscol; }
                    else if (k >= texsrc.w) { colours[tmp] = thiscol; }
                    else if (l < 0) { colours[tmp] = thiscol; }
                    else if (l >= texsrc.h) { colours[tmp] = thiscol; }
                    else{ colours[tmp] = GRRLIB_GetPixelFromtexImg(k, l, texsrc); }
                    tmp++;
                }
            }

            for (tmp = 0; tmp < numba; tmp++) {
                newr += (colours[tmp] >> 24) & 0xFF;
                newg += (colours[tmp] >> 16) & 0xFF;
                newb += (colours[tmp] >> 8) & 0xFF;
                newa += colours[tmp] & 0xFF;
            }

            newr /= numba;
            newg /= numba;
            newb /= numba;
            newa /= numba;

            GRRLIB_SetPixelTotexImg(x, y, texdest, (newr<<24) | (newg<<16) | (newb<<8) | newa);
        }
    }
}

/**
 * A texture effect.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture destination.
 * @param factor The factor level of the effect.
 */
void GRRLIB_BMFX_Pixelate(GRRLIB_texImg texsrc, GRRLIB_texImg texdest, int factor) {
    unsigned int x, y;
    unsigned int xx, yy;
    u32 rgb;

    for(x=0; x<texsrc.w-1-factor; x+= factor) {
        for(y=0; y<texsrc.h-1-factor; y+=factor) {
            rgb=GRRLIB_GetPixelFromtexImg(x, y, texsrc);
                for(xx=x; xx<x+factor; xx++) {
                    for(yy=y; yy<y+factor; yy++) {
                        GRRLIB_SetPixelTotexImg(xx, yy, texdest, rgb);
                    }
                }
        }
    }
}

/**
 * A texture effect.
 * @see GRRLIB_FlushTex
 * @param texsrc the texture source.
 * @param texdest the texture destination.
 * @param factor The factor level of the effect.
 */
void GRRLIB_BMFX_Scatter(GRRLIB_texImg texsrc, GRRLIB_texImg texdest, int factor) {
    unsigned int x, y;
    int val1, val2;
    u32 val3, val4;
    int factorx2 = factor*2;

    for(y=0; y<texsrc.h; y++) {
        for(x=0; x<texsrc.w; x++) {
            val1 = x + (int) (factorx2 * (rand() / (RAND_MAX + 1.0))) - factor;
            val2 = y + (int) (factorx2 * (rand() / (RAND_MAX + 1.0))) - factor;

            if((val1 >= texsrc.w) || (val1 <0) || (val2 >= texsrc.h) || (val2 <0)) {
            }
            else {
                val3 = GRRLIB_GetPixelFromtexImg(x, y, texsrc);
                val4 = GRRLIB_GetPixelFromtexImg(val1, val2, texsrc);
                GRRLIB_SetPixelTotexImg(x, y, texdest, val4);
                GRRLIB_SetPixelTotexImg(val1, val2, texdest, val3);
            }
        }
    }
}

/**
 *
 * @param v
 * @param color
 * @param n
 * @param fmt
 */
void GRRLIB_GXEngine(Vector v[], u32 color, long n, u8 fmt) {
    int i;

    GX_Begin(fmt, GX_VTXFMT0, n);
    for(i=0; i<n; i++) {
        GX_Position3f32(v[i].x, v[i].y,  v[i].z);
        GX_Color1u32(color);
    }
    GX_End();
}

/**
 * Initialize GRRLIB. Call this at the beginning your code.
 * @see GRRLIB_Exit
 */
void _GRRLIB_Init_Video()
{
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    if(rmode == NULL)
        return;

    /* Widescreen patch by CashMan's Productions (http://www.CashMan-Productions.fr.nf) */
    if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) 
    {
        rmode->viWidth = 678; 
        rmode->viXOrigin = (VI_MAX_WIDTH_NTSC - 678)/2;
    }

    VIDEO_Configure (rmode);
}

int _GRRLIB_Init(void *fb0, void *fb1)
{
    f32 yscale;
    u32 xfbHeight;
    Mtx44 perspective;

	if (fb0) {
	    xfb[0] = fb0;
	} else {
	    xfb[0] = (u32 *)MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	}

	if (fb1) {
	    xfb[1] = fb1;
	} else {
	    xfb[1] = (u32 *)MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	}

    if(xfb[0] == NULL || xfb[1] == NULL)
        return -1;

	if (fb0 == NULL)
		VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	if (fb1 == NULL)
		VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);

	fb = 0;

	if (fb0 == NULL) {
		VIDEO_SetNextFramebuffer(xfb[fb]);
		VIDEO_SetBlack(FALSE);
		VIDEO_Flush();
		VIDEO_WaitVSync();
		if(rmode->viTVMode&VI_NON_INTERLACE)
			VIDEO_WaitVSync();
	}

    gp_fifo = (u8 *) memalign(32, DEFAULT_FIFO_SIZE);
    if(gp_fifo == NULL)
        return -1;
    memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

    // clears the bg to color and clears the z buffer
    GXColor background = { 0, 0, 0, 0xff };
    GX_SetCopyClear (background, GX_MAX_Z24);

    // other gx setup
    yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

    if (rmode->aa)
        GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
    else
        GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    GX_SetDispCopyGamma(GX_GM_1_0);


    // setup the vertex descriptor
    // tells the flipper to expect direct data
    GX_ClearVtxDesc();
    GX_InvVtxCache ();
    GX_InvalidateTexAll();

    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);


    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_TRUE);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    guMtxIdentity(GXmodelView2D);
    guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
    GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

    guOrtho(perspective,0, 480, 0, 640, 0, 300.0F);
    GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetAlphaUpdate(GX_TRUE);

    GX_SetCullMode(GX_CULL_NONE);
	return 0;
}

void GRRLIB_Init()
{
	_GRRLIB_Init_Video();
	_GRRLIB_Init(NULL, NULL);
}

int GRRLIB_Init_VMode(GXRModeObj *a_rmode, void *fb0, void *fb1)
{
	if (a_rmode == NULL) return -1;
	rmode = a_rmode;
	return _GRRLIB_Init(fb0, fb1);
}


/**
 * Call this function after drawing.
 */
void GRRLIB_Render() {
    GX_DrawDone ();

    fb ^= 1;        // flip framebuffer
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(xfb[fb], GX_TRUE);
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void _GRRLIB_Render() {
    GX_DrawDone ();
    fb ^= 1;        // flip framebuffer
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(xfb[fb], GX_TRUE);
}

void _GRRLIB_VSync() {
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void** _GRRLIB_GetXFB(int *cur_fb)
{
	*cur_fb = fb;
	return xfb;
}

void _GRRLIB_SetFB(int cur_fb)
{
	fb = cur_fb;
}


/**
 * Call this before exiting your application.
 */
void GRRLIB_Exit() {
    GX_Flush();
    GX_AbortFrame();

    if(xfb[0] != NULL) {
        free(MEM_K1_TO_K0(xfb[0]));
        xfb[0] = NULL;
    }
    if(xfb[1] != NULL) {
        free(MEM_K1_TO_K0(xfb[1]));
        xfb[1] = NULL;
    }
    if(gp_fifo != NULL) {
        free(gp_fifo);
        gp_fifo = NULL;
    }
}

void _GRRLIB_Exit() {
    GX_Flush();
    GX_AbortFrame();

    if(gp_fifo != NULL) {
        free(gp_fifo);
        gp_fifo = NULL;
    }
}

/**
 * Make a PNG screenshot on the SD card.
 * @param File Name of the file to write.
 * @return True if every thing worked, false otherwise.
 */
bool GRRLIB_ScrShot(const char* File)
{
    IMGCTX pngContext;
    int ErrorCode = -1;

    if(fatInitDefault() && (pngContext = PNGU_SelectImageFromDevice(File)))
    {
        ErrorCode = PNGU_EncodeFromYCbYCr(pngContext, 640, 480, xfb[fb], 0);
        PNGU_ReleaseImageContext(pngContext);
    }
    return !ErrorCode;
}

#if 0
/**
 * Reads a pixel directly from the FrontBuffer.
 * Since the FB is stored in YCbCr,
 * @param x The x-coordinate within the FB.
 * @param y The y-coordinate within the FB.
 * @param R1 A pointer to a variable receiving the first Red value.
 * @param G1 A pointer to a variable receiving the first Green value.
 * @param B1 A pointer to a variable receiving the first Blue value.
 * @param R2 A pointer to a variable receiving the second Red value.
 * @param G2 A pointer to a variable receiving the second Green value.
 * @param B2 A pointer to a variable receiving the second Blue value.
 */
void GRRLIB_GetPixelFromFB(int x, int y, u8 *R1, u8 *G1, u8 *B1, u8* R2, u8 *G2, u8 *B2 ) {
    // Position Correction
    if (x > (rmode->fbWidth/2)) { x = (rmode->fbWidth/2); }
    if (x < 0) { x = 0; }
    if (y > rmode->efbHeight) { y = rmode->efbHeight; }
    if (y < 0) { y = 0; }

    // Preparing FB for reading
    u32 Buffer = (((u32 *)xfb[fb])[y*(rmode->fbWidth/2)+x]);
    u8 *Colors = (u8 *) &Buffer;

    /** Color channel:
    Colors[0] = Y1
    Colors[1] = Cb
    Colors[2] = Y2
    Colors[3] = Cr */

    *R1 = GRRLIB_ClampVar8( 1.164 * (Colors[0] - 16) + 1.596 * (Colors[3] - 128) );
    *G1 = GRRLIB_ClampVar8( 1.164 * (Colors[0] - 16) - 0.813 * (Colors[3] - 128) - 0.392 * (Colors[1] - 128) );
    *B1 = GRRLIB_ClampVar8( 1.164 * (Colors[0] - 16) + 2.017 * (Colors[1] - 128) );

    *R2 = GRRLIB_ClampVar8( 1.164 * (Colors[2] - 16) + 1.596 * (Colors[3] - 128) );
    *G2 = GRRLIB_ClampVar8( 1.164 * (Colors[2] - 16) - 0.813 * (Colors[3] - 128) - 0.392 * (Colors[1] - 128) );
    *B2 = GRRLIB_ClampVar8( 1.164 * (Colors[2] - 16) + 2.017 * (Colors[1] - 128) );
}


/**
 * A helper function for the YCbCr -> RGB conversion.
 * Clamps the given value into a range of 0 - 255 and thus preventing an overflow.
 * @param Value The value to clamp.
 * @return Returns a clean, clamped unsigned char.
 */
u8 GRRLIB_ClampVar8(float Value) {
    /* Using float to increase the precision.
    This makes a full spectrum (0 - 255) possible. */
    Value = roundf(Value);
    if (Value < 0) {
        Value = 0;
    } else if (Value > 255) {
        Value = 255;
    }
    return (u8)Value;
}

/**
 * Converts RGBA values to u32 color.
 * @param r Amount of Red (0 - 255);
 * @param g Amount of Green (0 - 255);
 * @param b Amount of Blue (0 - 255);
 * @param a Amount of Alpha (0 - 255);
 * @return Returns the color in u32 format.
 */
u32 GRRLIB_GetColor( u8 r, u8 g, u8 b, u8 a ) {
    return (r << 24) | (g << 16) | (b << 8) | a;
}

#endif

/**
 * Free memory allocated for texture.
 * @param tex A GRRLIB_texImg structure.
 */
/*
void GRRLIB_FreeTexture(struct GRRLIB_texImg *tex) {
    SAFE_FREE(tex->data);
    SAFE_FREE(tex);
    tex = NULL;
}
*/


void GRRLIB_DrawTile_begin(GRRLIB_texImg tex)
{
    GXTexObj texObj;

    GX_InitTexObj(&texObj, tex.data, tex.tilew*tex.nbtilew, tex.tileh*tex.nbtileh, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
    GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

    GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);
}


inline void GRRLIB_DrawTile_draw(f32 xpos, f32 ypos, GRRLIB_texImg tex, float degrees, float scaleX, f32 scaleY, u32 color, int frame)
{
    f32 width, height;
    Mtx m, m1, m2, mv;

    // Frame Correction by spiffen
    f32 FRAME_CORR = 0.001f;
    f32 s1 = (((frame%tex.nbtilew))/(f32)tex.nbtilew)+(FRAME_CORR/tex.w);
    f32 s2 = (((frame%tex.nbtilew)+1)/(f32)tex.nbtilew)-(FRAME_CORR/tex.w);
    f32 t1 = (((int)(frame/tex.nbtilew))/(f32)tex.nbtileh)+(FRAME_CORR/tex.h);
    f32 t2 = (((int)(frame/tex.nbtilew)+1)/(f32)tex.nbtileh)-(FRAME_CORR/tex.h);

    width = tex.tilew * 0.5f;
    height = tex.tileh * 0.5f;
    guMtxIdentity (m1);
    guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0f);
    Vector axis = (Vector) {0, 0, 1 };
    guMtxRotAxisDeg (m2, &axis, degrees);
    guMtxConcat(m2, m1, m);
    guMtxTransApply(m, m, xpos+width, ypos+height, 0);
    guMtxConcat (GXmodelView2D, m, mv);
    GX_LoadPosMtxImm (mv, GX_PNMTX0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(-width, -height, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s1, t1);

    GX_Position3f32(width, -height,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s2, t1);

    GX_Position3f32(width, height,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s2, t2);

    GX_Position3f32(-width, height,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s1, t2);
    GX_End();
    GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);
}

inline void GRRLIB_DrawTile_draw1(f32 xpos, f32 ypos, GRRLIB_texImg tex, u32 color, int frame)
{
    f32 width, height;

    // Frame Correction by spiffen
    f32 FRAME_CORR = 0.001f;
    f32 s1 = (((frame%tex.nbtilew))/(f32)tex.nbtilew)+(FRAME_CORR/tex.w);
    f32 s2 = (((frame%tex.nbtilew)+1)/(f32)tex.nbtilew)-(FRAME_CORR/tex.w);
    f32 t1 = (((int)(frame/tex.nbtilew))/(f32)tex.nbtileh)+(FRAME_CORR/tex.h);
    f32 t2 = (((int)(frame/tex.nbtilew)+1)/(f32)tex.nbtileh)-(FRAME_CORR/tex.h);

    width = tex.tilew * 0.5f;
    height = tex.tileh * 0.5f;
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(xpos, ypos, 0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s1, t1);

    GX_Position3f32(xpos+tex.tilew, ypos,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s2, t1);

    GX_Position3f32(xpos+tex.tilew, ypos+tex.tileh,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s2, t2);

    GX_Position3f32(xpos, ypos+tex.tileh,  0);
    GX_Color1u32(color);
    GX_TexCoord2f32(s1, t2);
    GX_End();
}

inline void GRRLIB_DrawTile_end(GRRLIB_texImg tex)
{
    GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

/**
 * Draw a slice.
 * @param xpos specifies the x-coordinate of the upper-left corner.
 * @param ypos specifies the y-coordinate of the upper-left corner.
 * @param tex texture to draw.
 * @param degrees angle of rotation.
 * @param scaleX
 * @param scaleY
 * @param color1 top
 * @param color2 bottom
 * @param x,y,w,h slice coords inside texture
 */
void GRRLIB_DrawSlice2(f32 xpos, f32 ypos, GRRLIB_texImg tex, float degrees,
		float scaleX, f32 scaleY, u32 color1, u32 color2,
		float x, float y, float w, float h)
{
    GXTexObj texObj;
    f32 width, height;
    Mtx m, m1, m2, mv;

    // Frame Correction by spiffen
	/*
    f32 FRAME_CORR = 0.001f;
    f32 s1 = (((frame%tex.nbtilew))/(f32)tex.nbtilew)+(FRAME_CORR/tex.w);
    f32 s2 = (((frame%tex.nbtilew)+1)/(f32)tex.nbtilew)-(FRAME_CORR/tex.w);
    f32 t1 = (((int)(frame/tex.nbtilew))/(f32)tex.nbtileh)+(FRAME_CORR/tex.h);
    f32 t2 = (((int)(frame/tex.nbtilew)+1)/(f32)tex.nbtileh)-(FRAME_CORR/tex.h);
	*/
    f32 FRAME_CORR = 0.001f;
    f32 s1 = (x     / (f32)tex.w) + (FRAME_CORR/tex.w);
    f32 s2 = ((x+w) / (f32)tex.w) - (FRAME_CORR/tex.w);
    f32 t1 = (y     / (f32)tex.h) + (FRAME_CORR/tex.h);
    f32 t2 = ((y+h) / (f32)tex.h) - (FRAME_CORR/tex.h);

    //GX_InitTexObj(&texObj, tex.data, tex.tilew*tex.nbtilew, tex.tileh*tex.nbtileh, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObj(&texObj, tex.data, tex.w, tex.h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
    GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

    //width = tex.tilew * 0.5f;
    //height = tex.tileh * 0.5f;
    width = w * 0.5f;
    height = h * 0.5f;
    guMtxIdentity (m1);
    guMtxScaleApply(m1, m1, scaleX, scaleY, 1.0f);
    Vector axis = (Vector) {0, 0, 1 };
    guMtxRotAxisDeg (m2, &axis, degrees);
    guMtxConcat(m2, m1, m);
    guMtxTransApply(m, m, xpos+width, ypos+height, 0);
    guMtxConcat (GXmodelView2D, m, mv);
    GX_LoadPosMtxImm (mv, GX_PNMTX0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(-width, -height, 0);
    GX_Color1u32(color1);
    GX_TexCoord2f32(s1, t1);

    GX_Position3f32(width, -height,  0);
    GX_Color1u32(color1);
    GX_TexCoord2f32(s2, t1);

    GX_Position3f32(width, height,  0);
    GX_Color1u32(color2);
    GX_TexCoord2f32(s2, t2);

    GX_Position3f32(-width, height,  0);
    GX_Color1u32(color2);
    GX_TexCoord2f32(s1, t2);
    GX_End();
    GX_LoadPosMtxImm (GXmodelView2D, GX_PNMTX0);

    GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

void GRRLIB_DrawSlice(f32 xpos, f32 ypos, GRRLIB_texImg tex, float degrees,
		float scaleX, f32 scaleY, u32 color,
		float x, float y, float w, float h)
{
	GRRLIB_DrawSlice2(xpos, ypos, tex, degrees,
		scaleX, scaleY, color, color,
		x, y, w, h);
}


void __GRRLIB_Print1w(f32 xpos, f32 ypos, struct GRRLIB_texImg tex,
		u32 color, const wchar_t *wtext)
{
	unsigned nc = tex.nbtilew * tex.nbtileh;
	wchar_t c;
    int i;
	f32 x;

	for (i = 0; wtext[i]; i++) {
		c = wtext[i];
		if ((unsigned)c >= nc) c = map_ufont(c);
		c -= tex.tilestart;
		x = xpos + i * tex.tilew;
		GRRLIB_DrawTile_draw1(x, ypos, tex, color, c);
	}
}

void GRRLIB_Print2(f32 xpos, f32 ypos, struct GRRLIB_texImg tex, u32 color, u32 outline, u32 shadow, const char *text)
{
	int len = strlen(text);
	wchar_t wtext[len+1];
	int wlen;
	wlen = mbstowcs(wtext, text, len);
	wtext[wlen] = 0;

    GRRLIB_DrawTile_begin(tex);
	if (shadow) {
		__GRRLIB_Print1w(xpos+1, ypos+0, tex, shadow, wtext);
		__GRRLIB_Print1w(xpos+1, ypos+1, tex, shadow, wtext);
		__GRRLIB_Print1w(xpos+0, ypos+0, tex, shadow, wtext);
		__GRRLIB_Print1w(xpos+2, ypos+2, tex, shadow, wtext);
	}
	if (outline) {
		__GRRLIB_Print1w(xpos-1, ypos-1, tex, outline, wtext);
		__GRRLIB_Print1w(xpos+1, ypos-1, tex, outline, wtext);
		__GRRLIB_Print1w(xpos-1, ypos+1, tex, outline, wtext);
		__GRRLIB_Print1w(xpos+1, ypos+1, tex, outline, wtext);
	}
	__GRRLIB_Print1w(xpos, ypos, tex, color, wtext);
    GRRLIB_DrawTile_end(tex);
}


void GRRLIB_Print(f32 xpos, f32 ypos, struct GRRLIB_texImg tex, u32 color, const char *text)
{
	GRRLIB_Print2(xpos, ypos, tex, color, 0, 0, text);
}


/**
 * Make a snapshot of the screen to a pre-allocated texture.
 * @return A pointer to a texture representing the screen or NULL if an error occurs.
 */
void GRRLIB_Screen2Texture_buf(GRRLIB_texImg *tx)
{
	if (tx == NULL) return;
	if (tx->data == NULL) return;
    if (tx->w != rmode->fbWidth) return;
    if (tx->h != rmode->efbHeight) return;
	GX_SetTexCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetTexCopyDst(rmode->fbWidth, rmode->efbHeight, GX_TF_RGBA8, GX_FALSE);
	GX_CopyTex(tx->data, GX_FALSE);
	GX_PixModeSync();
    GX_DrawDone();
    GRRLIB_FlushTex(*tx);
}

/**
 * Make a snapshot of the screen in a texture.
 * @return A pointer to a texture representing the screen or NULL if an error occurs.
 */
GRRLIB_texImg GRRLIB_Screen2Texture() {
    GRRLIB_texImg my_texture;

    my_texture.w = 0;
    my_texture.h = 0;
    my_texture.data = memalign (32, rmode->fbWidth * rmode->efbHeight * 4);
	if (my_texture.data == NULL) goto out;
    my_texture.w = rmode->fbWidth;
    my_texture.h = rmode->efbHeight;
	GRRLIB_Screen2Texture_buf(&my_texture);
	out:
    return my_texture;
}

/**
 * Make a snapshot of the screen to a pre-allocated texture.  Used for AA.
 * @return A pointer to a texture representing the screen or NULL if an error occurs.
 */
void GRRLIB_AAScreen2Texture_buf(GRRLIB_texImg *tx)
{
	if (tx == NULL) return;
	if (tx->data == NULL) return;
	if (tx->w != rmode->fbWidth) return;
	if (tx->h != rmode->efbHeight) return;

	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_TRUE);
	//GX_DrawDone();
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	GX_SetTexCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetTexCopyDst(rmode->fbWidth, rmode->efbHeight, GX_TF_RGBA8, GX_FALSE);
	GX_CopyTex(tx->data, GX_TRUE);
	GX_PixModeSync();
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);	
	GRRLIB_FlushTex(*tx);
}

/**
 * Make a snapshot of the screen in a texture.
 * @return A pointer to a texture representing the screen or NULL if an error occurs.
 */
GRRLIB_texImg GRRLIB_AAScreen2Texture() {
	GRRLIB_texImg my_texture;

	my_texture.w = 0;
	my_texture.h = 0;
	my_texture.data = memalign (32, rmode->fbWidth * rmode->efbHeight * 4);
	if (my_texture.data == NULL) goto out;
	my_texture.w = rmode->fbWidth;
	my_texture.h = rmode->efbHeight;
	out:
	return my_texture;
}

/**
 * Resets all the GX settings to the default
 */
void GRRLIB_ResetVideo() {
    f32 yscale;
    u32 xfbHeight;
    Mtx44 perspective;

    // other gx setup
	yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetDispCopyGamma(GX_GM_1_0);

    // setup the vertex descriptor
    // tells the flipper to expect direct data
    GX_ClearVtxDesc();
    GX_InvVtxCache ();
    GX_InvalidateTexAll();

    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);


    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_TRUE);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    guMtxIdentity(GXmodelView2D);
    guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
    GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);

    guOrtho(perspective,0, 480, 0, 640, 0, 300.0F);
    GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetAlphaUpdate(GX_TRUE);

    GX_SetCullMode(GX_CULL_NONE);
}

//AA jitter values
const float _jitter2[2][2] = {
	{ 0.246490f, 0.249999f },
	{ -0.246490f, -0.249999f }
};
const float _jitter3[3][2] = {
	{ -0.373411f, -0.250550f },
	{ 0.256263f, 0.368119f },
	{ 0.117148f, -0.117570f }
};
/*
const float _jitter4[4][2] = {
	{ -0.208147f, 0.353730f },
	{ 0.203849f, -0.353780f },
	{ -0.292626f, -0.149945f },
	{ 0.296924f, 0.149994f }
};
*/
const float _jitter4[4][2] = {
	{ -0.108147f, 0.253730f },
	{ 0.103849f, -0.253780f },
	{ -0.192626f, -0.049945f },
	{ 0.196924f, 0.049994f }
};


void GRRLIB_prepareAAPass(int aa_cnt, int aaStep) {
	float x = 0.0f;
	float y = 0.0f;
	u32 w = rmode->fbWidth;
	u32 h = rmode->efbHeight;
	switch (aa_cnt) {
		case 2:
			x += _jitter2[aaStep][0];
			y += _jitter2[aaStep][1];
			break;
		case 3:
			x += _jitter3[aaStep][0];
			y += _jitter3[aaStep][1];
			break;
		case 4:
			x += _jitter4[aaStep][0];
			y += _jitter4[aaStep][1];
			break;
	}
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetViewport(0+x, 0+y, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetScissor(0, 0, w, h);
	GX_InvVtxCache();
	GX_InvalidateTexAll();
}

void GRRLIB_drawAAScene(int aa_cnt, GRRLIB_texImg *texAABuffer) {
	GXTexObj texObj[4];
	Mtx modelViewMtx;
	u8 texFmt = GX_TF_RGBA8;
	u32 tw = rmode->fbWidth;
	u32 th = rmode->efbHeight;
	float w = 640.f;
	float h = 480.f;
	float x = 0.f;
	float y = 0.f;
	int i = 0;

	GX_SetNumChans(0);
	for (i = 0; i < aa_cnt; ++i) {
		GX_InitTexObj(&texObj[i], texAABuffer[i].data, tw , th, texFmt, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_LoadTexObj(&texObj[i], GX_TEXMAP0 + i);
	}
	
	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
		
	GX_SetTevKColor(GX_KCOLOR0, (GXColor){0xFF / 1, 0xFF / 5, 0xFF, 0xFF});    // Renders better gradients than 0xFF / aa
	GX_SetTevKColor(GX_KCOLOR1, (GXColor){0xFF / 2, 0xFF / 6, 0xFF, 0xFF});
	GX_SetTevKColor(GX_KCOLOR2, (GXColor){0xFF / 3, 0xFF / 7, 0xFF, 0xFF});
	GX_SetTevKColor(GX_KCOLOR3, (GXColor){0xFF / 4, 0xFF / 8, 0xFF, 0xFF});
	for (i = 0; i < aa_cnt; ++i) {
		GX_SetTevKColorSel(GX_TEVSTAGE0 + i, GX_TEV_KCSEL_K0_R + i);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0 + i, GX_TEV_KASEL_K0_R + i);
		GX_SetTevOrder(GX_TEVSTAGE0 + i, GX_TEXCOORD0, GX_TEXMAP0 + i, GX_COLORNULL);
		GX_SetTevColorIn(GX_TEVSTAGE0 + i, i == 0 ? GX_CC_ZERO : GX_CC_CPREV, GX_CC_TEXC, GX_CC_KONST, GX_CC_ZERO);
		GX_SetTevAlphaIn(GX_TEVSTAGE0 + i, i == 0 ? GX_CA_ZERO : GX_CA_APREV, GX_CA_TEXA, GX_CA_KONST, GX_CA_ZERO);
		GX_SetTevColorOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	}
  
/*	
	GX_SetTevKColor(GX_KCOLOR0, (GXColor){0xFF / aa_cnt, 0, 0, 0}); // Causes a color accuracy loss, in previous and better code i was using the final blender with alpha = 1/1, 1/2, 1/3 etc.
	for (i = 0; i < aa_cnt; ++i) {
		GX_SetTevKColorSel(GX_TEVSTAGE0 + i, GX_TEV_KCSEL_K0_R);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0 + i, GX_TEV_KASEL_K0_R);
		GX_SetTevOrder(GX_TEVSTAGE0 + i, GX_TEXCOORD0, GX_TEXMAP0 + i, GX_COLORNULL);
		GX_SetTevColorIn(GX_TEVSTAGE0 + i, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, i == 0 ? GX_CC_ZERO : GX_CC_CPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0 + i, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, i == 0 ? GX_CA_ZERO : GX_CA_APREV);
		GX_SetTevColorOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	}
*/

	GX_SetNumTevStages(aa_cnt);
	//
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	//
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	guMtxIdentity(modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(x, y, 0.f);
		GX_TexCoord2f32(0.f, 0.f);
		GX_Position3f32(x + w, y, 0.f);
		GX_TexCoord2f32(1.f, 0.f);
		GX_Position3f32(x + w, y + h, 0.f);
		GX_TexCoord2f32(1.f, 1.f);
		GX_Position3f32(x, y + h, 0.f);
		GX_TexCoord2f32(0.f, 1.f);
	GX_End();

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
}
