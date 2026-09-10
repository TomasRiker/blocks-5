#ifndef _FRAMESTATS_H
#define _FRAMESTATS_H

/*** Class for what the last few hundred frames cost ***/

class FrameStats
{
public:
	// What is timed in one turn of the main loop. INTERVAL is start to start -
	// the rate the player sees - and TOTAL how long that turn held the main
	// thread, which is a different number whenever something else sets the
	// pace: in the browser requestAnimationFrame does, natively the SDL_Delay
	// at the foot of the loop does.
	//
	// RENDER and PRESENT are what issuing the draw calls costs and not what
	// drawing them does. GL is asynchronous, so the work queues up and is paid
	// for wherever the driver next flushes - and natively that is somewhere in
	// PRESENT and SWAP, whichever it picks. Read those two as one number:
	// their split is the driver's choice and not a fact about the game.
	//
	// Measured under llvmpipe with a glFinish inserted to find out: render
	// *issues* in 2.2 ms and the finish after it takes another 5.9, the blit
	// issues in 1.3 and takes 3.0, and glXSwapBuffers costs 3.9 once nothing
	// is outstanding. Without that finish the same 5.9 turns up inside
	// PRESENT, which then reads 8.5 against 1.3 of actual work. So a single
	// PRESENT carrying all of it read as 14.9 ms and was really the level
	// rasterizing: the wall clock was true and the label was a lie.
	//
	// In the browser none of it appears anywhere: SDL_GL_SwapBuffers is
	// Browser.doSwapBuffers?.(), which is undefined off a worker and therefore
	// does nothing, and glFinish returns in 0.02 ms. The page composites the
	// canvas after the callback returns, so **the GPU cost is outside every
	// window here** and what is left is exactly main-thread CPU. That is the
	// right measure for anything the emulation or the JavaScript does, and no
	// measure at all of the hardware. INTERVAL minus TOTAL is what is left for
	// it: if the frame rate falls while TOTAL stays flat, the time is going
	// somewhere this cannot see.
	enum Phase
	{
		FS_INTERVAL = 0,
		FS_TOTAL,
		FS_RENDER,
		FS_UPDATE,
		FS_PRESENT,
		FS_SWAP,
		FS_NUM_PHASES
	};

	// Enough for eight seconds at sixty frames, which is the stretch anybody
	// looks at, and 10 KB.
	static const uint CAPACITY = 512;

	FrameStats();

	// Milliseconds, in the order of the enum.
	void addFrame(const float* p_phases);
	void clear();

	uint getCount() const { return count; }

	// The value at that percentile of what is recorded, 0 for nothing
	// recorded. Percentiles and not a mean, because a frame that tears the
	// audio or drops a beat is in the tail: an average hides exactly the
	// frames worth finding.
	float getPercentile(Phase phase, int percentile) const;

	// Frames whose TOTAL passed a threshold. That is the shape of the
	// question the browser keeps asking - Emscripten's OpenAL schedules
	// 500 ms of audio ahead, so a frame longer than that is a hole in the
	// music - and a percentile cannot answer it without knowing which one to
	// ask for.
	uint getCountOver(Phase phase, float milliseconds) const;

private:
	// A ring, so that the newest CAPACITY frames are always what is reported
	// and a long run does not grow without bound.
	float samples[CAPACITY][FS_NUM_PHASES];
	uint next;
	uint count;
};

#endif
