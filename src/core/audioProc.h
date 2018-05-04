#ifndef G_AUDIO_PROC_H
#define G_AUDIO_PROC_H


#include "mixer.h"
#include "audioBuffer.h"


class SampleChannel;


namespace giada {
namespace m {
namespace audioProc
{
/**/
void fillBuffer(SampleChannel* ch);

/* parseEvents
Parses events gathered by Mixer::masterPlay(). */

void parseEvents(SampleChannel* ch, mixer::FrameEvents ev, size_t chanIndex);

/**/
void process(SampleChannel* ch, giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in);

/* setReadActions
If enabled (v == true), Recorder will read actions from channel 'ch'. If 
recsStopOnChanHalt == true and v == false, will also kill the channel. */

void setReadActions(SampleChannel* ch, bool v, bool recsStopOnChanHalt);

/* kill
Stops a channel abruptly. */

void kill(SampleChannel* ch, int localFrame);

/* stop
Stops a channel normally (via key or MIDI). */

void stop(SampleChannel* ch);

/* stopBySeq
Stops a channel when the stop button on main transport is pressed. */

void stopBySeq(SampleChannel* ch, bool chansStopOnSeqHalt);

/* start
Starts a channel. doQuantize = false (don't quantize) when Mixer is reading 
actions from Recorder. If isUserGenerated means that the channel has been 
started by a human key press and not by a pre-recorded action. */

void start(SampleChannel* ch, int localFrame, bool doQuantize, bool forceStart, 
	bool isUserGenerated);

/* rewind
Rewinds channel when rewind button on main transport is pressed. */

void rewind(SampleChannel* ch);
}}};


#endif