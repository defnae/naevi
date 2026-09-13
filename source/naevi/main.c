// source/naevi/main.c

#define WNONBUILTINS

#include <headers/typ.h>
#include <headers/def.h>
#include <headers/str.h>

#include <naevi/headers/pt.h>
#include <naevi/headers/posix.h>

#define TAB_WIDTH 8
#define MAX_HISTORY 4096

#define NORMAL_MODE 0
#define INSERT_MODE 1
#define COMMAND_MODE 2
#define SEARCH_MODE 3
#define REPLACE_MODE 4

#define LINE_DIGITS_MIN 4
#define LINE_SEP_WIDTH 3

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
#define NORETURN __attribute__((noreturn))

#define noaof register

typedef struct {
	PN *Root;
	PN **UndoStack, **RedoStack;

	size_t *Newlines;
	size_t *MatchOffsets;
	unsigned byte *AddBuffer, *Buffer;

	size_t AddCapacity, AddLength, CommandLineLength, CursorColumn, CursorRow, DirtyLine, Length, MatchCount, MatchIndex, MatchLength, NewlineCount, OriginalMatchTotal, OutputLength, RedoCapacity, RedoCount, RenderedTopLine, ReplacedThisSession, ReplaceFindLength, ReplaceWithLength, SearchLength, SearchLineLength, StatusLength, TopLineIndex, UndoCapacity, UndoCount;
	signed dword MatchOffsetDelta;
	unsigned dword RNGState;

	ws_row ScreenRows;
	ws_col ScreenColumns;

	struct termios Termios;
	unsigned byte Character, CurrentMode, Dirty, DirtyKind, Force, Lines, Lead, MatchActive, MatchCaseSensitive, MatchEscArmed, MatchFromReplace, MatchWholeWord, Pushback, ReplaceLeaderPending, ReplaceStage, ReplacingInProgress, Running, Undo;

	unsigned byte CommandLineBuffer[4096], Filename[4096], ReplaceFindBuffer[4096], ReplaceWithBuffer[4096], SearchBuffer[4096], SearchLineBuffer[4096], StatusBuffer[4096];
	unsigned byte OutputBuffer[1024 * 64], ScratchLineBuffer[1024 * 64], ScratchSaveBuffer[1024 * 64];

	unsigned byte padding[6];
} Globals;

static Globals GlobalData;
static Globals *G = &GlobalData;

static sig_atomic_t resize_pending;

void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);

raise(signed dword);
system(byte *);

void exit(signed dword);

static ABI unsigned byte ready(signed dword);
static ABI unsigned byte *ulltod(unsigned qword, unsigned byte *);

static ABI void on_winch(int);
static ABI NORETURN void on_sigterm(int);
static ABI void on_sigint(int);

static ABI INLINE unsigned dword rng_next(void);

static ABI PN *pn_create(unsigned dword, size_t, size_t, size_t, PN *, PN *, unsigned dword);
static ABI INLINE PN *pn_retain(PN *);

static ABI void pn_release(PN *);
static ABI PN *pn_merge(PN *, PN *);
static ABI void pn_split(PN *, size_t, SP *);

static ABI size_t newline_lower_bound(size_t);
static ABI size_t count_newlines(unsigned dword, size_t, size_t);

static ABI PN *pt_locate(size_t, size_t *, size_t *);
static ABI unsigned byte pt_character_at(size_t);

static ABI size_t pt_extract(size_t, size_t, unsigned byte *, size_t);

static ABI size_t pt_line_offset(size_t);
static ABI void pt_line_bounds(size_t, size_t *, size_t *);
static ABI void pt_insert_bytes(size_t, unsigned byte *, size_t);
static ABI void pt_delete_range(size_t, size_t);

static ABI void push_undo(PN *);

static ABI void push_redo(PN *);

static ABI void undo_begin_edit(void);
static ABI void fix_cursor(void);
static ABI void undo(void);
static ABI void redo(void);

static ABI size_t line_length(size_t);
static ABI size_t line_layout(size_t, size_t, size_t *, size_t *);
static ABI INLINE size_t utf8_char_length(unsigned dword);

static ABI void clamp_column(void);
static ABI void out_flush(void);
static ABI void out_bytes(unsigned byte *, size_t);
static ABI void out_str(byte *);
static ABI INLINE void out_byte(unsigned dword);

static ABI void seq_out(byte *);
static ABI void set_cursor(unsigned dword, unsigned dword);
static ABI void status(byte *);
static ABI unsigned byte save(void);
static ABI void render(void);
static ABI INLINE size_t cursor_offset(void);

static ABI void insert_char(size_t, unsigned dword, size_t);
static ABI void delete_char(size_t, size_t);
static ABI void delete_char_at_cursor(void);
static ABI void adjust_scroll(void);
static ABI void move_cursor(signed dword);

static ABI void mark_dirty(unsigned dword, size_t);
static ABI size_t visual_row_of(size_t);
static ABI ws_row draw_line(size_t, unsigned dword, unsigned dword, size_t);

static ABI size_t lines_width(void);
static ABI void draw_lines(size_t);

static ABI unsigned byte do_search(unsigned byte *, size_t, signed dword);

static ABI unsigned byte bytes_equal(unsigned byte *, unsigned byte *, size_t, signed dword);
static ABI unsigned byte is_word_byte(signed dword);
static ABI void offset_to_cursor(size_t, size_t *, size_t *);
static ABI void free_match_set(void);
static ABI size_t find_all_matches(unsigned byte *, size_t);
static ABI void goto_match(size_t);
static ABI void status_match_progress(byte *, size_t, size_t);
static ABI unsigned char match_step(signed dword);
static ABI void replace_current_match(void);
static ABI void replace_all_remaining(void);

main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	static struct sigaction sa;
	static struct termios settings;
	static struct winsize window_size;

	static off_t file_size;
	static unsigned byte drain_char, key_code, next_char, raw_input, tilde_char;

	static unsigned byte stat_buffer[STAT_BUFFER_SIZE];

	noaof ssize_t read_count;
	noaof tcflag_t terminal_mask;
	noaof signed dword fd, input_char;
	noaof unsigned byte is_merging_up, is_nav_key;
	noaof size_t char_length, chars_to_delete, clear_idx, cmd_clear_idx, delete_idx, end_offset, join_column, len, name_length, offset, old_column, path_length, start_offset, total_read;

	noaof unsigned byte *cmd_string, *file_path;

	(void) envp;

	G->ScreenRows = 24;
	G->ScreenColumns = 80;
	G->CurrentMode = NORMAL_MODE;
	G->RNGState = 0x9E3779B9U;
	G->Running = true;
	G->Lines = true;

	if (ioctl(1, TIOCGWINSZ, &window_size) == 0) {
		if (window_size.ws_row > 0)
			G->ScreenRows = window_size.ws_row;
		if (window_size.ws_col > 0)
			G->ScreenColumns = window_size.ws_col;
	}

	sa.sa_handler = on_winch;
	sigemptyset(&sa.sa_mask);

	sa.sa_flags = 0;
	sigaction(SIGWINCH, &sa, (struct sigaction *) NULL);

	signal(SIGTERM, on_sigterm);
	signal(SIGINT, on_sigint);

	if (argc < 2) G->Root = 0;
	else {
		file_path = (unsigned byte *) argv[1];
		path_length = strlen((byte *) file_path);

		if (path_length >= sizeof(G->Filename))
			path_length = sizeof(G->Filename) - 1;

		for (clear_idx = 0; clear_idx < sizeof(G->Filename); clear_idx++)
			G->Filename[clear_idx] = 0;

		memcpy(G->Filename, file_path, path_length);

		fd = open(argv[1], O_RDONLY, 0);
		if (fd < 0) G->Root = 0;
		else {
			if (fstat(fd, (struct stat *) stat_buffer) != 0) file_size = -1;
			else memcpy(&file_size, (stat_buffer + STAT_SIZE_OFFSET), sizeof(file_size));

			if (file_size > 0) {
				total_read = 0;

				G->Buffer = (unsigned byte *) malloc((size_t) file_size);
				if (!G->Buffer) {
					close(fd);

					if (G->Newlines) free(G->Newlines);

					exit(137);
				}

				while (total_read < (size_t) file_size) {
					read_count = read(fd, G->Buffer + total_read, (size_t) file_size - total_read);
					if (read_count <= 0) break;

					total_read += (size_t) read_count;
				}

				G->Length = total_read;
				{
					noaof size_t count_init, i_init;
					count_init = 0;
					for (i_init = 0; i_init < G->Length; i_init++)
						if (G->Buffer[i_init] == '\n') count_init++;

					G->Newlines = count_init ? (size_t *) malloc(count_init * sizeof(size_t)) : 0;
					if (count_init && !G->Newlines) {
						G->NewlineCount = 0;
					} else {
						G->NewlineCount = 0;
						for (i_init = 0; i_init < G->Length; i_init++)
							if (G->Buffer[i_init] == '\n') G->Newlines[G->NewlineCount++] = i_init;
					}
				}

				G->Root = (G->Length > 0) ? pn_create(PS_ORIGIN, 0, G->Length, G->NewlineCount, 0, 0, rng_next()) : 0;
			} else G->Root = 0;

			close(fd);

			G->Dirty = false;
		}
	}

	tcgetattr(0, &G->Termios);

	memcpy(&settings, &G->Termios, sizeof(settings));

	terminal_mask = ICANON | ECHO | ISIG;

	settings.c_lflag &= ~terminal_mask;
	settings.c_cc[VMIN] = 1;
	settings.c_cc[VTIME] = 0;

	tcsetattr(0, TCSANOW, &settings);

	seq_out("2J");

	G->DirtyKind = DIRTY_FULL;
	render();

	while (G->Running) {
		if (G->Pushback) {
			input_char = G->Character;

			G->Pushback = false;
		} else {
			read_count = read(0, &raw_input, 1);
			if (read_count <= 0) {
				if (resize_pending) {
					resize_pending = false;

					if (ioctl(1, TIOCGWINSZ, &window_size) == 0) {
						if (window_size.ws_row > 0)
							G->ScreenRows = window_size.ws_row;
						if (window_size.ws_col > 0)
							G->ScreenColumns = window_size.ws_col;
					}

					adjust_scroll();

					G->DirtyKind = DIRTY_FULL;
					render();

					continue;
				}

				break;
			}

			input_char = raw_input;
		}

		if (input_char == 0x03) {
			raise(SIGINT);

			render();
			continue;
		}

		is_nav_key = false;

		if (input_char == 0x1B && ready(20)) {

			read_count = read(0, &next_char, 1);
			if (read_count > 0 && next_char == '[' && ready(20)) {
				read_count = read(0, &key_code, 1);
				if (read_count > 0) {
					switch (key_code) {
						case 'A': input_char = 'k'; is_nav_key = true; break;
						case 'B': input_char = 'j'; is_nav_key = true; break;
						case 'C': input_char = 'l'; is_nav_key = true; break;
						case 'D': input_char = 'h'; is_nav_key = true; break;
						case 'H': input_char = '0'; is_nav_key = true; break;
						case 'F': input_char = '$'; is_nav_key = true; break;

						case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': {
							if (ready(20)) {
								read_count = read(0, &tilde_char, 1);
								if (read_count > 0) {
									if (tilde_char == '~') {
										switch (key_code) {
											case '1': case '7': input_char = '0'; is_nav_key = true; break;
											case '4': case '8': input_char = '$'; is_nav_key = true; break;
											case '2': input_char = INSERT_KEY; is_nav_key = true; break;
											case '3': input_char = 'x'; is_nav_key = true; break;
											case '5': input_char = 0x02; is_nav_key = true; break;
											case '6': input_char = 0x06; is_nav_key = true; break;

											default: break;
										}
									} else {
										drain_char = tilde_char;
										while ((drain_char < 0x40 || drain_char > 0x7E) && ready(20))
											if (read(0, &drain_char, 1) <= 0) break;
									}
								}
							} break;
						}

						default: break;
					}
				} else {
					G->Character = next_char;
					G->Pushback = true;

					input_char = 0x1B;
				}
			}

			if (is_nav_key) {
				if (input_char == INSERT_KEY) {
					if (G->CurrentMode == NORMAL_MODE || G->CurrentMode == INSERT_MODE) {
						if (G->CurrentMode == INSERT_MODE) {
							if (G->CursorColumn > 0) {
								start_offset = pt_line_offset(G->CursorRow);

								G->CursorColumn--;
								while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80)
									G->CursorColumn--;
							}

							G->CurrentMode = NORMAL_MODE;
							G->Undo = false;
						} else G->CurrentMode = INSERT_MODE;

						clamp_column();
						adjust_scroll();
					}
				} else {
					switch (G->CurrentMode) {
						case NORMAL_MODE: case INSERT_MODE: {
							if (input_char == 'x') delete_char_at_cursor();
							else if (G->CurrentMode == NORMAL_MODE && input_char == 'l' && match_step(1)) { }
							else if (G->CurrentMode == NORMAL_MODE && input_char == 'j' && match_step(1)) { }
							else if (G->CurrentMode == NORMAL_MODE && input_char == 'h' && match_step(-1)) { }
							else if (G->CurrentMode == NORMAL_MODE && input_char == 'k' && match_step(-1)) { }
							else move_cursor(input_char);

							clamp_column();
							adjust_scroll();

							break;
						}

						default: break;
					}
				}

				render();

				continue;
			}
		}

		if (input_char == 0x1B) {
				switch (G->CurrentMode) {
					case INSERT_MODE: {
						if (G->CursorColumn > 0) {
							start_offset = pt_line_offset(G->CursorRow);

							G->CursorColumn--;
							while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80)
								G->CursorColumn--;
						}

						G->CurrentMode = NORMAL_MODE;
						G->Undo = false;

						switch (G->CurrentMode) {
							case NORMAL_MODE: clamp_column(); break;

							default: break;
						}

						adjust_scroll();

						break;
					}

					case COMMAND_MODE: {
						G->CurrentMode = NORMAL_MODE;
						G->CommandLineLength = 0;

						break;
					}

					case SEARCH_MODE: {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;
							break;
						}

						G->CurrentMode = NORMAL_MODE;
						G->SearchLength = 0;
						free_match_set();

						break;
					}

					case REPLACE_MODE: {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;
							break;
						}

						G->CurrentMode = NORMAL_MODE;
						G->ReplaceStage = 0;
						G->ReplaceFindLength = 0;
						G->ReplaceWithLength = 0;
						free_match_set();

						break;
					}

					case NORMAL_MODE: {
						if (G->MatchActive && G->MatchFromReplace) {
							if (G->MatchEscArmed) {
								free_match_set();
								G->MatchFromReplace = false;
								G->MatchEscArmed = false;
								G->Undo = false;
							} else {
								G->MatchEscArmed = true;
							}
						} else if (G->MatchActive) {
							free_match_set();
						}

						break;
					}

					default: break;
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

						if (input_char != 'd') break;

						if (G->CursorRow < ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) {
							start_offset = pt_line_offset(G->CursorRow);
							end_offset = (G->CursorRow + 1 < ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) ? pt_line_offset(G->CursorRow + 1) : (G->Root ? G->Root->SubtreeLength : 0);
							len = end_offset - start_offset;

							if (len > 0) {
								undo_begin_edit();
								pt_delete_range(start_offset, len);
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

						if (input_char == 'g') {
							G->CursorRow = 0;
							G->CursorColumn = 0;
							G->TopLineIndex = 0;
						} break;
					}

					default: {
						switch (input_char) {
							case 'h': if (!match_step(-1)) move_cursor('h'); break;
							case 'l': if (!match_step(1)) move_cursor('l'); break;
							case 'j': if (!match_step(1)) move_cursor('j'); break;
							case 'k': if (!match_step(-1)) move_cursor('k'); break;
							case '0': move_cursor('0'); break;
							case '$': move_cursor('$'); break;

							case '\n': case '\r': {
								if (G->MatchActive && G->MatchFromReplace)
									replace_current_match();

								break;
							}

							case 'G': if (((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) > 0) {
								G->CursorRow = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;
							} break;

							case 'g': G->Lead = 'g'; break;
							case 'i': G->CurrentMode = INSERT_MODE; break;

							case 'a': {
								if (G->MatchActive && G->MatchFromReplace) {
									replace_all_remaining();
									break;
								}

								len = line_length(G->CursorRow);
								G->CurrentMode = INSERT_MODE;

								if (len > 0) {
									start_offset = pt_line_offset(G->CursorRow);
									char_length = utf8_char_length(pt_character_at(start_offset + G->CursorColumn));
									G->CursorColumn += char_length;
								} break;
							}

							case 'A': G->CurrentMode = INSERT_MODE; G->CursorColumn = line_length(G->CursorRow); break;
							case 'I': G->CursorColumn = 0; G->CurrentMode = INSERT_MODE; break;

							case 'o': {
								offset = pt_line_offset(G->CursorRow) + line_length(G->CursorRow);
								if (offset < (G->Root ? G->Root->SubtreeLength : 0) && pt_character_at(offset) == '\n')
									offset++;

								insert_char(offset, '\n', G->CursorRow);

								G->CursorRow++;
								G->CursorColumn = 0;
								G->CurrentMode = INSERT_MODE;

								break;
							}

							case 'O': {
								insert_char(pt_line_offset(G->CursorRow), '\n', G->CursorRow);

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

								for (cmd_clear_idx = 0; cmd_clear_idx < sizeof(G->CommandLineBuffer); cmd_clear_idx++)
									G->CommandLineBuffer[cmd_clear_idx] = 0;

								break;
							}

							case '/': {
								G->CurrentMode = SEARCH_MODE;
								G->SearchLength = 0;

								for (cmd_clear_idx = 0; cmd_clear_idx < sizeof(G->SearchBuffer); cmd_clear_idx++)
									G->SearchBuffer[cmd_clear_idx] = 0;

								break;
							}

							case 0x0B: {
								G->CurrentMode = REPLACE_MODE;
								G->ReplaceStage = 0;
								G->ReplaceFindLength = 0;
								G->ReplaceWithLength = 0;
								G->ReplaceLeaderPending = false;

								for (cmd_clear_idx = 0; cmd_clear_idx < sizeof(G->ReplaceFindBuffer); cmd_clear_idx++)
									G->ReplaceFindBuffer[cmd_clear_idx] = 0;

								for (cmd_clear_idx = 0; cmd_clear_idx < sizeof(G->ReplaceWithBuffer); cmd_clear_idx++)
									G->ReplaceWithBuffer[cmd_clear_idx] = 0;

								break;
							}

							case 'n': {
								if (!match_step(1) && G->SearchLineLength) {
									if (!do_search(G->SearchLineBuffer, G->SearchLineLength, 1))
										status("Pattern not found.");
								} break;
							}

							case 'N': {
								if (!match_step(-1) && G->SearchLineLength) {
									if (!do_search(G->SearchLineBuffer, G->SearchLineLength, -1))
										status("Pattern not found.");
								} break;
							}

							case 0x06: move_cursor(0x06); break;
							case 0x02: move_cursor(0x02); break;

							default: break;
						} break;
					}
				}

				switch (G->CurrentMode) {
					case NORMAL_MODE: clamp_column(); break;

					default: break;
				}

				adjust_scroll();

				break;
			}

			case INSERT_MODE: {
				switch (input_char) {
					case 0x1B: {
						if (G->CursorColumn > 0) {
							start_offset = pt_line_offset(G->CursorRow);

							G->CursorColumn--;
							while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80)
								G->CursorColumn--;
						}

						G->CurrentMode = NORMAL_MODE;
						G->Undo = false;

						break;
					}

					case 127: case 8: {
						if (cursor_offset() > 0) {
							is_merging_up = (G->CursorColumn == 0 && G->CursorRow > 0);
							join_column = is_merging_up ? line_length(G->CursorRow - 1) : 0;

							if (is_merging_up) {
								delete_char(cursor_offset() - 1, G->CursorRow - 1);

								G->CursorRow--;
								G->CursorColumn = join_column;
							} else if (G->CursorColumn > 0) {
								start_offset = pt_line_offset(G->CursorRow);
								old_column = G->CursorColumn;

								G->CursorColumn--;
								while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80)
									G->CursorColumn--;

								chars_to_delete = old_column - G->CursorColumn;
								for (delete_idx = 0; delete_idx < chars_to_delete; delete_idx++)
									delete_char(start_offset + G->CursorColumn, G->CursorRow);
							}
						} break;
					}

					case '\n': case '\r': {
						insert_char(cursor_offset(), '\n', G->CursorRow);

						G->CursorRow++;
						G->CursorColumn = 0;

						break;
					}

					case '\t': {
						insert_char(cursor_offset(), '\t', G->CursorRow);
						G->CursorColumn++;

						break;
					}

					default: {
						if (input_char >= 32 && input_char < 0x100) {
							insert_char(cursor_offset(), (unsigned byte) input_char, G->CursorRow);
							G->CursorColumn++;
						} break;
					}
				}

				switch (G->CurrentMode) {
					case NORMAL_MODE: clamp_column(); break;

					default: break;
				}

				adjust_scroll();

				break;
			}

			case COMMAND_MODE: {
				switch (input_char) {
					case 0x1B: {
						G->CurrentMode = NORMAL_MODE;
						G->CommandLineLength = 0;

						break;
					}

					case '\n': case '\r': {
						G->CommandLineBuffer[G->CommandLineLength] = '\0';
						cmd_string = G->CommandLineBuffer;

						switch (cmd_string[0]) {
							case 'q': {
								switch (cmd_string[1]) {
									case '\0': {
										if (G->Dirty && !G->Force) {
											status("Unsaved changes, :q! to force.");
										} else {
											G->Running = false;
										} break;
									}

									case '!': {
										switch (cmd_string[2]) {
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
								switch (cmd_string[1]) {
									case '\0': case '!': {
										switch (cmd_string[cmd_string[1] == '!' ? 2 : 1]) {
											case '\0': {
												if (!save()) break;
												break;
											}

											default: status("Unknown command."); break;
										} break;
									}

									case 'q': {
										switch (cmd_string[2]) {
											case '\0': {
												if (!save()) break;

												G->Running = false; break;
											}

											case '!': {
												switch (cmd_string[3]) {
													case '\0': {
														if (!save()) break;

														G->Running = false; break;
													}

													default: status("Unknown command."); break;
												} break;
											}

											case ' ': {
												switch (cmd_string[3]) {
													case '\0': status("Unknown command."); break;

													default: {
														static unsigned byte old_filename_backup[4096];
														size_t b_idx;
														for (b_idx = 0; b_idx < sizeof(G->Filename); b_idx++) old_filename_backup[b_idx] = G->Filename[b_idx];

														name_length = strlen((byte *) (cmd_string + 3));

														if (name_length >= sizeof(G->Filename))
															name_length = sizeof(G->Filename) - 1;

														memcpy(G->Filename, cmd_string + 3, name_length);
														G->Filename[name_length] = '\0';

														if (!save()) {
															for (b_idx = 0; b_idx < sizeof(G->Filename); b_idx++) G->Filename[b_idx] = old_filename_backup[b_idx];
															break;
														}

														G->Running = false;

														break;
													}
												} break;
											}

											default: status("Unknown command."); break;
										} break;
									}

									case ' ': {
										switch (cmd_string[2]) {
											case '\0': status("Unknown command."); break;

											default: {
												static unsigned byte old_filename_backup[4096];

												size_t b_idx;
												for (b_idx = 0; b_idx < sizeof(G->Filename); b_idx++) old_filename_backup[b_idx] = G->Filename[b_idx];

												name_length = strlen((byte *) (cmd_string + 2));

												if (name_length >= sizeof(G->Filename))
													name_length = sizeof(G->Filename) - 1;

												memcpy(G->Filename, cmd_string + 2, name_length);
												G->Filename[name_length] = '\0';

												if (!save()) {
													for (b_idx = 0; b_idx < sizeof(G->Filename); b_idx++) G->Filename[b_idx] = old_filename_backup[b_idx];
													break;
												}

												break;
											}
										} break;
									}

									default: status("Unknown command."); break;
								} break;
							}

							case 'x': {
								switch (cmd_string[1]) {
									case '\0': {
										if (!save()) break;

										G->Running = false; break;
									}

									case '!': {
										switch (cmd_string[2]) {
											case '\0': {
												if (!save()) break;

												G->Running = false; break;
											}

											default: status("Unknown command."); break;
										} break;
									}

									default: status("Unknown command."); break;
								} break;
							}

							case '!': {
								tcsetattr(0, TCSANOW, &G->Termios);

								seq_out("2J");
								set_cursor(0, 0);
								out_flush();

								system((byte *) (cmd_string + 1));

								out_str("\r\nPress any key to continue...");
								out_flush();

								read(0, &drain_char, 1);

								tcsetattr(0, TCSANOW, &settings);

								G->DirtyKind = DIRTY_FULL;

								break;
							}

							case 'N': {
								switch (cmd_string[1]) {
									case '\0': {
										G->Lines = !G->Lines;

										mark_dirty(DIRTY_FULL, 0);

										break;
									}

									default: status("Unknown command."); break;
								} break;
							}

							default: {
								if (cmd_string[0] >= '0' && cmd_string[0] <= '9') {
									noaof size_t target_line, digit_idx;

									target_line = 0;
									digit_idx = 0;

									while (cmd_string[digit_idx] >= '0' && cmd_string[digit_idx] <= '9') {
										target_line = target_line * 10 + (size_t) (cmd_string[digit_idx] - '0');
										digit_idx++;
									}

									if (cmd_string[digit_idx] != '\0') {
										status("Unknown command.");
									} else {
										if (target_line > 0) target_line--;

										if (target_line >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1))
											target_line = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1) - 1;

										G->CursorRow = target_line;
										G->CursorColumn = 0;

										clamp_column();
										adjust_scroll();
									}
								} else status("Unknown command.");

								break;
							}
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
						if (input_char >= 32 && input_char < 127 && G->CommandLineLength + 1 < sizeof(G->CommandLineBuffer))
							G->CommandLineBuffer[G->CommandLineLength++] = (unsigned byte) input_char;

						break;
					}
				} break;
			}

			case SEARCH_MODE: {
				switch (input_char) {
					case '\n': case '\r': {
						if (G->ReplaceLeaderPending) G->ReplaceLeaderPending = false;

						if (G->SearchLength) {
							memcpy(G->SearchLineBuffer, G->SearchBuffer, G->SearchLength);
							G->SearchLineLength = G->SearchLength;

							if (find_all_matches(G->SearchLineBuffer, G->SearchLineLength) > 0) {
								G->MatchFromReplace = false;
								goto_match(0);
								status_match_progress("Matches", 1, G->MatchCount);
							} else {
								status("Pattern not found.");
							}

							clamp_column();
							adjust_scroll();
						}

						G->CurrentMode = NORMAL_MODE;
						G->SearchLength = 0;

						break;
					}

					case 127: case 8: {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;
							break;
						}

						if (G->SearchLength > 0)
							G->SearchLength--;

						break;
					}

					case '~': {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;

							if (G->SearchLength + 1 < sizeof(G->SearchBuffer))
								G->SearchBuffer[G->SearchLength++] = '~';
						} else {
							G->ReplaceLeaderPending = true;
						}

						break;
					}

					default: {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;

							if (input_char == 'c') {
								G->MatchCaseSensitive = !G->MatchCaseSensitive;
								break;
							} else if (input_char == 'w') {
								G->MatchWholeWord = !G->MatchWholeWord;
								break;
							}

							if (input_char >= 32 && input_char < 0x100 && G->SearchLength + 1 < sizeof(G->SearchBuffer))
								G->SearchBuffer[G->SearchLength++] = '~';
						}

						if (input_char >= 32 && input_char < 0x100 && G->SearchLength + 1 < sizeof(G->SearchBuffer))
							G->SearchBuffer[G->SearchLength++] = (unsigned byte) input_char;

						break;
					}
				} break;
			}

			case REPLACE_MODE: {
				switch (input_char) {
					case '\n': case '\r': {
						if (G->ReplaceLeaderPending) G->ReplaceLeaderPending = false;

						if (G->ReplaceStage == 0) {
							G->ReplaceStage = 1;
							break;
						}

						if (G->ReplaceFindLength) {
							if (find_all_matches(G->ReplaceFindBuffer, G->ReplaceFindLength) > 0) {
								G->MatchFromReplace = true;
								G->OriginalMatchTotal = G->MatchCount;
								G->ReplacedThisSession = 0;

								goto_match(0);
								status_match_progress("Matches", 1, G->MatchCount);
							} else {
								status("Pattern not found.");
							}

							clamp_column();
							adjust_scroll();
						}

						G->CurrentMode = NORMAL_MODE;
						G->ReplaceStage = 0;

						break;
					}

					case 127: case 8: {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;
							break;
						}

						if (G->ReplaceStage == 0) {
							if (G->ReplaceFindLength > 0)
								G->ReplaceFindLength--;
						} else {
							if (G->ReplaceWithLength > 0)
								G->ReplaceWithLength--;
						}

						break;
					}

					case '~': {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;

							if (G->ReplaceStage == 0) {
								if (G->ReplaceFindLength + 1 < sizeof(G->ReplaceFindBuffer))
									G->ReplaceFindBuffer[G->ReplaceFindLength++] = '~';
							} else {
								if (G->ReplaceWithLength + 1 < sizeof(G->ReplaceWithBuffer))
									G->ReplaceWithBuffer[G->ReplaceWithLength++] = '~';
							}
						} else {
							G->ReplaceLeaderPending = true;
						}

						break;
					}

					default: {
						if (G->ReplaceLeaderPending) {
							G->ReplaceLeaderPending = false;

							if (input_char == 'c') {
								G->MatchCaseSensitive = !G->MatchCaseSensitive;
								break;
							} else if (input_char == 'w') {
								G->MatchWholeWord = !G->MatchWholeWord;
								break;
							}

							if (input_char >= 32 && input_char < 0x100) {
								if (G->ReplaceStage == 0) {
									if (G->ReplaceFindLength + 1 < sizeof(G->ReplaceFindBuffer))
										G->ReplaceFindBuffer[G->ReplaceFindLength++] = '~';
								} else {
									if (G->ReplaceWithLength + 1 < sizeof(G->ReplaceWithBuffer))
										G->ReplaceWithBuffer[G->ReplaceWithLength++] = '~';
								}
							}
						}

						if (input_char >= 32 && input_char < 0x100) {
							if (G->ReplaceStage == 0) {
								if (G->ReplaceFindLength + 1 < sizeof(G->ReplaceFindBuffer))
									G->ReplaceFindBuffer[G->ReplaceFindLength++] = (unsigned byte) input_char;
							} else {
								if (G->ReplaceWithLength + 1 < sizeof(G->ReplaceWithBuffer))
									G->ReplaceWithBuffer[G->ReplaceWithLength++] = (unsigned byte) input_char;
							}
						}

						break;
					}
				} break;
			}

			default: break;
		}

		render();
	}

	tcsetattr(0, TCSANOW, &G->Termios);

	seq_out("?7h");
	seq_out("2J");

	set_cursor(0, 0);
	out_flush();

	if (G->Buffer) free(G->Buffer);
	if (G->Newlines) free(G->Newlines);

	exit(0);

	/* return 0; */
}

static ABI void on_winch(signum)
int signum;
{
    (void) signum;

	resize_pending = true;

	return;
}

static NORETURN ABI void on_sigterm(signum)
int signum;
{
    static unsigned byte save_filename[4104];

	noaof signed dword fd;
	noaof ssize_t write_result;
	noaof size_t chunk_written, current_pos, extracted, name_length, requested, total_length;

	(void) signum;

	if (G->Filename[0]) {
		name_length = strlen((byte *) G->Filename);
		if (name_length > sizeof(save_filename) - 6)
			name_length = sizeof(save_filename) - 6;

		memcpy(save_filename, G->Filename, name_length);
		memcpy(save_filename + name_length, ".save", 5);
		name_length += 5;
	} else {
		memcpy(save_filename, "naevi.save", 10);
		name_length = 10;
	}

	save_filename[name_length] = '\0';

	fd = open((byte *) save_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		total_length = (G->Root ? G->Root->SubtreeLength : 0);
		current_pos = 0;

		while (current_pos < total_length) {
			requested = total_length - current_pos;
			chunk_written = 0;

			if (requested > sizeof(G->ScratchSaveBuffer))
				requested = sizeof(G->ScratchSaveBuffer);

			extracted = pt_extract(current_pos, requested, G->ScratchSaveBuffer, sizeof(G->ScratchSaveBuffer));
			if (extracted == 0) break;

			while (chunk_written < extracted) {
				write_result = write(fd, G->ScratchSaveBuffer + chunk_written, extracted - chunk_written);
				if (write_result <= 0) break;

				chunk_written += (size_t) write_result;
			}

			current_pos += extracted;

			if (chunk_written < extracted) break;
		}

		close(fd);
	}

	exit(143);

	/* return; */
}

static void on_sigint(signum)
int signum;
{
    (void) signum;
	if (G->ReplacingInProgress) {
		G->ReplacingInProgress = false;
		return;
	}

	G->CurrentMode = NORMAL_MODE;

	return;
}

static ABI unsigned byte ready(ms)
signed dword ms;
{
	static struct pollfd poll_fd;

	poll_fd.fd = 0;
	poll_fd.events = POLLIN;
	poll_fd.revents = 0;

	return poll(&poll_fd, 1, ms) > 0;
}

static ABI unsigned byte *ulltod(ull, d)
unsigned qword ull;
unsigned byte *d;
{
	noaof unsigned byte *p;

	p = d + 20;

	*p = '\0';
	if (ull == 0) {
		*--p = '0';

		return p;
	}

	while (ull > 0) {
		*--p = (unsigned byte) ('0' + (ull % 10));

		ull /= 10;
	}

	return p;
}

static ABI INLINE unsigned dword rng_next(void)
{
	noaof unsigned dword state;

	state = G->RNGState;
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	G->RNGState = state;

	return state;
}

static ABI PN *pn_create(Source, start_offset, len, line_feeds, Left, Right, Priority)
unsigned dword Source;
size_t start_offset, len, line_feeds;
PN *Left, *Right;
unsigned dword Priority;
{
	noaof PN *Node;

	Node = (PN *) malloc(sizeof(PN));
	if (!Node) return (PN *) NULL;

	Node->Source = Source;
	Node->StartOffset = start_offset;
	Node->Length = len;
	Node->LineFeeds = line_feeds;
	Node->LeftChild = Left;
	Node->RightChild = Right;
	Node->Priority = Priority;
	Node->ReferenceCount = 1;

	Node->SubtreeLength = len + (Left ? Left->SubtreeLength : 0) + (Right ? Right->SubtreeLength : 0);
	Node->SubtreeLineFeeds = line_feeds + (Left ? Left->SubtreeLineFeeds : 0) + (Right ? Right->SubtreeLineFeeds : 0);

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

	return;
}

static ABI PN *pn_merge(Left, Right)
PN *Left, *Right;
{
	noaof PN *new_right, *new_left;

	if (!Left) return pn_retain(Right);
	if (!Right) return pn_retain(Left);

	if (Left->Priority > Right->Priority) {
		new_right = pn_merge(Left->RightChild, Right);

		return pn_create(Left->Source, Left->StartOffset, Left->Length, Left->LineFeeds, pn_retain(Left->LeftChild), new_right, Left->Priority);
	} else {
		new_left = pn_merge(Left, Right->LeftChild);

		return pn_create(Right->Source, Right->StartOffset, Right->Length, Right->LineFeeds, new_left, pn_retain(Right->RightChild), Right->Priority);
	}

	/* return (PN *) NULL; */
}

static ABI void pn_split(Node, split_key, Out)
PN *Node;
size_t split_key;
SP *Out;
{
	static SP sub_split;

	noaof size_t left_feeds, left_len, piece_offset, right_feeds;

	noaof PN *new_right, *new_left;

	if (!Node) {
		Out->LeftNode = 0;
		Out->RightNode = 0;

		return;
	}

	left_len = Node->LeftChild ? Node->LeftChild->SubtreeLength : 0;

	if (split_key <= left_len) {
		pn_split(Node->LeftChild, split_key, &sub_split);
		new_right = pn_create(Node->Source, Node->StartOffset, Node->Length, Node->LineFeeds, sub_split.RightNode, pn_retain(Node->RightChild), Node->Priority);

		Out->LeftNode = sub_split.LeftNode;
		Out->RightNode = new_right;

		return;
	} else if (split_key >= left_len + Node->Length) {
		pn_split(Node->RightChild, split_key - left_len - Node->Length, &sub_split);
		new_left = pn_create(Node->Source, Node->StartOffset, Node->Length, Node->LineFeeds, pn_retain(Node->LeftChild), sub_split.LeftNode, Node->Priority);

		Out->LeftNode = new_left;
		Out->RightNode = sub_split.RightNode;

		return;
	} else {
		piece_offset = split_key - left_len;
		left_feeds = count_newlines(Node->Source, Node->StartOffset, piece_offset);
		right_feeds = Node->LineFeeds - left_feeds;

		Out->LeftNode = pn_create(Node->Source, Node->StartOffset, piece_offset, left_feeds, pn_retain(Node->LeftChild), 0, rng_next());
		Out->RightNode = pn_create(Node->Source, Node->StartOffset + piece_offset, Node->Length - piece_offset, right_feeds, 0, pn_retain(Node->RightChild), rng_next());

		return;
	}
}

static ABI size_t newline_lower_bound(target)
size_t target;
{
	noaof size_t lo, mid, hi;

	lo = 0;
	hi = G->NewlineCount;

	while (lo < hi) {
		mid = lo + (hi - lo) / 2;
		if (G->Newlines[mid] < target)
			lo = mid + 1;
		else hi = mid;
	}

	return lo;
}

static ABI size_t count_newlines(Source, start_offset, len)
unsigned dword Source;
size_t start_offset, len;
{
	noaof size_t end_idx, count, start_idx, i;

	if (len == 0) return 0;

	if (Source == PS_ORIGIN) {
		start_idx = newline_lower_bound(start_offset);
		end_idx = newline_lower_bound(start_offset + len);

		return end_idx - start_idx;
	} else {
		count = 0;
		for (i = 0; i < len; i++) if (G->AddBuffer[start_offset + i] == '\n')
			count++;

		return count;
	}
}

static ABI PN *pt_locate(offset, piece_start, piece_offset)
size_t offset;
size_t *piece_start, *piece_offset;
{
	noaof size_t base_offset, left_len;

	noaof PN *Node;

	Node = G->Root;
	base_offset = 0;

	while (Node) {
		left_len = Node->LeftChild ? Node->LeftChild->SubtreeLength : 0;

		if (offset < left_len) {
			Node = Node->LeftChild;

			continue;
		}

		offset -= left_len;
		base_offset += left_len;

		if (offset < Node->Length) {
			*piece_start = base_offset;
			*piece_offset = offset;

			return Node;
		}

		offset -= Node->Length;
		base_offset += Node->Length;
		Node = Node->RightChild;
	}

	return 0;
}

static ABI unsigned byte pt_character_at(offset)
size_t offset;
{
	static size_t piece_offset, piece_start;

	noaof PN *Node;
	noaof unsigned byte *src;

	Node = pt_locate(offset, &piece_start, &piece_offset);

	if (!Node) return 0;

	src = (Node->Source == PS_ORIGIN) ? G->Buffer : G->AddBuffer;

	return src[Node->StartOffset + piece_offset];
}

static ABI size_t pt_extract(offset, len, dst, dst_cap)
size_t offset, len;
unsigned byte *dst;
size_t dst_cap;
{
    static size_t piece_offset, piece_start;

	noaof size_t available, written, room, requested, total_length;

	noaof PN *Node;
	noaof unsigned byte *src;

	written = 0;
	total_length = (G->Root ? G->Root->SubtreeLength : 0);

	if (offset > total_length) return 0;
	if (offset + len > total_length) len = total_length - offset;

	while (len > 0 && written < dst_cap) {
		Node = pt_locate(offset, &piece_start, &piece_offset);

		if (!Node) break;

		available = Node->Length - piece_offset;
		requested = (len < available) ? len : available;
		room = dst_cap - written;
		if (requested > room)
			requested = room;
		if (requested == 0) break;

		src = (Node->Source == PS_ORIGIN) ? G->Buffer : G->AddBuffer;
		memcpy(dst + written, src + Node->StartOffset + piece_offset, requested);

		written += requested;
		offset += requested;
		len -= requested;
	}

	return written;
}

static ABI size_t pt_line_offset(line)
size_t line;
{
	noaof size_t char_offset;
	noaof size_t base_offset, left_feeds, left_len, newline_idx;

	noaof PN *Node;

	if (line == 0) return 0;
	newline_idx = line - 1;

	Node = G->Root;
	base_offset = 0;

	while (Node) {
		left_feeds = Node->LeftChild ? Node->LeftChild->SubtreeLineFeeds : 0;
		left_len = Node->LeftChild ? Node->LeftChild->SubtreeLength : 0;

		if (newline_idx < left_feeds) {
			Node = Node->LeftChild;

			continue;
		}

		newline_idx -= left_feeds;
		base_offset += left_len;
		char_offset = 0;
		if (newline_idx < Node->LineFeeds) {
			noaof size_t lo_idx, target_nl, i_in;
			target_nl = newline_idx;
			if (Node->Source == PS_ORIGIN) {
				lo_idx = newline_lower_bound(Node->StartOffset);
				char_offset = base_offset + (G->Newlines[lo_idx + target_nl] - Node->StartOffset);
			} else {
				for (i_in = 0; i_in < Node->Length; i_in++) {
					if (G->AddBuffer[Node->StartOffset + i_in] == '\n') {
						if (target_nl == 0) {
							char_offset = base_offset + i_in;
							break;
						}
						target_nl--;
					}
				}
				if (target_nl > 0) char_offset = base_offset + Node->Length;
			}
			return char_offset + 1;
		}

		newline_idx -= Node->LineFeeds;
		base_offset += Node->Length;

		Node = Node->RightChild;
	}

	return (G->Root ? G->Root->SubtreeLength : 0);
}

static ABI void pt_line_bounds(line, out_start, out_length)
size_t line;
size_t *out_start, *out_length;
{
	noaof size_t end_offset, line_count, start_offset, total_length;

	line_count = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);
	total_length = (G->Root ? G->Root->SubtreeLength : 0);

	if (line >= line_count) {
		if (out_start) *out_start = total_length;
		if (out_length) *out_length = 0;

		return;
	}

	start_offset = pt_line_offset(line);
	end_offset = (line + 1 < line_count) ? pt_line_offset(line + 1) - 1 : total_length;

	if (out_start) *out_start = start_offset;
	if (out_length) *out_length = (end_offset > start_offset) ? (end_offset - start_offset) : 0;

	return;
}

static ABI void pt_insert_bytes(offset, data, len)
size_t offset;
unsigned byte *data;
size_t len;
{
	static SP split_result;

	noaof size_t feed_count, piece_start, needed_cap, new_cap;
	noaof unsigned byte *new_buf;

	noaof PN *new_piece, *merged, *new_root;

	if (len == 0) return;

	needed_cap = G->AddLength + len;
	if (needed_cap > G->AddCapacity) {
		new_cap = G->AddCapacity ? G->AddCapacity : 4096;
		while (new_cap < needed_cap) new_cap *= 2;

		new_buf = (unsigned byte *) realloc(G->AddBuffer, new_cap);
		if (new_buf) {
			G->AddBuffer = new_buf;
			G->AddCapacity = new_cap;
		}
	}

	memcpy(G->AddBuffer + G->AddLength, data, len);

	piece_start = G->AddLength;
	G->AddLength += len;

	feed_count = count_newlines(PS_ADD, piece_start, len);
	new_piece = pn_create(PS_ADD, piece_start, len, feed_count, 0, 0, rng_next());

	pn_split(G->Root, offset, &split_result);
	merged = pn_merge(split_result.LeftNode, new_piece);
	new_root = pn_merge(merged, split_result.RightNode);

	pn_release(split_result.LeftNode);
	pn_release(split_result.RightNode);
	pn_release(new_piece);
	pn_release(merged);

	pn_release(G->Root);

	G->Root = new_root;

	return;
}

static ABI void pt_delete_range(offset, len)
size_t offset, len;
{
	static SP split1, split2;

	noaof PN *new_root;

	if (len == 0) return;

	pn_split(G->Root, offset, &split1);
	pn_split(split1.RightNode, len, &split2);
	new_root = pn_merge(split1.LeftNode, split2.RightNode);

	pn_release(split1.LeftNode);
	pn_release(split1.RightNode);
	pn_release(split2.LeftNode);
	pn_release(split2.RightNode);

	pn_release(G->Root);

	G->Root = new_root;

	return;
}

static ABI void push_undo(root)
PN *root;
{
	noaof size_t i, needed_cap, new_cap;
	noaof PN **new_stack;

	if (G->UndoCount >= MAX_HISTORY) {
		pn_release(G->UndoStack[0]);
		for (i = 1; i < G->UndoCount; i++)
			G->UndoStack[i - 1] = G->UndoStack[i];

		G->UndoCount--;
	}

	needed_cap = G->UndoCount + 1;
	if (needed_cap > G->UndoCapacity) {
		new_cap = G->UndoCapacity ? G->UndoCapacity : 64;
		while (new_cap < needed_cap) new_cap *= 2;

		new_stack = (PN **) realloc(G->UndoStack, new_cap * sizeof(PN *));
		if (new_stack) {
			G->UndoStack = new_stack;
			G->UndoCapacity = new_cap;
		}
	}

	G->UndoStack[G->UndoCount++] = pn_retain(root);

	return;
}

static ABI void push_redo(root)
PN *root;
{
	noaof size_t needed_cap, new_cap;
	noaof PN **new_stack;

	needed_cap = G->RedoCount + 1;
	if (needed_cap > G->RedoCapacity) {
		new_cap = G->RedoCapacity ? G->RedoCapacity : 64;
		while (new_cap < needed_cap) new_cap *= 2;

		new_stack = (PN **) realloc(G->RedoStack, new_cap * sizeof(PN *));
		if (new_stack) {
			G->RedoStack = new_stack;
			G->RedoCapacity = new_cap;
		}
	}

	G->RedoStack[G->RedoCount++] = pn_retain(root);

	return;
}

static ABI void undo_begin_edit(void)
{
	if (!G->Undo) {
		push_undo(G->Root);
		{
			noaof size_t i;
			for (i = 0; i < G->RedoCount; i++) pn_release(G->RedoStack[i]);
			G->RedoCount = 0;
		}

		G->Undo = true;
	}

	return;
}

static ABI void fix_cursor(void)
{
	noaof size_t line_count;

	line_count = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

	if (G->CursorRow >= line_count)
		G->CursorRow = line_count - 1;

	clamp_column();
	adjust_scroll();

	return;
}

static ABI void undo(void)
{
	noaof PN *prev_root;

	if (G->UndoCount == 0) {
		status("Already at oldest change.");

		return;
	}

	push_redo(G->Root);

	prev_root = (G->UndoCount == 0 ? 0 : G->UndoStack[--G->UndoCount]);
	pn_release(G->Root);

	G->Root = prev_root;
	G->Undo = false;

	fix_cursor();
	mark_dirty(DIRTY_FULL, 0);
	status("Undo.");

	return;
}

static ABI void redo(void)
{
	noaof PN *next_root;

	if (G->RedoCount == 0) {
		status("Already at newest change.");

		return;
	}

	push_undo(G->Root);

	next_root = (G->RedoCount == 0 ? 0 : G->RedoStack[--G->RedoCount]);
	pn_release(G->Root);

	G->Root = next_root;
	G->Undo = false;

	fix_cursor();
	mark_dirty(DIRTY_FULL, 0);
	status("Redo.");

	return;
}

static ABI size_t line_length(line)
size_t line;
{
	static size_t length;

	pt_line_bounds(line, (size_t *) NULL, &length);

	return length;
}

static ABI size_t line_layout(line, stop_offset, out_row, out_col)
size_t line, stop_offset;
size_t *out_row, *out_col;
{
    static size_t line_len, start_offset;

	noaof unsigned byte has_stopped, c;
	noaof size_t char_offset, row_idx, scratch_len, tab_width, term_cols, vis_col;

	if (line >= ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1)) {
		if (out_row) *out_row = 0;
		if (out_col) *out_col = 0;

		return 1;
	}

	start_offset = 0;
	line_len = 0;
	pt_line_bounds(line, &start_offset, &line_len);

	term_cols = G->ScreenColumns ? G->ScreenColumns : 1;
	if (term_cols > lines_width()) term_cols -= lines_width();
	else term_cols = 1;

	scratch_len = pt_extract(start_offset, line_len, G->ScratchLineBuffer, sizeof(G->ScratchLineBuffer));
	if (line_len > scratch_len) line_len = scratch_len;

	char_offset = 0;
	row_idx = 0;
	vis_col = 0;
	has_stopped = false;

	while (char_offset < line_len) {
		c = G->ScratchLineBuffer[char_offset];

		if (c == '\t') {
			tab_width = TAB_WIDTH - (vis_col % TAB_WIDTH);

			if (vis_col + tab_width > term_cols && vis_col > 0) {
				row_idx++;
				vis_col = 0;

				continue;
			}

			if (!has_stopped && char_offset >= stop_offset) {
				if (out_row) *out_row = row_idx;
				if (out_col) *out_col = vis_col;

				has_stopped = true;
			}

			vis_col += tab_width;
			char_offset++;

		} else if ((c & 0xC0) == 0x80) {
			char_offset++;
		} else {
			if (vis_col + 1 > term_cols && vis_col > 0) {
				row_idx++;
				vis_col = 0;

				continue;
			}

			if (!has_stopped && char_offset >= stop_offset) {
				if (out_row) *out_row = row_idx;
				if (out_col) *out_col = vis_col;

				has_stopped = true;
			}

			vis_col++;
			char_offset++;

			while (char_offset < line_len && (G->ScratchLineBuffer[char_offset] & 0xC0) == 0x80) char_offset++;
		}
	}

	if (!has_stopped) {
		if (out_row) *out_row = row_idx;
		if (out_col) *out_col = vis_col;
	}

	return row_idx + 1;
}

static ABI INLINE size_t utf8_char_length(c)
unsigned dword c;
{
	if ((c & 0x80) == 0x00) return 1;
	if ((c & 0xE0) == 0xC0) return 2;
	if ((c & 0xF0) == 0xE0) return 3;
	if ((c & 0xF8) == 0xF0) return 4;

	return 1;
}

static ABI void clamp_column(void)
{
	noaof size_t len;
	noaof size_t start_offset;

	len = line_length(G->CursorRow);
	start_offset = pt_line_offset(G->CursorRow);

	if (len == 0) {
		G->CursorColumn = 0;

		return;
	}

	if (G->CurrentMode == INSERT_MODE) {
		if (G->CursorColumn >= len) G->CursorColumn = len;
	} else if (G->CursorColumn >= len) {
		G->CursorColumn = len - 1;
	}

	while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;

	return;
}

static ABI unsigned byte do_search(pattern, pattern_len, direction)
unsigned byte *pattern;
size_t pattern_len;
signed dword direction;
{
	static unsigned byte scratch[4096];

	noaof size_t line_count, line_idx, line_len, match_col, scan_len, scratch_len, search_line, start_col;

	if (pattern_len == 0) return false;

	line_count = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);
	if (line_count == 0) return false;

	search_line = G->CursorRow;
	start_col = (direction > 0) ? G->CursorColumn + 1 : G->CursorColumn;

	for (line_idx = 0; line_idx <= line_count; line_idx++) {
		line_len = line_length(search_line);

		scratch_len = pt_extract(pt_line_offset(search_line), line_len, scratch, sizeof(scratch));
		if (line_len > scratch_len) line_len = scratch_len;

		if (direction > 0) {
			if (line_idx > 0) start_col = 0;

			if (start_col < line_len && pattern_len <= line_len) {
				scan_len = line_len - pattern_len;

				for (match_col = start_col; match_col <= scan_len; match_col++) {
					if (memcmp(scratch + match_col, pattern, pattern_len) == 0) {
						G->CursorRow = search_line;
						G->CursorColumn = match_col;

						return true;
					}
				}
			}

			search_line = (search_line + 1 < line_count) ? search_line + 1 : 0;
		} else {
			if (line_idx > 0) start_col = line_len;

			if (pattern_len <= line_len && start_col >= pattern_len) {
				scan_len = start_col - pattern_len;
				if (scan_len > line_len - pattern_len) scan_len = line_len - pattern_len;

				match_col = scan_len + 1;
				while (match_col > 0) {
					match_col--;

					if (memcmp(scratch + match_col, pattern, pattern_len) == 0) {
						G->CursorRow = search_line;
						G->CursorColumn = match_col;

						return true;
					}
				}
			}

			search_line = (search_line > 0) ? search_line - 1 : line_count - 1;
		}
	}

	return false;
}

static ABI unsigned byte bytes_equal(a, b, len, case_sensitive)
unsigned byte *a;
unsigned byte *b;
size_t len;
signed dword case_sensitive;
{
	noaof size_t i;
	noaof unsigned byte ca, cb;

	if (case_sensitive) return (memcmp(a, b, len) == 0);

	for (i = 0; i < len; i++) {
		ca = a[i];
		cb = b[i];

		if (ca >= 'A' && ca <= 'Z') ca = (unsigned byte) (ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = (unsigned byte) (cb - 'A' + 'a');

		if (ca != cb) return false;
	}

	return true;
}

static ABI unsigned byte is_word_byte(c)
signed dword c;
{
	return (unsigned byte) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
}

static ABI void offset_to_cursor(offset, out_row, out_column)
size_t offset;
size_t *out_row;
size_t *out_column;
{
	noaof size_t lo, mid, hi, line_count;

	line_count = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

	lo = 0;
	hi = line_count;

	while (lo + 1 < hi) {
		mid = lo + (hi - lo) / 2;

		if (pt_line_offset(mid) <= offset) lo = mid;
		else hi = mid;
	}

	*out_row = lo;
	*out_column = offset - pt_line_offset(lo);

	return;
}

static ABI void free_match_set(void)
{
	if (G->MatchOffsets) free(G->MatchOffsets);

	G->MatchOffsets = 0;
	G->MatchCount = 0;
	G->MatchIndex = 0;
	G->MatchLength = 0;
	G->MatchActive = false;
	G->MatchEscArmed = false;
	G->MatchOffsetDelta = 0;

	return;
}

static ABI size_t find_all_matches(pattern, pattern_len)
unsigned byte *pattern;
size_t pattern_len;
{
	noaof size_t total_length, extracted_length, scan_len, i, count, capacity;
	noaof unsigned byte *doc;
	noaof size_t *new_offsets;

	free_match_set();

	if (pattern_len == 0) return 0;

	total_length = (G->Root ? G->Root->SubtreeLength : 0);
	if (total_length < pattern_len) return 0;

	doc = (unsigned byte *) malloc(total_length);
	if (!doc) return 0;

	extracted_length = pt_extract(0, total_length, doc, total_length);

	count = 0;
	capacity = 0;

	if (extracted_length >= pattern_len) {
		scan_len = extracted_length - pattern_len;

		for (i = 0; i <= scan_len; i++) {
			if (G->MatchWholeWord) {
				if (i > 0 && is_word_byte(doc[i - 1])) continue;
				if (i + pattern_len < extracted_length && is_word_byte(doc[i + pattern_len])) continue;
			}

			if (bytes_equal(doc + i, pattern, pattern_len, G->MatchCaseSensitive)) {
				if (count == capacity) {
					capacity = capacity ? capacity * 2 : 64;

					new_offsets = (size_t *) realloc(G->MatchOffsets, capacity * sizeof(size_t));
					if (!new_offsets) break;

					G->MatchOffsets = new_offsets;
				}

				G->MatchOffsets[count++] = i;
			}
		}
	}

	free(doc);

	G->MatchCount = count;
	G->MatchLength = pattern_len;
	G->MatchIndex = 0;
	G->MatchActive = (count > 0);

	return count;
}

static ABI void goto_match(index)
size_t index;
{
	static size_t row, column;
	noaof size_t adjusted_offset;

	if (!G->MatchActive || G->MatchCount == 0) return;
	if (index >= G->MatchCount) index = G->MatchCount - 1;

	G->MatchIndex = index;

	adjusted_offset = (size_t) ((signed dword) G->MatchOffsets[index] + G->MatchOffsetDelta);

	offset_to_cursor(adjusted_offset, &row, &column);

	G->CursorRow = row;
	G->CursorColumn = column;

	clamp_column();
	adjust_scroll();

	return;
}

static ABI void status_match_progress(verb, current, total)
byte *verb;
size_t current;
size_t total;
{
	noaof unsigned byte *s1, *s2;
	static unsigned byte num1[22], num2[22];

	s1 = ulltod((unsigned qword) current, num1);
	s2 = ulltod((unsigned qword) total, num2);

	G->StatusLength = 0;

	memcpy(G->StatusBuffer + G->StatusLength, verb, strlen(verb));
	G->StatusLength += strlen(verb);

	memcpy(G->StatusBuffer + G->StatusLength, ": ", 2);
	G->StatusLength += 2;

	memcpy(G->StatusBuffer + G->StatusLength, s1, strlen((byte *) s1));
	G->StatusLength += strlen((byte *) s1);

	memcpy(G->StatusBuffer + G->StatusLength, "/", 1);
	G->StatusLength += 1;

	memcpy(G->StatusBuffer + G->StatusLength, s2, strlen((byte *) s2));
	G->StatusLength += strlen((byte *) s2);

	memcpy(G->StatusBuffer + G->StatusLength, ".", 1);
	G->StatusLength += 1;

	return;
}

static ABI unsigned char match_step(direction)
signed dword direction;
{
	noaof size_t next_index;

	if (!G->MatchActive || G->MatchCount == 0) return false;

	G->MatchEscArmed = false;

	if (direction > 0)
		next_index = (G->MatchIndex + 1 < G->MatchCount) ? G->MatchIndex + 1 : 0;
	else
		next_index = (G->MatchIndex > 0) ? G->MatchIndex - 1 : G->MatchCount - 1;

	goto_match(next_index);
	status_match_progress("Matches", next_index + 1, G->MatchCount);

	return true;
}

static ABI void replace_current_match(void)
{
	noaof size_t match_offset, index;
	static size_t replaced_so_far, original_total;

	if (!G->MatchActive || G->MatchCount == 0) return;
	if (G->MatchIndex >= G->MatchCount) return;

	index = G->MatchIndex;
	match_offset = (size_t) ((signed dword) G->MatchOffsets[index] + G->MatchOffsetDelta);

	undo_begin_edit();

	pt_delete_range(match_offset, G->MatchLength);
	if (G->ReplaceWithLength > 0)
		pt_insert_bytes(match_offset, G->ReplaceWithBuffer, G->ReplaceWithLength);

	G->Dirty = true;

	G->MatchOffsetDelta += (signed dword) G->ReplaceWithLength - (signed dword) G->MatchLength;

	replaced_so_far = G->ReplacedThisSession + 1;
	original_total = G->OriginalMatchTotal;

	G->ReplacedThisSession = replaced_so_far;

	status_match_progress("Replace", replaced_so_far, original_total);

	mark_dirty(DIRTY_FULL, 0);

	if (index + 1 >= G->MatchCount) {
		free_match_set();
		G->Undo = false;
	} else {
		G->MatchIndex = index + 1;
		goto_match(G->MatchIndex);
	}

	return;
}

static ABI void replace_all_remaining(void)
{
	noaof size_t final_replaced, final_total;

	if (!G->MatchActive || G->MatchCount == 0) return;

	G->ReplacingInProgress = true;

	while (G->MatchActive && G->MatchCount > 0 && G->ReplacingInProgress)
		replace_current_match();

	final_replaced = G->ReplacedThisSession;
	final_total = G->OriginalMatchTotal;

	G->ReplacingInProgress = false;
	G->ReplacedThisSession = 0;

	status_match_progress("Replaced", final_replaced, final_total);

	return;
}

static ABI void out_flush(void)
{
	if (!G->OutputLength) return;

	write(1, G->OutputBuffer, G->OutputLength);

	G->OutputLength = 0;

	return;
}

static ABI void out_bytes(data, count)
unsigned byte *data;
size_t count;
{
	noaof size_t available;

	while (count > 0) {
		if (G->OutputLength == (1024 * 64)) out_flush();

		available = (1024 * 64) - G->OutputLength;
		if (count < available) available = count;

		memcpy(G->OutputBuffer + G->OutputLength, data, available);

		G->OutputLength += available;
		data += available;
		count -= available;
	}

	return;
}

static ABI void out_str(string)
byte *string;
{
	noaof size_t str_len;

	str_len = strlen(string);

	out_bytes((unsigned byte *) string, str_len);

	return;
}

static ABI INLINE void out_byte(ch)
unsigned dword ch;
{
	if (G->OutputLength == (1024 * 64)) out_flush();

	G->OutputBuffer[G->OutputLength++] = (unsigned byte) ch;

	return;
}

static ABI void seq_out(sequence)
byte *sequence;
{
	out_byte(0x1B);
	out_byte('[');
	out_str(sequence);

	return;
}

static ABI void set_cursor(row, column)
unsigned dword row, column;
{
	static unsigned byte col_buf[22], row_buf[22];

	noaof unsigned byte *col_str, *row_str;

	row_str = ulltod(row + 1, row_buf);
	col_str = ulltod(column + 1, col_buf);

	out_byte(0x1B);
	out_byte('[');
	out_str((byte *) row_str);
	out_byte(';');
	out_str((byte *) col_str);
	out_byte('H');

	return;
}

static ABI void status(message)
byte *message;
{
	noaof size_t msg_len;

	msg_len = strlen(message);

	if (msg_len >= sizeof(G->StatusBuffer))
		msg_len = sizeof(G->StatusBuffer) - 1;

	memcpy(G->StatusBuffer, message, msg_len);

	G->StatusLength = msg_len;

	return;
}

static ABI unsigned byte save(void)
{
    static unsigned byte num_buf[22];

	noaof signed dword fd;
	noaof ssize_t rw_result;
	noaof size_t total_written, chunk_written, current_pos, extracted, filename_len, num_str_len, requested, suffix_len, total_length;

	noaof unsigned byte *num_str, *unit_str, *suffix_str;

	total_written = 0;
	current_pos = 0;
	filename_len = 0;
	suffix_len = 0;

	suffix_str = (unsigned byte *) " written ";
	unit_str = (unsigned byte *) "B";

	if (!G->Filename[0]) {
		status("No filename.");

		return false;
	}

	fd = open((byte *) G->Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		status("Failed to write file.");

		return false;
	}

	total_length = (G->Root ? G->Root->SubtreeLength : 0);

	while (current_pos < total_length) {
		requested = total_length - current_pos;
		chunk_written = 0;

		if (requested > sizeof(G->ScratchSaveBuffer))
			requested = sizeof(G->ScratchSaveBuffer);

		extracted = pt_extract(current_pos, requested, G->ScratchSaveBuffer, sizeof(G->ScratchSaveBuffer));
		if (extracted == 0) break;

		while (chunk_written < extracted) {
			rw_result = write(fd, G->ScratchSaveBuffer + chunk_written, extracted - chunk_written);
			if (rw_result <= 0) break;

			chunk_written += (size_t) rw_result;
		}

		total_written += chunk_written;
		current_pos += extracted;

		if (chunk_written < extracted) break;
	}

	close(fd);
	G->Dirty = false;

	num_str = ulltod(total_written, num_buf);
	num_str_len = strlen((byte *) num_str);
	suffix_len = 9 + num_str_len + 1;

	filename_len = strlen((byte *) G->Filename);
	if (filename_len + suffix_len > sizeof(G->StatusBuffer)) {
		if (sizeof(G->StatusBuffer) > suffix_len)
			filename_len = sizeof(G->StatusBuffer) - suffix_len;
		else filename_len = 0;
	}

	G->StatusLength = 0;

	memcpy(G->StatusBuffer + G->StatusLength, G->Filename, filename_len);
	G->StatusLength += filename_len;

	memcpy(G->StatusBuffer + G->StatusLength, suffix_str, 9);
	G->StatusLength += 9;

	memcpy(G->StatusBuffer + G->StatusLength, num_str, num_str_len);
	G->StatusLength += num_str_len;

	memcpy(G->StatusBuffer + G->StatusLength, unit_str, 1);
	G->StatusLength += 1;

	return true;
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

	return;
}

static ABI size_t visual_row_of(line)
size_t line;
{
	noaof size_t line_idx, row;

	row = 0;
	for (line_idx = G->TopLineIndex; line_idx < line; line_idx++)
		row += line_layout(line_idx, (size_t) -1, 0, 0);

	return row;
}

static ABI size_t lines_width(void)
{
	noaof size_t digits, line_count;

	if (!G->Lines) return 0;

	line_count = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

	digits = 1;
	while (line_count >= 10) {
		line_count /= 10;
		digits++;
	}

	if (digits < LINE_DIGITS_MIN) digits = LINE_DIGITS_MIN;

	return digits + LINE_SEP_WIDTH;
}

static ABI void draw_lines(line)
size_t line;
{
	static unsigned byte num_buf[22];
	noaof unsigned byte *num_str;
	noaof size_t digits, num_str_len, pad_idx, width;

	width = lines_width();
	if (!width) return;

	digits = width - LINE_SEP_WIDTH;

	num_str = ulltod(line + 1, num_buf);
	num_str_len = strlen((byte *) num_str);

	seq_out("2m");

	for (pad_idx = num_str_len; pad_idx < digits; pad_idx++) out_byte(' ');

	out_str((byte *) num_str);
	out_byte(' ');

	out_byte(0xE2);
	out_byte(0x94);
	out_byte(0x82);

	out_byte(' ');

	seq_out("m");

	return;
}

static ABI ws_row draw_line(line_idx, screen_row, screen_rows, term_cols)
size_t line_idx;
unsigned dword screen_row, screen_rows;
size_t term_cols;
{
	noaof unsigned byte c;
	noaof size_t char_offset, scratch_len, tab_width, vis_col, i;
	static size_t start_offset, line_len;

	start_offset = 0;
	line_len = 0;
	pt_line_bounds(line_idx, &start_offset, &line_len);

	scratch_len = pt_extract(start_offset, line_len, G->ScratchLineBuffer, sizeof(G->ScratchLineBuffer));
	if (line_len > scratch_len)
		line_len = scratch_len;

	if (screen_row >= screen_rows)
		return (ws_row) screen_row;

	char_offset = 0;
	vis_col = 0;

	set_cursor(screen_row, 0);
	seq_out("2K");
	draw_lines(line_idx);

	while (char_offset < line_len) {
		c = G->ScratchLineBuffer[char_offset];

		if (c == '\t') {
			tab_width = TAB_WIDTH - (vis_col % TAB_WIDTH);

			if (vis_col + tab_width > term_cols && vis_col > 0) {
				screen_row++;
				if (screen_row >= screen_rows)
					return (ws_row) screen_row;

				vis_col = 0;
				set_cursor(screen_row, 0);
				seq_out("2K");
				for (i = 0; i < lines_width(); i++) out_byte(' ');

				continue;
			}

			for (i = 0; i < tab_width; i++) out_byte(' ');

			vis_col += tab_width;
			char_offset++;
		} else if ((c & 0xC0) == 0x80) {
			out_byte(c);

			char_offset++;
		} else {
			if (vis_col + 1 > term_cols && vis_col > 0) {
				screen_row++;
				if (screen_row >= screen_rows)
					return (ws_row) screen_row;

				vis_col = 0;
				set_cursor(screen_row, 0);
				seq_out("2K");
				for (i = 0; i < lines_width(); i++) out_byte(' ');

				continue;
			}

			out_byte(c);
			vis_col++;
			char_offset++;

			while (char_offset < line_len && (G->ScratchLineBuffer[char_offset] & 0xC0) == 0x80) {
				out_byte(G->ScratchLineBuffer[char_offset]);

				char_offset++;
			}
		}
	}

	return (ws_row) (screen_row + 1);
}

static ABI void render(void)
{
    static size_t col_in_line, row_in_line;
    static unsigned byte col_buf[22], row_buf[22];

	noaof ws_row cur_row, screen_rows;
	noaof unsigned byte kind, scrolled;
	noaof size_t col_str_len, cur_line, info_len, line_count, line_idx, row_str_len, term_cols, vis_row;

	noaof unsigned byte *col_str, *row_str;

	screen_rows = G->ScreenRows - 1;
	term_cols = G->ScreenColumns ? G->ScreenColumns : 1;
	if (term_cols > lines_width()) term_cols -= lines_width();
	else term_cols = 1;
	line_count = ((G->Root ? G->Root->SubtreeLineFeeds : 0) + 1);

	scrolled = (G->TopLineIndex != G->RenderedTopLine);
	kind = scrolled ? DIRTY_FULL : G->DirtyKind;

	seq_out("?25l");
	seq_out("?7l");

	if (kind == DIRTY_FULL || kind == DIRTY_FROM) {
		cur_line = (kind == DIRTY_FROM && G->DirtyLine > G->TopLineIndex) ? G->DirtyLine : G->TopLineIndex;
		cur_row = (ws_row) visual_row_of(cur_line);

		while (cur_row < screen_rows && cur_line < line_count) {
			cur_row = draw_line(cur_line, cur_row, screen_rows, term_cols);

			cur_line++;
		}

		while (cur_row < screen_rows) {
			set_cursor(cur_row, 0);
			seq_out("2K");
			draw_lines(cur_line);
			seq_out("2m");
			out_byte('~');
			seq_out("m");

			cur_row++;
			cur_line++;
		}
	} else if (kind == DIRTY_LINE) {
		cur_row = (ws_row) visual_row_of(G->DirtyLine);

		if (G->DirtyLine >= G->TopLineIndex && G->DirtyLine < line_count && cur_row < screen_rows)
			draw_line(G->DirtyLine, cur_row, screen_rows, term_cols);
	}

	G->DirtyKind = DIRTY_NONE;
	G->RenderedTopLine = G->TopLineIndex;

	set_cursor(G->ScreenRows - 1, 0);
	seq_out("2K");

	switch (G->CurrentMode) {
		case COMMAND_MODE: {
			out_byte(':');
			out_bytes(G->CommandLineBuffer, G->CommandLineLength);

			break;
		}

		case SEARCH_MODE: {
			out_byte('/');
			out_bytes(G->SearchBuffer, G->SearchLength);

			break;
		}

		case REPLACE_MODE: {
			seq_out("7m");
			out_str(" REPLACE ");
			seq_out("m");
			out_byte(' ');

			out_bytes(G->ReplaceFindBuffer, G->ReplaceFindLength);

			if (G->ReplaceStage == 1) {
				out_str(" \xE2\x86\x92 ");
				out_bytes(G->ReplaceWithBuffer, G->ReplaceWithLength);
			}

			out_byte(' ');

			if (G->MatchCaseSensitive) seq_out("7m");
			out_str("[Aa]");
			if (G->MatchCaseSensitive) seq_out("m");

			out_byte(' ');

			if (G->MatchWholeWord) seq_out("7m");
			out_str("[W]");
			if (G->MatchWholeWord) seq_out("m");

			break;
		}

		default: {
			switch (G->CurrentMode) {
				case INSERT_MODE: {
					seq_out("7m");
					out_str(" INSERT ");
					seq_out("m");
					out_byte(' ');

					break;
				}

				default: {
					seq_out("7m");
					out_str(" NORMAL ");
					seq_out("m");
					out_byte(' ');

					break;
				}
			}

			if (G->StatusLength) {
				out_bytes(G->StatusBuffer, G->StatusLength);
			} else {
				out_str(G->Filename[0] ? (byte *) G->Filename : "[No Name]");
				if (G->Dirty) out_str(" [+]");
			}

			row_str = ulltod(G->CursorRow + 1, row_buf);
			col_str = ulltod(G->CursorColumn + 1, col_buf);
			row_str_len = strlen((byte *) row_str);
			col_str_len = strlen((byte *) col_str);
			info_len = row_str_len + 1 + col_str_len;

			if (G->ScreenColumns > info_len + 1) {
				set_cursor(G->ScreenRows - 1, (ws_col) (G->ScreenColumns - info_len - 1));
				out_str((byte *) row_str);
				out_byte(':');
				out_str((byte *) col_str);
			} break;
		}
	}

	switch (G->CurrentMode) {
		case COMMAND_MODE: {
			set_cursor(G->ScreenRows - 1, (ws_col) (1 + G->CommandLineLength));

			break;
		}

		case SEARCH_MODE: {
			set_cursor(G->ScreenRows - 1, (ws_col) (1 + G->SearchLength));

			break;
		}

		case REPLACE_MODE: {
			if (G->ReplaceStage == 0) {
				set_cursor(G->ScreenRows - 1, (ws_col) (10 + G->ReplaceFindLength));
			} else {
				set_cursor(G->ScreenRows - 1, (ws_col) (10 + G->ReplaceFindLength + 3 + G->ReplaceWithLength));
			}

			break;
		}

		default: {
			vis_row = 0;

			for (line_idx = G->TopLineIndex; line_idx < G->CursorRow; line_idx++)
				vis_row += line_layout(line_idx, (size_t) -1, 0, 0);

			line_layout(G->CursorRow, G->CursorColumn, &row_in_line, &col_in_line);
			vis_row += row_in_line;
			set_cursor((ws_row) vis_row, (ws_col) (col_in_line + lines_width()));

			break;
		}
	}

	seq_out("?7h");
	seq_out("?25h");

	out_flush();

	return;
}

static ABI INLINE size_t cursor_offset(void)
{
	noaof size_t offset;
	noaof size_t total_length;

	offset = pt_line_offset(G->CursorRow) + G->CursorColumn;
	total_length = (G->Root ? G->Root->SubtreeLength : 0);

	return offset > total_length ? total_length : offset;
}

static ABI void insert_char(offset, c, line)
size_t offset;
unsigned dword c;
size_t line;
{
	noaof unsigned byte structural;
	noaof size_t rows_after, rows_before;

	structural = (c == '\n');
	rows_before = structural ? 0 : line_layout(line, (size_t) -1, 0, 0);

	undo_begin_edit();
	pt_insert_bytes(offset, (unsigned byte *) &c, 1);

	G->Dirty = true;

	if (!structural) {
		rows_after = line_layout(line, (size_t) -1, 0, 0);
		if (rows_after != rows_before) structural = true;
	}

	mark_dirty(structural ? DIRTY_FROM : DIRTY_LINE, line);

	return;
}

static ABI void delete_char(offset, line)
size_t offset, line;
{
	noaof unsigned byte structural;
	noaof size_t rows_after, rows_before;

	if (offset >= (G->Root ? G->Root->SubtreeLength : 0)) return;

	structural = (pt_character_at(offset) == '\n');
	rows_before = structural ? 0 : line_layout(line, (size_t) -1, 0, 0);

	undo_begin_edit();
	pt_delete_range(offset, 1);

	G->Dirty = true;

	if (!structural) {
		rows_after = line_layout(line, (size_t) -1, 0, 0);
		if (rows_after != rows_before)
			structural = true;
	}

	mark_dirty(structural ? DIRTY_FROM : DIRTY_LINE, line);

	return;
}

static ABI void delete_char_at_cursor(void)
{
	noaof size_t char_length, remaining, start_offset, len, i;

	len = line_length(G->CursorRow);
	if (len > 0 && G->CursorColumn < len) {
		start_offset = pt_line_offset(G->CursorRow);
		remaining = len - G->CursorColumn;
		char_length = utf8_char_length(pt_character_at(start_offset + G->CursorColumn));

		if (char_length > remaining)
			char_length = remaining;

		for (i = 0; i < char_length; i++)
			delete_char(cursor_offset(), G->CursorRow);
	}

	G->Undo = false;

	return;
}

static ABI void adjust_scroll(void)
{
	noaof ws_row screen_rows;
	noaof size_t line_idx, vis_rows;
	static size_t row_in_line;

	screen_rows = G->ScreenRows - 1;

	if (G->CursorRow < G->TopLineIndex)
		G->TopLineIndex = G->CursorRow;

	vis_rows = 0;
	for (line_idx = G->TopLineIndex; line_idx < G->CursorRow; line_idx++)
		vis_rows += line_layout(line_idx, (size_t) -1, 0, 0);

	line_layout(G->CursorRow, G->CursorColumn, &row_in_line, (size_t *) NULL);
	vis_rows += row_in_line + 1;

	while (vis_rows > screen_rows && G->TopLineIndex < G->CursorRow) {
		vis_rows -= line_layout(G->TopLineIndex, (size_t) -1, 0, 0);

		G->TopLineIndex++;
	}

	return;
}

static ABI void move_cursor(direction)
signed dword direction;
{
	noaof size_t start_offset, len;

	len = line_length(G->CursorRow);
	start_offset = pt_line_offset(G->CursorRow);

	switch (direction) {
		case 'h': {
			if (G->CursorColumn > 0) {
				G->CursorColumn--;

				while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
			} break;
		}

		case 'l': {
			if (len > 0 && G->CursorColumn < len) {
				G->CursorColumn++;
				while (G->CursorColumn < len && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80)
					G->CursorColumn++;
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
					while (G->CursorColumn > 0 && (pt_character_at(start_offset + G->CursorColumn) & 0xC0) == 0x80) G->CursorColumn--;
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

	return;
}
