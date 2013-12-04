
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
    
    ReportMessage(shader4 ? "Shader4 supported" : "Shader4 unsupported");
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

