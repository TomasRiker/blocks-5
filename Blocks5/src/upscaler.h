#ifndef _UPSCALER_H
#define _UPSCALER_H

// How the internally rendered 640x480 frame reaches the screen.
//
// Each filter is a class of its own: it knows its name in config.xml, does
// its own drawing, brings its own settings along and knows whether it
// distorts the picture. The Engine owns one of each kind, picks one of them
// and needs to know nothing else about them - above all not that there is a
// curved tube.

// Everything a filter needs in order to draw. All of it belongs to the
// Engine: framebuffer, texture and vertex buffer come and go with the GL
// context, not per filter.
struct PresentContext
{
	Vec2i rectPosition;   // bottom left of the destination rect in the window
	Vec2i rectSize;       // its size; the aspect ratio is already right
	Vec2i displaySize;    // window size, for the clip coordinates
	Vec2i frameSize;      // the rendered frame, always 640x480
	Vec2i textureSize;    // power of two; frameSize sits at its bottom left
	uint vertexBuffer;    // WebGL forbids vertex data from application memory
};

// A compiled present program. All of them share the vertex shader in
// upscaler.cpp, and each filter brings its own fragment source. The four
// uniforms here are the ones SharpFit and the CRT filter both have; the plain
// program Sharp and Smooth draw through has only decal, and use() skips a
// location of -1. A filter that needs more fetches them itself.
struct PresentProgram
{
	PresentProgram();

	// p_name appears in the error message and nowhere else. A second call
	// deletes the old program first.
	bool create(const char* p_fragmentSource, const char* p_name);
	void destroy();

	// Bind the program and set the four shared uniforms. A filter can then add
	// its own afterwards, before drawQuad().
	void use(const PresentContext& context) const;

	// Draw the destination rect as two triangles and clean up the state.
	void drawQuad(const PresentContext& context) const;

	// Set a uniform if the shader has it: one with a line commented out may
	// not.
	static void setUniform(int location, float value);

	uint id;
	// One line each: Tools/verify.py looks for "Type Name;" and would miss a
	// combined declaration - and with it a member the constructor forgets.
	int decal;
	int textureSize;
	int frameSize;
	int prescale;
};

class Upscaler
{
public:
	Upscaler();
	// Clears *no* GL state: the destructor runs long after the context is
	// gone. destroyGL() is there for that.
	virtual ~Upscaler();

	// The name in config.xml, in the options dialog, in the log and in the
	// test hook - a file format, not a caption. The caption is the $ID on the
	// label elements in data/options.xml.
	virtual const char* getName() const = 0;

	// Create and tear down GL state, both only with a standing context and
	// both allowed more than once. The base class compiles the filter's
	// fragment shader into program; a filter with uniforms of its own fetches
	// their locations after that, valid exactly as long as the program is.
	// False from createGL() ends the game: the options dialog offers all
	// four, so every one has to work.
	virtual bool createGL();
	virtual void destroyGL();

	// GL_NEAREST or GL_LINEAR for the framebuffer texture.
	virtual GLint getTextureFilter() const = 0;

	// Only Sharp needs an integer scale step: at a fractional factor nearest
	// doubles some source pixels and not others.
	virtual bool wantsIntegerScale() const { return false; }

	// Put the picture on the screen. Texture filter and the black background
	// are already set, the texture is bound. The default draws the rect
	// through program - Sharp and Smooth differ in nothing but the texture
	// filter and both draw that way.
	virtual void present(const PresentContext& context);

	// The same mapping as in the filter's own shader, in both directions; the
	// coordinates run from -1 to 1 out from the centre of the picture. A
	// filter that does not distort the picture returns its argument unchanged.
	// Both run on every tick and every recorded frame, so a virtual call and
	// not a lookup by name.
	virtual Vec2f warpToSource(const Vec2f& p) const { return p; }
	virtual Vec2f warpToOutput(const Vec2f& s) const { return s; }

	// Is this filter really distorting the picture right now? The test hook
	// reports it, which is how a test notices that its window coordinates no
	// longer hold.
	virtual bool distortsCursor() const { return false; }

	// Read the filter's own settings from <Config> and write them back there.
	// The filter creates its element itself, so the element name stands in
	// u_*.cpp, where Tools/verify.py's config check sees both halves together.
	// Runs without a GL context, and may come more than once: the options
	// dialog's Cancel calls loadConfig() mid-game. Read only what is there
	// and reset nothing.
	virtual void loadConfig(TiXmlElement* p_config);
	virtual void saveConfig(TiXmlElement* p_config);

protected:
	// The fragment shader createGL() builds program from. The base class's
	// hands the texel through as it is; SharpFit and the CRT filter bring
	// their own.
	virtual const char* getFragmentSource() const;

	PresentProgram program;
};

#endif
