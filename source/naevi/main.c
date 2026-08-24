// source/naevi/main.c

#define WNONBUILTINS

#include <headers/typ.h>
#include <headers/def.h>
#include <headers/str.h>

#include <naevi/headers/pt.h>
#include <naevi/headers/posix.h>

#define TAB_WIDTH 4
#define MAX_HISTORY 4096

#define NORMAL_MODE 0
#define INSERT_MODE 1
#define COMMAND_MODE 2

#define PS_ORIGIN 0
#define PS_ADD 1

#define DIRTY_NONE 0
#define DIRTY_LINE 1
#define DIRTY_FROM 2
#define DIRTY_FULL 3

#define INSERT_KEY 0x2D

#define true ((unsigned byte) 1)
#define false ((unsigned byte) 0)

#ifdef __x86_64__
#if defined(__CYGWIN__) || defined(__MSYS__) || defined(_WIN32)
#define ABI __attribute__((ms_abi))
#else
#define ABI __attribute__((sysv_abi))
#endif
#else
#define ABI
#endif

#define INLINE __attribute__((always_inline)) __inline__

typedef struct {
	PN *Root;
	PN **UndoStack, **RedoStack;

	size_t *Newlines;
	unsigned byte *AddBuffer, *Buffer;

	size_t AddCapacity, AddLength, CommandLineLength, CursorColumn, CursorRow, DirtyLine, Length, NewlineCount, OutputLength, RedoCapacity, RedoCount, RenderedTopLine, StatusLength, TopLineIndex, UndoCapacity, UndoCount;
	unsigned dword RNGState;

	ws_row ScreenRows;
	ws_col ScreenColumns;

	termios Termios;
	unsigned byte Character, CurrentMode, Dirty, DirtyKind, Force, Lead, Pushback, Running, Undo;

	unsigned byte CommandLineBuffer[4096], Filename[4096], StatusBuffer[4096];
	unsigned byte OutputBuffer[1024 * 64], ScratchLineBuffer[1024 * 64], ScratchSaveBuffer[1024 * 64];

	unsigned byte padding[3];
} Globals;

static Globals GlobalData;
static Globals *G = &GlobalData;

void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);

static ABI unsigned char ready(signed int);
static ABI unsigned char *ull2s(unsigned qword, unsigned byte *);

static ABI void acap(size_t);
static ABI INLINE unsigned int rng_next(void);

static ABI PN *pn_create(unsigned int, size_t, size_t, size_t, PN *, PN *, unsigned int);
static ABI INLINE PN *pn_retain(PN *);

static ABI void pn_release(PN *);
static ABI PN *pn_merge(PN *, PN *);
static ABI SP pn_split(PN *, size_t);

static ABI size_t lown(size_t);
static ABI size_t cntnl(unsigned int, size_t, size_t);
static ABI size_t findnl(unsigned int, size_t, size_t, size_t);

static ABI PN *pt_locate(size_t, size_t *, size_t *);
static ABI unsigned char pt_character_at(size_t);

static ABI size_t pt_extract(size_t, size_t, unsigned char *, size_t);
static ABI unsigned char pt_offset_of_newline(size_t, size_t *);

static ABI size_t pt_line_offset(size_t);
static ABI void pt_line_bounds(size_t, size_t *, size_t *);
static ABI void pt_insertb(size_t, const unsigned char *, size_t);
static ABI void pt_delete_range(size_t, size_t);
static ABI void pt_build_index(void);

static ABI void ucap(size_t);
static ABI void rcap(size_t);
static ABI void pushu(PN *);
static ABI INLINE PN *popu(void);

static ABI void pushr(PN *);
static ABI INLINE PN *popr(void);

static ABI void clrr(void);
static ABI void undo_begin_edit(void);
static ABI void fixcur(void);
static ABI void undo(void);
static ABI void redo(void);

static ABI size_t linelen(size_t);
static ABI size_t linelay(size_t, size_t, size_t *, size_t *);
static ABI INLINE size_t utf8clen(unsigned int);

static ABI void clamp_column(void);
static ABI void oflush(void);
static ABI void obytes(const unsigned char *, size_t);
static ABI void ostr(const unsigned char *);
static ABI INLINE void obyte(unsigned int);

static ABI void seqout(const unsigned char *);
static ABI void setcur(unsigned int, unsigned int);
static ABI void status(const unsigned char *);
static ABI void save(void);
static ABI void render(void);
static ABI INLINE size_t offsetcur(void);

static ABI void insbuf(size_t, unsigned int, size_t);
static ABI void delbuf(size_t, size_t);
static ABI void delete_char_at_cursor(void);
static ABI void adjscr(void);
static ABI void movcur(signed int);

static ABI void mark_dirty(unsigned int, size_t);
static ABI size_t visual_row_of(size_t);
static ABI ws_row draw_line(size_t, unsigned int, unsigned int, size_t);

main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	static off_t fileSize;
	static termios settings;
	static winsize windowSize;
	static tcflag_t terminalMask;
	static ssize_t readBytesCount;
	static signed dword fd, inputByte;
	static unsigned byte drainByte, isMergingUp, isNavigationKey, keyCode, nextCharacter, rawInputByte, statBuffer[STAT_BUFFER_SIZE], tildeCharacter;
	static size_t bytesToDelete, characterLength, clearIndex, commandClearIndex, deleteIndex, endOffset, joinColumn, nameLength, offset, oldColumn, pathLength, startOffset, totalBytesRead, len;

	static off_t *sizePointer;
	static unsigned byte *commandString;
	static const unsigned byte *filePath;

	(void) envp;

	G->ScreenRows = 24;
	G->ScreenColumns = 80;
	G->CurrentMode = NORMAL_MODE;
	G->RNGState = 0x9E3779B9U;
	G->Running = true;

	if (ioctl(1, TIOCGWINSZ, &windowSize) == 0) {
		if (windowSize.ws_row > 0)
			G->ScreenRows = windowSize.ws_row;
		if (windowSize.ws_col > 0)
			G->ScreenColumns = windowSize.ws_col;
	}

	if (argc < 2) G->Root = 0;
	else {
		filePath = (const unsigned byte *) argv[1];
		pathLength = strlen((const byte *) filePath);

		if (pathLength >= sizeof(G->Filename))
			pathLength = sizeof(G->Filename) - 1;

		for (clearIndex = 0; clearIndex < sizeof(G->Filename); clearIndex++)
			G->Filename[clearIndex] = 0;

		memcpy(G->Filename, filePath, pathLength);

		fd = open(argv[1], O_RDONLY, 0);
		if (fd < 0) G->Root = 0;
		else {
			if (fstat(fd, (stat *) statBuffer) != 0) fileSize = -1;
			else {
				sizePointer = (off_t *) (statBuffer + STAT_SIZE_OFFSET);

				fileSize = *sizePointer;
			}

			if (fileSize > 0) {
				totalBytesRead = 0;

				G->Buffer = (unsigned byte *) malloc((size_t) fileSize);
				if (!G->Buffer) {
					close(fd);

					if (G->Newlines) free(G->Newlines);

					return 255;
				}

				while (totalBytesRead < (size_t) fileSize) {
					readBytesCount = read(fd, G->Buffer + totalBytesRead, (size_t) fileSize - totalBytesRead);
					if (readBytesCount <= 0) break;

					totalBytesRead += (size_t) readBytesCount;
				}

				G->Length = totalBytesRead;
				pt_build_index();

				G->Root = (G->Length > 0) ? pn_create(PS_ORIGIN, 0, G->Length, G->NewlineCount, 0, 0, rng_next()) : 0;
			} else G->Root = 0;

			close(fd);

			G->Dirty = false;
		}
	}

	tcgetattr(0, &G->Termios);

	settings = G->Termios;

	terminalMask = ICANON | ECHO | ISIG;

	settings.c_lflag &= ~terminalMask;
	settings.c_cc[VMIN] = 1;
	settings.c_cc[VTIME] = 0;

	tcsetattr(0, TCSANOW, &settings);

	seqout((const unsigned byte *) "2J");

	G->DirtyKind = DIRTY_FULL;
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

		if (inputByte == 0x1B && ready(20)) {
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
											case '2': inputByte = INSERT_KEY; isNavigationKey = true; break;
											case '3': inputByte = 'x'; isNavigationKey = true; break;
											case '5': inputByte = 0x02; isNavigationKey = true; break;
											case '6': inputByte = 0x06; isNavigationKey = true; break;

											default: break;
										}
									} else {
										drainByte = tildeCharacter;
										while ((drainByte < 0x40 || drainByte > 0x7E) && ready(20))
											if (read(0, &drainByte, 1) <= 0) break;
									}
								}
							} break;
						}

						default: break;
					}
				} else {
					G->Character = nextCharacter;
					G->Pushback = true;

					inputByte = 0x1B;
				}
			}

			if (isNavigationKey) {
				if (inputByte == INSERT_KEY) {
					if (G->CurrentMode == NORMAL_MODE || G->CurrentMode == INSERT_MODE) {
						if (G->CurrentMode == INSERT_MODE) {
							if (G->CursorColumn > 0) {
								startOffset = pt_line_offset(G->CursorRow);

								G->CursorColumn--;
								while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80)
									G->CursorColumn--;
							}

							G->CurrentMode = NORMAL_MODE;
							G->Undo = false;
						} else G->CurrentMode = INSERT_MODE;

						clamp_column();
						adjscr();
					}
				} else {
					switch (G->CurrentMode) {
						case NORMAL_MODE: case INSERT_MODE: {
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

			if (inputByte == 0x1B) {
				switch (G->CurrentMode) {
					case INSERT_MODE: {
						if (G->CursorColumn > 0) {
							startOffset = pt_line_offset(G->CursorRow);

							G->CursorColumn--;
							while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80)
								G->CursorColumn--;
						}

						G->CurrentMode = NORMAL_MODE;
						G->Undo = false;

						switch (G->CurrentMode) {
							case NORMAL_MODE: clamp_column(); break;

							default: break;
						}

						adjscr();

						break;
					}

					case COMMAND_MODE: {
						G->CurrentMode = NORMAL_MODE;
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
			case NORMAL_MODE: {
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
								mark_dirty(DIRTY_FROM, G->CursorRow);
							}
						}

						if (G->CursorRow >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) && ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0)
							G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;

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

							case 'G': if (((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0) {
								G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;
							} break;

							case 'g': G->Lead = 'g'; break;
							case 'i': G->CurrentMode = INSERT_MODE; break;

							case 'a': {
								len = linelen(G->CursorRow);
								G->CurrentMode = INSERT_MODE;

								if (len > 0) {
									startOffset = pt_line_offset(G->CursorRow);
									characterLength = utf8clen(pt_character_at(startOffset + G->CursorColumn));
									G->CursorColumn += characterLength;
								} break;
							}

							case 'A': G->CurrentMode = INSERT_MODE; G->CursorColumn = linelen(G->CursorRow); break;
							case 'I': G->CursorColumn = 0; G->CurrentMode = INSERT_MODE; break;

							case 'o': {
								offset = pt_line_offset(G->CursorRow) + linelen(G->CursorRow);
								if (offset < (G->Root ? G->Root->SubtreeLength : 0) && pt_character_at(offset) == '\n')
									offset++;

								insbuf(offset, '\n', G->CursorRow);

								G->CursorRow++;
								G->CursorColumn = 0;
								G->CurrentMode = INSERT_MODE;

								break;
							}

							case 'O': {
								insbuf(pt_line_offset(G->CursorRow), '\n', G->CursorRow);

								G->CursorColumn = 0;
								G->CurrentMode = INSERT_MODE;

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
								G->CurrentMode = COMMAND_MODE;
								G->CommandLineLength = 0;

								for (commandClearIndex = 0; commandClearIndex < sizeof(G->CommandLineBuffer); commandClearIndex++)
									G->CommandLineBuffer[commandClearIndex] = 0;

								break;
							}

							case 0x06: movcur(0x06); break;
							case 0x02: movcur(0x02); break;

							default: break;
						} break;
					}
				}

				switch (G->CurrentMode) {
					case NORMAL_MODE: clamp_column(); break;

					default: break;
				}

				adjscr();

				break;
			}

			case INSERT_MODE: {
				switch (inputByte) {
					case 0x1B: {
						if (G->CursorColumn > 0) {
							startOffset = pt_line_offset(G->CursorRow);

							G->CursorColumn--;
							while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80)
								G->CursorColumn--;
						}

						G->CurrentMode = NORMAL_MODE;
						G->Undo = false;

						break;
					}

					case 127: case 8: {
						if (offsetcur() > 0) {
							isMergingUp = (G->CursorColumn == 0 && G->CursorRow > 0);
							joinColumn = isMergingUp ? linelen(G->CursorRow - 1) : 0;

							if (isMergingUp) {
								delbuf(offsetcur() - 1, G->CursorRow - 1);

								G->CursorRow--;
								G->CursorColumn = joinColumn;
							} else if (G->CursorColumn > 0) {
								startOffset = pt_line_offset(G->CursorRow);
								oldColumn = G->CursorColumn;

								G->CursorColumn--;
								while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80)
									G->CursorColumn--;

								bytesToDelete = oldColumn - G->CursorColumn;
								for (deleteIndex = 0; deleteIndex < bytesToDelete; deleteIndex++)
									delbuf(startOffset + G->CursorColumn, G->CursorRow);
							}
						} break;
					}

					case '\n': case '\r': {
						insbuf(offsetcur(), '\n', G->CursorRow);

						G->CursorRow++;
						G->CursorColumn = 0;

						break;
					}

					case '\t': {
						insbuf(offsetcur(), '\t', G->CursorRow);
						G->CursorColumn++;

						break;
					}

					default: {
						if (inputByte >= 32 && inputByte < 0x100) {
							insbuf(offsetcur(), (unsigned byte) inputByte, G->CursorRow);
							G->CursorColumn++;
						} break;
					}
				}

				switch (G->CurrentMode) {
					case NORMAL_MODE: clamp_column(); break;

					default: break;
				}

				adjscr();

				break;
			}

			case COMMAND_MODE: {
				switch (inputByte) {
					case 0x1B: {
						G->CurrentMode = NORMAL_MODE;
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
										if (G->Dirty && !G->Force) {
											status((const unsigned byte *) "Unsaved changes, :q! to force.");
										} else {
											G->Running = false;
										} break;
									}

									case '!': {
										switch (commandString[2]) {
											case '\0': {
												G->Running = false;

												break;
											}

											default: status((const unsigned byte *) "Unknown command."); break;
										} break;
									}

									default: status((const unsigned byte *) "Unknown command."); break;
								} break;
							}

							case 'w': {
								switch (commandString[1]) {
									case '\0': case '!': {
										switch (commandString[commandString[1] == '!' ? 2 : 1]) {
											case '\0': save(); break;

											default: status((const unsigned byte *) "Unknown command."); break;
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

													default: status((const unsigned byte *) "Unknown command."); break;
												} break;
											}

											case ' ': {
												switch (commandString[3]) {
													case '\0': status((const unsigned byte *) "Unknown command."); break;

													default: {
														nameLength = strlen((const byte *) (commandString + 3));

														if (nameLength >= sizeof(G->Filename))
															nameLength = sizeof(G->Filename) - 1;

														memcpy(G->Filename, commandString + 3, nameLength);
														G->Filename[nameLength] = '\0';

														save();
														G->Running = false;

														break;
													}
												} break;
											}

											default: status((const unsigned byte *) "Unknown command."); break;
										} break;
									}

									case ' ': {
										switch (commandString[2]) {
											case '\0': status((const unsigned byte *) "Unknown command."); break;

											default: {
												nameLength = strlen((const byte *) (commandString + 2));

												if (nameLength >= sizeof(G->Filename))
													nameLength = sizeof(G->Filename) - 1;

												memcpy(G->Filename, commandString + 2, nameLength);
												G->Filename[nameLength] = '\0';

												save(); break;
											}
										} break;
									}

									default: status((const unsigned byte *) "Unknown command."); break;
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

											default: status((const unsigned byte *) "Unknown command."); break;
										} break;
									}

									default: status((const unsigned byte *) "Unknown command."); break;
								} break;
							}

							default: status((const unsigned byte *) "Unknown command."); break;
						}

						G->CurrentMode = NORMAL_MODE;
						G->CommandLineLength = 0;

						break;
					}

					case 127: case 8: {
						if (G->CommandLineLength > 0)
							G->CommandLineLength--;

						break;
					}

					default: {
						if (inputByte >= 32 && inputByte < 127 && G->CommandLineLength + 1 < sizeof(G->CommandLineBuffer))
							G->CommandLineBuffer[G->CommandLineLength++] = (unsigned byte) inputByte;

						break;
					}
				} break;
			}

			default: break;
		}

		render();
	}

	tcsetattr(0, TCSANOW, &G->Termios);

	seqout((const unsigned byte *) "?7h");
	seqout((const unsigned byte *) "2J");

	setcur(0, 0);
	oflush();

	if (G->Buffer) free(G->Buffer);
	if (G->Newlines) free(G->Newlines);

	return 0;
}

static ABI unsigned byte ready(ms)
signed dword ms;
{
	static pollfd pollDescriptor;

	pollDescriptor.fd = 0;
	pollDescriptor.events = POLLIN;
	pollDescriptor.revents = 0;

	return poll(&pollDescriptor, 1, ms) > 0;
}

static ABI unsigned byte *ull2s(value, buffer)
unsigned qword value;
unsigned byte *buffer;
{
	static unsigned byte *pointer;

	pointer = buffer + 20;

	*pointer = '\0';
	if (value == 0) {
		*--pointer = '0';

		return pointer;
	}

	while (value > 0) {
		*--pointer = (unsigned byte) ('0' + (value % 10));

		value /= 10;
	}

	return pointer;
}

static ABI void acap(neededCapacity)
size_t neededCapacity;
{
	static size_t newCapacity;
	static unsigned byte *newBuffer;

	if (neededCapacity <= G->AddCapacity) return;

	newCapacity = G->AddCapacity ? G->AddCapacity : 4096;
	while (newCapacity < neededCapacity) newCapacity *= 2;

	newBuffer = (unsigned byte *) realloc(G->AddBuffer, newCapacity);
	if (newBuffer) {
		G->AddBuffer = newBuffer;
		G->AddCapacity = newCapacity;
	}
}

static ABI INLINE unsigned dword rng_next(void) {
	static unsigned dword stateValue;

	stateValue = G->RNGState;
	stateValue ^= stateValue << 13;
	stateValue ^= stateValue >> 17;
	stateValue ^= stateValue << 5;
	G->RNGState = stateValue;

	return stateValue;
}

static ABI PN *pn_create(Source, startOffset, len, lineFeeds, leftChild, rightChild, Priority)
unsigned dword Source;
size_t startOffset, len, lineFeeds;
PN *leftChild, *rightChild;
unsigned dword Priority;
{
	static PN *Node;

	Node = (PN *) malloc(sizeof(PN));
	if (!Node) return (PN *) NULL;

	Node->Source = Source;
	Node->StartOffset = startOffset;
	Node->Length = len;
	Node->LineFeeds = lineFeeds;
	Node->LeftChild = leftChild;
	Node->RightChild = rightChild;
	Node->Priority = Priority;
	Node->ReferenceCount = 1;

	Node->SubtreeLength = len + (leftChild ? leftChild->SubtreeLength : 0) + (rightChild ? rightChild->SubtreeLength : 0);
	Node->SubtreeLineFeeds = lineFeeds + (leftChild ? leftChild->SubtreeLineFeeds : 0) + (rightChild ? rightChild->SubtreeLineFeeds : 0);

	return Node;
}

static ABI INLINE PN *pn_retain(Node)
PN *Node;
{
	if (Node) Node->ReferenceCount++;

	return Node;
}

static ABI void pn_release(Node)
PN *Node;
{
	if (!Node) return;

	Node->ReferenceCount--;
	if (Node->ReferenceCount <= 0) {
		pn_release(Node->LeftChild);
		pn_release(Node->RightChild);

		free(Node);
	}
}

static ABI PN *pn_merge(leftNode, rightNode)
PN *leftNode, *rightNode;
{
	static PN *newRightChild;
	static PN *newLeftChild;

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

static ABI SP pn_split(Node, splitKey)
PN *Node;
size_t splitKey;
{
	static SP splitResult, subtreeSplitResult;
	static size_t leftLineFeeds, leftSubtreeLength, offsetInPiece, rightLineFeeds;

	static PN *newRightNode, *newLeftNode;

	if (!Node) {
		splitResult.LeftNode = 0;
		splitResult.RightNode = 0;

		return splitResult;
	}

	leftSubtreeLength = Node->LeftChild ? Node->LeftChild->SubtreeLength : 0;

	if (splitKey <= leftSubtreeLength) {
		subtreeSplitResult = pn_split(Node->LeftChild, splitKey);
		newRightNode = pn_create(Node->Source, Node->StartOffset, Node->Length, Node->LineFeeds, subtreeSplitResult.RightNode, pn_retain(Node->RightChild), Node->Priority);

		splitResult.LeftNode = subtreeSplitResult.LeftNode;
		splitResult.RightNode = newRightNode;

		return splitResult;
	} else if (splitKey >= leftSubtreeLength + Node->Length) {
		subtreeSplitResult = pn_split(Node->RightChild, splitKey - leftSubtreeLength - Node->Length);
		newLeftNode = pn_create(Node->Source, Node->StartOffset, Node->Length, Node->LineFeeds, pn_retain(Node->LeftChild), subtreeSplitResult.LeftNode, Node->Priority);

		splitResult.LeftNode = newLeftNode;
		splitResult.RightNode = subtreeSplitResult.RightNode;

		return splitResult;
	} else {
		offsetInPiece = splitKey - leftSubtreeLength;
		leftLineFeeds = cntnl(Node->Source, Node->StartOffset, offsetInPiece);
		rightLineFeeds = Node->LineFeeds - leftLineFeeds;

		splitResult.LeftNode = pn_create(Node->Source, Node->StartOffset, offsetInPiece, leftLineFeeds, pn_retain(Node->LeftChild), 0, rng_next());
		splitResult.RightNode = pn_create(Node->Source, Node->StartOffset + offsetInPiece, Node->Length - offsetInPiece, rightLineFeeds, 0, pn_retain(Node->RightChild), rng_next());

		return splitResult;
	}
}

static ABI size_t lown(targetValue)
size_t targetValue;
{
	static size_t lowerBound, middleIndex, upperBound;

	lowerBound = 0;
	upperBound = G->NewlineCount;

	while (lowerBound < upperBound) {
		middleIndex = lowerBound + (upperBound - lowerBound) / 2;
		if (G->Newlines[middleIndex] < targetValue)
			lowerBound = middleIndex + 1;
		else upperBound = middleIndex;
	}

	return lowerBound;
}

static ABI size_t cntnl(Source, startOffset, len)
unsigned dword Source;
size_t startOffset, len;
{
	static size_t endIndex, newlineCount, startIndex, i;

	if (len == 0) return 0;

	if (Source == PS_ORIGIN) {
		startIndex = lown(startOffset);
		endIndex = lown(startOffset + len);

		return endIndex - startIndex;
	} else {
		newlineCount = 0;
		for (i = 0; i < len; i++) if (G->AddBuffer[startOffset + i] == '\n')
			newlineCount++;

		return newlineCount;
	}
}

static ABI size_t findnl(Source, startOffset, len, targetNewlineIndex)
unsigned dword Source;
size_t startOffset, len, targetNewlineIndex;
{
	static size_t lowerBoundIndex, i;

	if (Source == PS_ORIGIN) {
		lowerBoundIndex = lown(startOffset);

		return G->Newlines[lowerBoundIndex + targetNewlineIndex] - startOffset;
	} else {
		for (i = 0; i < len; i++) {
			if (G->AddBuffer[startOffset + i] == '\n') {
				if (targetNewlineIndex == 0) {
					return i;
				}

				targetNewlineIndex--;
			}
		}

		return len;
	}
}

static ABI PN *pt_locate(offset, pieceAbsoluteStart, offsetInPiece)
size_t offset;
size_t *pieceAbsoluteStart, *offsetInPiece;
{
	static size_t baseOffset, leftSubtreeLength;

	static PN *currentNode;

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

static ABI unsigned byte pt_character_at(offset)
size_t offset;
{
	static size_t offsetInPiece, pieceAbsoluteStart;

	static PN *Node;
	static const unsigned byte *sourceBuffer;

	Node = pt_locate(offset, &pieceAbsoluteStart, &offsetInPiece);

	if (!Node) return 0;

	sourceBuffer = (Node->Source == PS_ORIGIN) ? G->Buffer : G->AddBuffer;
	return sourceBuffer[Node->StartOffset + offsetInPiece];
}

static ABI size_t pt_extract(offset, len, destinationBuffer, destinationCapacity)
size_t offset, len;
unsigned byte *destinationBuffer;
size_t destinationCapacity;
{
	static size_t availableBytes, bytesWritten, destinationRoom, offsetInPiece, pieceAbsoluteStart, requestedBytes, totalLength;

	static PN *Node;
	static const unsigned byte *sourceBuffer;

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
		if (requestedBytes > destinationRoom)
			requestedBytes = destinationRoom;
		if (requestedBytes == 0) break;

		sourceBuffer = (Node->Source == PS_ORIGIN) ? G->Buffer : G->AddBuffer;
		memcpy(destinationBuffer + bytesWritten, sourceBuffer + Node->StartOffset + offsetInPiece, requestedBytes);

		bytesWritten += requestedBytes;
		offset += requestedBytes;
		len -= requestedBytes;
	}

	return bytesWritten;
}

static ABI unsigned byte pt_offset_of_newline(newlineIndex, outOffset)
size_t newlineIndex;
size_t *outOffset;
{
	static size_t baseOffset, leftSubtreeLineFeeds, leftSubtreeLength;

	static PN *Node;

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

static ABI size_t pt_line_offset(lineNumber)
size_t lineNumber;
{
	static size_t characterOffset;

	if (lineNumber == 0) return 0;
	if (pt_offset_of_newline(lineNumber - 1, &characterOffset))
		return characterOffset + 1;

	return (G->Root ? G->Root->SubtreeLength : 0);
}

static ABI void pt_line_bounds(lineNumber, outStart, outLength)
size_t lineNumber;
size_t *outStart, *outLength;
{
	static size_t endOffset, lineCount, startOffset, totalLength;

	lineCount = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);
	totalLength = (G->Root ? G->Root->SubtreeLength : 0);

	if (lineNumber >= lineCount) {
		if (outStart) *outStart = totalLength;
		if (outLength) *outLength = 0;

		return;
	}

	startOffset = pt_line_offset(lineNumber);
	endOffset = (lineNumber + 1 < lineCount) ? pt_line_offset(lineNumber + 1) - 1 : totalLength;

	if (outStart) *outStart = startOffset;
	if (outLength) *outLength = (endOffset > startOffset) ? (endOffset - startOffset) : 0;
}

static ABI void pt_insertb(offset, dataBytes, len)
size_t offset;
const unsigned byte *dataBytes;
size_t len;
{
	static SP splitResult;
	static size_t lineFeedCount, pieceStartOffset;

	static PN *newPieceNode, *mergedNode1, *newRootNode;

	if (len == 0) return;

	acap(G->AddLength + len);
	memcpy(G->AddBuffer + G->AddLength, dataBytes, len);

	pieceStartOffset = G->AddLength;
	G->AddLength += len;

	lineFeedCount = cntnl(PS_ADD, pieceStartOffset, len);
	newPieceNode = pn_create(PS_ADD, pieceStartOffset, len, lineFeedCount, 0, 0, rng_next());

	splitResult = pn_split(G->Root, offset);
	mergedNode1 = pn_merge(splitResult.LeftNode, newPieceNode);
	newRootNode = pn_merge(mergedNode1, splitResult.RightNode);

	pn_release(splitResult.LeftNode);
	pn_release(splitResult.RightNode);
	pn_release(newPieceNode);
	pn_release(mergedNode1);

	pn_release(G->Root);

	G->Root = newRootNode;
}

static ABI void pt_delete_range(offset, len)
size_t offset, len;
{
	static SP splitResult1, splitResult2;

	static PN *newRootNode;

	if (len == 0) return;

	splitResult1 = pn_split(G->Root, offset);
	splitResult2 = pn_split(splitResult1.RightNode, len);
	newRootNode = pn_merge(splitResult1.LeftNode, splitResult2.RightNode);

	pn_release(splitResult1.LeftNode);
	pn_release(splitResult1.RightNode);
	pn_release(splitResult2.LeftNode);
	pn_release(splitResult2.RightNode);

	pn_release(G->Root);

	G->Root = newRootNode;
}

static ABI void pt_build_index(void) {
	static size_t count, i;

	count = 0;
	for (i = 0; i < G->Length; i++)
		if (G->Buffer[i] == '\n') count++;

	G->Newlines = count ? (size_t *) malloc(count * sizeof(size_t)) : 0;
	if (count && !G->Newlines) {
		G->NewlineCount = 0;

		return;
	}

	G->NewlineCount = 0;

	for (i = 0; i < G->Length; i++)
		if (G->Buffer[i] == '\n') G->Newlines[G->NewlineCount++] = i;
}

static ABI void ucap(neededCapacity)
size_t neededCapacity;
{
	static size_t newCapacity;

	static PN **newStack;

	if (neededCapacity <= G->UndoCapacity) return;

	newCapacity = G->UndoCapacity ? G->UndoCapacity : 64;
	while (newCapacity < neededCapacity) newCapacity *= 2;

	newStack = (PN **) realloc(G->UndoStack, newCapacity * sizeof(PN *));
	if (newStack) {
		G->UndoStack = newStack;
		G->UndoCapacity = newCapacity;
	}
}

static ABI void rcap(neededCapacity)
size_t neededCapacity;
{
	static size_t newCapacity;
	static PN **newStack;

	if (neededCapacity <= G->RedoCapacity) return;

	newCapacity = G->RedoCapacity ? G->RedoCapacity : 64;
	while (newCapacity < neededCapacity) newCapacity *= 2;

	newStack = (PN **) realloc(G->RedoStack, newCapacity * sizeof(PN *));
	if (newStack) {
		G->RedoStack = newStack;
		G->RedoCapacity = newCapacity;
	}
}

static ABI void pushu(rootNode)
PN *rootNode;
{
	static size_t i;

	if (G->UndoCount >= MAX_HISTORY) {
		pn_release(G->UndoStack[0]);
		for (i = 1; i < G->UndoCount; i++)
			G->UndoStack[i - 1] = G->UndoStack[i];

		G->UndoCount--;
	}

	ucap(G->UndoCount + 1);
	G->UndoStack[G->UndoCount++] = pn_retain(rootNode);
}

static ABI INLINE PN *popu(void) {
	if (G->UndoCount == 0) return 0;

	return G->UndoStack[--G->UndoCount];
}

static ABI void pushr(rootNode)
PN *rootNode;
{
	rcap(G->RedoCount + 1);

	G->RedoStack[G->RedoCount++] = pn_retain(rootNode);
}

static ABI INLINE PN *popr(void) {
	if (G->RedoCount == 0) return 0;

	return G->RedoStack[--G->RedoCount];
}

static ABI void clrr(void) {
	static size_t i;

	for (i = 0; i < G->RedoCount; i++) pn_release(G->RedoStack[i]);

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
	static size_t lineCount;

	lineCount = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

	if (G->CursorRow >= lineCount)
		G->CursorRow = lineCount - 1;

	clamp_column();
	adjscr();
}

static ABI void undo(void) {
	static PN *previousRootNode;

	if (G->UndoCount == 0) {
		status((const unsigned byte *) "Already at oldest change.");

		return;
	}

	pushr(G->Root);

	previousRootNode = popu();
	pn_release(G->Root);

	G->Root = previousRootNode;
	G->Undo = false;

	fixcur();
	mark_dirty(DIRTY_FULL, 0);
	status((const unsigned byte *) "Undo.");
}

static ABI void redo(void) {
	static PN *nextRootNode;

	if (G->RedoCount == 0) {
		status((const unsigned byte *) "Already at newest change.");

		return;
	}

	pushu(G->Root);

	nextRootNode = popr();
	pn_release(G->Root);

	G->Root = nextRootNode;
	G->Undo = false;

	fixcur();
	mark_dirty(DIRTY_FULL, 0);
	status((const unsigned byte *) "Redo.");
}

static ABI size_t linelen(lineNumber)
size_t lineNumber;
{
	static size_t length;

	pt_line_bounds(lineNumber, (size_t *) NULL, &length);

	return length;
}

static ABI size_t linelay(lineNumber, stopByteOffset, stopRowPointer, stopColumnPointer)
size_t lineNumber, stopByteOffset;
size_t *stopRowPointer, *stopColumnPointer;
{
	static unsigned byte hasStopped, c;
	static size_t byteOffset, lineBytesLength, rowIndex, scratchLength, startOffset, tabWidth, terminalColumns, visualColumn;

	if (lineNumber >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) {
		if (stopRowPointer) *stopRowPointer = 0;
		if (stopColumnPointer) *stopColumnPointer = 0;

		return 1;
	}

	startOffset = 0;
	lineBytesLength = 0;
	pt_line_bounds(lineNumber, &startOffset, &lineBytesLength);
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
			tabWidth = TAB_WIDTH - (visualColumn % TAB_WIDTH);

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

		} else if ((c & 0xC0) == 0x80) {
			byteOffset++;
		} else {
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

static ABI INLINE size_t utf8clen(c)
unsigned dword c;
{
	if ((c & 0x80) == 0x00) return 1;
	if ((c & 0xE0) == 0xC0) return 2;
	if ((c & 0xF0) == 0xE0) return 3;
	if ((c & 0xF8) == 0xF0) return 4;

	return 1;
}

static ABI void clamp_column(void) {
	static size_t len;
	static size_t startOffset;

	len = linelen(G->CursorRow);
	startOffset = pt_line_offset(G->CursorRow);

	if (len == 0) {
		G->CursorColumn = 0;

		return;
	}

	if (G->CurrentMode == INSERT_MODE) {
		if (G->CursorColumn >= len) G->CursorColumn = len;
	} else if (G->CursorColumn >= len) {
		G->CursorColumn = len - 1;
	}

	while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
}

static ABI void oflush(void) {
	if (!G->OutputLength) return;

	write(1, G->OutputBuffer, G->OutputLength);

	G->OutputLength = 0;
}

static ABI void obytes(stringBytes, count)
const unsigned byte *stringBytes;
size_t count;
{
	static size_t available;

	while (count > 0) {
		if (G->OutputLength == (1024 * 64)) oflush();

		available = (1024 * 64) - G->OutputLength;
		if (count < available) available = count;

		memcpy(G->OutputBuffer + G->OutputLength, stringBytes, available);

		G->OutputLength += available;
		stringBytes += available;
		count -= available;
	}
}

static ABI void ostr(string)
const unsigned byte *string;
{
	static size_t stringLength;

	stringLength = strlen((const byte *) string);

	obytes(string, stringLength);
}

static ABI INLINE void obyte(byteValue)
unsigned dword byteValue;
{
	if (G->OutputLength == (1024 * 64)) oflush();

	G->OutputBuffer[G->OutputLength++] = (unsigned byte) byteValue;
}

static ABI void seqout(sequence)
const unsigned byte *sequence;
{
	obyte(0x1B);
	obyte('[');
	ostr(sequence);
}

static ABI void setcur(row, column)
unsigned dword row, column;
{
	static unsigned byte columnBuffer[22], rowBuffer[22];

	static unsigned byte *columnString, *rowString;

	rowString = ull2s(row + 1, rowBuffer);
	columnString = ull2s(column + 1, columnBuffer);

	obyte(0x1B);
	obyte('[');
	ostr(rowString);
	obyte(';');
	ostr(columnString);
	obyte('H');
}

static ABI void status(message)
const unsigned byte *message;
{
	static size_t messageLength;

	messageLength = strlen((const byte *) message);

	if (messageLength >= sizeof(G->StatusBuffer))
		messageLength = sizeof(G->StatusBuffer) - 1;

	memcpy(G->StatusBuffer, message, messageLength);

	G->StatusLength = messageLength;
}

static ABI void save(void) {
	static signed dword fd;
	static ssize_t readWriteResult;
	static unsigned byte numberBuffer[22];
	static size_t bytesWrittenTotal, chunkWrittenBytes, currentPosition, extractedBytes, filenameLength, numberStringLength, requestedBytes, suffixLength, totalLength;

	static unsigned byte *numberString;
	static const unsigned byte *bytesUnitString, *writtenSuffixString;

	bytesWrittenTotal = 0;
	currentPosition = 0;
	filenameLength = 0;
	suffixLength = 0;

	writtenSuffixString = (const unsigned byte *) " written ";
	bytesUnitString = (const unsigned byte *) "B";

	if (!G->Filename[0]) {
		status((const unsigned byte *) "No filename.");

		return;
	}

	fd = open((const byte *) G->Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		status((const unsigned byte *) "Failed to write file.");

		return;
	}

	totalLength = (G->Root ? G->Root->SubtreeLength : 0);

	while (currentPosition < totalLength) {
		requestedBytes = totalLength - currentPosition;
		chunkWrittenBytes = 0;

		if (requestedBytes > sizeof(G->ScratchSaveBuffer))
			requestedBytes = sizeof(G->ScratchSaveBuffer);

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
	numberStringLength = strlen((const byte *) numberString);
	suffixLength = 9 + numberStringLength + 1;

	filenameLength = strlen((const byte *) G->Filename);
	if (filenameLength + suffixLength > sizeof(G->StatusBuffer)) {
		if (sizeof(G->StatusBuffer) > suffixLength)
			filenameLength = sizeof(G->StatusBuffer) - suffixLength;
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

static ABI void mark_dirty(kind, line)
unsigned dword kind;
size_t line;
{
	if (G->DirtyKind == DIRTY_FULL || kind == DIRTY_FULL) {
		G->DirtyKind = DIRTY_FULL;

		return;
	}

	if (G->DirtyKind == DIRTY_NONE) {
		G->DirtyKind = (unsigned byte) kind;
		G->DirtyLine = line;

		return;
	}

	if (kind == DIRTY_LINE && G->DirtyKind == DIRTY_LINE && line == G->DirtyLine) return;

	G->DirtyKind = DIRTY_FROM;
	if (line < G->DirtyLine) G->DirtyLine = line;
}

static ABI size_t visual_row_of(line)
size_t line;
{
	static size_t lineIndex, row;

	row = 0;
	for (lineIndex = G->TopLineIndex; lineIndex < line; lineIndex++)
		row += linelay(lineIndex, (size_t) -1, 0, 0);

	return row;
}

static ABI ws_row draw_line(lineIndex, screenRow, screenRows, terminalColumns)
size_t lineIndex;
unsigned dword screenRow, screenRows;
size_t terminalColumns;
{
	static unsigned byte c;
	static size_t byteOffset, lineBytesLength, scratchLength, startOffset, tabWidth, visualColumn, i;

	startOffset = 0;
	lineBytesLength = 0;
	pt_line_bounds(lineIndex, &startOffset, &lineBytesLength);

	scratchLength = pt_extract(startOffset, lineBytesLength, G->ScratchLineBuffer, sizeof(G->ScratchLineBuffer));
	if (lineBytesLength > scratchLength)
		lineBytesLength = scratchLength;

	if (screenRow >= screenRows)
		return (ws_row) screenRow;

	byteOffset = 0;
	visualColumn = 0;

	setcur(screenRow, 0);
	seqout((const unsigned byte *) "2K");

	while (byteOffset < lineBytesLength) {
		c = G->ScratchLineBuffer[byteOffset];

		if (c == '\t') {
			tabWidth = TAB_WIDTH - (visualColumn % TAB_WIDTH);

			if (visualColumn + tabWidth > terminalColumns && visualColumn > 0) {
				screenRow++;
				if (screenRow >= screenRows)
					return (ws_row) screenRow;

				visualColumn = 0;
				setcur(screenRow, 0);
				seqout((const unsigned byte *) "2K");

				continue;
			}

			for (i = 0; i < tabWidth; i++) obyte(' ');

			visualColumn += tabWidth;
			byteOffset++;
		} else if ((c & 0xC0) == 0x80) {
			obyte(c);

			byteOffset++;
		} else {
			if (visualColumn + 1 > terminalColumns && visualColumn > 0) {
				screenRow++;
				if (screenRow >= screenRows)
					return (ws_row) screenRow;

				visualColumn = 0;
				setcur(screenRow, 0);
				seqout((const unsigned byte *) "2K");

				continue;
			}

			obyte(c);
			visualColumn++;
			byteOffset++;

			while (byteOffset < lineBytesLength && (G->ScratchLineBuffer[byteOffset] & 0xC0) == 0x80) {
				obyte(G->ScratchLineBuffer[byteOffset]);

				byteOffset++;
			}
		}
	}

	return (ws_row) (screenRow + 1);
}

static ABI void render(void) {
	static unsigned byte kind, scrolled;
	static ws_row currentScreenRow, screenRows;
	static unsigned byte columnBuffer[22], rowBuffer[22];
	static size_t columnInLine, columnStringLength, currentLine, infoLength, lineCount, lineIndex, rowInLine, rowStringLength, terminalColumns, visualRow;

	static unsigned byte *columnString, *rowString;

	screenRows = G->ScreenRows - 1;
	terminalColumns = G->ScreenColumns ? G->ScreenColumns : 1;
	lineCount = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

	scrolled = (G->TopLineIndex != G->RenderedTopLine);
	kind = scrolled ? DIRTY_FULL : G->DirtyKind;

	seqout((const unsigned byte *) "?25l");
	seqout((const unsigned byte *) "?7l");

	if (kind == DIRTY_FULL || kind == DIRTY_FROM) {
		currentLine = (kind == DIRTY_FROM && G->DirtyLine > G->TopLineIndex) ? G->DirtyLine : G->TopLineIndex;
		currentScreenRow = (ws_row) visual_row_of(currentLine);

		while (currentScreenRow < screenRows && currentLine < lineCount) {
			currentScreenRow = draw_line(currentLine, currentScreenRow, screenRows, terminalColumns);

			currentLine++;
		}

		while (currentScreenRow < screenRows) {
			setcur(currentScreenRow, 0);
			seqout((const unsigned byte *) "2K");
			obyte('~');

			currentScreenRow++;
		}
	} else if (kind == DIRTY_LINE) {
		currentScreenRow = (ws_row) visual_row_of(G->DirtyLine);

		if (G->DirtyLine >= G->TopLineIndex && G->DirtyLine < lineCount && currentScreenRow < screenRows)
			draw_line(G->DirtyLine, currentScreenRow, screenRows, terminalColumns);
	}

	G->DirtyKind = DIRTY_NONE;
	G->RenderedTopLine = G->TopLineIndex;

	setcur(G->ScreenRows - 1, 0);
	seqout((const unsigned byte *) "2K");

	switch (G->CurrentMode) {
		case COMMAND_MODE: {
			obyte(':');
			obytes(G->CommandLineBuffer, G->CommandLineLength);

			break;
		}

		default: {
			switch (G->CurrentMode) {
				case INSERT_MODE: {
					seqout((const unsigned byte *) "7m");
					ostr((const unsigned byte *) " INSERT ");
					seqout((const unsigned byte *) "m");
					obyte(' ');

					break;
				}

				default: {
					seqout((const unsigned byte *) "7m");
					ostr((const unsigned byte *) " NORMAL ");
					seqout((const unsigned byte *) "m");
					obyte(' ');

					break;
				}
			}

			if (G->StatusLength) {
				obytes(G->StatusBuffer, G->StatusLength);
			} else {
				ostr(G->Filename[0] ? G->Filename : (const unsigned byte *) "[No Name]");
				if (G->Dirty) ostr((const unsigned byte *) " [+]");
			}

			rowString = ull2s(G->CursorRow + 1, rowBuffer);
			columnString = ull2s(G->CursorColumn + 1, columnBuffer);
			rowStringLength = strlen((const byte *) rowString);
			columnStringLength = strlen((const byte *) columnString);
			infoLength = rowStringLength + 1 + columnStringLength;

			if (G->ScreenColumns > infoLength + 1) {
				setcur(G->ScreenRows - 1, (ws_col) (G->ScreenColumns - infoLength - 1));
				ostr(rowString);
				obyte(':');
				ostr(columnString);
			} break;
		}
	}

	switch (G->CurrentMode) {
		case COMMAND_MODE: {
			setcur(G->ScreenRows - 1, (ws_col) (1 + G->CommandLineLength));

			break;
		}

		default: {
			visualRow = 0;

			for (lineIndex = G->TopLineIndex; lineIndex < G->CursorRow; lineIndex++)
				visualRow += linelay(lineIndex, (size_t) -1, 0, 0);

			linelay(G->CursorRow, G->CursorColumn, &rowInLine, &columnInLine);
			visualRow += rowInLine;
			setcur((ws_row) visualRow, (ws_col) columnInLine);

			break;
		}
	}

	seqout((const unsigned byte *) "?7h");
	seqout((const unsigned byte *) "?25h");

	oflush();
}

static ABI INLINE size_t offsetcur(void) {
	static size_t offset;
	static size_t totalLength;

	offset = pt_line_offset(G->CursorRow) + G->CursorColumn;
	totalLength = (G->Root ? G->Root->SubtreeLength : 0);

	return offset > totalLength ? totalLength : offset;
}

static ABI void insbuf(offset, c, line)
size_t offset;
unsigned dword c;
size_t line;
{
	static unsigned byte structural;
	static size_t rowsAfter, rowsBefore;

	structural = (c == '\n');
	rowsBefore = structural ? 0 : linelay(line, (size_t) -1, 0, 0);

	undo_begin_edit();
	pt_insertb(offset, (const unsigned byte *) &c, 1);

	G->Dirty = true;

	if (!structural) {
		rowsAfter = linelay(line, (size_t) -1, 0, 0);
		if (rowsAfter != rowsBefore) structural = true;
	}

	mark_dirty(structural ? DIRTY_FROM : DIRTY_LINE, line);
}

static ABI void delbuf(offset, line)
size_t offset, line;
{
	static unsigned byte structural;
	static size_t rowsAfter, rowsBefore;

	if (offset >= (G->Root ? G->Root->SubtreeLength : 0)) return;

	structural = (pt_character_at(offset) == '\n');
	rowsBefore = structural ? 0 : linelay(line, (size_t) -1, 0, 0);

	undo_begin_edit();
	pt_delete_range(offset, 1);

	G->Dirty = true;

	if (!structural) {
		rowsAfter = linelay(line, (size_t) -1, 0, 0);
		if (rowsAfter != rowsBefore)
			structural = true;
	}

	mark_dirty(structural ? DIRTY_FROM : DIRTY_LINE, line);
}

static ABI void delete_char_at_cursor(void) {
	static size_t characterLength, remainingBytes, startOffset, len, i;

	len = linelen(G->CursorRow);
	if (len > 0 && G->CursorColumn < len) {
		startOffset = pt_line_offset(G->CursorRow);
		remainingBytes = len - G->CursorColumn;
		characterLength = utf8clen(pt_character_at(startOffset + G->CursorColumn));

		if (characterLength > remainingBytes)
			characterLength = remainingBytes;

		for (i = 0; i < characterLength; i++)
			delbuf(offsetcur(), G->CursorRow);
	}

	G->Undo = false;
}

static ABI void adjscr(void) {
	static ws_row screenRows;
	static size_t lineIndex, rowInLine, visualRows;

	screenRows = G->ScreenRows - 1;

	if (G->CursorRow < G->TopLineIndex)
		G->TopLineIndex = G->CursorRow;

	visualRows = 0;
	for (lineIndex = G->TopLineIndex; lineIndex < G->CursorRow; lineIndex++)
		visualRows += linelay(lineIndex, (size_t) -1, 0, 0);

	linelay(G->CursorRow, G->CursorColumn, &rowInLine, (size_t *) NULL);
	visualRows += rowInLine + 1;

	while (visualRows > screenRows && G->TopLineIndex < G->CursorRow) {
		visualRows -= linelay(G->TopLineIndex, (size_t) -1, 0, 0);

		G->TopLineIndex++;
	}
}

static ABI void movcur(direction)
signed dword direction;
{
	static size_t startOffset, len;

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
				if (G->CurrentMode == INSERT_MODE)
					G->CursorColumn = len;
				else {
					G->CursorColumn = len - 1;
					while (G->CursorColumn > 0 && (pt_character_at(startOffset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
				}
			} else G->CursorColumn = 0;

			break;
		}

		case 0x06: {
			G->CursorRow += (G->ScreenRows - 1) / 2;
			if (G->CursorRow >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) && ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0)
				G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;

			break;
		}

		case 0x02: {
			if (G->CursorRow >= (size_t) (G->ScreenRows - 1) / 2)
				G->CursorRow -= (G->ScreenRows - 1) / 2;
			else G->CursorRow = 0;

			break;
		}

		default: break;
	}
}
