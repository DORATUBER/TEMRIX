#pragma once

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef uint64_t uintptr_t;
typedef uint64_t EfiStatus;
typedef void* EfiHandle;

enum : EfiStatus
{
    EfiSuccess         = 0x0000000000000000ULL,
    EfiLoadError       = 0x8000000000000001ULL,
    EfiInvalidParameter= 0x8000000000000002ULL,
    EfiUnsupported     = 0x8000000000000003ULL,
    EfiBadBufferSize   = 0x8000000000000004ULL,
    EfiBufferTooSmall  = 0x8000000000000005ULL,
    EfiNotReady        = 0x8000000000000006ULL,
    EfiDeviceError     = 0x8000000000000007ULL,
    EfiWriteProtected  = 0x8000000000000008ULL,
    EfiOutOfResources  = 0x8000000000000009ULL,
    EfiVolumeCorrupted = 0x800000000000000AULL,
    EfiVolumeFull      = 0x800000000000000BULL,
    EfiNoMedia         = 0x800000000000000CULL,
    EfiMediaChanged    = 0x800000000000000DULL,
    EfiNotFound        = 0x800000000000000EULL,
    EfiAccessDenied    = 0x800000000000000FULL,
    EfiNoResponse      = 0x8000000000000010ULL,
    EfiNoMapping       = 0x8000000000000011ULL,
    EfiTimeout         = 0x8000000000000012ULL,
    EfiNotStarted      = 0x8000000000000013ULL,
    EfiAlreadyStarted  = 0x8000000000000014ULL,
    EfiAborted         = 0x8000000000000015ULL,
    EfiIcmpError       = 0x8000000000000016ULL,
    EfiTftpError       = 0x8000000000000017ULL,
    EfiProtocolError   = 0x8000000000000018ULL
};

enum : uint64_t
{
    EfiBlack         = 0x00,
    EfiBlue          = 0x01,
    EfiGreen         = 0x02,
    EfiCyan          = 0x03,
    EfiRed           = 0x04,
    EfiMagenta       = 0x05,
    EfiBrown         = 0x06,
    EfiLightGray     = 0x07,
    EfiDarkGray      = 0x08,
    EfiLightBlue     = 0x09,
    EfiLightGreen    = 0x0A,
    EfiLightCyan     = 0x0B,
    EfiLightRed      = 0x0C,
    EfiLightMagenta  = 0x0D,
    EfiYellow        = 0x0E,
    EfiWhite         = 0x0F
};

enum : uint16_t
{
    EfiScanNull       = 0x00,
    EfiScanUp         = 0x01,
    EfiScanDown       = 0x02,
    EfiScanRight      = 0x03,
    EfiScanLeft       = 0x04,
    EfiScanHome       = 0x05,
    EfiScanEnd        = 0x06,
    EfiScanInsert     = 0x07,
    EfiScanDelete     = 0x08,
    EfiScanPageUp     = 0x09,
    EfiScanPageDown   = 0x0A,
    EfiScanF1         = 0x0B,
    EfiScanF2         = 0x0C,
    EfiScanF3         = 0x0D,
    EfiScanF4         = 0x0E,
    EfiScanF5         = 0x0F,
    EfiScanF6         = 0x10,
    EfiScanF7         = 0x11,
    EfiScanF8         = 0x12,
    EfiScanF9         = 0x13,
    EfiScanF10        = 0x14,
    EfiScanEscape     = 0x17
};

struct EfiGuid
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};

static const EfiGuid GopGuid = {
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

static const EfiGuid LoadedImageGuid = {0x5B1B31A1, 0x9562, 0x11D2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};
static const EfiGuid SfGuid = {0x964E5B22, 0x6459, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};

static const EfiGuid Acpi20Guid = {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
static const EfiGuid Acpi10Guid = {0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};

struct EfiConfigurationTable
{
    EfiGuid VendorGuid;
    void *VendorTable;
};

#define EfiScanEscape 0x0017

struct EfiInputKey
{
    uint16_t ScanCode;
    uint16_t UnicodeChar;
};

struct EfiSimpleTextInputProtocol
{
    EfiStatus (*Reset)(EfiSimpleTextInputProtocol *, uint8_t ExtendedVerification);
    EfiStatus (*ReadKeyStroke)(EfiSimpleTextInputProtocol *, EfiInputKey *Key);
    void *WaitForKey;
};

struct EfiSimpleTextOutputProtocol
{
    void *Reset;
    EfiStatus (*OutputString)(EfiSimpleTextOutputProtocol *, const uint16_t *);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    EfiStatus (*SetAttribute)(EfiSimpleTextOutputProtocol *, uint64_t Attribute);
    EfiStatus (*ClearScreen)(EfiSimpleTextOutputProtocol *);
};

struct EfiFileProtocol
{
    uint64_t _pad;
    EfiStatus (*Open)(EfiFileProtocol *, EfiFileProtocol **, uint16_t *, uint64_t, uint64_t);
    EfiStatus (*Close)(EfiFileProtocol *);
    void *Delete;
    EfiStatus (*Read)(EfiFileProtocol *, uint64_t *, void *);
    void *Write;
    void *GetPosition;
    EfiStatus (*SetPosition)(EfiFileProtocol *, uint64_t);
    EfiStatus (*GetInfo)(EfiFileProtocol *, const EfiGuid *, uint64_t *, void *);
};

struct EfiFileInfo
{
    uint64_t Size;
    uint64_t FileSize;
    uint64_t PhysicalSize;
    uint8_t CreateTime[16];
    uint8_t LastAccessTime[16];
    uint8_t ModificationTime[16];
    uint64_t Attribute;
    uint16_t FileName[256];
};

static const EfiGuid FILE_INFO_GUID = {
    0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

struct EfiSimpleFileSystemProtocol
{
    uint64_t Revision;
    EfiStatus (*OpenVolume)(EfiSimpleFileSystemProtocol *, EfiFileProtocol **);
};

struct LoadedFile {
    uint64_t physAddr;
    uint64_t size;
};

struct BootInfo {
    uint32_t* Framebuffer;
    uint32_t Width;
    uint32_t Height;
    uint32_t PixelsPerScanLine;
    void* MemoryMap;
    uint32_t MemoryMapSize;
    uint32_t DescriptorSize;
    void* RSDP;
    uint64_t PML4;
    uint64_t PTPoolBase;
    uint64_t PTPoolPages;
    uint64_t trampolineAddr;
    LoadedFile initProcess;
};

struct EfiBootServices
{
    uint8_t Hdr[24];

    EfiStatus (*RaiseTPL)(uint64_t NewTPL);
    EfiStatus (*RestoreTPL)(uint64_t OldTPL);

    EfiStatus (*AllocatePages)(
        uint32_t Type,
        uint32_t MemoryType,
        uint64_t Pages,
        uint64_t *Memory);

    EfiStatus (*FreePages)(
        uint64_t Memory,
        uint64_t Pages);

    EfiStatus (*GetMemoryMap)(
        uintptr_t *MemoryMapSize,
        void *MemoryMap,
        uintptr_t *MapKey,
        uintptr_t *DescriptorSize,
        uint32_t *DescriptorVersion);

    EfiStatus (*AllocatePool)(
        uint32_t PoolType,
        uintptr_t Size,
        void **Buffer);

    EfiStatus (*FreePool)(
        void *Buffer);

    EfiStatus (*CreateEvent)(
        uint32_t Type,
        uint64_t NotifyTpl,
        void (*NotifyFunction)(void *Event, void *Context),
        void *NotifyContext,
        void **Event);

    EfiStatus (*SetTimer)(
        void *Event,
        uint32_t Type,
        uint64_t TriggerTime);

    EfiStatus (*WaitForEvent)(
        uint64_t NumberOfEvents,
        void **Event,
        uint64_t *Index);

    EfiStatus (*SignalEvent)(void *Event);
    EfiStatus (*CloseEvent)(void *Event);
    EfiStatus (*CheckEvent)(void *Event);

    EfiStatus (*InstallProtocolInterface)(
        void **Handle,
        const EfiGuid *Protocol,
        void *InterfaceType,
        void *Interface);

    EfiStatus (*ReinstallProtocolInterface)(
        void *Handle,
        const EfiGuid *Protocol,
        void *OldInterface,
        void *NewInterface);

    EfiStatus (*UninstallProtocolInterface)(
        void *Handle,
        const EfiGuid *Protocol,
        void *Interface);

    EfiStatus (*HandleProtocol)(
        EfiHandle Handle,
        const EfiGuid *Protocol,
        void **Interface);

    void *Reserved;

    EfiStatus (*RegisterProtocolNotify)(
        const EfiGuid *Protocol,
        void *Event,
        void **Registration);

    EfiStatus (*LocateHandle)(
        uint32_t SearchType,
        const EfiGuid *Protocol,
        void *SearchKey,
        uintptr_t *BufferSize,
        EfiHandle *Buffer);

    EfiStatus (*LocateDevicePath)(
        const EfiGuid *Protocol,
        void **DevicePath,
        EfiHandle *Device);

    EfiStatus (*InstallConfigurationTable)(
        const EfiGuid *Guid,
        void *Table);

    EfiStatus (*LoadImage)(
        uint8_t BootPolicy,
        EfiHandle ParentImageHandle,
        void *DevicePath,
        void *SourceBuffer,
        uintptr_t SourceSize,
        EfiHandle *ImageHandle);

    EfiStatus (*StartImage)(
        EfiHandle ImageHandle,
        uintptr_t *ExitDataSize,
        uint16_t **ExitData);

    EfiStatus (*Exit)(
        EfiHandle ImageHandle,
        EfiStatus ExitStatus,
        uintptr_t ExitDataSize,
        uint16_t *ExitData);

    EfiStatus (*UnloadImage)(EfiHandle ImageHandle);

    EfiStatus (*ExitBootServices)(
        EfiHandle ImageHandle,
        uintptr_t MapKey);

    EfiStatus (*GetNextMonotonicCount)(uint64_t *Count);
    EfiStatus (*Stall)(uint64_t Microseconds);

    EfiStatus (*SetWatchdogTimer)(
        uint64_t Timeout,
        uint64_t WatchdogCode,
        uint64_t DataSize,
        uint16_t *WatchdogData);

    EfiStatus (*ConnectController)(
        EfiHandle ControllerHandle,
        EfiHandle *DriverImageHandle,
        void *RemainingDevicePath,
        uint8_t Recursive);

    EfiStatus (*DisconnectController)(
        EfiHandle ControllerHandle,
        EfiHandle DriverImageHandle,
        EfiHandle ChildHandle);

    EfiStatus (*OpenProtocol)(
        EfiHandle Handle,
        const EfiGuid *Protocol,
        void **Interface,
        EfiHandle AgentHandle,
        EfiHandle ControllerHandle,
        uint32_t Attributes);

    EfiStatus (*CloseProtocol)(
        EfiHandle Handle,
        const EfiGuid *Protocol,
        EfiHandle AgentHandle,
        EfiHandle ControllerHandle);

    EfiStatus (*OpenProtocolInformation)(
        EfiHandle Handle,
        const EfiGuid *Protocol,
        void **EntryBuffer,
        uintptr_t *EntryCount);

    EfiStatus (*ProtocolsPerHandle)(
        EfiHandle Handle,
        EfiGuid ***ProtocolBuffer,
        uintptr_t *ProtocolBufferCount);

    EfiStatus (*LocateHandleBuffer)(
        uint32_t SearchType,
        const EfiGuid *Protocol,
        void *SearchKey,
        uint64_t *NoHandles,
        EfiHandle **Buffer);

    EfiStatus (*LocateProtocol)(
        const EfiGuid *Protocol,
        void *Registration,
        void **Interface);
};

struct EfiSystemTable
{
    uint8_t Hdr[24];
    uint16_t *FirmwareVendor;
    uint32_t FirmwareRevision;
    uint32_t _pad;
    EfiHandle ConsoleInHandle;
    EfiSimpleTextInputProtocol *ConsoleIn;
    EfiHandle ConsoleOutHandle;
    EfiSimpleTextOutputProtocol *ConsoleOut;
    EfiHandle StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    EfiBootServices *BootServices;
    uint64_t NumberOfTableEntries;
    void *ConfigurationTable;
};

struct EfiGraphicsOutputModeInformation
{
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    uint32_t PixelFormat;
    uint32_t PixelInformation[4];
    uint32_t PixelsPerScanLine;
};

struct EfiGraphicsOutputProtocolMode
{
    uint32_t MaxMode;
    uint32_t Mode;
    EfiGraphicsOutputModeInformation *Info;
    uint64_t SizeOfInfo;
    uint64_t FrameBufferBase;
    uint64_t FrameBufferSize;
};

struct EfiGraphicsOutputProtocol
{
    EfiStatus (*QueryMode)(
        EfiGraphicsOutputProtocol *This,
        uint32_t ModeNumber,
        uint64_t *SizeOfInfo,
        EfiGraphicsOutputModeInformation **Info);

    EfiStatus (*SetMode)(
        EfiGraphicsOutputProtocol *This,
        uint32_t ModeNumber);

    void *Blt;

    EfiGraphicsOutputProtocolMode *Mode;
};

struct EfiLoadedImageProtocol
{
    uint32_t Revision;
    EfiHandle ParentHandle;
    EfiSystemTable *SystemTable;
    EfiHandle DeviceHandle;
};