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
	uint textureID;
	uint vertexBuffer;    // WebGL forbids vertex data from application memory
};

// A compiled present program. All of them share the vertex shader - the only
// text in upscaler.cpp - while each filter brings its own fragment source.
// Only the four uniforms that *every* one of them has live here; a filter
// that needs more fetches them itself. There is therefore not a single one
// here that sits at -1 for half the filters.
struct PresentProgram
{
	PresentProgram();

	// p_name appears in the error message and nowhere else. A second call
	// clears the old program away first: in the browser the GL context can be
	// lost.
	bool create(const char* p_fragmentSource, const char* p_name);
	void destroy();
	bool isLinked() const { return id != 0; }

	// Bind the program and set the four shared uniforms. A filter can then add
	// its own afterwards, before drawQuad().
	void use(const PresentContext& context) const;

	// Draw the destination rect as two triangles and clean up the state.
	void drawQuad(const PresentContext& context) const;

	// Set a uniform if it exists. Otherwise a filter that comments out a line
	// of its shader gets GL_INVALID_OPERATION instead of nothing.
	static void setUniform(int location, double value);

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
	// both allowed more than once. A filter that needs none leaves the base
	// case alone.
	virtual bool createGL() { return true; }
	virtual void destroyGL() {}

	// Can this filter draw on this machine? Without a compiled program, no. A
	// framebuffer object is not a condition but a precondition: without one
	// the Engine never even calls createGL().
	virtual bool isAvailable() const { return true; }

	// GL_NEAREST or GL_LINEAR for the framebuffer texture.
	virtual GLint getTextureFilter() const = 0;

	// Only Sharp needs an integer scale step: at a fractional factor nearest
	// doubles some source pixels and not others.
	virtual bool wantsIntegerScale() const { return false; }

	// Put the picture on the screen. Texture filter, matrices and the black
	// background are already set, the texture is bound. The default is the
	// fixed-function quad - Sharp and Smooth differ in nothing but the texture
	// filter and both draw that way.
	virtual void present(const PresentContext& context);

	// The same mapping as in the filter's own shader, in both directions; the
	// coordinates run from -1 to 1 out from the centre of the picture. A
	// filter that does not distort the picture returns its argument unchanged.
	// Both sit on the 20 ms tick and on every recorded frame - a virtual
	// call is fine, a lookup by name would not be.
	virtual Vec2d warpToSource(const Vec2d& p) const { return p; }
	virtual Vec2d warpToOutput(const Vec2d& s) const { return s; }

	// Is this filter really distorting the picture right now? The test hook
	// reports it, which is how a test notices that its window coordinates no
	// longer hold.
	virtual bool distortsCursor() const { return false; }

	// Read the filter's own settings from <Config> and write them back there.
	// The filter creates its element itself - that is why the element name
	// stands in u_*.cpp and not in the Engine, and why Tools/verify.py sees
	// both halves together. Runs without a GL context, and may come more than
	// once: the options dialog's Cancel button calls loadConfig() too, in the
	// middle of the game. Read only what is there and reset nothing.
	virtual void loadConfig(TiXmlElement* p_config);
	virtual void saveConfig(TiXmlElement* p_config);
};

#endif
