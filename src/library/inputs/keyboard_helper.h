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

#ifndef LIBTAS_KEYBOARDHELPER_H_INCLUDED
#define LIBTAS_KEYBOARDHELPER_H_INCLUDED

#include "../external/SDL1.h"
#include "shared/inputs/AllInputsFlat.h"

#include <SDL2/SDL.h>

namespace libtas {

/* Helper functions to translate from a Xlib KeySym to
 * a SDL 1 SDLKey or a SDL 2 Keycode
 */
SDL_Keycode X11_TranslateKeysym(unsigned int kc);
SDL1::SDLKey X11_Translate1Keysym(unsigned int xsym);

void X11_InitKeymap(void);

/* Fill the keyboard array that SDL functions are expected,
 * based on the KeySym set we get from our AllInputsFlat struct.
 */
void xkeyboardToSDLkeyboard(const std::array<unsigned int,AllInputsFlat::MAXKEYS>& Xkeyboard, Uint8* SDLkeyboard);
void xkeyboardToSDL1keyboard(const std::array<unsigned int,AllInputsFlat::MAXKEYS>& Xkeyboard, Uint8* SDLkeyboard);

/* Fill the SDL 1 or SDL 2 full key struct based on a Xlib KeySym */
void xkeysymToSDL(SDL_Keysym *keysym, unsigned int xkeysym);
void xkeysymToSDL1(SDL1::SDL_keysym *keysym, unsigned int xkeysym);

/* Build the Xlib/SDL2 modifier from the keyboard input */
unsigned int xkeyboardToXMod(const std::array<unsigned int,AllInputsFlat::MAXKEYS>& Xkeyboard);
SDL_Keymod xkeyboardToSDLMod(const std::array<unsigned int,AllInputsFlat::MAXKEYS>& Xkeyboard);

}

#endif
