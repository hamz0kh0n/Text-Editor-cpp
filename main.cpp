#include "editor.h"

int main() {
    DocumentState doc;
    loadDefaults(doc);

    int choice = 0;
    bool running = true;
    do {
        showMenu(doc);
        if (!readInt("  Choice: ", 0, 15, choice)) break;   // end of input -> leave cleanly
        running = handleChoice(choice, doc);
    } while (running);

    cleanupDocument(doc);   // free every node and command
    return 0;
}
