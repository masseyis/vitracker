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

    const auto& s = inst.getSends();
    juce::DynamicObject::Ptr sends = new juce::DynamicObject();
    sends->setProperty("reverb", s.reverb);
    sends->setProperty("delay", s.delay);
    sends->setProperty("chorus", s.chorus);
    sends->setProperty("drive", s.drive);
    sends->setProperty("sidechainDuck", s.sidechainDuck);
    obj->setProperty("sends", juce::var(sends.get()));

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
                    s->setProperty("fx1c", static_cast<int>(step.fx1.command));
                    s->setProperty("fx1v", static_cast<int>(step.fx1.value));
                }
                if (!step.fx2.isEmpty())
                {
                    s->setProperty("fx2c", static_cast<int>(step.fx2.command));
                    s->setProperty("fx2v", static_cast<int>(step.fx2.value));
                }
                if (!step.fx3.isEmpty())
                {
                    s->setProperty("fx3c", static_cast<int>(step.fx3.command));
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
                    step.fx1.command = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx1c")));
                    step.fx1.value = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx1v")));
                }
                if (stepObj->hasProperty("fx2c"))
                {
                    step.fx2.command = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx2c")));
                    step.fx2.value = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx2v")));
                }
                if (stepObj->hasProperty("fx3c"))
                {
                    step.fx3.command = static_cast<uint8_t>(static_cast<int>(stepObj->getProperty("fx3c")));
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

} // namespace model
