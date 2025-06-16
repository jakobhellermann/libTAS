#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

#include "GameThread.h"
#include "Context.h"
#include "utils.h"
#include "../library/logging.h"
#include "../shared/GameInfo.h"
#include "../shared/inputs/AllInputs.h"
#include "../shared/sockethelpers.h"
#include "../shared/messages.h"

using namespace libtas;

class GameLoop {
    Context *context;

public:
    explicit GameLoop(Context *context) : context(context) {
    }

    void start();

private:
    pid_t fork_pid = 0;

    void init();

    void initProcessMessages();

    bool startFrameMessages();

    void sleepSendPreview();

    /* Get the address of a symbol from a executable or library file */
    uint64_t getSymbolAddress(const char *symbol, const char *file);

    void processInputs(AllInputs &ai);

    void endFrameMessages(AllInputs &ai);

    void loopExit();
};


uint64_t GameLoop::getSymbolAddress(const char *symbol, const char *file) {
    std::ostringstream cmd;
    cmd << "readelf -Ws \"" << file << "\" | grep " << symbol << " | awk '{print $2}'";

    uint64_t addr = std::strtoull(queryCmd(cmd.str()).c_str(), nullptr, 16);
    return addr;
}

void GameLoop::initProcessMessages() {
    bool inited = initSocketProgram(fork_pid);
    if (!inited) {
        loopExit();
        return;
    }


    int message = receiveMessage();
    while (message != MSGB_END_INIT) {
        std::cout << "initProcessMessages: " << get_message_name(message) << std::endl;


        switch (message) {
            /* Get the game process pid */
            case MSGB_PID_ARCH: {
                int addr_size;
                receiveData(&context->game_pid, sizeof(pid_t));
                receiveData(&addr_size, sizeof(int));
                // MemAccess::init(context.game_pid, addr_size);
                std::cout << "MSGB_PID_ARCH " << context->game_pid << std::endl;
                std::cout << "MSGB_PID_ARCH " << addr_size << std::endl;
                break;
            }

            case MSGB_GIT_COMMIT: {
                std::string lib_commit = receiveString();
                std::cout << "MSGB_GIT_COMMIT" << lib_commit << std::endl;
                break;
            }

            default:
                std::cout << "unknown message " << message << std::endl;
                // loopExit();
                return;
        }

        message = receiveMessage();
    }

    /* Send information to the game */

    /* Send shared config size */
    sendMessage(MSGN_CONFIG_SIZE);
    int config_size = sizeof(SharedConfig);
    sendData(&config_size, sizeof(int));

    /* Send shared config */

    /* This is a bit hackish, change the initial time to the current realtime before
     * sending so that the game gets the correct time after restarting. */
    struct timespec it = {context->config.sc.initial_time_sec, context->config.sc.initial_time_nsec};
    context->config.sc.initial_time_sec = context->current_realtime_sec;
    context->config.sc.initial_time_nsec = context->current_realtime_nsec;
    sendMessage(MSGN_CONFIG);
    sendData(&context->config.sc, sizeof(SharedConfig));
    context->config.sc.initial_time_sec = it.tv_sec;
    context->config.sc.initial_time_nsec = it.tv_nsec;

    sendMessage(MSGN_INITIAL_FRAMECOUNT_TIME);
    sendData(&context->framecount, sizeof(uint64_t));
    sendData(&context->current_time_sec, sizeof(int64_t));
    sendData(&context->current_time_nsec, sizeof(int64_t));

    sendMessage(MSGN_END_INIT);
}

bool GameLoop::startFrameMessages() {
    context->draw_frame = true;

    /* Indicate if the current frame rendering will be skipped due to fast-forward */
    bool skip_draw_frame = false;

    int encoding_segment = 0;

    /* Wait for frame boundary */
    int message = receiveMessage();

    while (message != MSGB_START_FRAMEBOUNDARY) {
        GameInfo game_info;

        // std::cout << "[R] startFrameMessages: " << get_message_name(message) << std::endl;

        switch (message) {
            case MSGB_WINDOW_ID: {
                uint32_t int_window;
                receiveData(&int_window, sizeof(uint32_t));
                // TODO gameEvents->registerGameWindow(int_window);
                // context->game_window = static_cast<xcb_window_t>(int_window);

                break;
            }

            case MSGB_ALERT_MSG:
                /* Ask the UI thread to display the alert. He is in charge of
                 * freeing the string.
                 */
                // TODO emit alertToShow(QString(receiveString().c_str()));
                break;
            case MSGB_ENCODE_FAILED:
                // TODO context->config.sc.av_dumping = false;
                // context->config.sc_modified = true;
                // emit sharedConfigChanged();
                break;
            case MSGB_FRAMECOUNT_TIME:
                receiveData(&context->framecount, sizeof(uint64_t));
                receiveData(&context->current_time_sec, sizeof(uint64_t));
                receiveData(&context->current_time_nsec, sizeof(uint64_t));
                receiveData(&context->current_realtime_sec, sizeof(uint64_t));
                receiveData(&context->current_realtime_nsec, sizeof(uint64_t));
                context->new_realtime_sec = context->current_realtime_sec;
                context->new_realtime_nsec = context->current_realtime_nsec;

                // std::cout << "[R] current_time_sec: " << context->current_time_sec << std::endl;
                // std::cout << "[R] running: " << context->config.sc.running << std::endl;

                // if (context->config.sc.recording == SharedConfig::RECORDING_WRITE) {
                //     /* If the input editor is opened, recording does not truncate inputs */
                //     bool notTruncInputs = false;
                //     emit isInputEditorVisible(notTruncInputs);
                //
                //     if (!notTruncInputs || (context->framecount > context->config.sc.movie_framecount)) {
                //         context->config.sc.movie_framecount = context->framecount;
                //         movie.header->length_sec = context->current_time_sec - context->config.sc.initial_monotonic_time_sec;
                //         movie.header->length_nsec = context->current_time_nsec - context->config.sc.initial_monotonic_time_nsec;
                //         if (movie.header->length_nsec < 0) {
                //             movie.header->length_nsec += 1000000000;
                //             movie.header->length_sec--;
                //         }
                //     }
                // }

                /* Check and update the moviefile length when reaching the end of
                 * the movie, useful when variable framerate is being used */
                // else if (context->config.sc.recording == SharedConfig::RECORDING_READ &&
                //     context->framecount == context->config.sc.movie_framecount) {
                //
                //     uint64_t cur_sec = context->current_time_sec - context->config.sc.initial_monotonic_time_sec;
                //     uint64_t cur_nsec = context->current_time_nsec - context->config.sc.initial_monotonic_time_nsec;
                //
                //     if (movie.header->length_sec != cur_sec ||
                //         movie.header->length_nsec != cur_nsec) {
                //
                //         if (movie.header->length_sec != -1)
                //             emit alertToShow(QString("Movie length mismatch. Metadata stores %1.%2 seconds but end time is %3.%4 seconds.").arg(movie.header->length_sec).arg(movie.header->length_nsec, 9, 10, QChar('0')).arg(cur_sec).arg(cur_nsec, 9, 10, QChar('0')));
                //
                //         movie.header->length_sec = cur_sec;
                //         movie.header->length_nsec = cur_nsec;
                //         movie.inputs->wasModified();
                //     }
                // }

                /* When not recording, always truncate the movie */
                // if (context->config.sc.recording == SharedConfig::NO_RECORDING) {
                //     context->config.sc.movie_framecount = context->framecount;
                //     movie.header->length_sec = context->current_time_sec - context->config.sc.initial_monotonic_time_sec;
                //     movie.header->length_nsec = context->current_time_nsec - context->config.sc.initial_monotonic_time_nsec;
                //     if (movie.header->length_nsec < 0) {
                //         movie.header->length_nsec += 1000000000;
                //         movie.header->length_sec--;
                //     }
                // }

                break;
            case MSGB_GAMEINFO:
                receiveData(&game_info, sizeof(game_info));
                // emit gameInfoChanged(game_info);
                break;
            case MSGB_FPS:
                receiveData(&context->fps, sizeof(float));
                receiveData(&context->lfps, sizeof(float));
                break;
            case MSGB_ENCODING_SEGMENT:
                receiveData(&encoding_segment, sizeof(int));
                break;
            case MSGB_GETTIME_BACKTRACE: {
                int type;
                receiveData(&type, sizeof(int));
                uint64_t hash;
                receiveData(&hash, sizeof(uint64_t));
                std::string trace = receiveString();
                // emit getTimeTrace(type, static_cast<unsigned long long>(hash), trace);
            }
            break;
            case MSGB_NONDRAW_FRAME:
                context->draw_frame = false;
                break;

            case MSGB_SKIPDRAW_FRAME:
                skip_draw_frame = true;
                break;

            case MSGB_SYMBOL_ADDRESS: {
                std::string sym = receiveString();
                uint64_t addr = getSymbolAddress(sym.c_str(), context->gamepath.c_str());
                sendData(&addr, sizeof(uint64_t));
                break;
            }
            case MSGB_QUIT:
                if (!context->interactive) {
                    /* Exit the program when game has exit */
                    exit(0);
                }
                return true;
            case -1:
                std::cerr << "The connection to the game was lost. Exiting" << std::endl;
                return true;
            case -2:
                std::cerr << "The connection to the game was closed. Exiting" << std::endl;
                return true;
            case MSGN_START_FRAMEBOUNDARY:
                std::cerr << "huh " << get_message_name(message) << std::endl;
                break;
            default:
                std::cerr << "Got unknown message!!! " << get_message_name(message) << std::endl;
                return true;
        }
        message = receiveMessage();
    }

    // Lua::Callbacks::call(Lua::NamedLuaFunction::CallbackFrame);

    /* Store in movie and indicate the input editor if the current frame
     * is a draw frame or not */
    // movie.editor->setDraw(context->draw_frame);

    /* Send ram watches */
    if (context->draw_frame && !skip_draw_frame) {
        std::string ramwatch;
        // emit getRamWatch(ramwatch);
        while (!ramwatch.empty()) {
            sendMessage(MSGN_RAMWATCH);
            sendString(ramwatch);
            // emit getRamWatch(ramwatch);
        }
    }

    /* Execute the lua callback onPaint here */
    if (context->draw_frame && !skip_draw_frame) {
        // Lua::Callbacks::call(Lua::NamedLuaFunction::CallbackPaint);
    }

    sendMessage(MSGN_START_FRAMEBOUNDARY);

    return false;
}

void GameLoop::init() {
}

void GameLoop::loopExit() {
    closeSocket();
    remove_savestates(context);
    wait(nullptr);
    // TODO
}


void GameLoop::sleepSendPreview() {
    /* Sleep a bit to not surcharge the processor */
    struct timespec tim = {0, 33L * 1000L * 1000L};
    nanosleep(&tim, NULL);

    /* Send marker text if it has changed */
    static std::string old_marker_text;
    std::string text;
    // emit getMarkerText(text);
    if (old_marker_text != text) {
        old_marker_text = text;
        sendMessage(MSGN_MARKER);
        sendString(text);
    }

    /* Send a preview of inputs so that the game can display them
     * on the HUD */

    /* Don't preview when reading inputs */
    if (context->config.sc.recording == SharedConfig::RECORDING_READ) {
        sendMessage(MSGN_EXPOSE);
        return;
    }

    // TODO
    // static AllInputs preview_ai, last_preview_ai;
    // if (gameEvents->haveFocus()) {
    //     /* Format the keyboard and mouse state and save it in the AllInputs struct */
    //     context->config.km->buildAllInputs(preview_ai, context->game_window, context->config.sc, false,
    //                                        context->mouse_wheel);
    // }

    // /* Fill controller inputs from the controller input window. */
    // for (int j = 0; j < AllInputs::MAXJOYS; j++) {
    //     if (preview_ai.controllers[j]) {
    //         emit fillControllerInputs(*preview_ai.controllers[j], j);
    //     }
    // }

    // /* Send inputs if changed */
    // if (!(preview_ai == last_preview_ai)) {
    //     preview_ai.send(true);
    //     last_preview_ai = preview_ai;
    // }

    sendMessage(MSGN_EXPOSE);
}


void GameLoop::processInputs(AllInputs &ai) {
    ai.clear();

    auto si = SingleInput();
    si.type = SingleInput::IT_KEYBOARD;
    si.which = 0xff51;
    ai.setInput(si, 1);
}


void GameLoop::endFrameMessages(AllInputs &ai) {
    /* If the user stopped the game with the Stop button, don't write back
     * savefiles.*/
    if (context->status == Context::QUITTING) {
        context->config.sc.write_savefiles_on_exit = false;
        context->config.sc_modified = true;
    }

    /* If the game was restarted, write back savefiles.*/
    if (context->status == Context::RESTARTING) {
        context->config.sc.write_savefiles_on_exit = true;
        context->config.sc_modified = true;
    }

    /* Send shared config if modified */
    if (context->config.sc_modified) {
        /* Send config */
        sendMessage(MSGN_CONFIG);
        sendData(&context->config.sc, sizeof(SharedConfig));
        context->config.sc_modified = false;
    }

    /* Send dump file if modified */
    if (context->config.dumpfile_modified) {
        sendMessage(MSGN_DUMP_FILE);
        sendString(context->config.dumpfile);
        sendString(context->config.ffmpegoptions);
        context->config.dumpfile_modified = false;
    }

    /* Send inputs and end of frame */
    ai.send(false);

    if ((context->status == Context::QUITTING) || (context->status == Context::RESTARTING)) {
        sendMessage(MSGN_USERQUIT);
    }

    sendMessage(MSGN_END_FRAMEBOUNDARY);
}

void GameLoop::start() {
    init();
    initProcessMessages();

    while (true) {
        bool exitMsg = startFrameMessages();
        if (exitMsg) {
            loopExit();
            return;
        }

        /* We are at a frame boundary */
        /* If we did not yet receive the game window id, just make the game running */
        bool endInnerLoop = false;
        if (context->game_window)
            do {
                /* Check if game is still running */
                int ret = waitpid(fork_pid, nullptr, WNOHANG);
                if (ret == fork_pid) {
                    loopExit();
                    return;
                }

                //int eventFlag = gameEvents->handleEvent();

                endInnerLoop = context->config.sc.running ||
                    // (eventFlag & GameEvents::RETURN_FLAG_ADVANCE) ||
                    (context->status == Context::QUITTING);

                if (!endInnerLoop) {
                    sleepSendPreview();
                }
            } while (!endInnerLoop);

        if (context->framecount == 10) {
            sendMessage(MSGN_SAVESTATE_INDEX);
            int id = 0;
            sendData(&id, sizeof(int));

            sendMessage(MSGN_SAVESTATE_PATH);
            sendString("/tmp/testsavestate");

            sendMessage(MSGN_SAVESTATE);
            int message = receiveMessage();
            std::cout << "save result " << get_message_name(message) << std::endl;
        }

        if (context->framecount == 50) {
            sendMessage(MSGN_SAVESTATE_INDEX);
            int id = 0;
            sendData(&id, sizeof(int));

            sendMessage(MSGN_SAVESTATE_PATH);
            sendString("/tmp/testsavestate");

            sendMessage(MSGN_LOADSTATE);
            int message = receiveMessage();
            std::cout << "load result " << get_message_name(message) << std::endl;

            if (message == MSGB_LOADING_SUCCEEDED) {

                sendMessage(MSGN_CONFIG);
                sendData(&context->config.sc, sizeof(SharedConfig));

                int m = receiveMessage();
                if (m != MSGB_FRAMECOUNT_TIME) {
                    std::cerr << "Got wrong message after state loading" << std::endl;
                    exit(1);
                }

                receiveData(&context->framecount, sizeof(uint64_t));
                receiveData(&context->current_time_sec, sizeof(uint64_t));
                receiveData(&context->current_time_nsec, sizeof(uint64_t));
                receiveData(&context->current_realtime_sec, sizeof(uint64_t));
                receiveData(&context->current_realtime_nsec, sizeof(uint64_t));
                context->new_realtime_sec = context->current_realtime_sec;
                context->new_realtime_nsec = context->current_realtime_nsec;

				// context->config.sc.running = false;
                // sendMessage(MSGN_CONFIG);
                // sendData(&context->config.sc, sizeof(SharedConfig));
				// sendMessage(MSGN_USERQUIT);
				// exit(0);
            }

        }


        AllInputs ai;
        processInputs(ai);

        // TODO shared config
        // TODO pause
        // TODO seek

        // std::this_thread::sleep_for(std::chrono::milliseconds(100));

        endFrameMessages(ai);
    }
}

bool startGame = false;
int main() {
    Context c = {};
    Context *context = &c;
    context->gamepath = "/home/jakob/.cache/rust/release/examples/breakout";
    context->libtaspath = "/home/jakob/dev/contrib/libTAS/build/src/library/libtas.so";
    context->config.sc.logging_level = LL_INFO;
    context->config.sc.logging_include_flags = LCF_ALL;
    context->config.sc.running = true;
    context->config.sc.osd = true;

    context->config.sc.initial_framerate_num = 30;

    // context->config.sc.fastforward = true;
    // context->config.sc.fastforward_render = SharedConfig::FF_RENDER_SOME;

    if (startGame) {
        int err = removeSocket();
        if (err != 0) {
            std::cerr << "Could not remove socket file /tmp/libTAS.socket: " << strerror(err) << std::endl;
        }

        pid_t fork_pid = fork();
        if (fork_pid == 0) {
            GameThread::launch(context);
        }
    }

    GameLoop game(context);
    game.start();
}
