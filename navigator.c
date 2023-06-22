#define _DEFAULT_SOURCE

#include <ncurses.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

typedef struct File {
    char name[20], type[10];
} File;

typedef struct Directory {
    char *path;
    File *files;
    size_t len, cap, cursor;
} Directory;

int compare(const void *fst, const void *snd) {
    File fst_file = *(File *) fst;
    File snd_file = *(File *) snd;

    fst_file.name[0] = tolower(fst_file.name[0]);
    snd_file.name[0] = tolower(snd_file.name[0]);

    return strcmp(fst_file.name, snd_file.name);
}

Directory *directory_init(char *path) {
    Directory *dir = malloc(sizeof(Directory));
    dir->path = path;
    dir->cap = 10;
    dir->len = 0;
    dir->cursor = 0;
    dir->files = malloc(dir->cap * sizeof(File));

    DIR *temp_dir = opendir(path);
    if (temp_dir == NULL) return NULL;

    struct dirent *entry;
    File file;
    while (entry = readdir(temp_dir)) {
        if (entry->d_name[0] == '.' && strcmp(entry->d_name, "..") != 0) continue;

        strncpy(file.name, entry->d_name, 20);
        switch (entry->d_type) {
            case DT_REG: strncpy(file.type, "File", 10); break;
            case DT_DIR: strncpy(file.type, "Directory", 10); break;
        }

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

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);

    char path[256];
    sprintf(path, "/home/%s", getlogin());

    size_t width = getmaxx(stdscr);
    size_t height = getmaxy(stdscr);

    Directory *dir = directory_init(path);
    bool dir_changed = false;
    char input;

    while (true) {
        clear();
        mvprintw(height - 1, 0, "[Sakis Navigator] Path: %s, Cursor: %2d", path, dir->cursor);

        size_t start = 0;
        if (dir->cursor >= height - 1) {
            start = dir->cursor - height + 2;
        }

        for (size_t i = 0; i < height - 1; i++) {
            move(i, 0);

            if (i == dir->cursor || (start > 0 && i == height - 2)) {
                attron(COLOR_PAIR(1));
            }

            printw("[%2d] %-20s %-10s", start + i, dir->files[start + i].name, dir->files[start + i].type);
            
            if (i == dir->cursor || (start > 0 && i == height - 2)) {
                attroff(COLOR_PAIR(1));
            }

            if (i == dir->len - 1) break;
        }
        refresh();

        move(dir->cursor, 0);
        input = getch();

        switch (input) {
            case 'q': case 'Q': {
                directory_free(dir);
                refresh();
                endwin();
                return 0;
            }
            case 'w': case 'W': {
                if (dir->cursor != 0) {
                    dir->cursor--;
                }
            }
            break;
            case 's': case 'S': {
                if (dir->cursor != dir->len - 1) {
                    dir->cursor++;
                }
            }
            break;
            case '\n': {
                if (strcmp(dir->files[dir->cursor].type, "Directory") == 0) {
                    strcat(path, "/");
                    strcat(path, dir->files[dir->cursor].name);
                    directory_free(dir);
                    dir = directory_init(path);
                } else if (strcmp(dir->files[dir->cursor].type, "File") == 0) {
                    char command[256];
                    sprintf(command, "code %s/%s", path, dir->files[dir->cursor].name);
                    system(command);
                }
            }
            break;
        }
    }    
}