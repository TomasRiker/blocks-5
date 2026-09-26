#ifndef _FRAMESTATS_H
#define _FRAMESTATS_H

/*** Class for what the last few hundred frames cost ***/

class FrameStats
{
public:
	// What is timed in one turn of the main loop. INTERVAL is start to start -
	// the rate the player sees - and TOTAL how long the turn held the main
	// thread; they differ whenever something else sets the pace
	// (requestAnimationFrame in the browser, natively the SDL_Delay at the
	// foot of the loop).
	//
	// RENDER and PRESENT are what issuing the draw calls costs, not drawing
	// them: GL is asynchronous, and natively the work is paid for somewhere in
	// PRESENT and SWAP, wherever the driver picks, so read those two as one
	// number. Measured under llvmpipe with a glFinish inserted: render issues
	// in 2.2 ms and the finish after it takes another 5.9, which without the
	// finish lands in PRESENT.
	//
	// In the browser the GPU cost is outside every window here:
	// SDL_GL_SwapBuffers does nothing off a worker, glFinish returns in
	// 0.02 ms, and the page composites the canvas after the callback returns.
	// What is left is main-thread CPU, so a frame rate that falls while TOTAL
	// stays flat is time going somewhere this cannot see.
	//
	// DRAWS is a count, not a duration: the draw calls the renderer made in
	// the turn. It rides in the same ring because the ring, the percentiles
	// and the threshold count are the same work whatever a column means.
	enum Phase
	{
		FS_INTERVAL = 0,
		FS_TOTAL,
		FS_RENDER,
		FS_UPDATE,
		FS_PRESENT,
		FS_SWAP,
		FS_DRAWS,
		FS_NUM_PHASES
	};

	// Ten seconds at the fifty frames a second the loop aims for; 14 KB.
	static const uint CAPACITY = 500;

	FrameStats();

	// One row in the order of the enum: milliseconds, and DRAWS a plain count.
	void addFrame(const float* p_phases);
	void clear();

	uint getCount() const { return count; }

	// The value at that percentile of what is recorded, 0 for nothing
	// recorded. Percentiles and not a mean: the frames that tear the audio or
	// drop a beat are in the tail, which an average hides.
	float getPercentile(Phase phase, int percentile) const;

	// Frames whose value in that phase passed a threshold - a question a
	// percentile cannot answer without knowing which one to ask for, such as
	// a frame longer than the 500 ms of audio Emscripten's OpenAL schedules
	// ahead, which leaves a hole in the music.
	uint getCountOver(Phase phase, float milliseconds) const;

private:
	// A ring: the newest CAPACITY frames are what is reported, and a long run
	// does not grow.
	float samples[CAPACITY][FS_NUM_PHASES];
	uint next;
	uint count;
};

#endif
