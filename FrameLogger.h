
//
// FrameLogger.h - draw pretty profiling graph overlays
// 

// Copyright (c) 2013-2015 Arthur Danskin
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

static const uint kGraphItems    = 240;
static const uint kGraphColors[] = {
    0xff0000,
    0xff8000,
    0xffff00,
//    0x80ff00,
    0x00ff00,
//    0x00ff80,
    0x00ffff,
    0x0080ff,
    0x0000ff,
    0x8000ff,
    0xff00ff,
    0xff0080,
};

struct FrameLogger {

    typedef std::pair<lstring, double>          PhaseTime;
    typedef std::unordered_map<lstring, double> Dict;
    typedef std::vector<PhaseTime>              VDict;

    struct Stats     { double mean = 0.f, stddev = 0.f; }; // seconds
    struct PhaseData { uint color = 0; bool stacked = true; };

    bool                                         m_paused  = true;

    // must be called from the same thread
    double                                       m_frameStartTime;
    double                                       m_frameDeadline;
    vector<PhaseTime>                            m_currentPhases;
    Dict                                         m_current;
    std::unordered_map<lstring, Stats>           m_stats;

    // shared, mutex protected
    std::mutex                                   m_mutex;
    std::deque< Dict >                           m_log;
    std::unordered_map<lstring, PhaseData >      m_phaseData;

    // render thread
    TriMesh<VertexPosColor>                      m_tri;
    LineMesh<VertexPosColor>                     m_line;
    double                                       m_curMaxTimeMs = 16.0;
    double                                       m_maxTimeMs = 16.0;
    std::vector<VDict>                           m_sortLog;
    uint                                         m_frame = 0;
    
    // RAII phase timing wrapper
    struct Phase {
        FrameLogger &logger;
        const char  *name;
        Phase(FrameLogger& l, const char *n) : logger(l), name(n)
        {
            logger.beginPhase(name);
        }
        ~Phase()
        {
            logger.endPhase(name);
        }
    };

    void beginFrame(double deadline)
    {
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
        const double time = OL_GetCurrentTime() - m_currentPhases.back().second;
        map_setdefault(m_current, m_currentPhases.back().first, 0.0) += time;
        m_currentPhases.pop_back();
        if (m_currentPhases.size())
            m_currentPhases.back().second += time;
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
        while (m_log.size() > kGraphItems)
            m_log.pop_front();
        m_current.clear();
    }

    void renderGraph(const ShaderState& screenSS, float2 graphStart, float2 graphSize)
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
        foreach (VDict& vec, m_sortLog) {
            std::sort(vec.begin(), vec.end(), [&](const PhaseTime& a, const PhaseTime& b) { 
                    return m_stats[a.first].stddev < m_stats[b.first].stddev;
                });
        }


        // draw labels
        vector<double> heights;
        double         ystart = 0;
        uint           pindex     = 0;

        foreach (const PhaseTime &phase, m_sortLog[0]) 
        {
            const Stats  stat       = m_stats[phase.first];
            const double y          = 1000.f * stat.mean * (graphSize.y / m_maxTimeMs);
            const double textHeight = GLText::getScaledSize(10);
            double       yoff       = 0;

            if (m_phaseData[phase.first].stacked) {
                yoff    = y + ystart;
                ystart += y;
            } else {
                yoff = y;
            }

            foreach (double d, heights) {
                if (abs(d - yoff) < textHeight)
                    yoff = d + textHeight;
            }
            heights.push_back(yoff);

            GLText::DrawScreen(screenSS, graphStart + float2(-3, yoff),
                               GLText::MID_RIGHT, ALPHA_OPAQUE|m_phaseData[phase.first].color,
                               textHeight, "%s: %.1f/%.1f", phase.first.c_str(), 
                               1000.0 * stat.mean, 1000.0 * stat.stddev);
            pindex++;
        }
        
        // graph
        const float2 pointSize(graphSize.x / kGraphItems, 
                               kPointHeightMs * graphSize.y / m_maxTimeMs);
        uint xi = 0;
        foreach (const VDict& mp, m_sortLog) 
        {
            const float x = (0.5f + xi) * (graphSize.x / kGraphItems);
            pindex = 0;
            ystart = 0;
            foreach (const PhaseTime &phase, mp) {
                m_tri.color(m_phaseData[phase.first].color, 0.5f);
                const float y = 1000.f * phase.second * (graphSize.y / m_maxTimeMs);
                if (m_phaseData[phase.first].stacked) {
                    m_tri.PushRectCorners(graphStart + float2(x - 0.5f * pointSize.x, ystart), 
                                          graphStart + float2(x + 0.5f * pointSize.x, ystart + y));
                    ystart += y;
                } else {
                    m_tri.PushRect(graphStart + float2(x, y), pointSize);
                }
                pindex++;
            }
            xi++;
        }
    }

    void render(const View& view)
    {
        ShaderState screenSS = view.getScreenShaderState();

        static const float2 kGraphStart(500, 40);
        const float2 graphSize = toGoldenRatioX(view.sizePoints.x - kGraphStart.x - kGraphStart.y);

        m_tri.clear();
        m_line.clear();

        // axes
        m_line.color(COLOR_GREEN);
        m_line.PushLine(kGraphStart, kGraphStart + float2(graphSize.x, 0));
        m_line.PushLine(kGraphStart, kGraphStart + float2(0, graphSize.y));
        const int tines = m_maxTimeMs / 5.f;
        if (tines < graphSize.y/2.f)
        {
            for (uint ms=0; ms<m_maxTimeMs; ms += 5) {
                const float y = ms * (graphSize.y / m_maxTimeMs);
                m_line.PushLine(kGraphStart + float2(0, y), kGraphStart + float2(ms&1 ? 10 : 20, y));
            }
        }
        m_line.color(COLOR_GREEN, 0.5f);
        const float dly = 1000.f * m_frameDeadline * (graphSize.y / m_maxTimeMs);
        m_line.PushLine(kGraphStart + float2(0.f, dly),
                        kGraphStart + float2(graphSize.x, dly));

        renderGraph(screenSS, kGraphStart, graphSize);

        m_line.Draw(screenSS, ShaderColor::instance());
        m_tri.Draw(screenSS, ShaderColor::instance());
    }
    
};


#endif // LOGGING_H
