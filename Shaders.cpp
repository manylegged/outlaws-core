
#include "StdAfx.h"
#include "Shaders.h"

void ShaderPosBase::DrawQuad(const ShaderState& ss, float2 a, float2 b, float2 c, float2 d) const
{
    const float2 v[] = { a, b, c, d };

    UseProgram(ss, &v[0]);
    ss.DrawElements(GL_TRIANGLES, 6, kQuadIndices);
    UnuseProgram();
}

void ShaderPosBase::DrawLineCircle(const ShaderState& ss, float radius, uint numVerts) const
{
    numVerts = std::min(kCircleMaxVerts, numVerts);
    float2 verts[kCircleMaxVerts];
    
    for (int i=0; i != numVerts; ++i)
    {
        const float angle = (float) i * (2.f * M_PIf / (float) numVerts);
        verts[i].x = radius * cos(angle);
        verts[i].y = radius * sin(angle);
    }

    UseProgram(ss, verts);
    ss.DrawArrays(GL_LINE_LOOP, numVerts);
    UnuseProgram();
}

void ShaderPosBase::DrawCircle(const ShaderState& ss, float radius, uint numVerts) const
{
    numVerts = std::min(kCircleMaxVerts - kCircleMaxVerts/3, numVerts);
    float2 verts[kCircleMaxVerts];
    
    verts[0].x = 0;
    verts[0].y = 0;
    
    int j=1;
    for (int i=0; i < (numVerts+1); ++i)
    {
        const float angle = (float) i * (2 * M_PI / (float) numVerts);
        verts[j].x = radius * cos(angle);
        verts[j].y = radius * sin(angle);
        j++;
        if (i % 3 == 0)
        {
            verts[j] = verts[0];
            j++;
        }
    }

    UseProgram(ss, verts);
    ss.DrawArrays(GL_TRIANGLE_STRIP, j);
    UnuseProgram();
}

void ShaderTextureBase::bindTextureDrawElements(const ShaderState &ss, const GLTexture &texture,
                                                VertexPosTex *vtx, int icount, const uint *idxs) const
{    
    texture.BindTexture(0);
    UseProgram(ss, vtx, texture);
    ss.DrawElements(GL_TRIANGLES, icount, idxs);
    UnuseProgram();
}

static void flipTexCoords(const GLTexture& texture, VertexPosTex *vtx, int count)
{
    // OpenGL texture coordinate origin is in the lower left.
    // My image loaders like to use upper left.
    if (texture.isFlipped())
    {
        for (int i=0; i<count; i++)
            vtx[i].tex.y = 1.f - vtx[i].tex.y;
    }
}

void ShaderTextureBase::DrawQuad(const ShaderState& ss, const GLTexture& texture,
                                 float2 a, float2 b, float2 c, float2 d) const
{
    const float2 tc = texture.tcoordmax();
    VertexPosTex v[] = { { float3(a, 0), float2(0, tc.y) },
                         { float3(b, 0), tc },
                         { float3(c, 0), float2(tc.x, 0) },
                         { float3(d, 0), float2() } };
    flipTexCoords(texture, v, arraySize(v));
    bindTextureDrawElements(ss, texture, v, arraySize(kQuadIndices), kQuadIndices);
}

void ShaderTextureBase::DrawQuad(const ShaderState& ss, float2 a, float2 b, float2 c, float2 d) const
{
    VertexPosTex v[] = { { float3(a, 0), float2(0, 1) },
                         { float3(b, 0), f2(1, 1) },
                         { float3(c, 0), float2(1, 0) },
                         { float3(d, 0), float2() } };
    UseProgram(ss, v);
    ss.DrawElements(GL_TRIANGLES, arraySize(kQuadIndices), kQuadIndices);
    UnuseProgram();
}


void ShaderTextureBase::DrawRectScale(const ShaderState &ss, const GLTexture& texture,
                                      float2 scale, float2 pos, float2 rad) const
{
    const float2 tc = scale * texture.tcoordmax();

    const float2 a = pos + flipX(rad);
    const float2 b = pos + rad;
    const float2 c = pos + flipY(rad);
    const float2 d = pos - rad;

    VertexPosTex v[] = { { float3(a, 0), float2(0, tc.y) },
                         { float3(b, 0), tc },
                         { float3(c, 0), float2(tc.x, 0) },
                         { float3(d, 0), float2() } };
    flipTexCoords(texture, v, arraySize(v));
    bindTextureDrawElements(ss, texture, v, arraySize(kQuadIndices), kQuadIndices);
}


void ShaderTextureBase::DrawButton(const ShaderState &ss, const GLTexture& texture, float2 pos, float2 r) const
{
    const float o = kButtonCorners;
    VertexPosTex v[6] = { 
        { float3(pos + float2(-r.x, lerp(r.y, -r.y, o)), 0.f), float2(0.f, 1.f - o) },
        { float3(pos + float2(lerp(-r.x, r.x, o), r.y),  0.f), float2(o, 1.f)       },
        { float3(pos + float2(r.x, r.y),                 0.f), float2(1.f)          },
        { float3(pos + float2(r.x, lerp(-r.y, r.y, o)),  0.f), float2(1.f, o),      },
        { float3(pos + float2(lerp(r.x, -r.x, o), -r.y), 0.f), float2(1.f - o, 0.f) },
        { float3(pos + float2(-r.x, -r.y),               0.f), float2(0.f)          } };
    static const uint i[] = { 0,1,2, 0,2,3, 0,3,4, 0,4,5 };
    flipTexCoords(texture, v, arraySize(v));
    bindTextureDrawElements(ss, texture, v, arraySize(i), i);
}

void ShaderTonemap::LoadTheProgram()
{
    m_header = "#define DITHER 0\n";
    LoadProgram("ShaderTonemap");
    GET_UNIF_LOC(texture1);
    GET_ATTR_LOC(SourceTexCoord);
}

void ShaderTonemap::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const GLTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(SourceTexCoord, &ptr->tex, (const VertexPosTex*) NULL);
    glUniform1i(texture1, 0);
}


void ShaderTonemapDither::LoadTheProgram()
{
    m_header = "#define DITHER 1\n";
    LoadProgram("ShaderTonemap");
    GET_UNIF_LOC(texture1);
    GET_UNIF_LOC(dithertex);
    GET_ATTR_LOC(SourceTexCoord);
}

void ShaderTonemapDither::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const GLTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(SourceTexCoord, &ptr->tex, (const VertexPosTex*) NULL);
    getDitherTex().BindTexture(1);
    glUniform1i(texture1, 0);
    glUniform1i(dithertex, 1);
}


void ShaderColorLuma::LoadTheProgram()
{
    LoadProgram("ShaderColorLuma");
    GET_ATTR_LOC(SourceColor);
    GET_ATTR_LOC(Luma);
}

static DEFINE_CVAR(float, kBlurFactor, 0.5f);

float ShaderBlur::getBlurOffset(int sample) const
{
    // interp uses linear filtering between each pair of pixels
    const float offset = sample - floor(samples / 2.f);
    return (2.f * offset + 0.5f);
}

const ShaderBlur &ShaderBlur::instance(int radius)
{
    static vector<ShaderBlur*> shaders(20, NULL);
    // radius is in points
    const int scale = OL_GetCurrentBackingScaleFactor();
    const int idx = max(1, int(floor(radius * scale / 2.f)));
    ShaderBlur* &sdr = vec_index(shaders, idx);
    if (!sdr || sdr->scale != scale) {
        if (!sdr)
            sdr = new ShaderBlur();
        sdr->LoadShader(idx, scale);
    }
    return *sdr;
}

void ShaderBlur::LoadShader(int smpls, int scl)
{
    samples = smpls;
    scale = scl;
    
    // reference:
    // http://www.realtimerendering.com/blog/quick-gaussian-filtering/
    // http://prideout.net/archive/bloom/

    // stddev indicates how much of the gaussian is cut off
    // higher is more blury, but less smooth blur
    const float stddev = samples * kBlurFactor;

    // avoid loosing light due to clamping - make sure the total equals one
    float total = 0.f;
    for (int i=0; i<samples; i++) {
        total += gaussian(getBlurOffset(i), stddev);
    }

    m_argstr = str_format("%d", samples);
    m_header = str_format("#define BLUR_SIZE %d\n", samples);
    m_header += "#define BLUR(SRC, TC, OFF) (";
    for (int i=0; i<samples; i++) {
        if (i != 0)
            m_header += " + ";
        const float offset = getBlurOffset(i);
        const float weight = gaussian(offset, stddev) / total;
        // DPRINT(SHADER, ("%d. %.1f = %.4f", i, offset, weight));
        m_header += str_format("%f * texture2D(SRC, TC + OFF[%d])", weight, i);
    }
    m_header += ")\n";
    LoadProgram("ShaderBlur");
    usourceTex      = getUniformLocation("source");
    asourceTexCoord = getAttribLocation("SourceTexCoord");
    uoffsets        = getUniformLocation("offsets");
    glReportError();
}


void ShaderBlur::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const GLTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(asourceTexCoord, &ptr->tex, (const VertexPosTex*) NULL);
    glUniform1i(usourceTex, 0);

    ot.BindTexture(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    offsets.resize(samples);
    const float of = 1.f / float((dimension == 0) ? ot.size().x : ot.size().y);
    for (int i=0; i<samples; i++)
    {
        offsets[i] = float2();
        offsets[i][dimension] = of * getBlurOffset(i);
    }

    glUniform1i(usourceTex, 0);
    glUniform2fv(uoffsets, offsets.size(), &offsets[0][0]);
    glReportError();
}
