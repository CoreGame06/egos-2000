ENTRY(_start)
SECTIONS
{
  . = 0x80300000;
  .text : { *(.text) }
}
