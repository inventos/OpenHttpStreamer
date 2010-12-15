#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <memory.h>
 
 
static char *ngx_http_mp4frag(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t ngx_http_mp4frag_commands[] = {
    { ngx_string("mp4frag"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_mp4frag,
      0,
      0,
      NULL },
    ngx_null_command
};
 

static ngx_http_module_t ngx_http_mp4frag_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */
 
    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */
 
    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */
 
    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};
 
 
ngx_module_t ngx_http_mp4frag_module = {
    NGX_MODULE_V1,
    &ngx_http_mp4frag_module_ctx,  /* module context */
    ngx_http_mp4frag_commands,     /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static uint16_t get16(const u_char *buf) {
    return buf[0] * 0x100 + buf[1];
}

static uint32_t get24(const u_char *buf) {
    return buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
}

static uint32_t get32(const u_char *buf) {
    return buf[0] * 0x1000000 + buf[1] * 0x10000 + buf[2] * 0x100 + buf[3];
}

static void write24(u_char *buf, uint32_t value) {
    buf[0] = value / 0x10000;
    buf[1] = value / 0x100;
    buf[2] = value;
}

static void write32(u_char *buf, uint32_t value) {
    buf[0] = value / 0x1000000;
    write24(buf + 1, value);
}

static u_char *write_video_packet(u_char *buf, int keyframe, int prefix, uint32_t comp_offset,
                                uint32_t timestamp,
                                const void *data, uint32_t size) {
    *buf = 9;
    write24(buf + 1, size + 5);
    write24(buf + 4, timestamp % 0x1000000);
    buf[7] = (timestamp / 0x1000000) & 0xff;
    buf[8] = buf[9] = buf[10] = 0;
    buf[11] = (prefix || keyframe) ? 0x17 : 0x27;
    buf[12] = prefix ? 0 : 1;
    write24(buf + 13, comp_offset);
    memcpy(buf + 16, data, size);
    write32(buf + 16 + size, size + 5 + 11);
    return buf + 16 + size + 4;
}

static u_char *write_audio_packet(u_char *buf, int prefix, uint32_t timestamp,
                                      const void *data, uint32_t size) {
    *buf = 8;
    write24(buf + 1, size + 2);
    write24(buf + 4, timestamp % 0x1000000);
    buf[7] = (timestamp / 0x1000000) & 0xff;
    buf[8] = buf[9] = buf[10] = 0;
    buf[11] = 0xaf;
    buf[12] = prefix ? 0 : 1;
    memcpy(buf + 13, data, size);
    write32(buf + 13 + size, size + 2 + 11);
    return buf + 13 + size + 4;
}

static u_char *write_fragment_prefix(u_char *buf, ngx_str_t *video, ngx_str_t *audio, uint32_t ts) {
    buf = write_video_packet(buf, 1, 1, 0, ts, (const char *)video->data, video->len);
    return write_audio_packet(buf, 1, ts, (const char *)audio->data, audio->len);
}

static ngx_int_t make_mapping(const char *filename, ngx_str_t *pmap, ngx_http_request_t *r) {
    int fd;
    struct stat st;
    void *addr;

    fd = open(filename, O_RDONLY);
    if ( fd == -1 ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "opening %s", filename);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    if ( fstat(fd, &st) == -1 ) {
        close(fd);
        ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "stat %s", filename);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    addr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if ( addr == (void*)(-1) ) {
        close(fd);
        ngx_log_error(NGX_LOG_ERR, r->connection->log, errno, "mmap %s", filename);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    close(fd);
    pmap->data = (u_char*)addr;
    pmap->len = st.st_size;
    return NGX_OK;
}

static void free_mapping(ngx_str_t *pmap) {
    if ( pmap->data && pmap->len )
        munmap(pmap->data, pmap->len);
}

static ngx_int_t ngx_http_mp4frag_handler(ngx_http_request_t *r)
{
    ngx_int_t    rc;
    ngx_chain_t  out;
    u_char *resp_body;
    ngx_buf_t   *resp;
    char *lastslash;
    char *mediafilename;
    u_char *last;
    size_t root;
    ngx_str_t path;
    ngx_str_t index_map = ngx_null_string;
    const u_char *indexptr;
    ngx_str_t mediafile_map = ngx_null_string;
    unsigned medianum, fragnum;
    uint16_t nmedia;
    ngx_str_t videocodecdata, audiocodecdata;
    uint32_t mediaoffset, fragments_offset;
    uint16_t mediafilenamelen;
    uint16_t nsamples, nfragments;
    uint32_t totalsize;
    unsigned int iii;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }
 
    /* discard request body, since we don't need it here */
    rc = ngx_http_discard_request_body(r);
 
    if (rc != NGX_OK) {
        return rc;
    }
 
#if 0
    /* set the 'Content-type' header */
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";
#endif
 
    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    lastslash = strrchr((char*)path.data, '/');
    if ( lastslash ) {
        path.len = lastslash - (char*)path.data;
    }

    /* определить путь к файлу индекса и номер фрагмента: */
    *lastslash++ = 0;
    // uri - dirname, lastslash - basename
    if ( memcmp(lastslash, "Seg1-Frag", 9) != 0 ) {
        return NGX_HTTP_NOT_FOUND;
    }
    fragnum = atoi(lastslash + 9);

    lastslash = strrchr((char*)path.data, '/');
    if ( !lastslash ) {
        return NGX_HTTP_NOT_FOUND;
    }
    medianum = atoi(lastslash + 1);

    memcpy(lastslash + 1, "index", 6);
    
    if ( (rc = make_mapping((const char *)path.data, &index_map, r)) != NGX_OK ) {
        return rc;
    }
    indexptr = index_map.data;

#define CHECKMAP(nbytes) do { if (indexptr + nbytes >= index_map.data + index_map.len) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "index underrun, offset 0x%0x", nbytes); goto GENERAL_ERROR;}  } while(0)

    CHECKMAP(10); /* 8 for signature and 2 for media count */
    if ( memcmp(index_map.data, "mp4frag", 7) != 0 || index_map.data[7] /* version */ > 2 ) {
        free_mapping(&index_map);
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Bad index format in %s", path.data);
        goto GENERAL_ERROR;
    }
    indexptr += 8;

    nmedia = get16(indexptr);
    if ( medianum >= nmedia ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "No media #%d in %s, total %d media", medianum, path.data, nmedia);
        goto GENERAL_ERROR;
    }
    indexptr += 2;
    CHECKMAP(nmedia * 4);

    mediaoffset = get32(indexptr + medianum * 4);
    if ( mediaoffset >= index_map.len ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "short index in %s", path.data);
        goto GENERAL_ERROR;
    }
    indexptr = index_map.data;
    CHECKMAP(mediaoffset);
    indexptr = index_map.data + mediaoffset;

    CHECKMAP(2);
    mediafilenamelen = get16(indexptr);
    indexptr += 2;
    CHECKMAP(mediafilenamelen);
    if ( index_map.data[7] == 1 ) {
        if ( (mediafilename = ngx_pcalloc(r->pool, mediafilenamelen + 1)) == NULL ) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Insufficient memory");
            goto GENERAL_ERROR;
        }
        memcpy(mediafilename, (const char *)indexptr, mediafilenamelen);
        mediafilename[mediafilenamelen] = 0;
    }
    else /* index_map.data[7] == 2 */ {
        mediafilename = (char *)indexptr;
    }

    indexptr += mediafilenamelen;
 
    CHECKMAP(2);
    videocodecdata.len = get16(indexptr);
    indexptr += 2;
    CHECKMAP(videocodecdata.len);
    videocodecdata.data = (u_char*)indexptr;
    indexptr += videocodecdata.len;
    CHECKMAP(2);
    audiocodecdata.len = get16(indexptr);
    indexptr += 2;
    CHECKMAP(audiocodecdata.len);
    audiocodecdata.data = (u_char*)indexptr;
    indexptr += audiocodecdata.len;

    /* number of fragments in the media */
    CHECKMAP(2);
    nfragments = get16(indexptr);
    if ( fragnum > nfragments || fragnum < 1 ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "No fragment #%d in media #%d in file %s", fragnum, medianum, path.data);
        goto NOT_FOUND;
    }
    indexptr += 2;

    CHECKMAP(nfragments * 4);

    fragments_offset = get32(indexptr + (fragnum - 1) * 4);
    indexptr = index_map.data;
    CHECKMAP(fragments_offset);
    indexptr += fragments_offset;

    CHECKMAP(6);  /* first entry should present anyway */
    nsamples = get16(indexptr);
    if ( nsamples < 1 ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Number of samples is 0 for media #%d, fragment #%d, index %s",
                      medianum, fragnum, path.data);
        goto GENERAL_ERROR;
    }

    /* total fragment size in bytes: */
    totalsize = get32(indexptr + 2);

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "nsamples=%d, totalsize=%d", nsamples, totalsize);

    indexptr += 6;

    /* allocate memory for response */
    if ( (resp_body = ngx_pcalloc(r->pool, totalsize)) == NULL ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Insufficient memory");
        goto GENERAL_ERROR;
    }

    if ( (resp = ngx_pcalloc(r->pool, sizeof(ngx_buf_t))) == NULL ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Insufficient memory");
        goto GENERAL_ERROR;
    }
 
    out.buf = resp;
    out.next = NULL;

    resp->pos = resp_body;
    resp->last = resp_body + totalsize;
    resp->memory = 1; 
    resp->last_buf = 1;
 
    if ( make_mapping(mediafilename, &mediafile_map, r) != NGX_OK ) goto GENERAL_ERROR;
 
    /* generate the fragment */
    write32(resp_body, totalsize);
    memcpy(resp_body + 4, "mdat", 4);

    CHECKMAP(16 * nsamples);

    /* fragment timestamp is equal to first sample timestamp */
    resp_body = write_fragment_prefix(resp_body + 8, &videocodecdata, &audiocodecdata, get32(indexptr + 8));

    for ( iii = 0; iii < nsamples; ++iii ) {
        uint32_t offset = get32(indexptr);
        uint32_t size = get32(indexptr + 4);
        uint32_t timestamp = get32(indexptr + 8);
        uint32_t composition_offset = get24(indexptr + 12);
        uint8_t flags = indexptr[15];
        indexptr += 16;
        if ( flags ) {
            resp_body = write_video_packet(resp_body, flags & 2, 0, composition_offset, timestamp,
                                     mediafile_map.data + offset, size);
        }
        else {
            resp_body = write_audio_packet(resp_body, 0, timestamp, mediafile_map.data + offset, size);
        }
    }

    free_mapping(&index_map);
    free_mapping(&mediafile_map);

    if ( resp_body != resp->last ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "response buffer overrun");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = totalsize;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only || r->method == NGX_HTTP_HEAD) {
        return rc;
    }

    /* send the buffer chain of your response */
    return ngx_http_output_filter(r, &out);

GENERAL_ERROR:
    free_mapping(&index_map);
    free_mapping(&mediafile_map);
    return NGX_HTTP_INTERNAL_SERVER_ERROR;

NOT_FOUND:
    free_mapping(&index_map);
    free_mapping(&mediafile_map);
    return NGX_HTTP_NOT_FOUND;

}
 
 
static char *ngx_http_mp4frag(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf;
 
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_mp4frag_handler;
 
    return NGX_CONF_OK;
}
