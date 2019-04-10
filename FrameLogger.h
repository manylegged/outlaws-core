
//
// FrameLogger.h - draw pretty profiling graph overlays
// 

// Copyright (c) 2013-2016 Arthur Danskin
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

#ifndef LOGGING_H
#define LOGGING_H

#include "StdAfx.h"
#include "Shaders.h"
#include "GLText.h"

static const uint kGraphColors[] = {
    0xff0000,
    0xff6000,
    0xff9000,
    0xffc000,
    0xffff00,
    0xb0ff00,
    0x00ff00,
    0x00ffff,
    0x00a0ff,
    0x0060ff,
    0x0000ff,
    0x6000ff,
    0x9000ff,
    0xc000ff,
    0xff00ff,
    0xff0080,
};

#define COLOR_AXIS 0x01ff5f

static const uint kFrameLoggerSortFrames = 120;

struct FrameLogger {

    struct PhaseTime {
        lstring first;
        int     index;
        double second;
    };
    // typedef std::pair<lstring, double>          PhaseTime;
    typedef std::unordered_map<lstring, double> Dict;
    typedef std::vector<PhaseTime>              VDict;

    struct PhaseData {
        lstring name;
        uint    color   = 0;
        bool    stacked = true;
        int     index   = 0;
        double  mean    = 0, stddev=0;
    };

    const char*                                  m_title = NULL;
    float                                        m_barWidth = 2.f;
    int                                          m_graphItems = 240;
    bool                                         m_paused = true;

    // must be called from the same thread
    double                                       m_frameStartTime;
    double                                       m_frameDeadline;
    vector<PhaseTime>                            m_currentPhases;
    Dict                                         m_current;

    // shared, mutex protected
    std::mutex                                   m_mutex;
    std::deque< Dict >                           m_log;
    std::unordered_map<lstring, PhaseData>       m_phaseData;

    // render thread
    double                                       m_curMaxTimeMs = 16.0;
    double                                       m_maxTimeMs = 16.0;
    float                                        m_labelWidth = 100.f;
    std::vector<VDict>                           m_sortLog;
    vector<PhaseData>                            m_phaseCache;
    std::set<double>                             m_heights;
    uint                                         m_frame = 0;
    
    // RAII phase timing wrapper
    struct Phase {
        FrameLogger *logger;
        const char  *name;
        Phase(FrameLogger& l, const char *n, double t=0) : logger(&l), name(n) { l.beginPhase(name, t); }
        Phase(FrameLogger* l, const char *n, double t=0) : logger(l), name(n) { if (l) l->beginPhase(name, t); }
        ~Phase() { end(); }
        void end() { if (logger) logger->endPhase(name); logger = NULL; }
        void phase(const char* nm, double t=0)
        {
            if (!logger)
                return;
            logger->endPhase(name);
            logger->beginPhase(nm, t);
            name = nm;
        }

        double time() const
        {
            if (!logger)
                return 0.0;
            return max(0.0, OL_GetCurrentTime() - logger->m_currentPhases.back().second);
        }
            
    };

    FrameLogger(const char* ttl) : m_title(ttl) {}

    void beginFrame(bool active, double deadline)
    {
        m_paused = !active;
        if (m_paused)
            return;
        m_frameStartTime = OL_GetCurrentTime();
        m_frameDeadline  = deadline;
        m_current.clear();
    }

    void beginPhase(const char *phase, double t=0.0)
    {
        if (m_paused)
            return;
        m_currentPhases.push_back({phase, 0, t ? t : OL_GetCurrentTime()});
    }

    void endPhase(const char *phase)
    {
        if (m_paused)
            return;
        ASSERT(m_currentPhases.size() && m_currentPhases.back().first == phase);
        const double time = max(0.0, OL_GetCurrentTime() - m_currentPhases.back().second);
        double &curtime = m_current[m_currentPhases.back().first];
        curtime += time;
        m_currentPhases.pop_back();
        foreach (PhaseTime &pt, m_currentPhases) {
            pt.second += time;
        }
    }

    void addPhase(const char *phase, double length)
    {
        if (m_paused)
            return;
        map_setdefault(m_current, phase, 0.0) += length;

        std::lock_guard<std::mutex> l(m_mutex);
        m_phaseData[phase].stacked = false;
    }

    void addStack(const char *phase, double length)
    {
        if (m_paused)
            return;
        map_setdefault(m_current, phase, 0.0) += length;
    }

    void endFrame(double frame_time=0)
    {
        if (m_paused)
            return;
        ASSERT(m_currentPhases.empty());
        if (frame_time == 0)
        {
            ASSERT(m_frameStartTime > 0.0);
            addPhase("Frame", OL_GetCurrentTime() - m_frameStartTime);
            m_frameStartTime = 0.0;
        }
        else
        {
            addPhase("Frame", frame_time);
        }

        std::lock_guard<std::mutex> l(m_mutex);
        for_ (phase, m_current) {
        // foreach (const PhaseTime& phase, m_current) {
            PhaseData &pd = m_phaseData[phase.first];
            if (!pd.color)
                pd.color = kGraphColors[m_phaseData.size() % arraySize(kGraphColors)];
        }
        
        m_log.push_back(move(m_current));
        while (m_log.size() > m_graphItems)
        {
            m_current = move(m_log.front());
            m_log.pop_front();
        }
        m_current.clear();
    }

    void renderGraph(const ShaderState& ss, DMesh &mesh, float2 graphStart, float2 graphSize)
    {
        std::lock_guard<std::mutex> l(m_mutex);

        if (m_log.empty())
            return;

        static const float kPointHeightMs = 0.1f;

        int i=0;
        m_phaseCache.resize(m_phaseData.size());
        for_ (it, m_phaseData) {
            it.second.index = i;
            it.second.name = it.first;
            m_phaseCache[i] = it.second;
            i++;
        }
        
        // m_sortLog.clear();
        // foreach (const Dict& pt, m_log)
            // m_sortLog.push_back(VDict(pt.begin(), pt.end()));
        m_sortLog.resize(m_log.size());
        for (int k=0; k<m_log.size(); k++)
        {
            VDict &sl = m_sortLog[k];
            Dict &pt = m_log[k];
            sl.resize(pt.size());
            int j = 0;
            for_(x, pt)
            {
                sl[j++] = { x.first, m_phaseData[x.first].index, x.second };
            }
        }

        // compute statistics
        if (m_frame++ % kFrameLoggerSortFrames == 0)
        {
            m_curMaxTimeMs = 0;
            const double inv_size = 1.0 / m_log.size();

            for_ (pd, m_phaseCache) {
                pd.mean = 0;
                pd.stddev = 0;
            }
            
            for_ (vd, m_sortLog) {
                for_ (pt, vd)
                    m_phaseCache[pt.index].mean += pt.second;
            }

            for_ (pd, m_phaseCache)
                pd.mean *= inv_size;

            for_ (vd, m_sortLog) {
                for_ (pt, vd) {
                    PhaseData &pd = m_phaseCache[pt.index];
                    pd.stddev += squared(pt.second - pd.mean);
                }
            }

            for_ (pd, m_phaseCache)
            {
                pd.stddev = sqrt(pd.stddev * inv_size);
                if (pd.name == "Frame")
                    m_curMaxTimeMs = max(1.0, 1000.f * (pd.mean + 2.f * pd.stddev));
                m_phaseData[pd.name] = pd;
            }
        }

        //m_maxTimeMs = lerp(m_maxTimeMs, m_curMaxTimeMs, 0.01);
        m_maxTimeMs = m_curMaxTimeMs;
        foreach (VDict& vec, m_sortLog) {
            std::sort(vec.begin(), vec.end(), [&](const PhaseTime& a, const PhaseTime& b) { 
                    return m_phaseCache[a.index].stddev < m_phaseCache[b.index].stddev;
                });
        }


        // draw labels
        m_heights.clear();
        double           ystart = 0;
        uint             pindex = 0;
        float            lwidth = 0.f;

        const double textSize = 8;
        const double textHeight = GLText::getScaledSize(textSize);
        
        for_ (pd, m_phaseCache) 
        {
            const double     y     = 1000.f * pd.mean * (graphSize.y / m_maxTimeMs);
            double           yoff  = 0;

            if (pd.stacked) {
                yoff    = y + ystart;
                ystart += y;
            } else {
                yoff = y;
            }

            foreach (double d, m_heights) {
                if (abs(d - yoff) < textHeight)
                    yoff = d + textHeight;
            }
            m_heights.insert(yoff);

            float2 lsize = GLText::Fmt(ss, graphStart + float2(-3, yoff), GLText::MID_RIGHT,
                                       ALPHA_OPAQUE|pd.color, textSize,
                                       "%s: %.1f/%.1f", pd.name.c_str(),
                                       1000.0 * pd.mean, 1000.0 * pd.stddev);
            lwidth = max(lwidth, lsize.x);
            pindex++;
        }
        m_labelWidth = lwidth;
        
        // graph
        const float2 pointSize(graphSize.x / m_graphItems,
                               clamp(kPointHeightMs * graphSize.y / m_maxTimeMs, 1.0, graphSize.y/10.0));
        uint xi = 0;
        foreach (const VDict& mp, m_sortLog) 
        {
            const float x = (0.5f + xi) * pointSize.x;
            pindex = 0;
            ystart = 0;
            foreach (const PhaseTime &phase, mp) {
                const PhaseData &pd = m_phaseCache[phase.index];
                mesh.tri.color(pd.color, 0.5f);
                const float y = 1000.f * phase.second * (graphSize.y / m_maxTimeMs);
                if (pd.stacked) {
                    mesh.tri.PushRectCorners(graphStart + float2(x - 0.5f * pointSize.x, ystart),
                                          graphStart + float2(x + 0.5f * pointSize.x, ystart + y));
                    ystart += y;
                } else {
                    mesh.tri.PushRect(graphStart + float2(x, y), pointSize);
                }
                pindex++;
            }
            xi++;
        }
    }

    void render(const View& view)
    {
        const ShaderState ss = view.getScreenShaderState();

        DMesh::Handle h(theDMesh());
        
        h.mp.line.color(COLOR_AXIS);
        // h.mp.line.Rect(view.center, view.size/2.f);

        const float2 start = view.center - view.size/2.f + justX(m_labelWidth) + float2(10.f);
        const float2 graphSize = view.size - justX(m_labelWidth) - float2(20.f);

        m_graphItems = clamp(floor_int(graphSize.x/m_barWidth), 60, 240);

        // axes
        h.mp.line.PushLine(start, start + justX(graphSize));
        h.mp.line.PushLine(start, start + justY(graphSize));
        const int tines = m_maxTimeMs / 5.f;
        if (tines < graphSize.y/4.f)
        {
            for (uint ms=0; ms<m_maxTimeMs; ms += 5) {
                const float y = ms * (graphSize.y / m_maxTimeMs);
                h.mp.line.PushLine(start + justY(y), start + float2(ms&1 ? 10 : 20, y));
            }
        }
        if (m_frameDeadline > 0.f)
        {
            const float dly = 1000.f * m_frameDeadline * (graphSize.y / m_maxTimeMs);
            h.mp.line.color(COLOR_AXIS, 0.5f);
            h.mp.line.PushLine(start + justY(dly), start + float2(graphSize.x, dly));
        }
        
        renderGraph(ss, h.mp, start, graphSize);
        h.Draw(ss);

        if (m_title)
            GLText::Put(ss, view.center + justY(view.size/2.f), GLText::DOWN_CENTERED,
                        0x80ffffff, 16.f, m_title);
    }
    
};


inline void renderLoggerDash(vector<FrameLogger *> &loggers, View view)
{
    if (loggers.size() == 1) {
        view.size = view.sizePoints * float2(0.6f, 0.6f);
        view.center = view.sizePoints * float2(0.6f, 0.4f);
        loggers[0]->render(view);
    } else if (loggers.size() == 2) {
        view.size = view.sizePoints * float2(0.45f, 0.6f);
        view.center.y = view.sizePoints.y * 0.4f;
        view.center.x = view.sizePoints.x * 0.25f;
        loggers[0]->render(view);
        view.center.x = view.sizePoints.x * 0.75f;
        loggers[1]->render(view);
    } else if (loggers.size() > 2) {
        view.size = view.sizePoints/2.f * 0.9f;
        for (int x=0; x<2; x++) {
            for (int y=0; y<2; y++) {
                const int i = y * 2 + x;
                if (i >= loggers.size())
                    continue;
                view.center = view.sizePoints/2.f * float2(x + 0.5f, y + 0.5f);
                loggers[i]->render(view);
            }
        }
    }
}


#endif // LOGGING_H
