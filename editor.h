#pragma once
// ============================================================================
//  DSA Text Editor
//  Doubly Linked List text buffer + linked-list Stacks for Undo / Redo.
//  Design rules: structs only, no global variables, DocumentState passed
//  by reference everywhere, recursive keyword search, menu-driven console UI.
// ============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cctype>

// ─────────────────────────────────────────
//  STRUCTS
// ─────────────────────────────────────────

struct LineNode {
    std::string data;
    LineNode*   prev;
    LineNode*   next;
    explicit LineNode(const std::string& text) : data(text), prev(nullptr), next(nullptr) {}
};

enum CommandType { INSERT, DEL, EDIT };

// One reversible action. `group` ties several commands together so that a
// compound action (replace-all, move line, load-append) undoes in ONE step.
struct Command {
    CommandType type;
    int         lineNumber;
    std::string oldData;
    std::string newData;
    int         group;
    Command*    next;
    Command(CommandType t, int ln, const std::string& od, const std::string& nd, int g)
        : type(t), lineNumber(ln), oldData(od), newData(nd), group(g), next(nullptr) {}
};

struct Stack {
    Command* top;
    int      size;
    Stack() : top(nullptr), size(0) {}
};

struct DocumentState {
    LineNode*   head;
    LineNode*   tail;          // O(1) append + faster access to lines near the end
    int         lineCount;
    Stack       undoStack;
    Stack       redoStack;
    int         nextGroupId;   // source of unique group ids
    bool        modified;      // unsaved-changes flag
    std::string currentFile;   // remembers the last saved/loaded file name
    std::string clipboard;     // line clipboard
    bool        hasClipboard;
    DocumentState()
        : head(nullptr), tail(nullptr), lineCount(0), nextGroupId(0),
          modified(false), hasClipboard(false) {}
};

// ─────────────────────────────────────────
//  STACK OPERATIONS
// ─────────────────────────────────────────

inline void pushNode(Stack& s, Command* cmd) {
    cmd->next = s.top;
    s.top = cmd;
    s.size++;
}

inline void pushCommand(Stack& s, CommandType type, int ln,
                        const std::string& oldD, const std::string& newD, int group) {
    pushNode(s, new Command(type, ln, oldD, newD, group));
}

inline Command* popCommand(Stack& s) {
    if (!s.top) return nullptr;
    Command* cmd = s.top;
    s.top = s.top->next;
    s.size--;
    cmd->next = nullptr;
    return cmd;
}

inline void clearStack(Stack& s) {
    while (s.top) {
        Command* tmp = s.top;
        s.top = s.top->next;
        delete tmp;
    }
    s.size = 0;
}

// ─────────────────────────────────────────
//  LOW-LEVEL DLL OPERATIONS (no history, no printing)
// ─────────────────────────────────────────

// Walks from whichever end is closer: at most n/2 steps.
inline LineNode* getNodeAt(DocumentState& doc, int pos) {
    if (pos < 1 || pos > doc.lineCount) return nullptr;
    LineNode* cur;
    if (pos <= doc.lineCount / 2) {
        cur = doc.head;
        for (int i = 1; i < pos; i++) cur = cur->next;
    } else {
        cur = doc.tail;
        for (int i = doc.lineCount; i > pos; i--) cur = cur->prev;
    }
    return cur;
}

inline bool rawInsert(DocumentState& doc, int pos, const std::string& text) {
    if (pos < 1 || pos > doc.lineCount + 1) return false;
    LineNode* node = new LineNode(text);
    if (!doc.head) {
        doc.head = doc.tail = node;
    } else if (pos == 1) {
        node->next = doc.head;
        doc.head->prev = node;
        doc.head = node;
    } else if (pos == doc.lineCount + 1) {
        node->prev = doc.tail;
        doc.tail->next = node;
        doc.tail = node;
    } else {
        LineNode* after = getNodeAt(doc, pos);   // node currently sitting at pos
        node->prev = after->prev;
        node->next = after;
        after->prev->next = node;
        after->prev = node;
    }
    doc.lineCount++;
    return true;
}

inline bool rawDelete(DocumentState& doc, int pos, std::string* saved = nullptr) {
    LineNode* node = getNodeAt(doc, pos);
    if (!node) return false;
    if (saved) *saved = node->data;
    if (node->prev) node->prev->next = node->next; else doc.head = node->next;
    if (node->next) node->next->prev = node->prev; else doc.tail = node->prev;
    delete node;
    doc.lineCount--;
    return true;
}

inline bool rawEdit(DocumentState& doc, int pos, const std::string& text, std::string* old = nullptr) {
    LineNode* node = getNodeAt(doc, pos);
    if (!node) return false;
    if (old) *old = node->data;
    node->data = text;
    return true;
}

// ─────────────────────────────────────────
//  EDITING OPERATIONS (recorded for undo/redo, return success)
//  group = 0 -> the call is its own undo step
// ─────────────────────────────────────────

inline int beginGroup(DocumentState& doc) { return ++doc.nextGroupId; }

inline void recordCommand(DocumentState& doc, CommandType t, int ln,
                          const std::string& od, const std::string& nd, int group) {
    pushCommand(doc.undoStack, t, ln, od, nd, group);
    clearStack(doc.redoStack);
    doc.modified = true;
}

inline bool insertLine(DocumentState& doc, int pos, const std::string& text, int group = 0) {
    if (!rawInsert(doc, pos, text)) return false;
    if (group == 0) group = beginGroup(doc);
    recordCommand(doc, INSERT, pos, "", text, group);
    return true;
}

inline bool deleteLine(DocumentState& doc, int pos, int group = 0, std::string* removed = nullptr) {
    std::string saved;
    if (!rawDelete(doc, pos, &saved)) return false;
    if (removed) *removed = saved;
    if (group == 0) group = beginGroup(doc);
    recordCommand(doc, DEL, pos, saved, "", group);
    return true;
}

inline bool editLine(DocumentState& doc, int pos, const std::string& newText, int group = 0) {
    std::string oldText;
    if (!rawEdit(doc, pos, newText, &oldText)) return false;
    if (group == 0) group = beginGroup(doc);
    recordCommand(doc, EDIT, pos, oldText, newText, group);
    return true;
}

// Move the line at `from` so that it ends up at line `to` (one undo step).
inline bool moveLine(DocumentState& doc, int from, int to) {
    if (from < 1 || from > doc.lineCount || to < 1 || to > doc.lineCount || from == to) return false;
    int g = beginGroup(doc);
    std::string text;
    deleteLine(doc, from, g, &text);
    insertLine(doc, to, text, g);
    return true;
}

// ─────────────────────────────────────────
//  CLIPBOARD
// ─────────────────────────────────────────

inline bool copyLine(DocumentState& doc, int pos) {
    LineNode* n = getNodeAt(doc, pos);
    if (!n) return false;
    doc.clipboard = n->data;
    doc.hasClipboard = true;
    return true;
}

inline bool cutLine(DocumentState& doc, int pos) {
    std::string removed;
    if (!deleteLine(doc, pos, 0, &removed)) return false;
    doc.clipboard = removed;
    doc.hasClipboard = true;
    return true;
}

inline bool pasteLine(DocumentState& doc, int pos) {
    if (!doc.hasClipboard) return false;
    return insertLine(doc, pos, doc.clipboard);
}

// ─────────────────────────────────────────
//  UNDO / REDO  (commands are MOVED between stacks, never re-allocated)
// ─────────────────────────────────────────

// Returns how many primitive commands were reversed (0 = nothing to undo).
inline int undoOperation(DocumentState& doc) {
    if (!doc.undoStack.top) return 0;
    int g = doc.undoStack.top->group, n = 0;
    while (doc.undoStack.top && doc.undoStack.top->group == g) {
        Command* c = popCommand(doc.undoStack);
        if      (c->type == INSERT) rawDelete(doc, c->lineNumber);
        else if (c->type == DEL)    rawInsert(doc, c->lineNumber, c->oldData);
        else                        rawEdit  (doc, c->lineNumber, c->oldData);
        pushNode(doc.redoStack, c);
        n++;
    }
    doc.modified = true;
    return n;
}

inline int redoOperation(DocumentState& doc) {
    if (!doc.redoStack.top) return 0;
    int g = doc.redoStack.top->group, n = 0;
    while (doc.redoStack.top && doc.redoStack.top->group == g) {
        Command* c = popCommand(doc.redoStack);
        if      (c->type == INSERT) rawInsert(doc, c->lineNumber, c->newData);
        else if (c->type == DEL)    rawDelete(doc, c->lineNumber);
        else                        rawEdit  (doc, c->lineNumber, c->newData);
        pushNode(doc.undoStack, c);
        n++;
    }
    doc.modified = true;
    return n;
}

// ─────────────────────────────────────────
//  SEARCH  &  REPLACE  (both recursive, no loops over the list)
// ─────────────────────────────────────────

inline std::string toLowerStr(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Prints every matching line, returns the number of matching lines.
// When caseSensitive is false the caller passes an already-lowercased keyword.
inline int searchRecursive(LineNode* node, const std::string& keyword, int lineNum, bool caseSensitive) {
    if (!node) return 0;                                              // base case
    const std::string hay = caseSensitive ? node->data : toLowerStr(node->data);
    int hit = 0;
    if (hay.find(keyword) != std::string::npos) {
        std::cout << "  Line " << lineNum << ": " << node->data << "\n";
        hit = 1;
    }
    return hit + searchRecursive(node->next, keyword, lineNum + 1, caseSensitive);
}

inline int searchDocument(DocumentState& doc, const std::string& keyword, bool caseSensitive = false) {
    if (keyword.empty()) return 0;
    return searchRecursive(doc.head, caseSensitive ? keyword : toLowerStr(keyword), 1, caseSensitive);
}

// Replaces every occurrence of `from` in `s`; returns the number of replacements.
inline int replaceInString(const std::string& s, const std::string& from, const std::string& to,
                           bool caseSensitive, std::string& out) {
    const std::string hay    = caseSensitive ? s    : toLowerStr(s);
    const std::string needle = caseSensitive ? from : toLowerStr(from);
    out.clear();
    size_t i = 0;
    int count = 0;
    while (true) {
        size_t p = hay.find(needle, i);
        if (p == std::string::npos) { out.append(s, i, std::string::npos); break; }
        out.append(s, i, p - i);
        out += to;
        i = p + needle.size();
        count++;
    }
    return count;
}

inline void replaceRecursive(DocumentState& doc, LineNode* node, int lineNum,
                             const std::string& from, const std::string& to,
                             bool caseSensitive, int group, int& linesChanged, int& occurrences) {
    if (!node) return;
    LineNode* nextNode = node->next;      // editLine never relinks, but be safe
    std::string replaced;
    int c = replaceInString(node->data, from, to, caseSensitive, replaced);
    if (c > 0) {
        editLine(doc, lineNum, replaced, group);
        linesChanged++;
        occurrences += c;
    }
    replaceRecursive(doc, nextNode, lineNum + 1, from, to, caseSensitive, group, linesChanged, occurrences);
}

// The whole replace-all is ONE undo step.
inline int replaceAll(DocumentState& doc, const std::string& from, const std::string& to,
                      bool caseSensitive, int& occurrences) {
    occurrences = 0;
    if (from.empty()) return 0;
    int linesChanged = 0;
    int g = beginGroup(doc);
    replaceRecursive(doc, doc.head, 1, from, to, caseSensitive, g, linesChanged, occurrences);
    return linesChanged;
}

// ─────────────────────────────────────────
//  DISPLAY & STATISTICS
// ─────────────────────────────────────────

inline void displayDocument(DocumentState& doc) {
    if (!doc.head) { std::cout << "  [Document is empty]\n"; return; }
    int width = (int)std::to_string(doc.lineCount).size();
    LineNode* cur = doc.head;
    int i = 1;
    while (cur) {
        std::cout << "  " << std::setw(width) << i++ << " | " << cur->data << "\n";
        cur = cur->next;
    }
}

inline void showStatistics(DocumentState& doc) {
    int words = 0;
    long chars = 0;
    for (LineNode* cur = doc.head; cur; cur = cur->next) {
        chars += (long)cur->data.size();
        std::istringstream iss(cur->data);
        std::string w;
        while (iss >> w) words++;
    }
    std::cout << "  Lines      : " << doc.lineCount << "\n"
              << "  Words      : " << words << "\n"
              << "  Characters : " << chars << "\n"
              << "  Undo steps : " << doc.undoStack.size << " command(s)\n"
              << "  Redo steps : " << doc.redoStack.size << " command(s)\n"
              << "  File       : " << (doc.currentFile.empty() ? "(unsaved)" : doc.currentFile)
              << (doc.modified ? "  [modified]" : "") << "\n";
}

// ─────────────────────────────────────────
//  MEMORY CLEANUP
// ─────────────────────────────────────────

inline void clearDocument(DocumentState& doc) {
    LineNode* cur = doc.head;
    while (cur) {
        LineNode* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
    doc.head = doc.tail = nullptr;
    doc.lineCount = 0;
}

inline void cleanupDocument(DocumentState& doc) {
    clearDocument(doc);
    clearStack(doc.undoStack);
    clearStack(doc.redoStack);
}

// ─────────────────────────────────────────
//  FILE I/O
// ─────────────────────────────────────────

inline bool saveFile(DocumentState& doc, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) return false;
    for (LineNode* cur = doc.head; cur; cur = cur->next) file << cur->data << "\n";
    if (!file) return false;
    doc.currentFile = filename;
    doc.modified = false;
    return true;
}

// replaceDoc = true : wipe the current document (and history) and load the file.
// replaceDoc = false: append the file's lines at the end as ONE undoable step.
// Returns the number of lines loaded, or -1 if the file could not be opened.
inline int loadFile(DocumentState& doc, const std::string& filename, bool replaceDoc) {
    std::ifstream file(filename);
    if (!file) return -1;                       // nothing is touched on failure
    int g = 0;
    if (replaceDoc) {
        cleanupDocument(doc);
        doc.modified = false;
    } else {
        g = beginGroup(doc);
    }
    std::string line;
    int n = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // Windows files
        if (replaceDoc) rawInsert(doc, doc.lineCount + 1, line);
        else            insertLine(doc, doc.lineCount + 1, line, g);
        n++;
    }
    if (replaceDoc) doc.currentFile = filename;
    return n;
}

// ─────────────────────────────────────────
//  DEFAULT DATA (loaded at start-up, as required by the course)
// ─────────────────────────────────────────

inline void loadDefaults(DocumentState& doc) {
    rawInsert(doc, 1, "Welcome to the DSA Text Editor!");
    rawInsert(doc, 2, "Built with a Doubly Linked List and Stack.");
    rawInsert(doc, 3, "Use the menu below to edit this document.");
    rawInsert(doc, 4, "You can undo and redo any operation.");
    rawInsert(doc, 5, "Happy editing!");
    doc.modified = false;
}

// ─────────────────────────────────────────
//  SAFE INPUT HELPERS (re-ask on invalid input)
//  All return false on end-of-input so the program can exit cleanly.
// ─────────────────────────────────────────

inline bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    return (bool)std::getline(std::cin, out);
}

inline bool readInt(const std::string& prompt, int lo, int hi, int& out) {
    std::string s;
    while (true) {
        if (!readLine(prompt, s)) return false;
        try {
            size_t used = 0;
            int v = std::stoi(s, &used);
            while (used < s.size() && std::isspace((unsigned char)s[used])) used++;
            if (used == s.size() && v >= lo && v <= hi) { out = v; return true; }
        } catch (...) {}
        std::cout << "  [!] Please enter a whole number from " << lo << " to " << hi << ".\n";
    }
}

inline bool readYesNo(const std::string& prompt, bool& yes) {
    std::string s;
    while (true) {
        if (!readLine(prompt + " (y/n): ", s)) return false;
        if (!s.empty() && (s[0] == 'y' || s[0] == 'Y')) { yes = true;  return true; }
        if (!s.empty() && (s[0] == 'n' || s[0] == 'N')) { yes = false; return true; }
        std::cout << "  [!] Please answer y or n.\n";
    }
}

// ─────────────────────────────────────────
//  MENU
// ─────────────────────────────────────────

inline void showMenu(DocumentState& doc) {
    std::cout << "\n+------------------------------------------+\n"
              << "|            DSA TEXT EDITOR               |\n"
              << "+------------------------------------------+\n"
              << "|  1. Display Document     8. Redo         |\n"
              << "|  2. Insert Line          9. Copy Line    |\n"
              << "|  3. Delete Line         10. Cut Line     |\n"
              << "|  4. Edit Line           11. Paste Line   |\n"
              << "|  5. Search (Recursive)  12. Move Line    |\n"
              << "|  6. Find & Replace All  13. Statistics   |\n"
              << "|  7. Undo                14. Save to File |\n"
              << "|  0. Exit                15. Load File    |\n"
              << "+------------------------------------------+\n"
              << "  " << doc.lineCount << " line(s)"
              << "  | undo: " << doc.undoStack.size << "  redo: " << doc.redoStack.size
              << (doc.modified ? "  | *unsaved*" : "") << "\n";
}

// Returns false when the program should exit.
inline bool handleChoice(int choice, DocumentState& doc) {
    int pos = 0, pos2 = 0;
    std::string text, keyword, replacement, filename;
    bool yes = false;

    switch (choice) {
    case 1:
        std::cout << "\n--- Document ---\n";
        displayDocument(doc);
        break;

    case 2:
        if (!readInt("  Insert at line (1 to " + std::to_string(doc.lineCount + 1) + "): ", 1, doc.lineCount + 1, pos)) return false;
        if (!readLine("  Text: ", text)) return false;
        insertLine(doc, pos, text);
        std::cout << "  [OK] Line inserted.\n";
        break;

    case 3:
        if (doc.lineCount == 0) { std::cout << "  [!] Document is empty.\n"; break; }
        if (!readInt("  Delete line (1 to " + std::to_string(doc.lineCount) + "): ", 1, doc.lineCount, pos)) return false;
        deleteLine(doc, pos);
        std::cout << "  [OK] Line deleted.\n";
        break;

    case 4:
        if (doc.lineCount == 0) { std::cout << "  [!] Document is empty.\n"; break; }
        if (!readInt("  Edit line (1 to " + std::to_string(doc.lineCount) + "): ", 1, doc.lineCount, pos)) return false;
        std::cout << "  Current: " << getNodeAt(doc, pos)->data << "\n";
        if (!readLine("  New text: ", text)) return false;
        editLine(doc, pos, text);
        std::cout << "  [OK] Line updated.\n";
        break;

    case 5: {
        if (!readLine("  Keyword: ", keyword)) return false;
        if (keyword.empty()) { std::cout << "  [!] Keyword cannot be empty.\n"; break; }
        if (!readYesNo("  Case sensitive?", yes)) return false;
        std::cout << "\n--- Search Results ---\n";
        int hits = searchDocument(doc, keyword, yes);
        if (hits == 0) std::cout << "  [!] Keyword not found.\n";
        else           std::cout << "  " << hits << " matching line(s).\n";
        break;
    }

    case 6: {
        if (!readLine("  Find: ", keyword)) return false;
        if (keyword.empty()) { std::cout << "  [!] Search text cannot be empty.\n"; break; }
        if (!readLine("  Replace with: ", replacement)) return false;
        if (!readYesNo("  Case sensitive?", yes)) return false;
        int occ = 0;
        int lines = replaceAll(doc, keyword, replacement, yes, occ);
        if (lines == 0) std::cout << "  [!] Nothing to replace.\n";
        else std::cout << "  [OK] Replaced " << occ << " occurrence(s) on " << lines
                       << " line(s). (Undo reverts all of it in one step.)\n";
        break;
    }

    case 7: {
        int n = undoOperation(doc);
        if (n == 0) std::cout << "  [!] Nothing to undo.\n";
        else        std::cout << "  [OK] Undo successful" << (n > 1 ? " (" + std::to_string(n) + " changes reverted)" : "") << ".\n";
        break;
    }

    case 8: {
        int n = redoOperation(doc);
        if (n == 0) std::cout << "  [!] Nothing to redo.\n";
        else        std::cout << "  [OK] Redo successful" << (n > 1 ? " (" + std::to_string(n) + " changes reapplied)" : "") << ".\n";
        break;
    }

    case 9:
    case 10:
        if (doc.lineCount == 0) { std::cout << "  [!] Document is empty.\n"; break; }
        if (!readInt(std::string("  ") + (choice == 9 ? "Copy" : "Cut") + " line (1 to " + std::to_string(doc.lineCount) + "): ", 1, doc.lineCount, pos)) return false;
        if (choice == 9) copyLine(doc, pos); else cutLine(doc, pos);
        std::cout << "  [OK] " << (choice == 9 ? "Copied" : "Cut") << " to clipboard: " << doc.clipboard << "\n";
        break;

    case 11:
        if (!doc.hasClipboard) { std::cout << "  [!] Clipboard is empty.\n"; break; }
        if (!readInt("  Paste at line (1 to " + std::to_string(doc.lineCount + 1) + "): ", 1, doc.lineCount + 1, pos)) return false;
        pasteLine(doc, pos);
        std::cout << "  [OK] Pasted.\n";
        break;

    case 12:
        if (doc.lineCount < 2) { std::cout << "  [!] Need at least 2 lines to move one.\n"; break; }
        if (!readInt("  Move which line (1 to " + std::to_string(doc.lineCount) + "): ", 1, doc.lineCount, pos)) return false;
        if (!readInt("  To position (1 to " + std::to_string(doc.lineCount) + "): ", 1, doc.lineCount, pos2)) return false;
        if (moveLine(doc, pos, pos2)) std::cout << "  [OK] Line moved.\n";
        else                          std::cout << "  [!] Source and target are the same line.\n";
        break;

    case 13:
        std::cout << "\n--- Statistics ---\n";
        showStatistics(doc);
        break;

    case 14:
        if (!doc.currentFile.empty()) {
            if (!readLine("  Filename [" + doc.currentFile + "]: ", filename)) return false;
            if (filename.empty()) filename = doc.currentFile;
        } else {
            if (!readLine("  Filename: ", filename)) return false;
        }
        if (filename.empty()) { std::cout << "  [!] Filename cannot be empty.\n"; break; }
        if (saveFile(doc, filename)) std::cout << "  [OK] Saved to " << filename << "\n";
        else                         std::cout << "  [!] Could not open file for writing.\n";
        break;

    case 15: {
        if (!readLine("  Filename: ", filename)) return false;
        if (filename.empty()) { std::cout << "  [!] Filename cannot be empty.\n"; break; }
        bool replaceDoc = true;
        if (doc.lineCount > 0) {
            if (!readYesNo("  Replace current document? (n = append at end)", replaceDoc)) return false;
        }
        if (replaceDoc && doc.modified) {
            if (!readYesNo("  You have unsaved changes. Discard them?", yes)) return false;
            if (!yes) { std::cout << "  Load cancelled.\n"; break; }
        }
        int n = loadFile(doc, filename, replaceDoc);
        if (n < 0) std::cout << "  [!] File not found.\n";
        else       std::cout << "  [OK] Loaded " << n << " line(s) from " << filename << "\n";
        break;
    }

    case 0:
        if (doc.modified) {
            if (!readYesNo("  You have unsaved changes. Exit anyway?", yes)) return false;
            if (!yes) return true;
        }
        std::cout << "  Goodbye!\n";
        return false;

    default:
        std::cout << "  [!] Invalid choice.\n";
    }
    return true;
}
