#ifndef G_MIDI_CHANNEL_PROC_H
#define G_MIDI_CHANNEL_PROC_H


#include "mixer.h"
#include "audioBuffer.h"


class MidiChannel;


namespace giada {
namespace m {
namespace midiChannelProc
{
/* parseEvents
Parses events gathered by Mixer::masterPlay(). */

void parseEvents(MidiChannel* ch, mixer::FrameEvents ev, size_t chanIndex);

/**/
void process(MidiChannel* ch, giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in);

/* kill
Stops a channel abruptly. */

void kill(MidiChannel* ch, int localFrame);

/* start
Starts a channel. */

void start(MidiChannel* ch);

/* stopBySeq
Stops a channel when the stop button on main transport is pressed. */

void stopBySeq(MidiChannel* ch);

/* rewind
Rewinds channel when rewind button on main transport is pressed. */

void rewindBySeq(MidiChannel* ch);

/* mute|unmute
Mutes/unmutes a channel. */

void mute  (MidiChannel* ch);
void unmute(MidiChannel* ch);
}}};


#endif