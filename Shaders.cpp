
#include "StdAfx.h"
#include "Shaders.h"

void ShaderPosBase::DrawLineCircle(const ShaderState& ss, float radius, int numVerts) const
{
    static const int maxVerts = 64;
    numVerts = std::min(maxVerts, numVerts);
    float2 verts[maxVerts];
    
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

void ShaderPosBase::DrawCircle(const ShaderState& ss, float radius, int numVerts) const
{
    static const int maxVerts = 64;
    numVerts = std::min(maxVerts - maxVerts/3, numVerts);
    float2 verts[maxVerts];
    
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

static const ushort kQuadIndices[] = {0, 1, 2, 1, 3, 2};

void ShaderTextureBase::bindTextureDrawElements(const ShaderState &ss, const OutlawTexture &texture,
                                                VertexPosTex *vtx, int icount, const ushort *idxs) const
{
    BindTexture(texture, 0);
    UseProgram(ss, vtx, texture);
    ss.DrawElements(GL_TRIANGLES, icount, idxs);
    UnuseProgram();
}

void ShaderTextureBase::DrawQuad(const ShaderState& ss, const OutlawTexture& texture,
                                 float2 a, float2 b, float2 c, float2 d) const
{
    const float wi = (texture.texwidth ? (float) texture.width / texture.texwidth : 1.f);
    const float hi = (texture.texheight ? (float) texture.height / texture.texheight : 1.f);
    VertexPosTex v[] = { { float3(a, 0), float2(0, hi) },
                         { float3(b, 0), float2(wi, hi) },
                         { float3(c, 0), float2(0, 0) },
                         { float3(d, 0), float2(wi, 0) } };
    bindTextureDrawElements(ss, texture, v, arraySize(kQuadIndices), kQuadIndices);
}


void ShaderTextureBase::DrawRectScaleAngle(const ShaderState &ss, const OutlawTexture& texture,
                            float2 scale, float angle, float2 pos, float2 rad) const
{
    const float wi = scale.x * (texture.texwidth ? (float) texture.width / texture.texwidth : 1.f);
    const float hi = scale.y * (texture.texheight ? (float) texture.height / texture.texheight : 1.f);
    const float2 rot = angleToVector(angle);

    const float2 a = pos + flipX(rad);
    const float2 b = pos + rad;
    const float2 c = pos - rad;
    const float2 d = pos + flipY(rad);

    VertexPosTex v[] = { { float3(a, 0), rotate(float2(0, hi), rot) },
                         { float3(b, 0), rotate(float2(wi, hi), rot) },
                         { float3(c, 0), float2() },
                         { float3(d, 0), rotate(float2(wi, 0), rot) } };
    bindTextureDrawElements(ss, texture, v, arraySize(kQuadIndices), kQuadIndices);
}


void ShaderTextureBase::DrawButton(const ShaderState &ss, const OutlawTexture& texture, float2 pos, float2 r) const
{
    static const float o = 0.1f;
    VertexPosTex v[6] = { 
        { float3(pos + float2(-r.x, lerp(r.y, -r.y, o)), 0.f), float2(0.f, 1.f - o) },
        { float3(pos + float2(lerp(-r.x, r.x, o), r.y),  0.f), float2(o, 1.f)       },
        { float3(pos + float2(r.x, r.y),                 0.f), float2(1.f)          },
        { float3(pos + float2(r.x, lerp(-r.y, r.y, o)),  0.f), float2(1.f, o),      },
        { float3(pos + float2(lerp(r.x, -r.x, o), -r.y), 0.f), float2(1.f - o, 0.f) },
        { float3(pos + float2(-r.x, -r.y),               0.f), float2(0.f)          } };
    static const ushort i[] = { 0,1,2, 0,2,3, 0,3,4, 0,4,5 };
    bindTextureDrawElements(ss, texture, v, arraySize(i), i);
}

void ShaderTonemap::LoadTheProgram()
{
    m_header = "#define DITHER 0\n";
    LoadProgram("ShaderTonemap");
    GET_UNIF_LOC(texture1);
    GET_ATTR_LOC(SourceTexCoord);
}

void ShaderTonemap::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const
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

void ShaderTonemapDither::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(SourceTexCoord, &ptr->tex, (const VertexPosTex*) NULL);
    getDitherTex().BindTexture(1);
    glUniform1i(texture1, 0);
    glUniform1i(dithertex, 1);
}


void ShaderColorLuma::LoadTheProgram()
{
    LoadProgram("ShaderColorLuma",
                //"#extension GL_EXT_gpu_shader4 : require\n"
        //"flat varying vec4 DestinationColor;\n"
        "varying vec4 DestinationColor;\n"
        ,
        "attribute vec4 SourceColor;\n"
        "attribute float Luma;\n"
        "void main(void) {\n"
        "    DestinationColor = Luma * SourceColor;\n"
        "    gl_Position = Transform * Position;\n"
        "}\n"
        ,
        "void main(void) {\n"
        "    gl_FragColor = DestinationColor;\n"
        "}\n"
        );
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


void ShaderBlur::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(asourceTexCoord, &ptr->tex, (const VertexPosTex*) NULL);
    glUniform1i(usourceTex, 0);

    glBindTexture(GL_TEXTURE_2D, ot.texnum);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    offsets.resize(samples);
    const float of = 1.f / float((dimension == 0) ? ot.width : ot.height);
    for (int i=0; i<samples; i++)
    {
        offsets[i] = float2();
        offsets[i][dimension] = of * getBlurOffset(i);
    }

    glUniform1i(usourceTex, 0);
    glUniform2fv(uoffsets, offsets.size(), &offsets[0][0]);
    glReportError();
}
