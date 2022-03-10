#pragma once
#include <glad/glad.h> 	

#include <string>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cstring>

#include "Interface/IGraphicManager.h"
#include "Type.h"
#include "Image.h"
#include "Handle.h"
#include "Utils/Logging.h"

#if defined(DEBUG_OPENGL)
#define CHECK_GL_ERROR_IF_DEBUG() CheckGlError(__SHORT_FILE__, __FUNCTION__, __LINE__);
#else
#define CHECK_GL_ERROR_IF_DEBUG()
#endif

namespace wallpaper
{

class SceneMesh;
class ShaderValue;

namespace gl
{

constexpr uint16_t TexSlotMaxNum {512};

inline char const* const GLErrorToStr(GLenum const err) noexcept {
#define Enum_GLError(glerr) case glerr: return #glerr;
  switch (err) {
    // opengl 2
	Enum_GLError(GL_NO_ERROR);
	Enum_GLError(GL_INVALID_ENUM);
	Enum_GLError(GL_INVALID_VALUE);
	Enum_GLError(GL_INVALID_OPERATION);
	Enum_GLError(GL_OUT_OF_MEMORY);
    // opengl 3 errors (1)
	Enum_GLError(GL_INVALID_FRAMEBUFFER_OPERATION);
    default:
      return "Unknown GLError";
  }
#undef Enum_GLError
}


inline void CheckGlError(const char* file, const char* func, int line) {
	int err = glGetError();
	if(err != 0)
		WallpaperLog(LOGLEVEL_ERROR, file, line, "%s(%d) at %s", GLErrorToStr(err), err, func);
}


GLuint ToGLType(ShaderType);
GLenum ToGLType(TextureType);
GLenum ToGLType(TextureWrap);
GLenum ToGLType(TextureFilter);
GLenum ToGLType(MeshPrimitive);


template <typename Getiv, typename GetLog>
std::string GetInfoLog(GLuint name, Getiv getiv, GetLog getLog) {
	GLint bufLength = 0;
	getiv(name, GL_INFO_LOG_LENGTH, &bufLength);
	if (bufLength <= 0)
		bufLength = 2048;

	std::string infoLog;
	infoLog.resize(bufLength);
	GLsizei len = 0;
	getLog(name, (GLsizei)infoLog.size(), &len, &infoLog[0]);
	if (len <= 0)
		return "(unknown reason)";

	infoLog.resize(len);
	return infoLog;
}

inline void SetUniformF(GLint loc, int count, const float *udata) {
	if (loc >= 0) {
		switch (count) {
		case 1:
			glUniform1f(loc, udata[0]);
			break;
		case 2:
			glUniform2fv(loc, 1, udata);
			break;
		case 3:
			glUniform3fv(loc, 1, udata);
			break;
		case 4:
			glUniform4fv(loc, 1, udata);
			break;
		}
		CHECK_GL_ERROR_IF_DEBUG();
	}
}

inline void SetUniformI(GLint loc, int count, const int *udata) {
	if (loc >= 0) {
		switch (count) {
		case 1:
			glUniform1iv(loc, 1, (GLint *)udata);
			break;
		case 2:
			glUniform2iv(loc, 1, (GLint *)udata);
			break;
		case 3:
			glUniform3iv(loc, 1, (GLint *)udata);
			break;
		case 4:
			glUniform4iv(loc, 1, (GLint *)udata);
			break;
		}
		CHECK_GL_ERROR_IF_DEBUG();
	}
}

inline void SetUniformMat4(GLint loc, const float *udata) {
	if (loc >= 0) {
		glUniformMatrix4fv(loc, 1, false, udata);
		CHECK_GL_ERROR_IF_DEBUG();
	}
}
inline void SetUniformMat3(GLint loc, const float *udata) {
	if (loc >= 0) {
		glUniformMatrix3fv(loc, 1, false, udata);
		CHECK_GL_ERROR_IF_DEBUG();
	}
}

/*
typedef uint32_t GLenum;
typedef unsigned char GLboolean;
typedef uint32_t GLbitfield;
typedef void GLvoid;
typedef int32_t GLint;
typedef uint32_t GLuint;
typedef int32_t GLsizei;
typedef float GLfloat;
typedef double GLdouble;
typedef double GLclampd;
typedef char GLchar;
typedef char GLcharARB;
*/

constexpr uint16_t MaxTexBinding {16};

struct ViewPort {
	uint16_t x {0};
	uint16_t y {0};
	uint16_t width {1};
	uint16_t height {1};
};

struct GContext {
	GLuint defaultFb {0};
	GLuint curFb {0};
	GLuint backCurFb {0};

	ViewPort viewPort;
};

struct GBindings {
	std::array<HwTexHandle, MaxTexBinding> texs;
	std::uint8_t texNum {1};
};

struct GPass {
	HwRenderTargetHandle target;
	ViewPort viewport;
	HwShaderHandle shader;
	BlendMode blend {BlendMode::Normal};
	std::array<bool, 4> colorMask {true, true, true, true};
};

struct GTexture {
	struct Desc {
		uint16_t w {2};
		uint16_t h {2};
    	uint16_t numMips {0};
		uint16_t numSlots {1};
		uint16_t activeSlot {0};
		GLenum target {0xFFFF};
		TextureFormat format {TextureFormat::RGBA8};
		TextureSample sample;
	};
	std::array<GLuint, TexSlotMaxNum> gltexs;
	Desc desc;
	static void Init(GTexture& t, const Desc& d) {
		t.desc = d;
		if(t.desc.numSlots > TexSlotMaxNum) {
			LOG_ERROR("texture solts num overflow: %d", t.desc.numSlots);
			t.desc.numSlots = TexSlotMaxNum;
		}
		for(uint16_t i=0;i<t.desc.numSlots;i++) {
			auto& gltex = t.gltexs[i];
			glGenTextures(1, &gltex);
			glBindTexture(t.desc.target, gltex);
			glTexParameteri(t.desc.target, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(t.desc.target, GL_TEXTURE_MAX_LEVEL, t.desc.numMips);
			CHECK_GL_ERROR_IF_DEBUG();
			glBindTexture(t.desc.target, 0);
		}
	}
	static void Destroy(GTexture& t) {
		for(uint16_t i=0;i<t.desc.numSlots;i++) {
			glDeleteTextures(1, &t.gltexs[i]);
			t.gltexs[i] = 0;
		}
		CHECK_GL_ERROR_IF_DEBUG();
	}
};

struct GFrameBuffer {
	struct Desc {
		uint16_t width {1};
		uint16_t height {1};
		std::array<HwTexHandle, MaxAttachmentNum> attachs;
	};
	GLuint glfb {0};
	uint16_t width {1};
	uint16_t height {1};
	static void Init(GFrameBuffer& f, const Desc& d) { 
		f.width = d.width;
		f.height = d.width;
		glGenFramebuffers(1, &f.glfb);
	}
	static void Destroy(GFrameBuffer& f) {
		glDeleteFramebuffers(1, &f.glfb);
		CHECK_GL_ERROR_IF_DEBUG();
	}
};


struct GShader {
	struct Desc {
		std::string vs;
		std::string fg;
		struct AttribLoc {
			uint32_t location;
			std::string name;
		};
		std::vector<AttribLoc> attrs;
		std::vector<std::string> texnames;
	};
	struct UniformLoc {
		std::string name;
		int32_t location = -1;
		uint32_t type;
		int32_t count;
	};
	GLuint glpro;
	std::vector<UniformLoc> uniformLocs;

	static int GetUnifLoc(const GShader& s, std::string_view name) {
		for(const auto& u:s.uniformLocs) {
			if(name == u.name)
				return u.location;
		}
		return -1;
	}
	static GLuint Compile(GLenum stage, const std::string& source) {
		GLuint shader = glCreateShader(stage);
		const char* source_char = source.c_str();	
		glShaderSource(shader, 1, &source_char, nullptr);
		glCompileShader(shader);
		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if(!success)
		{
			std::string infolog = GetInfoLog(shader, glGetShaderiv, glGetShaderInfoLog);
			LOG_ERROR("COMPILATION_FAILED\n %s", infolog.c_str());
			LOG_INFO(source.c_str());
		}
		CHECK_GL_ERROR_IF_DEBUG();
		return shader;
	}
	static void QueryProUniforms(GShader& s) {
		int n_uniform = 0;
		glGetProgramiv(s.glpro, GL_ACTIVE_UNIFORMS, &n_uniform);
		CHECK_GL_ERROR_IF_DEBUG();
		auto& uniforms = s.uniformLocs;
		uniforms.resize(n_uniform);
		for(int i=0; i<n_uniform ;i++) {
			char* name = new char[256];
			glGetActiveUniform(s.glpro, i, 256, nullptr, &uniforms[i].count, &uniforms[i].type, name);
			uniforms[i].name = std::string(name);
			//LOG_INFO("------------" + uniforms[i].name);
			delete [] name;
			uniforms[i].location = glGetUniformLocation(s.glpro, uniforms[i].name.c_str());
		}
		CHECK_GL_ERROR_IF_DEBUG();
	}
	static void Init(GShader& s, const Desc& desc) {
		s.glpro = glCreateProgram();

		GLuint glvs,glfg;
		glvs = Compile(GL_VERTEX_SHADER, desc.vs);
		glfg = Compile(GL_FRAGMENT_SHADER, desc.fg);
		glAttachShader(s.glpro, glvs);
		glAttachShader(s.glpro, glfg);
		CHECK_GL_ERROR_IF_DEBUG();
		for(auto& attr:desc.attrs) {
			glBindAttribLocation(s.glpro, attr.location, attr.name.c_str());
		}
		CHECK_GL_ERROR_IF_DEBUG();

		glLinkProgram(s.glpro);
		glUseProgram(s.glpro);
		int success;
		glGetProgramiv(s.glpro, GL_LINK_STATUS, &success);
		if(!success) {
			std::string infoLog = GetInfoLog(s.glpro, glGetProgramiv, glGetProgramInfoLog);
			LOG_ERROR("LINKING_FAILED\n %s", infoLog.c_str());
		}

		QueryProUniforms(s);
		{
			for(int i=0;i<desc.texnames.size();i++) {
				int32_t loc = GetUnifLoc(s, desc.texnames[i]);
				if(loc == -1) continue;
				int slot = i;
				SetUniformI(loc, 1, &slot);
			}
		}

		glUseProgram(0);
		glDeleteShader(glvs);
		glDeleteShader(glfg);
		CHECK_GL_ERROR_IF_DEBUG();
	}
	static void Destroy(GShader& s) {
		glDeleteProgram(s.glpro);
		s.glpro = 0;
	}
};

struct GLUniform {
	char name[256];
	GLenum type;
	int location = -1;
	int count;
};

inline void TextureFormat2GLFormat(TextureFormat texformat, GLint& internalFormat, GLenum& format, GLenum& type) {
	type = GL_UNSIGNED_BYTE;	
	format = GL_RGBA;
	switch(texformat) {
	case TextureFormat::R8:
		internalFormat = GL_R8;
		format = GL_RED;
		break;
	case TextureFormat::RG8:
		internalFormat = GL_RG8;
		format = GL_RG;
		break;
	case TextureFormat::RGB8:
		internalFormat = GL_RGB8;	
		format = GL_RGB;
		break;
	case TextureFormat::RGBA8:
		internalFormat = GL_RGBA8;
		break;
	case TextureFormat::BC1:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case TextureFormat::BC2:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case TextureFormat::BC3:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	}
}


class GLWrapper : NoCopy,NoMove {
public:
	GLWrapper();
	~GLWrapper() {
		ClearAll();
		GFrameBuffer::Destroy(m_clearFb);
	}

	bool Init(void *get_proc_address(const char*));

	void gBindFramebuffer(GLenum target, GLuint fb) {
		glBindFramebuffer(target, fb);
		m_context.backCurFb = m_context.curFb;
		m_context.curFb = fb;
	}

	HwTexHandle CreateTexture(const GTexture::Desc& desc, const Image* image = nullptr) {
		HwTexHandle texh = m_texPool.Alloc(desc);
		auto* tex = m_texPool.Lookup(texh);
		if(image != nullptr) {
			auto slotsNum = tex->desc.numSlots;
			for(uint16_t i=0;i<slotsNum;i++) {
				auto target = tex->desc.target;
				glBindTexture(target, tex->gltexs[i]);
				CHECK_GL_ERROR_IF_DEBUG();

				auto& img = image->slots[i];
				for(uint16_t imip=0;imip<img.size();imip++) {
					auto& mip = img[imip];
					GLenum format, type;	
					GLint internalFormat;
					auto texformat = tex->desc.format;
					TextureFormat2GLFormat(texformat, internalFormat, format, type);
					GLuint pbo;
					std::size_t bufferSize = mip.size;
					if(texformat == TextureFormat::R8 || texformat == TextureFormat::RG8) bufferSize *= 2;
					glGenBuffers(1, &pbo);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
					glBufferData(GL_PIXEL_UNPACK_BUFFER, bufferSize, 0, GL_STATIC_DRAW);
					CHECK_GL_ERROR_IF_DEBUG();
					void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
					CHECK_GL_ERROR_IF_DEBUG();
					if(ptr == nullptr) {
						LOG_ERROR("Can't map pbo buffer");
						continue;
					}
					memcpy(ptr, mip.data.get(), mip.size);
					glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
					CHECK_GL_ERROR_IF_DEBUG();

					switch(texformat) {
					case TextureFormat::R8:
					case TextureFormat::RG8:
					case TextureFormat::RGB8:
					case TextureFormat::RGBA8:
						glTexImage2D(tex->desc.target, imip, internalFormat, mip.width, mip.height, 0, format, type, NULL);
						break;
					case TextureFormat::BC1:
					case TextureFormat::BC2:
					case TextureFormat::BC3:
						glCompressedTexImage2D(tex->desc.target, imip, internalFormat, mip.width, mip.height, 0, mip.size, NULL);
						break;
					}
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
					glDeleteBuffers(1, &pbo);
					CHECK_GL_ERROR_IF_DEBUG();
				}
				glTexParameteri(target, GL_TEXTURE_WRAP_S, ToGLType(tex->desc.sample.wrapS));
				glTexParameteri(target, GL_TEXTURE_WRAP_T, ToGLType(tex->desc.sample.wrapT));
				glTexParameteri(target, GL_TEXTURE_MAG_FILTER, ToGLType(tex->desc.sample.magFilter));
				glTexParameteri(target, GL_TEXTURE_MIN_FILTER, ToGLType(tex->desc.sample.minFilter));
				glBindTexture(target, 0);
			}
		} else {
			tex->desc.numSlots = 1;
			glBindTexture(tex->desc.target, tex->gltexs[0]);
			GLenum format, type;	
			GLint internalFormat;
			auto texformat = tex->desc.format;
			TextureFormat2GLFormat(texformat, internalFormat, format, type);
			auto target = tex->desc.target;
			glTexImage2D(target, 0, internalFormat, tex->desc.w, tex->desc.h, 0, format, type, 0);
    		tex->desc.sample = {TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE,
                                TextureFilter::LINEAR, TextureFilter::LINEAR};
			glTexParameteri(target, GL_TEXTURE_WRAP_S, ToGLType(tex->desc.sample.wrapS));
			glTexParameteri(target, GL_TEXTURE_WRAP_T, ToGLType(tex->desc.sample.wrapT));
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, ToGLType(tex->desc.sample.magFilter));
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, ToGLType(tex->desc.sample.minFilter));
			glBindTexture(target, 0);
		}
		return texh;
	}
	void DestroyTexture(HwTexHandle h) {
		m_texPool.Free(h);
	}
	void UpdateTextureSlot(HwTexHandle h, uint16_t index) {
		auto* img = m_texPool.Lookup(h);
		if(img != nullptr) {
			img->desc.activeSlot = index;
		}
	}
	void CopyTexture(HwTexHandle dst, HwTexHandle src) {
		auto* srcTex = m_texPool.Lookup(src);
		if(srcTex == nullptr) return;
		auto* dstTex = m_texPool.Lookup(dst);
		if(dstTex == nullptr) return;
		GLuint fbo {0};
		auto target = srcTex->desc.target;
		glGenFramebuffers(1, &fbo);
		gBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
			target, srcTex->gltexs[srcTex->desc.activeSlot], 0);
		CHECK_GL_ERROR_IF_DEBUG();
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindTexture(target, dstTex->gltexs[dstTex->desc.activeSlot]);
		CHECK_GL_ERROR_IF_DEBUG();
		GLenum format, type;	
		GLint internalFormat;
		auto texformat = srcTex->desc.format;
		TextureFormat2GLFormat(texformat, internalFormat, format, type);
		glCopyTexSubImage2D(target, 0, 0, 0, 0, 0, srcTex->desc.w, srcTex->desc.h);
		CHECK_GL_ERROR_IF_DEBUG();
		glBindTexture(target, 0);
		glDeleteFramebuffers(1, &fbo);
		m_context.curFb = 0;
		CHECK_GL_ERROR_IF_DEBUG();

		//gBindFramebuffer(GL_FRAMEBUFFER, m_context.backCurFb);
	}

	void ClearTexture(HwTexHandle thandle, std::array<float, 4> clearcolors) {
		gBindFramebuffer(GL_FRAMEBUFFER, m_clearFb.glfb);

		auto* tex = m_texPool.Lookup(thandle);
		assert(tex != nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				tex->desc.target, tex->gltexs[tex->desc.activeSlot], 0);
		glColorMask(true, true, true, true);
		glClearColor(clearcolors[0],clearcolors[1],clearcolors[2],clearcolors[3]);
		glClear(GL_COLOR_BUFFER_BIT);

		//gBindFramebuffer(GL_FRAMEBUFFER, m_context.backCurFb);
	}

	void ApplyBindings(const GBindings& binds) {
		for(uint16_t i=0;i<binds.texNum;i++) {
			auto* tex = m_texPool.Lookup(binds.texs[i]);
			if(tex != nullptr) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(tex->desc.target, tex->gltexs[tex->desc.activeSlot]);
			}
		}
		CHECK_GL_ERROR_IF_DEBUG();
	}

	HwShaderHandle CreateShader(const GShader::Desc& desc) {
		return m_shaderPool.Alloc(desc);
	}
	void DestroyShader(HwShaderHandle h) {
		m_shaderPool.Free(h);
	}

	void BeginPass(GPass& pass) {
		auto* shader = m_shaderPool.Lookup(pass.shader);
		if(shader != nullptr) glUseProgram(shader->glpro);
		CHECK_GL_ERROR_IF_DEBUG();
		{
			auto* fb = m_fbPool.Lookup(pass.target);
			if(fb != nullptr) {
				gBindFramebuffer(GL_FRAMEBUFFER, fb->glfb);
				auto& v = pass.viewport;
				glViewport(v.x, v.y, v.width, v.height);
			} else {
				gBindFramebuffer(GL_FRAMEBUFFER, m_context.defaultFb);
				auto& v = m_context.viewPort;
				glViewport(v.x, v.y, v.width, v.height);
			}
		}

		SetBlend(pass.blend);
		{
			auto& c = pass.colorMask;
			glColorMask(c[0], c[1], c[2], c[3]);
		}
		glDisable(GL_DEPTH_TEST);
		
	}
	void EndPass(GPass& pass) {
		glUseProgram(0);
	}

	HwRenderTargetHandle CreateRenderTarget(const GFrameBuffer::Desc& desc) {
		auto rtHandle = m_fbPool.Alloc(desc);
		auto* fb = m_fbPool.Lookup(rtHandle);
		gBindFramebuffer(GL_FRAMEBUFFER, fb->glfb);
		CHECK_GL_ERROR_IF_DEBUG();

		for(uint8_t i=0;i<desc.attachs.size();i++) {
			auto* tex = m_texPool.Lookup(desc.attachs[i]);
			if(i == 0 && tex == nullptr)
				assert(false);
			if(tex == nullptr) break;
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, 
				tex->desc.target, tex->gltexs[tex->desc.activeSlot], 0);
		}
		CHECK_GL_ERROR_IF_DEBUG();

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			LOG_ERROR("GL_FRAMEBUFFER_UNSUPPORTED");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			LOG_ERROR("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
			break;
		default:
			LOG_ERROR("framebuffer not complite %d", status);
			break;
		}
		//glColorMask(true, true, true, true);
		//glClearColor(0,0,0,1.0f);
		//glClear(GL_COLOR_BUFFER_BIT);
		gBindFramebuffer(GL_FRAMEBUFFER, m_context.backCurFb);
		CHECK_GL_ERROR_IF_DEBUG();
		return rtHandle;
	}
	void UpdateRenderTarget(HwRenderTargetHandle h, const GFrameBuffer::Desc& desc) {
		auto *fb = m_fbPool.Lookup(h);
		if(fb == nullptr) return;
		gBindFramebuffer(GL_FRAMEBUFFER, fb->glfb);
		for(uint8_t i=0;i<desc.attachs.size();i++) {
			auto* tex = m_texPool.Lookup(desc.attachs[i]);
			if(i == 0 && tex == nullptr)
				assert(false);
			if(tex == nullptr) break;
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, 
				tex->desc.target, tex->gltexs[tex->desc.activeSlot], 0);
		}
		CHECK_GL_ERROR_IF_DEBUG();

		//gBindFramebuffer(GL_FRAMEBUFFER, m_context.backCurFb);
	}
	void DestroyRenderTarget(HwRenderTargetHandle h) {
		auto * fb = m_fbPool.Lookup(h);
		if(fb->glfb == m_context.defaultFb) {
			gBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		m_fbPool.Free(h);
	}

	//void CopyTexture(GLFramebuffer* src, GLTexture* dst);
	//GLTexture* CopyTexture(GLFramebuffer* fbo);
	void ClearColor(float r, float g, float b, float a);
	void Viewport(int32_t ,int32_t ,int32_t ,int32_t);

	/*
	void TextureImage(GLTexture *texture, int level, int width, int height, TextureFormat texformat, uint8_t *data, std::size_t imgsize=0);
	void TextureImagePbo(GLTexture *texture, int level, int width, int height, TextureFormat texformat, uint8_t *data, std::size_t imgsize);
	*/

	void SetBlend(BlendMode);
	std::unordered_set<std::string> GetUniforms(HwShaderHandle h) {
		auto* shader = m_shaderPool.Lookup(h);
		std::unordered_set<std::string> result;
		for(auto& el:shader->uniformLocs) {
			if(!el.name.empty())
				result.insert(el.name);
		}
		return result;
	}

	void UpdateUniform(HwShaderHandle h, const ShaderValue& sv);

	void UseShader(HwShaderHandle h, const std::function<void()>& func) {
		auto* shader = m_shaderPool.Lookup(h);
		if(shader == nullptr)  
			glUseProgram(0);
		else 
			glUseProgram(shader->glpro);
		func();
		glUseProgram(0);
	}

	void SetUniform(HwShaderHandle h, GLUniform* uniform, const void* value) {
		auto* shader = m_shaderPool.Lookup(h);
		if(shader == nullptr) return;
		GLint loc = uniform->location;
		if(loc == -1){
			loc = glGetUniformLocation(shader->glpro, uniform->name);
			uniform->location = loc;
		}

		int count = uniformCount_[uniform->type];

		switch(uniform->type)
		{   
		case GL_FLOAT:
		case GL_FLOAT_VEC2:
		case GL_FLOAT_VEC3:
		case GL_FLOAT_VEC4:
			SetUniformF(loc, count, static_cast<const float*>(value));
			break;
		case GL_INT:
		case GL_SAMPLER_2D:
			SetUniformI(loc, count, static_cast<const int*>(value));
			break;
		case GL_FLOAT_MAT4:
			SetUniformMat4(loc, static_cast<const float*>(value));
			break;
		}   
	}

	void SetDefaultFrameBuffer(uint hw, uint16_t width, uint16_t height) {
		m_context.defaultFb = hw;
		m_context.viewPort.width = width;
		m_context.viewPort.height = height;
	}

	// scene
	void LoadMesh(SceneMesh&);
	void RenderMesh(const SceneMesh&);
	bool MeshLoaded(const SceneMesh& m) const;
	void CleanMeshBuf();

	void ClearAll() {
		CleanMeshBuf();
		m_texPool.FreeAll();
		m_shaderPool.FreeAll();
		m_fbPool.FreeAll();
	}

private:
	GContext m_context;	

	// for tex clear;
	GFrameBuffer m_clearFb;

	std::unordered_map<int, int> uniformCount_;	

	std::unordered_map<uint32_t, uint32_t> m_bufMap;
	std::unordered_map<uint32_t, uint32_t> m_vaoMap;
	uint32_t m_bufidgen {0};
	uint32_t m_vaoidgen {0};

	HandlePool<GTexture, HwTexHandle> m_texPool { HandleMaxNum };
	HandlePool<GShader, HwShaderHandle> m_shaderPool { HandleMaxNum };
	HandlePool<GFrameBuffer, HwRenderTargetHandle> m_fbPool { HandleMaxNum };
};
}
}
