#include <stdio.h>
#include "lwan-coro.h"

static int
foo(coro_t* coro)
{
    void *data = coro_get_data(coro);
    for (int i = 0; i < 10; i++) {
        printf("[%d]function_foo, arg=%s\n", i, (const char*)data);
        coro_yield(coro, CORO_MAY_RESUME);
    }
    
    return CORO_FINISHED;
}


int
main(int argc, char *argv[])
{
    coro_switcher_t switcher;
    coro_t *coro_foo = coro_new(&switcher, foo, "foo");

    while (1) {
        if (coro_resume(coro_foo))
            break;
    }

    coro_free(coro_foo);
    
    return 0;
}
