/*
 * Simplified tuning system for Vitracker DX7 engine
 * Only supports standard 12-TET tuning (no microtuning)
 */

#ifndef __SYNTH_TUNING_H
#define __SYNTH_TUNING_H

#include "synth.h"
#include <memory>
#include <string>

class TuningState {
public:
    virtual ~TuningState() { }

    virtual int32_t midinote_to_logfreq(int midinote) = 0;
    virtual bool is_standard_tuning() { return true; }
    virtual int scale_length() { return 12; }
    virtual std::string display_tuning_str() { return "Standard Tuning"; }
};

std::shared_ptr<TuningState> createStandardTuning();

#endif
