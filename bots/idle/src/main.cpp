// Idle sparring bot: drafts at random (yields) and never moves. Used to time the other bot's unopposed farming.
#include <cambc.h>
int main() {
    while (true) cambc::end_turn();
}
