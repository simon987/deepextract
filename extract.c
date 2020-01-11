#include "extract.h"

#include <string.h>
#include <ftw.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <archive.h>
#include <archive_entry.h>
#include <errno.h>

#define ARC_BUF_SIZE 8192

tpool_t *pool;

int Verbose;
int RootLen;
int Flatten;
char *DstPath;

typedef struct vfile vfile_t;

const char *archive_extensions[] = {
        ".iso",
        ".zip",
        ".rar",
        ".ar",
        ".arc",
        ".warc",
        ".7z",
        ".tgz",
        ".tar.gz",
        ".tar.zstd",
        ".tar.xz",
        ".tar.bz",
        ".tar.bz2",
        ".tar.lz4",
        ".tar.lzma",
        ".docx",
        ".pptx",
        ".xlsx",
        ".epub",
        ".cbz",
        ".jar",
        ".deb",
        ".rpm",
        ".xpi",
};

typedef int (*read_func_t)(struct vfile *, void *buf, size_t size);

typedef void (*close_func_t)(struct vfile *);

typedef struct vfile {
    union {
        int fd;
        struct archive *arc;
    };

    int is_fs_file;
    char *filepath;
    struct stat info;
    read_func_t read;
    close_func_t close;
} vfile_t;

typedef struct {
    struct vfile vfile;
    int base;
    char filepath[1];
} job_t;

typedef struct arc_data {
    vfile_t *f;
    char buf[ARC_BUF_SIZE];
} arc_data_f;

int ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return (str_len >= suffix_len) && (memcmp(str + str_len - suffix_len, suffix, sizeof(char) * suffix_len) == 0);
}

int is_archive(char *filepath, int base) {
    for (int i = 0; i < (sizeof(archive_extensions) / sizeof(archive_extensions[0])); i++) {
        if (ends_with(filepath + base, archive_extensions[i])) {
            return TRUE;
        }
    }
    return FALSE;
}


int vfile_open_callback(struct archive *a, void *user_data) {
    arc_data_f *data = user_data;

    if (data->f->is_fs_file && data->f->fd == -1) {
        data->f->fd = open(data->f->filepath, O_RDONLY);
    }

    return ARCHIVE_OK;
}


#define IS_EEXIST_ERR(ret) (ret < 0 && errno == EEXIST)

void copy_or_link(vfile_t *src, const char *dst) {

    char new_name[8192];

    if (src->is_fs_file) {
        int ret = link(src->filepath, dst);

        if (IS_EEXIST_ERR(ret)) {
            int i = 1;
            do {
                sprintf(new_name, "%s.%d", dst, i);
                ret = link(src->filepath, new_name);
                i++;
            } while (IS_EEXIST_ERR(ret));
        }
    } else {
        int ret = open(dst, O_WRONLY | O_CREAT | O_EXCL, src->info.st_mode);

        if (IS_EEXIST_ERR(ret)) {
            int i = 1;
            do {
                sprintf(new_name, "%s.%d", dst, i);
                ret = open(new_name, O_WRONLY | O_CREAT | O_EXCL, src->info.st_mode);
                i++;
            } while (IS_EEXIST_ERR(ret));
        }

        if (ret > 0) {
            int fd = ret;
            char buf[ARC_BUF_SIZE];

            while (1) {
                ret = src->read(src, buf, ARC_BUF_SIZE);

                write(fd, buf, ARC_BUF_SIZE);
                if (ret != ARC_BUF_SIZE) {
                    break;
                }
            }
            close(fd);
        }
    }
}

long vfile_read_callback(struct archive *a, void *user_data, const void **buf) {
    arc_data_f *data = user_data;

    *buf = data->buf;
    return data->f->read(data->f, data->buf, ARC_BUF_SIZE);
}

int vfile_close_callback(struct archive *a, void *user_data) {
    arc_data_f *data = user_data;

    if (data->f->close != NULL) {
        data->f->close(data->f);
    }

    return ARCHIVE_OK;
}

int arc_read(struct vfile *f, void *buf, size_t size) {
    return archive_read_data(f->arc, buf, size);
}


void handle_file(void *arg) {
    job_t *job = arg;

    if (is_archive(job->filepath, job->base)) {
        struct archive *a;
        struct archive_entry *entry;

        arc_data_f data;
        data.f = &job->vfile;

        int ret = 0;
        if (data.f->is_fs_file) {
            a = archive_read_new();
            archive_read_support_filter_all(a);
            archive_read_support_format_all(a);

            ret = archive_read_open_filename(a, job->filepath, ARC_BUF_SIZE);
        } else {
            a = archive_read_new();
            archive_read_support_filter_all(a);
            archive_read_support_format_all(a);

            ret = archive_read_open(
                    a, &data,
                    vfile_open_callback,
                    vfile_read_callback,
                    vfile_close_callback
            );
        }

        if (ret != ARCHIVE_OK) {
            fprintf(stderr, "(arc.c) %s [%d] %s", job->filepath, ret, archive_error_string(a));
            archive_read_free(a);
            return;
        }

        job_t *sub_job = malloc(sizeof(job_t) + 8192);

        sub_job->vfile.close = NULL;
        sub_job->vfile.read = arc_read;
        sub_job->vfile.arc = a;
        sub_job->vfile.filepath = sub_job->filepath;
        sub_job->vfile.is_fs_file = FALSE;

        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            if (S_ISREG(archive_entry_filetype(entry))) {
                sprintf(sub_job->filepath, "%s#/%s", job->vfile.filepath, archive_entry_pathname(entry));
                sub_job->base = (int) (strrchr(sub_job->filepath, '/') - sub_job->filepath) + 1;
                sub_job->vfile.info = *archive_entry_stat(entry);
                handle_file(sub_job);
            }
        }

        free(sub_job);

    } else {
        char *relpath = job->filepath + RootLen;
        char dstpath[8192];
        strcpy(dstpath, DstPath);

        if (Flatten) {
            strcat(dstpath, job->filepath + job->base);
        } else {
            strcat(dstpath, relpath);
        }

        if (Verbose) {
            printf("%s -> %s\n", relpath, dstpath);
        }

        copy_or_link(&job->vfile, dstpath);
    }
}


int fs_read(struct vfile *f, void *buf, size_t size) {

    if (f->fd == -1) {
        f->fd = open(f->filepath, O_RDONLY);
        if (f->fd == -1) {
            return -1;
        }
    }

    return read(f->fd, buf, size);
}

#define CLOSE_FILE(f) if (f.close != NULL) {f.close(&f);};

void fs_close(struct vfile *f) {
    if (f->fd != -1) {
        close(f->fd);
    }
}

job_t *create_fs_job(const char *filepath, int base, struct stat info) {
    int len = (int) strlen(filepath);
    job_t *job = malloc(sizeof(job_t) + len);

    strcpy(job->filepath, filepath);

    job->base = base;

    job->vfile.filepath = job->filepath;
    job->vfile.read = fs_read;
    job->vfile.close = fs_close;
    job->vfile.close = fs_close;
    job->vfile.info = info;
    job->vfile.is_fs_file = 1;
    job->vfile.fd = -1;

    return job;
}

int handle_entry(const char *filepath, const struct stat *info, int typeflag, struct FTW *ftw) {
    if (typeflag == FTW_F && S_ISREG(info->st_mode)) {
        job_t *job = create_fs_job(filepath, ftw->base, *info);
        tpool_add_work(pool, handle_file, job);
    }

    return 0;
}

int walk_directory_tree(const char *dirpath) {
    return nftw(dirpath, handle_entry, 15, FTW_PHYS);
}

int extract(args_t *args) {

    Verbose = args->verbose;
    DstPath = "/home/drone/Documents/test/";
    RootLen = 22;
    Flatten = 1;

    pool = tpool_create(args->threads);
    tpool_start(pool);

    walk_directory_tree("/home/drone/Downloads/");

    tpool_wait(pool);
    tpool_destroy(pool);

    return 0;
}

