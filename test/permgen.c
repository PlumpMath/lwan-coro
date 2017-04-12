#include <stdio.h>
#include <lwan-coro.h>


struct perma {
    int *a;
    int n;
};


static void
print_result(int *a, int n)
{
    int i;
    
    for (i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
}


static void swap(int *x, int *y)
{
    int t;

    t = *x;
    *x = *y;
    *y = t;
}


static int
permgen(coro_t *coro)
{
    int i;
    struct perma *pa = (struct perma *) coro_get_data(coro);
    int *a = pa->a;
    int n = pa->n;
    
    if (n <= 0) {
        coro_yield(coro, CORO_MAY_RESUME);
    } else {
        for (i = 0; i < n; i++) {
            swap(a+n-1, a+i);
            pa->n = n - 1;
            permgen(coro);
            swap(a+n-1, a+i);
        }
    }
}


int
main(int argc, char *argv[])
{
    coro_switcher_t switcher;
    int a[] = { 1, 2, 3, 4 };
    int n = sizeof(a)/sizeof(int);
    struct perma pa = { a, n };
    coro_t *coro = coro_new(&switcher, permgen, &pa);

    while (coro_resume(coro) == CORO_MAY_RESUME) {
        struct perma *pa = (struct perma *) coro_get_data(coro);
        print_result(pa->a, n);
    }

    return 0;
}
