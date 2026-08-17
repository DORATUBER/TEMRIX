#include <temrixstd.h>
#include "nvme.hpp"
#include "FileSystem/Ext4/ext4.hpp"
#include "FileSystem/Ext4Backend.hpp"
#include "FileSystem/TemrixFs/trxfs.hpp"
#include  "FileSystem/TemrixFsBackend.hpp"
#include "FileSystem/FsServer.hpp"

int main(int argc, char **argv)
{
    String::Print("[fs] starting\n");

    uint64_t count = Syscall::Pci::Count();
    if (count == 0)
    {
        String::Print("[fs] no PCI devices found\n");
        return -1;
    }

    Syscall::Pci::KernelDevice devices[64];
    uint64_t fetched = Syscall::Pci::GetDevices(devices, count);

    uint64_t nvmeIndex = (uint64_t)-1;
    for (uint64_t i = 0; i < fetched; i++)
    {
        if (devices[i].classCode == 0x01 && devices[i].subclass == 0x08)
        {
            nvmeIndex = i;
            break;
        }
    }

    if (nvmeIndex == (uint64_t)-1)
    {
        String::Print("[fs] no NVMe device found\n");
        return -1;
    }

    uint64_t mmioBase = Syscall::Memory::MapBar(nvmeIndex, 0);
    if (mmioBase == 0)
    {
        String::Print("[fs] failed to map BAR0\n");
        return -1;
    }

    String::Print("[fs] mapping BAR0 and re-initializing NVMe controller...\n");

    NvmeController *ctrl = NvmeController::init(mmioBase);
    if (!ctrl)
    {
        String::Print("[fs] NvmeController::init failed\n");
        return -1;
    }

    String::Print("[fs] NVMe controller init OK\n");

    Ext4 ext4Fs;
    if (!ext4Fs.init(ctrl))
    {
        String::Print("[fs] ext4 init failed\n");
        NvmeController::destroy(ctrl);
        return -1;
    }
    String::Print("[fs] ext4 init OK\n");

    TemrixFs trxFs;
    bool haveTrx = trxFs.init(ctrl);
    if (!haveTrx)
        String::Print("[fs] TemrixFs: no TEMRIX partition found - / will be ext4-backed\n");

    Ext4Backend ext4Backend(ext4Fs);
    TemrixFsBackend trxBackend(trxFs);

    Vfs vfs;
    if (haveTrx)
    {
        vfs.mount("/", trxBackend.ref());
        vfs.mount("/ext4", ext4Backend.ref());
    }
    else
    {
        vfs.mount("/", ext4Backend.ref()); 
    }

    FsServer server(vfs);

    if (!server.init())
    {
        String::Print("[fs] FsServer::init failed\n");
        NvmeController::destroy(ctrl);
        return -1;
    }

    String::Print("[fs] registry ready, serving requests\n");

    Syscall::IO::Flush();

    while (true)
    {
        server.poll();
        Syscall::Process::Wait();
    }
    return 0;
}