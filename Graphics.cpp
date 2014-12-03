
//
// Graphics.cpp - drawing routines
// 
// Created on 10/31/12.
// 

// Copyright (c) 2013 Arthur Danskin
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

#ifndef ASSERT_MAIN_THREAD
#define ASSERT_MAIN_THREAD()
#endif

bool supports_ARB_Framebuffer_object = false;

static const uint kDebugFrames = 10;

bool isGLExtensionSupported(const char *name)
{
    static const GLubyte* val = glGetString(GL_EXTENSIONS);
    static const string str((const char*)val);
    return str.find(name) != std::string::npos;
}

uint graphicsDrawCount = 0;
uint gpuMemoryUsed = 0;

vector<GLRenderTexture*> GLRenderTexture::s_bound;

#define CASE_STR(X) case X: return #X
static const char* textureFormatToString(GLint fmt)
{
    switch (fmt) {
        CASE_STR(GL_RGB);
        CASE_STR(GL_RGBA);
        CASE_STR(GL_BGRA);
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
    case GL_RGB16F_ARB:
    case GL_RGBA16F_ARB: return 2 * 4;
    default:
        return 4;
    }
}

static GLint s_defaultFramebuffer = -1;

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
    
    if (s_defaultFramebuffer < 0)
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
    if (m_format == GL_RGBA16F_ARB)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_HALF_FLOAT_OES, 0);
    }
    else
#endif
    {
        glTexImage2D(GL_TEXTURE_2D, 0, m_format, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    }
    glReportError();
    gpuMemoryUsed += width * height * textureFormatBytesPerPixel(m_format);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbname);

    // The depth buffer
    if (zflags&HASZ)
    {
        glGenRenderbuffers(1, &m_zrbname);
        glBindRenderbuffer(GL_RENDERBUFFER, m_zrbname);
        glReportError();
        
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glReportError();
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_zrbname);
        glReportError();

        gpuMemoryUsed += width * height * 2;
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

    // Always check that our framebuffer is ok
    glReportError();
    glReportFramebufferError();
}

void GLRenderTexture::clear()
{
    if (m_fbname)
        glDeleteFramebuffers(1, &m_fbname);
    if (m_texname) {
        gpuMemoryUsed -= m_size.x * m_size.y * textureFormatBytesPerPixel(m_format);
        glDeleteTextures(1, &m_texname);
    }
    if (m_zrbname) {
        gpuMemoryUsed -= m_size.x * m_size.y * 2;
        glDeleteRenderbuffers(1, &m_zrbname);
    }
    if (m_fbname || m_texname || m_zrbname) {
        glReportError();
    }
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
    ASSERT(!isZero(size));
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

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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
            glViewport(0, 0, globals.windowSizePixels.x, globals.windowSizePixels.y);
            glReportError();
        }
    }
}

void GLTexture::clear()
{
    if (m_texname)
    {
        gpuMemoryUsed -= m_texsize.x * m_texsize.y * textureFormatBytesPerPixel(m_format);
        glDeleteTextures(1, &m_texname);
    }
    m_texname = 0;
}

void GLTexture::DrawFSBegin(ShaderState& ss) const
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
#if !OPENGL_ES
    glDisable(GL_ALPHA_TEST);
#endif

    ss.uTransform = glm::ortho(0.f, 1.f, 0.f, 1.f, -1.f, 1.f);
}

void GLTexture::DrawFSEnd() const
{
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
#if !OPENGL_ES
    glEnable(GL_ALPHA_TEST);
#endif
    glReportError();
}

void GLTexture::TexImage2D(int2 size, GLenum format, const uint *data)
{
    if (!m_texname) {
        glGenTextures(1, &m_texname);
    } else {
        gpuMemoryUsed -= m_texsize.x * m_texsize.y * textureFormatBytesPerPixel(m_format);
    }
    glBindTexture(GL_TEXTURE_2D, m_texname);

#if OPENGL_ES
    m_texsize = float2(roundUpPower2(size.x), roundUpPower2(size.y));
    glTexImage2D(GL_TEXTURE_2D, 0, format,
                 m_texsize.x, m_texsize.y-1, 0, format, GL_UNSIGNED_BYTE, data);
    glReportError();
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
    m_texsize = float2(size);
    glTexImage2D(GL_TEXTURE_2D, 0, m_format,
                 size.x, size.y-1, 0, format, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    glReportError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_SGIS);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_size = float2(size);
    gpuMemoryUsed += m_texsize.x * m_texsize.y * textureFormatBytesPerPixel(m_format);
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

void GLTexture::loadFile(const char* fname)
{
    clear();

    OutlawTexture tex = OL_LoadTexture(fname);
    ASSERT(tex.texnum);
    
    m_texname = tex.texnum;
    m_size.x  = tex.width;
    m_size.y  = tex.height;
    m_format  = GL_RGB;
}

GLTexture PixImage::uploadTexture() const
{
    GLTexture tex;
    tex.setFormat(GL_RGBA);
    tex.TexImage2D(m_size, GL_BGRA, &m_data[0]);
    return tex;
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

static void checkProgramInfoLog(GLuint prog, const char* name)
{
    const uint bufsize = 2048;
    char buf[bufsize];
    GLsizei length = 0;
    glGetProgramInfoLog(prog, bufsize, &length, buf);
    if (length && !ignoreShaderLog(buf))
    {
        ASSERT(length < bufsize);
        OLG_OnAssertFailed(name, -1, "", "", "GL Program Info log for '%s': %s", name, buf);
    }
}


ShaderProgramBase::ShaderProgramBase()
{
}

GLuint ShaderProgramBase::createShader(const char* txt, GLenum type) const
{
    GLuint idx = glCreateShader(type);
    glShaderSource(idx, 1, &txt, NULL);
    glCompileShader(idx);

    {
        const uint bufsize = 2048;
        char buf[bufsize];
        GLsizei length = 0;
        glGetShaderInfoLog(idx, bufsize, &length, buf);
        if (length && !ignoreShaderLog(buf)) {
            ASSERT(length < bufsize);
            OLG_OnAssertFailed(m_name.c_str(), -1, "", "glCompileShader", "GL Shader Info Log:\n%s\n%s",
                               str_add_line_numbers(txt).c_str(), buf);
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

GLenum glReportError1(const char *file, uint line, const char *function)
{
    ASSERT_MAIN_THREAD();

    if (!(globals.debugRender&DBG_GLERROR) && globals.frameStep > kDebugFrames)
        return GL_NO_ERROR;

	GLenum err = GL_NO_ERROR;
	while (GL_NO_ERROR != (err = glGetError()))
    {
        const char* msg = (const char *)gluErrorString(err);
        OLG_OnAssertFailed(file, line, function, "glGetError", "%s", msg);
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

GLenum glReportFramebufferError1(const char *file, uint line, const char *function)
{
    ASSERT_MAIN_THREAD();

    if (!(globals.debugRender&DBG_GLERROR) && globals.frameStep > kDebugFrames)
        return GL_NO_ERROR;

	GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (GL_FRAMEBUFFER_COMPLETE != err)
    {
        OLG_OnAssertFailed(file, line, function, "glCheckFramebufferStatus", "%s", getGLFrameBufferStatusString(err).c_str());
    }
    
	return err;
}

void glReportValidateShaderError1(const char *file, uint line, const char *function, GLuint program)
{
    ASSERT_MAIN_THREAD();

    if (!(globals.debugRender&DBG_GLERROR) && globals.frameStep > kDebugFrames)
        return;

    glValidateProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
    checkProgramInfoLog(program, "validate");
    glReportError();

    ASSERT(status == GL_TRUE);
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

bool ShaderProgramBase::LoadProgram(const char* name, const char* shared, const char *vertf, const char *fragf)
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
        "uniform mat4 Transform;\n";
    
    string vertful = header + vertheader + vertf;
    string fragful = header + fragf;

    GLuint vert = createShader(vertful.c_str(), GL_VERTEX_SHADER);
    GLuint frag = createShader(fragful.c_str(), GL_FRAGMENT_SHADER);

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

    checkProgramInfoLog(m_programHandle, name);
    
    GLint linkSuccess = 0;
    glGetProgramiv(m_programHandle, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE) {
        DPRINT(SHADER, ("Compiling %s failed", name));
        glDeleteProgram(m_programHandle);
        m_programHandle = 0;
        return false;
    }
    
    m_positionSlot = glGetAttribLocation(m_programHandle, "Position");
    glReportError();
    
    m_transformUniform = glGetUniformLocation(m_programHandle, "Transform");
    glReportError();
    return true;
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss, uint size, const float3* pos) const
{
    UseProgramBase(ss);
    if (m_positionSlot >= 0) {
        glEnableVertexAttribArray(m_positionSlot);
        vap1(m_positionSlot, size, pos);
        glReportError();
    }
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss, uint size, const float2* pos) const
{
    UseProgramBase(ss);
    if (m_positionSlot >= 0) {
        glEnableVertexAttribArray(m_positionSlot);
        vap1(m_positionSlot, size, pos);
        glReportError();
    }
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss) const
{
    ASSERT_MAIN_THREAD();
    ASSERT(isLoaded());
    glReportError();
    glUseProgram(m_programHandle);
    glUniformMatrix4fv(m_transformUniform, 1, GL_FALSE, &ss.uTransform[0][0]);
    glReportError();
}

void ShaderProgramBase::UnuseProgram() const
{
    ASSERT_MAIN_THREAD();
    glReportValidateShaderError(m_programHandle);
    if (m_positionSlot >= 0) {
        glDisableVertexAttribArray(m_positionSlot);
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

    uint xCount = ceil((last.x - first.x) / width);
    uint yCount = ceil((last.y - first.y) / width);

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

void PushButton(TriMesh<VertexPosColor>* triP, LineMesh<VertexPosColor>* lineP, float2 pos, float2 r, uint bgColor, uint fgColor, float alpha)
{
    static const float o = 0.1f;
    const float2 v[6] = { pos + float2(-r.x, lerp(r.y, -r.y, o)),
                          pos + float2(lerp(-r.x, r.x, o), r.y),
                          pos + float2(r.x, r.y),
                          pos + float2(r.x, lerp(-r.y, r.y, o)),
                          pos + float2(lerp(r.x, -r.x, o), -r.y),
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
    ss.color32(bgColor, alpha);
    ShaderUColor::instance().DrawRect(ss, pos, rad);
    ss.color32(fgColor,alpha);
    ShaderUColor::instance().DrawLineRect(ss, pos, rad);
}

float2 DrawBar(const ShaderState &s1, uint fill, uint line, float alpha, float2 p, float2 s, float a)
{
    ShaderState ss = s1;
    a = clamp(a, 0.f, 1.f);
    ss.color(fill, alpha);
    const float2 wp = p + float2(1.f, -1.f);
    const float2 ws = s - float2(2.f);
    ShaderUColor::instance().DrawQuad(ss, wp, wp + a * justX(ws), wp - justY(ws), wp + float2(a*ws.x, -ws.y));
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

void fadeFullScreen(const ShaderState &s_, const View& view, uint color, float alpha)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    ShaderState ss = s_;
    ss.color(color, alpha);
    ShaderUColor::instance().DrawRectCorners(ss, float2(0), view.sizePoints);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void sexyFillScreen(const ShaderState &ss, const View& view, uint color0, uint color1, float alpha)
{
    if (alpha < epsilon || (color0 == 0 && color1 == 0))
        return;
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    const float2 ws = view.sizePoints;
    const float t = globals.renderTime / 20.f;
    const uint a = ALPHAF(alpha);

    // 1 2
    // 0 3
    const VertexPosColor v[] = {
        VertexPosColor(float2(),  a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(t)))),
        VertexPosColor(justY(ws), a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(3.f * t)))),
        VertexPosColor(ws,        a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(5.f * t)))),
        VertexPosColor(justX(ws), a|rgb2bgr(lerpXXX(color0, color1, unorm_sin(7.f * t)))),
    };
    static const ushort i[] = {0, 1, 2, 0, 2, 3};

    ShaderColorDither::instance().UseProgram(ss, &v[0], (VertexPosColor*) NULL);
    ss.DrawElements(GL_TRIANGLES, 6, i);
    ShaderColorDither::instance().UnuseProgram();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
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
        glDisable(GL_BLEND);
        
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

        glEnable(GL_BLEND);
    }

    if (!bindFB)
    {
        getWrite().DrawFullscreen<ShaderTexture>();
    }
    // nothing to do if bindFB and no blur
}

ShaderState View::getWorldShaderState() const
{
    // +y is up in world coordinates
    const float2 s = 0.5f * sizePoints * float(scale);
    ShaderState ws;

#if 0
    ws.uTransform = glm::ortho(-s.x, s.x, -s.y, s.y, -10.f, 10.f);
    ws.rotate(-angle);
    ws.translate(-position);
#else
    const float fovy   = M_PI_2f;
    const float aspect = sizePoints.x / sizePoints.y;
    const float dist   = s.y;
    const float mznear = max(1.f, dist - 10.f);
    const float mzfar  = dist + clamp(zfar, 5.f, 10000.f);

    const glm::mat4 view = glm::lookAt(float3(position, dist),
                                       float3(position, 0.f),
                                       float3(angleToVector(angle + 0.5f * M_PIf), 0));
    const glm::mat4 proj = glm::perspective(fovy, aspect, mznear, mzfar);
    ws.uTransform = proj * view;
#endif

    ws.translateZ(z);
    return ws;
}

ShaderState View::getScreenShaderState() const
{
    ShaderState ss;
    static DEFINE_CVAR(float, kScreenFrustumDepth, 100.f);

#if 0
    cpBB screenBB = getScreenPointBB();
    ss.uTransform = glm::ortho((float)screenBB.l, (float)screenBB.r, (float)screenBB.b, (float)screenBB.t, -10.f, 10.f);
#else
    const float2 pos   = 0.5f * sizePoints;
    const float fovy   = M_PI_2f;
    const float aspect = sizePoints.x / sizePoints.y;
    const float dist   = pos.y;
    const float mznear  = max(dist - kScreenFrustumDepth, 1.f);

    const glm::mat4 view = glm::lookAt(float3(pos, dist),
                                       float3(pos, 0.f),
                                       float3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(fovy, aspect, mznear, dist + kScreenFrustumDepth);
    ss.uTransform = proj * view;
#endif

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

        // OutlawTexture t = tex->getTexture();
        // OL_SaveTexture(&t, "~/Desktop/dither.png");
    }
    
    return *tex;
}
