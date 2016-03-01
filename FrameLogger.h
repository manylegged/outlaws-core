
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

struct FrameLogger {

    typedef std::pair<lstring, double>          PhaseTime;
    typedef std::unordered_map<lstring, double> Dict;
    typedef std::vector<PhaseTime>              VDict;

    struct Stats     { double mean = 0.f, stddev = 0.f; }; // seconds
    struct PhaseData { uint color = 0; bool stacked = true; };

    const char*                                  m_title = NULL;
    float                                        m_barWidth = 2.f;
    int                                          m_graphItems = 240;
    bool                                         m_paused = true;

    // must be called from the same thread
    double                                       m_frameStartTime;
    double                                       m_frameDeadline;
    vector<PhaseTime>                            m_currentPhases;
    Dict                                         m_current;
    std::unordered_map<lstring, Stats>           m_stats;

    // shared, mutex protected
    std::mutex                                   m_mutex;
    std::deque< Dict >                           m_log;
    std::unordered_map<lstring, PhaseData>       m_phaseData;

    // render thread
    double                                       m_curMaxTimeMs = 16.0;
    double                                       m_maxTimeMs = 16.0;
    float                                        m_labelWidth = 100.f;
    std::vector<VDict>                           m_sortLog;
    std::unordered_set<lstring>                  m_phases;
    uint                                         m_frame = 0;
    
    // RAII phase timing wrapper
    struct Phase {
        FrameLogger *logger;
        const char  *name;
        Phase(FrameLogger& l, const char *n) : logger(&l), name(n) { l.beginPhase(name); }
        Phase(FrameLogger* l, const char *n) : logger(l), name(n) { if (l) l->beginPhase(name); }
        ~Phase() { end(); }
        void end() { if (logger) logger->endPhase(name); logger = NULL; }
        void phase(const char* nm)
        {
            if (!logger)
                return;
            logger->endPhase(name);
            logger->beginPhase(nm);
            name = nm;
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

    void beginPhase(const char *phase)
    {
        if (m_paused)
            return;
        m_currentPhases.push_back(std::make_pair(phase, OL_GetCurrentTime()));
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

    void endFrame()
    {
        if (m_paused)
            return;
        ASSERT(m_currentPhases.empty());
        ASSERT(m_frameStartTime > 0.0);
        addPhase("Frame", OL_GetCurrentTime() - m_frameStartTime);
        m_frameStartTime = 0.0;

        std::lock_guard<std::mutex> l(m_mutex);
        foreach (const PhaseTime& phase, m_current) {
            if (!m_phaseData[phase.first].color)
                m_phaseData[phase.first].color = kGraphColors[m_phaseData.size() % arraySize(kGraphColors)];
        }
        
        m_log.push_back(m_current);
        while (m_log.size() > m_graphItems)
            m_log.pop_front();
        m_current.clear();
    }

    void renderGraph(const ShaderState& ss, DMesh &mesh, float2 graphStart, float2 graphSize)
    {
        std::lock_guard<std::mutex> l(m_mutex);

        if (m_log.empty())
            return;

        static const float kPointHeightMs = 0.1f;

        m_sortLog.clear();
        foreach (const Dict& pt, m_log) {
            m_sortLog.push_back(VDict(pt.begin(), pt.end()));
        }

        // compute statistics
        if (m_frame++ % 60 == 0)
        {
            m_curMaxTimeMs = 0;
            m_stats.clear();
            foreach (const PhaseTime &phase, m_log[0]) 
            {
                Stats stat;
                foreach (const Dict& mp, m_log) {
                    double times = map_get(mp, phase.first, 0.0);
                    stat.mean += times;
                    m_curMaxTimeMs = max(m_curMaxTimeMs, 1000.f * times);
                }
                stat.mean /= m_log.size();

                foreach (const Dict& mp, m_log) {
                    double times = map_get(mp, phase.first, 0.0);
                    stat.stddev += (times - stat.mean) * (times - stat.mean);
                }
                stat.stddev = sqrt(stat.stddev / m_log.size());
            
                m_stats[phase.first] = stat;
            }
        }

        //m_maxTimeMs = lerp(m_maxTimeMs, m_curMaxTimeMs, 0.01);
        m_maxTimeMs = m_curMaxTimeMs;
        m_phases.clear();
        foreach (VDict& vec, m_sortLog) {
            std::sort(vec.begin(), vec.end(), [&](const PhaseTime& a, const PhaseTime& b) { 
                    return m_stats[a.first].stddev < m_stats[b.first].stddev;
                });
            foreach (const PhaseTime &phase, vec)
                m_phases.insert(phase.first);
        }


        // draw labels
        std::set<double> heights;
        double           ystart = 0;
        uint             pindex = 0;
        float            lwidth = 0.f;

        foreach (const lstring phase, m_phases) 
        {
            const Stats  stat       = m_stats[phase];
            const double y          = 1000.f * stat.mean * (graphSize.y / m_maxTimeMs);
            const double textHeight = GLText::getScaledSize(8);
            double       yoff       = 0;

            if (m_phaseData[phase].stacked) {
                yoff    = y + ystart;
                ystart += y;
            } else {
                yoff = y;
            }

            foreach (double d, heights) {
                if (abs(d - yoff) < textHeight)
                    yoff = d + textHeight;
            }
            heights.insert(yoff);

            float2 lsize = GLText::DrawScreen(ss, graphStart + float2(-3, yoff),
                                              GLText::MID_RIGHT, ALPHA_OPAQUE|m_phaseData[phase].color,
                                              textHeight, "%s: %.1f/%.1f", phase.c_str(), 
                                              1000.0 * stat.mean, 1000.0 * stat.stddev);
            lwidth = max(lwidth, lsize.x);
            pindex++;
        }
        m_labelWidth = lwidth;
        
        // graph
        const float2 pointSize(graphSize.x / m_graphItems,
                               clamp(kPointHeightMs * graphSize.y / m_maxTimeMs, 1.f, graphSize.y/10.f));
        uint xi = 0;
        foreach (const VDict& mp, m_sortLog) 
        {
            const float x = (0.5f + xi) * pointSize.x;
            pindex = 0;
            ystart = 0;
            foreach (const PhaseTime &phase, mp) {
                mesh.tri.color(m_phaseData[phase.first].color, 0.5f);
                const float y = 1000.f * phase.second * (graphSize.y / m_maxTimeMs);
                if (m_phaseData[phase.first].stacked) {
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
        // h.mp.line.PushRect(view.center, view.size/2.f);

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
