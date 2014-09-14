-- glsl shaders
-- #version 120

{
   ShaderWormhole = {
      "varying vec4 DestinationColor;"
      ,
      "
      attribute float StartTime;
      attribute vec2  Offset;
      attribute vec4  Color;
      uniform   float CurrentTime;
      uniform   vec4  CenterPos;
      void main(void) {
          float edge = StartTime ;//* (1 + sin(CurrentTime/10000) / 2);
          //float a = -210 * edge;
          float a = -210 * edge;
          float b = 40.f;
          float spirals = 8.0;
          float scale   = 1.8;
          float range   = spirals * scale;
          float theta = mod((-CurrentTime / 1 + 10000 * edge) / 8, spirals) * scale;
          //float r = a + b * theta;
          //float r = 10 * a / (range - theta);
          float r = a * exp(b * theta / 350);
          vec2 rot = vec2(cos(theta), sin(theta));
          float spin = 10 * edge;
          vec2 rot2 = vec2(cos(spin * theta), sin(spin * theta));
          float size = 24;// + 5 * sin(6 * (range - theta));
          vec2 offset = size * vec2(rot2.x * Offset.x - rot2.y * Offset.y,
                                   rot2.y * Offset.x + rot2.x * Offset.y);
          vec4 pos = CenterPos + vec4(offset + r * rot,
                                     -1000/theta - 0.25 * (range - theta) - 20 * edge,
                                     0);
          gl_Position = Transform * pos;

          float fadein = 3;
          float fadeout = 1;
          float alpha = 0.5 * ((theta > (range - fadein)) ? (range - theta) / fadein :
                               (theta < fadeout) ? theta / fadeout : 1.0);
          DestinationColor = Color.a * alpha * vec4(mix(vec3(.54, .97, 0.9),
                                                      vec3(0.0, .23, .56), edge), 
                                  0.65);
      }"
      ,
      "void main(void) {
         gl_FragColor = DestinationColor;
      }"
   },

   ShaderIridescent = {
      "varying vec4 DestinationColor;"
      ,
      "attribute vec4 SourceColor0;
      attribute vec4 SourceColor1;
      attribute float TimeA;
      uniform float TimeU;
      void main(void) {
          gl_Position = Transform * Position;
          float val = 0.5 + 0.5 * sin(TimeU + TimeA);
          DestinationColor = mix(SourceColor0, SourceColor1, val);
      }"
      ,
      "void main(void) {
            gl_FragColor = DestinationColor;
      }"
   },

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
          vec4 color = texture2D(texture1, DestTexCoord);
          float mx = max(color.r, max(color.g, color.b));
          if (mx > 1.0) {
              // color.rgb += 1.0 * vec3(mx - 1.0);
              color.rgb += 1.0 * vec3(log(mx));
          }
      //    color.rgb = max(color.rgb, vec3(0.01, 0.01, 0.01));
      //    color.rgb *= 1.5;
      //    color.rgb = color.rgb / (1 + max(color.r, max(color.g, color.b)));
      //    color.rgb = color.rgb / (1 + color.rgb);
      //    color.rgb = pow(color.rgb,vec3(1/2.2));
          gl_FragColor = color;
      }"
   },
   
   ShaderTonemapDither = {
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
          vec4 color = texture2D(texture1, DestTexCoord);
          if (color.rgb != vec3(0.0))
          {
              float mx = max(color.r, max(color.g, color.b));
              if (mx > 1.0) {
                  color.rgb += 1.0 * vec3(mx - 1.0);
              }
              float ditherv = texture2D(dithertex, gl_FragCoord.xy / 8.0).r / 128.0 - (1.0 / 128.0);
              color += vec4(ditherv);
          }
      //    color.rgb = max(color.rgb, vec3(0.01, 0.01, 0.01));
      //    color.rgb *= 1.5;
      //    color.rgb = color.rgb / (1 + max(color.r, max(color.g, color.b)));
      //    color.rgb = color.rgb / (1 + color.rgb);
      //    color.rgb = pow(color.rgb,vec3(1/2.2));
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

      -- http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
      "vec3 hsv2rgb(vec3 c)
       {
           vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
           vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
           return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
       }

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

       // http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
       vec3 rgb2hsv(vec3 c)
       {
           vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
           vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
           vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

           float d = q.x - min(q.w, q.y);
           float e = 1.0e-10;
           return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
       }

       vec3 hsv2rgb(vec3 c)
       {
           vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
           vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
           return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
       }

       void main(void) {
           vec4 tcolor = texture2D(texture1, DestTexCoord);
           gl_FragColor = vec4(hsv2rgb(DestColor.rgb * rgb2hsv(tcolor.rgb)), 
                              DestColor.a * tcolor.a);
       }"
   },

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

   ShaderBlur = {
      "varying vec2 DestTexCoord;"
      ,
      "attribute vec2 SourceTexCoord;
       void main(void) {
           DestTexCoord = SourceTexCoord;
           gl_Position  = Transform * Position;
       }"
      ,
      "uniform sampler2D source;
       uniform float coefficients[5];
       uniform vec2 offsets[5];
       void main() {
           float d = 0.1;
           vec4 c = vec4(0, 0, 0, 0);
           vec2 tc = DestTexCoord;
           c += coefficients[0] * texture2D(source, tc + offsets[0]);
           c += coefficients[1] * texture2D(source, tc + offsets[1]);
           c += coefficients[2] * texture2D(source, tc + offsets[2]);
           c += coefficients[3] * texture2D(source, tc + offsets[3]);
           c += coefficients[4] * texture2D(source, tc + offsets[4]);
           gl_FragColor = c;
       }"
   }
}
