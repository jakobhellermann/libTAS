/*
    Copyright 2015-2024 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBTAS_PERFTIMER_H_INCL
#define LIBTAS_PERFTIMER_H_INCL

#include "TimeHolder.h"
#include "shared/lcf.h"

namespace libtas {

class PerfTimer
{
    public:

        enum TimerType {
            NoTimer = -1,
            GameTimer = 0,
            FrameTimer,
            RenderTimer,
            IdleTimer,
            WaitTimer,
            TimeTimer,
            SpecialTimer,
            TotalTimer,
        };

        void switchTimer(TimerType type);
        TimerType currentTimer();
        void print();

    private:
        
        TimeHolder current_time[TotalTimer];
        TimeHolder elapsed[TotalTimer];
        TimerType current_type = NoTimer;
};

class PerfTimerCall
{
public:
    PerfTimerCall(LogCategoryFlag lcf);
    ~PerfTimerCall();
private:
    PerfTimer::TimerType type = PerfTimer::NoTimer;
};

extern PerfTimer perfTimer;

}

#endif
