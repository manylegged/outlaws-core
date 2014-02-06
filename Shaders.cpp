
#include "StdAfx.h"
#include "Shaders.h"


ShaderIridescent::ShaderIridescent()
{
    const bool shader4 =
#if __APPLE__
    	true;
#else
    glewIsSupported("EXT_gpu_shader4");
#endif
    
    OL_ReportMessage(shader4 ? "Shader4 supported" : "Shader4 unsupported");
    LoadProgram(shader4 ? 
                "#extension GL_EXT_gpu_shader4 : require\n"
                "flat varying vec4 DestinationColor;\n" : "varying vec4 DestinationColor;\n"
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

