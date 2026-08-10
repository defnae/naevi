// main.c

#include <headers/naevi/naevi.h>

#include <headers/abi.h>
#include <headers/attr.h>

#include <headers/std/int.h>
#include <headers/std/char.h>
#include <headers/std/bool.h>
#include <headers/std/def.h>
#include <headers/std/str.h>

#define LINE_SCRATCH_BUFFER_SIZE (1024 * 64)
#define SAVE_CHUNK_BUFFER_SIZE (1024 * 64)
#define UNDO_HISTORY_LIMIT 1000

typedef struct {
    PN* Root;

    char8_t* Buffer;
    size_t Length;
    size_t* Newlines;
    size_t NewlineCount;

    char8_t* AddBuffer;
    size_t AddLength;
    size_t AddCapacity;

    PN** UndoStack;
    size_t UndoCount;
    size_t UndoCapacity;
    PN** RedoStack;
    size_t RedoCount;
    size_t RedoCapacity;

    size_t CursorRow;
    size_t CursorColumn;
    size_t TopLineIndex;
    size_t StatusLength;
    size_t CommandLineLength;
    size_t OutputLength;

    uint32_t RNGState;
    termios Termios;

    unsigned short ScreenRows;
    unsigned short ScreenColumns;

    uint8_t CurrentMode;
    bool Dirty;
    bool Running;
    bool Force;
    char8_t Character;
    bool Pushback;
    char8_t Lead;
    bool Undo;
    char8_t pad[4];

    char8_t Filename[TEXT_BUFFER_SIZE];
    char8_t StatusBuffer[TEXT_BUFFER_SIZE];
    char8_t CommandLineBuffer[TEXT_BUFFER_SIZE];
    char8_t OutputBuffer[OUTPUT_BUFFER_MAXIMUM];
    char8_t ScratchLineBuffer[LINE_SCRATCH_BUFFER_SIZE];
    char8_t ScratchSaveBuffer[SAVE_CHUNK_BUFFER_SIZE];
} Globals;

static Globals GlobalData;
static Globals* G = &GlobalData;

static ABI bool ready(int ms);
static ABI char8_t* ull2s(uint64_t value, char8_t* buffer);

static ABI void acap(size_t neededCapacity);
static ABI INLINE uint32_t rng_next(void);

static ABI PN* pn_create(PS Source, size_t startOffset, size_t len, size_t lineFeeds, PN* leftChild, PN* rightChild, uint32_t Priority);
static ABI INLINE PN* pn_retain(PN* Node);
static ABI void pn_release(PN* Node);
static ABI PN* pn_merge(PN* leftNode, PN* rightNode);
static ABI SP pn_split(PN* Node, size_t splitKey);

static ABI size_t lown(size_t targetValue);
static ABI size_t cntnl(PS Source, size_t startOffset, size_t len);
static ABI size_t findnl(PS Source, size_t startOffset, size_t len, size_t targetNewlineIndex);

static ABI PN* pt_locate(size_t offset, size_t* pieceAbsoluteStart, size_t* offsetInPiece);
static ABI char8_t pt_character_at(size_t offset);
static ABI size_t pt_extract(size_t offset, size_t len, char8_t* destinationBuffer, size_t destinationCapacity);
static ABI bool pt_offset_of_newline(size_t newlineIndex, size_t* outOffset);
static ABI size_t pt_line_offset(size_t lineNumber);
static ABI void pt_insertb(size_t offset, const char8_t* dataBytes, size_t len);
static ABI void pt_delete_range(size_t offset, size_t len);
static ABI void pt_build_index(void);

static ABI void ucap(size_t neededCapacity);
static ABI void rcap(size_t neededCapacity);
static ABI void pushu(PN* rootNode);
static ABI INLINE PN* popu(void);
static ABI void pushr(PN* rootNode);
static ABI INLINE PN* popr(void);
static ABI void clrr(void);
static ABI void undo_begin_edit(void);
static ABI void fixcur(void);
static ABI void undo(void);
static ABI void redo(void);

static ABI size_t linelen(size_t lineNumber);
static ABI size_t linelay(size_t lineNumber, size_t stopByteOffset, size_t* stopRowPointer, size_t* stopColumnPointer);
static ABI INLINE size_t utf8clen(char8_t c);
static ABI void clamp_column(void);
static ABI void oflush(void);
static ABI void obytes(const char8_t* stringBytes, size_t count);
static ABI void ostr(const char8_t* string);
static ABI INLINE void obyte(char8_t byteValue);
static ABI void seqout(const char8_t* sequence);
static ABI void setcur(unsigned short row, unsigned short column);
static ABI void status(const char8_t* message);
static ABI void save(void);
static ABI void render(void);
static ABI INLINE size_t offsetcur(void);
static ABI void insbuf(size_t offset, char8_t c);
static ABI void delbuf(size_t offset);
static ABI void delete_char_at_cursor(void);
static ABI void adjscr(void);
static ABI void movcur(int direction);

ABI int main(int argc, char* argv[]) {
    int inputByte, fd;

    ssize_t readBytesCount;
    off_t fileSize;

    size_t pathLength, clearIndex, totalBytesRead, startOffset, endOffset, len, characterLength, offset, commandClearIndex, bytesToDelete, deleteIndex, joinColumn, oldColumn, nameLength;

    tcflag_t terminalMask;

    bool isNavigationKey, isMergingUp;
    char8_t rawInputByte, nextCharacter, keyCode, tildeCharacter, drainByte;
    uint8_t statBuffer[STAT_BUFFER_SIZE];

    winsize windowSize;
    termios settings;

    const char8_t* filePath;

    off_t* sizePointer;
    char8_t* commandString;

    G->ScreenRows = 24;
    G->ScreenColumns = 80;
    G->CurrentMode = EDITOR_MODE_NORMAL;
    G->Running = true;
    G->RNGState = 0x9E3779B9u;

    if (ioctl(1, TIOCGWINSZ, &windowSize) == 0) {
        if (windowSize.ws_row > 0) G->ScreenRows = windowSize.ws_row;
        if (windowSize.ws_col > 0) G->ScreenColumns = windowSize.ws_col;
    }

    if (argc < 2) G->Root = 0;

    else {
        filePath = argv[1];
        pathLength = strlen((const char*) filePath);

        if (pathLength >= sizeof(G->Filename)) pathLength = sizeof(G->Filename) - 1;

        for (clearIndex = 0; clearIndex < sizeof(G->Filename); clearIndex++) G->Filename[clearIndex] = 0;

        memcpy(G->Filename, filePath, pathLength);

        fd = open(argv[1], O_RDONLY, 0);
        if (fd < 0) G->Root = 0;

        else {
            if (fstat(fd, (stat*) statBuffer) != 0) fileSize = -1;

            else {
                sizePointer = (off_t*) (statBuffer + STAT_SIZE_OFFSET);
                fileSize = *sizePointer;
            }

            if (fileSize > 0) {
                totalBytesRead = 0;

                G->Buffer = (char8_t*) malloc((size_t) fileSize);
                while (totalBytesRead < (size_t) fileSize) {
                    readBytesCount = read(fd, G->Buffer + totalBytesRead, (size_t) fileSize - totalBytesRead);
                    if (readBytesCount <= 0) break;

                    totalBytesRead += (size_t) readBytesCount;
                }

                G->Length = totalBytesRead;
                pt_build_index();
                G->Root = (G->Length > 0) ? pn_create(PIECE_SOURCE_ORIGINAL, 0, G->Length, G->NewlineCount, 0, 0, rng_next()) : 0;
            } else G->Root = 0;

            close(fd);

            G->Dirty = false;
        }
    }

    tcgetattr(0, &G->Termios);
    settings = G->Termios;
    terminalMask = TERMINAL_FLAG_CANONICAL_MODE | TERMINAL_FLAG_ECHO | TERMINAL_FLAG_SIGNAL;
    settings.c_lflag &= ~terminalMask;
    settings.c_cc[TERMINAL_INDEX_VMIN] = 1;
    settings.c_cc[TERMINAL_INDEX_VTIME] = 0;
    tcsetattr(0, TCSANOW, &settings);

    seqout("2J");
    render();

    while (G->Running) {
        if (G->Pushback) {
            inputByte = G->Character;
            G->Pushback = false;
        } else {
            readBytesCount = read(0, &rawInputByte, 1);
            if (readBytesCount <= 0) break;
            inputByte = rawInputByte;
        }

        if (inputByte == 0x1b && ready(20)) {
            isNavigationKey = false;

            readBytesCount = read(0, &nextCharacter, 1);
            if (readBytesCount > 0 && nextCharacter == '[' && ready(20)) {
                readBytesCount = read(0, &keyCode, 1);
                if (readBytesCount > 0) {
                    switch (keyCode) {
                        case 'A': inputByte = 'k'; isNavigationKey = true; break;
                        case 'B': inputByte = 'j'; isNavigationKey = true; break;
                        case 'C': inputByte = 'l'; isNavigationKey = true; break;
                        case 'D': inputByte = 'h'; isNavigationKey = true; break;
                        case 'H': inputByte = '0'; isNavigationKey = true; break;
                        case 'F': inputByte = '$'; isNavigationKey = true; break;

                        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': {
                            if (ready(20)) {
                                readBytesCount = read(0, &tildeCharacter, 1);
                                if (readBytesCount > 0) {
                                    if (tildeCharacter == '~') {
                                        switch (keyCode) {
                                            case '1': case '7': inputByte = '0'; isNavigationKey = true; break;
                                            case '4': case '8': inputByte = '$'; isNavigationKey = true; break;
                                            case '2': inputByte = KEY_INSERT; isNavigationKey = true; break;
                                            case '3': inputByte = 'x'; isNavigationKey = true; break;
                                            case '5': inputByte = 0x02; isNavigationKey = true; break;
                                            case '6': inputByte = 0x06; isNavigationKey = true; break;

                                            default: break;
                                        }
                                    } else {
                                        drainByte = tildeCharacter;
                                        while ((drainByte < 0x40 || drainByte > 0x7E) && ready(20)) if (read(0, &drainByte, 1) <= 0) break;
                                    }
                                }
                            } break;
                        }

                        default: break;
                    }
                } else {
                    G->Character = nextCharacter;
                    G->Pushback = true;

                    inputByte = 0x1b;
                }
            }

            if (isNavigationKey) {
                if (inputByte == KEY_INSERT) {
                    if (G->CurrentMode == EDITOR_MODE_NORMAL || G->CurrentMode == EDITOR_MODE_INSERT) {
                        if (G->CurrentMode == EDITOR_MODE_INSERT) {
                            if (G->CursorColumn > 0) {
                                startOffset = pt_line_offset(G->CursorRow);
                                G->CursorColumn--;
                                while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
                            }

                            G->CurrentMode = EDITOR_MODE_NORMAL;
                            G->Undo = false;
                        } else G->CurrentMode = EDITOR_MODE_INSERT;

                        clamp_column();
                        adjscr();
                    }
                } else {
                    switch (G->CurrentMode) {
                        case EDITOR_MODE_NORMAL: case EDITOR_MODE_INSERT: {
                            if (inputByte == 'x') delete_char_at_cursor();
                            else movcur(inputByte);
                            clamp_column();
                            adjscr();

                            break;
                        }

                        default: break;
                    }
                }

                render();

                continue;
            }

            if (inputByte == 0x1b) {
                switch (G->CurrentMode) {
                    case EDITOR_MODE_INSERT: {
                        if (G->CursorColumn > 0) {
                            startOffset = pt_line_offset(G->CursorRow);
                            G->CursorColumn--;
                            while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
                        }

                        G->CurrentMode = EDITOR_MODE_NORMAL;
                        G->Undo = false;

                        switch (G->CurrentMode) {
                            case EDITOR_MODE_NORMAL: clamp_column(); break;

                            default: break;
                        }

                        adjscr();

                        break;
                    }

                    case EDITOR_MODE_COMMAND: {
                        G->CurrentMode = EDITOR_MODE_NORMAL;
                        G->CommandLineLength = 0;

                        break;
                    }

                    default: break;
                }
            }

            render();

            continue;
        }

        switch (G->CurrentMode) {
            case EDITOR_MODE_NORMAL: {
                G->StatusLength = 0;

                switch (G->Lead) {
                    case 'd': {
                        G->Lead = 0;
                        if (inputByte != 'd') break;

                        if (G->CursorRow < ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) {
                            startOffset = pt_line_offset(G->CursorRow);
                            endOffset = (G->CursorRow + 1 < ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) ? pt_line_offset(G->CursorRow + 1) : (G->Root ? G->Root->SubtreeLength : 0);
                            len = endOffset - startOffset;

                            if (len > 0) {
                                undo_begin_edit();
                                pt_delete_range(startOffset, len);
                                G->Undo = false;

                                G->Dirty = true;
                            }
                        }

                        if (G->CursorRow >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) && ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0) G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;

                        break;
                    }

                    case 'g': {
                        G->Lead = 0;
                        if (inputByte == 'g') {
                            G->CursorRow = 0;
                            G->CursorColumn = 0;
                            G->TopLineIndex = 0;
                        } break;
                    }

                    default: {
                        switch (inputByte) {
                            case 'h': movcur('h'); break;
                            case 'l': movcur('l'); break;
                            case 'j': movcur('j'); break;
                            case 'k': movcur('k'); break;
                            case '0': movcur('0'); break;
                            case '$': movcur('$'); break;
                            case 'G': if (((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0) G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1; break;
                            case 'g': G->Lead = 'g'; break;
                            case 'i': G->CurrentMode = EDITOR_MODE_INSERT; break;
                            case 'a': {
                                len = linelen(G->CursorRow);
                                G->CurrentMode = EDITOR_MODE_INSERT;
                                if (len > 0) {
                                    startOffset = pt_line_offset(G->CursorRow);
                                    characterLength = utf8clen(pt_character_at(startOffset + G->CursorColumn));
                                    G->CursorColumn += characterLength;
                                } break;
                            }

                            case 'A': G->CurrentMode = EDITOR_MODE_INSERT; G->CursorColumn = linelen(G->CursorRow); break;
                            case 'I': G->CursorColumn = 0; G->CurrentMode = EDITOR_MODE_INSERT; break;

                            case 'o': {
                                offset = pt_line_offset(G->CursorRow) + linelen(G->CursorRow);
                                if (offset < (G->Root ? G->Root->SubtreeLength : 0) && pt_character_at(offset) == '\n') offset++;
                                insbuf(offset, '\n');
                                G->CursorRow++;
                                G->CursorColumn = 0;
                                G->CurrentMode = EDITOR_MODE_INSERT;

                                break;
                            }

                            case 'O': {
                                insbuf(pt_line_offset(G->CursorRow), '\n');
                                G->CursorColumn = 0;
                                G->CurrentMode = EDITOR_MODE_INSERT;

                                break;
                            }

                            case 'x': {
                                delete_char_at_cursor();

                                break;
                            }

                            case 'd': G->Lead = 'd'; break;

                            case 'u': undo(); break;
                            case 0x12: redo(); break;

                            case ':': {
                                G->CurrentMode = EDITOR_MODE_COMMAND;
                                G->CommandLineLength = 0;

                                for (commandClearIndex = 0; commandClearIndex < sizeof(G->CommandLineBuffer); commandClearIndex++) G->CommandLineBuffer[commandClearIndex] = 0;

                                break;
                            }

                            case 0x06: movcur(0x06); break;
                            case 0x02: movcur(0x02); break;

                            default: break;
                        } break;
                    }
                }

                switch (G->CurrentMode) {
                    case EDITOR_MODE_NORMAL: clamp_column(); break;

                    default: break;
                }

                adjscr();

                break;
            }

            case EDITOR_MODE_INSERT: {
                switch (inputByte) {
                    case 0x1b: {
                        if (G->CursorColumn > 0) {
                            startOffset = pt_line_offset(G->CursorRow);
                            G->CursorColumn--;
                            while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
                        }

                        G->CurrentMode = EDITOR_MODE_NORMAL;
                        G->Undo = false;

                        break;
                    }

                    case 127: case 8: {
                        if (offsetcur() > 0) {
                            isMergingUp = (G->CursorColumn == 0 && G->CursorRow > 0);
                            joinColumn = isMergingUp ? linelen(G->CursorRow - 1) : 0;

                            if (isMergingUp) {
                                delbuf(offsetcur() - 1);
                                G->CursorRow--;
                                G->CursorColumn = joinColumn;
                            } else if (G->CursorColumn > 0) {
                                startOffset = pt_line_offset(G->CursorRow);
                                oldColumn = G->CursorColumn;
                                G->CursorColumn--;
                                while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;

                                bytesToDelete = oldColumn - G->CursorColumn;
                                for (deleteIndex = 0; deleteIndex < bytesToDelete; deleteIndex++) delbuf(startOffset + G->CursorColumn);
                            }
                        } break;
                    }

                    case '\n': case '\r': {
                        insbuf(offsetcur(), '\n');

                        G->CursorRow++;
                        G->CursorColumn = 0;

                        break;
                    }

                    case '\t': {
                        insbuf(offsetcur(), '\t');
                        G->CursorColumn++;

                        break;
                    }

                    default: {
                        if (inputByte >= 32 && inputByte < 0x100) {
                            insbuf(offsetcur(), (char8_t) inputByte);
                            G->CursorColumn++;
                        } break;
                    }
                }

                switch (G->CurrentMode) {
                    case EDITOR_MODE_NORMAL: clamp_column(); break;

                    default: break;
                }

                adjscr();

                break;
            }

            case EDITOR_MODE_COMMAND: {
                switch (inputByte) {
                    case 0x1b: {
                        G->CurrentMode = EDITOR_MODE_NORMAL;
                        G->CommandLineLength = 0;

                        break;
                    }

                    case '\n': case '\r': {
                        G->CommandLineBuffer[G->CommandLineLength] = '\0';
                        commandString = G->CommandLineBuffer;

                        switch (commandString[0]) {
                            case 'q': {
                                switch (commandString[1]) {
                                    case '\0': {
                                        if (G->Dirty && !G->Force) status("Unsaved changes, :q! to force.");

                                        else G->Running = false; break;
                                    }

                                    case '!': {
                                        switch (commandString[2]) {
                                            case '\0': {
                                                G->Running = false;

                                                break;
                                            }

                                            default: status("Unknown command."); break;
                                        } break;
                                    }

                                    default: status("Unknown command."); break;
                                } break;
                            }

                            case 'w': {
                                switch (commandString[1]) {
                                    case '\0': case '!': {
                                        switch (commandString[commandString[1] == '!' ? 2 : 1]) {
                                            case '\0': save(); break;

                                            default: status("Unknown command."); break;
                                        } break;
                                    }

                                    case 'q': {
                                        switch (commandString[2]) {
                                            case '\0': {
                                                save();

                                                G->Running = false; break;
                                            }

                                            case '!': {
                                                switch (commandString[3]) {
                                                    case '\0': {
                                                        save();

                                                        G->Running = false; break;
                                                    }

                                                    default: status("Unknown command."); break;
                                                } break;
                                            }

                                            case ' ': {
                                                switch (commandString[3]) {
                                                    case '\0': status("Unknown command."); break;

                                                    default: {
                                                        nameLength = strlen((const char*) (commandString + 3));

                                                        if (nameLength >= sizeof(G->Filename)) nameLength = sizeof(G->Filename) - 1;
                                                        memcpy(G->Filename, commandString + 3, nameLength);
                                                        G->Filename[nameLength] = '\0';

                                                        save();
                                                        G->Running = false;

                                                        break;
                                                    }
                                                } break;
                                            }

                                            default: status("Unknown command."); break;
                                        } break;
                                    }

                                    case ' ': {
                                        switch (commandString[2]) {
                                            case '\0': status("Unknown command."); break;

                                            default: {
                                                nameLength = strlen((const char*) (commandString + 2));

                                                if (nameLength >= sizeof(G->Filename)) nameLength = sizeof(G->Filename) - 1;
                                                memcpy(G->Filename, commandString + 2, nameLength);
                                                G->Filename[nameLength] = '\0';

                                                save(); break;
                                            }
                                        } break;
                                    }

                                    default: status("Unknown command."); break;
                                } break;
                            }

                            case 'x': {
                                switch (commandString[1]) {
                                    case '\0': {
                                        save();

                                        G->Running = false; break;
                                    }

                                    case '!': {
                                        switch (commandString[2]) {
                                            case '\0': {
                                                save();

                                                G->Running = false; break;
                                            }

                                            default: status("Unknown command."); break;
                                        } break;
                                    }

                                    default: status("Unknown command."); break;
                                } break;
                            }

                            default: status("Unknown command."); break;
                        }

                        G->CurrentMode = EDITOR_MODE_NORMAL;
                        G->CommandLineLength = 0;

                        break;
                    }

                    case 127: case 8: {
                        if (G->CommandLineLength > 0) G->CommandLineLength--;

                        break;
                    }

                    default: {
                        if (inputByte >= 32 && inputByte < 127 && G->CommandLineLength + 1 < sizeof(G->CommandLineBuffer)) G->CommandLineBuffer[G->CommandLineLength++] = (char8_t) inputByte;

                        break;
                    }
                } break;
            }

            default: break;
        }

        render();
    }

    tcsetattr(0, TCSANOW, &G->Termios);
    seqout("?7h");
    seqout("2J");
    setcur(0, 0);
    oflush();

    return 0;
}

static ABI bool ready(int ms) {
    pollfd pollDescriptor;

    pollDescriptor.fd = 0;
    pollDescriptor.events = POLLIN;
    pollDescriptor.revents = 0;

    return poll(&pollDescriptor, 1, ms) > 0;
}

static ABI char8_t* ull2s(uint64_t value, char8_t* buffer) {
    char8_t* pointer;

    pointer = buffer + 20;

    *pointer = '\0';
    if (value == 0) {
        *--pointer = '0';

        return pointer;
    }

    while (value > 0) {
        *--pointer = (char8_t) ('0' + (value % 10));

        value /= 10;
    }

    return pointer;
}

static ABI void acap(size_t neededCapacity) {
    size_t newCapacity;

    char8_t* newBuffer;

    if (neededCapacity <= G->AddCapacity) return;

    newCapacity = G->AddCapacity ? G->AddCapacity : 4096;
    while (newCapacity < neededCapacity) newCapacity *= 2;

    newBuffer = (char8_t*) realloc(G->AddBuffer, newCapacity);
    if (newBuffer) {
        G->AddBuffer = newBuffer;
        G->AddCapacity = newCapacity;
    }
}

static ABI INLINE uint32_t rng_next(void) {
    uint32_t stateValue;

    stateValue = G->RNGState;
    stateValue ^= stateValue << 13;
    stateValue ^= stateValue >> 17;
    stateValue ^= stateValue << 5;
    G->RNGState = stateValue;

    return stateValue;
}

static ABI PN* pn_create(PS Source, size_t startOffset, size_t len, size_t lineFeeds, PN* leftChild, PN* rightChild, uint32_t Priority) {
    PN* Node;

    Node = (PN*) malloc(sizeof(PN));

    Node->Source = Source;
    Node->StartOffset = startOffset;
    Node->Length = len;
    Node->LineFeeds = lineFeeds;
    Node->LeftChild = leftChild;
    Node->RightChild = rightChild;
    Node->Priority = Priority;
    Node->ReferenceCount = 1;
    Node->pad = 0;

    Node->SubtreeLength = len + (leftChild ? leftChild->SubtreeLength : 0) + (rightChild ? rightChild->SubtreeLength : 0);
    Node->SubtreeLineFeeds = lineFeeds + (leftChild ? leftChild->SubtreeLineFeeds : 0) + (rightChild ? rightChild->SubtreeLineFeeds : 0);

    return Node;
}

static ABI INLINE PN* pn_retain(PN* Node) {
    if (Node) Node->ReferenceCount++;

    return Node;
}

static ABI void pn_release(PN* Node) {
    if (!Node) return;

    Node->ReferenceCount--;
    if (Node->ReferenceCount <= 0) {
        pn_release(Node->LeftChild);
        pn_release(Node->RightChild);

        free(Node);
    }
}

static ABI PN* pn_merge(PN* leftNode, PN* rightNode) {
    PN* newRightChild, *newLeftChild;

    if (!leftNode) return pn_retain(rightNode);
    if (!rightNode) return pn_retain(leftNode);

    if (leftNode->Priority > rightNode->Priority) {
        newRightChild = pn_merge(leftNode->RightChild, rightNode);

        return pn_create(leftNode->Source, leftNode->StartOffset, leftNode->Length, leftNode->LineFeeds, pn_retain(leftNode->LeftChild), newRightChild, leftNode->Priority);
    } else {
        newLeftChild = pn_merge(leftNode, rightNode->LeftChild);

        return pn_create(rightNode->Source, rightNode->StartOffset, rightNode->Length, rightNode->LineFeeds, newLeftChild, pn_retain(rightNode->RightChild), rightNode->Priority);
    }
}

static ABI SP pn_split(PN* Node, size_t splitKey) {
    SP splitResult, subtreeSplitResult;
    size_t leftSubtreeLength, offsetInPiece, leftLineFeeds, rightLineFeeds;

    PN* newRightNode, *newLeftNode;

    if (!Node) {
        splitResult.left_node = 0;
        splitResult.right_node = 0;

        return splitResult;
    }

    leftSubtreeLength = Node->LeftChild ? Node->LeftChild->SubtreeLength : 0;

    if (splitKey <= leftSubtreeLength) {
        subtreeSplitResult = pn_split(Node->LeftChild, splitKey);
        newRightNode = pn_create(Node->Source, Node->StartOffset, Node->Length, Node->LineFeeds, subtreeSplitResult.right_node, pn_retain(Node->RightChild), Node->Priority);

        splitResult.left_node = subtreeSplitResult.left_node;
        splitResult.right_node = newRightNode;

        return splitResult;
    } else if (splitKey >= leftSubtreeLength + Node->Length) {
        subtreeSplitResult = pn_split(Node->RightChild, splitKey - leftSubtreeLength - Node->Length);
        newLeftNode = pn_create(Node->Source, Node->StartOffset, Node->Length, Node->LineFeeds, pn_retain(Node->LeftChild), subtreeSplitResult.left_node, Node->Priority);

        splitResult.left_node = newLeftNode;
        splitResult.right_node = subtreeSplitResult.right_node;

        return splitResult;
    } else {
        offsetInPiece = splitKey - leftSubtreeLength;
        leftLineFeeds = cntnl(Node->Source, Node->StartOffset, offsetInPiece);
        rightLineFeeds = Node->LineFeeds - leftLineFeeds;

        splitResult.left_node = pn_create(Node->Source, Node->StartOffset, offsetInPiece, leftLineFeeds, pn_retain(Node->LeftChild), 0, rng_next());
        splitResult.right_node = pn_create(Node->Source, Node->StartOffset + offsetInPiece, Node->Length - offsetInPiece, rightLineFeeds, 0, pn_retain(Node->RightChild), rng_next());

        return splitResult;
    }
}

static ABI size_t lown(size_t targetValue) {
    size_t lowerBound, upperBound, middleIndex;

    lowerBound = 0;
    upperBound = G->NewlineCount;

    while (lowerBound < upperBound) {
        middleIndex = lowerBound + (upperBound - lowerBound) / 2;
        if (G->Newlines[middleIndex] < targetValue) lowerBound = middleIndex + 1;
        else upperBound = middleIndex;
    }

    return lowerBound;
}

static ABI size_t cntnl(PS Source, size_t startOffset, size_t len) {
    size_t startIndex, endIndex, iterationIndex, newlineCount;

    if (len == 0) return 0;

    if (Source == PIECE_SOURCE_ORIGINAL) {
        startIndex = lown(startOffset);
        endIndex = lown(startOffset + len);

        return endIndex - startIndex;
    } else {
        newlineCount = 0;
        for (iterationIndex = 0; iterationIndex < len; iterationIndex++) if (G->AddBuffer[startOffset + iterationIndex] == '\n') newlineCount++;

        return newlineCount;
    }
}

static ABI size_t findnl(PS Source, size_t startOffset, size_t len, size_t targetNewlineIndex) {
    size_t lowerBoundIndex, iterationIndex;

    if (Source == PIECE_SOURCE_ORIGINAL) {
        lowerBoundIndex = lown(startOffset);

        return G->Newlines[lowerBoundIndex + targetNewlineIndex] - startOffset;
    } else {
        for (iterationIndex = 0; iterationIndex < len; iterationIndex++) if (G->AddBuffer[startOffset + iterationIndex] == '\n') {
            if (targetNewlineIndex == 0) return iterationIndex;

            targetNewlineIndex--;
        }

        return len;
    }
}

static ABI PN* pt_locate(size_t offset, size_t* pieceAbsoluteStart, size_t* offsetInPiece) {
    size_t baseOffset, leftSubtreeLength;
    PN* currentNode;

    currentNode = G->Root;
    baseOffset = 0;

    while (currentNode) {
        leftSubtreeLength = currentNode->LeftChild ? currentNode->LeftChild->SubtreeLength : 0;

        if (offset < leftSubtreeLength) {
            currentNode = currentNode->LeftChild;

            continue;
        }

        offset -= leftSubtreeLength;
        baseOffset += leftSubtreeLength;

        if (offset < currentNode->Length) {
            *pieceAbsoluteStart = baseOffset;
            *offsetInPiece = offset;

            return currentNode;
        }

        offset -= currentNode->Length;
        baseOffset += currentNode->Length;
        currentNode = currentNode->RightChild;
    }

    return 0;
}

static ABI char8_t pt_character_at(size_t offset) {
    size_t pieceAbsoluteStart, offsetInPiece;
    PN* Node;
    const char8_t* sourceBuffer;

    Node = pt_locate(offset, &pieceAbsoluteStart, &offsetInPiece);

    if (!Node) return 0;

    sourceBuffer = (Node->Source == PIECE_SOURCE_ORIGINAL) ? G->Buffer : G->AddBuffer;
    return sourceBuffer[Node->StartOffset + offsetInPiece];
}

static ABI size_t pt_extract(size_t offset, size_t len, char8_t* destinationBuffer, size_t destinationCapacity) {
    size_t bytesWritten, totalLength, pieceAbsoluteStart, offsetInPiece, availableBytes, requestedBytes, destinationRoom;
    PN* Node;
    const char8_t* sourceBuffer;

    bytesWritten = 0;
    totalLength = (G->Root ? G->Root->SubtreeLength : 0);

    if (offset > totalLength) return 0;
    if (offset + len > totalLength) len = totalLength - offset;

    while (len > 0 && bytesWritten < destinationCapacity) {
        Node = pt_locate(offset, &pieceAbsoluteStart, &offsetInPiece);

        if (!Node) break;

        availableBytes = Node->Length - offsetInPiece;
        requestedBytes = (len < availableBytes) ? len : availableBytes;
        destinationRoom = destinationCapacity - bytesWritten;
        if (requestedBytes > destinationRoom) requestedBytes = destinationRoom;
        if (requestedBytes == 0) break;

        sourceBuffer = (Node->Source == PIECE_SOURCE_ORIGINAL) ? G->Buffer : G->AddBuffer;
        memcpy(destinationBuffer + bytesWritten, sourceBuffer + Node->StartOffset + offsetInPiece, requestedBytes);

        bytesWritten += requestedBytes;
        offset += requestedBytes;
        len -= requestedBytes;
    }

    return bytesWritten;
}

static ABI bool pt_offset_of_newline(size_t newlineIndex, size_t* outOffset) {
    size_t baseOffset, leftSubtreeLineFeeds, leftSubtreeLength;
    PN* Node;

    Node = G->Root;
    baseOffset = 0;

    while (Node) {
        leftSubtreeLineFeeds = Node->LeftChild ? Node->LeftChild->SubtreeLineFeeds : 0;
        leftSubtreeLength = Node->LeftChild ? Node->LeftChild->SubtreeLength : 0;

        if (newlineIndex < leftSubtreeLineFeeds) {
            Node = Node->LeftChild;

            continue;
        }

        newlineIndex -= leftSubtreeLineFeeds;
        baseOffset += leftSubtreeLength;

        if (newlineIndex < Node->LineFeeds) {
            *outOffset = baseOffset + findnl(Node->Source, Node->StartOffset, Node->Length, newlineIndex);

            return true;
        }

        newlineIndex -= Node->LineFeeds;
        baseOffset += Node->Length;

        Node = Node->RightChild;
    }

    return false;
}

static ABI size_t pt_line_offset(size_t lineNumber) {
    size_t characterOffset;

    if (lineNumber == 0) return 0;
    if (pt_offset_of_newline(lineNumber - 1, &characterOffset)) return characterOffset + 1;

    return (G->Root ? G->Root->SubtreeLength : 0);
}

static ABI void pt_insertb(size_t offset, const char8_t* dataBytes, size_t len) {
    size_t pieceStartOffset, lineFeedCount;
    SP splitResult;
    PN* newPieceNode, *mergedNode1, *newRootNode;

    if (len == 0) return;

    acap(G->AddLength + len);
    memcpy(G->AddBuffer + G->AddLength, dataBytes, len);

    pieceStartOffset = G->AddLength;
    G->AddLength += len;

    lineFeedCount = cntnl(PIECE_SOURCE_ADD, pieceStartOffset, len);
    newPieceNode = pn_create(PIECE_SOURCE_ADD, pieceStartOffset, len, lineFeedCount, 0, 0, rng_next());

    splitResult = pn_split(G->Root, offset);
    mergedNode1 = pn_merge(splitResult.left_node, newPieceNode);
    newRootNode = pn_merge(mergedNode1, splitResult.right_node);

    pn_release(splitResult.left_node);
    pn_release(splitResult.right_node);
    pn_release(newPieceNode);
    pn_release(mergedNode1);

    pn_release(G->Root);

    G->Root = newRootNode;
}

static ABI void pt_delete_range(size_t offset, size_t len) {
    SP splitResult1, splitResult2;
    PN* newRootNode;

    if (len == 0) return;

    splitResult1 = pn_split(G->Root, offset);
    splitResult2 = pn_split(splitResult1.right_node, len);
    newRootNode = pn_merge(splitResult1.left_node, splitResult2.right_node);

    pn_release(splitResult1.left_node);
    pn_release(splitResult1.right_node);
    pn_release(splitResult2.left_node);
    pn_release(splitResult2.right_node);

    pn_release(G->Root);

    G->Root = newRootNode;
}

static ABI void pt_build_index(void) {
    size_t iterationIndex, count;

    count = 0;
    for (iterationIndex = 0; iterationIndex < G->Length; iterationIndex++) if (G->Buffer[iterationIndex] == '\n') count++;

    G->Newlines = count ? (size_t*) malloc(count * sizeof(size_t)) : 0;
    G->NewlineCount = 0;

    for (iterationIndex = 0; iterationIndex < G->Length; iterationIndex++) if (G->Buffer[iterationIndex] == '\n') G->Newlines[G->NewlineCount++] = iterationIndex;
}

static ABI void ucap(size_t neededCapacity) {
    size_t newCapacity;
    PN** newStack;

    if (neededCapacity <= G->UndoCapacity) return;

    newCapacity = G->UndoCapacity ? G->UndoCapacity : 64;
    while (newCapacity < neededCapacity) newCapacity *= 2;

    newStack = (PN**) realloc(G->UndoStack, newCapacity * sizeof(PN*));
    if (newStack) {
        G->UndoStack = newStack;
        G->UndoCapacity = newCapacity;
    }
}

static ABI void rcap(size_t neededCapacity) {
    size_t newCapacity;
    PN** newStack;

    if (neededCapacity <= G->RedoCapacity) return;

    newCapacity = G->RedoCapacity ? G->RedoCapacity : 64;
    while (newCapacity < neededCapacity) newCapacity *= 2;

    newStack = (PN**) realloc(G->RedoStack, newCapacity * sizeof(PN*));
    if (newStack) {
        G->RedoStack = newStack;
        G->RedoCapacity = newCapacity;
    }
}

static ABI void pushu(PN* rootNode) {
    size_t iterationIndex;

    if (G->UndoCount >= UNDO_HISTORY_LIMIT) {
        pn_release(G->UndoStack[0]);
        for (iterationIndex = 1; iterationIndex < G->UndoCount; iterationIndex++) G->UndoStack[iterationIndex - 1] = G->UndoStack[iterationIndex];

        G->UndoCount--;
    }

    ucap(G->UndoCount + 1);
    G->UndoStack[G->UndoCount++] = pn_retain(rootNode);
}

static ABI INLINE PN* popu(void) {
    if (G->UndoCount == 0) return 0;

    return G->UndoStack[--G->UndoCount];
}

static ABI void pushr(PN* rootNode) {
    rcap(G->RedoCount + 1);

    G->RedoStack[G->RedoCount++] = pn_retain(rootNode);
}

static ABI INLINE PN* popr(void) {
    if (G->RedoCount == 0) return 0;

    return G->RedoStack[--G->RedoCount];
}

static ABI void clrr(void) {
    size_t iterationIndex;

    for (iterationIndex = 0; iterationIndex < G->RedoCount; iterationIndex++) pn_release(G->RedoStack[iterationIndex]);

    G->RedoCount = 0;
}

static ABI void undo_begin_edit(void) {
    if (!G->Undo) {
        pushu(G->Root);
        clrr();

        G->Undo = true;
    }
}

static ABI void fixcur(void) {
    size_t lineCount;

    lineCount = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

    if (G->CursorRow >= lineCount) G->CursorRow = lineCount - 1;

    clamp_column();
    adjscr();
}

static ABI void undo(void) {
    PN* previousRootNode;

    if (G->UndoCount == 0) {
        status("Already at oldest change.");

        return;
    }

    pushr(G->Root);

    previousRootNode = popu();
    pn_release(G->Root);

    G->Root = previousRootNode;
    G->Undo = false;

    fixcur();
    status("Undo.");
}

static ABI void redo(void) {
    PN* nextRootNode;

    if (G->RedoCount == 0) {
        status("Already at newest change.");

        return;
    }

    pushu(G->Root);

    nextRootNode = popr();
    pn_release(G->Root);

    G->Root = nextRootNode;
    G->Undo = false;

    fixcur();
    status("Redo.");
}

static ABI size_t linelen(size_t lineNumber) {
    size_t startOffset, endOffset, totalLength, lineCount;

    lineCount = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

    if (lineNumber >= lineCount) return 0;

    startOffset = pt_line_offset(lineNumber);
    totalLength = (G->Root ? G->Root->SubtreeLength : 0);
    endOffset = (lineNumber + 1 < lineCount) ? pt_line_offset(lineNumber + 1) - 1 : totalLength;

    return (endOffset > startOffset) ? (endOffset - startOffset) : 0;
}

static ABI size_t linelay(size_t lineNumber, size_t stopByteOffset, size_t* stopRowPointer, size_t* stopColumnPointer) {
    size_t startOffset, lineBytesLength, terminalColumns, byteOffset, rowIndex, visualColumn, scratchLength, tabWidth;
    bool hasStopped;
    char8_t c;

    if (lineNumber >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) {
        if (stopRowPointer) *stopRowPointer = 0;
        if (stopColumnPointer) *stopColumnPointer = 0;

        return 1;
    }

    startOffset = pt_line_offset(lineNumber);
    lineBytesLength = linelen(lineNumber);
    terminalColumns = G->ScreenColumns ? G->ScreenColumns : 1;

    scratchLength = pt_extract(startOffset, lineBytesLength, G->ScratchLineBuffer, sizeof(G->ScratchLineBuffer));
    if (lineBytesLength > scratchLength) lineBytesLength = scratchLength;

    byteOffset = 0;
    rowIndex = 0;
    visualColumn = 0;
    hasStopped = false;

    while (byteOffset < lineBytesLength) {
        c = G->ScratchLineBuffer[byteOffset];

        if (c == '\t') {
            tabWidth = TABULATION_WIDTH - (visualColumn % TABULATION_WIDTH);

            if (visualColumn + tabWidth > terminalColumns && visualColumn > 0) {
                rowIndex++;
                visualColumn = 0;

                continue;
            }

            if (!hasStopped && byteOffset >= stopByteOffset) {
                if (stopRowPointer) *stopRowPointer = rowIndex;
                if (stopColumnPointer) *stopColumnPointer = visualColumn;

                hasStopped = true;
            }

            visualColumn += tabWidth;
            byteOffset++;

        } else if ((c & 0xC0) == 0x80) byteOffset++;

        else {
            if (visualColumn + 1 > terminalColumns && visualColumn > 0) {
                rowIndex++;
                visualColumn = 0;

                continue;
            }

            if (!hasStopped && byteOffset >= stopByteOffset) {
                if (stopRowPointer) *stopRowPointer = rowIndex;
                if (stopColumnPointer) *stopColumnPointer = visualColumn;

                hasStopped = true;
            }

            visualColumn++;
            byteOffset++;

            while (byteOffset < lineBytesLength && (G->ScratchLineBuffer[byteOffset] & 0xC0) == 0x80) byteOffset++;
        }
    }

    if (!hasStopped) {
        if (stopRowPointer) *stopRowPointer = rowIndex;
        if (stopColumnPointer) *stopColumnPointer = visualColumn;
    }

    return rowIndex + 1;
}

static ABI INLINE size_t utf8clen(char8_t c) {
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;

    return 1;
}

static ABI void clamp_column(void) {
    size_t len, startOffset;

    len = linelen(G->CursorRow);
    startOffset = pt_line_offset(G->CursorRow);

    if (len == 0) {
        G->CursorColumn = 0;

        return;
    }

    if (G->CurrentMode == EDITOR_MODE_INSERT) if (G->CursorColumn >= len) G->CursorColumn = len;

    while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
}

static ABI void oflush(void) {
    if (!G->OutputLength) return;

    write(1, G->OutputBuffer, G->OutputLength);

    G->OutputLength = 0;
}

static ABI void obytes(const char8_t* stringBytes, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (G->OutputLength == OUTPUT_BUFFER_MAXIMUM) oflush();

        G->OutputBuffer[G->OutputLength++] = stringBytes[i];
    }
}

static ABI void ostr(const char8_t* string) {
    size_t stringLength;

    stringLength = strlen((const char*) string);

    obytes(string, stringLength);
}

static ABI INLINE void obyte(char8_t byteValue) {
    if (G->OutputLength == OUTPUT_BUFFER_MAXIMUM) oflush();

    G->OutputBuffer[G->OutputLength++] = byteValue;
}

static ABI void seqout(const char8_t* sequence) {
    obyte(0x1b);
    obyte('[');
    ostr(sequence);
}

static ABI void setcur(unsigned short row, unsigned short column) {
    char8_t rowBuffer[22], columnBuffer[22];
    char8_t* rowString, *columnString;

    rowString = ull2s(row + 1, rowBuffer);
    columnString = ull2s(column + 1, columnBuffer);

    obyte(0x1b);
    obyte('[');
    ostr(rowString);
    obyte(';');
    ostr(columnString);
    obyte('H');
}

static ABI void status(const char8_t* message) {
    size_t messageLength;

    messageLength = strlen((const char*) message);

    if (messageLength >= sizeof(G->StatusBuffer)) messageLength = sizeof(G->StatusBuffer) - 1;

    memcpy(G->StatusBuffer, message, messageLength);

    G->StatusLength = messageLength;
}

static ABI void save(void) {
    int fd;
    ssize_t readWriteResult;
    size_t bytesWrittenTotal, currentPosition, totalLength, filenameLength, numberStringLength, suffixLength, requestedBytes, extractedBytes, chunkWrittenBytes;
    char8_t numberBuffer[22];

    char8_t* numberString;
    const char8_t* writtenSuffixString, *bytesUnitString;

    bytesWrittenTotal = 0;
    currentPosition = 0;
    filenameLength = 0;
    suffixLength = 0;

    writtenSuffixString = " written ";
    bytesUnitString = "B";

    if (!G->Filename[0]) {
        status("No filename.");

        return;
    }

    fd = open(G->Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        status("Failed to write file.");

        return;
    }

    totalLength = (G->Root ? G->Root->SubtreeLength : 0);

    while (currentPosition < totalLength) {
        requestedBytes = totalLength - currentPosition;
        chunkWrittenBytes = 0;

        if (requestedBytes > sizeof(G->ScratchSaveBuffer)) requestedBytes = sizeof(G->ScratchSaveBuffer);

        extractedBytes = pt_extract(currentPosition, requestedBytes, G->ScratchSaveBuffer, sizeof(G->ScratchSaveBuffer));
        if (extractedBytes == 0) break;

        while (chunkWrittenBytes < extractedBytes) {
            readWriteResult = write(fd, G->ScratchSaveBuffer + chunkWrittenBytes, extractedBytes - chunkWrittenBytes);
            if (readWriteResult <= 0) break;

            chunkWrittenBytes += (size_t) readWriteResult;
        }

        bytesWrittenTotal += chunkWrittenBytes;
        currentPosition += extractedBytes;

        if (chunkWrittenBytes < extractedBytes) break;
    }

    close(fd);
    G->Dirty = false;

    numberString = ull2s(bytesWrittenTotal, numberBuffer);
    numberStringLength = strlen((const char*) numberString);
    suffixLength = 9 + numberStringLength + 1;

    filenameLength = strlen((const char*) G->Filename);
    if (filenameLength + suffixLength > sizeof(G->StatusBuffer)) {
        if (sizeof(G->StatusBuffer) > suffixLength) filenameLength = sizeof(G->StatusBuffer) - suffixLength;
        else filenameLength = 0;
    }

    G->StatusLength = 0;

    memcpy(G->StatusBuffer + G->StatusLength, G->Filename, filenameLength);
    G->StatusLength += filenameLength;

    memcpy(G->StatusBuffer + G->StatusLength, writtenSuffixString, 9);
    G->StatusLength += 9;

    memcpy(G->StatusBuffer + G->StatusLength, numberString, numberStringLength);
    G->StatusLength += numberStringLength;

    memcpy(G->StatusBuffer + G->StatusLength, bytesUnitString, 1);
    G->StatusLength += 1;
}

static ABI void render(void) {
    unsigned short screenRows, currentScreenRow;
    size_t terminalColumns, currentLine, lineCount, lineBytesLength, startOffset, rowsForLine, subRowIndex, byteOffset, scratchLength, visualColumn, tabWidth, iterationIndex, rowStringLength, columnStringLength, infoLength, visualRow, lineIndex, rowInLine, columnInLine;
    char8_t c;
    char8_t rowBuffer[22], columnBuffer[22];

    char8_t* rowString, *columnString;

    screenRows = G->ScreenRows - 1;
    currentScreenRow = 0;
    terminalColumns = G->ScreenColumns ? G->ScreenColumns : 1;
    currentLine = G->TopLineIndex;
    lineCount = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

    seqout("?25l");
    seqout("?7l");

    while (currentScreenRow < screenRows && currentLine < lineCount) {
        lineBytesLength = linelen(currentLine);
        startOffset = pt_line_offset(currentLine);
        rowsForLine = linelay(currentLine, (size_t) -1, 0, 0);
        byteOffset = 0;

        scratchLength = pt_extract(startOffset, lineBytesLength, G->ScratchLineBuffer, sizeof(G->ScratchLineBuffer));
        if (lineBytesLength > scratchLength) lineBytesLength = scratchLength;

        for (subRowIndex = 0; subRowIndex < rowsForLine && currentScreenRow < screenRows; subRowIndex++) {
            visualColumn = 0;

            setcur(currentScreenRow, 0);
            seqout("2K");

            while (byteOffset < lineBytesLength) {
                c = G->ScratchLineBuffer[byteOffset];

                if (c == '\t') {
                    tabWidth = TABULATION_WIDTH - (visualColumn % TABULATION_WIDTH);

                    if (visualColumn + tabWidth > terminalColumns && visualColumn > 0) break;

                    for (iterationIndex = 0; iterationIndex < tabWidth; iterationIndex++) obyte(' ');

                    visualColumn += tabWidth;
                    byteOffset++;
                } else if ((c & 0xC0) == 0x80) {
                    obyte(c);

                    byteOffset++;
                } else {
                    if (visualColumn + 1 > terminalColumns && visualColumn > 0) break;

                    obyte(c);
                    visualColumn++;
                    byteOffset++;

                    while (byteOffset < lineBytesLength && (G->ScratchLineBuffer[byteOffset] & 0xC0) == 0x80) {
                        obyte(G->ScratchLineBuffer[byteOffset]);

                        byteOffset++;
                    }
                }
            }

            currentScreenRow++;
        }

        currentLine++;
    }

    while (currentScreenRow < screenRows) {
        setcur(currentScreenRow, 0);
        seqout("2K");
        obyte('~');

        currentScreenRow++;
    }

    setcur(G->ScreenRows - 1, 0);
    seqout("2K");

    switch (G->CurrentMode) {
        case EDITOR_MODE_COMMAND: {
            obyte(':');
            obytes(G->CommandLineBuffer, G->CommandLineLength);

            break;
        }

        default: {
            switch (G->CurrentMode) {
                case EDITOR_MODE_INSERT: {
                    seqout("7m");
                    ostr(" INSERT ");
                    seqout("m");
                    obyte(' ');

                    break;
                }

                default: {
                    seqout("7m");
                    ostr(" NORMAL ");
                    seqout("m");
                    obyte(' ');

                    break;
                }
            }

            if (G->StatusLength) obytes(G->StatusBuffer, G->StatusLength);

            else {
                ostr(G->Filename[0] ? G->Filename : "[No Name]");
                if (G->Dirty) ostr(" [+]");
            }

            rowString = ull2s(G->CursorRow + 1, rowBuffer);
            columnString = ull2s(G->CursorColumn + 1, columnBuffer);
            rowStringLength = strlen((const char*) rowString);
            columnStringLength = strlen((const char*) columnString);
            infoLength = rowStringLength + 1 + columnStringLength;

            if (G->ScreenColumns > infoLength + 1) {
                setcur(G->ScreenRows - 1, (unsigned short) (G->ScreenColumns - infoLength - 1));
                ostr(rowString);
                obyte(':');
                ostr(columnString);
            } break;
        }
    }

    switch (G->CurrentMode) {
        case EDITOR_MODE_COMMAND: {
            setcur(G->ScreenRows - 1, (unsigned short) (1 + G->CommandLineLength));

            break;
        }

        default: {
            visualRow = 0;

            for (lineIndex = G->TopLineIndex; lineIndex < G->CursorRow; lineIndex++) visualRow += linelay(lineIndex, (size_t) -1, 0, 0);

            linelay(G->CursorRow, G->CursorColumn, &rowInLine, &columnInLine);
            visualRow += rowInLine;
            setcur((unsigned short) visualRow, (unsigned short) columnInLine);

            break;
        }
    }

    seqout("?7h");
    seqout("?25h");

    oflush();
}

static ABI INLINE size_t offsetcur(void) {
    size_t offset, totalLength;

    offset = pt_line_offset(G->CursorRow) + G->CursorColumn;
    totalLength = (G->Root ? G->Root->SubtreeLength : 0);

    return offset > totalLength ? totalLength : offset;
}

static ABI void insbuf(size_t offset, char8_t c) {
    undo_begin_edit();
    pt_insertb(offset, &c, 1);

    G->Dirty = true;
}

static ABI void delbuf(size_t offset) {
    if (offset >= (G->Root ? G->Root->SubtreeLength : 0)) return;

    undo_begin_edit();
    pt_delete_range(offset, 1);

    G->Dirty = true;
}

static ABI void delete_char_at_cursor(void) {
    size_t len, startOffset, remainingBytes, characterLength, iterationIndex;

    len = linelen(G->CursorRow);
    if (len > 0 && G->CursorColumn < len) {
        startOffset = pt_line_offset(G->CursorRow);
        remainingBytes = len - G->CursorColumn;
        characterLength = utf8clen(pt_character_at(startOffset + G->CursorColumn));

        if (characterLength > remainingBytes) characterLength = remainingBytes;

        for (iterationIndex = 0; iterationIndex < characterLength; iterationIndex++) delbuf(offsetcur());
    }

    G->Undo = false;
}

static ABI void adjscr(void) {
    unsigned short screenRows;
    size_t visualRows, lineIndex, rowInLine;

    screenRows = G->ScreenRows - 1;

    if (G->CursorRow < G->TopLineIndex) G->TopLineIndex = G->CursorRow;

    visualRows = 0;
    for (lineIndex = G->TopLineIndex; lineIndex < G->CursorRow; lineIndex++) visualRows += linelay(lineIndex, (size_t) -1, 0, 0);

    linelay(G->CursorRow, G->CursorColumn, &rowInLine, (size_t*) NULL);
    visualRows += rowInLine + 1;

    while (visualRows > screenRows && G->TopLineIndex < G->CursorRow) {
        visualRows -= linelay(G->TopLineIndex, (size_t) -1, 0, 0);

        G->TopLineIndex++;
    }
}

static ABI void movcur(int direction) {
    size_t len, startOffset;

    len = linelen(G->CursorRow);
    startOffset = pt_line_offset(G->CursorRow);

    switch (direction) {
        case 'h': {
            if (G->CursorColumn > 0) {
                G->CursorColumn--;

                while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
            } break;
        }

        case 'l': {
            if (len > 0 && G->CursorColumn < len) {
                G->CursorColumn++;
                while (G->CursorColumn < len && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
            } break;
        }

        case 'j': {
            if (G->CursorRow + 1 < ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) G->CursorRow++;

            break;
        }

        case 'k': {
            if (G->CursorRow > 0) G->CursorRow--;

            break;
        }

        case '0': {
            G->CursorColumn = 0;

            break;
        }

        case '$': {
            if (len > 0) {
                if (G->CurrentMode == EDITOR_MODE_INSERT) G->CursorColumn = len;

                else {
                    G->CursorColumn = len - 1;
                    while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
                }
            } else G->CursorColumn = 0;

            break;
        }

        case 0x06: {
            G->CursorRow += (G->ScreenRows - 1) / 2;
            if (G->CursorRow >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) && ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0) G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;

            break;
        }

        case 0x02: {
            if (G->CursorRow >= (size_t) (G->ScreenRows - 1) / 2) G->CursorRow -= (G->ScreenRows - 1) / 2;
            else G->CursorRow = 0;

            break;
        }

        default: break;
    }
}
