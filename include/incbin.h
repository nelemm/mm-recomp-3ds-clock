#ifndef INCBIN_H
#define INCBIN_H

// Embeds a binary file in .rodata and declares `identifier` / `identifier_end`.
#define INCBIN(identifier, filename)              \
    __asm__(".pushsection .rodata\n"              \
            "\t.balign 8\n"                       \
            "\t.local " #identifier "\n"          \
            "\t.type " #identifier ", @object\n"  \
            #identifier ":\n"                     \
            "\t.incbin \"" filename "\"\n"        \
            "\t.local " #identifier "_end\n"      \
            "\t.type " #identifier "_end, @object\n" \
            #identifier "_end:\n"                 \
            "\t.balign 8\n"                       \
            "\t.popsection\n");                   \
    extern unsigned char identifier[];            \
    extern unsigned char identifier##_end[];

#endif
