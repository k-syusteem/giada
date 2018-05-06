#include "midiChannel.h"
#include "pluginHost.h"
#include "kernelMidi.h"
#include "const.h"
#include "midiProc.h"


namespace giada {
namespace m {
namespace midiProc
{
namespace
{
void onFirstBeat(MidiChannel* ch)
{
	if (ch->status == STATUS_ENDING) {
		ch->status = STATUS_OFF;
		ch->sendMidiLplay(); // TODO midi
	}
	else
	if (ch->status == STATUS_WAIT) {
		ch->status = STATUS_PLAY;
		ch->sendMidiLplay(); // TODO midi
	}
}


/* -------------------------------------------------------------------------- */


void parseAction(MidiChannel* ch, const recorder::action* a, int localFrame)
{
	if (ch->isPlaying() && !ch->mute) {
		if (ch->midiOut)
			kernelMidi::send(a->iValue | MIDI_CHANS[ch->midiOutChan]);
#ifdef WITH_VST
		ch->addVstMidiEvent(a->iValue, localFrame);
#endif
	}
}

}; // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void parseEvents(MidiChannel* ch, mixer::FrameEvents fe, size_t chanIndex)
{
	if (fe.onFirstBeat)
		onFirstBeat(ch);
	for (const recorder::action* action : fe.actions)
		if (action->chan == ch->index && action->type == G_ACTION_MIDI)
			parseAction(ch, action, fe.frameLocal);
}


/* -------------------------------------------------------------------------- */


void process(MidiChannel* ch, giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in)
{
	#ifdef WITH_VST
		pluginHost::processStack(ch->buffer, pluginHost::CHANNEL, ch);
	#endif

		/* TODO - isn't this useful only if WITH_VST ? */
		for (int i=0; i<out.countFrames(); i++)
			for (int j=0; j<out.countChannels(); j++)
				out[i][j] += ch->buffer[i][j] * ch->volume;	
}


/* -------------------------------------------------------------------------- */


void start(MidiChannel* ch)
{
	switch (ch->status) {
		case STATUS_PLAY:
			ch->status = STATUS_ENDING;
			ch->sendMidiLplay(); // TODO midi
			break;
		case STATUS_ENDING:
		case STATUS_WAIT:
			ch->status = STATUS_OFF;
			ch->sendMidiLplay(); // TODO midi
			break;
		case STATUS_OFF:
			ch->status = STATUS_WAIT;
			ch->sendMidiLplay(); // TODO midi
			break;
	}	
}


/* -------------------------------------------------------------------------- */


void kill(MidiChannel* ch, int localFrame)
{
	if (ch->isPlaying()) {
		if (ch->midiOut)
			kernelMidi::send(MIDI_ALL_NOTES_OFF);
#ifdef WITH_VST
		ch->addVstMidiEvent(MIDI_ALL_NOTES_OFF, 0);
#endif
	}
	ch->status = STATUS_OFF;
	ch->sendMidiLplay(); // TODO midi
}


/* -------------------------------------------------------------------------- */


void rewind(MidiChannel* ch)
{
	if (ch->midiOut)
		kernelMidi::send(MIDI_ALL_NOTES_OFF);
#ifdef WITH_VST
		ch->addVstMidiEvent(MIDI_ALL_NOTES_OFF, 0);
#endif	
}


/* -------------------------------------------------------------------------- */


void mute(MidiChannel* ch)
{
	ch->mute = true;
	if (ch->midiOut)
		kernelMidi::send(MIDI_ALL_NOTES_OFF);
#ifdef WITH_VST
		ch->addVstMidiEvent(MIDI_ALL_NOTES_OFF, 0);
#endif
	ch->sendMidiLmute(); // TODO midi
}


/* -------------------------------------------------------------------------- */


void unmute(MidiChannel* ch)
{
	ch->mute = false;
	ch->sendMidiLmute(); // TODO midi
}


/* -------------------------------------------------------------------------- */


void stopBySeq(MidiChannel* ch)
{
	kill(ch, 0);
}
}}};