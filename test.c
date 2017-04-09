#include <stdio.h>
#include <lwan-coro.h>

struct point {
    int x, y, maxx, maxy;
};


static int
iter(coro_t* coro)
{
    int i, j;
    struct point *p = (struct point *) coro_get_data(coro);
    
    for (i = 0; i < p->maxx; i++) {
        for (j = 0; j < p->maxy; j++) {
            p = (struct point *) coro_get_data(coro);
            p->x = i;
            p->y = j;
            coro_yield(coro, CORO_MAY_RESUME);
        }
    }

    coro_yield(coro, CORO_FINISHED);
    __builtin_unreachable();
}


int main()
{
    coro_switcher_t switcher;
    struct point p = {0, 0, 2, 2};
    coro_t *coro = coro_new(&switcher, iter, &p);

    while (1) {
        if (coro_resume(coro)) {
            break;
        }
        struct point *p = (struct point *) coro_get_data(coro);
        printf("(%d, %d)\n", p->x, p->y);
    }

    coro_free(coro);
    
    return 0;
}

