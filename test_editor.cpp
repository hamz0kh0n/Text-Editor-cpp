// Automated tests for editor.h  —  build: g++ -std=c++11 test_editor.cpp -o test_editor
#include "editor.h"
#include <cstdio>

static int passed = 0, failed = 0;
#define CHECK(cond) do { if (cond) passed++; else { failed++; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static std::string dump(DocumentState& d) {           // forward walk
    std::string s;
    for (LineNode* c = d.head; c; c = c->next) s += c->data + "|";
    return s;
}
static std::string dumpBack(DocumentState& d) {       // backward walk (checks prev/tail links)
    std::string s;
    for (LineNode* c = d.tail; c; c = c->prev) s = c->data + "|" + s;
    return s;
}
static bool consistent(DocumentState& d) { return dump(d) == dumpBack(d); }

int main() {
    { DocumentState d;                                   // empty document
      CHECK(!deleteLine(d, 1)); CHECK(!editLine(d, 1, "x"));
      CHECK(undoOperation(d) == 0); CHECK(redoOperation(d) == 0);
      CHECK(searchDocument(d, "a") == 0);
      CHECK(insertLine(d, 1, "A")); CHECK(d.head == d.tail); CHECK(consistent(d));
      CHECK(deleteLine(d, 1)); CHECK(d.head == nullptr && d.tail == nullptr);
      cleanupDocument(d); }

    { DocumentState d;                                   // insert at head / middle / tail
      insertLine(d, 1, "B"); insertLine(d, 1, "A"); insertLine(d, 3, "D"); insertLine(d, 3, "C");
      CHECK(dump(d) == "A|B|C|D|"); CHECK(consistent(d));
      CHECK(!insertLine(d, 0, "bad")); CHECK(!insertLine(d, 9, "bad"));
      CHECK(getNodeAt(d, 1)->data == "A" && getNodeAt(d, 4)->data == "D" && getNodeAt(d, 3)->data == "C");
      cleanupDocument(d); }

    { DocumentState d;                                   // undo / redo of every command type
      insertLine(d, 1, "one"); insertLine(d, 2, "two"); editLine(d, 1, "ONE"); deleteLine(d, 2);
      CHECK(dump(d) == "ONE|");
      undoOperation(d); CHECK(dump(d) == "ONE|two|");
      undoOperation(d); CHECK(dump(d) == "one|two|");
      undoOperation(d); undoOperation(d); CHECK(dump(d) == ""); CHECK(undoOperation(d) == 0);
      redoOperation(d); redoOperation(d); redoOperation(d); redoOperation(d);
      CHECK(dump(d) == "ONE|"); CHECK(redoOperation(d) == 0); CHECK(consistent(d));
      undoOperation(d); insertLine(d, 1, "new");        // new action clears redo
      CHECK(d.redoStack.size == 0);
      cleanupDocument(d); }

    { DocumentState d;                                   // grouped ops undo in ONE step
      loadDefaults(d);
      insertLine(d, 6, "the cat"); insertLine(d, 7, "The CAT");
      int occ = 0; int lines = replaceAll(d, "cat", "dog", false, occ);
      CHECK(lines == 2 && occ == 2); CHECK(getNodeAt(d, 6)->data == "the dog");
      CHECK(getNodeAt(d, 7)->data == "The dog");
      CHECK(undoOperation(d) == 2); CHECK(getNodeAt(d, 7)->data == "The CAT");
      CHECK(redoOperation(d) == 2); CHECK(getNodeAt(d, 6)->data == "the dog");
      int occ2 = 0; CHECK(replaceAll(d, "CAT", "x", true, occ2) == 0);   // case-sensitive: no match now
      CHECK(replaceAll(d, "", "x", true, occ2) == 0);
      cleanupDocument(d); }

    { DocumentState d;                                   // move + clipboard
      insertLine(d, 1, "A"); insertLine(d, 2, "B"); insertLine(d, 3, "C");
      CHECK(moveLine(d, 1, 3)); CHECK(dump(d) == "B|C|A|"); CHECK(consistent(d));
      undoOperation(d); CHECK(dump(d) == "A|B|C|");
      CHECK(!moveLine(d, 2, 2)); CHECK(!moveLine(d, 0, 1));
      CHECK(!pasteLine(d, 1)); CHECK(copyLine(d, 2)); CHECK(pasteLine(d, 4)); CHECK(dump(d) == "A|B|C|B|");
      CHECK(cutLine(d, 1)); CHECK(dump(d) == "B|C|B|"); CHECK(d.clipboard == "A");
      cleanupDocument(d); }

    { DocumentState d;                                   // search
      insertLine(d, 1, "Hello World"); insertLine(d, 2, "hello again"); insertLine(d, 3, "bye");
      CHECK(searchDocument(d, "hello", false) == 2); CHECK(searchDocument(d, "hello", true) == 1);
      CHECK(searchDocument(d, "zzz", false) == 0);
      cleanupDocument(d); }

    { DocumentState d;                                   // large list: recursion + ends
      for (int i = 1; i <= 2000; i++) insertLine(d, i, "line " + std::to_string(i));
      CHECK(d.lineCount == 2000); CHECK(getNodeAt(d, 1999)->data == "line 1999");
      CHECK(searchDocument(d, "line 1500", true) == 1); CHECK(consistent(d));
      cleanupDocument(d); CHECK(d.lineCount == 0 && d.undoStack.size == 0); }

    { DocumentState d;                                   // file round-trip, CRLF, append, failure
      loadDefaults(d); CHECK(saveFile(d, "t_out.txt")); CHECK(!d.modified); CHECK(d.currentFile == "t_out.txt");
      { std::ofstream f("t_crlf.txt", std::ios::binary); f << "x\r\ny\r\n"; }
      CHECK(loadFile(d, "t_crlf.txt", true) == 2); CHECK(dump(d) == "x|y|"); CHECK(!d.modified);
      CHECK(loadFile(d, "t_out.txt", false) == 5); CHECK(d.lineCount == 7);
      undoOperation(d); CHECK(dump(d) == "x|y|");        // append undone in one step
      CHECK(loadFile(d, "does_not_exist.txt", true) == -1); CHECK(dump(d) == "x|y|");
      std::remove("t_out.txt"); std::remove("t_crlf.txt");
      cleanupDocument(d); }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
