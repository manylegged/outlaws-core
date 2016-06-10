-- glsl shaders
-- #version 120

-- format is { COMMON_HEADER, VERTEX_SHADER, FRAGMENT_SHADER }

-- implicit parameters:
-- attribute vec4 Position;
-- uniform mat4 Transform;

{
   -- spaceship hulls
   ShaderIridescent = {
      "varying vec4 DestinationColor;"
      ,
      "attribute vec4 SourceColor0;
      attribute vec4 SourceColor1;
      attribute float TimeA;
      void main(void) {
          gl_Position = Transform * Position;
          float val = 0.5 + 0.5 * sin(0.5 * (Time + TimeA));
          DestinationColor = mix(SourceColor0, SourceColor1, val);
      }"
      ,
      "void main(void) {
            gl_FragColor = DestinationColor;
      }"
   },

   -- draws projectiles, shields, lasers, etc
   ShaderColorLuma = {
      "varying vec4 DestinationColor;"
        ,
        "attribute vec4 SourceColor;
        attribute float Luma;
        void main(void) {
            DestinationColor = Luma * SourceColor;
            gl_Position = Transform * Position;
        }"
        ,
        "void main(void) {
            gl_FragColor = DestinationColor;
        }" 
   },

   -- draws stars
   ShaderStars = {
      "varying vec4 DestinationColor;"
      ,
      "attribute vec4 SourceColor;
       attribute float Size;
       uniform float ToPixels;
       uniform float EyeZ;
       void main(void) {
           DestinationColor = SourceColor;
           gl_PointSize = ToPixels * Size / (EyeZ - Position.z);
           gl_Position = Transform * Position;
       }"
      ,
      "void main(void) {
           vec2 coord = 2.0 * (gl_PointCoord - 0.5);
           //float val = abs(coord.x) + abs(coord.y);
           float val = (length(coord) + abs(coord.x) + abs(coord.y)) / 2.0;
           float alpha = min(1.0 - val, 1.0);
           if (alpha <= 0.0)
               discard;
           gl_FragColor = DestinationColor * vec4(1.0, 1.0, 1.0, alpha);
       }"
   },

   -- draws the wormhole
   ShaderWormhole = {
      "varying vec4 DestColor0;
       varying vec4 DestColor1;
       varying vec2 DestTex;
       float length2(vec2 x) { return dot(x, x); }"

      ,
      "attribute vec2 TexCoord;
      
      void main(void) {
          DestColor0 = vec4(0.2, 0.3, 0.8, 0);
          DestColor1 = mix(vec4(0, 0.9, 0.7, 1.1), vec4(DestColor0.xyz, 1.0), length2(TexCoord));
          DestTex = TexCoord;
          gl_Position = Transform * Position;
      }"
      ,
      "
      #include 'noise3D.glsl'

      vec2 rotate(vec2 v, float a) {
          vec2 r = vec2(cos(a), sin(a));
          return vec2(r.x * v.x - r.y * v.y, r.y * v.x + r.x * v.y);
      }

      void main(void) {
          float r = length2(DestTex);
          float val = snoise(vec3(rotate(DestTex, Time + 5 * r), Time/3 + 2*r) * 2);
          float aval = snoise(vec3(rotate(DestTex, Time + 3 * r), Time/10) * 1);
          float alpha = 1. + 1. * aval;           
          alpha *= max(0, 1 - r) * 3 * r;
          vec4 color = mix(DestColor0, DestColor1, 0.8 + 0.5 * val);
          gl_FragColor = vec4(alpha * color.a * color.xyz, 0.0);
      }"
   }

   -- draws resource packets (R)
   ShaderResource = {
      "varying vec4 DestColor0;
       varying vec4 DestColor1;
       varying vec2 DestPos;
       varying float DestRad;"
      ,
      "attribute vec4 SourceColor0;
      attribute vec4 SourceColor1;
      attribute float Radius;
      uniform float ToPixels;
      void main(void) {
          DestColor0 = SourceColor0;
          DestColor1 = SourceColor1;
          DestPos = Position.xy / 100;
          DestRad = sqrt(Radius);
          gl_Position = Transform * Position;
          gl_PointSize = 2 * ToPixels * Radius;
      }"
      ,
      "
      #include 'noise2D.glsl'

      float length2(vec2 x) { return dot(x, x); }
      vec2 rotate(vec2 v, float a) {
          vec2 r = vec2(cos(a), sin(a));
          return vec2(r.x * v.x - r.y * v.y, r.y * v.x + r.x * v.y);
      }

      void main(void) {
            vec2 coord = gl_PointCoord.xy - 0.5;
            float len2c = length2(coord);
            float post = DestPos.x + DestPos.y + Time;
            float thresh = 1.0 - (4.0 * len2c);
            if (thresh <= 0)
                discard;
            vec2 pos = 0.1 * (DestPos + vec2(0, -Time/2) + DestRad * rotate(coord, len2c * 7 + (DestRad * DestRad / 10) + mod(Time/5, 2 * M_PI)));
            float val = 0.5 * snoise(pos * 1.5) + 0.25 * snoise(pos * 3);
            gl_FragColor.a = (1.0 + 0.5 * sin(5 * val)) * thresh;
            gl_FragColor.xyz = gl_FragColor.a * mix(DestColor0.xyz, DestColor1.xyz, 0.5 + 0.5 * sin(10 * val));
      }"
   },

   -- used for nice gradient backgrounds
   ShaderColorDither = {
      "varying vec4 DestinationColor;"
      ,
      "attribute vec4 SourceColor;
      void main(void) {
          DestinationColor = SourceColor;
          gl_Position = Transform * Position;
      }"
      ,
      "uniform sampler2D dithertex;
      void main(void) {
            float ditherv = texture2D(dithertex, gl_FragCoord.xy / 8.0).r / 64.0 - (1.0 / 128.0);
            gl_FragColor = DestinationColor + vec4(ditherv);
      }"
   },

   -- passthrough texture shader - used in many places for drawing render targets
   ShaderTexture = {
      "varying vec2 DestTexCoord;
       varying vec4 DestColor;\n"
      ,
      "attribute vec2 SourceTexCoord;
      uniform vec4 SourceColor;
      void main(void) {
          DestTexCoord = SourceTexCoord;
          DestColor    = SourceColor;
          gl_Position  = Transform * Position;
      }"
      ,
      "uniform sampler2D texture1;
       void main(void) {
           vec2 texCoord = DestTexCoord;
           gl_FragColor = DestColor * texture2D(texture1, texCoord);
       }"
   }

   ShaderTextureWarp = {
      "varying vec2 DestTexCoord;
       varying vec4 DestColor;"
      ,
      "attribute vec2 SourceTexCoord;
      uniform vec4 SourceColor;
      void main(void) {
          DestTexCoord = SourceTexCoord;
          DestColor    = SourceColor;
          gl_Position  = Transform * Position;
      }"
      ,
      "uniform sampler2D texture1;
       uniform sampler2D warpTex;
       uniform vec2      camWorldPos;
       uniform vec2      camWorldSize;
      #include 'noise2D.glsl'
      void main(void) {
         vec2 texCoord = DestTexCoord;
          //vec2 roll = (texCoord - vec2(0.5));
          //texCoord += 3.0 * vec2(roll.y, -roll.x) * max(0.0, (0.5 - length(roll)));
          float warpv = length(texture2D(warpTex, texCoord).rgb);
          if (warpv > 0.0)
              texCoord += 100.0 * warpv * snoise(camWorldPos + 0.1 * Time + 0.01 * texCoord * camWorldSize) /
                          max(camWorldSize.x, camWorldSize.y);
          gl_FragColor = DestColor * texture2D(texture1, texCoord);
      }"
   }

   -- HDR postprocessing. makes really bright areas (i.e. with a lot of particles) more white to make them look brighter
   ShaderTonemap = {
      "varying vec2 DestTexCoord;"
      ,
      "attribute vec2 SourceTexCoord;
      void main(void) {
          DestTexCoord = SourceTexCoord;
          gl_Position  = Transform * Position;
      }"
      ,
      "uniform sampler2D texture1;
      uniform sampler2D dithertex;
      void main(void) {
          vec2 texCoord = DestTexCoord;
          vec4 color = texture2D(texture1, texCoord);
          if (color.rgb != vec3(0.0))
          {
              float mx = max(color.r, max(color.g, color.b));
              if (mx > 1.0) {
                  //color.rgb += 1.0 * vec3(mx - 1.0);
                  color.rgb += 1.0 * vec3(log(mx));
              }
      #if DITHER
              float ditherv = texture2D(dithertex, gl_FragCoord.xy / 8.0).r / 128.0 - (1.0 / 128.0);
              color += vec4(ditherv);
      #endif
          }
          gl_FragColor = color;
      }"
   },

   ShaderHsv = {
      "varying vec4 DestHSVA;"
      ,
      "attribute vec4 ColorHSVA;
       void main(void) {
           DestHSVA = ColorHSVA;
           gl_Position = Transform * Position;
       }"
      ,

      "
       #include 'hsv_rgb.glsl'
       void main(void) {
           gl_FragColor = vec4(hsv2rgb(DestHSVA.rgb), DestHSVA.a);
      }"
   },


   ShaderTextureHSV = {
      "varying vec2 DestTexCoord;
       varying vec4 DestColor;"
      ,
      "attribute vec2 SourceTexCoord;
       uniform vec4 SourceColor;
       void main(void) {
           DestTexCoord = SourceTexCoord;
           DestColor    = SourceColor;
           gl_Position  = Transform * Position;
       }"
      ,
      "uniform sampler2D texture1;
       #include 'hsv_rgb.glsl'

       void main(void) {
           vec4 tcolor = texture2D(texture1, DestTexCoord);
           gl_FragColor = vec4(hsv2rgb(DestColor.rgb * rgb2hsv(tcolor.rgb)), 
                              DestColor.a * tcolor.a);
       }"
   },

   -- draws dynamic glowey halos around spaceships
   -- FIXME optimize this for 1 point?
   -- NOTE: the lengthSqr distance compare, with scale * scale, has precision issues
   -- NOTE: when scale is very small!
   ShaderWorley = {
      ""
      ,
      "void main(void) {
          gl_Position = Transform * Position;
      }"
      ,
      "uniform vec2  points[POINTS];
      uniform vec3  color;
      uniform float scale;
      uniform sampler2D dither;
      void main(void) {
          float minv = 100000000000000.0;
          for (int i=0; i<POINTS; i++)
          {
               vec2 d = points[i] - gl_FragCoord.xy;
      
      //         float v = length(d);
      //         float v = distance(points[i],  gl_FragCoord.xy);
                 float v = d.x * d.x + d.y * d.y;
      //         float v = sqrt(d.x*d.x + d.x*d.y + d.y* d.y);
      //           float v = abs(d.x) + abs(d.y);
               minv = min(minv, v);
          }
      //    float myv = minv * scale;
          float myv = sqrt(minv) * scale;
      //    float myv = minv * scale * scale;
          if (myv > 1.0) {
              discard;
          }
          float alpha = 1.0 - myv;
          gl_FragColor = vec4(alpha * color.rgb, alpha);
      #if DITHER
          //alpha += texture2D(dither, gl_FragCoord.xy / 8.0).r / 4.0 - (1.0 / 128.0);
          gl_FragColor += vec4(texture2D(dither, gl_FragCoord.xy / 8.0).r / 16.0 - (1.0 / 64.0));
      #endif
          
      }",
   }

   -- obsolete shader for hexagon particles
   ShaderParticles = {
      "varying vec4 DestinationColor;"
      ,
      "attribute vec2  Offset;
       attribute float StartTime;
       attribute float EndTime;
       attribute vec3  Velocity;
       attribute vec4  Color;
       uniform   float CurrentTime;
       void main(void) {
           if (CurrentTime >= EndTime) {
               gl_Position = vec4(0.0, 0.0, -99999999.0, 1);
               return;
           }
           float deltaT = CurrentTime - StartTime;
           vec3  velocity = pow(0.8, deltaT) * Velocity;
           vec3  position = Position.xyz + vec3(Offset, 0.0) + deltaT * velocity;
           float v = deltaT / (EndTime - StartTime);
           DestinationColor = (1.0 - v) * Color;
           gl_Position = Transform * vec4(position, 1);
       }"
      ,
      "void main(void) {
           gl_FragColor = DestinationColor;
       }"
   }

   -- used for particles (GL_POINTS)
   ShaderParticlePoints = {
      "varying vec4 DestinationColor;
       varying float Sides;"
      ,
      "attribute vec2  Offset;
       attribute float StartTime;
       attribute float EndTime;
       attribute vec3  Velocity;
       attribute vec4  Color;
       uniform   float CurrentTime;
       uniform   float ToPixels;
       void main(void) {
           float size = 1.5 * ToPixels * Offset.x;
           if (CurrentTime >= EndTime || size < 0.25) {
               gl_Position = vec4(0.0, 0.0, -99999999.0, 1);
               return;
           }
           float deltaT = CurrentTime - StartTime;
           vec3  velocity = pow(0.8, deltaT) * Velocity;
           vec3  position = Position.xyz + deltaT * velocity;
           float v = deltaT / (EndTime - StartTime);
           DestinationColor = (1.0 - v) * Color;
           Sides = Offset.y;
           gl_PointSize = size;
           gl_Position = Transform * vec4(position, 1);
       }"
      ,
      "
       float length2(vec2 x) { return dot(x, x); }
       float mlength(vec2 x) { return abs(x.x) + abs(x.y); }
       float squared(float x) { return x * x; }

       void main(void) {
           //float val = 1.0 - squared(2.0 * length(gl_PointCoord - 0.5));
           float val = 1.0 - 4.0 * length2(gl_PointCoord - 0.5);
           //float val = 1.0 - 2.0 * mlength(gl_PointCoord - 0.5);
           //float val = 1.0 - 2.0 * length(gl_PointCoord - 0.5);
           if (val <= 0.0)
               discard;
           //val = max(0.0, val);
           if (Sides > 0.0) {
               gl_FragColor = DestinationColor;
           } else {
               gl_FragColor = DestinationColor * val;
           }
       }"
   }

   -- full screen blurs. 
   ShaderBlur = {
      "varying vec2 DestTexCoord;"
      ,
      "attribute vec2 SourceTexCoord;
       void main(void) {
           DestTexCoord = SourceTexCoord;
           gl_Position  = Transform * Position;
       }"
      ,
      -- BLUR expands to the sum of a number of texture2d calls depending on the blur radius
      "uniform sampler2D source;
       uniform vec2 offsets[BLUR_SIZE];
       void main() {
           gl_FragColor = BLUR(source, DestTexCoord, offsets);
       }"
   }
}
