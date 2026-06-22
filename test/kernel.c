extern "C" void _start(void) {
    volatile unsigned char* uart = (unsigned char*)0x10000000;
    const char* msg = "Hello from test kernel!\n";
    uart[3] = 0x03;
    uart[2] = 0x07;
    uart[1] = 0x01;
    while (*msg) {
        if (*msg == '\n') { while (!(uart[5] & 0x20)); uart[0] = '\r'; }
        while (!(uart[5] & 0x20));
        uart[0] = *msg++;
    }
    while (1) { asm volatile("wfi"); }
}
