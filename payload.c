
asm(R"(.word write_sram_patched + 1
.word write_eeprom_patched + 1
.word read_sram_patched + 1
.word 0
.word verify_sram_patched + 1
.word 0

.thumb
write_eeprom_patched:
    push {r4, lr}
	mov r2, r1
	add r2, # 8
	mov r3, sp
write_eeprom_patched_byte_swap_loop:
    ldrb r4, [r1]
	add r1, # 1
	sub r3, # 1
	strb r4, [r3]
	cmp r1, r2
	bne write_eeprom_patched_byte_swap_loop
	
	mov r1, # 0x0e
	lsl r1, # 24
	lsl r0, # 3
	add r1, r0
	mov r2, # 8
	mov r0, r3
	mov sp, r3
	bl write_sram_patched
	
    mov r0, # 0
	add sp, # 8
	pop {r4, pc}
    
    
)");

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

int my_memcpy(unsigned char *dst, unsigned char *src, unsigned size)
{
    int hits = 0;
    while (size)
    {
        if (*dst != *src)
            ++hits;
        *dst = *src;
        ++dst;
        ++src;
        --size;
    }
    return hits;
}

void write_sram_patched(unsigned char *src, unsigned char *dst, unsigned size)
{
    unsigned char sector_buf[0x1000];
    while (size)
    {
        int prefix = 0x0FFF & (unsigned) dst;
        unsigned char *sector = dst - prefix;
        int len = size;
        if (len + prefix > 0x1000)
        {
            len = 0x1000;
            len -= prefix;      
        }
        
        my_memcpy(sector_buf, sector, 0x1000);
        if (my_memcpy(sector_buf + prefix, src, len))
        {
            flashEraseSector(sector);
            for (int i = 0; i < 0x1000; ++i)
                flashProgramByte(&sector[i], sector_buf[i]);        
        }
        
        src += len;
        dst += len;
        size -= len;
    }
}

void read_sram_patched(unsigned char *src, unsigned char *dst, unsigned size)
{
    my_memcpy(dst, src, size);
}

unsigned char *verify_sram_patched(unsigned char *src, unsigned char *tgt, unsigned size)
{
    while (size)
    {
        if (*src != *tgt)
            return tgt;
        
        ++src;
        ++tgt;
        --size;
    }
    return 0;
}