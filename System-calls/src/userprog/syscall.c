#include "userprog/syscall.h"


#define USER_ASSERT(CONDITION) \
    if (CONDITION)             \
    {                          \
    }                          \
    else                       \
    {                          \
        exit(-1);              \
    }





void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");

    lock_init(&file_lock);
}

static void syscall_handler(struct intr_frame *f)
{
    USER_ASSERT(is_user_mem(f->esp, sizeof(void *)));

    void *args[4];
    for (size_t i = 0; i != 4; ++i)
        args[i] = f->esp + i * sizeof(void *);

    int syscall_num = *(int *)args[0];

    /* Check validation. */
    switch (syscall_num)
    {
    case SYS_READ:
    case SYS_WRITE:
        USER_ASSERT(is_user_mem(args[3], sizeof(void *)));
    case SYS_CREATE:
    case SYS_SEEK:
        USER_ASSERT(is_user_mem(args[2], sizeof(void *)));
    case SYS_EXIT:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE:
        USER_ASSERT(is_user_mem(args[1], sizeof(void *)));
    case SYS_HALT:
        break;
    default:
        NOT_REACHED();
    }

    /* Forward. */
    switch (syscall_num)
    {
    case SYS_HALT:
        halt();
        NOT_REACHED();
    case SYS_EXIT:
        exit(*(int *)args[1]);
        NOT_REACHED();
    case SYS_EXEC:
        f->eax = exec(*(const char **)args[1]);
        break;
    case SYS_WAIT:
        f->eax = wait(*(pid_t *)args[1]);
        break;
    case SYS_CREATE:
        f->eax = create(*(const char **)args[1], *(unsigned *)args[2]);
        break;
    case SYS_REMOVE:
        f->eax = remove(*(const char **)args[1]);
        break;
    case SYS_OPEN:
        f->eax = open(*(const char **)args[1]);
        break;
    case SYS_FILESIZE:
        f->eax = filesize(*(int *)args[1]);
        break;
    case SYS_READ:
        f->eax = read(*(int *)args[1], *(void **)args[2], *(unsigned *)args[3]);
        break;
    case SYS_WRITE:
        f->eax = write(*(int *)args[1], *(const void **)args[2], *(unsigned *)args[3]);
        break;
    case SYS_SEEK:
        seek(*(int *)args[1], *(unsigned *)args[2]);
        break;
    case SYS_TELL:
        f->eax = tell(*(int *)args[1]);
        break;
    case SYS_CLOSE:
        close(*(int *)args[1]);
        break;
    default:
        NOT_REACHED();
    }
}

/* Returns true if PTR is not a null pointer,
    a pointer to kernel virtual address space
    or a pointer to unmapped virtual memory. */
static bool is_valid_ptr(const void *ptr)
{
    return ptr != NULL && is_user_vaddr(ptr) && pagedir_get_page(thread_current()->pagedir, ptr) != NULL;
}

/* Returns true if [START, START + SIZE) is all valid. */
static bool is_user_mem(const void *start, size_t size)
{
    for (const void *ptr = start; ptr < start + size; ptr += PGSIZE)
    {
        if (!is_valid_ptr(ptr))
            return false;
    }

    if (size > 1 && !is_valid_ptr(start + size - 1))
        return false;

    return true;
}

/* Returns true if STR is a valid string in user space. */
static bool is_valid_str(const char *str)
{
    if (!is_valid_ptr(str))
        return false;

    for (const char *c = str; *c != '\0';)
    {
        ++c;
        if (c - str + 2 == PGSIZE || !is_valid_ptr(c))
            return false;
    }

    return true;
}

static struct open_file *get_file_by_fd(const int fd)
{
    struct list *l = &thread_current()->process->files;

    for (struct list_elem *e = list_begin(l); e != list_end(l); e = list_next(e))
    {
        struct open_file *f = list_entry(e, struct open_file, elem);
        if (f->fd == fd)
            return f;
    }

    USER_ASSERT(false);
}

/* Terminates the current user program, returning
    STATUS to the kernel. If the process’s parent
    waits for it, this is the status that will be
    returned. Conventionally, a status of 0 indicates
    success and nonzero values indicate errors. 
    
    -tại sao nó lại đc gọi là systerm call: một tiến trình 
    máy tính kết thúc quá trình thực thi của nó bằng cách thực 
    hiện lệnh gọi thoát hệ thống nhằm thông báo cho kenel 
    Để quản lý tài nguyên , hệ điều hành thu hồi các tài nguyên ( bộ nhớ , tệp , v.v.) 
    đã được sử dụng bởi tiến trình. */
static void exit(int status)
{
    struct process *self = thread_current()->process;//tiến trình hiện tại đang chạy

    while (!list_empty(&self->files))
    {
        struct open_file *f = list_entry(list_back(&self->files),
                                         struct open_file, elem);
        close(f->fd);
    }

    self->exit_code = status;
    thread_exit();
}

/* Writes SIZE bytes from buffer to the open file FD. Returns the
    number of bytes actually written, which may be less than SIZE if
    some bytes could not be written.

    Writing past end-of-file would normally extend the file, but file
    growth is not implemented by the basic file system. The expected
    behavior is to write as many bytes as possible up to end-of-file
    and return the actual number written, or 0 if no bytes could be
    written at all.

    Fd 1 writes to the console. The code to write to the console should
    write all of buffer in one call to putbuf(), at least as long as SIZE
    is not bigger than a few hundred bytes. (It is reasonable to break up
    larger buffers.) Otherwise, lines of text output by different processes
    may end up interleaved on the console, confusing both human readers and
    the grading scripts. */
static int write(int fd, const void *buffer, unsigned size)
{
    USER_ASSERT(is_user_mem(buffer, size));
    USER_ASSERT(fd != STDIN_FILENO);

    if (fd == STDOUT_FILENO)
    {
        putbuf((const char *)buffer, size);
        return size;
    }

    struct open_file *f = get_file_by_fd(fd);

    lock_acquire(&file_lock);
    int ret = file_write(f->file, buffer, size);
    lock_release(&file_lock);

    return ret;
}

/* Runs the executable whose name is given in CMD_LINE,
    passing any given arguments, and returns the new
    process’s program id (pid). Must return pid -1,
    which otherwise should not be a valid pid, if
    the program cannot load or run for any reason.
    Thus, the parent process cannot return from the
    exec until it knows whether the child process
    successfully loaded its executable. Use appropriate
    synchronization to ensure this. 
    
    */
static pid_t exec(const char *cmd_line)
{
    USER_ASSERT(is_valid_str(cmd_line));

    lock_acquire(&file_lock);
    pid_t pid = process_execute(cmd_line);
    lock_release(&file_lock);

    if (pid == TID_ERROR)
        return -1;

    struct process *child = get_child(pid);
    sema_down(&child->sema_load);

    if (child->status == PROCESS_FAILED)
    {
        sema_down(&child->sema_wait);
        palloc_free_page(child);
        return -1;
    }
    else
    {
        ASSERT(child->status == PROCESS_NORMAL);
        return pid;
    }
}

/* Waits for a child process PID and retrieves the
    child’s exit status.

    If PID is still alive, waits until it terminates.
    Then, returns the status that PID passed to exit.
    If PID did not call exit(), but was terminated by
    the kernel (e.g. killed due to an exception), wait(PID)
    must return -1. It is perfectly legal for a parent
    process to wait for child processes that have already
    terminated by the time the parent calls wait, but the
    kernel must still allow the parent to retrieve its
    child’s exit status, or learn that the child was
    terminated by the kernel.

    wait must fail and return -1 immediately if any of the
    following conditions is true:

    a. PID does not refer to a direct child of the calling
    process. PID is a direct child of the calling process if
    and only if the calling process received PID as a return
    value from a successful call to exec. Note that children
    are not inherited: if A spawns child B and B spawns child
    process C, then A cannot wait for C, even if B is dead.
    A call to wait(C) by process A must fail. Similarly, orphaned
    processes are not assigned to a new parent if their parent
    process exits before they do.

    b. The process that calls wait has already called wait on PID.
    That is, a process may wait for any given child at most once.

    Processes may spawn any number of children, wait for them in
    any order, and may even exit without having waited for some or
    all of their children. The design should consider all the ways
    in which waits can occur. All of a process’s resources, including
    its struct thread, must be freed whether its parent ever waits
    for it or not, and regardless of whether the child exits before
    or after its parent.

    Must ensure that Pintos does not terminate until the initial
    process exits. The supplied Pintos code tries to do this by
    calling process_wait() (in‘userprog/process.c’) from main()
    (in ‘threads/init.c’). Implement process_wait() according to the
    comment at the top of the function and then implement the wait
    system call in terms of process_wait(). */
static int wait(pid_t pid)
{
    return process_wait(pid);
}

/* Terminates Pintos by calling shutdown_power_off()
    (declaredin‘devices/shutdown.h’). This should be
    seldom used, because you losesome information
    about possible deadlock situations, etc. */
static void halt(void)
{
    shutdown_power_off();
}

/* Creates a new file called FILE initially INITIAL_SIZE bytes in size.
    Returns true if successful, false otherwise. Creating a new file
    does not open it: opening the new file is a separate operation which
    would require a open system call. */
static bool create(const char *file, unsigned initial_size)
{
    USER_ASSERT(is_valid_str(file));

    lock_acquire(&file_lock);
    bool ret = filesys_create(file, initial_size);
    lock_release(&file_lock);

    return ret;
}

/* Deletes the file called FILE. Returns true if successful, false
    otherwise. A file may be removed regardless of whether it is open
    or closed, and removing an open file does not close it. */
static bool remove(const char *file)
{
    USER_ASSERT(is_valid_str(file));

    lock_acquire(&file_lock);
    bool ret = filesys_remove(file);
    lock_release(&file_lock);

    return ret;
}

/* Opens the file called FILE. Returns a nonnegative integer handle
    called a “file descriptor” (fd), or -1 if the file could not be
    opened.

    File descriptors numbered 0 and 1 are reserved for the console:
    fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is
    standard output. The open system call will never return either of
    these file descriptors, which are valid as system call arguments
    only.

    Each process has an independent set of file descriptors. File
    descriptors are not inherited by child processes.

    When a single file is opened more than once, whether by a single
    process or different processes, each open returns a new file
    descriptor. Different file descriptors for a single file are closed
    independently in separate calls to close and they do not share a
    file position. */
static int open(const char *file)
{
    USER_ASSERT(is_valid_str(file));

    lock_acquire(&file_lock);
    struct file *f = filesys_open(file);
    lock_release(&file_lock);

    if (f == NULL)
        return -1;

    struct process *self = thread_current()->process;

    struct open_file *open_file = malloc(sizeof(struct open_file));
    open_file->fd = self->fd++;
    open_file->file = f;
    list_push_back(&self->files, &open_file->elem);

    return open_file->fd;
}

/* Returns the size, in bytes, of the file open as FD. */
static int filesize(int fd)
{
    struct open_file *f = get_file_by_fd(fd);

    lock_acquire(&file_lock);
    int ret = file_length(f->file);
    lock_release(&file_lock);

    return ret;
}

/* Reads SIZE bytes from the file open as FD into buffer. Returns
    the number of bytes actually read (0 at end of file), or -1 if
    the file could not be read (due to a condition other than end
    of file).

    Fd 0 reads from the keyboard using input_getc(). */
static int read(int fd, void *buffer, unsigned size)
{
    USER_ASSERT(is_user_mem(buffer, size));
    USER_ASSERT(fd != STDOUT_FILENO);

    if (fd == STDIN_FILENO)
    {
        uint8_t *c = buffer;
        for (unsigned i = 0; i != size; ++i)
            *c++ = input_getc();
        return size;
    }

    struct open_file *f = get_file_by_fd(fd);

    lock_acquire(&file_lock);
    int ret = file_read(f->file, buffer, size);
    lock_release(&file_lock);

    return ret;
}

/* Changes the next byte to be read or written in open file FD to POSITION,
    expressed in bytes from the beginning of the file. (Thus, a position of
    0 is the file’s start.)

    A seek past the current end of a file is not an error. A later read
    obtains 0 bytes,indicating end of file. A later write extends the file,
    filling any unwritten gap with zeros. (However, in  Pintos files have a
    fixed length until project 4 is complete, so writes past end of file
    will return an error.) These semantics are implemented in the file
    system and do not require any special effort in system call implementation. */
static void seek(int fd, unsigned position)
{
    struct open_file *f = get_file_by_fd(fd);

    lock_acquire(&file_lock);
    file_seek(f->file, position);
    lock_release(&file_lock);
}

/* Returns the position of the next byte to be read or written in open
    file FD, expressed in bytes from the beginning of the file. */
static unsigned tell(int fd)
{
    struct open_file *f = get_file_by_fd(fd);

    lock_acquire(&file_lock);
    int ret = file_tell(f->file);
    lock_release(&file_lock);

    return ret;
}

/* Closes file descriptor FD. Exiting or terminating a process implicitly
    closes all its open file descriptors, as if by calling this function
    for each one. */
static void close(int fd)
{
    struct open_file *f = get_file_by_fd(fd);

    lock_acquire(&file_lock);
    file_close(f->file);
    lock_release(&file_lock);

    list_remove(&f->elem);
    free(f);
}
