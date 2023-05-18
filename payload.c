
asm(R"(.word write_sram_patched + 1
.word write_eeprom_patched + 1
.word read_sram_patched + 1
.word read_eeprom_patched + 1
.word verify_sram_patched + 1
.word verify_eeprom_patched + 1)");

#define SRAM_BASE ((volatile unsigned char*) (0x0E000000))
#define FLASH_MAGIC_0 (0x5555)
#define FLASH_MAGIC_1 (0x2AAA)

static void flashEraseSector(volatile unsigned char *tgt)
{
    SRAM_BASE[FLASH_MAGIC_0] = 0xAA;
    SRAM_BASE[FLASH_MAGIC_1] = 0x55;
    SRAM_BASE[FLASH_MAGIC_0] = 0x80;
    SRAM_BASE[FLASH_MAGIC_0] = 0xAA;
    SRAM_BASE[FLASH_MAGIC_1] = 0x55;
    *tgt = 0x30;
    __asm("nop");
    while (*tgt != 0xFF)
        ;
    SRAM_BASE[FLASH_MAGIC_0] = 0xAA;
    SRAM_BASE[FLASH_MAGIC_1] = 0x55;
    SRAM_BASE[FLASH_MAGIC_0] = 0xF0;
}
static void flashProgramByte(volatile unsigned char *tgt, unsigned char data)
{
    SRAM_BASE[FLASH_MAGIC_0] = 0xAA;
    SRAM_BASE[FLASH_MAGIC_1] = 0x55;
    SRAM_BASE[FLASH_MAGIC_0] = 0xA0;
    *tgt = data;
    __asm("nop");
    while (*tgt != data)
        ;
    SRAM_BASE[FLASH_MAGIC_0] = 0xAA;
    SRAM_BASE[FLASH_MAGIC_1] = 0x55;
    SRAM_BASE[FLASH_MAGIC_0] = 0xF0;
}

#define LOADFACTOR_LOG2 1

int my_memcpy(unsigned char *dst, int dstride, unsigned char *src, int sstride, unsigned size)
{
    int hits = 0;
    while (size)
    {
        if (*dst != *src)
            ++hits;
        *dst = *src;
        dst += dstride;
        src += sstride;
        --size;
    }
    return hits;
}

unsigned char *translate(unsigned char *addr, int loadfactor_log2)
{
    unsigned res = (unsigned) addr;
    res &= 0x0000FFFF;
    res <<= loadfactor_log2;
    res |= 0x0E000000;
    return (unsigned char *) res;
}

void write_sram_patched(unsigned char *src, unsigned char *dst, unsigned size)
{
    int loadfactor_log2 = LOADFACTOR_LOG2;
    unsigned sector_usage = 0x1000 >> loadfactor_log2;
    unsigned char sector_buf[sector_usage];
    while (size)
    {
        int prefix = (sector_usage - 1) & (unsigned) dst;
        unsigned char *sector = translate(dst - prefix, loadfactor_log2);
        int len = size;
        if (len + prefix > sector_usage)
        {
            len = sector_usage;
            len -= prefix;      
        }
        
        my_memcpy(sector_buf, 1, sector, 1 << loadfactor_log2, sector_usage);
        if (my_memcpy(sector_buf + prefix, 1, src, 1, len))
        {
            flashEraseSector(sector);
            for (int i = 0; i < sector_usage; ++i)
                flashProgramByte(&sector[i << loadfactor_log2], sector_buf[i]);        
        }
        
        src += len;
        dst += len;
        size -= len;
    }
}

void read_sram_patched(unsigned char *src, unsigned char *dst, unsigned size)
{
    int loadfactor_log2 = LOADFACTOR_LOG2;
    my_memcpy(dst, 1, translate(src, loadfactor_log2), 1 << loadfactor_log2, size);
}

unsigned char *verify_sram_patched(unsigned char *src, unsigned char *tgt, unsigned size)
{
    int loadfactor_log2 = LOADFACTOR_LOG2;
    while (size)
    {
        if (*src != *translate(tgt, loadfactor_log2))
            return tgt;
        
        ++src;
        ++tgt;
        --size;
    }
    return 0;
}

// Just shims for now...
static unsigned char *eep2sram(unsigned addr)
{
    addr <<= 3;
    addr |= 0x0e000000;
    return (unsigned char*) addr;
}

unsigned write_eeprom_patched(unsigned addr, unsigned char *src)
{
    write_sram_patched(src, eep2sram(addr), 8);
    return 0;
}
unsigned read_eeprom_patched(unsigned addr, unsigned char *dst)
{
    read_sram_patched(eep2sram(addr), dst, 8);
    return 0;
}
unsigned verify_eeprom_patched(unsigned addr, unsigned char *src)
{
    return !!verify_sram_patched(src, eep2sram(addr), 8);
}