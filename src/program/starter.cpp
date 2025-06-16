#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include "GameThread.h"
#include "Context.h"
#include "../library/logging.h"
#include "../shared/sockethelpers.h"

int main(int argc, char **argv) {
    Context c = {};
    Context *context = &c;
    if (argc >= 2) {
        context->gamepath = argv[1];
        context->gameexecutable = context->gamepath;
    } else {
        context->gamepath = "/home/jakob/.cache/rust/release/examples/breakout";
        context->gameexecutable = context->gamepath;
    }
    context->gameexecutable = context->gamepath;
    context->libtaspath = "/home/jakob/dev/contrib/libtas-autotools/build/src/library/libtas.so";
    context->config.sc.logging_level = LL_DEBUG;
    context->config.sc.logging_include_flags = LCF_CHECKPOINT;

    if (int err = removeSocket(); err != 0) {
        std::cerr << "Could not remove socket file /tmp/libTAS.socket: " << strerror(err) << std::endl;
    }

    // if (pid_t fork_pid = fork(); fork_pid == 0) {
    GameThread::launch(context);
    // }
}
