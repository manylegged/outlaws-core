
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

void ShaderTextureBase::DrawQuad(const ShaderState& ss, const OutlawTexture& texture,
                                 float2 a, float2 b, float2 c, float2 d) const
{
    const float wi = texture.texwidth ? (float) texture.width / texture.texwidth : 1.f;
    const float hi = texture.texheight ? (float) texture.height / texture.texheight : 1.f;
    VertexPosTex v[] = { { float3(a, 0), float2(0, hi) },
                         { float3(b, 0), float2(wi, hi) },
                         { float3(c, 0), float2(0, 0) },
                         { float3(d, 0), float2(wi, 0) } };
    static const ushort i[] = {0, 1, 2, 1, 3, 2};

    BindTexture(texture, 0);
    UseProgram(ss, v, texture);
    ss.DrawElements(GL_TRIANGLES, 6, i);
    UnuseProgram();
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
    BindTexture(texture, 0);
    UseProgram(ss, v, texture);
    ShaderState s1 = ss;
    s1.DrawElements(GL_TRIANGLES, arraySize(i), i);
    UnuseProgram();
}

ShaderIridescent::ShaderIridescent()
{
    const bool shader4 =
#if __APPLE__
#  if OPENGL_ES
        false;
#  else
    true;
#  endif
#else
    glewIsSupported("GL_EXT_gpu_shader4");
#endif
    
    OL_ReportMessage(shader4 ? "Shader4 supported" : "Shader4 unsupported");
    LoadProgram(shader4 ? 
               "#extension GL_EXT_gpu_shader4 : require\n"
               "flat varying vec4 DestinationColor;\n" : 
                "varying vec4 DestinationColor;\n"
                ,
                "attribute vec4 SourceColor0;\n"
                "attribute vec4 SourceColor1;\n"
                "attribute float TimeA;\n"
                "uniform float TimeU;\n"
                "void main(void) {\n"
                "    gl_Position = Transform * Position;\n"
                "    float val = 0.5 + 0.5 * sin(TimeU + TimeA);\n"
                "    DestinationColor = mix(SourceColor0, SourceColor1, val);\n"
                "}\n"
                ,
                "void main(void) {\n"
                "    gl_FragColor = DestinationColor;\n"
                "}\n"
        );
    GET_ATTR_LOC(SourceColor0);
    GET_ATTR_LOC(SourceColor1);
    GET_ATTR_LOC(TimeA);
    GET_UNIF_LOC(TimeU);
}

ShaderTonemap::ShaderTonemap()
{
    LoadProgram("varying vec2 DestTexCoord;\n"
                ,
                "attribute vec2 SourceTexCoord;\n"
                "void main(void) {\n"
                "    DestTexCoord = SourceTexCoord;\n"
                "    gl_Position  = Transform * Position;\n"
                "}\n"
                ,
                "uniform sampler2D texture1;\n"
                "uniform sampler2D dithertex;\n"
                "void main(void) {\n"
                "    vec4 color = texture2D(texture1, DestTexCoord);\n"
                "    float mx = max(color.r, max(color.g, color.b));\n"
                "    if (mx > 1.0) {\n"
                "        color.rgb += 1.0 * vec3(mx - 1.0);\n"
                "    }\n"
                //"    color.rgb = max(color.rgb, vec3(0.01, 0.01, 0.01));"
                //"    color.rgb *= 1.5;\n"
                //"    color.rgb = color.rgb / (1 + max(color.r, max(color.g, color.b)));\n"
                //"    color.rgb = color.rgb / (1 + color.rgb);\n"
                //"    color.rgb = pow(color.rgb,vec3(1/2.2));\n"
                "    gl_FragColor = color;\n"
                "}\n"
        );
    GET_UNIF_LOC(texture1);
    GET_ATTR_LOC(SourceTexCoord);
}

void ShaderTonemap::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(SourceTexCoord, &ptr->texCoord, (const VertexPosTex*) NULL);
    glUniform1i(texture1, 0);
}


ShaderTonemapDither::ShaderTonemapDither()
{
    LoadProgram("varying vec2 DestTexCoord;\n"
                ,
                "attribute vec2 SourceTexCoord;\n"
                "void main(void) {\n"
                "    DestTexCoord = SourceTexCoord;\n"
                "    gl_Position  = Transform * Position;\n"
                "}\n"
                ,
                "uniform sampler2D texture1;\n"
                "uniform sampler2D dithertex;\n"
                "void main(void) {\n"
                "    vec4 color = texture2D(texture1, DestTexCoord);\n"
                "    float mx = max(color.r, max(color.g, color.b));\n"
                "    if (mx > 1.0) {\n"
                "        color.rgb += 1.0 * vec3(mx - 1.0);\n"
                "    }\n"
                "    float ditherv = texture2D(dithertex, gl_FragCoord.xy / 8.0).r / 64.0;\n"
                "    color += vec4(ditherv);\n"
                //"    color.rgb = max(color.rgb, vec3(0.01, 0.01, 0.01));"
                //"    color.rgb *= 1.5;\n"
                //"    color.rgb = color.rgb / (1 + max(color.r, max(color.g, color.b)));\n"
                //"    color.rgb = color.rgb / (1 + color.rgb);\n"
                //"    color.rgb = pow(color.rgb,vec3(1/2.2));\n"
                "    gl_FragColor = color;\n"
                "}\n"
        );
    GET_UNIF_LOC(texture1);
    GET_UNIF_LOC(dithertex);
    GET_ATTR_LOC(SourceTexCoord);
}

void ShaderTonemapDither::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(SourceTexCoord, &ptr->texCoord, (const VertexPosTex*) NULL);
    getDitherTex().BindTexture(1);
    glUniform1i(texture1, 0);
    glUniform1i(dithertex, 1);
}


ShaderColorLuma::ShaderColorLuma()
{
    LoadProgram(//"#extension GL_EXT_gpu_shader4 : require\n"
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


ShaderBlur::ShaderBlur()
{
    // TODO calculate offset in vertex shader
    LoadProgram("varying vec2 DestTexCoord;\n"
                ,
                "attribute vec2 SourceTexCoord;\n"
                "void main(void) {\n"
                "    DestTexCoord = SourceTexCoord;\n"
                "    gl_Position  = Transform * Position;\n"
                "}\n"
                ,
                "uniform sampler2D source;\n"
                "uniform float coefficients[5];\n"
                "uniform vec2 offsets[5];\n"
                "void main() {\n"
                "    float d = 0.1;\n"
                "    vec4 c = vec4(0, 0, 0, 0);\n"
                "    vec2 tc = DestTexCoord;\n"
                "    c += coefficients[0] * texture2D(source, tc + offsets[0]);\n"
                "    c += coefficients[1] * texture2D(source, tc + offsets[1]);\n"
                "    c += coefficients[2] * texture2D(source, tc + offsets[2]);\n"
                "    c += coefficients[3] * texture2D(source, tc + offsets[3]);\n"
                "    c += coefficients[4] * texture2D(source, tc + offsets[4]);\n"
                "    gl_FragColor = c;\n"
                "}\n"
        );
    usourceTex      = getUniformLocation("source");
    asourceTexCoord = getAttribLocation("SourceTexCoord");
    ucoefficients   = getUniformLocation("coefficients");
    uoffsets        = getUniformLocation("offsets");
    glReportError();


    coefficients[0] = 0.0539909665;
    coefficients[1] = 0.2419707245;
    coefficients[2] = 0.3989422804;
    coefficients[3] = 0.2419707245;
    coefficients[4] = 0.0539909665;
}


void ShaderBlur::UseProgram(const ShaderState &ss, const VertexPosTex *ptr, const OutlawTexture &ot) const
{
    UseProgramBase(ss, &ptr->pos, (const VertexPosTex*) NULL);

    vertexAttribPointer(asourceTexCoord, &ptr->texCoord, (const VertexPosTex*) NULL);
    glUniform1i(usourceTex, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        
    memset(offsets, 0, sizeof(offsets));
    const float of = 1.5f / float((dimension == 0) ? ot.width : ot.height);
    for (int i=0; i<5; i++)
        offsets[i][dimension] = of * (2 - i);

    glUniform1i(usourceTex, 0);
    glUniform2fv(uoffsets, 5, &offsets[0][0]);
    glUniform1fv(ucoefficients, 5, &coefficients[0]);
    glReportError();
}
