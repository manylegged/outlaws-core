
//
// Graphics.cpp - drawing routines
// 
// Created on 10/31/12.
// 

// Copyright (c) 2013-2016 Arthur Danskin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#include "StdAfx.h"
#include "Graphics.h"
#include "Shaders.h"
#include "Event.h"

#ifndef ASSERT_MAIN_THREAD
#define ASSERT_MAIN_THREAD()
#endif

// #define DEBUG_RENDER (kDebugFlags&EDebug::GLERROR)
#define DEBUG_RENDER (globals.debugRender&DBG_GLERROR)

template struct TriMesh<VertexPosColor>;
template struct LineMesh<VertexPosColor>;
template struct MeshPair<VertexPosColor, VertexPosColor>;

#ifndef GL_CLAMP_TO_EDGE_SGIS
#define GL_CLAMP_TO_EDGE_SGIS GL_CLAMP_TO_EDGE
#endif

#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV GL_BGRA
#endif

bool supports_ARB_Framebuffer_object = false;

static const uint kDebugFrames = 10;
extern bool kHeadlessMode;

bool isGLExtensionSupported(const char *name)
{
    static const char* val = (const char*) glGetString(GL_EXTENSIONS);
    return str_contains(val, name);
}

uint graphicsDrawCount = 0;
uint gpuMemoryUsed = 0;

void deleteBufferInMainThread(GLuint buffer)
{
    globals.deleteGLBuffers(1, &buffer);
}

static const char* getGLErrorString(GLenum err)
{
    switch (err)
    {
        CASE_STR(GL_NO_ERROR);
        CASE_STR(GL_INVALID_ENUM);
        CASE_STR(GL_INVALID_VALUE);
        CASE_STR(GL_INVALID_OPERATION);
        CASE_STR(GL_OUT_OF_MEMORY);
    default: return "UNKNOWN";
    }
}

GLenum glReportError1(const char *file, uint line, const char *function)
{
    ASSERT_MAIN_THREAD();

#if !IS_DEVEL
    if (!DEBUG_RENDER && globals.frameStep > kDebugFrames)
        return GL_NO_ERROR;
#endif

	GLenum err = GL_NO_ERROR;
	while ((err = glGetError()) != GL_NO_ERROR)
    {
        OLG_OnAssertFailed(file, line, function, "glGetError", "%s", getGLErrorString(err));
    }
    
	return err;
}


static string getGLFrameBufferStatusString(GLenum err)
{
    switch(err) {
        case 0: return "Error checking framebuffer status";
        CASE_STR(GL_FRAMEBUFFER_COMPLETE);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        CASE_STR(GL_FRAMEBUFFER_UNSUPPORTED);
#if OPENGL_ES
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS);
#else
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT);
        CASE_STR(GL_FRAMEBUFFER_UNDEFINED);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_EXT);
#endif
        default: return str_format("0x%0x", err);
    }
}

static GLenum glReportFramebufferError1(const char *file, uint line, const char *function)
{
    ASSERT_MAIN_THREAD();

    if (!DEBUG_RENDER && globals.frameStep > kDebugFrames)
        return GL_NO_ERROR;

	GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (GL_FRAMEBUFFER_COMPLETE != err)
    {
        OLG_OnAssertFailed(file, line, function, "glCheckFramebufferStatus", "%s", getGLFrameBufferStatusString(err).c_str());
    }
    
	return err;
}

#define glReportFramebufferError() glReportFramebufferError1(__FILE__, __LINE__, __func__)

void GLScope::set(bool val)
{
    DASSERT(type == BOOL);
    if ((bool)current.bval == val)
        return;
    current.bval = val;
                   
    switch (enm)
    {
    case GL_DEPTH_WRITEMASK:
        glDepthMask(val ? GL_TRUE : GL_FALSE);
        break;
    case GL_BLEND:
    case GL_DEPTH_TEST:
    case GL_ALPHA_TEST:
        if (val)
            glEnable(enm);
        else
            glDisable(enm);
        break;
    default:
        ASSERT_FAILED("GLScope", "Unknown bool enum %#x", (int)enm);
    }
}

void GLScope::set(float val)
{
    DASSERT(type == FLOAT);
    if (current.fval == val)
        return;
    current.fval = val;
        
    switch (enm)
    {
    case GL_LINE_WIDTH:
        glLineWidth(val);
        break;
    default:
        ASSERT_FAILED("GLScope", "Unknown float enum %#x", (int)enm);
    }
}

GLScope::~GLScope()
{
    switch (type)
    {
    case BOOL: set((bool)saved.bval); break;
    case FLOAT: set((float)saved.fval); break;
    }
}

GLScopeBlend::GLScopeBlend(GLenum srgb, GLenum drgb,
             GLenum salpha, GLenum dalpha)
{
    if (salpha == 0)
        salpha = srgb;
    if (dalpha == 0)
        dalpha = drgb;
    glGetIntegerv(GL_BLEND_SRC_RGB, &src_color);
    glGetIntegerv(GL_BLEND_DST_RGB, &dst_color);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);

    changed = (srgb != src_color || drgb != dst_color || salpha != src_alpha || dalpha != dst_alpha);
    if (changed)
        glBlendFuncSeparate(srgb, drgb, salpha, dalpha);
}

GLScopeBlend::~GLScopeBlend()
{
    if (changed)
        glBlendFuncSeparate(src_color, dst_color, src_alpha, dst_alpha);
}


static bool ignoreShaderLog(const char* buf)
{
    // damnit ATI driver
    static string kNoErrors = "No errors.\n";
    return (buf == kNoErrors ||
            str_contains(buf, "Validation successful") ||
            str_contains(buf, "successfully compiled") ||
            str_contains(buf, "shader(s) linked."));
}

static void checkProgramInfoLog(GLuint prog, const char* name, LogRecorder *logger)
{
    const uint bufsize = 2048;
    char buf[bufsize];
    GLsizei length = 0;
    glGetProgramInfoLog(prog, bufsize, &length, buf);
    if (length && !ignoreShaderLog(buf))
    {
        if (logger)
            logger->Report(buf);
        OLG_OnAssertFailed(name, -1, "", "", "GL Program Info log for '%s': %s", name, buf);
    }
}

void glReportValidateShaderError1(const char *file, uint line, const char *function, GLuint program, const char *name)
{
    ASSERT_MAIN_THREAD();

    if (!DEBUG_RENDER && globals.frameStep > kDebugFrames)
        return;

    glValidateProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
    checkProgramInfoLog(program, "validate", NULL);
    glReportError1(file, line, function);
    ASSERT_(status == GL_TRUE, file, line, function, "%s", name);
}


vector<GLRenderTexture*> GLRenderTexture::s_bound;

GLRenderTexture* GLRenderTexture::getBound(int idx)
{
    return vec_at(s_bound, -idx - 1);
}

static const char* textureFormatToString(GLint fmt)
{
    switch (fmt) {
        CASE_STR(GL_RGB);
        CASE_STR(GL_RGBA);
        CASE_STR(GL_BGRA);
        CASE_STR(GL_ALPHA);
        CASE_STR(GL_LUMINANCE_ALPHA);
        CASE_STR(GL_LUMINANCE);
#if OPENGL_ES
        CASE_STR(GL_RGB16F_EXT);
#else
        CASE_STR(GL_BGR);
        CASE_STR(GL_RGBA16F_ARB);
        CASE_STR(GL_RGB16F_ARB);
#endif
        default: return "<unknown>";
    }
}

static int textureFormatBytesPerPixel(GLint fmt)
{
    switch (fmt) {
#if OPENGL_ES
    case GL_RGB16F_EXT:
#else
    case GL_RGB16F_ARB:
    case GL_RGBA16F_ARB:
#endif
        return 2 * 4;
    case GL_LUMINANCE_ALPHA:
        return 2;
    case GL_LUMINANCE:
    case GL_ALPHA:
        return 1;
    default:
        return 4;
    }
}

static GLint s_defaultFramebuffer = -1;

uint GLRenderTexture::getSizeBytes() const
{
    uint size = GLTexture::getSizeBytes();
    if (m_zflags&HASZ)
        size += m_texsize.x * m_texsize.y * 2;
    return size;
}

void GLRenderTexture::Generate(ZFlags zflags)
{
    ASSERT_MAIN_THREAD();

    ASSERT(m_size.x >= 1 && m_size.y >= 1);

    GLsizei width = m_size.x;
    GLsizei height = m_size.y;
    
#if OPENGL_ES
    // textures must be a power of 2 on ios
    width = roundUpPower2(width);
    height = roundUpPower2(height);
#endif
    
    if (s_defaultFramebuffer < 0 && !kHeadlessMode)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s_defaultFramebuffer);
    }

    m_texsize = float2(width, height);
    DPRINT(SHADER, ("Generating render texture, %dx%d %s %s", width, height,
                    textureFormatToString(m_format), (zflags&HASZ) ? "Z16" : "No_Z"));

    glReportError();
    glGenFramebuffers(1, &m_fbname);
    glReportError();

    glGenTextures(1, &m_texname);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glReportError();
 
#if OPENGL_ES
    if (m_format == GL_RGBA16F_EXT)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_HALF_FLOAT_OES, 0);
    }
    else
#endif
    {
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    }
    glReportError();

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbname);
    glReportError();

    // The depth buffer
    if (zflags&HASZ)
    {
        glGenRenderbuffers(1, &m_zrbname);
        glReportError();
        glBindRenderbuffer(GL_RENDERBUFFER, m_zrbname);
        glReportError();
        
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glReportError();
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_zrbname);
        glReportError();
    }
    else
    {
        m_zrbname = 0;
    }

    // Set "renderedTexture" as our colour attachement #0
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texname, 0);
    glReportError();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#if 0 
    // Set the list of draw buffers.
    GLenum DrawBuffers[2] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
#endif

    gpuMemoryUsed += getSizeBytes();

    // Always check that our framebuffer is ok
    glReportError();
    glReportFramebufferError();
}

void GLRenderTexture::clear()
{
    if (m_fbname)
        globals.deleteGLFramebuffers(1, &m_fbname);
    if (m_texname) {
        globals.deleteGLTextures(1, &m_texname);
    }
    if (m_zrbname) {
        globals.deleteGLRenderbuffers(1, &m_zrbname);
    }
    gpuMemoryUsed -= getSizeBytes();
    m_size = float2(0.f);
    m_texsize = float2(0.f);
    m_texname = 0;
    m_fbname = 0;
    m_zrbname = 0;
}

#if __APPLE__
#define HAS_BLIT_FRAMEBUFFER 1
#else
#define HAS_BLIT_FRAMEBUFFER glBlitFramebuffer
#endif

void GLRenderTexture::BindFramebuffer(float2 size, ZFlags zflags)
{
    ASSERT(!nearZero(size));
    if (size != m_size || ((zflags&HASZ) && !(m_zflags&HASZ)))
        clear();
    m_size = size;
    m_zflags = zflags;
    if (!m_fbname)
        Generate(zflags);
    RebindFramebuffer();

#if !OPENGL_ES
    const GLint def = s_bound.size() ? s_bound.back()->m_fbname : s_defaultFramebuffer;
    if ((zflags == KEEPZ) && HAS_BLIT_FRAMEBUFFER && def >= 0)
    {
        ASSERT(def != m_fbname);
        const float2 lastSize = s_bound.size() ? s_bound.back()->m_size : globals.windowSizePixels;
        ASSERT(lastSize.x > 0.f && lastSize.y > 0.f);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, def);
        glReportError();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbname);
        glReportError();
        // only GL_NEAREST is supported for depth buffers
        glBlitFramebuffer(0, 0, lastSize.x, lastSize.y, 0, 0, m_size.x, m_size.y,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glReportError();
    }
    else
#endif
    if (zflags&HASZ)
    {
        glClear(GL_DEPTH_BUFFER_BIT);
        glReportError();
    }
    
    ASSERT(s_bound.empty() || s_bound.back() != this);
    s_bound.push_back(this);
}

void GLRenderTexture::RebindFramebuffer()
{
    ASSERT(m_size.x >= 1 && m_size.y >= 1);
    ASSERT(m_fbname && m_texname);

    BindTexture(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_SGIS);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbname);
    glReportFramebufferError();
    glViewport(0,0,m_size.x,m_size.y);
    glReportError();
}

void GLRenderTexture::UnbindFramebuffer() const
{
    ASSERT(s_bound.size() && s_bound.back() == this);
    s_bound.pop_back();
    
    if (s_bound.size())
    {
        s_bound.back()->RebindFramebuffer();
    }
    else
    {
        glReportFramebufferError();
        if (s_defaultFramebuffer >= 0)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, s_defaultFramebuffer);
            glReportError();
            glReportFramebufferError();
            glViewport(0, 0, globals.viewportSizePixels.x, globals.viewportSizePixels.y);
            glReportError();
        }
    }
}


uint GLTexture::getSizeBytes() const
{
    return m_texsize.x * m_texsize.y * textureFormatBytesPerPixel(m_format);
}

void GLTexture::clear()
{
    if (m_texname)
    {
        gpuMemoryUsed -= getSizeBytes();
        globals.deleteGLTextures(1, &m_texname);
    }
    m_texname = 0;
}

void GLTexture::TexImage2D(GLenum int_format, int2 size, GLenum format, GLenum type, const void *data)
{
    if (!int_format)
    {
        switch (format)
        {
        case GL_RGBA: case GL_RGB:
        case GL_LUMINANCE_ALPHA: case GL_LUMINANCE: case GL_ALPHA:
            int_format = format;
            break;
        default:
        case GL_BGRA:
            int_format = GL_RGBA;
            break;
        case GL_BGR:
            int_format = GL_RGB;
            break;
        }
    }
    
    m_format = int_format;
    if (!m_texname) {
        glGenTextures(1, &m_texname);
    } else {
        gpuMemoryUsed -= getSizeBytes();
    }
    glBindTexture(GL_TEXTURE_2D, m_texname);

#if OPENGL_ES
    m_texsize = float2(roundUpPower2(size.x), roundUpPower2(size.y));
    glTexImage2D(GL_TEXTURE_2D, 0, format,
                 m_texsize.x, m_texsize.y, 0, format, GL_UNSIGNED_BYTE, data);
#else
    m_texsize = float2(size);
    glTexImage2D(GL_TEXTURE_2D, 0, m_format,
                 size.x, size.y, 0, format, type, data);
#endif

    glReportError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_size = float2(size);
    gpuMemoryUsed += getSizeBytes();
}

void GLTexture::SetTexWrap(bool enable)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    const GLint param = enable ? GL_REPEAT :  GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, param);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, param);
}

void GLTexture::SetTexMagFilter(GLint filter)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLTexture::GenerateMipmap()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glReportError();
}

bool GLTexture::loadFile(const char* fname)
{
    clear();
    OutlawImage image = OL_LoadImage(fname);
    if (!image.data)
        return false;

    TexImage2D(image);
    m_flipped = true;

    glBindTexture(GL_TEXTURE_2D, m_texname);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    OL_FreeImage(&image);
    return true;
}

static void invert_image(uint *pix, int width, int height)
{
    for (int y=0; y<height/2; y++)
    {
        for (int x=0; x<width; x++)
        {
            const int top = y * width + x;
            const int bot = (height - y - 1) * width + x;
            const uint temp = pix[top];
            pix[top] = pix[bot];
            pix[bot] = temp;
        }
    }
}

bool GLTexture::writeFile(const char *fname) const
{
#if OPENGL_ES
    return false;
#else
    const int2 sz = ceil_int(m_texsize);
    const size_t size = sz.x * sz.y * 4;
    if (size == 0 || m_texname == 0)
        return false;
    
    uint *pix = (uint*) malloc(size);

    OutlawImage img = {};
    img.width = sz.x;
    img.height = sz.y;
    img.format = GL_RGBA;
    img.type = GL_UNSIGNED_BYTE;
    img.data = (char*) pix;

    BindTexture(0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    glReportError();

    invert_image(pix, sz.x, sz.y);

    const int success = OL_SaveImage(&img, fname);
    free(pix);
    return success;
#endif
}


GLTexture PixImage::uploadTexture() const
{
    GLTexture tex;
    tex.TexImage2D(GL_RGBA, m_size, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &m_data[0]);
    return tex;
}

ShaderProgramBase::ShaderProgramBase()
{
}

ShaderProgramBase::~ShaderProgramBase()
{
    reset(); 
}

static const char* getShaderName(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER: return "Vertex Shader";
    case GL_FRAGMENT_SHADER: return "Fragment Shader";
    default: return "";
    }        
}

GLuint ShaderProgramBase::createShader(const char* txt, GLenum type, LogRecorder *logger) const
{
    GLuint idx = glCreateShader(type);
    glShaderSource(idx, 1, &txt, NULL);
    glCompileShader(idx);

    {
        const uint bufsize = 2048;
        char buf[bufsize];
        GLsizei length = 0;
        glGetShaderInfoLog(idx, bufsize, &length, buf);
        if (length && !ignoreShaderLog(buf))
        {
            ASSERT(length < bufsize);
            if (logger)
                logger->ReportLines(str_format("GL %s Info Log for %s:\n%s\n%s",
                                               getShaderName(type), m_name.c_str(),
                                               str_add_line_numbers(txt).c_str(), buf));
            OLG_OnAssertFailed(m_name.c_str(), -1, "", "glCompileShader",
                               "GL %s Info Log for %s:\n%s\n%s", m_name.c_str(),
                               str_add_line_numbers(txt).c_str(), getShaderName(type), buf);
        }
    }
    
    GLint val = 0;
    glGetShaderiv(idx, GL_COMPILE_STATUS, &val);
    if (val == GL_FALSE) {
        glDeleteShader(idx);
        return 0;
    }
    
    return idx;
}

GLint ShaderProgramBase::getAttribLocation(const char *name) const
{
    if (!isLoaded())
        return -1;
    GLint v = glGetAttribLocation(m_programHandle, name);
    ASSERTF(v >= 0, "%s::%s", m_name.c_str(), name);
    glReportError();
    return v;
}

GLint ShaderProgramBase::getUniformLocation(const char *name) const
{
    if (!isLoaded())
        return -1;
    GLint v = glGetUniformLocation(m_programHandle, name);
    ASSERTF(v >= 0, "%s::%s", m_name.c_str(), name);
    glReportError();
    return v;
}

void ShaderProgramBase::reset()
{
    ASSERT_MAIN_THREAD();
    if (m_programHandle) {
        glDeleteProgram(m_programHandle);
        m_programHandle = 0;
        m_name = "";
    }
}

bool ShaderProgramBase::LoadProgram(const char* name, const char* shared, const char *vertf, const char *fragf, LogRecorder *logger)
{
    ASSERT_MAIN_THREAD();

    m_name = name;
    DPRINT(SHADER, ("Compiling %s(%s)", name, m_argstr.c_str()));
    
    string header =
#if OPENGL_ES
        "precision highp float;\n"
        "precision highp sampler2D;\n"
#else
        "#version 120\n"
#endif
         "#define M_PI 3.1415926535897932384626433832795\n";

    header += m_header + "\n";
    header += shared;
    header += "\n";
    static const char* vertheader =
        "attribute vec4 Position;\n"
        "uniform mat4 Transform;\n"
        "uniform float Time;\n";

    static const char* fragheader = "uniform float Time;\n";
    
    string vertful = header + vertheader + vertf;
    string fragful = header + fragheader + fragf;

    GLuint vert = createShader(vertful.c_str(), GL_VERTEX_SHADER, logger);
    GLuint frag = createShader(fragful.c_str(), GL_FRAGMENT_SHADER, logger);

    if (!vert || !frag)
        return false;

    if (m_programHandle) {
        DPRINT(SHADER, ("Deleting old %s", name));
        glDeleteProgram(m_programHandle);
    }
    
    m_programHandle = glCreateProgram();
    ASSERTF(m_programHandle, "%s", m_name.c_str());
    if (!m_programHandle)
        return false;
    glGetError();
    glAttachShader(m_programHandle, vert);
    glReportError();
    glAttachShader(m_programHandle, frag);
    glReportError();
    
    glLinkProgram(m_programHandle);
    glReportError();

    glDeleteShader(vert);
    glReportError();
    glDeleteShader(frag);
    glReportError();

    checkProgramInfoLog(m_programHandle, name, logger);
    
    GLint linkSuccess = 0;
    glGetProgramiv(m_programHandle, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE) {
        DPRINT(SHADER, ("Compiling %s failed", name));
        glDeleteProgram(m_programHandle);
        m_programHandle = 0;
        return false;
    }
    
    a_position = glGetAttribLocation(m_programHandle, "Position");
    u_transform = glGetUniformLocation(m_programHandle, "Transform");
    u_time = glGetUniformLocation(m_programHandle, "Time");
    
    return true;
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss, uint size, const float3* pos) const
{
    UseProgramBase(ss);
    if (a_position >= 0) {
        glEnableVertexAttribArray(a_position);
        vap1(a_position, size, pos);
        glReportError();
    }
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss, uint size, const float2* pos) const
{
    UseProgramBase(ss);
    if (a_position >= 0) {
        glEnableVertexAttribArray(a_position);
        vap1(a_position, size, pos);
        glReportError();
    }
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss) const
{
    ASSERT_MAIN_THREAD();
    ASSERTF(isLoaded(), "%s", m_name.c_str());
    glReportError();
    glUseProgram(m_programHandle);
    glUniformMatrix4fv(u_transform, 1, GL_FALSE, &ss.uTransform[0][0]);
    if (u_time >= 0)
        glUniform1f(u_time, globals.renderTime);
    glReportError();
}

void ShaderProgramBase::UnuseProgram() const
{
    ASSERT_MAIN_THREAD();
    glReportValidateShaderError(m_programHandle, m_name.c_str());
    if (a_position >= 0) {
        glDisableVertexAttribArray(a_position);
    }
    foreach (GLuint slot, m_enabledAttribs)
        glDisableVertexAttribArray(slot);
    m_enabledAttribs.clear();
    glUseProgram(0);
}

void ShaderState::DrawElements(GLenum dt, size_t ic, const ushort* i) const
{
    ASSERT_MAIN_THREAD();
    glDrawElements(dt, (GLsizei) ic, GL_UNSIGNED_SHORT, i);
    glReportError();
    graphicsDrawCount++;
}

void ShaderState::DrawElements(uint dt, size_t ic, const uint* i) const
{
    ASSERT_MAIN_THREAD();
    glDrawElements(dt, (GLsizei) ic, GL_UNSIGNED_INT, i);
    glReportError();
    graphicsDrawCount++;
}

void ShaderState::DrawArrays(uint dt, size_t count) const
{
    ASSERT_MAIN_THREAD();
    glDrawArrays(dt, 0, (GLsizei)count);
    glReportError();
    graphicsDrawCount++;
}


void DrawAlignedGrid(ShaderState &wss, const View& view, float size, float z)
{
    const double2 roundedCam  = double2(round(view.position, size));
    const double2 roundedSize = double2(round(0.5f * view.getWorldSize(z), size) + float2(size));
    ShaderUColor::instance().DrawGrid(wss, size, 
                                      double3(roundedCam - roundedSize, z), 
                                      double3(roundedCam + roundedSize, z));
} 


void ShaderPosBase::DrawGrid(const ShaderState &ss_, double width, double3 first, double3 last) const
{
    ShaderState ss = ss_;
    ss.translateZ(first.z);

    uint xCount = ceil_int((last.x - first.x) / width);
    uint yCount = ceil_int((last.y - first.y) / width);

    float2 *v = new float2[2 * (xCount + yCount)];
    float2 *pv = v;

    for (uint x=0; x<xCount; x++)
    {
        pv->x = first.x + x * width;
        pv->y = first.y;
        pv++;
        pv->x = first.x + x * width;
        pv->y = last.y;
        pv++;
    }

    for (uint y=0; y<yCount; y++)
    {
        pv->x = first.x;
        pv->y = first.y + y * width;
        pv++;
        pv->x = last.x;
        pv->y = first.y + y * width;
        pv++;
    }

    UseProgram(ss, v);
    ss.DrawArrays(GL_LINES, 2 * (xCount + yCount));
    UnuseProgram();
    delete[] v;
}

DEFINE_CVAR(float, kButtonCorners, 0.1f);

// rectangle missing 2 corners
void PushButton(TriMesh<VertexPosColor>* triP, LineMesh<VertexPosColor>* lineP, float2 pos, float2 r, uint bgColor, uint fgColor, float alpha)
{
    const float2 v[6] = { pos + float2(-r.x, lerp(r.y, -r.y, kButtonCorners)),
                          pos + float2(lerp(-r.x, r.x, kButtonCorners), r.y),
                          pos + float2(r.x, r.y),
                          pos + float2(r.x, lerp(-r.y, r.y, kButtonCorners)),
                          pos + float2(lerp(r.x, -r.x, kButtonCorners), -r.y),
                          pos + float2(-r.x, -r.y) };

    if (triP && (bgColor&ALPHA_OPAQUE) && alpha > epsilon) {
        triP->color32(bgColor, alpha);
        triP->PushPoly(v, 6);
    }

    if (lineP && (fgColor&ALPHA_OPAQUE) && alpha > epsilon) {
        lineP->color32(fgColor, alpha);
        lineP->PushLoop(v, 6);
    }
}

// only missing 1 corner
void PushButton1(TriMesh<VertexPosColor>* triP, LineMesh<VertexPosColor>* lineP, float2 pos, float2 r, uint bgColor, uint fgColor, float alpha)
{
    const float2 v[5] = { pos + float2(-r.x, r.y),
                          pos + float2(r.x, r.y),
                          pos + float2(r.x, lerp(-r.y, r.y, kButtonCorners)),
                          pos + float2(lerp(r.x, -r.x, kButtonCorners), -r.y),
                          pos + float2(-r.x, -r.y) };

    if (triP && (bgColor&ALPHA_OPAQUE) && alpha > epsilon) {
        triP->color32(bgColor, alpha);
        triP->PushPoly(v, 5);
    }

    if (lineP && (fgColor&ALPHA_OPAQUE) && alpha > epsilon) {
        lineP->color32(fgColor, alpha);
        lineP->PushLoop(v, 5);
    }
}

void DrawButton(const ShaderState *data, float2 pos, float2 r, uint bgColor, uint fgColor, float alpha)
{
    if (alpha < epsilon)
        return;
    DMesh::Handle h(theDMesh());
    PushButton(&h.mp.tri, &h.mp.line, pos, r, bgColor, fgColor, alpha);
    h.Draw(*data);
}

void DrawFilledRect(const ShaderState &s_, float2 pos, float2 rad, uint bgColor, uint fgColor, float alpha)
{
    ShaderState ss = s_;
    ss.translateZ(-1.f);
    ss.color32(bgColor, alpha);
    ShaderUColor::instance().DrawRect(ss, pos, rad);
    ss.color32(fgColor,alpha);
    ss.translateZ(0.1f);
    ShaderUColor::instance().DrawLineRect(ss, pos, rad);
}

float2 DrawBar(const ShaderState &s1, uint fill, uint line, float alpha, float2 p, float2 s, float a)
{
    ShaderState ss = s1;
    a = clamp(a, 0.f, 1.f);
    ss.color(fill, alpha);
    const float2 wp = p + float2(1.f, -1.f);
    const float2 ws = s - float2(2.f);
    ShaderUColor::instance().DrawQuad(ss, wp, wp + a * justX(ws), wp + float2(a*ws.x, -ws.y), wp - justY(ws));
    ss.color(line, alpha);
    ShaderUColor::instance().DrawLineQuad(ss, p, p + justX(s), p - justY(s), p + flipY(s));
    return s;
}


void PushRect(TriMesh<VertexPosColor>* triP, LineMesh<VertexPosColor>* lineP, float2 pos, float2 r, 
             uint bgColor, uint fgColor, float alpha)
{
    if (lineP) {
        lineP->color32(fgColor, alpha);
        lineP->PushRect(pos, r);
    }
    if (triP) {
        triP->color32(bgColor, alpha);
        triP->PushRect(pos, r);
    }
}

void fadeFullScreen(const View& view, uint color)
{
    if (view.alpha < epsilon)
        return;
    GLScope s0(GL_DEPTH_WRITEMASK, false);
    GLScope s1(GL_DEPTH_TEST, false);
    ShaderState ss = view.getScreenShaderState();
    ss.color(color, view.alpha);
    ShaderUColor::instance().DrawRectCorners(ss, -0.1f * view.sizePoints, 1.1f * view.sizePoints);
}

void sexyFillScreen(const ShaderState &ss, const View& view, uint color0, uint color1)
{
    if (view.alpha < epsilon || (color0 == 0 && color1 == 0))
        return;

    GLScope s0(GL_DEPTH_WRITEMASK, false);
    GLScope s1(GL_DEPTH_TEST, false);
    const float2 ws = 1.2f * view.sizePoints;
    const float2 ps = -0.1f * view.sizePoints;
    const float t = globals.renderTime / 20.f;
    const uint a = ALPHAF(view.alpha);

    // 0 1
    // 3 2
    const VertexPosColor v[] = {
        VertexPosColor(ps + justY(ws), a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(3.f * t)))),
        VertexPosColor(ps + ws,        a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(5.f * t)))),
        VertexPosColor(ps + justX(ws), a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(7.f * t)))),
        VertexPosColor(ps,  a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(t)))),
    };
    DrawElements(ShaderColorDither::instance(), ss, GL_TRIANGLES, v,
                 kQuadIndices, arraySize(kQuadIndices));
}    

void PushUnlockDial(TriMesh<VertexPosColor> &mesh, float2 pos, float rad, float progress,
                    uint color, float alpha)
{
    mesh.color(color, alpha * smooth_clamp(0.f, 1.f, progress, 0.2f));
    mesh.PushSector(pos, rad * lerp(1.3f, 1.f, progress), progress, M_PIf * progress);
}

static DEFINE_CVAR(float, kSpinnerRate, M_PI/2.f);

void renderLoadingSpinner(LineMesh<VertexPosColor> &mesh, float2 pos, float2 size, float alpha, float progress)
{
    const float ang = kSpinnerRate * globals.renderTime + M_TAOf * progress;
    const float2 rad = float2(min_dim(size) * 0.4f, 0.f);
    mesh.color(0xffffff, 0.5f * alpha);
    mesh.PushTri(pos + rotate(rad, ang),
                 pos + rotate(rad, ang+M_TAOf/3.f),
                 pos + rotate(rad, ang+2.f*M_TAOf/3.f));
}

void renderLoadingSpinner(const ShaderState &ss_, float2 pos, float2 size, float alpha, float progress)
{
	ShaderState ss = ss_;
    const float ang = kSpinnerRate * globals.renderTime + M_TAOf * progress;
    const float2 rad = float2(min_dim(size) * 0.4f, 0.f);
    ss.color(0xffffff, 0.5f * alpha);
    
    GLScope s(GL_DEPTH_TEST, false);
    ShaderUColor::instance().DrawLineTri(ss,
                                         pos + rotate(rad, ang),
                                         pos + rotate(rad, ang+M_TAOf/3.f),
                                         pos + rotate(rad, ang+2.f*M_TAOf/3.f));
}



void PostProc::BindWriteFramebuffer()
{
    getWrite().BindFramebuffer(m_res);
}

void PostProc::UnbindWriteFramebuffer()
{
    getWrite().UnbindFramebuffer();
}

void PostProc::Draw(bool bindFB)
{
    // assume write was just written
    const ShaderBlur *blurShader = m_blur ? &ShaderBlur::instance(m_blur) : NULL;

    if (m_blur)
    {
        GLScope s(GL_BLEND, false);
        
        swapRW(); 
        BindWriteFramebuffer();
        blurShader->setDimension(1);
        getRead().DrawFullscreen(*blurShader);
        UnbindWriteFramebuffer();
        
        swapRW();
        BindWriteFramebuffer();
        blurShader->setDimension(0);
        getRead().DrawFullscreen(*blurShader);
        UnbindWriteFramebuffer();
    }

    if (!bindFB)
    {
        getWrite().DrawFullscreen<ShaderTexture>();
    }
    // nothing to do if bindFB and no blur
}

View::View()
{
}


View operator+(const View& a, const View& b)
{
    View r(a);
    r.position = a.position + b.position;
    r.velocity = a.velocity + b.velocity;
    r.scale    = a.scale + b.scale;
    r.rot      = a.rot + b.rot;
    return r;
}

View operator*(float a, const View& b)
{
    View r(b);
    r.position = a * b.position;
    r.velocity = a * b.velocity;
    r.scale    = a * b.scale;
    r.rot      = a * b.rot;
    return r;
}

float View::getScale() const 
{
    return ((0.5f * sizePoints.y * scale) - z) / (0.5f * sizePoints.y); 
}

float2 View::toWorld(float2 p) const
{
    p -= 0.5f * sizePoints;
    p *= getScale();
    p = ::rotate(p, rot);
    p += position;
    return p;
}

float2 View::toScreen(float2 p) const
{
    p -= position;
    p = ::rotateN(p, rot);
    p /= getScale();
    p += 0.5f * sizePoints;
    return p;
}

bool View::intersectRectangle(const float3 &a, const float2 &r) const
{
    // FIXME take angle into account
    float2 zPlaneSize = (0.5f * scale * sizePoints) - getAspect() * a.z;
    return intersectRectangleRectangle(float2(a.x, a.y), r, 
                                       position, zPlaneSize);
}

float View::getScreenLineWidth(float scl) const
{
    const float pointSize = sizePixels.x / sizePoints.x;
    return clamp(getScreenPointSizeInPixels(), 0.1f, 1.5f * pointSize) * scl;
}

float View::getWorldLineWidth() const
{
    const float pointSize = sizePixels.x / sizePoints.x;
    return clamp(getWorldPointSizeInPixels(), 0.1f, 1.5f * pointSize);
}

void View::setScreenLineWidth(float scl) const
{
    glLineWidth(getScreenLineWidth(scl));
    glReportError();
}

void View::setWorldLineWidth() const
{
    glLineWidth(getWorldLineWidth());
    glReportError();
}

uint View::getCircleVerts(float worldRadius, int mx) const
{
    const uint verts = clamp(uint(round(toScreenSize(worldRadius))), 3, mx);
    return verts;
}

static DEFINE_CVAR(float, kFOV, M_PI_2f);

ShaderState View::getWorldShaderState(float2 zminmax) const
{
    static DEFINE_CVAR(float, kUpAngle, M_PI_2f);
    
    // +y is up in world coordinates
    const float2 s = 0.5f * sizePoints * float(scale);
    ShaderState ws;

    const float aspect = sizePoints.x / sizePoints.y;
    const float dist   = s.y;
    // const float mznear = min(1.f, dist - 100.f);
    const float mznear = clamp(dist + zminmax.x - 10.f, dist / 10.f, dist / 2.f);
    const float mzfar  = dist + ((zminmax.y == 0.f) ? 2000.f : max(zminmax.y, 5.f));
    ASSERT(mznear < mzfar);

    const glm::mat4 view = glm::lookAt(float3(position, dist),
                                       float3(position, 0.f),
                                       float3(::rotate(rot, kUpAngle), 0));
    const glm::mat4 proj = glm::perspective(kFOV, aspect, mznear, mzfar);
    ws.uTransform = proj * view;

    ws.translateZ(z);

    return ws;
}

ShaderState View::getScreenShaderState() const
{
    ShaderState ss;
    static DEFINE_CVAR(float, kScreenFrustumDepth, 100.f);
    static DEFINE_CVAR(float, kMouseScreenSkew, /*-0.005f*/ 0.f);
    static DEFINE_CVAR(float, kScreenZoom, 0.025f);

    const float2 offs = kMouseScreenSkew * (KeyState::instance().cursorPosScreen - 0.5f * globals.windowSizePoints);
    const float2 pos   = 0.5f * sizePoints;
    const float aspect = sizePoints.x / sizePoints.y;
    const float dist   = pos.y + kScreenZoom * pos.y * (1.f - alpha);
    const float mznear  = max(dist - kScreenFrustumDepth, 1.f);

    const glm::mat4 view = glm::lookAt(float3(pos + offs, dist),
                                       float3(pos, 0.f),
                                       float3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(M_PI_2f, aspect, mznear, dist + kScreenFrustumDepth);
    ss.uTransform = proj * view;

    //ss.translate(float3(offset.x, offset.y, toScreenSize(offset.z)));
    //ss.translate(offset);

    return ss;
}


const GLTexture &getDitherTex()
{
    static GLTexture *tex = NULL;

    if (!tex)
    {
        static const char pattern[] = {
            0, 32,  8, 40,  2, 34, 10, 42,   /* 8x8 Bayer ordered dithering  */
            48, 16, 56, 24, 50, 18, 58, 26,   /* pattern.  Each input pixel   */
            12, 44,  4, 36, 14, 46,  6, 38,   /* is scaled to the 0..63 range */
            60, 28, 52, 20, 62, 30, 54, 22,   /* before looking in this table */
            3, 35, 11, 43,  1, 33,  9, 41,   /* to determine the action.     */
            51, 19, 59, 27, 49, 17, 57, 25,
            15, 47,  7, 39, 13, 45,  5, 37,
            63, 31, 55, 23, 61, 29, 53, 21 };

        GLuint name = 0;
        glGenTextures(1, &name);
        glBindTexture(GL_TEXTURE_2D, name);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 8, 8, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pattern);
        glReportError();

        tex = new GLTexture(name, float2(8.f), GL_LUMINANCE);
    }
    
    return *tex;
}
