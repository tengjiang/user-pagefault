#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <time.h>


#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

static int page_size;
static char *des_page = NULL; // A global page to be copied into the faulting page

static void *
fault_handler_thread(void *arg)
{
    static struct uffd_msg msg;   /* Data read from userfaultfd */
    static int fault_cnt = 0;     /* Number of faults so far handled */
    long uffd;                    /* userfaultfd file descriptor */
    struct uffdio_copy uffdio_copy;
    ssize_t nread;

    uffd = (long) arg;

    /* Loop, handling incoming events on the userfaultfd
        file descriptor */

    for (;;) {

        /* See what poll() tells us about the userfaultfd */

        struct pollfd pollfd;
        int nready;
        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
            errExit("poll");

        // printf("\nfault_handler_thread():\n");
        // printf("    poll() returns: nready = %d; "
        //         "POLLIN = %d; POLLERR = %d\n", nready,
        //         (pollfd.revents & POLLIN) != 0,
        //         (pollfd.revents & POLLERR) != 0);

        /* Read an event from the userfaultfd */

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        }

        if (nread == -1)
            errExit("read");

        /* We expect only one kind of event; verify that assumption */

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        /* Display info about the page-fault event */

        // printf("    UFFD_EVENT_PAGEFAULT event: ");
        // printf("flags = %llx; ", msg.arg.pagefault.flags);
        // printf("address = %llx\n", msg.arg.pagefault.address);

        /* Copy the page pointed to by 'page' into the faulting
            region. Vary the contents that are copied in, so that it
            is more obvious that each fault is handled separately. */

        memset(des_page, 'A' + fault_cnt % 20, page_size); // Simulate process overhead
        fault_cnt++;

        uffdio_copy.src = (unsigned long) des_page;

        /* We need to handle page faults in units of pages(!).
            So, round faulting address down to page boundary */

        uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
                                            ~(page_size - 1);
        uffdio_copy.len = page_size;
        uffdio_copy.mode = 0;
        uffdio_copy.copy = 0;
        if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
            errExit("ioctl-UFFDIO_COPY");

        // printf("        (uffdio_copy.copy returned %lld)\n",
        //         uffdio_copy.copy);
    }
}

int
main(int argc, char *argv[])
{
    clock_t start, end;
    long uffd;          /* userfaultfd file descriptor */
    char *addr;         /* Start of region handled by userfaultfd */
    unsigned long len;  /* Length of region handled by userfaultfd */
    pthread_t thr;      /* ID of thread that handles page faults */
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;
    int s; // for pthread_create
    char *raw_pg_addr; // Without using using userfaultfd
    double cpu_time_used;
    int fault_cnt; // bookkeeping for raw pagefault

    // Get the PID
    printf("fault_handler_thread tgid and pid: %d\n", getpid());

    // Wait
    // getchar();

    if (argc != 2) {
        fprintf(stderr, "Usage: %s num-pages\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    page_size = sysconf(_SC_PAGE_SIZE);
    printf("page size: %d\n", page_size);
    len = strtoul(argv[1], NULL, 0) * page_size;

    /* Create a page that will be copied into the faulting region */

    if (des_page == NULL) {
        des_page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (des_page == MAP_FAILED)
            errExit("mmap");
    }

    // Initialize the allocated memory to zeros
    memset(des_page, 0, page_size);

    /* Create and enable userfaultfd object */

    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1)
        errExit("userfaultfd");

    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
        errExit("ioctl-UFFDIO_API");

    /* Create a private anonymous mapping. The memory will be
        demand-zero paged--that is, not yet allocated. When we
        actually touch the memory, it will be allocated via
        the userfaultfd. */

    addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        errExit("mmap");
    
    raw_pg_addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw_pg_addr == MAP_FAILED)
        errExit("mmap");

    printf("Address returned by mmap() = %p\n", addr);

    /* Register the memory range of the mapping we just created for
        handling by the userfaultfd object. In mode, we request to track
        missing pages (i.e., pages that have not yet been faulted in). */

    uffdio_register.range.start = (unsigned long) addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    /* Create a thread that will process the userfaultfd events */

    s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }

    /* Main thread now touches memory in the mapping, touching
        locations 1024 bytes apart. This will trigger userfaultfd
        events for all pages in the region. */

    
    unsigned long int l;
    l = 0xf;    /* Ensure that faulting address is not on a page
                    boundary, in order to test that we correctly
                    handle that case in fault_handling_thread() */
    start = clock();

    while (l < len) {
        char c = addr[l];
        // printf("Read address %p in main(): ", addr + l);
        // printf("%c\n", c);
        l += 4096;
        // usleep(100000);         /* Slow things down a little */
    }

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken userfaultfd():  %f seconds\n", cpu_time_used);


    l = 0xf;
    fault_cnt = 0;
    // memset(des_page, 0, page_size);

    start = clock();
    while (l < len) {
        memset(raw_pg_addr + fault_cnt * page_size, 'A' + fault_cnt % 20, page_size);
        // printf("%p\n", raw_pg_addr + fault_cnt * page_size);
        fault_cnt++;
        char c = raw_pg_addr[l];
        // printf("Read address %p in main(): ", raw_pg_addr + l);
        // printf("%c\n", c);
        l += 4096;
        // usleep(100000);         /* Slow things down a little */
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken raw page fault:  %f seconds\n", cpu_time_used);

    exit(EXIT_SUCCESS);
}
