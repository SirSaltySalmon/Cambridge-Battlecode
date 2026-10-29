// Dispatch draft and gameplay; a failed turn is logged and still yields.
#include <cambc.h>
#include "board.h"
#include "bot.h"
#include "draft.h"

int main() {
    Bot bot;
    while (true) {
        try {
            if (cambc::is_draft())
                draft::play();
            else
                bot.play_turn();
        } catch (const std::exception& error) {
            say(std::string("bot error: ") + error_text(error));
        } catch (...) {
            say("bot error: unknown exception");
        }

        { volatile int sink = 0; for (int i = 0; i < 300; ++i) sink += i; }
        cambc::end_turn();
    }
}
