#include "shaderconv.h"

#include <stdio.h>
#include "../glx/hardext.h"
#include "debug.h"
#include "fpe_shader.h"
#include "init.h"
#include "preproc.h"
#include "string_utils.h"
#include "shader_hacks.h"

typedef struct {
    const char* glname;
    const char* name;
    const char* type;
    const char* prec;
    reserved_attrib_t attrib;
} builtin_attrib_t;

const builtin_attrib_t builtin_attrib[] = {
    {"gl_Vertex", "_gl4es_Vertex", "vec4", "highp", ATT_VERTEX},
    {"gl_Color", "_gl4es_Color", "vec4", "lowp", ATT_COLOR},
    {"gl_MultiTexCoord0", "_gl4es_MultiTexCoord0", "vec4", "highp", ATT_MULTITEXCOORD0},
    {"gl_MultiTexCoord1", "_gl4es_MultiTexCoord1", "vec4", "highp", ATT_MULTITEXCOORD1},
    {"gl_MultiTexCoord2", "_gl4es_MultiTexCoord2", "vec4", "highp", ATT_MULTITEXCOORD2},
    {"gl_MultiTexCoord3", "_gl4es_MultiTexCoord3", "vec4", "highp", ATT_MULTITEXCOORD3},
    {"gl_MultiTexCoord4", "_gl4es_MultiTexCoord4", "vec4", "highp", ATT_MULTITEXCOORD4},
    {"gl_MultiTexCoord5", "_gl4es_MultiTexCoord5", "vec4", "highp", ATT_MULTITEXCOORD5},
    {"gl_MultiTexCoord6", "_gl4es_MultiTexCoord6", "vec4", "highp", ATT_MULTITEXCOORD6},
    {"gl_MultiTexCoord7", "_gl4es_MultiTexCoord7", "vec4", "highp", ATT_MULTITEXCOORD7},
    {"gl_SecondaryColor", "_gl4es_SecondaryColor", "vec4", "lowp", ATT_SECONDARY},
    {"gl_Normal", "_gl4es_Normal", "vec3", "highp", ATT_NORMAL},
    {"gl_FogCoord", "_gl4es_FogCoord", "float", "highp", ATT_FOGCOORD}
};

typedef struct {
    const char* glname;
    const char* name;
    const char* type;
    int   texarray;
    reserved_matrix_t matrix;
} builtin_matrix_t;

const builtin_matrix_t builtin_matrix[] = {
    {"gl_ModelViewMatrixInverseTranspose", "_gl4es_ITModelViewMatrix", "mat4", 0, MAT_MV_IT},
    {"gl_ModelViewMatrixInverse", "_gl4es_IModelViewMatrix", "mat4", 0, MAT_MV_I},
    {"gl_ModelViewMatrixTranspose", "_gl4es_TModelViewMatrix", "mat4", 0, MAT_MV_T},
    {"gl_ModelViewMatrix", "_gl4es_ModelViewMatrix", "mat4", 0, MAT_MV},
    {"gl_ProjectionMatrixInverseTranspose", "_gl4es_ITProjectionMatrix", "mat4", 0, MAT_P_IT},
    {"gl_ProjectionMatrixInverse", "_gl4es_IProjectionMatrix", "mat4", 0, MAT_P_I},
    {"gl_ProjectionMatrixTranspose", "_gl4es_TProjectionMatrix", "mat4", 0, MAT_P_T},
    {"gl_ProjectionMatrix", "_gl4es_ProjectionMatrix", "mat4", 0, MAT_P},
    {"gl_ModelViewProjectionMatrixInverseTranspose", "_gl4es_ITModelViewProjectionMatrix", "mat4", 0, MAT_MVP_IT},
    {"gl_ModelViewProjectionMatrixInverse", "_gl4es_IModelViewProjectionMatrix", "mat4", 0, MAT_MVP_I},
    {"gl_ModelViewProjectionMatrixTranspose", "_gl4es_TModelViewProjectionMatrix", "mat4", 0, MAT_MVP_T},
    {"gl_ModelViewProjectionMatrix", "_gl4es_ModelViewProjectionMatrix", "mat4", 0, MAT_MVP},
    // non standard version to avoid useless array of Matrix Uniform (in case the compiler as issue optimising this)
    {"gl_TextureMatrix_0", "_gl4es_TextureMatrix_0", "mat4", 0, MAT_T0},
    {"gl_TextureMatrix_1", "_gl4es_TextureMatrix_1", "mat4", 0, MAT_T1},
    {"gl_TextureMatrix_2", "_gl4es_TextureMatrix_2", "mat4", 0, MAT_T2},
    {"gl_TextureMatrix_3", "_gl4es_TextureMatrix_3", "mat4", 0, MAT_T3},
    {"gl_TextureMatrix_4", "_gl4es_TextureMatrix_4", "mat4", 0, MAT_T4},
    {"gl_TextureMatrix_5", "_gl4es_TextureMatrix_5", "mat4", 0, MAT_T5},
    {"gl_TextureMatrix_6", "_gl4es_TextureMatrix_6", "mat4", 0, MAT_T6},
    {"gl_TextureMatrix_7", "_gl4es_TextureMatrix_7", "mat4", 0, MAT_T7},
    // regular texture matrix
    {"gl_TextureMatrixInverseTranspose", "_gl4es_ITTextureMatrix", "mat4", 1, MAT_T0_IT},
    {"gl_TextureMatrixInverse", "_gl4es_ITextureMatrix", "mat4", 1, MAT_T0_I},
    {"gl_TextureMatrixTranspose", "_gl4es_TTextureMatrix", "mat4", 1, MAT_T0_T},
    {"gl_TextureMatrix", "_gl4es_TextureMatrix", "mat4", 1, MAT_T0},
    {"gl_NormalMatrix", "_gl4es_NormalMatrix", "mat3", 0, MAT_N}
  };

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
static const char* gl4es_MaxLightsSource =
"#define _gl4es_MaxLights " STR(MAX_LIGHT) "\n";
static const char* gl4es_MaxClipPlanesSource =
"#define _gl4es_MaxClipPlanes " STR(MAX_CLIP_PLANES) "\n";
static const char* gl4es_MaxTextureUnitsSource =
"#define _gl4es_MaxTextureUnits " STR(MAX_TEX) "\n";
static const char* gl4es_MaxTextureCoordsSource =
"#define _gl4es_MaxTextureCoords " STR(MAX_TEX) "\n";
#undef STR
#undef STR_HELPER

static const char* gl4es_LightSourceParametersSource = 
"struct gl_LightSourceParameters\n"
"{\n"
"   vec4 ambient;\n"
"   vec4 diffuse;\n"
"   vec4 specular;\n"
"   vec4 position;\n"
"   vec4 halfVector;\n"   //halfVector = normalize(normalize(position) + vec3(0,0,1) if vbs==FALSE)
"   vec3 spotDirection;\n"
"   float spotExponent;\n"
"   float spotCutoff;\n"
"   float spotCosCutoff;\n"
"   float constantAttenuation;\n"
"   float linearAttenuation;\n"
"   float quadraticAttenuation;\n"
"};\n"
"uniform gl_LightSourceParameters gl_LightSource[gl_MaxLights];\n";

static const char* gl4es_LightModelParametersSource =
"struct gl_LightModelParameters {\n"
"  vec4 ambient;\n"
"};\n"
"uniform gl_LightModelParameters gl_LightModel;\n";

static const char* gl4es_MaterialParametersSource =
"struct gl_MaterialParameters\n"
"{\n"
"   vec4 emission;\n"
"   vec4 ambient;\n"
"   vec4 diffuse;\n"
"   vec4 specular;\n"
"   float shininess;\n"
"};\n"
"uniform gl_MaterialParameters gl_FrontMaterial;\n"
"uniform gl_MaterialParameters gl_BackMaterial;\n";

static const char* gl4es_LightModelProductsSource =
"struct gl_LightModelProducts\n"
"{\n"
"   vec4 sceneColor;\n"
"};\n"
"uniform gl_LightModelProducts gl_FrontLightModelProduct;\n"
"uniform gl_LightModelProducts gl_BackLightModelProduct;\n";

static const char* gl4es_LightProductsSource =
"struct gl_LightProducts\n"
"{\n"
"   vec4 ambient;\n"
"   vec4 diffuse;\n"
"   vec4 specular;\n"
"};\n"
"uniform gl_LightProducts gl_FrontLightProduct[gl_MaxLights];\n"
"uniform gl_LightProducts gl_BackLightProduct[gl_MaxLights];\n";

static const char* gl4es_PointSpriteSource =
"struct gl_PointParameters\n"
"{\n"
"   float size;\n"
"   float sizeMin;\n"
"   float sizeMax;\n"
"   float fadeThresholdSize;\n"
"   float distanceConstantAttenuation;\n"
"   float distanceLinearAttenuation;\n"
"   float distanceQuadraticAttenuation;\n"
"};\n"
"uniform gl_PointParameters gl_Point;\n";

static const char* gl4es_FogParametersSource =
"struct gl_FogParameters {\n"
"    lowp vec4 color;\n"
"    mediump float density;\n"
"    mediump float start;\n"
"    mediump float end;\n"
"    mediump float scale;\n"   // Derived:   1.0 / (end - start) 
"};\n"
"uniform gl_FogParameters gl_Fog;\n";

static const char* gl4es_texenvcolorSource =
"uniform vec4 gl_TextureEnvColor[gl_MaxTextureUnits];\n";

static const char* gl4es_texgeneyeSource[4] = {
"uniform vec4 gl_EyePlaneS[gl_MaxTextureCoords];\n",
"uniform vec4 gl_EyePlaneT[gl_MaxTextureCoords];\n",
"uniform vec4 gl_EyePlaneR[gl_MaxTextureCoords];\n",
"uniform vec4 gl_EyePlaneQ[gl_MaxTextureCoords];\n" };

static const char* gl4es_texgenobjSource[4] = {
"uniform vec4 gl_ObjectPlaneS[gl_MaxTextureCoords];\n",
"uniform vec4 gl_ObjectPlaneT[gl_MaxTextureCoords];\n",
"uniform vec4 gl_ObjectPlaneR[gl_MaxTextureCoords];\n",
"uniform vec4 gl_ObjectPlaneQ[gl_MaxTextureCoords];\n" };

static const char* gl4es_clipplanesSource = 
"uniform vec4  gl_ClipPlane[gl_MaxClipPlanes];\n";

static const char* gl4es_normalscaleSource =
"uniform float gl_NormalScale;\n";

static const char* gl4es_instanceID =
"#define GL_ARB_draw_instanced 1\n"
"uniform int _gl4es_InstanceID;\n";

static const char* gl4es_frontColorSource =
"varying lowp vec4 _gl4es_FrontColor;\n";

static const char* gl4es_backColorSource =
"varying lowp vec4 _gl4es_BackColor;\n";

static const char* gl4es_frontSecondaryColorSource =
"varying lowp vec4 _gl4es_FrontSecondaryColor;\n";

static const char* gl4es_backSecondaryColorSource =
"varying lowp vec4 _gl4es_BackSecondaryColor;\n";

static const char* gl4es_texcoordSource =
"varying mediump vec4 _gl4es_TexCoord[%d];\n";

static const char* gl4es_texcoordSourceAlt =
"varying mediump vec4 _gl4es_TexCoord_%d;\n";

static const char* gl4es_fogcoordSource =
"varying mediump float _gl4es_FogFragCoord;\n";

static const char* gl4es_ftransformSource = 
"\n"
"highp vec4 ftransform() {\n"
" return gl_ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n";

static const char* gl4es_dummyClipVertex = 
"vec4 dummyClipVertex_%d";

static const char* gl_TexCoordSource = "gl_TexCoord[";

static const char* gl_TexMatrixSources[] = {
"gl_TextureMatrixInverseTranspose[",
"gl_TextureMatrixInverse[",
"gl_TextureMatrixTranspose[",
"gl_TextureMatrix["
};

static const char* GLESHeader[] = {
  "#version 100\n%sprecision %s float;\nprecision %s int;\n",
  "#version 120\n%sprecision %s float;\nprecision %s int;\n",
  "#version 310es\n%sprecision %s float;\nprecision %s int;\n",
  "#version 300es\n%sprecision %s float;\nprecision %s int;\n"
};

static const char* gl4es_transpose =
"mat2 gl4es_transpose(mat2 m) {\n"
" return mat2(m[0][0], m[0][1],\n"
"             m[1][0], m[1][1]);\n"
"}\n"
"mat3 gl4es_transpose(mat3 m) {\n"
" return mat3(m[0][0], m[0][1], m[0][2],\n"
"             m[1][0], m[1][1], m[1][2],\n"
"             m[2][0], m[2][1], m[2][2]);\n"
"}\n"
"mat4 gl4es_transpose(mat4 m) {\n"
" return mat4(m[0][0], m[0][1], m[0][2], m[0][3],\n"
"             m[1][0], m[1][1], m[1][2], m[1][3],\n"
"             m[2][0], m[2][1], m[2][2], m[2][3],\n"
"             m[3][0], m[3][1], m[3][2], m[3][3]);\n"
"}\n";

static const char* HackAltPow = 
"float pow(float f, int a) {\n"
" return pow(f, float(a));\n"
"}\n";
static const char* HackAltMax = 
"float max(float a, int b) {\n"
" return max(a, float(b));\n"
"}\n"
"float max(int a, float b) {\n"
" return max(float(a), b);\n"
"}\n";
static const char* HackAltMin = 
"float min(float a, int b) {\n"
" return min(a, float(b));\n"
"}\n"
"float min(int a, float b) {\n"
" return min(float(a), b);\n"
"}\n";
static const char* HackAltClamp = 
"float clamp(float f, int a, int b) {\n"
" return clamp(f, float(a), float(b));\n"
"}\n"
"float clamp(float f, float a, int b) {\n"
" return clamp(f, a, float(b));\n"
"}\n"
"float clamp(float f, int a, float b) {\n"
" return clamp(f, float(a), b);\n"
"}\n"
"vec2 clamp(vec2 f, int a, int b) {\n"
" return clamp(f, float(a), float(b));\n"
"}\n"
"vec2 clamp(vec2 f, float a, int b) {\n"
" return clamp(f, a, float(b));\n"
"}\n"
"vec2 clamp(vec2 f, int a, float b) {\n"
" return clamp(f, float(a), b);\n"
"}\n"
"vec3 clamp(vec3 f, int a, int b) {\n"
" return clamp(f, float(a), float(b));\n"
"}\n"
"vec3 clamp(vec3 f, float a, int b) {\n"
" return clamp(f, a, float(b));\n"
"}\n"
"vec3 clamp(vec3 f, int a, float b) {\n"
" return clamp(f, float(a), b);\n"
"}\n"
"vec4 clamp(vec4 f, int a, int b) {\n"
" return clamp(f, float(a), float(b));\n"
"}\n"
"vec4 clamp(vec4 f, float a, int b) {\n"
" return clamp(f, a, float(b));\n"
"}\n"
"vec4 clamp(vec4 f, int a, float b) {\n"
" return clamp(f, float(a), b);\n"
"}\n";


static const char* HackAltMod = 
"float mod(float f, int a) {\n"
" return mod(f, float(a));\n"
"}\n"
"vec2 mod(vec2 f, int a) {\n"
" return mod(f, float(a));\n"
"}\n"
"vec3 mod(vec3 f, int a) {\n"
" return mod(f, float(a));\n"
"}\n"
"vec4 mod(vec4 f, int a) {\n"
" return mod(f, float(a));\n"
"}\n";

static const char* texture2DLodAlt =
"vec4 _gl4es_texture2DLod(sampler2D sampler, vec2 coord, float lod) {\n"
" return texture2D(sampler, coord);\n"
"}\n";


char* ConvertShader(const char* pEntry, int isVertex, shaderconv_need_t *need)
{
  int fpeShader = (strstr(pEntry, fpeshader_signature)!=NULL)?1:0;
  int maskbefore = 4|(isVertex?1:2);
  int maskafter = 8|(isVertex?1:2);
  if((globals4es.dbgshaderconv&maskbefore)==maskbefore) {
    printf("Shader source%s:\n%s\n", pEntry, fpeShader?" (FPEShader generated)":"");
  }
  int comments = globals4es.comments;
  
  char* pBuffer = (char*)pEntry;

  int version120 = 0;
  char* versionString = NULL;
  if(!fpeShader) {
    extensions_t exts;  // dummy...
    exts.cap = exts.size = 0;
    exts.ext = NULL;
    // hacks
    char* pHacked = ShaderHacks(pBuffer);
    // preproc first
    pBuffer = preproc(pHacked, comments, globals4es.shadernogles, &exts, &versionString);
    if(pHacked!=pEntry && pHacked!=pBuffer)
      free(pHacked);
    // now comment all line starting with precision...
    if(strstr(pBuffer, "\nprecision")) {
      int sz = strlen(pBuffer);
      pBuffer = InplaceReplace(pBuffer, &sz, "\nprecision", "\n//precision");
    }
    // should do something with the extension list...
    if(exts.ext)
      free(exts.ext);
  }

  static shaderconv_need_t dummy_need = {0};
  if(!need) {
    need = &dummy_need;
    need->need_texcoord = -1;
    need->need_clean = 1; // no hack, this is a dummy need structure
  }
  int notexarray = globals4es.notexarray || need->need_notexarray || fpeShader;

  //const char* GLESUseFragHighp = "#extension GL_OES_fragment_precision_high : enable\n"; // does this one is needed?  
  char GLESFullHeader[512];
  int wanthighp = !fpeShader;
  if(wanthighp && !hardext.highp) wanthighp = 0;
  int versionHeader = 0;
  if(versionString && strcmp(versionString, "120")==0)
     version120 = 1;
  if(version120) {
    if(hardext.glsl120) versionHeader = 1;
    else if(hardext.glsl310es) versionHeader = 2;
    else if(hardext.glsl300es) { versionHeader = 3; /* location on uniform not supported ! */ }
    /* else no location or in / out are supported */
  }
  //sprintf(GLESFullHeader, GLESHeader, (wanthighp && hardext.highp==1 && !isVertex)?GLESUseFragHighp:"", (wanthighp)?"highp":"mediump", (wanthighp)?"highp":"mediump");
  sprintf(GLESFullHeader, GLESHeader[versionHeader], "", (wanthighp)?"highp":"mediump", (wanthighp)?"highp":"mediump");

  int tmpsize = strlen(pBuffer)*2+strlen(GLESFullHeader)+100;
  char* Tmp = (char*)calloc(1, tmpsize);
  strcpy(Tmp, pBuffer);

  // and now change the version header, and add default precision
  char* newptr;
  newptr=strstr(Tmp, "#version");
  if (!newptr) {
    Tmp = InplaceInsert(Tmp, GLESFullHeader, Tmp, &tmpsize);
  } else {
    while(*newptr!=0x0a) newptr++;
    newptr++;
    memmove(Tmp, newptr, strlen(newptr)+1);
    Tmp = InplaceInsert(Tmp, GLESFullHeader, Tmp, &tmpsize);
  }
  int headline = 3;
  // check if gl_FragDepth is used
  int fragdepth = (strstr(pBuffer, "gl_FragDepth"))?1:0;
  const char* GLESUseFragDepth = "#extension GL_EXT_frag_depth : enable\n";
  const char* GLESFakeFragDepth = "mediump float fakeFragDepth = 0.0;\n";
  if (fragdepth) {
    /* If #extension is used, it should be placed before the second line of the header. */
    if(hardext.fragdepth)
      Tmp = InplaceInsert(GetLine(Tmp, 1), GLESUseFragDepth, Tmp, &tmpsize);
    else
      Tmp = InplaceInsert(GetLine(Tmp, headline-1), GLESFakeFragDepth, Tmp, &tmpsize);
    headline++;
  }
  int derivatives = (strstr(pBuffer, "dFdx(") || strstr(pBuffer, "dFdy(") || strstr(pBuffer, "fwidth("))?1:0;
  const char* GLESUseDerivative = "#extension GL_OES_standard_derivatives : enable\n";
  // complete fake value... A better thing should be use....
  const char* GLESFakeDerivative = "float dFdx(float p) {return 0.0001;}\nvec2 dFdx(vec2 p) {return vec2(0.0001);}\nvec3 dFdx(vec3 p) {return vec3(0.0001);}\n"
  "float dFdy(float p) {return 0.0001;}\nvec2 dFdy(vec2 p) {return vec2(0.0001);}\nvec3 dFdy(vec3 p) {return vec3(0.0001);}\n"
  "float fwidth(float p) {return abs(dFdx(p))+abs(dFdy(p));}\nvec2 fwidth(vec2 p) {return abs(dFdx(p))+abs(dFdy(p));}\n"
  "vec3 fwidth(vec3 p) {return abs(dFdx(p))+abs(dFdy(p));}\n";
  if (derivatives) {
    /* If #extension is used, it should be placed before the second line of the header. */
    if(hardext.derivatives)
      Tmp = InplaceInsert(GetLine(Tmp, 1), GLESUseDerivative, Tmp, &tmpsize);
    else
      Tmp = InplaceInsert(GetLine(Tmp, headline-1), GLESFakeDerivative, Tmp, &tmpsize);
    headline++;
  }
  // if some functions are used, add some int/float alternative
  if(strstr(Tmp, "pow(") || strstr(Tmp, "pow (")) {
      Tmp = InplaceInsert(GetLine(Tmp, headline), HackAltPow, Tmp, &tmpsize);
  }
  if(strstr(Tmp, "max(") || strstr(Tmp, "max (")) {
      Tmp = InplaceInsert(GetLine(Tmp, headline), HackAltMax, Tmp, &tmpsize);
  }
  if(strstr(Tmp, "min(") || strstr(Tmp, "min (")) {
      Tmp = InplaceInsert(GetLine(Tmp, headline), HackAltMin, Tmp, &tmpsize);
  }
  if(strstr(Tmp, "clamp(") || strstr(Tmp, "clamp (")) {
      Tmp = InplaceInsert(GetLine(Tmp, headline), HackAltClamp, Tmp, &tmpsize);
  }
  if(strstr(Tmp, "mod(") || strstr(Tmp, "mod (")) {
      Tmp = InplaceInsert(GetLine(Tmp, headline), HackAltMod, Tmp, &tmpsize);
  }
  if(!isVertex && (strstr(Tmp, "texture2DLod(") || strstr(Tmp, "texture2DLod ("))) {
      Tmp = InplaceReplace(Tmp, &tmpsize, "texture2DLod(", "_gl4es_texture2DLod(");
      Tmp = InplaceReplace(Tmp, &tmpsize, "texture2DLod (", "_gl4es_texture2DLod (");
      Tmp = InplaceInsert(GetLine(Tmp, headline), texture2DLodAlt, Tmp, &tmpsize);
  }
    // now check to remove trailling "f" after float, as it's not supported too
  newptr = Tmp;
  // simple state machine...
  int state = 0;
  while (*newptr!=0x00) {
    switch(state) {
      case 0:
        if ((*newptr >= '0') && (*newptr <= '9'))
          state = 1;  // integer part
        else if (*newptr == '.')
          state = 2;  // fractional part
        else if ((*newptr==' ') || (*newptr==0x0d) || (*newptr==0x0a) || (*newptr=='-') || (*newptr=='+') || (*newptr=='*') || (*newptr=='/') || (*newptr=='(') || (*newptr==')' || (*newptr=='>') || (*newptr=='<')))
          state = 0; // separator
        else 
          state = 3; // something else
        break;
      case 1: // integer part
        if ((*newptr >= '0') && (*newptr <= '9'))
          state = 1;  // integer part
        else if (*newptr == '.')
          state = 2;  // fractional part
        else if ((*newptr==' ') || (*newptr==0x0d) || (*newptr==0x0a) || (*newptr=='-') || (*newptr=='+') || (*newptr=='*') || (*newptr=='/') || (*newptr=='(') || (*newptr==')' || (*newptr=='>') || (*newptr=='<')))
          state = 0; // separator
        else  if (*newptr == 'f' ) {
          // remove that f
          memmove(newptr, newptr+1, strlen(newptr+1)+1);
          newptr--;
        } else
          state = 3;
          break;
      case 2: // fractionnal part
        if ((*newptr >= '0') && (*newptr <= '9'))
          state = 2;
        else if ((*newptr==' ') || (*newptr==0x0d) || (*newptr==0x0a) || (*newptr=='-') || (*newptr=='+') || (*newptr=='*') || (*newptr=='/') || (*newptr=='(') || (*newptr==')' || (*newptr=='>') || (*newptr=='<')))
          state = 0; // separator
        else  if (*newptr == 'f' ) {
          // remove that f
          memmove(newptr, newptr+1, strlen(newptr+1)+1);
          newptr--;
        } else
          state = 3;
          break;
      case 3:
        if ((*newptr==' ') || (*newptr==0x0d) || (*newptr==0x0a) || (*newptr=='-') || (*newptr=='+') || (*newptr=='*') || (*newptr=='/') || (*newptr=='(') || (*newptr==')' || (*newptr=='>') || (*newptr=='<')))
          state = 0; // separator
        else      
          state = 3;
          break;
    }
    newptr++;
  }
  Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FragDepth", (hardext.fragdepth)?"gl_FragDepthEXT":"fakeFragDepth");
  {
    // check for ftransform function
    if(isVertex) {
      if(strstr(Tmp, "ftransform(")) {
        Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_ftransformSource, Tmp, &tmpsize);
        // don't increment headline count, as all variying and attributes should be created before
      }
    }
    if(strstr(Tmp, "transpose(") || strstr(Tmp, "transpose ") || strstr(Tmp, "transpose\t")) {
      Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_transpose, Tmp, &tmpsize);
      InplaceReplace(Tmp, &tmpsize, "transpose", "gl4es_transpose");
      // don't increment headline count, as all variying and attributes should be created before
    }
    // check for builtin matrix uniform...
    {
      // first check number of texture matrices used
      int ntex = -1;
      // Try to determine max Texture matrice used, for each transposed inverse or regular...
      for(int i=0; i<4; ++i) {
        char* p = Tmp;
        while((p=strstr(p, gl_TexMatrixSources[i]))) {
          p+=strlen(gl_TexMatrixSources[i]);
          if(*p>='0' && *p<='9') {
            int n = (*p) - '0';
            if (ntex<n) ntex = n;
          }
        }
      }
        
      // if failed to determine, take max...
      if (ntex==-1) ntex = hardext.maxtex; else ++ntex;
      // change gl_TextureMatrix[X] to gl_TextureMatrix_X if notexrray
      if(notexarray) {
        for (int k=0; k<ntex+1; k++) {
          char d[100];
          char d2[100];
          sprintf(d2, "gl_TextureMatrix[%d]", k);
          if(strstr(Tmp, d2)) {
            sprintf(d, "gl_TextureMatrix_%d", k);
            Tmp = InplaceReplace(Tmp, &tmpsize, d2, d);
          }
        }
      }

      int n = sizeof(builtin_matrix)/sizeof(builtin_matrix_t);
      for (int i=0; i<n; i++) {
          if(strstr(Tmp, builtin_matrix[i].glname)) {
              // ok, this matrix is used
              // replace gl_name by _gl4es_ one
              Tmp = InplaceReplace(Tmp, &tmpsize, builtin_matrix[i].glname, builtin_matrix[i].name);
              // insert a declaration of it
              char def[100];
              int ishighp = (isVertex || hardext.highp)?1:0;
              if(builtin_matrix[i].matrix == MAT_N) {
                if(need->need_normalmatrix && !hardext.highp)
                  ishighp = 0;
                if(!hardext.highp && !isVertex)
                  need->need_normalmatrix = 1;
              }
              if(builtin_matrix[i].matrix == MAT_MV) {
                if(need->need_mvmatrix && !hardext.highp)
                  ishighp = 0;
                if(!hardext.highp && !isVertex)
                  need->need_mvmatrix = 1;
              }
              if(builtin_matrix[i].matrix == MAT_MVP) {
                if(need->need_mvpmatrix && !hardext.highp)
                  ishighp = 0;
                if(!hardext.highp && !isVertex)
                  need->need_mvpmatrix = 1;
              }
              if(builtin_matrix[i].texarray)
                  sprintf(def, "uniform %s%s %s[%d];\n", (ishighp)?"highp ":"mediump ", builtin_matrix[i].type, builtin_matrix[i].name, ntex);
              else
                  sprintf(def, "uniform %s%s %s;\n", (ishighp)?"highp ":"mediump ", builtin_matrix[i].type, builtin_matrix[i].name);
              Tmp = InplaceInsert(GetLine(Tmp, headline++), def, Tmp, &tmpsize);
          }
      }
    }
  }
  // Handling of gl_LightSource[x].halfVector => normalize(gl_LightSource[x].position - gl_Vertex), but what if in the FragShader ?
/*  if(strstr(Tmp, "halfVector"))
  {
    char *p = Tmp;
    while((p=strstr(p, "gl_LightSource["))) {
      char *p2 = strchr(p, ']');
      if (p2 && !strncmp(p2, "].halfVector", strlen("].halfVector"))) {
        // found an occurence, lets change
        char p3[500];
        strncpy(p3,p, (p2-p)+1); p3[(p2-p)+1]='\0';
        char p4[500], p5[500];
        sprintf(p4, "%s.halfVector", p3);
        sprintf(p5, "normalize(normalize(%s.position.xyz) + vec3(0., 0., 1.))", p3);
        Tmp = InplaceReplace(Tmp, &tmpsize, p4, p5);
        p = Tmp;
      } else
        ++p;
    }
  }*/
  // checking "#extension" keyword, and clean up some...
  /*{
    char* p = strstr(Tmp, "#extension");  // should test this is #first character in the line
    while(p) {
      char *p2 = NextStr(StrNext(Tmp, "#extension"));
      char *p3 = NextBlank(p2);
      char keyw[50];
      if(p3-p2<50) {
        strncpy(keyw, p2, p3-p2);
        // now, checking the keywords...
        if(strcmp(keyw, "GL_ARB_draw_instanced")==0) {
          // ok, this one is safe to ignore... Not even checking what state is asked
          p3 = NextLine(p);
          while (p!=p3) *(p++)=' '; // blank the line....
        }
      }
      // all done
      p = strstr(p+1, "#extension");
    }
  }*/ // done in preproc now
  if(isVertex) {
      // check for builtin OpenGL attributes...
      int n = sizeof(builtin_attrib)/sizeof(builtin_attrib_t);
      for (int i=0; i<n; i++) {
          if(strstr(Tmp, builtin_attrib[i].glname)) {
              // ok, this attribute is used
              // replace gl_name by _gl4es_ one
              Tmp = InplaceReplace(Tmp, &tmpsize, builtin_attrib[i].glname, builtin_attrib[i].name);
              // insert a declaration of it
              char def[100];
              sprintf(def, "attribute %s %s %s;\n", builtin_attrib[i].prec, builtin_attrib[i].type, builtin_attrib[i].name);
              Tmp = InplaceInsert(GetLine(Tmp, headline++), def, Tmp, &tmpsize);
          }
      }
  }
  // cleaning up the "centroid" keyword...
  if(strstr(Tmp, "centroid"))
  {
    char *p = Tmp;
    while((p=strstr(p, "centroid"))!=NULL)
    {
      if(p[8]==' ' || p[8]=='\t') { // what next...
        const char* p2 = GetNextStr(p+8);
        if(strcmp(p2, "uniform")==0 || strcmp(p2, "varying")==0) {
          memset(p, ' ', 8);  // erase the keyword...
        }
      } 
      p+=8;
    }
  }
  
  // check for builtin OpenGL gl_LightSource & friends
  if(strstr(Tmp, "gl_LightSourceParameters") || strstr(Tmp, "gl_LightSource"))
  {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_LightSourceParametersSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_LightSourceParametersSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_LightSourceParameters", "_gl4es_LightSourceParameters");
  }
  if(strstr(Tmp, "gl_LightModelParameters") || strstr(Tmp, "gl_LightModel"))
  {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_LightModelParametersSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_LightModelParametersSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_LightModelParameters", "_gl4es_LightModelParameters");
  }
  if(strstr(Tmp, "gl_LightModelProducts") || strstr(Tmp, "gl_FrontLightModelProduct") || strstr(Tmp, "gl_BackLightModelProduct"))
  {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_LightModelProductsSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_LightModelProductsSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_LightModelProducts", "_gl4es_LightModelProducts");
  }
  if(strstr(Tmp, "gl_LightProducts") || strstr(Tmp, "gl_FrontLightProduct") || strstr(Tmp, "gl_BackLightProduct"))
  {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_LightProductsSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_LightProductsSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_LightProducts", "_gl4es_LightProducts");
  }
  if(strstr(Tmp, "gl_MaterialParameters ") || (strstr(Tmp, "gl_FrontMaterial")) || strstr(Tmp, "gl_BackMaterial"))
  {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_MaterialParametersSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_MaterialParametersSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_MaterialParameters", "_gl4es_MaterialParameters");
  }
  if(strstr(Tmp, "gl_LightSource")) {
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_LightSource", "_gl4es_LightSource");
  }
  if(strstr(Tmp, "gl_LightModel"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_LightModel", "_gl4es_LightModel");
  if(strstr(Tmp, "gl_FrontLightModelProduct"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FrontLightModelProduct", "_gl4es_FrontLightModelProduct");
  if(strstr(Tmp, "gl_BackLightModelProduct"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_BackLightModelProduct", "_gl4es_BackLightModelProduct");
  if(strstr(Tmp, "gl_FrontLightProduct"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FrontLightProduct", "_gl4es_FrontLightProduct");
  if(strstr(Tmp, "gl_BackLightProduct"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_BackLightProduct", "_gl4es_BackLightProduct");
  if(strstr(Tmp, "gl_FrontMaterial"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FrontMaterial", "_gl4es_FrontMaterial");
  if(strstr(Tmp, "gl_BackMaterial"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_BackMaterial", "_gl4es_BackMaterial");
  if(strstr(Tmp, "gl_MaxLights"))
  {
    Tmp = InplaceInsert(GetLine(Tmp, 2), gl4es_MaxLightsSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_MaxLightsSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_MaxLights", "_gl4es_MaxLights");
  }
  if(strstr(Tmp, "gl_NormalScale")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_normalscaleSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_normalscaleSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_NormalScale", "_gl4es_NormalScale");
  }
  if(strstr(Tmp, "gl_InstanceID") || strstr(Tmp, "gl_InstanceIDARB")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_instanceID, Tmp, &tmpsize);
    headline+=CountLine(gl4es_instanceID);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_InstanceIDARB", "_gl4es_InstanceID");
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_InstanceID", "_gl4es_InstanceID");
  }
  if(strstr(Tmp, "gl_ClipPlane")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_clipplanesSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_clipplanesSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_ClipPlane", "_gl4es_ClipPlane");
  }
  if(strstr(Tmp, "gl_MaxClipPlanes")) {
    Tmp = InplaceInsert(GetLine(Tmp, 2), gl4es_MaxClipPlanesSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_MaxClipPlanesSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_MaxClipPlanes", "_gl4es_MaxClipPlanes");
  }

  if(strstr(Tmp, "gl_PointParameters") || strstr(Tmp, "gl_Point"))
    {
      Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_PointSpriteSource, Tmp, &tmpsize);
      headline+=CountLine(gl4es_PointSpriteSource);
      Tmp = InplaceReplace(Tmp, &tmpsize, "gl_PointParameters", "_gl4es_PointParameters");
    }
  if(strstr(Tmp, "gl_Point"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_Point", "_gl4es_Point");
  if(strstr(Tmp, "gl_FogParameters") || strstr(Tmp, "gl_Fog"))
    {
      Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_FogParametersSource, Tmp, &tmpsize);
      headline+=CountLine(gl4es_FogParametersSource);
      Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FogParameters", "_gl4es_FogParameters");
    }
  if(strstr(Tmp, "gl_Fog"))
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_Fog", "_gl4es_Fog");
  if(strstr(Tmp, "gl_TextureEnvColor")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texenvcolorSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_texenvcolorSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_TextureEnvColor", "_gl4es_TextureEnvColor");
  }
  if(strstr(Tmp, "gl_EyePlaneS")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgeneyeSource[0], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgeneyeSource[0]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_EyePlaneS", "_gl4es_EyePlaneS");
  }
  if(strstr(Tmp, "gl_EyePlaneT")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgeneyeSource[1], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgeneyeSource[1]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_EyePlaneT", "_gl4es_EyePlaneT");
  }
  if(strstr(Tmp, "gl_EyePlaneR")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgeneyeSource[2], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgeneyeSource[2]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_EyePlaneR", "_gl4es_EyePlaneR");
  }
  if(strstr(Tmp, "gl_EyePlaneQ")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgeneyeSource[3], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgeneyeSource[3]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_EyePlaneQ", "_gl4es_EyePlaneQ");
  }
  if(strstr(Tmp, "gl_ObjectPlaneS")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgenobjSource[0], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgenobjSource[0]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_ObjectPlaneS", "_gl4es_ObjectPlaneS");
  }
  if(strstr(Tmp, "gl_ObjectPlaneT")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgenobjSource[1], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgenobjSource[1]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_ObjectPlaneT", "_gl4es_ObjectPlaneT");
  }
  if(strstr(Tmp, "gl_ObjectPlaneR")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgenobjSource[2], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgenobjSource[2]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_ObjectPlaneR", "_gl4es_ObjectPlaneR");
  }
  if(strstr(Tmp, "gl_ObjectPlaneQ")) {
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_texgenobjSource[3], Tmp, &tmpsize);
    headline+=CountLine(gl4es_texgenobjSource[3]);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_ObjectPlaneQ", "_gl4es_ObjectPlaneQ");
  }
  // builtin varying
  int nvarying = 0;
  if(strstr(Tmp, "gl_Color") || need->need_color) {
    if(need->need_color<1) need->need_color = 1;
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_Color", (need->need_color==1)?"gl_FrontColor":"(gl_FrontFacing?gl_FrontColor:gl_BackColor)");
  }
  if(strstr(Tmp, "gl_FrontColor") || need->need_color) {
    if(need->need_color<1) need->need_color = 1;
    nvarying+=1;
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_frontColorSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_frontColorSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FrontColor", "_gl4es_FrontColor");
  }
  if(strstr(Tmp, "gl_BackColor") || (need->need_color==2)) {
    need->need_color = 2;
    nvarying+=1;
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_backColorSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_backColorSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_BackColor", "_gl4es_BackColor");
  }
  if(strstr(Tmp, "gl_SecondaryColor") || need->need_secondary) {
    if(need->need_secondary<1) need->need_secondary = 1;
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_SecondaryColor", (need->need_secondary==1)?"gl_FrontSecondaryColor":"(gl_FrontFacing?gl_FrontSecondaryColor:gl_BackSecondaryColor)");
  }
  if(strstr(Tmp, "gl_FrontSecondaryColor") || need->need_secondary) {
    if(need->need_secondary<1) need->need_secondary = 1;
    nvarying+=1;
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_frontSecondaryColorSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_frontSecondaryColorSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FrontSecondaryColor", "_gl4es_FrontSecondaryColor");
  }
  if(strstr(Tmp, "gl_BackSecondaryColor") || (need->need_secondary==2)) {
    need->need_secondary = 2;
    nvarying+=1;
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_backSecondaryColorSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_backSecondaryColorSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_BackSecondaryColor", "_gl4es_BackSecondaryColor");
  }
  if(strstr(Tmp, "gl_FogFragCoord") || need->need_fogcoord) {
    need->need_fogcoord = 1;
    nvarying+=1;
    Tmp = InplaceInsert(GetLine(Tmp, headline), gl4es_fogcoordSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_fogcoordSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_FogFragCoord", "_gl4es_FogFragCoord");
  }
  if(strstr(Tmp, "gl_TexCoord") || need->need_texcoord!=-1) {
    int ntex = need->need_texcoord;
    // Try to determine max gl_TexCoord used
    char* p = Tmp;
    int notexarray_ok = 1;
    while((p=strstr(p, gl_TexCoordSource))) {
      p+=strlen(gl_TexCoordSource);
      if(*p>='0' && *p<='9') {
        int n = (*p) - '0';
        if(p[1]>='0' && p[1]<='9')
          n = n*10 + (p[1] - '0');
        if (ntex<n) ntex = n;
      } else 
        notexarray_ok=0;
    }
    // if failed to determine, take max...
    if (ntex==-1) ntex = hardext.maxtex;
    // check constraint, and switch to notexarray if needed
    if (!notexarray && ntex+nvarying>hardext.maxvarying && !need->need_clean && notexarray_ok) {
      notexarray = 1;
      need->need_notexarray = 1;
    }
    // prefer notexarray...
    if(!isVertex && notexarray_ok && !need->need_clean) {
      notexarray = 1;
      need->need_notexarray = 1;
    }
    // check constaints
    if (!notexarray && ntex+nvarying>hardext.maxvarying) ntex = hardext.maxvarying - nvarying;
    need->need_texcoord = ntex;
    char d[100];
    if(notexarray) {
      for (int k=0; k<ntex+1; k++) {
        char d2[100];
        sprintf(d2, "gl_TexCoord[%d]", k);
        if(strstr(Tmp, d2)) {
          sprintf(d, gl4es_texcoordSourceAlt, k);
          Tmp = InplaceInsert(GetLine(Tmp, headline), d, Tmp, &tmpsize);
          headline+=CountLine(d);
          sprintf(d, "_gl4es_TexCoord_%d", k);
          Tmp = InplaceReplace(Tmp, &tmpsize, d2, d);
        }
      }
    } else {
      sprintf(d, gl4es_texcoordSource, ntex+1);
      Tmp = InplaceInsert(GetLine(Tmp, headline), d, Tmp, &tmpsize);
      headline+=CountLine(d);
      Tmp = InplaceReplace(Tmp, &tmpsize, "gl_TexCoord", "_gl4es_TexCoord");
    }
  }
  if(strstr(Tmp, "gl_MaxTextureUnits")) {
    Tmp = InplaceInsert(GetLine(Tmp, 2), gl4es_MaxTextureUnitsSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_MaxTextureUnitsSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_MaxTextureUnits", "_gl4es_MaxTextureUnits");
  }
  if(strstr(Tmp, "gl_MaxTextureCoords")) {
    Tmp = InplaceInsert(GetLine(Tmp, 2), gl4es_MaxTextureCoordsSource, Tmp, &tmpsize);
    headline+=CountLine(gl4es_MaxTextureCoordsSource);
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_MaxTextureCoords", "_gl4es_MaxTextureCoords");
  }
  if(strstr(Tmp, "gl_ClipVertex")) {
    // gl_ClipVertex is not handled for now
    // Proper way would be to copy handling from fpe_shader, but then, need to use gl_ClipPlane...
    static int ncv = 0;
    char CV[60];
    sprintf(CV, gl4es_dummyClipVertex, ncv);
    ++ncv;
    Tmp = InplaceReplace(Tmp, &tmpsize, "gl_ClipVertex", CV);
  }
  if(strstr(Tmp, "mat2x2")) {
    // better to use #define ?
    Tmp = InplaceReplace(Tmp, &tmpsize, "mat2x2", "mat2");
  }
  if(strstr(Tmp, "mat3x3")) {
    // better to use #define ?
    Tmp = InplaceReplace(Tmp, &tmpsize, "mat3x3", "mat3");
  }
  
  // finish
  if((globals4es.dbgshaderconv&maskafter)==maskafter) {
    printf("New Shader source:\n%s\n", Tmp);
  }
  // clean preproc'd source
  if(pEntry!=pBuffer)
    free(pBuffer);
  return Tmp;
}

int isBuiltinAttrib(const char* name) {
    int n = sizeof(builtin_attrib)/sizeof(builtin_attrib_t);
    for (int i=0; i<n; i++) {
        if (strcmp(builtin_attrib[i].name, name)==0)
            return builtin_attrib[i].attrib;
    }
    return -1;
}

int isBuiltinMatrix(const char* name) {
    int ret = -1;
    int n = sizeof(builtin_matrix)/sizeof(builtin_matrix_t);
    for (int i=0; i<n && ret==-1; i++) {
        if (strncmp(builtin_matrix[i].name, name, strlen(builtin_matrix[i].name))==0) {
            int l=strlen(builtin_matrix[i].name);
            if(strlen(name)==l || (strlen(name)==l+3 && name[l]=='[' && builtin_matrix[i].texarray)) {
                ret=builtin_matrix[i].matrix;
                if(builtin_matrix[i].texarray) {
                    int n = name[l+1] - '0';
                    ret+=n*4;
                }
            }
        }
    }
    return ret;
}

const char* hasBuiltinAttrib(const char* vertexShader, int Att) {
    // first search for the string
    const char* ret = NULL;
    int n = sizeof(builtin_attrib)/sizeof(builtin_attrib_t);
    for (int i=0; i<n && !ret; i++) {
        if (builtin_attrib[i].attrib == Att)
            ret = builtin_attrib[i].name;
    }
    if (!ret)
      return NULL;
    if(strstr(vertexShader, ret)) // it's here!
      return ret;
    return NULL;  // nope
}