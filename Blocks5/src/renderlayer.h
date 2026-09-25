#ifndef _RENDERLAYER_H
#define _RENDERLAYER_H

/*** The passes Level::render makes over the objects ***/

// A render layer is a pass, not a depth: Level::render walks these in the order
// written below - all but the last, which the level editor draws itself - and
// asks every object to draw what it has on that one. The order within a pass
// is Level::sortObjects' business.
//
// Single bits, so an object's set is the OR of the layers it draws on and a
// pass tests it with one AND (Object::getRenderLayers()); 32 at most. No file
// stores a layer, so the values can be renumbered freely.
enum RenderLayer
{
	RL_LAVA_EDGE	= 0x001,	// the lava outline, drawn into the stencil buffer
	RL_LAVA_BACK	= 0x002,	// the lava itself
	RL_LAVA_FRONT	= 0x004,	// and its second, additive pass
	RL_FLOOR		= 0x008,	// panels lying on the ground, under everything
	RL_WIRE			= 0x010,	// the electronics connections
	RL_MAIN			= 0x020,	// the middle ground, where nearly every object is
	RL_EFFECT		= 0x040,	// beams and shots, drawn over the middle ground
	RL_EDITOR		= 0x080,	// what only the level editor shows
	RL_LIGHT		= 0x100,	// the lights night vision accumulates
	RL_SPARKLE		= 0x200,	// the glints it puts on top of them
	RL_OVERLAY		= 0x400,	// speech balloons and the hint note
	RL_HINT_PREVIEW	= 0x800		// the hint editor's own preview of a note
};

#endif
