#pragma once

#include <temrixstd.h>

#define NVME_REG_CSTS 0x1C
#define NVME_REG_ASQ_TAIL_DB 0x1000
#define NVME_REG_ACQ_HEAD_DB 0x1004
#define NVME_REG_IOSQ_TAIL_DB 0x1008
#define NVME_REG_IOCQ_HEAD_DB 0x100C

#define NVME_CSTS_RDY_MASK 0x1
#define NVME_CSTS_RDY_SHIFT 0

#define NVME_CQE_PHASE_MASK 0x1
#define NVME_CQE_STATUS_SHIFT 1
#define NVME_CQE_STATUS_MASK 0x7FF

#define NVME_QUEUE_DEPTH 64
#define NVME_SQ_ENTRY_SIZE 64
#define NVME_CQ_ENTRY_SIZE 16
#define NVME_PAGE_SIZE 4096
#define NVME_PRP_LIST_MAX_ENTRIES (NVME_PAGE_SIZE / sizeof(uint64_t))

#define NVME_ADMIN_OP_CREATE_IOSQ 0x01
#define NVME_ADMIN_OP_CREATE_IOCQ 0x05

#define NVME_IO_OP_FLUSH 0x00
#define NVME_IO_OP_WRITE 0x01
#define NVME_IO_OP_READ 0x02

#define NVME_CDW11_PC_FLAG 0x1
#define NVME_CDW11_IEN_FLAG 0x2
#define NVME_CDW11_QPRIO_URGENT (0 << 1)

#define NVME_CDW10_QUEUE_CREATE(qid, depth) \
    (((uint32_t)((depth) - 1) << 16) | (uint32_t)(qid))

#define NVME_CDW11_CQ_FLAGS (NVME_CDW11_PC_FLAG)
#define NVME_CDW11_SQ_FLAGS(cqid) ((uint32_t)((cqid) << 16) | NVME_CDW11_PC_FLAG)

#define NVME_READY_POLL_ITERS 100000
#define NVME_READY_POLL_DELAY 1000
#define NVME_ADMIN_COMPLETE_ITERS 1000000
#define NVME_IO_COMPLETE_ITERS 1000000

static constexpr uint32_t SECTOR_SIZE = 512;

static inline uint32_t nvmeRead32(uint64_t base, uint32_t offset)
{
    return *((volatile uint32_t *)(base + offset));
}
static inline void nvmeWrite32(uint64_t base, uint32_t offset, uint32_t val)
{
    *((volatile uint32_t *)(base + offset)) = val;
}
static inline uint64_t nvmeRead64(uint64_t base, uint32_t offset)
{
    return *((volatile uint64_t *)(base + offset));
}

struct __attribute__((packed)) SqEntry
{
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t reserved;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;

    void clear()
    {
        opcode = flags = 0;
        cid = 0;
        nsid = 0;
        reserved = mptr = prp1 = prp2 = 0;
        cdw10 = cdw11 = cdw12 = cdw13 = cdw14 = cdw15 = 0;
    }
};

struct __attribute__((packed)) CqEntry
{
    uint32_t dw0;
    uint32_t reserved;
    uint16_t sqHead;
    uint16_t sqId;
    uint16_t cid;
    uint16_t status;
};

struct PrpList
{
    uint64_t *virt;
    uint64_t phys;
    uint32_t count;

    int build(uint64_t bufPhys, uint64_t byteLen)
    {
        uint64_t firstPageEnd = (bufPhys & ~(uint64_t)(NVME_PAGE_SIZE - 1)) + NVME_PAGE_SIZE;
        uint64_t remaining = byteLen - (firstPageEnd - bufPhys);

        uint32_t n = (uint32_t)((remaining + NVME_PAGE_SIZE - 1) / NVME_PAGE_SIZE);
        if (n > NVME_PRP_LIST_MAX_ENTRIES)
            return -1;

        uint64_t page = firstPageEnd;
        for (uint32_t i = 0; i < n; i++, page += NVME_PAGE_SIZE)
            virt[i] = page;

        count = n;
        return 0;
    }
};

static int setPrp(SqEntry *cmd,
                  uint64_t bufPhys,
                  uint64_t byteLen,
                  uint64_t listPhys)
{
    cmd->prp1 = bufPhys;
    cmd->prp2 = 0;

    uint64_t firstPageEnd = (bufPhys & ~(uint64_t)(NVME_PAGE_SIZE - 1)) + NVME_PAGE_SIZE;
    if (byteLen <= firstPageEnd - bufPhys)
        return 0;

    uint64_t remaining = byteLen - (firstPageEnd - bufPhys);
    if (remaining <= NVME_PAGE_SIZE)
    {
        cmd->prp2 = firstPageEnd;
        return 0;
    }

    if (listPhys == 0)
        return -1;
    cmd->prp2 = listPhys;
    return 0;
}

struct PendingIo
{
    uint16_t cid;
    bool inUse;
};

class NvmeController
{
public:
    explicit NvmeController(uint64_t mmioBase,
                            SqEntry *asq, uint64_t asqPhys,
                            CqEntry *acq, uint64_t acqPhys,
                            SqEntry *iosq, uint64_t iosqPhys,
                            CqEntry *iocq, uint64_t iocqPhys)
        : base(mmioBase), asq(asq), acq(acq), iosq(iosq), iocq(iocq), asqPhys(asqPhys), acqPhys(acqPhys), iosqPhys(iosqPhys), iocqPhys(iocqPhys), sqTail(0), cqHead(0), ioSqTail(0), ioCqHead(0), phase(1), ioPhase(1), nextCid(0)
    {
        for (int i = 0; i < NVME_QUEUE_DEPTH; i++)
            pendingIos[i] = {0, false};
    }

    static NvmeController *init(uint64_t base)
    {
        uint32_t cc = nvmeRead32(base, 0x14);
        nvmeWrite32(base, 0x14, cc & ~1u);
        if (!pollReady(base, 0))
            return nullptr;

        Syscall::Memory::DmaAllocResult asqDma, acqDma;
        if (Syscall::Memory::AllocDma(NVME_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &asqDma) != 0)
            return nullptr;
        if (Syscall::Memory::AllocDma(NVME_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &acqDma) != 0)
            return nullptr;
        uint64_t asqVirt = asqDma.virt, asqPhys = asqDma.phys;
        uint64_t acqVirt = acqDma.virt, acqPhys = acqDma.phys;
        zeroMem(asqVirt, NVME_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE);
        zeroMem(acqVirt, NVME_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE);

        nvmeWrite32(base, 0x24, ((NVME_QUEUE_DEPTH - 1) << 16) | (NVME_QUEUE_DEPTH - 1));
        nvmeWrite32(base, 0x28, (uint32_t)(asqPhys & 0xFFFFFFFF));
        nvmeWrite32(base, 0x2C, (uint32_t)(asqPhys >> 32));
        nvmeWrite32(base, 0x30, (uint32_t)(acqPhys & 0xFFFFFFFF));
        nvmeWrite32(base, 0x34, (uint32_t)(acqPhys >> 32));

        nvmeWrite32(base, 0x14, (6 << 16) | (4 << 20) | 1);
        if (!pollReady(base, 1))
            return nullptr;

        Syscall::Memory::DmaAllocResult iosqDma, iocqDma;
        if (Syscall::Memory::AllocDma(NVME_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &iosqDma) != 0)
            return nullptr;
        if (Syscall::Memory::AllocDma(NVME_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &iocqDma) != 0)
            return nullptr;
        uint64_t iosqVirt = iosqDma.virt, iosqPhys = iosqDma.phys;
        uint64_t iocqVirt = iocqDma.virt, iocqPhys = iocqDma.phys;
        zeroMem(iosqVirt, NVME_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE);
        zeroMem(iocqVirt, NVME_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE);

        uint64_t selfPhys;
        uint64_t selfVirt = Syscall::Memory::Map(sizeof(NvmeController));
        NvmeController *ctrl = (NvmeController *)selfVirt;
        *ctrl = NvmeController(
            base,
            (SqEntry *)asqVirt, asqPhys,
            (CqEntry *)acqVirt, acqPhys,
            (SqEntry *)iosqVirt, iosqPhys,
            (CqEntry *)iocqVirt, iocqPhys);

        if (ctrl->createIoCq() != 0)
            return nullptr;
        if (ctrl->createIoSq() != 0)
            return nullptr;

        return ctrl;
    }

    static void destroy(NvmeController *ctrl)
    {
        if (!ctrl)
            return;

        uint64_t asqVirt = (uint64_t)ctrl->asq;
        uint64_t acqVirt = (uint64_t)ctrl->acq;
        uint64_t iosqVirt = (uint64_t)ctrl->iosq;
        uint64_t iocqVirt = (uint64_t)ctrl->iocq;

        Syscall::Memory::Unmap((uint64_t)ctrl, sizeof(NvmeController));

        Syscall::Memory::Unmap(asqVirt, NVME_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE);
        Syscall::Memory::Unmap(acqVirt, NVME_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE);
        Syscall::Memory::Unmap(iosqVirt, NVME_QUEUE_DEPTH * NVME_SQ_ENTRY_SIZE);
        Syscall::Memory::Unmap(iocqVirt, NVME_QUEUE_DEPTH * NVME_CQ_ENTRY_SIZE);
    }

    int waitReady(int ready)
    {
        for (int i = 0; i < NVME_READY_POLL_ITERS; i++)
        {
            uint32_t csts = nvmeRead32(base, NVME_REG_CSTS);
            if (((csts >> NVME_CSTS_RDY_SHIFT) & NVME_CSTS_RDY_MASK) == (uint32_t)ready)
                return 0;
            for (volatile int d = 0; d < NVME_READY_POLL_DELAY; d++)
                ;
        }
        return -1;
    }

    void adminSubmit(SqEntry *cmd)
    {
        asq[sqTail] = *cmd;
        sqTail = (sqTail + 1) % NVME_QUEUE_DEPTH;
        nvmeWrite32(base, NVME_REG_ASQ_TAIL_DB, sqTail);
    }

    int adminComplete()
    {
        for (int i = 0; i < NVME_ADMIN_COMPLETE_ITERS; i++)
        {
            CqEntry *cqe = &acq[cqHead];
            if ((cqe->status & NVME_CQE_PHASE_MASK) == phase)
            {
                cqHead = (cqHead + 1) % NVME_QUEUE_DEPTH;
                if (cqHead == 0)
                    phase ^= 1;
                nvmeWrite32(base, NVME_REG_ACQ_HEAD_DB, cqHead);
                return ((cqe->status >> NVME_CQE_STATUS_SHIFT) & NVME_CQE_STATUS_MASK) == 0 ? 0 : -1;
            }
        }
        return -1;
    }

    int createIoCq()
    {
        SqEntry cmd;
        cmd.clear();
        cmd.opcode = NVME_ADMIN_OP_CREATE_IOCQ;
        cmd.cid = nextCid++;
        cmd.prp1 = iocqPhys;
        cmd.cdw10 = NVME_CDW10_QUEUE_CREATE(1, NVME_QUEUE_DEPTH);
        cmd.cdw11 = NVME_CDW11_CQ_FLAGS;
        adminSubmit(&cmd);
        return adminComplete();
    }

    int createIoSq()
    {
        SqEntry cmd;
        cmd.clear();
        cmd.opcode = NVME_ADMIN_OP_CREATE_IOSQ;
        cmd.cid = nextCid++;
        cmd.prp1 = iosqPhys;
        cmd.cdw10 = NVME_CDW10_QUEUE_CREATE(1, NVME_QUEUE_DEPTH);
        cmd.cdw11 = NVME_CDW11_SQ_FLAGS(1);
        adminSubmit(&cmd);
        return adminComplete();
    }

    int buildReadWriteCmd(SqEntry *cmd, uint8_t opcode, uint64_t lba, uint32_t count,
                uint64_t bufPhys, uint32_t sectorSize, uint64_t listPhys)
    {
        cmd->clear();
        cmd->opcode = opcode;
        cmd->cid = nextCid++;
        cmd->nsid = 1;
        cmd->cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
        cmd->cdw11 = (uint32_t)(lba >> 32);
        cmd->cdw12 = count - 1;

        uint64_t byteLen = (uint64_t)count * sectorSize;
        if (setPrp(cmd, bufPhys, byteLen, listPhys) != 0)
            return -1;

        return cmd->cid;
    }

    int buildReadCmd(SqEntry *cmd, uint64_t lba, uint32_t count,
                    uint64_t bufPhys, uint32_t sectorSize, uint64_t listPhys)
    {
        return buildReadWriteCmd(cmd, NVME_IO_OP_READ, lba, count, bufPhys, sectorSize, listPhys);
    }

    int buildWriteCmd(SqEntry *cmd, uint64_t lba, uint32_t count,
                    uint64_t bufPhys, uint32_t sectorSize, uint64_t listPhys)
    {
        return buildReadWriteCmd(cmd, NVME_IO_OP_WRITE, lba, count, bufPhys, sectorSize, listPhys);
    }

    bool popIoCqe(uint16_t &cid, int &status)
    {
        CqEntry *cqe = &iocq[ioCqHead];
        if ((cqe->status & NVME_CQE_PHASE_MASK) != ioPhase)
            return false;

        cid = cqe->cid;
        status = (cqe->status >> NVME_CQE_STATUS_SHIFT) & NVME_CQE_STATUS_MASK;

        ioCqHead = (ioCqHead + 1) % NVME_QUEUE_DEPTH;
        if (ioCqHead == 0)
            ioPhase ^= 1;
        nvmeWrite32(base, NVME_REG_IOCQ_HEAD_DB, ioCqHead);
        return true;
    }

    int readSectors(uint64_t lba, uint32_t count, uint64_t bufPhys,
                    uint32_t sectorSize, uint64_t listPhys = 0)
    {
        SqEntry cmd;
        int cid = buildReadCmd(&cmd, lba, count, bufPhys, sectorSize, listPhys);
        if (cid < 0)
            return -1;

        ioSubmit(&cmd);
        return ioCompleteFor((uint16_t)cid);
    }

    int readSectorsBatch(uint64_t *lbas, uint64_t *bufs, uint64_t *prpLists,
                        uint32_t count, uint32_t sectorSize, int n)
    {
        if (n <= 0 || n > NVME_QUEUE_DEPTH - 1)
            return -1;

        uint16_t submittedCids[NVME_QUEUE_DEPTH];

        for (int i = 0; i < n; i++)
        {
            SqEntry cmd;
            int cid = buildReadCmd(&cmd, lbas[i], count, bufs[i], sectorSize,
                                    prpLists ? prpLists[i] : 0);
            if (cid < 0)
                return -1;

            submittedCids[i] = (uint16_t)cid;
            ioSubmitOne(&cmd);
        }
        nvmeWrite32(base, NVME_REG_IOSQ_TAIL_DB, ioSqTail);

        bool done[NVME_QUEUE_DEPTH] = {};
        int errors = 0, remaining = n, spins = 0;
        const int MAX_SPINS = NVME_IO_COMPLETE_ITERS * n;

        while (remaining > 0)
        {
            uint16_t cid; int status;
            if (!popIoCqe(cid, status))
            {
                if (++spins > MAX_SPINS)
                {
                    String::Printf("[nvme] batch timeout, %d/%d completed\n", n - remaining, n);
                    return -1;
                }
                continue;
            }

            bool matched = false;
            for (int i = 0; i < n; i++)
            {
                if (submittedCids[i] == cid && !done[i])
                {
                    done[i] = true;
                    if (status != 0)
                        errors++;
                    remaining--;
                    matched = true;
                    break;
                }
            }
            if (!matched)
                String::Printf("[nvme] batch: unmatched completion cid=%u\n", cid);
        }

        return errors == 0 ? 0 : -1;
    }

    int readAsync(uint64_t lba, uint32_t count, uint64_t bufPhys,
                uint32_t sectorSize, uint64_t listPhys = 0)
    {
        if (inflight() >= NVME_QUEUE_DEPTH - 1)
            return -1;

        SqEntry cmd;
        int cid = buildReadCmd(&cmd, lba, count, bufPhys, sectorSize, listPhys);
        if (cid < 0)
            return -1;

        ioSubmit(&cmd);
        pendingIos[cid % NVME_QUEUE_DEPTH] = {(uint16_t)cid, true};
        return cid;
    }

    int writeSectors(uint64_t lba, uint32_t count, uint64_t bufPhys,
                    uint32_t sectorSize, uint64_t listPhys = 0)
    {
        SqEntry cmd;
        int cid = buildWriteCmd(&cmd, lba, count, bufPhys, sectorSize, listPhys);
        if (cid < 0)
            return -1;

        ioSubmit(&cmd);
        return ioCompleteFor((uint16_t)cid);
    }

    int writeSectorsBatch(uint64_t *lbas, uint64_t *bufs, uint64_t *prpLists,
                        uint32_t count, uint32_t sectorSize, int n)
    {
        if (n <= 0 || n > NVME_QUEUE_DEPTH - 1)
            return -1;

        uint16_t submittedCids[NVME_QUEUE_DEPTH];

        for (int i = 0; i < n; i++)
        {
            SqEntry cmd;
            int cid = buildWriteCmd(&cmd, lbas[i], count, bufs[i], sectorSize,
                                    prpLists ? prpLists[i] : 0);
            if (cid < 0)
                return -1;

            submittedCids[i] = (uint16_t)cid;
            ioSubmitOne(&cmd);
        }
        nvmeWrite32(base, NVME_REG_IOSQ_TAIL_DB, ioSqTail);

        bool done[NVME_QUEUE_DEPTH] = {};
        int errors = 0, remaining = n, spins = 0;
        const int MAX_SPINS = NVME_IO_COMPLETE_ITERS * n;

        while (remaining > 0)
        {
            uint16_t cid; int status;
            if (!popIoCqe(cid, status))
            {
                if (++spins > MAX_SPINS)
                {
                    String::Printf("[nvme] write batch timeout, %d/%d completed\n", n - remaining, n);
                    return -1;
                }
                continue;
            }

            bool matched = false;
            for (int i = 0; i < n; i++)
            {
                if (submittedCids[i] == cid && !done[i])
                {
                    done[i] = true;
                    if (status != 0)
                        errors++;
                    remaining--;
                    matched = true;
                    break;
                }
            }
            if (!matched)
                String::Printf("[nvme] write batch: unmatched completion cid=%u\n", cid);
        }

        return errors == 0 ? 0 : -1;
    }

    int writeAsync(uint64_t lba, uint32_t count, uint64_t bufPhys,
                uint32_t sectorSize, uint64_t listPhys = 0)
    {
        if (inflight() >= NVME_QUEUE_DEPTH - 1)
            return -1;

        SqEntry cmd;
        int cid = buildWriteCmd(&cmd, lba, count, bufPhys, sectorSize, listPhys);
        if (cid < 0)
            return -1;

        ioSubmit(&cmd);
        pendingIos[cid % NVME_QUEUE_DEPTH] = {(uint16_t)cid, true};
        return cid;
    }

    int flush()
    {
        SqEntry cmd;
        cmd.clear();
        cmd.opcode = NVME_IO_OP_FLUSH;
        cmd.cid = nextCid++;
        cmd.nsid = 1;

        ioSubmit(&cmd);
        return ioCompleteFor((uint16_t)cmd.cid);
    }

    int pollAny()
    {
        uint16_t cid; int status;
        if (!popIoCqe(cid, status))
            return -1;

        pendingIos[cid % NVME_QUEUE_DEPTH].inUse = false;
        return status == 0 ? cid : -1;
    }

    int inflight() const
    {
        int count = 0;
        for (int i = 0; i < NVME_QUEUE_DEPTH; i++)
            if (pendingIos[i].inUse)
                count++;
        return count;
    }

    int drainAll()
    {
        int errors = 0;
        while (inflight() > 0)
        {
            int r = pollAny();
            if (r == -1 && inflight() > 0)
                errors++;
        }
        return errors == 0 ? 0 : -1;
    }

private:
    static bool pollReady(uint64_t base, int target)
    {
        for (int i = 0; i < NVME_READY_POLL_ITERS; i++)
        {
            if ((int)(nvmeRead32(base, NVME_REG_CSTS) & NVME_CSTS_RDY_MASK) == target)
                return true;
            for (volatile int d = 0; d < NVME_READY_POLL_DELAY; d++)
                ;
        }
        return false;
    }

    static void zeroMem(uint64_t virt, uint32_t bytes)
    {
        for (uint32_t i = 0; i < bytes; i++)
            ((uint8_t *)virt)[i] = 0;
    }

    void ioSubmitOne(SqEntry *cmd)
    {
        iosq[ioSqTail] = *cmd;
        ioSqTail = (ioSqTail + 1) % NVME_QUEUE_DEPTH;
    }

    void ioSubmit(SqEntry *cmd)
    {
        ioSubmitOne(cmd);
        nvmeWrite32(base, NVME_REG_IOSQ_TAIL_DB, ioSqTail);
    }

    int ioComplete()
    {
        return ioCompleteFor(0);
    }

    int ioCompleteFor(uint16_t expectedCid)
    {
        for (int i = 0; i < NVME_IO_COMPLETE_ITERS; i++)
        {
            CqEntry *cqe = &iocq[ioCqHead];
            if ((cqe->status & NVME_CQE_PHASE_MASK) == ioPhase)
            {
                uint16_t cid = cqe->cid;
                int status = (cqe->status >> NVME_CQE_STATUS_SHIFT) & NVME_CQE_STATUS_MASK;

                ioCqHead = (ioCqHead + 1) % NVME_QUEUE_DEPTH;
                if (ioCqHead == 0)
                    ioPhase ^= 1;
                nvmeWrite32(base, NVME_REG_IOCQ_HEAD_DB, ioCqHead);

                if (expectedCid != 0 && cid != expectedCid)
                {
                    String::Printf("[nvme] STALE completion cid=%u while waiting for cid=%u\n",
                                   cid, expectedCid);
                    continue;
                }
                return status == 0 ? 0 : -1;
            }
        }
        return -1;
    }

    uint64_t base;

    SqEntry *asq;
    CqEntry *acq;
    SqEntry *iosq;
    CqEntry *iocq;

    uint64_t asqPhys;
    uint64_t acqPhys;
    uint64_t iosqPhys;
    uint64_t iocqPhys;

    uint16_t sqTail;
    uint16_t cqHead;
    uint16_t ioSqTail;
    uint16_t ioCqHead;

    uint8_t phase;
    uint8_t ioPhase;
    uint16_t nextCid;

    PendingIo pendingIos[NVME_QUEUE_DEPTH];
};