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


#ifndef G_CHANNEL_H
#define G_CHANNEL_H


#include <vector>
#include <string>
#include <pthread.h>
#include "mixer.h"
#include "midiMapConf.h"
#include "midiEvent.h"
#include "recorder.h"
#include "audioBuffer.h"

#ifdef WITH_VST
	#include "../deps/juce-config.h"
#endif


class Plugin;
class MidiMapConf;
class geChannel;


class Channel
{
protected:

	Channel(int type, int status, int bufferSize);

	/* sendMidiLMessage
	Composes a MIDI message by merging bytes from MidiMap conf class, and sends it 
	to KernelMidi. */

	void sendMidiLmessage(uint32_t learn, const giada::m::midimap::message_t& msg);

#ifdef WITH_VST

	/* MidiBuffer contains MIDI events. When ready, events are sent to each plugin 
	in the channel. This is available for any kind of channel, but it makes sense 
	only for MIDI channels. */

	juce::MidiBuffer midiBuffer;

#endif
	
public:

	virtual ~Channel();

	/* copy
	Makes a shallow copy (no vChan/pChan allocation) of another channel. */

	virtual void copy(const Channel* src, pthread_mutex_t* pluginMutex) = 0;

	/* parseEvents
	Prepares channel for rendering. This is called on each frame. */

	virtual void parseEvents(giada::m::mixer::FrameEvents fe, size_t index) = 0;

	/* process
	Merges vChannels into buffer, plus plugin processing (if any). Warning:
	inBuffer might be nullptr if no input devices are available for recording. */

	virtual void process(giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in) = 0;

	/* start
	Action to do when channel starts. doQuantize = false (don't quantize)
	when Mixer is reading actions from Recorder. If isUserGenerated means that
	the channel has been started by a human key press and not a pre-recorded
	action. */

	virtual void start(int localFrame, bool doQuantize, int quantize,
		bool mixerIsRunning, bool forceStart, bool isUserGenerated, bool record,
		int velocity) = 0;

	/* stop
	What to do when channel is stopped normally (via key or MIDI). */

	virtual void stop(bool isUserGenerated) = 0;

	/* kill
	What to do when channel stops abruptly. */

	virtual void kill(int localFrame) = 0;
	virtual void manualKill() = 0;

	/* mute
	What to do when channel is muted. If internal == true, set internal mute 
	without altering main mute. */

	virtual void setMute  (bool internal) = 0;
	virtual void unsetMute(bool internal) = 0;

	/* empty
	Frees any associated resources (e.g. waveform for SAMPLE). */

	virtual void empty() = 0;

	/* stopBySeq
	What to do when channel is stopped by sequencer. */

	virtual void stopBySeq(bool chansStopOnSeqHalt) = 0;

	/* rewind
	Rewinds channel when rewind button is pressed. */

	virtual void rewind() = 0;

	/* fillBuffer
	Fill audio buffer with audio data from the internal source. This is actually 
	useful to sample channels only. */

	virtual void fillBuffer() = 0;

	/* canInputRec
	Tells whether a channel can accept and handle input audio. Always false for
	Midi channels, true for Sample channels only if they don't contain a
	sample yet.*/

	virtual bool canInputRec() = 0;

	/* readPatch
	Fills channel with data from patch. */

	virtual void readPatch(const std::string& basePath, int i);

	/* writePatch
	Fills a patch with channel values. Returns the index of the last 
	Patch::channel_t added. */

	virtual void writePatch(int i, bool isProject);

	/* receiveMidi
	Receives and processes midi messages from external devices. */

	virtual void receiveMidi(const giada::m::MidiEvent& midiEvent);

	/* allocBuffers
	Mandatory method to allocate memory for internal buffers. Call it after the
	object has been constructed. */

	virtual bool allocBuffers();

	/* calcPanning
	Given an audio channel (stereo: 0 or 1) computes the current panning value. */

	float calcPanning(int ch);

	bool isPlaying() const;
	float getPan() const;
	bool isPreview() const;

	/* isMidiAllowed
	Given a MIDI channel 'c' tells whether this channel should be allowed to receive
	and process MIDI events on MIDI channel 'c'. */

	bool isMidiInAllowed(int c) const;

	/* sendMidiL*
	 * send MIDI lightning events to a physical device. */

	void sendMidiLmute();
	void sendMidiLsolo();
	void sendMidiLplay();

	void setPan(float v);
	void setVolumeI(float v);
	void setPreviewMode(int m);

#ifdef WITH_VST

	/* getPluginMidiEvents
	 * Return a reference to midiBuffer stack. This is available for any kind of
	 * channel, but it makes sense only for MIDI channels. */

	juce::MidiBuffer& getPluginMidiEvents();

	void clearMidiBuffer();

#endif

	/* bufferSize
	Size of every buffer in this channel (vChan, pChan) */

	int bufferSize;

  geChannel* guiChannel;        // pointer to a gChannel object, part of the GUI

	/* vChan
	Virtual channel for internal processing. */
	
	giada::m::AudioBuffer vChan;

	/* previewMode
	Whether the channel is in audio preview mode or not. */

	int previewMode;

	float       pan;
	float       volume;   // global volume
	bool        armed;
	std::string name;
	int         index;    // unique id
	int         type;     // midi or sample
	int         status;   // status: see const.h
	int         key;      // keyboard button
	bool        mute;     // global mute
	bool        solo;

	/* volume_*
	Internal volume variables: volume_i for envelopes, volume_d keeps track of
	the delta during volume changes. */
	
	float volume_i;
	float volume_d;

	bool mute_i;                // internal mute
	
  bool hasActions;      // has something recorded
  bool readActions;     // read what's recorded
	int  recStatus;       // status of recordings (waiting, ending, ...)
  
  bool      midiIn;               // enable midi input
  uint32_t  midiInKeyPress;
  uint32_t  midiInKeyRel;
  uint32_t  midiInKill;
  uint32_t  midiInArm;
  uint32_t  midiInVolume;
  uint32_t  midiInMute;
  uint32_t  midiInSolo;

  /* midiInFilter
  Which MIDI channel should be filtered out when receiving MIDI messages. -1
  means 'all'. */

  int midiInFilter;

	/*  midiOutL*
	 * Enable MIDI lightning output, plus a set of midi lighting event to be sent
	 * to a device. Those events basically contains the MIDI channel, everything
	 * else gets stripped out. */

	bool     midiOutL;
  uint32_t midiOutLplaying;
  uint32_t midiOutLmute;
  uint32_t midiOutLsolo;

#ifdef WITH_VST
  std::vector <Plugin*> plugins;
#endif

};


#endif
