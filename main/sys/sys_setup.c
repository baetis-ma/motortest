uint32_t ccount_stamp;
uint32_t IRAM_ATTR cycles(int reset)
{
    uint32_t ccount;
    __asm__ __volatile__ ( "rsr     %0, ccount" : "=a" (ccount) );
    if (reset == 1){ ccount_stamp = ccount; ccount = 0; }
    else { ccount = ccount - ccount_stamp; }
    ccount = ccount / CPU_FREQ;
    printf(" timer =  %10.3f ms\n", (float) ccount/1000);
    return ccount;
}

