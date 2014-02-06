
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

vector<GLRenderTexture*> GLRenderTexture::s_bound;

void GLRenderTexture::Generate()
{
    ASSERT_MAIN_THREAD();
    glReportError();
    glGenFramebuffers(1, &m_fbname);
    glReportError();

    glGenTextures(1, &m_texname);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glReportError();
 
    glTexImage2D(GL_TEXTURE_2D, 0, m_format, m_size.x, m_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glReportError();

    // The depth buffer
    glGenRenderbuffers(1, &m_zrbname);
    glBindRenderbuffer(GL_RENDERBUFFER, m_zrbname);
    glReportError();
        
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_size.x, m_size.y);
    glReportError();
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbname);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_zrbname);
    glReportError();

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
    if (m_texname)
        glDeleteTextures(1, &m_texname);
    if (m_zrbname)
        glDeleteRenderbuffers(1, &m_zrbname);
    if (m_fbname || m_texname || m_zrbname) {
        glReportError();
    }
    m_size = int2(0);
    m_texname = 0;
    m_fbname = 0;
    m_zrbname = 0;
}

void GLRenderTexture::BindFramebuffer(float2 size, bool keepz)
{
    ASSERT(!isZero(size));
    if (size != m_size)
        clear();
    m_size = size;
    if (!m_fbname)
        Generate();
    RebindFramebuffer();

    if (keepz)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_bound.size() ? s_bound.back()->m_fbname : 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbname);
        glBlitFramebuffer(0, 0, m_size.x, m_size.y, 0, 0, m_size.x, m_size.y,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    else
    {
        glClear(GL_DEPTH_BUFFER_BIT);
    }
    
    glReportError();
    ASSERT(s_bound.empty() || s_bound.back() != this);
    s_bound.push_back(this);
}

void GLRenderTexture::RebindFramebuffer()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbname);
    glViewport(0,0,m_size.x,m_size.y);
    glReportError();
    glReportFramebufferError();
}

void GLRenderTexture::UnbindFramebuffer(float2 vpsize) const
{
    ASSERT(s_bound.size() && s_bound.back() == this);
    s_bound.pop_back();
    if (s_bound.size())
    {
        s_bound.back()->RebindFramebuffer();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, vpsize.x, vpsize.y);
        glReportError();
    }
}

ShaderState GLRenderTexture::DrawFSBegin() const
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);

    ShaderState ss;
    ss.uTransform = glm::ortho(0.f, 1.f, 0.f, 1.f, -1.f, 1.f);
    return ss;
}

void GLRenderTexture::DrawFSEnd() const
{
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glReportError();
}

void GLTexture::TexImage2D(int2 size, GLenum format, const uint *data)
{
    if (!m_texname)
        glGenTextures(1, &m_texname);
    glBindTexture(GL_TEXTURE_2D, m_texname);
    glTexImage2D(GL_TEXTURE_2D, 0, m_format,
                 size.x, size.y-1, 0, format, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    glReportError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_size = float2(size);
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

static string kNoErrors("No errors.\n");

static void printShaderInfoLog(const char* txt, GLuint id)
{
    const uint bufsize = 2048;
    char buf[bufsize];
    GLsizei length;
    glGetShaderInfoLog(id, bufsize, &length, buf);
    if (length && buf != kNoErrors) {
        string st = str_addLineNumbers(txt);
        ReportMessagef("For Shader:\n%s\n%s", st.c_str(), buf);
        ASSERT(length < bufsize);
        ASSERT(0);
    }
}

static void printProgramInfoLog(GLuint id) 
{
    const uint bufsize = 2048;
    char buf[bufsize];
    GLsizei length;
    glGetProgramInfoLog(id, bufsize, &length, buf);
    if (length && buf != kNoErrors) {
        OL_ReportMessage(buf);
        ASSERT(length < bufsize);
        ASSERT(0);
    }
}


static uint createShader(const char *txt, GLenum type)
{
    uint id = glCreateShader(type);
    glShaderSource(id, 1, &txt, NULL);
    glCompileShader(id);
    printShaderInfoLog(txt, id);

    return id;
}

GLenum glReportError1(const char *file, uint line, const char *function)
{
    ASSERT_MAIN_THREAD();
	GLenum err = glGetError();
	if (GL_NO_ERROR != err)
    {
		ReportMessagef("%s:%d:%s(): %s", file, line, function, (char *) gluErrorString (err));
        ASSERTF(0, "%s", (char *) gluErrorString (err));
    }
    
	return err;
}

#define CASE_STR(X) case X: return #X
static const char* glFrameBufferStatusString(GLenum err)
{
    switch(err) {
        CASE_STR(GL_FRAMEBUFFER_COMPLETE);
        CASE_STR(GL_FRAMEBUFFER_UNDEFINED);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
        CASE_STR(GL_FRAMEBUFFER_UNSUPPORTED);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT);
        CASE_STR(GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_EXT);
    }
    return NULL;
}

GLenum glReportFramebufferError1(const char *file, uint line, const char *function)
{
    ASSERT_MAIN_THREAD();
	GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (GL_FRAMEBUFFER_COMPLETE != err)
    {
		ReportMessagef("%s:%d:%s(): %s", file, line, function, glFrameBufferStatusString(err));
        ASSERTF(0, "%s", glFrameBufferStatusString(err));
    }
    
	return err;
}

void glReportValidateShaderError1(const char *file, uint line, const char *function, GLuint program)
{
    ASSERT_MAIN_THREAD();
    glValidateProgram(program);
    printProgramInfoLog(program);
    glReportError();
}


bool ShaderProgramBase::LoadProgram(const char* shared, const char *vertf, const char *fragf)
{
    ASSERT_MAIN_THREAD();
    string header = string("#version 120\n") + shared;
    static const char* vertheader =
        "attribute vec4 Position;\n"
        "uniform mat4 Transform;\n";
    
    string vertful = header + vertheader + vertf;
    string fragful = header + fragf;

    uint vert = createShader(vertful.c_str(), GL_VERTEX_SHADER);
    uint frag = createShader(fragful.c_str(), GL_FRAGMENT_SHADER);
    
    m_programHandle = glCreateProgram();
    glAttachShader(m_programHandle, vert);
    glAttachShader(m_programHandle, frag);
    
    //glBindAttribLocation(m_programHandle, 0, "Position");
    //glBindAttribLocation(m_programHandle, 1, "SourceColor");
    
    glLinkProgram(m_programHandle);
    printProgramInfoLog(m_programHandle);
    glReportError();
    
    GLint linkSuccess;
    glGetProgramiv(m_programHandle, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE)
        return false;
    
    m_positionSlot = glGetAttribLocation(m_programHandle, "Position");
    
    glReportError();
    
    m_transformUniform = glGetUniformLocation(m_programHandle, "Transform");
    
    m_loaded = true;
    return m_loaded;
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss, uint size, const float3* pos) const
{
    UseProgramBase(ss);
    glEnableVertexAttribArray(m_positionSlot);
    vap1(m_positionSlot, size, pos);
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss, uint size, const float2* pos) const
{
    UseProgramBase(ss);
    glEnableVertexAttribArray(m_positionSlot);
    vap1(m_positionSlot, size, pos);
}

void ShaderProgramBase::UseProgramBase(const ShaderState& ss) const
{
    ASSERT_MAIN_THREAD();
    glUseProgram(m_programHandle);
    glUniformMatrix4fv(m_transformUniform, 1, GL_FALSE, &ss.uTransform[0][0]);
    glReportError();
}

void ShaderProgramBase::UnuseProgram() const
{
    ASSERT_MAIN_THREAD();
    glReportValidateShaderError(m_programHandle);
    glDisableVertexAttribArray(m_positionSlot);
    foreach (GLuint slot, m_enabledAttribs)
        glDisableVertexAttribArray(slot);
    m_enabledAttribs.clear();
    glUseProgram(0);
}

void ShaderState::DrawElements(uint dt, size_t ic, const ushort* i) const
{
    ASSERT_MAIN_THREAD();
    glDrawElements(dt, (GLsizei) ic, GL_UNSIGNED_SHORT, i);
    glReportError();
    globals.drawCount++;
}

void ShaderState::DrawElements(uint dt, size_t ic, const uint* i) const
{
    ASSERT_MAIN_THREAD();
    glDrawElements(dt, (GLsizei) ic, GL_UNSIGNED_INT, i);
    glReportError();
    globals.drawCount++;
}

void ShaderState::DrawArrays(uint dt, size_t count) const
{
    ASSERT_MAIN_THREAD();
    glDrawArrays(dt, 0, (GLsizei)count);
    glReportError();
    globals.drawCount++;
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
    float2 v[6] = { pos + float2(-r.x, lerp(r.y, -r.y, o)),
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
    TriMesh<VertexPosColor> vpt;
    LineMesh<VertexPosColor> vpl;
    PushButton(&vpt, &vpl, pos, r, bgColor, fgColor, alpha);
    vpt.Draw(*data, ShaderColor::instance());
    vpl.Draw(*data, ShaderColor::instance());
}

void DrawBox(ShaderState *ss, float2 pos, float2 size, uint bgColor, uint fgColor, float alpha)
{
    ss->color32(bgColor, alpha);
    ShaderUColor::instance().DrawRect(*ss, pos, size);
    ss->color32(fgColor,alpha);
    ShaderUColor::instance().DrawLineRect(*ss, pos, size);
}

void PushBox(TriMesh<VertexPosColor>* triP, LineMesh<VertexPosColor>* lineP, float2 pos, float2 r, 
             uint bgColor, uint fgColor, float alpha)
{
    if (lineP) {
        lineP->color32(fgColor, alpha);
        lineP->PushRect(pos, r);
    }
    if (triP) {
        triP->color32(fgColor, alpha);
        triP->PushRect(pos, r);
    }
}

void fadeFullScreen(ShaderState &ss, const View& view, uint color, float alpha)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    ss.color(color, alpha);
    ShaderUColor::instance().DrawRectCorners(ss, float2(0), view.sizePoints);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}


void PostProc::BindWriteFramebuffer()
{
    getWrite().BindFramebuffer(m_res);
}

void PostProc::UnbindWriteFramebuffer()
{
    getWrite().UnbindFramebuffer(m_res);
}

void PostProc::Draw(bool bindFB)
{
    // assume write was just written

    if (m_blurLevel > 1)
    {
        swapRW(); 
        BindWriteFramebuffer();
        ShaderBlur::instance().setDimension(1);
        getRead().DrawFullscreen<ShaderBlur>();
        ShaderBlur::instance().setDimension(0);
        UnbindWriteFramebuffer();
    }

    if (bindFB && m_blurLevel > 1)
    {
        swapRW();
        BindWriteFramebuffer();
        getRead().DrawFullscreen<ShaderBlur>();
        UnbindWriteFramebuffer();
    }
    else if (!bindFB)
    {
        if (m_blurLevel > 1) {
            getWrite().DrawFullscreen<ShaderBlur>();
        } else {
            getWrite().DrawFullscreen<ShaderTexture>();
        }
    }
    // nothing to do if bindFB and no blur
}

