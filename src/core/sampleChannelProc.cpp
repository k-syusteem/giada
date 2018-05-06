#include <cassert>
#include <samplerate.h>
#include "../utils/math.h"
#include "../glue/channel.h"
#include "const.h"
#include "conf.h"
#include "wave.h"
#include "clock.h"
#include "kernelAudio.h"
#include "pluginHost.h"
#include "sampleChannel.h"
#include "sampleChannelProc.h"


namespace giada {
namespace m {
namespace sampleChannelProc
{
namespace
{
void rewind_(SampleChannel* ch, int localFrame)
{
	ch->tracker = ch->begin;
	ch->mute_i  = false;
	ch->qWait   = false;  // Was in qWait mode? Reset occured, no more qWait now.

	/* On reset, if localFrame > 0 and channel is playing, fill again buffer to 
	create something like this:

		|abcdefabcdefab*abcdefabcde|
		[old data-----]*[new data--] */

	if (localFrame > 0 && ch->isPlaying())
		ch->tracker = ch->fillBuffer(ch->buffer, ch->tracker, localFrame, false); 
}


/* -------------------------------------------------------------------------- */


/* quantize
Starts channel according to quantizer. Index = array index of mixer::channels 
used by recorder, localFrame = frame within the current buffer, 
globalFrame = frame within the whole sequencer loop.  */

void quantize_(SampleChannel* ch, int index, int localFrame, int globalFrame)
{
	/* skip if LOOP_ANY or not in quantizer-wait mode */

	if ((ch->mode & LOOP_ANY) || !ch->qWait)
		return;

	/* no fadeout if the sample starts for the first time (from a
	 * STATUS_OFF), it would be meaningless. */

	if (ch->status == STATUS_OFF) {
		ch->status  = STATUS_PLAY;
		ch->sendMidiLplay(); /* MIDI TODO ********** */
		ch->qWait   = false;
		ch->tracker = ch->fillBuffer(ch->buffer, ch->tracker, localFrame, true);  /// FIXME: ???
	}
	else
		rewind_(ch, localFrame);

	/* Now we record the keypress, if the quantizer is on. SINGLE_PRESS needs 
	overdub. */

	if (recorder::canRec(ch, clock::isRunning(), mixer::recording)) {
		if (ch->mode == SINGLE_PRESS) {
			recorder::startOverdub(index, G_ACTION_KEYS, globalFrame, 
				kernelAudio::getRealBufSize());
			ch->readActions = false;   // don't read actions while overdubbing
		}
		else
			recorder::rec(index, G_ACTION_KEYPRESS, globalFrame);
		ch->hasActions = true;
	}	
}


/* -------------------------------------------------------------------------- */


/* onBar
Things to do when the sequencer is on a bar. */

void onBar_(SampleChannel* ch, int localFrame)
{
	if (ch->mode == LOOP_REPEAT) {
		if (ch->status == STATUS_PLAY)
			rewind_(ch, localFrame);
	}
	else
	if (ch->mode == LOOP_ONCE_BAR) {
		if (ch->status == STATUS_WAIT) {
			ch->status  = STATUS_PLAY;
			ch->tracker = ch->fillBuffer(ch->buffer, ch->tracker, localFrame, true);
			ch->sendMidiLplay(); /* MIDI TODO ********** */
		}
	}	
}


/* -------------------------------------------------------------------------- */


/* onFirstBeat
Things to do when the sequencer is on the first beat. */

void onFirstBeat_(SampleChannel* ch, int localFrame)
{
	if (ch->wave == nullptr)
		return;

	if (ch->mode & LOOP_ANY) {
		if (ch->status == STATUS_PLAY)
			rewind_(ch, localFrame);
		else
		if (ch->status == STATUS_ENDING)
			kill(ch, localFrame);
	}

	if (ch->status == STATUS_WAIT) { /// FIXME - should be inside previous if!
		ch->status  = STATUS_PLAY;
		ch->sendMidiLplay(); /* MIDI TODO ********** */
		ch->tracker = ch->fillBuffer(ch->buffer, ch->tracker, localFrame, true);
	}

	if (ch->recStatus == REC_ENDING) {
		ch->recStatus = REC_STOPPED;
		setReadActions(ch, false, conf::recsStopOnChanHalt);  // rec stop
	}
	else
	if (ch->recStatus == REC_WAITING) {
		ch->recStatus = REC_READING;
		setReadActions(ch, true, conf::recsStopOnChanHalt);   // rec start
	}	
}


/* -------------------------------------------------------------------------- */


/* processFrameOnEnd
Things to do when the sample has reached the end (i.e. last frame). */

void processFrameOnEnd_(SampleChannel* ch, int localFrame, bool isClockRunning)
{
	/* SINGLE_ENDLESS runs forever unless it's in ENDING mode. */

	if (ch->mode & (SINGLE_BASIC | SINGLE_PRESS | SINGLE_RETRIG) ||
		 (ch->mode == SINGLE_ENDLESS && ch->status == STATUS_ENDING)   ||
		 (ch->mode & LOOP_ANY && !isClockRunning))     // stop loops when the seq is off
	{
		ch->status = STATUS_OFF;
		ch->sendMidiLplay();/* MIDI TODO ********** */
	}

	/* LOOP_ONCE or LOOP_ONCE_BAR: if ending (i.e. the user requested their
	 * termination), kill 'em. Let them wait otherwise. But don't put back in
	 * wait mode those already stopped by the conditionals above. */

	if (ch->mode & (LOOP_ONCE | LOOP_ONCE_BAR)) {
		if (ch->status == STATUS_ENDING)
			ch->status = STATUS_OFF;
		else
		if (ch->status != STATUS_OFF)
			ch->status = STATUS_WAIT;
	}

	rewind_(ch, localFrame);
}


/* -------------------------------------------------------------------------- */


/* processFrameOnPlay
Things to do when the sample is playing. */

void processFrameOnPlay_(SampleChannel* ch, int localFrame, bool isClockRunning)
{
	if (isClockRunning) 
		ch->calcVolumeEnvelope();
	for (int i=0; i<ch->buffer.countChannels(); i++)
		ch->buffer[localFrame][i] *= (ch->mute || ch->mute_i) ? 0.0f : ch->volume_i;
}


/* -------------------------------------------------------------------------- */


/* calcVolumeEnv
Computes any changes in volume done via envelope tool. */

void calcVolumeEnv_(SampleChannel* ch, int globalFrame)
{
	/* method: check this frame && next frame, then calculate delta */

	recorder::action* a0 = nullptr;
	recorder::action* a1 = nullptr;
	int res;

	/* get this action on frame 'frame'. It's unlikely that the action
	 * is not found. */

	res = recorder::getAction(ch->index, G_ACTION_VOLUME, globalFrame, &a0);
	if (res == 0)
		return;

	/* get the action next to this one.
	 * res == -1: a1 not found, this is the last one. Rewind the search
	 * and use action at frame number 0 (actions[0]).
	 * res == -2 G_ACTION_VOLUME not found. This should never happen */

	res = recorder::getNextAction(ch->index, G_ACTION_VOLUME, globalFrame, &a1);

	if (res == -1)
		res = recorder::getAction(ch->index, G_ACTION_VOLUME, 0, &a1);

	ch->volume_i = a0->fValue;
	ch->volume_d = ((a1->fValue - a0->fValue) / (a1->frame - a0->frame)) * 1.003f;
}


/* -------------------------------------------------------------------------- */


void parseAction_(SampleChannel* ch, const recorder::action* a, int localFrame, 
	int globalFrame)
{
	if (!ch->readActions)
		return;

	switch (a->type) {
		case G_ACTION_KEYPRESS:
			if (ch->mode & SINGLE_ANY)
				start(ch, localFrame, false, false, false, false, 0);
			break;
		case G_ACTION_KEYREL:
			if (ch->mode & SINGLE_ANY)
				stop(ch, false);
			break;
		case G_ACTION_KILL:
			if (ch->mode & SINGLE_ANY)
				kill(ch, localFrame);
			break;
		case G_ACTION_MUTEON:
			setMute(ch, true);   // internal mute
			break;
		case G_ACTION_MUTEOFF:
			unsetMute(ch, true); // internal mute
			break;
		case G_ACTION_VOLUME:
			calcVolumeEnv_(ch, globalFrame);
			break;
	}
}


/* -------------------------------------------------------------------------- */


void processFrame_(SampleChannel* ch, int localFrame, bool isClockRunning)
{
	assert(localFrame < ch->buffer.countFrames());

	if (ch->wave == nullptr || !ch->isPlaying())
		return;

	if (localFrame != ch->frameRewind)
		processFrameOnPlay_(ch, localFrame, isClockRunning);
	else 
		processFrameOnEnd_(ch, localFrame, isClockRunning);
}
}; // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void setReadActions(SampleChannel* ch, bool v, bool recsStopOnChanHalt)
{
	ch->readActions = v;
	if (!ch->readActions && recsStopOnChanHalt)
		kill(ch, 0); // FIXME - wrong frame value
}


/* -------------------------------------------------------------------------- */


void empty(SampleChannel* ch)
{
	ch->sendMidiLplay();/* MIDI TODO ********** */
}


/* -------------------------------------------------------------------------- */


void kill(SampleChannel* ch, int localFrame)
{
	if (ch->status == STATUS_OFF)
		return;
	if (localFrame != 0)
		ch->buffer.clear(localFrame); // clear data in range [localFrame, [end]]

	ch->status = STATUS_OFF;
	ch->sendMidiLplay();/* MIDI TODO ********** */
	rewind_(ch, localFrame);
}


/* -------------------------------------------------------------------------- */


void manualKill(SampleChannel* ch)
{
	/* action recording on:
			if sequencer is running, rec a killchan
		 action recording off:
			if chan has recorded events:
			|	 if seq is playing OR channel 'c' is stopped, de/activate recs
			|	 else kill chan
			else kill chan. */

	if (m::recorder::active) {
		if (!m::clock::isRunning()) 
			return;
		kill(ch, 0); // on frame 0: user-generated event
		if (m::recorder::canRec(ch, m::clock::isRunning(), m::mixer::recording) &&
				!(ch->mode & LOOP_ANY))
		{   // don't record killChan actions for LOOP channels
			m::recorder::rec(ch->index, G_ACTION_KILL, m::clock::getCurrentFrame());
			ch->hasActions = true;
		}
	}
	else {
		if (ch->hasActions) {
			if (m::clock::isRunning() || ch->status == STATUS_OFF)
				ch->readActions ? c::channel::stopReadingActions(ch) : c::channel::startReadingActions(ch); // TODO !!!
			else
				kill(ch, 0);  // on frame 0: user-generated event
		}
		else
			kill(ch, 0);    // on frame 0: user-generated event
	}
}


/* -------------------------------------------------------------------------- */


void startReadingActions(SampleChannel* ch, bool treatRecsAsLoops, bool recsStopOnChanHalt)
{
	if (treatRecsAsLoops)
		ch->recStatus = REC_WAITING;
	else
		setReadActions(ch, true, recsStopOnChanHalt);
}


/* -------------------------------------------------------------------------- */


void stopReadingActions(SampleChannel* ch, bool isClockRunning, bool treatRecsAsLoops, 
		bool recsStopOnChanHalt)
{
	/* First of all, if the clock is not running just stop and disable everything.
	Then if "treatRecsAsLoop" wait until the sequencer reaches beat 0, so put the
	channel in REC_ENDING status. */

	if (!isClockRunning) {
		ch->recStatus = REC_STOPPED;
		setReadActions(ch, false, false);
	}
	else
	if (ch->recStatus == REC_WAITING)
		ch->recStatus = REC_STOPPED;
	else
	if (ch->recStatus == REC_ENDING)
		ch->recStatus = REC_READING;
	else
	if (treatRecsAsLoops)
		ch->recStatus = REC_ENDING;
	else
		setReadActions(ch, false, recsStopOnChanHalt);
}


/* -------------------------------------------------------------------------- */


void stop(SampleChannel* ch, bool isUserGenerated)
{
	if (ch->mode == SINGLE_PRESS && ch->status == STATUS_PLAY)
		kill(ch, 0);  /// FIXME - wrong frame value
	else  // stop a SINGLE_PRESS immediately, if the quantizer is on
	if (ch->mode == SINGLE_PRESS && ch->qWait == true)
		ch->qWait = false;

	if (isUserGenerated) {
		/* record a key release only if channel is single_press. For any
		 * other mode the KEY REL is meaningless. */
		if (ch->mode == SINGLE_PRESS && recorder::canRec(ch, clock::isRunning(), mixer::recording))
			recorder::stopOverdub(clock::getCurrentFrame(), clock::getFramesInLoop(),
				&mixer::mutex);
	}
}


/* -------------------------------------------------------------------------- */


void stopBySeq(SampleChannel* ch, bool chansStopOnSeqHalt)
{
	/* Loop-mode samples in wait status get stopped right away. */

	if (ch->mode & LOOP_ANY && ch->status == STATUS_WAIT) {
		ch->status = STATUS_OFF;
		return;
	}

	/* When to kill samples on StopSeq:
	 *  - when chansStopOnSeqHalt == true (run the sample to end otherwise)
	 *  - when a channel has recs in play (1)
	 *
	 * Always kill at frame=0, this is a user-generated event.
	 *
	 * (1) a channel has recs in play when:
	 *  - Recorder has events for that channel
	 *  - G_Mixer has at least one sample in play
	 *  - Recorder's channel is active (altrimenti può capitare che si stoppino i
	 *    sample suonati manualmente in un canale con rec disattivate) */

	if (chansStopOnSeqHalt) {
		if ((ch->mode & LOOP_ANY) || (ch->hasActions && ch->readActions && ch->status == STATUS_PLAY))
			kill(ch, 0);
	}	
}


/* -------------------------------------------------------------------------- */


void rewindBySeq(SampleChannel* ch)
{
	/* rewind LOOP_ANY or SINGLE_ANY only if it's in read-record-mode */

	if (ch->wave != nullptr) {
		if ((ch->mode & LOOP_ANY) || (ch->recStatus == REC_READING && (ch->mode & SINGLE_ANY)))
			rewind_(ch, 0);  // rewind is user-generated events, always on frame 0
	}	
}


/* -------------------------------------------------------------------------- */


void setMute(SampleChannel* ch, bool isUserGenerated)
{
	isUserGenerated ? ch->mute = true : ch->mute_i = true;
	ch->sendMidiLmute(); /* MIDI TODO ********** */
}


/* -------------------------------------------------------------------------- */


void unsetMute(SampleChannel* ch, bool isUserGenerated)
{
	isUserGenerated ? ch->mute = false : ch->mute_i = false;
	ch->sendMidiLmute(); /* MIDI TODO ********** */
}


/* -------------------------------------------------------------------------- */


/* TODO join doQuantize, forceStart into one parameter */
void start(SampleChannel* ch, int localFrame, bool doQuantize, bool forceStart, 
	bool isUserGenerated, bool record, int velocity)
{
	if (record) {
		/* Record now if the quantizer is off, otherwise let mixer to handle it when a
		quantoWait has passed. Moreover, KEYPRESS and KEYREL are meaningless for loop 
		modes. */
		if (m::clock::getQuantize() == 0 &&
				m::recorder::canRec(ch, m::clock::isRunning(), m::mixer::recording) &&
				!(ch->mode & LOOP_ANY))
		{
			if (ch->mode == SINGLE_PRESS) {
				m::recorder::startOverdub(ch->index, G_ACTION_KEYS, m::clock::getCurrentFrame(),
					m::kernelAudio::getRealBufSize());
				ch->readActions = false;   // don't read actions while overdubbing
			}
			else {
				m::recorder::rec(ch->index, G_ACTION_KEYPRESS, m::clock::getCurrentFrame());
				ch->hasActions = true;

				/* Why return here? You record an action and then you call ch->start: 
				Mixer, which is on another thread, reads your newly recorded action if you 
				have readActions == true, and then ch->start kicks in right after it.
				The result: Mixer plays the channel (due to the new action) but the code
				in the switch below  kills it right away (because the sample is playing). 
				Fix: start channel only if you are not recording anything, i.e. let 
				Mixer play it. */

				if (ch->readActions)
					return;
			}
		}
	}

	if (velocity != 0) {
		/* For one-shot modes, velocity drives the internal volume. */
		if (ch->mode & SINGLE_ANY && ch->midiInVeloAsVol)
			ch->setVolumeI(u::math::map((float)velocity, 0.0f, 127.0f, 0.0f, 1.0f));		
	}

	switch (ch->status)	{
		case STATUS_EMPTY:
		case STATUS_MISSING:
		case STATUS_WRONG:
		{
			return;
		}
		case STATUS_OFF:
		{
			if (ch->mode & LOOP_ANY) {
				if (forceStart) {
					ch->status  = STATUS_PLAY;
					ch->tracker = localFrame;
				}
				else
					ch->status = STATUS_WAIT;
				ch->sendMidiLplay();   /* MIDI TODO ********** */
			}
			else {
				if (clock::getQuantize() > 0 && clock::isRunning() && doQuantize)
					ch->qWait = true;
				else {
					ch->status = STATUS_PLAY;
					ch->sendMidiLplay();   /* MIDI TODO ********** */

					/* Do fillBuffer only if this is not a user-generated event (i.e. is an
					action read by Mixer). Otherwise clear() will take take of calling
					fillBuffer on the next cycle. */

					if (!isUserGenerated)
						ch->tracker = ch->fillBuffer(ch->buffer, ch->tracker, localFrame, true);
				}
			}
			break;
		}
		case STATUS_PLAY:
		{
			if (ch->mode == SINGLE_RETRIG) {
				if (clock::getQuantize() > 0 && clock::isRunning() && doQuantize)
					ch->qWait = true;
				else
					rewind_(ch, localFrame);
			}
			else
			if (ch->mode & (LOOP_ANY | SINGLE_ENDLESS)) {
				ch->status = STATUS_ENDING;
				ch->sendMidiLplay();   /* MIDI TODO ********************/
			}
			break;
		}
		case STATUS_WAIT:
		{
			ch->status = STATUS_OFF;
			ch->sendMidiLplay();   /* MIDI TODO ********************/
			break;
		}
		case STATUS_ENDING:
		{
			ch->status = STATUS_PLAY;
			ch->sendMidiLplay();   /* MIDI TODO ********************/
			break;
		}
	}
}


/* -------------------------------------------------------------------------- */


void fillBuffer(SampleChannel* ch)
{
	ch->buffer.clear();
	if (ch->isPlaying())
		ch->tracker = ch->fillBuffer(ch->buffer, ch->tracker, 0, true);
}


/* -------------------------------------------------------------------------- */


void parseEvents(SampleChannel* ch, mixer::FrameEvents fe, size_t chanIndex)
{
	quantize_(ch, chanIndex, fe.frameLocal, fe.frameGlobal);
	if (fe.clockRunning) {
		if (fe.onBar)
			onBar_(ch, fe.frameLocal);
		if (fe.onFirstBeat)
			onFirstBeat_(ch, fe.frameLocal);
		for (const recorder::action* action : fe.actions)
			if (action->chan == ch->index)
				parseAction_(ch, action, fe.frameLocal, fe.frameGlobal);
	}
	processFrame_(ch, fe.frameLocal, fe.clockRunning);
}


/* -------------------------------------------------------------------------- */


void process(SampleChannel* ch, m::AudioBuffer& out, const m::AudioBuffer& in)
{
	/* normal play */
	/* normal play */
	/* normal play */
	if (mixer::isChannelAudible(ch))
	{
		assert(out.countSamples() == ch->buffer.countSamples());
		assert(in.countSamples()  == ch->buffer.countSamples());

		/* If armed and inbuffer is not nullptr (i.e. input device available) and
		input monitor is on, copy input buffer to vChan: this enables the input
		monitoring. The vChan will be overwritten later by pluginHost::processStack,
		so that you would record "clean" audio (i.e. not plugin-processed). */

		if (ch->armed && in.isAllocd() && ch->inputMonitor)
			for (int i=0; i<ch->buffer.countFrames(); i++)
				for (int j=0; j<ch->buffer.countChannels(); j++)
					ch->buffer[i][j] += in[i][j];   // add, don't overwrite

	#ifdef WITH_VST
		pluginHost::processStack(ch->buffer, pluginHost::CHANNEL, ch);
	#endif

			for (int i=0; i<out.countFrames(); i++)
				for (int j=0; j<out.countChannels(); j++)
					out[i][j] += ch->buffer[i][j] * ch->volume * ch->calcPanning(j) * ch->boost;
	}
	/* normal play */
	/* normal play */
	/* normal play */



	/* preview */
	/* preview */
	/* preview */


	if (ch->previewMode != G_PREVIEW_NONE) {
		ch->bufferPreview.clear();

		/* If the tracker exceedes the end point and preview is looped, split the 
		rendering as in SampleChannel::reset(). */

		if (ch->trackerPreview + ch->bufferPreview.countFrames() >= ch->end) {
			int offset = ch->end - ch->trackerPreview;
			ch->trackerPreview = ch->fillBuffer(ch->bufferPreview, ch->trackerPreview, 0, false);
			ch->trackerPreview = ch->begin;
			if (ch->previewMode == G_PREVIEW_LOOP)
				ch->trackerPreview = ch->fillBuffer(ch->bufferPreview, ch->begin, offset, false);
			else
			if (ch->previewMode == G_PREVIEW_NORMAL) {
				ch->previewMode = G_PREVIEW_NONE;
				if (ch->onPreviewEnd)
					ch->onPreviewEnd();
			}
		}
		else
			ch->trackerPreview = ch->fillBuffer(ch->bufferPreview, ch->trackerPreview, 0, false);

		for (int i=0; i<out.countFrames(); i++)
			for (int j=0; j<out.countChannels(); j++)
				out[i][j] += ch->bufferPreview[i][j] * ch->volume * ch->calcPanning(j) * ch->boost;
	}
	/* preview */	
	/* preview */
	/* preview */
}
}}};