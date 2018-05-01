#ifndef G_AUDIO_PROC_H
#define G_AUDIO_PROC_H


#include "mixer.h"
#include "audioBuffer.h"


class SampleChannel;


namespace giada {
namespace m {
namespace audioProc
{
void clearBuffers(SampleChannel* ch);
void start(SampleChannel* ch, int localFrame, bool doQuantize, bool forceStart, 
	bool isUserGenerated);
void prepare(SampleChannel* ch, mixer::FrameEvents ev, size_t chanIndex);
void process(SampleChannel* ch, giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in);
}}};


#endif