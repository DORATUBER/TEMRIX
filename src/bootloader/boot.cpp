#include "boot.hpp"
#include "memory.hpp"

extern "C" void enableSse();
extern "C" void setupGdt();

extern "C" void bootMain(EfiHandle imageHandle, EfiSystemTable *systemTable)
{
    BootContext ctx(imageHandle, systemTable);
    ctx.init();
    
    ctx.mmap.fetch(ctx.bs);

    ctx.con.printfln("Press ESC to continue...");
    ctx.input.echoUntilEscape(ctx.con, ctx.fb, ctx.mmap);

    ctx.allocatePageTablePool();
    ctx.allocateKernelStack();
    ctx.allocateTrampoline();  
    ctx.loadKernel();
    ctx.loadUserFile((const uint16_t *)u"\\EFI\\TEMRIX\\Init.trx",   ctx.initFile);
 
    BootInfo bootInfo = ctx.exitBootServices();

    bootInfo.initProcess = ctx.initFile;

    setupGdt();
    enableSse();

    ctx.setupPageTables(bootInfo);
    ctx.jumpToKernel(bootInfo);
}