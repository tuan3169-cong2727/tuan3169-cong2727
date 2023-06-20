#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/stdio.h"
#include "lib/kernel/stdio.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
static struct open_file
{
    int fd;
    struct file *file;
    struct list_elem elem;
};

static void syscall_handler(struct intr_frame *);
static bool is_valid_ptr(const void *ptr);
static bool is_user_mem(const void *start, size_t size);
static bool is_valid_str(const char *str);
static struct open_file *get_file_by_fd(const int fd);

static void halt(void) NO_RETURN;
static void exit(int status) NO_RETURN;
static pid_t exec(const char *file);
static int wait(pid_t);
static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);
static int open(const char *file);
static int filesize(int fd);
static int read(int fd, void *buffer, unsigned length);
static int write(int fd, const void *buffer, unsigned length);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);
void syscall_init(void);

static struct lock file_lock;

#endif /* userprog/syscall.h */
