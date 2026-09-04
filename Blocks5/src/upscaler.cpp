#include "pch.h"
#include "upscaler.h"
#include "glextensions.h"

/* Der Vertexshader aller Praesentierfilter. Die Eckpunkte kommen fertig in
   Clipkoordinaten aus drawQuad(), es gibt also nichts zu transformieren. Kein
   Anfassen des Fixed-Function-Zustands, und im Browser damit auch keine
   Beruehrung mit Emscriptens Immediate-Mode-Nachbau.

   Kein #version: 110 auf dem Desktop, 100 unter GLSL ES, und der Quelltext
   uebersetzt als beides. */
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

namespace
{
	// Uebersetzt eine Stufe und gibt bei einem Fehler das Log aus.
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
	// Nur das Programm. Die Uniformstellen sind ohnehin nur gueltig, solange es
	// eines gibt - wer sie ausserdem zuruecksetzt, fuehrt eine zweite Liste,
	// die vergessen werden kann.
	if(id) { glExtDeleteProgram(id); id = 0; }
}

void PresentProgram::setUniform(int location,
								double value)
{
	if(location >= 0) glExtUniform1f(location, static_cast<float>(value));
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
		// Der kleinste ganzzahlige Faktor, mit dem das 640x480-Bild das
		// Zielrechteck ausfuellt. Genau um den wuerde man mit nearest
		// vergroessern, bevor man heruntergeht - der Shader macht beides.
		const float x = static_cast<float>(max(1, static_cast<int>(
			ceil(static_cast<double>(context.rectSize.x) / context.frameSize.x))));
		const float y = static_cast<float>(max(1, static_cast<int>(
			ceil(static_cast<double>(context.rectSize.y) / context.frameSize.y))));
		glExtUniform2f(prescale, x, y);
	}
}

void PresentProgram::drawQuad(const PresentContext& context) const
{
	// Der Shader rechnet in Clipkoordinaten - keine Matrix und damit im Browser
	// keine Beruehrung mit Emscriptens Immediate-Mode-Nachbau.
	const int x = context.rectPosition.x;
	const int y = context.rectPosition.y;
	const int w = context.rectSize.x;
	const int h = context.rectSize.y;

	const float x0 = 2.0f * x       / context.displaySize.x - 1.0f;
	const float x1 = 2.0f * (x + w) / context.displaySize.x - 1.0f;
	const float y0 = 2.0f * y       / context.displaySize.y - 1.0f;
	const float y1 = 2.0f * (y + h) / context.displaySize.y - 1.0f;

	// Benutzt wird nur die linke untere Ecke der Zweierpotenz-Textur.
	const float fu = static_cast<float>(context.frameSize.x) / context.textureSize.x;
	const float fv = static_cast<float>(context.frameSize.y) / context.textureSize.y;

	// Zwei Dreiecke als Streifen: Position, dann Texturkoordinate.
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

void Upscaler::present(const PresentContext& context)
{
	// Ohne Shader: ein Viereck der festen Funktionsstufe. Der Texturfilter
	// steht schon, und er ist der ganze Unterschied zwischen "Scharf" und
	// "Weich".
	const double u = static_cast<double>(context.frameSize.x) / context.textureSize.x;
	const double v = static_cast<double>(context.frameSize.y) / context.textureSize.y;
	const int x = context.rectPosition.x;
	const int y = context.rectPosition.y;
	const int w = context.rectSize.x;
	const int h = context.rectSize.y;

	glBegin(GL_QUADS);
	glTexCoord2d(0.0, 0.0); glVertex2i(x,     y);
	glTexCoord2d(u,   0.0); glVertex2i(x + w, y);
	glTexCoord2d(u,   v);   glVertex2i(x + w, y + h);
	glTexCoord2d(0.0, v);   glVertex2i(x,     y + h);
	glEnd();
}

void Upscaler::loadConfig(TiXmlElement* p_config)
{
	// Die meisten Filter haben nichts einzustellen.
}

void Upscaler::saveConfig(TiXmlElement* p_config)
{
}
