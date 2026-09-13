#ifndef _RENDERLAYER_H
#define _RENDERLAYER_H

/*** The passes Level::render makes over the objects ***/

// A render layer is a pass, not a depth: Level::render walks these in the order
// written below and asks every object to draw whatever it has on that one.
// Sorting within a pass is Level::sortObjects' business.
//
// The values are single bits, so that an object's set of layers is the OR of
// the ones it draws on and testing a pass against it is one AND - see
// Object::getRenderLayers(). That puts a ceiling of 32 layers on this list,
// which is twelve away and would be a different design anyway.
//
// The order is the order the passes run in, which is the order a reader wants
// them. Nothing outside the program sees the values - a level file stores an
// object's type and position, never a layer - so they can be renumbered freely.
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
