#ifndef _CROSSFADE_H
#define _CROSSFADE_H

// Class for crossfades

class Crossfade
{
public:
	Crossfade();
	virtual ~Crossfade();

	virtual void render(float t, uint oldImageID, uint newImageID);

protected:
	// The two frame copies as render states: sampled with the frame's
	// texel scale, so that a crossfade writes its texture coordinates in
	// the game's own pixels.
	RenderState imageState(uint imageID) const;

	// The whole screen from one of the images, and a flat colour over it.
	void drawImage(uint imageID, const Vec4f& color) const;
	void drawColor(const Vec4f& color) const;

	// Four corners in 3D and the image's texels at them, in order, under a
	// matrix with the projection in it: what the 3D crossfades draw.
	void drawImage3D(uint imageID, const Mat4& transform, const Vec3f* p_corners,
					 const Vec2f* p_uvs, const Vec4f& color, bool cullBackFaces) const;

	Vec2i screenSize;
	// The whole ref the engine hands out for a frame copy, not just its texel
	// scale: that ref's y is negative, so it wraps rather than clamps, and a
	// ref rebuilt from the scale alone would lose the flag that says so.
	TextureRef screenRef;
};

#endif
