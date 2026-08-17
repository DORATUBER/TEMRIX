#include <temrixstd.h>
#include "nvme.hpp"
#include "FileSystem/Ext4/ext4.hpp"
#include "FileSystem/FsClient.hpp"

static uint32_t spawnTRX(Ext4 &fs, const char *path,
                          uint64_t requestedCapabilities = 0,
                          bool requestDeviceGrant = false,
                          uint8_t deviceGrantKind = 0,
                          uint64_t deviceGrantParam = 0)
{
    uint32_t fileSize = fs.fileSize(path);
    if (fileSize == 0)
    {
        String::Print("[init] file not found or empty: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    uint8_t *buf = (uint8_t *)Syscall::Memory::Map(fileSize);
    if (!buf)
    {
        String::Print("[init] out of memory for: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    uint32_t readSize = fileSize;
    if (!fs.readFile(path, buf, &readSize))
    {
        String::Print("[init] failed to read: ");
        String::Print(path);
        String::Print("\n");
        Syscall::Memory::Unmap((uint64_t)buf, fileSize);
        return (uint32_t)-1;
    }

    uint32_t pid = spawnFromBuffer(path, buf, readSize,
                                    nullptr, 0,
                                    nullptr, 0,
                                    requestedCapabilities,
                                    requestDeviceGrant,
                                    deviceGrantKind,
                                    deviceGrantParam);

    Syscall::Memory::Unmap((uint64_t)buf, fileSize);
    return pid;
}

static uint32_t spawnTRXViaFs(FsClient &client, const char *path,
                               uint64_t requestedCapabilities = 0,
                               bool requestDeviceGrant = false,
                               uint8_t deviceGrantKind = 0,
                               uint64_t deviceGrantParam = 0)
{
    uint64_t virt = 0;
    uint32_t len  = 0;

    if (!client.readFile(path, &virt, &len))
    {
        String::Print("[init] failed to read via fs server: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    return spawnFromBuffer(path, reinterpret_cast<uint8_t *>(virt), len,
                            nullptr, 0,
                            nullptr, 0,
                            requestedCapabilities,
                            requestDeviceGrant,
                            deviceGrantKind,
                            deviceGrantParam);
}

static void printHostname(FsClient &client)
{
    uint64_t virt = 0;
    uint32_t len  = 0;

    if (!client.readFile("/etc/hostname", &virt, &len))
    {
        String::Print("[init] failed to read /etc/hostname\n");
        return;
    }

    Syscall::IO::Write(reinterpret_cast<const char *>(virt), len);
    String::Print("\n");
}

static bool parseHex(const char *s, uint64_t &out)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    if (!*s)
        return false;
    uint64_t v = 0;
    while (*s)
    {
        char c = *s;
        uint8_t digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
        else return false;
        v = (v << 4) | digit;
        s++;
    }
    out = v;
    return true;
}

static bool parseGrant(const char *tok, uint8_t &kind, uint64_t &param)
{
    const char *classPrefix  = "class:";
    const char *devicePrefix = "device:";

    uint32_t classLen  = String::Length(classPrefix);
    uint32_t deviceLen = String::Length(devicePrefix);

    if (String::Compare(tok, classPrefix, classLen) == 0)
    {
        kind = (uint8_t)Syscall::Process::DeviceGrantKind::ClassWildcard;
        return parseHex(tok + classLen, param);
    }
    if (String::Compare(tok, devicePrefix, deviceLen) == 0)
    {
        kind = (uint8_t)Syscall::Process::DeviceGrantKind::SpecificDevice;
        return parseHex(tok + deviceLen, param);
    }
    if (String::Compare(tok, "any", 3) == 0 && tok[3] == '\0')
    {
        kind = (uint8_t)Syscall::Process::DeviceGrantKind::AnyDevice;
        param = 0;
        return true;
    }
    return false;
}

static bool parseCapsToken(const char *tok, uint64_t &capsOut)
{
    
    capsOut = 0;
    uint32_t i = 0;
    while (tok[i])
    {
        uint32_t start = i;
        while (tok[i] && tok[i] != ',')
            i++;
        uint32_t nameLen = i - start;

        char name[32];
        if (nameLen >= sizeof(name))
            nameLen = sizeof(name) - 1;
        for (uint32_t j = 0; j < nameLen; j++)
            name[j] = tok[start + j];
        name[nameLen] = '\0';

        if (String::Compare(name, "write", 5) == 0 && name[5] == '\0')
            capsOut |= Syscall::Process::CAP_WRITE;
        else if (String::Compare(name, "pci_msix", 8) == 0 && name[8] == '\0')
            capsOut |= Syscall::Process::CAP_PCI_MSIX;
        else if (String::Compare(name, "irq", 3) == 0 && name[3] == '\0')
            capsOut |= Syscall::Process::CAP_IRQ;
        else if (String::Compare(name, "alloc_vectors", 13) == 0 && name[13] == '\0')
            capsOut |= Syscall::Process::CAP_ALLOC_VECTORS;
        else if (String::Compare(name, "grant", 5) == 0 && name[5] == '\0')
            capsOut |= Syscall::Process::CAP_GRANT;
        else
        {
            String::Print("[init] warning: unrecognized capability: ");
            String::Print(name);
            String::Print("\n");
        }

        if (tok[i] == ',')
            i++;
    }
    return true;
}

static void spawnManifest(FsClient &client)
{
    uint64_t virt = 0;
    uint32_t len  = 0;

    if (!client.readFile("/ext4/etc/init.manifest", &virt, &len))
    {
        String::Print("[init] failed to read /etc/init.manifest\n");
        return;
    }

    const char *data = reinterpret_cast<const char *>(virt);

    char line[FS_MAX_PATH + 64];
    char path[FS_MAX_PATH];

    uint32_t i = 0;
    while (i < len)
    {
        while (i < len && data[i] == '\n')
            i++;

        if (i >= len)
            break;

        uint32_t start = i;
        while (i < len && data[i] != '\n')
            i++;

        uint32_t lineLen = i - start;
        if (lineLen >= sizeof(line))
            lineLen = sizeof(line) - 1;

        for (uint32_t j = 0; j < lineLen; j++)
            line[j] = data[start + j];
        line[lineLen] = '\0';

        
        uint32_t pos = 0;

        uint32_t sp = 0;
        while (line[sp] && line[sp] != ' ' && line[sp] != '\t')
            sp++;

        uint32_t pathLen = sp;
        if (pathLen >= FS_MAX_PATH)
            pathLen = FS_MAX_PATH - 1;
        for (uint32_t j = 0; j < pathLen; j++)
            path[j] = line[j];
        path[pathLen] = '\0';

        pos = sp;

        uint8_t  deviceGrantKind    = 0;
        uint64_t deviceGrantParam   = 0;
        bool     requestDeviceGrant = false;
        uint64_t requestedCaps      = 0;

        while (line[pos])
        {
            while (line[pos] == ' ' || line[pos] == '\t')
                pos++;
            if (!line[pos])
                break;

            uint32_t tokStart = pos;
            while (line[pos] && line[pos] != ' ' && line[pos] != '\t')
                pos++;

            uint32_t tokLen = pos - tokStart;
            char tok[64];
            if (tokLen >= sizeof(tok))
                tokLen = sizeof(tok) - 1;
            for (uint32_t j = 0; j < tokLen; j++)
                tok[j] = line[tokStart + j];
            tok[tokLen] = '\0';

            const char *capsPrefix = "caps:";
            uint32_t capsPrefixLen = String::Length(capsPrefix);

            if (String::Compare(tok, capsPrefix, capsPrefixLen) == 0)
            {
                uint64_t parsed = 0;
                parseCapsToken(tok + capsPrefixLen, parsed);
                requestedCaps |= parsed;
            }
            else
            {
                uint8_t kind = 0;
                uint64_t param = 0;
                if (parseGrant(tok, kind, param))
                {
                    deviceGrantKind = kind;
                    deviceGrantParam = param;
                    requestDeviceGrant = true;
                }
                else
                {
                    String::Print("[init] warning: unrecognized token for ");
                    String::Print(path);
                    String::Print(": ");
                    String::Print(tok);
                    String::Print("\n");
                }
            }
        }

        if (String::Compare(path, "/bin/fs.trx", String::Length("/bin/fs.trx")) == 0)
        {
            String::Print("[init] skipping /bin/fs.trx (already running)\n");
            continue;
        }

        String::Print("[init] manifest entry: ");
        String::Print(path);
        String::Print("\n");

        spawnTRXViaFs(client, path,
                      requestedCaps,
                      requestDeviceGrant,
                      deviceGrantKind,
                      deviceGrantParam);
    }
}

int main(int argc, char **argv)
{
    String::Print("[init] starting\n");

    uint32_t myPid = Syscall::Process::GetId();
    char myPidStr[21];
    Syscall::Service::Publish("init", String::FromU64(myPid, myPidStr));

    uint64_t count = Syscall::Pci::Count();
    if (count == 0)
    {
        String::Print("[init] no PCI devices found\n");
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
        String::Print("[init] no NVMe device found\n");
        return -1;
    }

    uint64_t mmioBase = Syscall::Memory::MapBar(nvmeIndex, 0);
    if (mmioBase == 0)
    {
        String::Print("[init] failed to map BAR0\n");
        return -1;
    }

    NvmeController *ctrl = NvmeController::init(mmioBase);
    if (!ctrl)
    {
        String::Print("[init] NvmeController::init failed\n");
        return -1;
    }

    Ext4 fs;
    if (!fs.init(ctrl))
    {
        String::Print("[init] ext4 init failed\n");
        NvmeController::destroy(ctrl);
        return -1;
    }

    uint32_t fsPid = spawnTRX(fs, "/bin/Fs.trx",
                            Syscall::Process::CAP_WRITE,
                            true,
                            (uint8_t)Syscall::Process::DeviceGrantKind::ClassWildcard,
                            0x01);

    if (fsPid == (uint32_t)-1)
    {
        String::Print("[init] failed to spawn fs server, halting\n");
        NvmeController::destroy(ctrl);
        Syscall::IO::Flush();
        return -1;
    }

    NvmeController::destroy(ctrl);

    FsClient client;
    if (!client.init())
    {
        String::Print("[init] FsClient::init failed\n");
        Syscall::IO::Flush();
        return -1;
    }

    printHostname(client);
    
    spawnManifest(client);

    return 0;
}