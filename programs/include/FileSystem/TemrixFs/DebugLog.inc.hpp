static char hexDigit(uint8_t v)
{
    return (v < 10) ? ('0' + v) : ('a' + (v - 10));
}

void printHex(const char *prefix, const uint8_t *data, size_t len)
{
    String::Print(prefix);

    char out[16 * 3 + 1];
    size_t p = 0;

    for (size_t i = 0; i < len; i++)
    {
        out[p++] = hexDigit((data[i] >> 4) & 0xF);
        out[p++] = hexDigit(data[i] & 0xF);

        if (i + 1 != len)
            out[p++] = ' ';
    }

    out[p] = '\0';

    String::Print(out);
    String::Print("\n");
}

void printGuid(const char *prefix, const Guid &g)
{
    String::Printf(
        "%s%08x-%04x-%04x-%02x%02x-"
        "%02x%02x%02x%02x%02x%02x\n",
        prefix,
        g.data1,
        (uint32_t)g.data2,
        (uint32_t)g.data3,
        g.data4[0], g.data4[1],
        g.data4[2], g.data4[3],
        g.data4[4], g.data4[5],
        g.data4[6], g.data4[7]);
}

void printUtf16Name(const char *prefix, const uint16_t *name, size_t maxChars)
{
    String::Print(prefix);

    char ascii[37];
    size_t j = 0;

    for (size_t i = 0; i < maxChars && j < sizeof(ascii) - 1; i++)
    {
        uint16_t ch = name[i];

        if (ch == 0)
            break;

        if (ch >= 32 && ch <= 126)
            ascii[j++] = (char)ch;
        else
            ascii[j++] = '?';
    }

    ascii[j] = '\0';

    String::Print(ascii);
    String::Print("\n");
}

void logGptHeader(const GptHeader *hdr)
{
    String::Printf("[temrixfs] --- GPT header ---\n");
    printHex("[temrixfs] signature:      ", (uint8_t *)hdr->signature, 8);
    String::Printf("[temrixfs] revision:       %u\n", hdr->revision);
    String::Printf("[temrixfs] headerSize:     %u\n", hdr->headerSize);
    String::Printf("[temrixfs] headerCrc32:    %x\n", hdr->headerCrc32);
    String::Printf("[temrixfs] myLba:          %llu\n", hdr->myLba);
    String::Printf("[temrixfs] alternateLba:   %llu\n", hdr->alternateLba);
    String::Printf("[temrixfs] firstUsableLba: %llu\n", hdr->firstUsableLba);
    String::Printf("[temrixfs] lastUsableLba:  %llu\n", hdr->lastUsableLba);
    printHex("[temrixfs] diskGuid:       ", hdr->diskGuid, 16);
    String::Printf("[temrixfs] entryLba:       %llu\n", hdr->partitionEntryLba);
    String::Printf("[temrixfs] numEntries:     %u\n", hdr->numPartitionEntries);
    String::Printf("[temrixfs] entrySize:      %u\n", hdr->partitionEntrySize);
    String::Printf("[temrixfs] arrayCrc32:     %x\n", hdr->partitionArrayCrc32);
}

void logPartitionEntry(uint32_t index, const GptPartitionEntry *entry, bool isTemrix)
{
    String::Printf("[temrixfs] entry %u:\n", index);
    printGuid     ("[temrixfs]   typeGuid:   ", entry->partitionTypeGuid);
    printGuid     ("[temrixfs]   uniqueGuid: ", entry->uniquePartitionGuid);
    String::Printf("[temrixfs]   startLba:   %llu\n", entry->startingLba);
    String::Printf("[temrixfs]   endLba:     %llu\n", entry->endingLba);
    String::Printf("[temrixfs]   attributes: %llx\n", entry->attributes);
    printUtf16Name("[temrixfs]   name:       ", entry->name, 36);
    String::Printf("[temrixfs]   match:      %s\n", isTemrix ? "TEMRIX" : "no");
}