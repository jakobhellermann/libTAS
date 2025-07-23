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

#include "XcbEventQueue.h"

#include "logging.h"
#include "inputs/inputs.h"
#include "xlib/XlibEventQueueList.h"
#include "xlib/XlibEventQueue.h"
#include "xlib/xdisplay.h" // x11::gameDisplays

#include <xcb/xproto.h>

namespace libtas {

XcbEventQueue::XcbEventQueue(xcb_connection_t *conn) : c(conn) {}

XcbEventQueue::~XcbEventQueue()
{
    for (auto* ev : eventQueue) {
        free(ev);
    }
}

void XcbEventQueue::setMask(xcb_window_t wid, uint32_t event_mask)
{
    eventMasks[wid] = event_mask;

    /* If the game is interested in the XCB_EVENT_MASK_ENTER_WINDOW event, send one immediately */
    // if (event_mask & XCB_EVENT_MASK_ENTER_WINDOW) {
    //     xcb_enter_notify_event_t ev;
    //     ev.response_type = XCB_ENTER_NOTIFY;
    //     ev.event = wid;
    //     ev.event_x = Inputs::game_ai.pointer.x;
    //     ev.event_y = Inputs::game_ai.pointer.y;
    //     ev.root_x = Inputs::game_ai.pointer.x;
    //     ev.root_y = Inputs::game_ai.pointer.y;
    //     ev.state = SingleInput::toXlibPointerMask(ai.pointer.mask);
    //
    //     debuglog(LCF_EVENTS | LCF_MOUSE, "   Inserting a XCB_EVENT_MASK_ENTER_WINDOW event");
    //     insert(reinterpret_cast<xcb_generic_event_t*>(&ev));
    // }
    //
    // /* If the game is interested in the XCB_EVENT_MASK_FOCUS_CHANGE event, send one immediately */
    // if (event_mask & XCB_EVENT_MASK_FOCUS_CHANGE) {
    //     xcb_focus_in_event_t ev;
    //     ev.response_type = XCB_FOCUS_IN;
    //     ev.event = wid;
    //
    //     debuglog(LCF_EVENTS | LCF_MOUSE | LCF_KEYBOARD, "   Inserting a XCB_EVENT_MASK_FOCUS_CHANGE event");
    //     insert(reinterpret_cast<xcb_generic_event_t*>(&ev));
    // }

    /* TODO: Until we merge Xlib and xcb event queues, a hack to propagate the
     * event mask from xcb to Xlib. */
    for (int i=0; i<GAMEDISPLAYNUM; i++) {
        if (x11::gameDisplays[i])
            xlibEventQueueList.getQueue(x11::gameDisplays[i])->setMask(wid, event_mask);
    }
}

#define EVENTQUEUE_MAXLEN 1024

int XcbEventQueue::insert(xcb_generic_event_t *event, bool full_event)
{
    /* Check if the window can produce such event */
    // auto mask = eventMasks.find(event->xany.window);
    // if (mask != eventMasks.end()) {
    //     if (!isTypeOfMask(event->xany.type, mask->second))
    //         return 0;
    // }
    // else {
    //     /* Check unmaskable events */
    //     if (!isTypeOfMask(event->xany.type, 0))
    //         return 0;
    // }

    /* Check the size of the queue */
    if (eventQueue.size() > 1024) {
        LOG(LL_DEBUG, LCF_EVENTS, "We reached the limit of the event queue size!");
        return -1;
    }

    xcb_generic_event_t* ev;

    if (full_event) {
        /* Figure out the size of the event (this is variable for xcb_ge_generic_event_t!) */
        size_t event_size = sizeof(xcb_generic_event_t);
        if (event->response_type == XCB_GE_GENERIC) {
            event_size += reinterpret_cast<xcb_ge_generic_event_t*>(event)->length * 4;
        }

        /* Allocate a copy of the event */
        ev = static_cast<xcb_generic_event_t*>(malloc(event_size));
        memcpy(ev, event, event_size);
    }
    else {
        /* If we're generating our own events, we generally won't actually have the full xcb_generic_event_t passed
         * The last 4 bytes of xcb_generic_event_t stores the full sequence number, and is not present in other struct definitions
         * We do need to actually set this to something, otherwise Xlib might throw an error (0 suffices here)
         */
        ev = static_cast<xcb_generic_event_t*>(malloc(sizeof(xcb_generic_event_t)));
        memcpy(ev, event, sizeof(xcb_generic_event_t) - 4);
        ev->full_sequence = 0;
    }

    /* Push the event copy at the beginning of the queue */
    eventQueue.push_front(ev);

    return 1;
}

xcb_generic_event_t* XcbEventQueue::pop()
{
    if (eventQueue.size() == 0)
        return nullptr;

    xcb_generic_event_t* event = eventQueue.back();
    eventQueue.pop_back();
    return event;
}

int XcbEventQueue::size()
{
    return eventQueue.size();
}

}
