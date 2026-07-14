define dump_spi0
  set $base = 0x01C41000

  printf "SPI 0 Register Dump:\n"

  printf "SPIGCR0  : 0x%08x\n", *(unsigned int*)($base + 0x0)
  printf "SPIGCR1  : 0x%08x\n", *(unsigned int*)($base + 0x4)
  printf "SPIINT0  : 0x%08x\n", *(unsigned int*)($base + 0x8)
  printf "SPILVL   : 0x%08x\n", *(unsigned int*)($base + 0xC)
  printf "SPIFLG   : 0x%08x\n", *(unsigned int*)($base + 0x10)

  set $i = 0
  while $i < 6
    printf "SPIPC%d  : 0x%08x\n", $i, *(unsigned int*)($base + 0x14 + (4 * $i))
    set $i = $i + 1
  end

  printf "SPIDAT0  : 0x%08x\n", *(unsigned int*)($base + 0x38)
  printf "SPIDAT1  : 0x%08x\n", *(unsigned int*)($base + 0x3C)
  printf "SPIBUF   : 0x%08x\n", *(unsigned int*)($base + 0x40)
  printf "SPIEMU   : 0x%08x\n", *(unsigned int*)($base + 0x44)
  printf "SPIDELAY : 0x%08x\n", *(unsigned int*)($base + 0x48)
  printf "SPIDEF   : 0x%08x\n", *(unsigned int*)($base + 0x4C)

  set $i = 0
  while $i < 4
    printf "SPIFMT%d : 0x%08x\n", $i, *(unsigned int*)($base + 0x50 + (4 * $i))
    set $i = $i + 1
  end

  printf "INTVEC1  : 0x%08x\n", *(unsigned int*)($base + 0x64)

end

define dump_spi1
  set $base = 0x01F0E000

  printf "SPI 1 Register Dump:\n"

  printf "SPIGCR0  : 0x%08x\n", *(unsigned int*)($base + 0x0)
  printf "SPIGCR1  : 0x%08x\n", *(unsigned int*)($base + 0x4)
  printf "SPIINT0  : 0x%08x\n", *(unsigned int*)($base + 0x8)
  printf "SPILVL   : 0x%08x\n", *(unsigned int*)($base + 0xC)
  printf "SPIFLG   : 0x%08x\n", *(unsigned int*)($base + 0x10)

  set $i = 0
  while $i < 6
    printf "SPIPC%d  : 0x%08x\n", $i, *(unsigned int*)($base + 0x14 + (4 * $i))
    set $i = $i + 1
  end

  printf "SPIDAT0  : 0x%08x\n", *(unsigned int*)($base + 0x38)
  printf "SPIDAT1  : 0x%08x\n", *(unsigned int*)($base + 0x3C)
  printf "SPIBUF   : 0x%08x\n", *(unsigned int*)($base + 0x40)
  printf "SPIEMU   : 0x%08x\n", *(unsigned int*)($base + 0x44)
  printf "SPIDELAY : 0x%08x\n", *(unsigned int*)($base + 0x48)
  printf "SPIDEF   : 0x%08x\n", *(unsigned int*)($base + 0x4C)

  set $i = 0
  while $i < 4
    printf "SPIFMT%d : 0x%08x\n", $i, *(unsigned int*)($base + 0x50 + (4 * $i))
    set $i = $i + 1
  end

  printf "INTVEC1  : 0x%08x\n", *(unsigned int*)($base + 0x64)

end

