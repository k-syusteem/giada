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


#ifndef G_SAMPLE_CHANNEL_H
#define G_SAMPLE_CHANNEL_H


#include <functional>
#include <samplerate.h>
#include "channel.h"


class Patch;
class Wave;


class SampleChannel : public Channel
{
private:

	/* rsmp_state, rsmp_data
	Structs from libsamplerate. */

	SRC_STATE* rsmp_state;
	SRC_DATA   rsmp_data;

public:

	SampleChannel(bool inputMonitor);
	~SampleChannel();

	void copy(const Channel* src, pthread_mutex_t* pluginMutex) override;
	void fillBuffer() override;
	void parseEvents(giada::m::mixer::FrameEvents fe, size_t index) override;
	void process(giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in) override;
	void start(int frame, bool doQuantize, int quantize, bool mixerIsRunning,
		bool forceStart, bool isUserGenerated, bool record, int velocity) override;
	void kill(int frame) override;
	void manualKill() override;
	void startReadingActions(bool treatRecsAsLoops, bool recsStopOnChanHalt) override;
	void stopReadingActions(bool isClockRunning, bool treatRecsAsLoops, 
		bool recsStopOnChanHalt) override;
	void empty() override;
	void stopBySeq(bool chansStopOnSeqHalt) override;
	void rewindBySeq() override;
	void stop(bool isUserGenerated) override;
	void setMute(bool isUserGenerated) override;
	void unsetMute(bool isUserGenerated) override;
  void readPatch(const std::string& basePath, int i) override;
	void writePatch(int i, bool isProject) override;
	bool canInputRec() override;
	void stopInputRec(int globalFrame, int quantize, bool mixerIsRunning) override;
	bool allocBuffers(int bufferSize) override;

	float getBoost() const;	
	int   getBegin() const;
	int   getEnd() const;
	float getPitch() const;

	/* fillBuffer
	Fills 'dest' buffer at point 'offset' with Wave data taken from 'start'. If 
	doRewind=false don't rewind internal tracker. Returns new sample position, 
	in frames. It resamples data if pitch != 1.0f. */

	int fillBuffer(giada::m::AudioBuffer& dest, int start, int offset, bool doRewind);

	/* reset
	Rewinds tracker to the beginning of the sample. */

	void reset(int frame);

	/* pushWave
	Adds a new wave to an existing channel. */

	void pushWave(Wave* w);

	/* getPosition
	Returns the position of an active sample. If EMPTY o MISSING returns -1. */

	int getPosition();

	void setPitch(float v);
	void setBegin(int f);
	void setEnd(int f);
	void setBoost(float v);

	void setReadActions(bool v, bool recsStopOnChanHalt);

	/* onPreviewEnd
	A callback fired when audio preview ends. */

	std::function<void()> onPreviewEnd;

	/* bufferPreview
	Extra buffer for audio preview. */

	giada::m::AudioBuffer bufferPreview;
	
	Wave* wave;
	int   tracker;         // chan position
	int   trackerPreview;  // chan position for audio preview
	int   shift;
	int   mode;            // mode: see const.h
	bool  qWait;           // quantizer wait
  bool  inputMonitor;  
	float boost;
	float pitch;

	/* begin, end
	Begin/end point to read wave data from/to. */

	int begin;
	int end;

	/* frameRewind
	Exact frame in which a rewind occurs. */

	int frameRewind;

	/* midi stuff */

  bool     midiInVeloAsVol;
  uint32_t midiInReadActions;
  uint32_t midiInPitch;
};

#endif
