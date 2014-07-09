
//
// AudioEvent.h
// 
// create an FMOD like Event API on top of cAudio/OpenAL
// centralize allocation of sources so we can avoid trying to play too many at once

// Copyright (c) 2014 Arthur Danskin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef AUDIO_EVENT_H
#define AUDIO_EVENT_H

#include "../cAudio/cAudio/include/cAudio.h"

#define AUDIO_FLAGS(F)                               \
    F(STREAM,      1<<0)                             \
    F(LOOP,        1<<1)                             \
    F(ROUND_ROBIN, 1<<2)                             \
    F(MUSIC,       1<<3)                             \
    F(CROSSFADE,   1<<4)                             \
    F(CLUSTER,     1<<5)                             \
    

DEFINE_ENUM(uint64, EnumAudioFlags, AUDIO_FLAGS);

static const uint kStreamSources = 2;
static const uint kSoundSources = 32 - kStreamSources;

inline cAudio::cVector3 c3(float2 v)
{
    return cAudio::cVector3(v.x, v.y, 0.f);
}

#define c3zero cAudio::cVector3(0.f) 

inline float2 c2(const cAudio::cVector3 &v)
{
    return float2(v.x, v.y);
}

static string str_tostr(const cAudio::cVector3& cv3)
{
    return str_format("(%f, %f, %f)", cv3.x, cv3.y, cv3.z);
}

static string sourceToString(cAudio::IAudioSource *src)
{
    string str;
#define PPROP(X) str += #X ": " + str_tostr(src->get ## X ()) + "\n"
#define PPROPis(X) str += #X ": " + str_tostr(src->is ## X ()) + "\n"
    PPROP(Position);
    PPROP(Velocity);
    //PPROP(Direction);
    PPROP(RolloffFactor);
    PPROP(MinDistance);
    PPROP(MaxDistance);
    PPROPis(Relative);
    str += "Total Gain: " + str_tostr(src->calculateGain()) + "\n";
    PPROP(Pitch);
    PPROP(Volume);
    //PPROP(MinVolume);
    //PPROP(MaxVolume);
    //PPROP(DopplerStrength);
    //PPROP(DopplerVelocity);
    PPROP(TotalAudioTime);
    PPROP(CurrentAudioTime);
    PPROPis(Valid);
    PPROPis(Looping);
#undef PPROPis
#undef PPROP
    return str;
}

class AudioAllocator {

    cAudio::IAudioManager                   *m_mgr = NULL;
    vector<pair<cAudio::IAudioSource*, int>> m_sources;
    vector<cAudio::IAudioSource*>            m_streamSources;
    std::map<lstring, cAudio::IAudioBuffer*> m_buffers;
    float2                                   m_lsnrPos;
    
public:

    float calculateGain(float2 pos, float refDist, float maxDist, float rolloff) const
    {
        // OpenAL Inverse Distance Clamped Model
        // distance = max(distance,AL_REFERENCE_DISTANCE);
        // distance = min(distance,AL_MAX_DISTANCE);
        // gain = AL_REFERENCE_DISTANCE / (AL_REFERENCE_DISTANCE +
        //                                 AL_ROLLOFF_FACTOR *
        //                                 (distance – AL_REFERENCE_DISTANCE));

        float dist = distance(pos, m_lsnrPos);
        dist = max(dist, refDist);
        dist = min(dist, maxDist);
        float gain = refDist / (refDist + rolloff * (dist - refDist));
        return gain;
    }

    bool init()
    {
        if (m_mgr)
            return true;
        m_mgr = cAudio::createAudioManager(true);
        if (!m_mgr)
            return false;
        m_mgr->setSpeedOfSound(5000.f);
        m_mgr->setDopplerFactor(1.f);
        return true;
    }
    
    AudioAllocator()
    {
        init();
    }

    ~AudioAllocator()
    {
        releaseAll();
        cAudio::destroyAudioManager(m_mgr);
    }

    static AudioAllocator& instance()
    {
        static AudioAllocator *aa = new AudioAllocator;
        return *aa;
    }

    cAudio::IAudioManager* getMgr() { return m_mgr; }

    void setListener(float2 pos, float2 vel)
    {
        cAudio::IListener *lst = m_mgr->getListener();
        lst->setPosition(c3(pos));
        lst->setVelocity(c3(vel));
        lst->setDirection(cAudio::cVector3(0.f, 0.f, 1.f));
        lst->setDirection(cAudio::cVector3(0.f, 1.f, 0.f));
        m_lsnrPos = pos;
    }

    uint getSourcesUsed() const
    {
        uint sources = m_streamSources.size();
        for (uint i=0; i<m_sources.size(); i++) {
            if (m_sources[i].first->isPlaying())
                sources++;
        }
        return sources;
    }

    uint getSourcesTotal() const
    {
        return m_streamSources.size() + m_sources.size();
    }

    uint getBufferCount() const { return m_buffers.size(); }
    const std::map<lstring, cAudio::IAudioBuffer*>& getBuffers() const { return m_buffers; }

    void releaseAll()
    {
        foreach (auto &src, m_sources) {
            src.first->stop();
            src.first->drop();
        }
        m_sources.clear();

        foreach (cAudio::IAudioSource* src, m_streamSources) {
            src->stop();
            src->drop();
        }
        m_streamSources.clear();

        foreach (auto& x, m_buffers) {
            ASSERT(x.second->getReferenceCount() == 1);
            x.second->drop();
        }
        m_buffers.clear();

        m_mgr->releaseAllSources();
        //cAudio::getLogger()->setLogLevel((globals.debugRender&DBG_SOUND) ? cAudio::ELL_DEBUG : cAudio::ELL_INFO);
    }

#define REUSE_SOURCES 0
    
    cAudio::IAudioSource *getSource(float newGain, int priority)
    {
        if (newGain < 0.001f)
            return NULL;
        if (!init())
            return NULL;
        
        cAudio::IAudioSource *usrc = NULL;
        
        float minVol = newGain;
        int   minIdx = -1;
        int   minPri = priority;

        // At least on Apple's OpenAL implementation, reusing sources like this results
        // in sounds not playing or playing in a weird muffled way
#if REUSE_SOURCES
        while (m_sources.size() < kSoundSources) {
            cAudio::IAudioSource *src = m_mgr->createStatic(NULL);
            ASSERT(src);
            if (src) {
                m_sources.push_back(make_pair(src, priority));
            }
        }

        for (uint i=0; i<m_sources.size(); i++) {
            cAudio::IAudioSource *src = m_sources[i].first;
            if (!src->isPlaying()) {
                minIdx = i;
                break;
            }
            const float vol = src->calculateGain();
            if (vol < minVol) {
                minIdx = i;
                minVol = vol;
            }
        }

        if (minIdx < 0)
            return NULL;
        usrc = m_sources[minIdx].first;
#else
        if (m_sources.size() >= kSoundSources)
        {
            // first steal by volume 
            for (uint i=0; i<m_sources.size();) {
                cAudio::IAudioSource *src = m_sources[i].first;
                const int             pri = m_sources[i].second;
                if (!src->isPlaying()) {
                    src->drop();
                    vector_remove_index(m_sources, i);
                } else {
                    if (pri < minPri)
                    {
                        minVol = src->calculateGain();
                        minPri = pri;
                        minIdx = i;
                    }
                    else if (pri == minPri)
                    {
                        const float vol = src->calculateGain();
                        if (vol < minVol) {
                            minIdx = i;
                            minPri = pri;
                            minVol = vol;
                        }
                    }
                    i++;
                }
            }

            if (m_sources.size() >= kSoundSources)
            {
                if (minIdx < 0)
                {
                    return NULL;
                }
                m_sources[minIdx].first->stop();
                m_sources[minIdx].first->drop();
                vector_remove_index(m_sources, minIdx);
            }
        }

        usrc = m_mgr->createStatic(NULL);
        if (usrc)
            m_sources.push_back(make_pair(usrc, priority));
#endif

        if (usrc) {
            usrc->stop();
            usrc->unRegisterAllEventHandlers();
            usrc->seek(0.f);
        }
        
        return usrc;
    }

    cAudio::IAudioSource *getStreamSource(lstring file)
    {
        if (!init())
            return NULL;
        if (m_streamSources.size() >= kStreamSources)
        {
            float minVol    = 1.f;
            int   minVolIdx = -1;
            
            for (uint i=0; i<m_streamSources.size(); ) {
                if (!m_streamSources[i]->isPlaying()) {
                    m_streamSources[i]->stop();
                    m_streamSources[i]->drop();
                    vector_remove_index(m_streamSources, i);
                } else {
                    const float vol = m_streamSources[i]->getVolume();
                    if (vol < minVol) {
                        minVol = vol;
                        minVolIdx = i;
                    }
                    i++;
                }
            }

            if (m_streamSources.size() >= kStreamSources) {
                if (minVolIdx < 0)
                    return NULL;
                
                m_streamSources[minVolIdx]->stop();
                m_streamSources[minVolIdx]->drop();
                vector_remove_index(m_streamSources, minVolIdx);
            }
        }

        cAudio::IAudioSource *src = m_mgr->create(file.c_str(), OL_PathForFile(file.c_str(), "r"));
        if (src) {
            m_streamSources.push_back(src);
        } else {
            cAudio::getLogger()->logDebug("Allocator", str_format("Failed to stream audio file '%s'", file.c_str()).c_str());
        }
        return src;
    }

    cAudio::IAudioBuffer* getBuffer(lstring fname)
    {
        if (!init())
            return NULL;
        cAudio::IAudioBuffer *&buf = m_buffers[fname];
        if (!buf) {
            const char* path = OL_PathForFile(fname.c_str(), "r");
            buf = m_mgr->createBuffer(path);
            if (!buf) {
                cAudio::getLogger()->logError("Allocator", str_format("Failed to load sound load '%s'", path).c_str());
            }
        }
        return buf;
    }
   
};

// loaded per-event type data
struct EventDescription {

    enum { AUDIO_FLAGS(SERIAL_TO_ENUM) };

    // read from audio.lua
    vector< vector<lstring> >  samples;
    float                      volume         = 1.0;
    float                      pitch          = 1.0;
    float                      pitchRandomize = 0.f;
    SerialEnum<EnumAudioFlags> flags;
    float                      rolloff        = 1.f;
    float                      minDist        = 1.f;
    float                      maxDist        = 9999999999.f;
    int                        priority       = 0;
    float2                     delay;

    lstring                    name;

    mutable uint               m_index = 0;

    void advance(int delta) const
    {
        if (samples.empty())
            return;
        const uint tracks = samples[0].size();
        if (tracks <= 1) {
            m_index = 0;
        } else if (flags&EventDescription::ROUND_ROBIN) {
            m_index = (m_index + delta) % tracks;
        } else {
            const uint last = m_index;
            do {
                m_index = randrange(0, tracks);
            } while (m_index == last);
        }
    }

    static EventDescription &getDefault()
    {
        static EventDescription def(false);
        return def;
    }

    EventDescription(bool usedef = true)
    {
        if (usedef)
            *this = getDefault();
    }
    
    typedef int VisitEnabled;

    template <typename V>
    bool accept(V& vis)
    {
        return (vis.VISIT(samples) &&
                vis.VISIT(volume) &&
                vis.VISIT(pitch) &&
                vis.VISIT(pitchRandomize) &&
                vis.VISIT(rolloff) &&
                vis.VISIT(minDist) &&
                vis.VISIT(maxDist) &&
                vis.VISIT(flags) &&
                vis.VISIT(delay) &&
                vis.VISIT(priority));
    }
};


// lightweight handle to a single playable sample
struct SoundLayer : public cAudio::ISourceEventHandler {

private:
    const EventDescription* m_se     = NULL;
    uint                    m_layer  = 0;
    float                   m_volume = 1.f;
    float                   m_pitch  = 1.f;
    float                   m_offset = 0.f;
    bool                    m_loop   = false;
    cAudio::IAudioSource*   m_source = NULL;

    const vector<lstring> &getSamples() const { return m_se->samples[m_layer]; }
    lstring getSample() const { return getSamples()[m_se->m_index]; }

public: 
    cAudio::IAudioSource *prepare(float gain3d=1.f)
    {
        if (m_source)
            return m_source;
        if (getSamples().empty())
            return NULL;
        const float vol = gain3d * m_se->volume * m_volume;
        if (vol < epsilon)
            return NULL;

        m_se->m_index = min(m_se->m_index, (uint)getSamples().size()-1);
        
        if (m_se->flags&EventDescription::STREAM) {
            m_source = AudioAllocator::instance().getStreamSource(getSample());
            if (!m_source)
                return NULL;
        } else {
            m_source = AudioAllocator::instance().getSource(vol, m_se->priority);
            if (!m_source)
                return NULL;
            cAudio::IAudioBuffer *buf = AudioAllocator::instance().getBuffer(getSample());
            if (!buf) {
                m_source->stop();
                m_source = NULL;
                return NULL;
            }
            m_source->setBuffer(buf);
            // DPRINT(SOUND, ("prepare %s:%d:%d : %s", m_se->name.c_str(), m_layer, m_se->m_index,
            //                getSamples()[m_se->m_index].c_str()));
        }
        
        m_source->registerEventHandler(this);

        m_source->setPitch(m_pitch * (m_se->pitch + randrange(-m_se->pitchRandomize, m_se->pitchRandomize)));
        m_source->setVolume(m_volume * m_se->volume);
        m_source->seek(m_offset, false);
        m_source->grab();
        return m_source;
    }

private: 

    // callbacks: !!!! Warning! called from cAudio thread on streaming sources !!!!!!!!

    void onRelease()
    {
        m_source = NULL;
    }

    void onStop()
    {
        if (m_source) {
            m_source->unRegisterEventHandler(this);
            m_source->drop();
            m_source = NULL;
        }
        m_offset = 0.f;
    }

public:

    void setIndex(int index) { m_se->m_index = index; }
    uint getIndex() const { return m_se->m_index; }

    void advance(int delta)
    {
        m_se->advance(delta);
    }

    SoundLayer() {}
    SoundLayer(const EventDescription* se, uint layer) : m_se(se), m_layer(layer) {}
    ~SoundLayer()
    {
        stop();
    }

    void setVolume(float v)
    {
        if (m_source && m_se)
            m_source->setVolume(v * m_se->volume);
        m_volume = v;
    }

    void setPitch(float v)
    {
        if (m_source && m_se)
            m_source->setPitch(v * m_se->pitch);
        m_pitch = v;
    }

    void setOffset(float s)
    {
        if (m_source && m_se)
            m_source->seek(s);
        m_offset = s;
    }

    void setLoop(bool loop)
    {
        if (m_source && m_se)
            m_source->loop(loop);
        m_loop = loop;
    }

    float getOffset() const { return m_source ? m_source->getCurrentAudioTime() : m_offset; }
    float getVolume() const { return m_volume; }
    float getPitch()  const { return m_pitch; }
    bool  isLoop()    const { return m_loop; }
    bool  isValid()   const { return m_se != NULL; }
    bool  isPlaying() const { return m_source && m_source->isPlaying(); }
    bool  isRelative() const { return m_source && m_source->isRelative(); }
    const EventDescription& getDescription() const { return *m_se; }

    void play2d()
    {
        if (prepare(1.f)) 
        {
            m_source->setPosition(c3zero);
            m_source->setVelocity(c3zero);
            m_source->setRolloffFactor(1.f);
            m_source->play2d((m_se->flags&EventDescription::LOOP) || m_loop);
            DPRINT(SOUND_EVENTS, ("Play2D %s:%d:\n%s", m_se->name.c_str(), m_se->m_index,
                                  sourceToString(m_source).c_str()));
        }
    }

    void play3d(float2 pos, float2 vel)
    {
        const float gain3d = AudioAllocator::instance().calculateGain(
            pos, m_se->minDist, m_se->maxDist, m_se->rolloff);

        if (prepare(gain3d)) 
        {
            m_source->setVelocity(c3(vel));
            m_source->setMinDistance(m_se->minDist);
            m_source->setMaxAttenuationDistance(m_se->maxDist);
            m_source->setRolloffFactor(m_se->rolloff);
            m_source->play3d(c3(pos), 1.f, (m_se->flags&EventDescription::LOOP) || m_loop);
            if (m_source->getBuffer() && m_source->getBuffer()->getChannels() == 2) {
                DPRINT(SOUND, ("Warning! Stereo sample %s:%d:%s does not support spacialization",
                               m_se->name.c_str(), m_se->m_index, getSample().c_str()));
            }
            DPRINT(SOUND_EVENTS, ("Play3D %s:%d:\n%s", m_se->name.c_str(), m_se->m_index,
                                  sourceToString(m_source).c_str()));
        }
    }

    void stop()
    {
        if (m_source) {
            m_source->unRegisterEventHandler(this); // first to prevent call to virtual onStop from destructor
            m_source->stop();
            m_source->drop();
            m_source = NULL;
        }
    }
};

static const uint kMaxSoundLayers = 3;

struct GameZone;

// layered virtual event handle
struct SoundEvent : public Watchable {

private:
    const EventDescription*            m_se     = NULL;
    float2                             m_pos;
    float2                             m_vel;
    SoundLayer                         m_layers[kMaxSoundLayers];
    uint                               m_layerCount = 0;

public:

    GameZone* zone = NULL;

    void setDescription(const EventDescription* se)
    {
        m_se = se;
        if (se) {
            m_layerCount = min((uint)se->samples.size(), kMaxSoundLayers);
            for (uint i=0; i<m_layerCount; i++)
                m_layers[i] = SoundLayer(se, i);
        } else {
            m_layerCount = 0;
        }
    }

    SoundEvent() {}
    SoundEvent(const EventDescription* se) { setDescription(se); }

    SoundEvent& operator=(const SoundEvent& o)
    {
        setDescription(o.m_se);
        m_pos = o.m_pos;
        m_vel = o.m_vel;
        zone = o.zone;
        return *this;
    }

    uint getLayerCount() const { return m_layerCount; }
    SoundLayer& getLayer(uint i) { return m_layers[i]; }
    const EventDescription &getDescription() const { return *m_se; }

    void setVolume(float v, int i) { m_layers[i].setVolume(v); }
    void setVolume(float v)
    {
        for (uint i=0; i<m_layerCount; i++)
            m_layers[i].setVolume(v);
    }

    void setOffset(float s)
    {
        for (uint i=0; i<m_layerCount; i++)
            m_layers[i].setOffset(s);
    }

    void setLoop(bool loop)
    {
        for (uint i=0; i<m_layerCount; i++)
            m_layers[i].setLoop(loop);
    }
    
    void setPitch(float v, int i=-1)
    {
        if (i >= 0)
            m_layers[i].setPitch(v);
        else
            foreach (SoundLayer& sl, m_layers)
                sl.setPitch(v);
    }

    float getVolume() const { return m_layerCount ? m_layers[0].getVolume() : 0.f; }
    float getOffset() const { return m_layerCount ? m_layers[0].getOffset() : 0.f; }    
    float getPitch()  const { return m_layerCount ? m_layers[0].getPitch() : 0.f; }
    float isRelative() const { return m_layerCount ? m_layers[0].isRelative() : 0.f; }
    bool  isLoop()    const { return m_layerCount && m_layers[0].isLoop(); }
    bool  isValid()   const { return m_se != NULL; }

    void   setPos(float2 p) { m_pos = p; }
    void   setVel(float2 v) { m_vel = v; }
    float2 getPos() const { return m_pos; }
    float2 getVel() const { return m_vel; }
   
    bool isPlaying() const 
    {
        foreach (const SoundLayer& sl, m_layers)
            if (sl.isPlaying())
                return true;
        return false;
    }

    void play2d()
    {
        if (!m_se || m_se->samples.empty() || m_se->volume < epsilon)
            return;
        for (uint i=0; i<m_layerCount; i++)
            m_layers[i].play2d();
    }
    
    void play3d()
    {
        if (!m_se || m_se->samples.empty() || m_se->volume < epsilon)
            return;
        for (uint i=0; i<m_layerCount; i++)
            m_layers[i].play3d(m_pos, m_vel);
    }

    void advance(int delta)
    {
        m_se->advance(delta);
    }

    void stop()
    {
        for (uint i=0; i<m_layerCount; i++)
            m_layers[i].stop();
    }
};



#endif // AUDIO_EVENT_H
