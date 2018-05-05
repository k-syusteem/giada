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


#include <cmath>
#include <cstring>
#include <cassert>
#include "../utils/log.h"
#include "../utils/fs.h"
#include "../utils/string.h"
#include "patch.h"
#include "audioProc.h"
#include "channelManager.h"
#include "const.h"
#include "conf.h"
#include "clock.h"
#include "mixer.h"
#include "wave.h"
#include "pluginHost.h"
#include "waveFx.h"
#include "waveManager.h"
#include "mixerHandler.h"
#include "kernelMidi.h"
#include "kernelAudio.h"
#include "sampleChannel.h"


using std::string;
using namespace giada::m;


SampleChannel::SampleChannel(int bufferSize, bool inputMonitor)
	: Channel          (G_CHANNEL_SAMPLE, STATUS_EMPTY, bufferSize),
		rsmp_state       (nullptr),
		wave             (nullptr),
		tracker          (0),
		fadeoutTracker   (0),
		trackerPreview   (0),
		shift            (0),
		mode             (G_DEFAULT_CHANMODE),
		qWait	           (false),
		inputMonitor     (inputMonitor),
		fadeinOn         (false),
		fadeinVol        (1.0f),
		fadeoutOn        (false),
		fadeoutVol       (1.0f),
		fadeoutStep      (G_DEFAULT_FADEOUT_STEP),
		boost            (G_DEFAULT_BOOST),
		pitch            (G_DEFAULT_PITCH),
		begin            (0),
		end              (0),
		frameRewind      (-1),
		midiInReadActions(0x0),
		midiInPitch      (0x0)
{
}


/* -------------------------------------------------------------------------- */


SampleChannel::~SampleChannel()
{
	if (wave != nullptr)
		delete wave;
	if (rsmp_state != nullptr)
		src_delete(rsmp_state);
}


/* -------------------------------------------------------------------------- */


bool SampleChannel::allocBuffers()
{
	if (!Channel::allocBuffers())
		return false;

	rsmp_state = src_new(SRC_LINEAR, G_MAX_IO_CHANS, nullptr);
	if (rsmp_state == nullptr) {
		gu_log("[SampleChannel::allocBuffers] unable to alloc memory for SRC_STATE!\n");
		return false;
	}

	if (!pChan.alloc(bufferSize, G_MAX_IO_CHANS)) {
		gu_log("[SampleChannel::allocBuffers] unable to alloc memory for pChan!\n");
		return false;
	}

	if (!vChanPreview.alloc(bufferSize, G_MAX_IO_CHANS)) {
		gu_log("[SampleChannel::allocBuffers] unable to alloc memory for vChanPreview!\n");
		return false;
	}

	return true;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::copy(const Channel* src_, pthread_mutex_t* pluginMutex)
{
	Channel::copy(src_, pluginMutex);
	const SampleChannel* src = static_cast<const SampleChannel*>(src_);
	tracker         = src->tracker;
	begin           = src->begin;
	end             = src->end;
	boost           = src->boost;
	mode            = src->mode;
	qWait           = src->qWait;
	fadeinOn        = src->fadeinOn;
	fadeinVol       = src->fadeinVol;
	fadeoutOn       = src->fadeoutOn;
	fadeoutVol      = src->fadeoutVol;
	fadeoutTracker  = src->fadeoutTracker;
	fadeoutStep     = src->fadeoutStep;
	fadeoutType     = src->fadeoutType;
	fadeoutEnd      = src->fadeoutEnd;
	setPitch(src->pitch);

	if (src->wave)
		pushWave(new Wave(*src->wave)); // invoke Wave's copy constructor
}


/* -------------------------------------------------------------------------- */


void SampleChannel::parseEvents(mixer::FrameEvents fe, size_t index)
{
	audioProc::parseEvents(this, fe, index);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::fillBuffer()
{
	audioProc::fillBuffer(this);
}



/* -------------------------------------------------------------------------- */


void SampleChannel::rewind()
{
	audioProc::rewind(this);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::stopBySeq(bool chansStopOnSeqHalt)
{
	audioProc::stopBySeq(this, chansStopOnSeqHalt);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::process(giada::m::AudioBuffer& out, const giada::m::AudioBuffer& in)
{
	audioProc::process(this, out, in);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::readPatch(const string& basePath, int i)
{
	Channel::readPatch("", i);
	channelManager::readPatch(this, basePath, i);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::writePatch(int i, bool isProject)
{
	Channel::writePatch(i, isProject);
	channelManager::writePatch(this, isProject, i);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setBegin(int f)
{
	if (f < 0)
		begin = 0;
	else
	if (f > wave->getSize())
		begin = wave->getSize();
	else
	if (f >= end)
		begin = end - 1;
	else
		begin = f;

	tracker = begin;
	trackerPreview = begin;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setEnd(int f)
{
	if (f < 0)
		end = begin + wave->getChannels();
	else
	if (f >= wave->getSize())
		end = wave->getSize() - 1;
	else
	if (f <= begin)
		end = begin + 1;
	else
		end = f;
}


/* -------------------------------------------------------------------------- */


int SampleChannel::getBegin() const { return begin; }
int SampleChannel::getEnd() const   { return end; }


/* -------------------------------------------------------------------------- */


void SampleChannel::setPitch(float v)
{
	if (v > G_MAX_PITCH)
		pitch = G_MAX_PITCH;
	else
	if (v < 0.1f)
		pitch = 0.1000f;
	else 
		pitch = v;

// ???? 	/* if status is off don't slide between frequencies */
// ???? 
// ???? 	if (status & (STATUS_OFF | STATUS_WAIT))
// ???? 		src_set_ratio(rsmp_state, 1/pitch);
}


float SampleChannel::getPitch() const { return pitch; }


/* -------------------------------------------------------------------------- */


int SampleChannel::getPosition()
{
	if (status & ~(STATUS_EMPTY | STATUS_MISSING | STATUS_OFF)) // if is not (...)
		return tracker - begin;
	else
		return -1;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setBoost(float v)
{
	if (v > G_MAX_BOOST_DB)
		boost = G_MAX_BOOST_DB;
	else 
	if (v < 0.0f)
		boost = 0.0f;
	else
		boost = v;
}


float SampleChannel::getBoost() const
{
	return boost;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::calcFadeoutStep()
{
	if (end - tracker < (1 / G_DEFAULT_FADEOUT_STEP))
		fadeoutStep = ceil((end - tracker) / volume); /// or volume_i ???
	else
		fadeoutStep = G_DEFAULT_FADEOUT_STEP;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setFadeIn(bool internal)
{
	if (internal) mute_i = false;  // remove mute before fading in
	else          mute   = false;
	fadeinOn  = true;
	fadeinVol = 0.0f;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setFadeOut(int actionPostFadeout)
{
	calcFadeoutStep();
	fadeoutOn   = true;
	fadeoutVol  = 1.0f;
	fadeoutType = FADEOUT;
	fadeoutEnd	= actionPostFadeout;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setXFade(int frame)
{
	gu_log("[xFade] frame=%d tracker=%d\n", frame, tracker);

	calcFadeoutStep();
	fadeoutOn      = true;
	fadeoutVol     = 1.0f;
	fadeoutType    = XFADE;
	fadeoutTracker = fillChan(pChan, tracker, 0, false);
	reset(frame);
}


/* -------------------------------------------------------------------------- */


void SampleChannel::empty()
{
	status = STATUS_OFF;
	if (wave != nullptr) {
		delete wave;
		wave = nullptr;
	}
  begin   = 0;
  end     = 0;
  tracker = 0;
	status  = STATUS_EMPTY;
  volume  = G_DEFAULT_VOL;
  boost   = G_DEFAULT_BOOST;
	sendMidiLplay();
}


/* -------------------------------------------------------------------------- */


void SampleChannel::pushWave(Wave* w)
{
	sendMidiLplay();     // FIXME - why here?!?!
	wave   = w;
	status = STATUS_OFF;
	begin  = 0;
	end    = wave->getSize() - 1;
	name   = wave->getBasename();
}


/* -------------------------------------------------------------------------- */


bool SampleChannel::canInputRec()
{
	return wave == nullptr && armed;
}


/* -------------------------------------------------------------------------- */


int SampleChannel::fillChan(giada::m::AudioBuffer& dest, int start, int offset, bool rewind)
{
	rsmp_data.data_in       = wave->getFrame(start);    // source data
	rsmp_data.input_frames  = end - start;              // how many readable frames
	rsmp_data.data_out      = dest[offset];             // destination (processed data)
	rsmp_data.output_frames = bufferSize - offset;      // how many frames to process
	rsmp_data.end_of_input  = false;
	rsmp_data.src_ratio     = 1 / pitch;

	src_process(rsmp_state, &rsmp_data);

	int position = start + rsmp_data.input_frames_used; // position goes forward of frames_used (i.e. read from wave)

	if (rewind) {
		int gen = rsmp_data.output_frames_gen;            // frames generated by this call
		if (gen == bufferSize - offset)
			frameRewind = -1;
		else
			frameRewind = gen + offset;
	}
	return position;
}


/* -------------------------------------------------------------------------- */


void SampleChannel::setMute(bool internal)
{
}
void SampleChannel::unsetMute(bool internal)
{
}
void SampleChannel::reset(int frame) // audioProc::rewind
{
}
void SampleChannel::start(int frame, bool doQuantize, int quantize,
		bool mixerIsRunning, bool forceStart, bool isUserGenerated)
{
}
void SampleChannel::kill(int frame)
{
}
void SampleChannel::stop()
{
}