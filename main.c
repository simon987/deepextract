#include <stdio.h>
#include <archive.h>

int main() {
    struct archive *a = archive_read_new();
    archive_read_disk_open(a, "");
    printf("Hello, World!\n");
    return 0;
}
