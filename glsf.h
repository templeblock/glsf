/**
 * Copyright (c) 2012 Kim Syrj�l� <kim.syrjala@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 */

#ifndef __GLSF_H__
#define __GLSF_H__

#include <GL/gl.h>
#include <stdint.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "utf8.h"

typedef struct {
    uint32_t codepoint;
    int32_t  x0, y0, x1, y1;
    float    scale;
    int32_t  index, advance, offset;
} GLSFglyph;

typedef struct {
    int32_t  width, height;
    uint8_t* data;
} GLSFbitmap;

typedef struct {
    uint32_t name;
    int32_t  width, height;
} GLSFtexture;

typedef struct {
    float x, y;
    float u, v;
    float r, g, b, a;
} GLSFvertex;

typedef struct {
    stbtt_fontinfo info;
    uint8_t*       data;
    int32_t        ascent, descent, linegap;
    float          size;
    GLSFglyph*     glyphs;
    size_t         num_glyphs;
    GLSFvertex*    vertices;
    size_t         num_vertices, max_vertices;
    GLSFtexture    texture;
} GLSFfont;

static GLSFfont* _glsf_font = NULL;

static GLSFfont*  glsfCreateFont( const char*, float, const char* );
static void       glsfDestroyFont( GLSFfont* );
static int32_t    glsfLoadGlyph( GLSFfont*, uint32_t, GLSFglyph* );
static int32_t    glsfLoadBitmap( GLSFfont*, GLSFglyph*, GLSFbitmap* );
static void       glsfFreeBitmap( GLSFbitmap* );
static int32_t    glsfLoadTexture( GLSFfont*, GLSFglyph*, size_t, GLSFtexture* );
static void       glsfFreeTexture( GLSFtexture* );
static int32_t    glsfUpdateFont( GLSFfont*, GLSFglyph*, size_t );
static GLSFglyph* glsfGetGlyph( GLSFfont*, uint32_t );
static void       glsfDrawString( GLSFfont*, const float[4], const float[4], const char* );
static void       glsfBegin( GLSFfont* );
static void       glsfEnd();
static void       glsfString( const float[4], const float[4], const char* );

/**
 * @fn glsfLoadGlyph
 */
static int32_t glsfLoadGlyph( GLSFfont* font, uint32_t codepoint, 
                              GLSFglyph* glyph )
{
    glyph->index = stbtt_FindGlyphIndex(&font->info, codepoint);
    if(glyph->index == 0) 
        return GL_FALSE;
    
    glyph->codepoint = codepoint;
    glyph->scale = stbtt_ScaleForPixelHeight(&font->info, font->size);
    
    int32_t lsb;
    stbtt_GetGlyphHMetrics(&font->info, glyph->index, 
                           &glyph->advance, &lsb);
    stbtt_GetGlyphBitmapBox(&font->info, glyph->index, glyph->scale,
                            glyph->scale, &glyph->x0, &glyph->y0,
                            &glyph->x1, &glyph->y1);
    
    return GL_TRUE;
}

/**
 * @fn glsfLoadBitmap
 */
static int32_t glsfLoadBitmap( GLSFfont* font, GLSFglyph* glyph, 
                               GLSFbitmap* bitmap )
{
    bitmap->width = glyph->x1 - glyph->x0;
    bitmap->height = glyph->y1 - glyph->y0;
    bitmap->data = (uint8_t*)malloc(sizeof(uint8_t) * 
                      (bitmap->width * bitmap->height));
    if(!bitmap->data) 
        return GL_FALSE;
    
    stbtt_MakeGlyphBitmap(&font->info, bitmap->data, bitmap->width, 
                          bitmap->height, bitmap->width, glyph->scale, 
                          glyph->scale, glyph->index);

    return GL_TRUE;
}

/**
 * @fn glsfFreeBitmap
 */
static void glsfFreeBitmap( GLSFbitmap* bitmap )
{
    if(bitmap->data)
        free(bitmap->data);
    memset(bitmap, 0, sizeof(GLSFbitmap));
}

/**
 * @fn glsfLoadTexture
 */
static int32_t glsfLoadTexture( GLSFfont* font, GLSFglyph* glyphs, 
                                size_t num_glyphs, GLSFtexture* texture )
{
    // Find out required dimensions for texture.
    int32_t req_width = 0, req_height = 0;
    uint32_t i;
    for(i = 0; i < num_glyphs; ++i) {
        int32_t width = glyphs[i].x1 - glyphs[i].x0;
        int32_t height = glyphs[i].y1 - glyphs[i].y0;
        req_width += width;
        if(height > req_height)
            req_height = height;
    }
    
    if(req_width < 2 || req_height < 2) {
        fprintf(stderr, "Invalid texture size: %ix%i\n", req_width, req_height);
        return GL_FALSE;
    }
    
    texture->width = req_width;
    texture->height = req_height;
    
    // Create a blank texture.
    glGenTextures(1, &texture->name);
    glBindTexture(GL_TEXTURE_2D, texture->name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texture->width, texture->height, 
                 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
    
    // Load glyph bitmaps in offsets.
    int32_t x_offset = 0;
    for(i = 0; i < num_glyphs; ++i) {
        GLSFbitmap bitmap;
        if(glsfLoadBitmap(font, &glyphs[i], &bitmap) == GL_FALSE)
            continue;
        
        glTexSubImage2D(GL_TEXTURE_2D, 0, x_offset, 0, bitmap.width,
                        bitmap.height, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.data);
        
        glyphs[i].offset = x_offset;
        x_offset += bitmap.width;
        
        glsfFreeBitmap(&bitmap);
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return GL_TRUE;
}

/**
 * @fn glsfFreeTexture
 */
static void glsfFreeTexture( GLSFtexture* texture )
{
    if(texture->name)
        glDeleteTextures(1, &texture->name);
    memset(texture, 0, sizeof(GLSFtexture));
}

/**
 * @fn glsfUpdateFont
 */
static int32_t glsfUpdateFont( GLSFfont* font, GLSFglyph* glyphs, 
                               size_t num_glyphs )
{
    if(num_glyphs == 0)
        return GL_FALSE;
    
    // Allocate new memory for concatenated glyphs.
    size_t num_new_glyphs = font->num_glyphs + num_glyphs;
    GLSFglyph* new_glyphs = (GLSFglyph*)malloc(sizeof(GLSFglyph) * num_new_glyphs);
    if(new_glyphs == NULL) 
        return GL_FALSE;
    
    // Copy old glyphs.
    if(font->num_glyphs > 0)
        memcpy(new_glyphs, font->glyphs, sizeof(GLSFglyph) * font->num_glyphs);
    memcpy(new_glyphs + font->num_glyphs, glyphs, sizeof(GLSFglyph) * num_glyphs);
    
    // Attempt creating a new texture.
    GLSFtexture new_texture;
    if(glsfLoadTexture(font, new_glyphs, num_new_glyphs, &new_texture) == GL_FALSE) {
        free(new_glyphs);
        return GL_FALSE;
    }
    
    // Replace old glyphs and texture.
    free(font->glyphs);
    font->glyphs = new_glyphs;
    font->num_glyphs = num_new_glyphs;
    glsfFreeTexture(&font->texture);
    font->texture = new_texture;
    
    return GL_TRUE;
}

/**
 * @fn glsfGetGlyph
 * @brief Fetch a glyph by codepoint from font. Tries loading the glyph
 *        and adding it if not found.
 */
static GLSFglyph* glsfGetGlyph( GLSFfont* font, uint32_t codepoint )
{
    // Fetch existing glyph in font.
    uint32_t i;
    for(i = 0; i < font->num_glyphs; ++i)
        if(font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    
    // Load the missing glyph.
    GLSFglyph new_glyph;
    if(glsfLoadGlyph(font, codepoint, &new_glyph) == GL_FALSE)
        return NULL;
    
    // Attempt updating font with new glyph.
    if(glsfUpdateFont(font, &new_glyph, 1) == GL_FALSE)
        return NULL;
    
    // Success.
    return glsfGetGlyph(font, codepoint);
    //return &font->glyphs[font->num_glyphs - 1];
}

/**
 * @fn glsfCreateFont
 */
static GLSFfont* glsfCreateFont( const char* filename, float size,
                                 const char* pre )
{
    // Read file.
    FILE* file = fopen(filename, "rb");
    if(!file) {
        fprintf(stderr, "Failed opening \"%s\".\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    size_t filesize = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t* buffer = (uint8_t*)malloc(filesize);
    if(!buffer) {
        fprintf(stderr, "Failed allocating file buffer.\n");
        fclose(file);
        return NULL;
    }
    fread(buffer, 1, filesize, file);
    fclose(file);
    
    // Create and initialize font.
    GLSFfont* new_font = (GLSFfont*)malloc(sizeof(GLSFfont));
    memset(new_font, 0, sizeof(GLSFfont));
   
    if(!stbtt_InitFont(&new_font->info, buffer, 0)) {
        fprintf(stderr, "Failed initializing font.\n");
        free(buffer);
        glsfDestroyFont(new_font);
        return NULL;
    }
    
    stbtt_GetFontVMetrics(&new_font->info, &new_font->ascent,
                          &new_font->descent, &new_font->linegap);
    new_font->size = size;
    new_font->data = buffer;
    
    // Initialize vertex array with some kind of size.
    new_font->vertices = (GLSFvertex*)malloc(sizeof(GLSFvertex) * 128);
    new_font->max_vertices = 128;

    // Preload some glyphs.
    if(strlen(pre) > 0) {
        GLSFglyph* new_glyphs = (GLSFglyph*)malloc(sizeof(GLSFglyph) * strlen(pre));
        uint32_t num_new_glyphs = 0;
        uint32_t state, codepoint;
        uint32_t i, j;
        for(state = UTF8_ACCEPT, i = 0; i < strlen(pre); ++i) {
            if(decutf8(&state, &codepoint, (uint8_t)pre[i]))
                continue;
            // Skip duplicates.
            int32_t duplicate = GL_FALSE;
            for(j = 0; j < i; ++j) {
                if(codepoint == new_glyphs[j].codepoint) {
                    duplicate = GL_TRUE;
                    break;
                }
            }
            if(duplicate == GL_FALSE && 
               glsfLoadGlyph(new_font, codepoint, &new_glyphs[num_new_glyphs]) == GL_TRUE) {
                num_new_glyphs++;
            }
        }
        glsfUpdateFont(new_font, new_glyphs, num_new_glyphs);
        free(new_glyphs);
    }
    
    return new_font;
}

/**
 * @fn glsfDestroyFont
 */
static void glsfDestroyFont( GLSFfont* font )
{
    glsfFreeTexture(&font->texture);
    
    if(font->data)
        free(font->data);
    if(font->glyphs)
        free(font->glyphs);
    if(font->vertices)
        free(font->vertices);

    free(font);
}

/**
 * @fn glsfEnqueueGlyph
 * @brief Adds vertices for a glyph in font's vertex array.
 */
static void glsfEnqueueGlyph( GLSFfont* font, GLSFglyph* glyph, float x, 
                              float y, const float color[4] )
{
    if(font->num_vertices + 6 > font->max_vertices)
        return;

    // Get viewport.
    int32_t viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Glyph dimensions in pixels. 
    float gw = glyph->x1 - glyph->x0;
    float gh = glyph->y1 - glyph->y0;
    
    // Texture dimensions. 
    float tw = font->texture.width;
    float th = font->texture.height;
    
    // Quad coords in pixels. 
    float x0 = x;
    float y0 = -y - glyph->y1; // glyph->y1 for vertical offset in bbox 
    float x1 = x0 + gw;
    float y1 = y0 + gh;
    
    // Texcoords. 
    float u0 = (float)glyph->offset / tw;
    float v0 = gh / th;
    float u1 = u0 + (gw / tw);
    float v1 = 0;
    
    // Offset Y with viewport height so zero is top. 
    y0 = y0 + viewport[3] - th;
    y1 = y1 + viewport[3] - th;
    
    // Transform quad coords into screen space. 
    x0 = (x0 / viewport[2]) * 2 - 1;
    y0 = (y0 / viewport[3]) * 2 - 1;
    x1 = (x1 / viewport[2]) * 2 - 1;
    y1 = (y1 / viewport[3]) * 2 - 1;
    
    // Add to vertex array.
    #define GLSF_VERTEX( X, Y, U, V ) \
        font->vertices[font->num_vertices].x = X;\
        font->vertices[font->num_vertices].y = Y;\
        font->vertices[font->num_vertices].u = U;\
        font->vertices[font->num_vertices].v = V;\
        font->vertices[font->num_vertices].r = color[0];\
        font->vertices[font->num_vertices].g = color[1];\
        font->vertices[font->num_vertices].b = color[2];\
        font->vertices[font->num_vertices].a = color[3];\
        font->num_vertices++;
    
    GLSF_VERTEX(x0, y0, u0, v0);
    GLSF_VERTEX(x0, y1, u0, v1);
    GLSF_VERTEX(x1, y0, u1, v0);
    GLSF_VERTEX(x0, y1, u0, v1);
    GLSF_VERTEX(x1, y1, u1, v1);
    GLSF_VERTEX(x1, y0, u1, v0);
    
    #undef GLSF_VERTEX
}

/**
 * @fn glsfEnqueueString
 * @brief Prepare a string to be drawn for font by calling EnqueueGlyph
 *        for each character.
 */
static void glsfEnqueueString( GLSFfont* font, const float rect[4],
                               const float color[4], const char* string )
{
    // Count number of glyphs in UTF-8 string.
    uint32_t i, num_glyphs = 0;
    uint32_t state, codepoint;
    for(state = UTF8_ACCEPT, i = 0; i < strlen(string); ++i) {
        if(decutf8(&state, &codepoint, (uint8_t)string[i]))
            continue;
        num_glyphs++;
    }
    
    // Number of vertices needed for this string.
    size_t num_vertices = num_glyphs * 6;
    
    // Resize vertex array if needed.
    if(num_vertices > font->max_vertices - font->num_vertices) {
        size_t max_vertices = font->num_vertices + num_vertices;
        GLSFvertex* vertices = (GLSFvertex*)malloc(sizeof(GLSFvertex)*max_vertices);
        memcpy(vertices, font->vertices, sizeof(GLSFvertex)*font->num_vertices);
        free(font->vertices);
        font->vertices = vertices;
        font->max_vertices = max_vertices;
    }
    
    // Vertical advance.
    float adv_y = (float)font->texture.height;
    
    // Add glyphs to vertex array.
    float cur_x = 0, cur_y = 0;
    for(state = UTF8_ACCEPT, i = 0; i < strlen(string); ++i) {
        if(decutf8(&state, &codepoint, *((uint8_t*)&string[i])))
            continue;
        
        // Handle newlines.
        if(codepoint == '\n') {
            cur_x = 0;
            cur_y += adv_y;
            continue;
        }
        
        GLSFglyph* glyph = glsfGetGlyph(font, codepoint);
        if(!glyph)
            continue;
        
        // Ignore space after newline.
        if(cur_x == 0 && glyph->codepoint == ' ')
            continue;
        
        // Horizontal Advance.
        float adv_x = (float)glyph->advance * glyph->scale;
        
        // Handle linebreaking.
        if(cur_x + adv_x > rect[2]) {
            cur_x = 0;
            cur_y += adv_y;
        }
        
        glsfEnqueueGlyph(font, glyph, cur_x + rect[0], cur_y + rect[1], color);
        
        // Advance cursor.
        cur_x += adv_x;
    }
}

/**
 * @fn glsfDrawFont
 * @brief Draws a font's vertices after some calls to EnqueueString.
 */
static void glsfDrawFont( GLSFfont* font )
{
    // Enough vertices for anything to be drawn?
    if(font->num_vertices < 6)
        return;

    // Backup some states.
    float modelview[16];
    float projection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    
    // Set required states.
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(GLSFvertex), font->vertices);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLSFvertex), &((float*)font->vertices)[2]);
    glColorPointer(4, GL_FLOAT, sizeof(GLSFvertex), &((float*)font->vertices)[4]);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, font->texture.name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Draw the vertices.
    glDrawArrays(GL_TRIANGLES, 0, font->num_vertices);

    // Restore states.
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(modelview);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Mark vertices drawn.
    font->num_vertices = 0;
}

/**
 * @fn glsfDrawString
 */
static void glsfDrawString( GLSFfont* font, const float rect[4], 
                            const float color[4], const char* string )
{
    glsfEnqueueString(font, rect, color, string);
    glsfDrawFont(font);
}

/**
 * @fn glsfBegin
 */
static void glsfBegin( GLSFfont* font )
{
    _glsf_font = font;
}

/**
 * @fn glsfEnd
 */
static void glsfEnd()
{
    glsfDrawFont(_glsf_font);
    _glsf_font = NULL;
}

/**
 * @fn glsfString
 */
static void glsfString( const float rect[4], const float color[4],
                        const char* string )
{
    glsfEnqueueString(_glsf_font, rect, color, string);
}

#endif
