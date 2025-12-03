#include "ProjectSerializer.h"

namespace model {

bool ProjectSerializer::save(const Project& project, const juce::File& file)
{
    auto json = toJson(project);

    // Save as plain JSON (human-readable, editable)
    return file.replaceWithText(json);
}

bool ProjectSerializer::load(Project& project, const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    auto json = file.loadFileAsString();
    return fromJson(project, json);
}

juce::String ProjectSerializer::toJson(const Project& project)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    root->setProperty("version", "1.0");
    root->setProperty("name", juce::String(project.getName()));
    root->setProperty("tempo", project.getTempo());
    root->setProperty("groove", juce::String(project.getGrooveTemplate()));

    // Instruments
    juce::Array<juce::var> instruments;
    for (int i = 0; i < project.getInstrumentCount(); ++i)
    {
        instruments.add(instrumentToVar(*project.getInstrument(i)));
    }
    root->setProperty("instruments", instruments);

    // Patterns
    juce::Array<juce::var> patterns;
    for (int i = 0; i < project.getPatternCount(); ++i)
    {
        patterns.add(patternToVar(*project.getPattern(i)));
    }
    root->setProperty("patterns", patterns);

    // Chains
    juce::Array<juce::var> chains;
    for (int i = 0; i < project.getChainCount(); ++i)
    {
        chains.add(chainToVar(*project.getChain(i)));
    }
    root->setProperty("chains", chains);

    // Song
    root->setProperty("song", songToVar(project.getSong()));

    // Mixer
    juce::DynamicObject::Ptr mixer = new juce::DynamicObject();
    const auto& m = project.getMixer();

    juce::Array<juce::var> trackVols, trackPans, trackMutes, trackSolos;
    for (int i = 0; i < 16; ++i)
    {
        trackVols.add(m.trackVolumes[i]);
        trackPans.add(m.trackPans[i]);
        trackMutes.add(m.trackMutes[i]);
        trackSolos.add(m.trackSolos[i]);
    }
    mixer->setProperty("trackVolumes", trackVols);
    mixer->setProperty("trackPans", trackPans);
    mixer->setProperty("trackMutes", trackMutes);
    mixer->setProperty("trackSolos", trackSolos);
    mixer->setProperty("masterVolume", m.masterVolume);
    root->setProperty("mixer", juce::var(mixer.get()));

    return juce::JSON::toString(juce::var(root.get()));
}

bool ProjectSerializer::fromJson(Project& project, const juce::String& json)
{
    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) return false;

    auto* obj = parsed.getDynamicObject();
    if (!obj) return false;

    project.setName(obj->getProperty("name").toString().toStdString());
    project.setTempo(static_cast<float>(obj->getProperty("tempo")));
    project.setGrooveTemplate(obj->getProperty("groove").toString().toStdString());

    // Load instruments
    auto* instrumentsArray = obj->getProperty("instruments").getArray();
    if (instrumentsArray)
    {
        for (int i = 0; i < instrumentsArray->size(); ++i)
        {
            // Ensure we have enough instruments
            while (project.getInstrumentCount() <= i)
                project.addInstrument();

            varToInstrument(*project.getInstrument(i), (*instrumentsArray)[i]);
        }
    }

    // Load patterns
    auto* patternsArray = obj->getProperty("patterns").getArray();
    if (patternsArray)
    {
        for (int i = 0; i < patternsArray->size(); ++i)
        {
            while (project.getPatternCount() <= i)
                project.addPattern();

            varToPattern(*project.getPattern(i), (*patternsArray)[i]);
        }
    }

    // Load chains
    auto* chainsArray = obj->getProperty("chains").getArray();
    if (chainsArray)
    {
        for (int i = 0; i < chainsArray->size(); ++i)
        {
            while (project.getChainCount() <= i)
                project.addChain();

            varToChain(*project.getChain(i), (*chainsArray)[i]);
        }
    }

    // Load song
    auto songVar = obj->getProperty("song");
    if (songVar.isObject())
    {
        varToSong(project.getSong(), songVar);
    }

    // Load mixer
    auto mixerVar = obj->getProperty("mixer");
    if (auto* mixerObj = mixerVar.getDynamicObject())
    {
        auto& m = project.getMixer();

        if (auto* vols = mixerObj->getProperty("trackVolumes").getArray())
        {
            for (int i = 0; i < std::min(16, vols->size()); ++i)
                m.trackVolumes[i] = static_cast<float>((*vols)[i]);
        }
        if (auto* pans = mixerObj->getProperty("trackPans").getArray())
        {
            for (int i = 0; i < std::min(16, pans->size()); ++i)
                m.trackPans[i] = static_cast<float>((*pans)[i]);
        }
        if (auto* mutes = mixerObj->getProperty("trackMutes").getArray())
        {
            for (int i = 0; i < std::min(16, mutes->size()); ++i)
                m.trackMutes[i] = static_cast<bool>((*mutes)[i]);
        }
        if (auto* solos = mixerObj->getProperty("trackSolos").getArray())
        {
            for (int i = 0; i < std::min(16, solos->size()); ++i)
                m.trackSolos[i] = static_cast<bool>((*solos)[i]);
        }
        m.masterVolume = static_cast<float>(mixerObj->getProperty("masterVolume"));
    }

    return true;
}

juce::var ProjectSerializer::instrumentToVar(const Instrument& inst)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(inst.getName()));

    const auto& p = inst.getParams();
    obj->setProperty("engine", p.engine);
    obj->setProperty("harmonics", p.harmonics);
    obj->setProperty("timbre", p.timbre);
    obj->setProperty("morph", p.morph);
    obj->setProperty("attack", p.attack);
    obj->setProperty("decay", p.decay);
    obj->setProperty("lpgColour", p.lpgColour);
    obj->setProperty("polyphony", p.polyphony);

    // Filter
    juce::DynamicObject::Ptr filter = new juce::DynamicObject();
    filter->setProperty("cutoff", p.filter.cutoff);
    filter->setProperty("resonance", p.filter.resonance);
    obj->setProperty("filter", juce::var(filter.get()));

    // LFO1
    juce::DynamicObject::Ptr lfo1 = new juce::DynamicObject();
    lfo1->setProperty("rate", p.lfo1.rate);
    lfo1->setProperty("shape", p.lfo1.shape);
    lfo1->setProperty("dest", p.lfo1.dest);
    lfo1->setProperty("amount", p.lfo1.amount);
    obj->setProperty("lfo1", juce::var(lfo1.get()));

    // LFO2
    juce::DynamicObject::Ptr lfo2 = new juce::DynamicObject();
    lfo2->setProperty("rate", p.lfo2.rate);
    lfo2->setProperty("shape", p.lfo2.shape);
    lfo2->setProperty("dest", p.lfo2.dest);
    lfo2->setProperty("amount", p.lfo2.amount);
    obj->setProperty("lfo2", juce::var(lfo2.get()));

    // ENV1
    juce::DynamicObject::Ptr env1 = new juce::DynamicObject();
    env1->setProperty("attack", p.env1.attack);
    env1->setProperty("decay", p.env1.decay);
    env1->setProperty("dest", p.env1.dest);
    env1->setProperty("amount", p.env1.amount);
    obj->setProperty("env1", juce::var(env1.get()));

    // ENV2
    juce::DynamicObject::Ptr env2 = new juce::DynamicObject();
    env2->setProperty("attack", p.env2.attack);
    env2->setProperty("decay", p.env2.decay);
    env2->setProperty("dest", p.env2.dest);
    env2->setProperty("amount", p.env2.amount);
    obj->setProperty("env2", juce::var(env2.get()));

    const auto& s = inst.getSends();
    juce::DynamicObject::Ptr sends = new juce::DynamicObject();
    sends->setProperty("reverb", s.reverb);
    sends->setProperty("delay", s.delay);
    sends->setProperty("chorus", s.chorus);
    sends->setProperty("drive", s.drive);
    sends->setProperty("sidechainDuck", s.sidechainDuck);
    obj->setProperty("sends", juce::var(sends.get()));

    // Per-instrument mixer controls
    obj->setProperty("volume", inst.getVolume());
    obj->setProperty("pan", inst.getPan());
    obj->setProperty("muted", inst.isMuted());
    obj->setProperty("soloed", inst.isSoloed());

    return juce::var(obj.get());
}

void ProjectSerializer::varToInstrument(Instrument& inst, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    inst.setName(obj->getProperty("name").toString().toStdString());

    auto& p = inst.getParams();
    p.engine = static_cast<int>(obj->getProperty("engine"));
    p.harmonics = static_cast<float>(obj->getProperty("harmonics"));
    p.timbre = static_cast<float>(obj->getProperty("timbre"));
    p.morph = static_cast<float>(obj->getProperty("morph"));
    p.attack = static_cast<float>(obj->getProperty("attack"));
    p.decay = static_cast<float>(obj->getProperty("decay"));
    p.lpgColour = static_cast<float>(obj->getProperty("lpgColour"));

    // Polyphony (with default for old files)
    if (obj->hasProperty("polyphony"))
        p.polyphony = static_cast<int>(obj->getProperty("polyphony"));

    // Filter
    auto filterVar = obj->getProperty("filter");
    if (auto* filterObj = filterVar.getDynamicObject())
    {
        p.filter.cutoff = static_cast<float>(filterObj->getProperty("cutoff"));
        p.filter.resonance = static_cast<float>(filterObj->getProperty("resonance"));
    }

    // LFO1
    auto lfo1Var = obj->getProperty("lfo1");
    if (auto* lfo1Obj = lfo1Var.getDynamicObject())
    {
        p.lfo1.rate = static_cast<int>(lfo1Obj->getProperty("rate"));
        p.lfo1.shape = static_cast<int>(lfo1Obj->getProperty("shape"));
        p.lfo1.dest = static_cast<int>(lfo1Obj->getProperty("dest"));
        p.lfo1.amount = static_cast<int>(lfo1Obj->getProperty("amount"));
    }

    // LFO2
    auto lfo2Var = obj->getProperty("lfo2");
    if (auto* lfo2Obj = lfo2Var.getDynamicObject())
    {
        p.lfo2.rate = static_cast<int>(lfo2Obj->getProperty("rate"));
        p.lfo2.shape = static_cast<int>(lfo2Obj->getProperty("shape"));
        p.lfo2.dest = static_cast<int>(lfo2Obj->getProperty("dest"));
        p.lfo2.amount = static_cast<int>(lfo2Obj->getProperty("amount"));
    }

    // ENV1
    auto env1Var = obj->getProperty("env1");
    if (auto* env1Obj = env1Var.getDynamicObject())
    {
        p.env1.attack = static_cast<float>(env1Obj->getProperty("attack"));
        p.env1.decay = static_cast<float>(env1Obj->getProperty("decay"));
        p.env1.dest = static_cast<int>(env1Obj->getProperty("dest"));
        p.env1.amount = static_cast<int>(env1Obj->getProperty("amount"));
    }

    // ENV2
    auto env2Var = obj->getProperty("env2");
    if (auto* env2Obj = env2Var.getDynamicObject())
    {
        p.env2.attack = static_cast<float>(env2Obj->getProperty("attack"));
        p.env2.decay = static_cast<float>(env2Obj->getProperty("decay"));
        p.env2.dest = static_cast<int>(env2Obj->getProperty("dest"));
        p.env2.amount = static_cast<int>(env2Obj->getProperty("amount"));
    }

    auto sendsVar = obj->getProperty("sends");
    if (auto* sendsObj = sendsVar.getDynamicObject())
    {
        auto& s = inst.getSends();
        s.reverb = static_cast<float>(sendsObj->getProperty("reverb"));
        s.delay = static_cast<float>(sendsObj->getProperty("delay"));
        s.chorus = static_cast<float>(sendsObj->getProperty("chorus"));
        s.drive = static_cast<float>(sendsObj->getProperty("drive"));
        s.sidechainDuck = static_cast<float>(sendsObj->getProperty("sidechainDuck"));
    }

    // Per-instrument mixer controls (defaults for old files)
    if (obj->hasProperty("volume"))
        inst.setVolume(static_cast<float>(obj->getProperty("volume")));
    if (obj->hasProperty("pan"))
        inst.setPan(static_cast<float>(obj->getProperty("pan")));
    if (obj->hasProperty("muted"))
        inst.setMuted(static_cast<bool>(obj->getProperty("muted")));
    if (obj->hasProperty("soloed"))
        inst.setSoloed(static_cast<bool>(obj->getProperty("soloed")));
}

juce::var ProjectSerializer::patternToVar(const Pattern& pattern)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(pattern.getName()));
    obj->setProperty("length", pattern.getLength());

    // Steps - only save non-empty
    juce::Array<juce::var> steps;
    for (int t = 0; t < 16; ++t)
    {
        for (int r = 0; r < pattern.getLength(); ++r)
        {
            const auto& step = pattern.getStep(t, r);
            if (!step.isEmpty())
            {
                juce::DynamicObject::Ptr s = new juce::DynamicObject();
                s->setProperty("t", t);
                s->setProperty("r", r);
                s->setProperty("n", static_cast<int>(step.note));
                s->setProperty("i", static_cast<int>(step.instrument));
                s->setProperty("v", static_cast<int>(step.volume));

                // FX commands
                if (!step.fx1.isEmpty())
                {
                    s->setProperty("fx1c", static_cast<int>(step.fx1.type));
                    s->setProperty("fx1v", static_cast<int>(step.fx1.value));
                }
                if (!step.fx2.isEmpty())
                {
                    s->setProperty("fx2c", static_cast<int>(step.fx2.type));
                    s->setProperty("fx2v", static_cast<int>(step.fx2.value));
                }
                if (!step.fx3.isEmpty())
                {
                    s->setProperty("fx3c", static_cast<int>(step.fx3.type));
                    s->setProperty("fx3v", static_cast<int>(step.fx3.value));
                }

                steps.add(juce::var(s.get()));
            }
        }
    }
    obj->setProperty("steps", steps);

    return juce::var(obj.get());
}

void ProjectSerializer::varToPattern(Pattern& pattern, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    pattern.setName(obj->getProperty("name").toString().toStdString());
    pattern.setLength(static_cast<int>(obj->getProperty("length")));

    // Clear pattern first
    pattern.clear();

    // Load steps
    auto* stepsArray = obj->getProperty("steps").getArray();
    if (stepsArray)
    {
        for (const auto& stepVar : *stepsArray)
        {
            auto* stepObj = stepVar.getDynamicObject();
            if (!stepObj) continue;

            int t = static_cast<int>(stepObj->getProperty("t"));
            int r = static_cast<int>(stepObj->getProperty("r"));

            if (t >= 0 && t < 16 && r >= 0 && r < pattern.getLength())
            {
                auto& step = pattern.getStep(t, r);
                step.note = static_cast<int8_t>(static_cast<int>(stepObj->getProperty("n")));
                step.instrument = static_cast<int16_t>(static_cast<int>(stepObj->getProperty("i")));
                step.volume = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("v")));

                if (stepObj->hasProperty("fx1c"))
                {
                    step.fx1.type = static_cast<FXType>(static_cast<int>(stepObj->getProperty("fx1c")));
                    step.fx1.value = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx1v")));
                }
                if (stepObj->hasProperty("fx2c"))
                {
                    step.fx2.type = static_cast<FXType>(static_cast<int>(stepObj->getProperty("fx2c")));
                    step.fx2.value = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx2v")));
                }
                if (stepObj->hasProperty("fx3c"))
                {
                    step.fx3.type = static_cast<FXType>(static_cast<int>(stepObj->getProperty("fx3c")));
                    step.fx3.value = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx3v")));
                }
            }
        }
    }
}

juce::var ProjectSerializer::chainToVar(const Chain& chain)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(chain.getName()));
    obj->setProperty("scaleLock", juce::String(chain.getScaleLock()));

    juce::Array<juce::var> patterns;
    for (int idx : chain.getPatternIndices())
    {
        patterns.add(idx);
    }
    obj->setProperty("patterns", patterns);

    return juce::var(obj.get());
}

void ProjectSerializer::varToChain(Chain& chain, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    chain.setName(obj->getProperty("name").toString().toStdString());
    chain.setScaleLock(obj->getProperty("scaleLock").toString().toStdString());

    // Clear and reload patterns
    while (chain.getPatternCount() > 0)
        chain.removePattern(0);

    auto* patternsArray = obj->getProperty("patterns").getArray();
    if (patternsArray)
    {
        for (const auto& p : *patternsArray)
        {
            chain.addPattern(static_cast<int>(p));
        }
    }
}

juce::var ProjectSerializer::songToVar(const Song& song)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // Save each track (column) as an array of chain indices
    juce::Array<juce::var> tracks;
    for (int t = 0; t < Song::NUM_TRACKS; ++t)
    {
        const auto& track = song.getTrack(t);
        juce::Array<juce::var> chainIndices;
        for (int chainIdx : track)
        {
            chainIndices.add(chainIdx);
        }
        tracks.add(chainIndices);
    }
    obj->setProperty("tracks", tracks);

    return juce::var(obj.get());
}

void ProjectSerializer::varToSong(Song& song, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    // Clear existing song data
    song.clear();

    // Load tracks
    auto* tracksArray = obj->getProperty("tracks").getArray();
    if (tracksArray)
    {
        for (int t = 0; t < std::min(Song::NUM_TRACKS, tracksArray->size()); ++t)
        {
            auto* chainIndices = (*tracksArray)[t].getArray();
            if (chainIndices)
            {
                for (int pos = 0; pos < chainIndices->size(); ++pos)
                {
                    int chainIdx = static_cast<int>((*chainIndices)[pos]);
                    song.setChain(t, pos, chainIdx);
                }
            }
        }
    }
}

} // namespace model
