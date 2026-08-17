#include <temrixstd/sys/mman.h>
#include <temrixstd/sys/process.h>

extern int main(int argc, char **argv);

extern "C" void _start(int argc, char **argv)
{
    Syscall::Memory::Init();

    int ret = main(argc, argv);

    Syscall::Process::Exit(ret);
}