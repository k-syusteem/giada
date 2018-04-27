#ifndef G_AUDIO_PROC_H
#define G_AUDIO_PROC_H


#include "mixer.h"


class SampleChannel;


namespace giada {
namespace m {
namespace audioProc
{
void prepare(SampleChannel* ch, mixer::FrameEvents ev);
}}};


#endif