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

void startReadingActions(SampleChannel* ch, bool treatRecsAsLoops, bool recsStopOnChanHalt);
void stopReadingActions(SampleChannel* ch, bool isClockRunning, bool treatRecsAsLoops, 
		bool recsStopOnChanHalt);

/* kill
Stops a channel abruptly. */

void kill(SampleChannel* ch, int localFrame);
void manualKill(SampleChannel* ch);

/* stop
Stops a channel normally (via key or MIDI). */

void stop(SampleChannel* ch, bool isUserGenerated);

/* stopBySeq
Stops a channel when the stop button on main transport is pressed. */

void stopBySeq(SampleChannel* ch, bool chansStopOnSeqHalt);

/* start
Starts a channel. doQuantize = false (don't quantize) when Mixer is reading 
actions from Recorder. If isUserGenerated means that the channel has been 
started by a human key press and not by a pre-recorded action. */

void start(SampleChannel* ch, int localFrame, bool doQuantize, bool forceStart, 
	bool isUserGenerated, bool record, int velocity);

/* rewind
Rewinds channel when rewind button on main transport is pressed. */

void rewind(SampleChannel* ch);

/* empty
Frees any associated resources (e.g. waveform). */

void empty(SampleChannel* ch);


void setMute  (SampleChannel* ch, bool internal);
void unsetMute(SampleChannel* ch, bool internal);
}}};


#endif