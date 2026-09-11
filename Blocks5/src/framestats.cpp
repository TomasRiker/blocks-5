#include "pch.h"
#include "framestats.h"

FrameStats::FrameStats()
{
	clear();
}

void FrameStats::clear()
{
	next = 0;
	count = 0;
}

void FrameStats::addFrame(const float* p_phases)
{
	for(int i = 0; i < FS_NUM_PHASES; i++) samples[next][i] = p_phases[i];
	next = (next + 1) % CAPACITY;
	if(count < CAPACITY) count++;
}

float FrameStats::getPercentile(Phase phase,
								int percentile) const
{
	if(!count) return 0.0f;

	// Sorted into a copy rather than in place: the ring is the record and the
	// next frame appends to it whatever anybody is asking. 512 floats is a
	// tenth of a millisecond to sort and this is asked once per report, not
	// once per frame.
	std::vector<float> values;
	values.reserve(count);
	for(uint i = 0; i < count; i++) values.push_back(samples[i][phase]);
	std::sort(values.begin(), values.end());

	// The nearest-rank definition: the smallest value that at least this share
	// of the samples is under. It needs no interpolation and cannot invent a
	// number that was never measured, which for a maximum is the whole point.
	if(percentile < 0) percentile = 0;
	if(percentile > 100) percentile = 100;
	uint rank = (static_cast<uint>(percentile) * count + 99) / 100;
	if(rank < 1) rank = 1;
	return values[rank - 1];
}

uint FrameStats::getCountOver(Phase phase,
							  float milliseconds) const
{
	uint over = 0;
	for(uint i = 0; i < count; i++) if(samples[i][phase] > milliseconds) over++;
	return over;
}
