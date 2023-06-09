
asm(R"(.section .text

.word write_sram_patched + 1
.word write_eeprom_patched + 1
.word read_sram_patched + 1
.word read_eeprom_patched + 1
.word verify_sram_patched + 1
.word verify_eeprom_patched + 1
eeprom_meta:
.word 0)");

struct eeprom_meta
{
    unsigned size;
    unsigned short addrs;
    unsigned short wait;
    unsigned char addr_bits;
};

struct eeprom_meta *get_eeprom_meta()
{
    struct eeprom_meta ***eeprom_meta_ptrptr;
    asm (R"(mov %[eeprom_meta_ptrptr], pc
    sub %[eeprom_meta_ptrptr], # . + 2 - eeprom_meta)" 
     : [eeprom_meta_ptrptr] "=r" (eeprom_meta_ptrptr));
    return **eeprom_meta_ptrptr;
}

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

unsigned char *translate(unsigned idx, int loadfactor_log2)
{
    return (unsigned char *) (0x0E000000 | idx << loadfactor_log2);
}

void write_core_patched(unsigned char *src, unsigned idx, unsigned size, int loadfactor_log2)
{
    unsigned sector_usage = 0x1000 >> loadfactor_log2;
    unsigned char sector_buf[sector_usage];
    while (size)
    {
        int prefix = (sector_usage - 1) & idx;
        unsigned char *sector = translate(idx - prefix, loadfactor_log2);
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
        idx += len;
        size -= len;
    }
}

void read_core_patched(unsigned char *dst, unsigned idx, unsigned size, int loadfactor_log2)
{
    my_memcpy(dst, 1, translate(idx, loadfactor_log2), 1 << loadfactor_log2, size);
}

int verify_core_patched(unsigned char *src, unsigned idx, unsigned size, int loadfactor_log2)
{
    while (size)
    {
        if (*src != *translate(idx, loadfactor_log2))
            return idx;
        
        ++src;
        ++idx;
        --size;
    }
    return -1;
}

void write_sram_patched(unsigned char *src, unsigned char *dst, unsigned size)
{
    write_core_patched(src, 0x00007FFF & (unsigned) dst, size, 1);
}
void read_sram_patched(unsigned char *src, unsigned char *dst, unsigned size)
{
    read_core_patched(dst, 0x00007FFF & (unsigned) src, size, 1);
}
unsigned char *verify_sram_patched(unsigned char *src, unsigned char *tgt, unsigned size)
{
    int error_idx = verify_core_patched(src, 0x00007FFF & (unsigned) tgt, size, 1);
    return error_idx < 0 ? 0 : (unsigned char *) (0x0E000000 | error_idx);
}

unsigned write_eeprom_patched(unsigned short addr, unsigned char *src)
{
    struct eeprom_meta *eeprom_meta = get_eeprom_meta();
    if (!eeprom_meta)
        return 1;
    int loadfactor_log2 = eeprom_meta->addrs == 0x40 ? 7 : 3;
    write_core_patched(src, addr << 3, 1 << 3, loadfactor_log2);
    return 0;
}
unsigned read_eeprom_patched(unsigned short addr, unsigned char *dst)
{
    struct eeprom_meta *eeprom_meta = get_eeprom_meta();
    if (!eeprom_meta)
        return 1;
    int loadfactor_log2 = eeprom_meta->addrs == 0x40 ? 7 : 3;
    read_core_patched(dst, addr << 3, 1 << 3, loadfactor_log2);
    return 0;
}
unsigned verify_eeprom_patched(unsigned short addr, unsigned char *src)
{
    
    struct eeprom_meta *eeprom_meta = get_eeprom_meta();
    if (!eeprom_meta)
        return 1;
    int loadfactor_log2 = eeprom_meta->addrs == 0x40 ? 7 : 3;
    return verify_core_patched(src, addr << 3, 1 << 3, loadfactor_log2) >= 0;
}