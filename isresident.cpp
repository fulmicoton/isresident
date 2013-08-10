#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string>
#include <iostream>
#include <algorithm>

using namespace std;

struct InMem {
    InMem()
    :filesize(-1)
    ,size_in_mem(-1) {}

    InMem(long filesize_, long size_in_mem_)
    :filesize(filesize_)
    ,size_in_mem(size_in_mem_) {}


    int percentage() {
        return (size_in_mem * 100) / filesize;  
    }

    long filesize;
    long size_in_mem;
};

class InMemScanner {

public:
    InMemScanner() {
        page_size = sysconf(_SC_PAGESIZE);
        round_size = page_size*1000;
        buffer = new unsigned char[nb_pages(round_size)];
    }


    unsigned int nb_pages(size_t length) {
        return (length+page_size-1) / page_size;
    }

    ~InMemScanner() {
        delete[] buffer;
    }

    InMem scan(const char* filepath /*, const char* inmem*/) {
        struct stat file_stat;
        // fetch the file size
        int status = stat(filepath, &file_stat);
        if (status != 0) {
            return InMem();
        }
        const size_t file_size = file_stat.st_size;
        int fd = open(filepath, O_RDONLY);
        // mmap our file entirely
        // that's just virtual memory, we won't access
        // it anyway
        char* data = (char*)mmap(0, file_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        size_t remaining_size = file_size;
        long page_in_mem = 0;
        while (remaining_size > 0) {
            const size_t check_size = min(remaining_size,round_size);
            remaining_size -= check_size;
            unsigned int page_checked = nb_pages(check_size);
            mincore(data, check_size, buffer);
            for (int i = 0; i < page_checked; i++) {
                page_in_mem += buffer[i];
            }
        }
        munmap(data, file_size);
        long size_in_mem = min(file_size, (size_t)(page_in_mem * page_size));
        return InMem(file_size, size_in_mem);
    }

private:
    InMemScanner(const InMemScanner&);
    size_t round_size;
    long page_size;
    unsigned char* buffer;
};

int main(int argc, char *argv[]) 
{
    InMemScanner scanner;
    if (argc == 1) {
        cout << "Usage:" << endl;
        cout << "  ./isresident <file>" << endl;
        cout << "  ./isresident ./*" << endl;
    }
    else {
        cout.width(45);
        cout << "FILE" << "\t";
        cout << "RSS" << "\t";
        cout << "SIZE" << "\t";
        cout << "PERCT" << "\t" << endl;
    }
    for (int i=1; i< argc; i++) {
        InMem in_mem = scanner.scan(argv[i]);
        cout.width(45);
        cout << argv[i] << "\t";
        cout << (in_mem.size_in_mem >> 10 )<< "\t";
        cout << (in_mem.filesize >> 10) << "\t";
        cout << in_mem.percentage() << " %" << endl;
    }
    return 0;
}