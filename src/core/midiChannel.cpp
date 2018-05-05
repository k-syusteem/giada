/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2018 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */


#include "../utils/log.h"
#include "midiProc.h"
#include "channelManager.h"
#include "channel.h"
#include "patch.h"
#include "const.h"
#include "clock.h"
#include "conf.h"
#include "mixer.h"
#include "pluginHost.h"
#include "kernelMidi.h"
#include "midiChannel.h"


using std::string;
using namespace giada::m;


MidiChannel::MidiChannel(int bufferSize)
	: Channel    (G_CHANNEL_MIDI, STATUS_OFF, bufferSize),
		midiOut    (false),
		midiOutChan(MIDI_CHANS[0])
{
}


/* -------------------------------------------------------------------------- */


MidiChannel::~MidiChannel() {}


/* -------------------------------------------------------------------------- */


void MidiChannel::copy(const Channel* src_, pthread_mutex_t* pluginMutex)
{
	Channel::copy(src_, pluginMutex);
	const MidiChannel* src = static_cast<const MidiChannel*>(src_);
	midiOut     = src->midiOut;
	midiOutChan = src->midiOutChan;
}


/* -------------------------------------------------------------------------- */


void MidiChannel::parseEvents(mixer::FrameEvents fe, size_t index)
{
	midiProc::parseEvents(this, fe, index);
}

/* -------------------------------------------------------------------------- */


void MidiChannel::process(giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in)
{
	midiProc::process(this, out, in);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::stopBySeq(bool chansStopOnSeqHalt)
{
	midiProc::stopBySeq(this);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::start(int frame, bool doQuantize, int quantize,
		bool mixerIsRunning, bool forceStart, bool isUserGenerated, bool record,
		int velocity)
{
	midiProc::start(this);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::kill(int localFrame)
{
	midiProc::kill(this, localFrame);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::manualKill()
{
	midiProc::kill(this, 0);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::rewind()
{
	midiProc::rewind(this);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::setMute(bool internal)
{
	midiProc::mute(this);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::unsetMute(bool internal)
{
	midiProc::unmute(this);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::readPatch(const string& basePath, int i)
{
	Channel::readPatch("", i);
	channelManager::readPatch(this, i);
}


/* -------------------------------------------------------------------------- */


void MidiChannel::writePatch(int i, bool isProject)
{
	Channel::writePatch(i, isProject);
	channelManager::writePatch(this, isProject, i);
}


/* -------------------------------------------------------------------------- */


#ifdef WITH_VST

void MidiChannel::addVstMidiEvent(uint32_t msg, int localFrame)
{
	juce::MidiMessage message = juce::MidiMessage(
		kernelMidi::getB1(msg),
		kernelMidi::getB2(msg),
		kernelMidi::getB3(msg));
	midiBuffer.addEvent(message, localFrame);
}

#endif


/* -------------------------------------------------------------------------- */


void MidiChannel::stop(bool isUserGenerated) {}
void MidiChannel::empty() {}


/* -------------------------------------------------------------------------- */


void MidiChannel::sendMidi(recorder::action* a, int localFrame)
{
	if (status & (STATUS_PLAY | STATUS_ENDING) && !mute) {
		if (midiOut)
			kernelMidi::send(a->iValue | MIDI_CHANS[midiOutChan]);

#ifdef WITH_VST
		addVstMidiEvent(a->iValue, localFrame);
#endif
	}
}


void MidiChannel::sendMidi(uint32_t data)
{
	if (status & (STATUS_PLAY | STATUS_ENDING) && !mute) {
		if (midiOut)
			kernelMidi::send(data | MIDI_CHANS[midiOutChan]);
#ifdef WITH_VST
		addVstMidiEvent(data, 0);
#endif
	}
}


/* -------------------------------------------------------------------------- */


void MidiChannel::receiveMidi(const MidiEvent& midiEvent)
{
	if (!armed)
		return;

	/* Now all messages are turned into Channel-0 messages. Giada doesn't care about
	holding MIDI channel information. Moreover, having all internal messages on
	channel 0 is way easier. */

	MidiEvent midiEventFlat(midiEvent);
	midiEventFlat.setChannel(0);

#ifdef WITH_VST

	while (true) {
		if (pthread_mutex_trylock(&pluginHost::mutex_midi) != 0)
			continue;
		gu_log("[Channel::processMidi] msg=%X\n", midiEventFlat.getRaw());
		addVstMidiEvent(midiEventFlat.getRaw(), 0);
		pthread_mutex_unlock(&pluginHost::mutex_midi);
		break;
	}

#endif

	if (recorder::canRec(this, clock::isRunning(), mixer::recording)) {
		recorder::rec(index, G_ACTION_MIDI, clock::getCurrentFrame(), midiEventFlat.getRaw());
		hasActions = true;
	}
}


/* -------------------------------------------------------------------------- */


bool MidiChannel::canInputRec()
{
	return false; // midi channels don't handle input audio
}


/* -------------------------------------------------------------------------- */


void MidiChannel::fillBuffer() {}
