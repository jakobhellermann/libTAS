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

#include "xkeyboard.h"
#include "inputs.h"
#include "xkeyboardlayout.h"

#include "logging.h"
#include "xlib/XlibGameWindow.h"
#include "GlobalState.h"
#include "shared/inputs/AllInputsFlat.h"

#include <cstring> // memset

namespace libtas {

/* Override */ int XQueryKeymap( Display* display, char keymap[32])
{
    LOGTRACE(LCF_KEYBOARD);

    memset(keymap, 0, 32);
    GlobalNoLog gnl; // Avoid logging on XkbKeycodeToKeysym
    for (int i=0; i<AllInputsFlat::MAXKEYS; i++) {
        if (Inputs::game_ai.keyboard[i] == 0) {
            break;
        }
        for (int kc=0; kc<256; kc++) {
            KeySym ks = XkbKeycodeToKeysym(display, (KeyCode)kc, 0, 0);
            if (ks == Inputs::game_ai.keyboard[i]) {
                keymap[kc>>3] |= (1 << (kc&0x7));
                break;
            }
        }
    }

    /* I don't know what it should return */
    return 0;
}

/* Override */ int XGrabKey(Display*, int, unsigned int, Window, Bool, int, int)
{
    LOGTRACE(LCF_KEYBOARD);
    return GrabSuccess;
}

/* Override */ int XGrabKeyboard(Display*, Window, Bool, int, int, Time)
{
    LOGTRACE(LCF_KEYBOARD);
    return GrabSuccess;
}

/* Override */ int XUngrabKey(Display*, int, unsigned int, Window)
{
    LOGTRACE(LCF_KEYBOARD);
    return 0;
}

/* Override */ int XUngrabKeyboard(Display*, Time)
{
    LOGTRACE(LCF_KEYBOARD);
    return 0;
}

/* Override */ int XGetInputFocus(Display* display, Window* focus_return, int* revert_to_return)
{
    RETURN_IF_NATIVE(XGetInputFocus, (display, focus_return, revert_to_return), nullptr);

    LOGTRACE(LCF_KEYBOARD);
    /* Returing the game Xlib window */
    if (focus_return && XlibGameWindow::get()) {
        *focus_return = XlibGameWindow::get();
    }
    return 0;
}

/* Override */ int XSetInputFocus(Display* display, Window focus, int revert_to, Time time)
{
    LOG(LL_TRACE, LCF_KEYBOARD, "%s called with window %d", __func__, focus);
    return 0;
}

}
