//
// Created by jakob on 5/23/25.
//

#include "messages.h"

const char *get_message_name(int id) {
    switch (id) {
#define X(name) case name: return #name;
        MSG_ID
#undef X
        default: return "UNKNOWN_MSG_ID";
    }
}

