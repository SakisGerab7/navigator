#define _DEFAULT_SOURCE

#include <ncurses.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

typedef struct File {
    enum {
        FILE_REGULAR,
        FILE_DIRECTORY
    } type;
    char name[20];
    size_t size;
} File;

typedef struct Directory {
    char *path;
    File *files;
    size_t len, cap, cursor;
    size_t maxname, maxsize;
} Directory;

static const char *FILE_TYPE_STR[] = {
    [FILE_REGULAR]   = "File",
    [FILE_DIRECTORY] = "Directory"
};

int compare(const void *fst, const void *snd) {
    File fst_file = *(File *) fst;
    File snd_file = *(File *) snd;

    fst_file.name[0] = tolower(fst_file.name[0]);
    snd_file.name[0] = tolower(snd_file.name[0]);

    return strcmp(fst_file.name, snd_file.name);
}

char *path_init() {
    char *username = getlogin();
    char *path = calloc(strlen(username) + 7, sizeof(char));
    sprintf(path, "/home/%s", username);
    return path;
}

char *path_update(char *path, char *addition) {
    char *new_path;
    size_t new_size;

    if (strcmp(addition, "..") == 0) {
        char *pos = strrchr(path, '/');
        if (pos != NULL) {
            new_size = (size_t) (pos - path);
            if (new_size == 0) new_size = 1;
            new_path = calloc(new_size + 1, sizeof(char));
            strncpy(new_path, path, new_size);
            free(path);
        } 
    } else {
        new_size = strlen(path) + strlen(addition) + 1;
        new_path = realloc(path, (new_size + 1) * sizeof(char));
        strcat(new_path, "/");
        strcat(new_path, addition);
    }

    return new_path;
}

Directory *directory_init(char *path) {
    Directory *dir = malloc(sizeof(Directory));
    dir->path = path;
    dir->cap = 10;
    dir->len = 0;
    dir->cursor = 0;
    dir->maxname = 0;
    dir->maxsize = 0;
    dir->files = malloc(dir->cap * sizeof(File));

    DIR *temp_dir = opendir(path);
    if (temp_dir == NULL) return NULL;

    struct dirent *entry;
    struct stat sb;
    File file;
    while ((entry = readdir(temp_dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            if (strcmp(path, "/") == 0) continue;
            if (strcmp(entry->d_name, "..") != 0) continue;
        }

        char file_path[256];
        sprintf(file_path, "%s/%s", path, entry->d_name);
        stat(file_path, &sb);
        file.size = sb.st_size / 1024;

        strncpy(file.name, entry->d_name, 20);
        switch (entry->d_type) {
            case DT_REG: file.type = FILE_REGULAR; break;
            case DT_DIR: file.type = FILE_DIRECTORY; break;
        }

        size_t fname_size = strlen(file.name);
        dir->maxname = fname_size > dir->maxname ? fname_size : dir->maxname;

        size_t digits = file.size == 0 ? 1 : (size_t) log10((double) file.size) + 1;
        dir->maxsize = digits > dir->maxsize ? digits : dir->maxsize;

        dir->files[dir->len++] = file;

        if (dir->len == dir->cap) {
            dir->cap += dir->cap / 2;
            dir->files = realloc(dir->files, dir->cap * sizeof(File));
        }
    }

    closedir(temp_dir);
    qsort(dir->files, dir->len, sizeof(File), compare);
    return dir;
}

void directory_free(Directory *dir) {
    free(dir->files);
    free(dir);
}

void print_bottom_text(size_t height, size_t width, char *path, size_t cursor) {
    time_t timer = time(NULL);
    struct tm *datetime = localtime(&timer);

    char time_buf[64];
    strftime(time_buf, 64, "%d %b %Y, %R ", datetime);

    size_t time_buf_len = strlen(time_buf);

    attron(COLOR_PAIR(2));
    mvprintw(height - 1, 0, " [Sakis Navigator] Path: %s, Cursor: %2lu", path, cursor);

    while (getcurx(stdscr) < width - time_buf_len) {
        addch(' ');
    }

    printw("%s", time_buf);
    attroff(COLOR_PAIR(2));
}

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, 8 | COLOR_BLACK);

    char *path = path_init();

    size_t width = getmaxx(stdscr);
    size_t height = getmaxy(stdscr);

    Directory *dir = directory_init(path);
    char input;

    while (true) {
        clear();

        print_bottom_text(height, width, path, dir->cursor);

        size_t start = 0;
        if (dir->cursor >= height - 1) {
            start = dir->cursor - height + 2;
        }

        for (size_t i = 0; i < height - 1; i++) {
            move(i, 0);

            if (i == dir->cursor || (start > 0 && i == height - 2)) {
                attron(COLOR_PAIR(1));
            }

            File *file = &dir->files[start + i];
            printw(" [%2d]  %-*s  %-10s", start + i, dir->maxname, file->name, FILE_TYPE_STR[file->type]);
            if (file->type == FILE_REGULAR) {
                printw("  %*lu KB ", dir->maxsize, file->size);
            }
            
            if (i == dir->cursor || (start > 0 && i == height - 2)) {
                attroff(COLOR_PAIR(1));
            }

            if (i == dir->len - 1) break;
        }

        refresh();
        move(dir->cursor, 0);
        input = getch();

        switch (input) {
            case 'q':
            case 'Q': {
                directory_free(dir);
                free(path);
                endwin();
                return 0;
            }
            case 'w':
            case 'W': {
                if (dir->cursor != 0) {
                    dir->cursor--;
                }
            }
            break;
            case 's':
            case 'S': {
                if (dir->cursor != dir->len - 1) {
                    dir->cursor++;
                }
            }
            break;
            case '\n': {
                File *file = &dir->files[dir->cursor];
                if (file->type == FILE_DIRECTORY) {
                    path = path_update(path, file->name);
                    directory_free(dir);
                    dir = directory_init(path);
                } else if (file->type == FILE_REGULAR) {
                    char command[256];
                    sprintf(command, "vim %s/%s", path, file->name);
                    system(command);
                }
            }
            break;
        }
    }    
}