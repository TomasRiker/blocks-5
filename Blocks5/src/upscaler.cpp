#include "pch.h"
#include "upscaler.h"
#include "glextensions.h"

/* The vertex shader of every present filter. The vertices arrive from
   drawQuad() ready in clip coordinates; there is nothing to transform.

   No #version: 110 on the desktop, 100 on the embedded shading language, and
   the source compiles as both. */
static const char* p_presentVertexShader =
	"#ifdef GL_ES\n"
	"precision highp float;\n"
	"#endif\n"
	"attribute vec2 aPosition;\n"
	"attribute vec2 aTexCoord;\n"
	"varying vec2 texCoord;\n"
	"void main()\n"
	"{\n"
	"    texCoord = aTexCoord;\n"
	"    gl_Position = vec4(aPosition, 0.0, 1.0);\n"
	"}\n";

/* The fragment shader of the two filters that bring none: the texel as the
   texture filter delivers it, opaque. Sharp and Smooth are that filter and
   nothing else, and it is set on the texture before present() runs. The
   precision preamble is the one every present shader uses: highp where the
   browser has it, since a texel of a 1024-wide texture is 1/1024 and a
   mediump coordinate has ten bits. */
static const char* p_plainFragmentShader =
	"#ifdef GL_ES\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"#endif\n"
	"uniform sampler2D decal;\n"
	"varying vec2 texCoord;\n"
	"void main()\n"
	"{\n"
	"    gl_FragColor = vec4(texture2D(decal, texCoord).rgb, 1.0);\n"
	"}\n";

namespace
{
	// Compiles one stage and prints the log on an error.
	uint compileShaderStage(GLenum type, const char* p_source, const char* p_what)
	{
		const uint shader = glExtCreateShader(type);
		if(!shader) return 0;

		glExtShaderSource(shader, 1, &p_source, 0);
		glExtCompileShader(shader);

		GLint ok = 0;
		glExtGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if(!ok)
		{
			char log[1024] = "";
			glExtGetShaderInfoLog(shader, sizeof(log) - 1, 0, log);
			printfLog("- WARNING: Could not compile the %s shader: %s\n", p_what, log);
			glExtDeleteShader(shader);
			return 0;
		}
		return shader;
	}
}

PresentProgram::PresentProgram()
{
	id = 0;
	decal = -1;
	textureSize = -1;
	frameSize = -1;
	prescale = -1;
}

bool PresentProgram::create(const char* p_fragmentSource,
							const char* p_name)
{
	destroy();

	const uint vs = compileShaderStage(GL_VERTEX_SHADER, p_presentVertexShader, "present vertex");
	if(!vs) return false;
	const uint fs = compileShaderStage(GL_FRAGMENT_SHADER, p_fragmentSource, p_name);
	if(!fs) { glExtDeleteShader(vs); return false; }

	id = glExtCreateProgram();
	glExtAttachShader(id, vs);
	glExtAttachShader(id, fs);
	glExtBindAttribLocation(id, 0, "aPosition");
	glExtBindAttribLocation(id, 1, "aTexCoord");
	glExtLinkProgram(id);

	glExtDeleteShader(vs);
	glExtDeleteShader(fs);

	GLint ok = 0;
	glExtGetProgramiv(id, GL_LINK_STATUS, &ok);
	if(!ok)
	{
		char log[1024] = "";
		glExtGetProgramInfoLog(id, sizeof(log) - 1, 0, log);
		printfLog("- WARNING: Could not link the %s program: %s\n", p_name, log);
		destroy();
		return false;
	}

	decal       = glExtGetUniformLocation(id, "decal");
	textureSize = glExtGetUniformLocation(id, "TextureSize");
	frameSize   = glExtGetUniformLocation(id, "FrameSize");
	prescale    = glExtGetUniformLocation(id, "Prescale");
	return true;
}

void PresentProgram::destroy()
{
	// The program only. The uniform locations are valid just as long as there
	// is one anyway - resetting them as well means keeping a second list, and
	// a list can be forgotten.
	if(id) { glExtDeleteProgram(id); id = 0; }
}

void PresentProgram::setUniform(int location,
								float value)
{
	if(location >= 0) glExtUniform1f(location, value);
}

void PresentProgram::use(const PresentContext& context) const
{
	glExtUseProgram(id);

	if(decal >= 0) glExtUniform1i(decal, 0);
	if(textureSize >= 0)
	{
		glExtUniform2f(textureSize, static_cast<float>(context.textureSize.x),
									static_cast<float>(context.textureSize.y));
	}
	if(frameSize >= 0)
	{
		glExtUniform2f(frameSize, static_cast<float>(context.frameSize.x),
								  static_cast<float>(context.frameSize.y));
	}
	if(prescale >= 0)
	{
		// The smallest integer factor with which the 640x480 frame fills the
		// destination rectangle. That is exactly what one would nearest-upscale
		// by before going back down - the shader does both.
		const float x = static_cast<float>(max(1, static_cast<int>(
			ceilf(static_cast<float>(context.rectSize.x) / context.frameSize.x))));
		const float y = static_cast<float>(max(1, static_cast<int>(
			ceilf(static_cast<float>(context.rectSize.y) / context.frameSize.y))));
		glExtUniform2f(prescale, x, y);
	}
}

void PresentProgram::drawQuad(const PresentContext& context) const
{
	// The shader works in clip coordinates, so the rect and the window size
	// are all it takes: no matrix anywhere.
	const int x = context.rectPosition.x;
	const int y = context.rectPosition.y;
	const int w = context.rectSize.x;
	const int h = context.rectSize.y;

	const Vec2f display = static_cast<Vec2f>(context.displaySize);
	const float x0 = 2.0f * static_cast<float>(x)     / display.x - 1.0f;
	const float x1 = 2.0f * static_cast<float>(x + w) / display.x - 1.0f;
	const float y0 = 2.0f * static_cast<float>(y)     / display.y - 1.0f;
	const float y1 = 2.0f * static_cast<float>(y + h) / display.y - 1.0f;

	// Only the bottom left corner of the power-of-two texture is used.
	const float fu = static_cast<float>(context.frameSize.x) / static_cast<float>(context.textureSize.x);
	const float fv = static_cast<float>(context.frameSize.y) / static_cast<float>(context.textureSize.y);

	// Two triangles as a strip: position, then texture coordinate.
	const float vertices[16] =
	{
		x0, y0, 0.0f, 0.0f,
		x1, y0, fu,   0.0f,
		x0, y1, 0.0f, fv,
		x1, y1, fu,   fv
	};

	glExtBindBuffer(GL_ARRAY_BUFFER, context.vertexBuffer);
	glExtBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glExtEnableVertexAttribArray(0);
	glExtEnableVertexAttribArray(1);
	glExtVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(0));
	glExtVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glExtDisableVertexAttribArray(0);
	glExtDisableVertexAttribArray(1);
	glExtBindBuffer(GL_ARRAY_BUFFER, 0);
	glExtUseProgram(0);
}

Upscaler::Upscaler()
{
}

Upscaler::~Upscaler()
{
}

bool Upscaler::createGL()
{
	return program.create(getFragmentSource(), getName());
}

void Upscaler::destroyGL()
{
	program.destroy();
}

const char* Upscaler::getFragmentSource() const
{
	return p_plainFragmentShader;
}

void Upscaler::present(const PresentContext& context)
{
	program.use(context);
	program.drawQuad(context);
}

void Upscaler::loadConfig(TiXmlElement* p_config)
{
	// Most filters have nothing to set.
}

void Upscaler::saveConfig(TiXmlElement* p_config)
{
}
