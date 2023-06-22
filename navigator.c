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

char *path_init() {
    char *username = getlogin();
    char *path = calloc(strlen("/home/") + strlen(username) + 1, sizeof(char));
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
    dir->files = malloc(dir->cap * sizeof(File));

    DIR *temp_dir = opendir(path);
    if (temp_dir == NULL) return NULL;

    struct dirent *entry;
    File file;
    while (entry = readdir(temp_dir)) {
        if (entry->d_name[0] == '.') {
            if (strcmp(path, "/") == 0) continue;
            if (strcmp(entry->d_name, "..") != 0) continue;
        }

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

    char *path = path_init();

    size_t width = getmaxx(stdscr);
    size_t height = getmaxy(stdscr);

    Directory *dir = directory_init(path);
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
                free(path);
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
                File *file = &dir->files[dir->cursor];
                if (strcmp(file->type, "Directory") == 0) {
                    path = path_update(path, file->name);
                    directory_free(dir);
                    dir = directory_init(path);
                } else if (strcmp(file->type, "File") == 0) {
                    char command[256];
                    sprintf(command, "code %s/%s", path, file->name);
                    system(command);
                }
            }
            break;
        }
    }    
}