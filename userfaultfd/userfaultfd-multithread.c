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

typedef struct {
    char * start_addr;
    unsigned long len;
} faulting_thread_args;

static int page_size;
static char *des_page = NULL; // Global page to be copied into the faulting page
pthread_t *faulting_thrs; /* list of faulting thread. */

static void *
faulting_thread(void *arg)
{
    unsigned long int l;
    faulting_thread_args *ft_args = (faulting_thread_args *)arg;
    // printf("Start addr: %p \n", ft_args->start_addr);
    l = 0xf;
    while (l < ft_args->len) {
        char c = ft_args->start_addr[l];
        printf("Read address %p in fault handling thread: ", ft_args->start_addr + l);
        printf("%c\n", c);
        l += page_size;
    }
    pthread_exit(NULL);
}

static void *
faulting_thread_raw(void *arg)
{
    unsigned long int l;
    int fault_cnt = 0; // bookkeeping for raw pagefault
    faulting_thread_args *ft_args = (faulting_thread_args *)arg;
    // printf("Start addr: %p \n", ft_args->start_addr);
    l = 0xf;
    while (l < ft_args->len) {
        memset(ft_args->start_addr + fault_cnt * page_size, 'A' + fault_cnt % 20, page_size);
        char c = ft_args->start_addr[l];
        printf("Read address %p in fault handling thread: ", ft_args->start_addr + l);
        printf("%c\n", c);
        l += page_size;
        fault_cnt += 1;
    }
    pthread_exit(NULL);
}

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

int main(int argc, char *argv[])
{
    clock_t start, end;
    long uffd;          /* userfaultfd file descriptor */
    char *addr;         /* Start of region handled by userfaultfd */
    unsigned long len;  /* Length of region handled by userfaultfd */
    unsigned long per_thread_len;
    pthread_t thr;      /* ID of thread that handles page faults */
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;
    int s; // for pthread_create
    char *raw_pg_addr; // Without using using userfaultfd
    double cpu_time_used;
    unsigned long num_faulting_threads; // # of threads to trigger page fault.
    unsigned long num_pages; // # of pages for each thread.
    faulting_thread_args *all_ft_args;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s num-pages num-fault-triggering-threads\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Get the page size */
    page_size = sysconf(_SC_PAGE_SIZE);
    // printf("page size: %d\n", page_size);

    num_pages = strtoul(argv[1], NULL, 0);
    num_faulting_threads = strtoul(argv[2], NULL, 0);

    len = num_pages * page_size * num_faulting_threads;
    per_thread_len = page_size * num_pages;
    
    printf("%lu faulting threads with each triggering %lu page faults \n",
        num_faulting_threads, num_pages);

    /* Create a page that will be copied into the faulting region */

    if (des_page == NULL) {
        des_page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (des_page == MAP_FAILED)
            errExit("mmap");
    }

    /* Initialize the allocated memory to all 'a', avoid using zero pages */
    memset(des_page, 'a', page_size);

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
    
    /* for raw page fault */
    raw_pg_addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw_pg_addr == MAP_FAILED)
        errExit("mmap");

    // printf("Address returned by mmap() = %p\n", addr);

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
        errExit("pthread_create for fault handling thread");
    }

    /* Each fault handling thread will try to trigger num_pages pages starting
        with addr[thread_idx * num_pages * page_size]. This will trigger
        userfaultfd events for all pages in the region. */

    /* Allocate memory for faulting threads */
    faulting_thrs = (pthread_t *)malloc(num_faulting_threads * sizeof(pthread_t));
    all_ft_args = (void *)malloc(num_faulting_threads * sizeof(faulting_thread_args));

    start = clock();

    for (unsigned long idx = 0; idx < num_faulting_threads; idx++) {
        (all_ft_args + idx)->len = per_thread_len;
        (all_ft_args + idx)->start_addr = addr + per_thread_len * idx;
        printf("The args: idx: %lu, start addr %p, len %lu\n", idx, (all_ft_args + idx)->start_addr, (all_ft_args + idx)->len);
        s = pthread_create(faulting_thrs + idx, NULL, faulting_thread, all_ft_args + idx);
        if (s != 0) {
            errno = s;
            free(faulting_thrs);
            free(all_ft_args);
            errExit("pthread_create for faulting thread");
        }
    }

    for (unsigned long idx = 0; idx < num_faulting_threads; idx++) {
        s = pthread_join(*(faulting_thrs + idx), NULL);
        if (s != 0) {
            errno = s;
            free(faulting_thrs);
            free(all_ft_args);
            errExit("pthread_join for faulting thread");
        }
    }

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken with %lu faulting thread:  %f seconds\n", num_faulting_threads, cpu_time_used);

    // Default page faulting
    
    start = clock();

    for (unsigned long idx = 0; idx < num_faulting_threads; idx++) {
        (all_ft_args + idx)->len = per_thread_len;
        (all_ft_args + idx)->start_addr = raw_pg_addr + per_thread_len * idx;
        printf("The args: idx: %lu, start addr %p, len %lu\n", idx, (all_ft_args + idx)->start_addr, (all_ft_args + idx)->len);
        s = pthread_create(faulting_thrs + idx, NULL, faulting_thread_raw, all_ft_args + idx);
        if (s != 0) {
            errno = s;
            free(faulting_thrs);
            free(all_ft_args);
            errExit("pthread_create for faulting thread");
        }
    }

    for (unsigned long idx = 0; idx < num_faulting_threads; idx++) {
        s = pthread_join(*(faulting_thrs + idx), NULL);
        if (s != 0) {
            errno = s;
            free(faulting_thrs);
            free(all_ft_args);
            errExit("pthread_join for faulting thread");
        }
    }

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken with raw page fault:  %f seconds\n", cpu_time_used);

    free(faulting_thrs);
    free(all_ft_args);
    exit(EXIT_SUCCESS);
}
