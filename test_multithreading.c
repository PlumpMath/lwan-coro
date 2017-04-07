#include <stdio.h>
#include <assert.h>

#include <apr_general.h>
#include <apr_network_io.h>
#include <apr_strings.h>
#include <apr_ring.h>

#include <lwan-coro.h>

#define DEF_REMOTE_HOST   "localhost"
#define DEF_REMOTE_PORT    8081
#define DEF_SOCK_TIMEOUT  (APR_USEC_PER_SEC * 30)
#define BUFSIZE            4096
#define CRLF_STR          "\r\n"


struct url_info {
    apr_pool_t *p;
    const char *host;
    const char *path;
    apr_ssize_t rv;
};


typedef struct _coro_elem_t {
    APR_RING_ENTRY(_coro_elem_t) link;
    coro_t *coro;
} coro_elem_t;

typedef struct _coro_ring_t coro_ring_t;
APR_RING_HEAD(_coro_ring_t, _coro_elem_t);


static int download(coro_t *coro);
static int do_client_task(apr_socket_t *sock, coro_t *coro);
static apr_status_t do_connect(apr_socket_t **sock, coro_t *coro);
static void get(coro_switcher_t *switcher, coro_ring_t *threads,
                const char *host, const char *path, apr_pool_t *mp);
static void dispatch(coro_ring_t *threads);


int
main(int argc, const char *argv[])
{
    apr_pool_t *mp;
    coro_ring_t *threads;
    coro_switcher_t switcher;
    
    apr_initialize();
    apr_pool_create(&mp, NULL);

    /* intialize the ring container */
    threads = apr_palloc(mp, sizeof(coro_ring_t));
    APR_RING_INIT(threads, _coro_elem_t, link);

    get(&switcher, threads, "www.w3.org", "/TR/html401/html40.txt", mp);
    get(&switcher, threads, "www.w3.org", "/TR/2002/REC-xhtml1-20020801/xhtml1.pdf", mp);
    get(&switcher, threads, "www.w3.org", "/TR/REC-html32.html", mp);
    get(&switcher, threads, "www.w3.org", "/TR/2000/REC-DOM-Level-2-Core-20001113/DOM2-Core.txt", mp);    
    
    dispatch(threads);

    apr_terminate();

    return 0;
}


static void
dispatch(coro_ring_t *threads)
{
    int rv;
    const coro_elem_t *ep;
    
    while (1) {
        if (APR_RING_EMPTY(threads, _coro_elem_t, link))
            break;
        
        for (ep = APR_RING_FIRST(threads);
             ep != APR_RING_SENTINEL(threads, _coro_elem_t, link);
             ep = APR_RING_NEXT(ep, link)) {
            coro_t *coro = ep->coro;
            if ((rv = coro_resume(coro)) != CORO_MAY_RESUME) {
                APR_RING_REMOVE(ep, link);
            }
        }
    }
}


static void
get(coro_switcher_t *switcher, coro_ring_t *threads,
    const char *host, const char *path, apr_pool_t *mp)
{
    struct url_info *uinfo = apr_pcalloc(mp, sizeof(*uinfo));
    uinfo->p = mp;
    uinfo->host = apr_pstrdup(mp, host);
    uinfo->path = apr_pstrdup(mp, path);

    coro_elem_t *obj = apr_palloc(mp, sizeof(coro_elem_t));
    obj->coro = coro_new(switcher, download, uinfo);
    
    APR_RING_INSERT_TAIL(threads, obj, _coro_elem_t, link);
}


int download(coro_t *coro)
{
    apr_status_t rv;
    apr_socket_t *s;
    int rc;
    
    rv = do_connect(&s, coro);
    if (rv != APR_SUCCESS) {
        goto error;
    }
    
    rc = do_client_task(s, coro);
    if (rc == CORO_ABORT) {
        return CORO_ABORT;
    }
    apr_socket_close(s);

    return CORO_FINISHED;

error:
    {
        char errbuf[256];
        apr_strerror(rv, errbuf, sizeof(errbuf));
        printf("error: %d, %s\n", rv, errbuf);
    }

    coro_yield(coro, CORO_ABORT);
    __builtin_unreachable();
    return CORO_ABORT;
}


static apr_status_t
do_connect(apr_socket_t **sock, coro_t * coro)
{
    apr_sockaddr_t *sa;
    apr_socket_t *s;
    apr_status_t rv;
    struct url_info *uinfo = (struct url_info *) coro_get_data(coro);
    apr_pool_t *mp = uinfo->p;
    
    rv = apr_sockaddr_info_get(&sa, uinfo->host, APR_INET, DEF_REMOTE_PORT, 0, mp);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    
    rv = apr_socket_create(&s, sa->family, SOCK_STREAM, APR_PROTO_TCP, mp);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    apr_socket_opt_set(s, APR_SO_NONBLOCK, 1);
    apr_socket_timeout_set(s, DEF_SOCK_TIMEOUT);

    rv = apr_socket_connect(s, sa);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    /* nonblocking */
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 1);
    apr_socket_timeout_set(s, 0);

    *sock = s;
    
    return APR_SUCCESS;
}


static int
do_client_task(apr_socket_t *sock, coro_t *coro)
{
    apr_status_t rv;
    struct url_info *uinfo = (struct url_info *) coro_get_data(coro);
    apr_pool_t *mp = uinfo->p;
    const char *req_hdr = apr_pstrcat(mp, "GET ", uinfo->path, " HTTP/1.0" CRLF_STR CRLF_STR, NULL);
    apr_ssize_t total = 0;
    apr_size_t len = strlen(req_hdr);

    
    rv = apr_socket_send(sock, req_hdr, &len);
    if (rv != APR_SUCCESS) {
        uinfo->rv = -1;
        coro_yield(coro, CORO_ABORT);
        __builtin_unreachable();
    }

    while (1) {
        char buf[BUFSIZE];
        apr_size_t len = sizeof(buf);
        
        apr_status_t rv = apr_socket_recv(sock, buf, &len);
        if (APR_STATUS_IS_EAGAIN(rv)) {
            coro_yield(coro, CORO_MAY_RESUME);
        } else if (APR_STATUS_IS_EOF(rv) || len == 0) {
            break;
        } else {
            total += len;
        }
    }

    uinfo->rv = total;

    printf("%s: %zd\n", uinfo->path, total);
    
    return CORO_FINISHED;
}

