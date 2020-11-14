#include "http.h"

// 获取一行内容,以\n\0作为结束符,返回该行的字节数
// TODO: 该函数的正确性还需要检查
int get_buffer_line(struct evbuffer* buffer, char* cbuf) {
    int i = 0;
    char c = '\0';
    char last_c = '\0';
    int flag = 0;

    // 逐字符读取一行,保存在cbuf中
    while (evbuffer_get_length(buffer)) {
        if(c == '\n') {
            break;
        }
        // cbuf溢出
        if(i >= CBUF_LEN - 2) {
            break;
        }
        // 从evbuffer读取一个字符保存在c，并从evbuffer中移除该字符，返回值为读出的字节数
        // 如果buffer为空，则返回-1
        if (evbuffer_remove(buffer, &c, 1) > 0) {
            if ((last_c == '\r') && (c == '\n')) {
                cbuf[i-1] = c;
                flag = 1;
                break;
            }
            last_c = c;
            cbuf[i++] = c;
        } else {
            c = '\n';
        }
    }
    cbuf[i] = '\0';
    return i+flag;
}

int handle_post_request(struct evhttp_request* req, char* whole_path) {
    // TODO: post请求的处理,梁
    struct evkeyvalq* headers;
	struct evkeyval *header;
    const char* bound_key = "boundary=";
    char first_boundary[128] = {};
    char last_boundary[128] = {};
    int content_len = 0;
    int current_len = 0;
    int data_left = -1;
    FILE* f = NULL;
    struct evbuffer *buf;

    // 获取一个请求的报头，报头里面的内容组成了一个队列
    // 队列的每个元素是一个键值对，例如'cookie':'asdasdas'这种表示方式
    headers = evhttp_request_get_input_headers(req);
    printf("---begin a request header print-----------------\n");
	for (header = headers->tqh_first; header; header = header->next.tqe_next) {
		printf("header in \"handle_post_request()\" is %s: %s\n", header->key, header->value);
        // 判断报头中的content-type，该值用于定义网络文件的类型和网页的编码
        // 服务器根据编码类型使用特定的解析方式，获取数据流中的数据
        // 例如：Content-Type:multipart/form-data; boundary=ZnGpDtePMx0KrHh_G0X99Yef9r8JZsRJSXC
        if (!evutil_ascii_strcasecmp(header->key, "Content-Type")) {
            // TODO: 这里为什么又加上了一个长度
            // 当有多个数据要提交时，会以boundary的值作为分割
            char* boundary_value = strstr(header->value, bound_key) + strlen(bound_key);
            printf("boundary value of this post request is: %s \n", boundary_value);

            // 将first_boundary赋值为--ZnGpDtePMx0KrHh_G0X99Yef9r8JZsRJSXC
            // 将last_boundary赋值为--ZnGpDtePMx0KrHh_G0X99Yef9r8JZsRJSXC--
            strncpy(first_boundary, "--", 2);
            strncat(first_boundary, boundary_value, strlen(boundary_value));
            strncpy(last_boundary, first_boundary, strlen(first_boundary));
            strncat(first_boundary, "\n", 1);
            strncat(last_boundary, "--\n", 3);
        } else if (!evutil_ascii_strcasecmp(header->key, "Content-Length")) {
            // 得到post请求包体中内容所占字节数
            content_len = atoi(header->value);
            printf("content length of this post request is: %dB. \n", content_len);
        }

	}
    printf("---finish a request header print----------------\n");
    
    if(!(f = fopen(whole_path, "w"))) {
        printf("post request can not open file %s to write\n", whole_path);
        evhttp_send_error(req, HTTP_INTERNAL, "Post fails in write into file.");
        return -1;
    }

    buf = evhttp_request_get_input_buffer(req);
    while(current_len != content_len) {
        char cbuf[CBUF_LEN];
        current_len += get_buffer_line(buf, cbuf);
        if (data_left > 0) {
            data_left -= strlen(cbuf);
            if (data_left < 0)
                cbuf[strlen(cbuf) - 1] = '\0';
            fputs(cbuf, f);
            cbuf[strlen(cbuf) - 1] = '\0';
            // logger(DEBUG, "tofile: %s", cbuf);
            continue;
        }
        if (!evutil_ascii_strcasecmp(cbuf, first_boundary)) {
            printf("find boundary, begin write post data\n");
        } else if (!evutil_ascii_strcasecmp(cbuf, last_boundary)) {
            printf("find boundary, finish write post data.\n");
            printf("write data length: %d, post data length: %d\n", current_len, content_len);
        }
        // 记录要post写入的文件类型
        if (strstr(cbuf, "Content-Type:")) {
            current_len += get_buffer_line(buf, cbuf);
            data_left = content_len - current_len - strlen(last_boundary) - 3;
        }
    }
    fclose(f);
    evhttp_send_reply(req, 200, "OK", NULL);
    return 0;

}

int handle_get_request(struct evhttp_request* req, const char* path, char* whole_path, char* decoded_path) {
    // TODO: get请求的处理,杨
}


void request_cb(struct evhttp_request* req, void*arg) 
{
    // TODO: 这里处理http服务器接收到的请求，分为get和post
    // struct options *o = (struct options*)arg;
    struct options *o = arg;
    const char *cmdtype;
    const char *uri;
    const char *path;
    struct evhttp_uri *decoded = NULL;
    char *decoded_path;
    size_t len;
    char *whole_path = NULL;

    // 得到了请求的类型
    switch (evhttp_request_get_command(req)) {
        case EVHTTP_REQ_GET: 	 cmdtype = "GET";     break;
        case EVHTTP_REQ_POST:    cmdtype = "POST";    break;
        case EVHTTP_REQ_HEAD: 	 cmdtype = "HEAD";    break;
        case EVHTTP_REQ_PUT: 	 cmdtype = "PUT";     break;
        case EVHTTP_REQ_DELETE:  cmdtype = "DELETE";  break;
        case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
        case EVHTTP_REQ_TRACE:   cmdtype = "TRACE";   break;
        case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
        case EVHTTP_REQ_PATCH:   cmdtype = "PATCH";   break;
        default:                 cmdtype = "unknown"; break;
    }
    // 得到发起请求的uri
    uri = evhttp_request_get_uri(req);

    printf("Received a %s request for %s\nHeaders:\n",
	    cmdtype, uri);

    // http状态码405
    if (!evutil_ascii_strcasecmp(cmdtype, "unknown")) {
        printf("[HTTP STATUS] 405 method not allowed\n");
        evhttp_send_error(req, HTTP_BADMETHOD, 0);
        return;
    }

    /* Decode the URI */
    decoded = evhttp_uri_parse(uri);
    // http状态码400
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

    /* Let's see what path the user asked for. */
    path = evhttp_uri_get_path(decoded);
    if (!path) path = "/";

    /* We need to decode it, to see what path the user really wanted. */
	decoded_path = evhttp_uridecode(path, 0, NULL);
	if (decoded_path == NULL)
		goto err;
    printf("decoded path: %s\n", decoded_path);

    /* Don't allow any ".."s in the path, to avoid exposing stuff outside
	 * of the docroot.  This test is both overzealous and underzealous:
	 * it forbids aceptable paths like "/this/one..here", but it doesn't
	 * do anything to prevent symlink following." */
	if (strstr(decoded_path, ".."))
		goto err; 

    len = strlen(decoded_path)+strlen(o->docroot)+2;
	if (!(whole_path = malloc(len))) {
		perror("malloc");
		goto err;
	}

    // TODO: 此处与官方文档略有不同
    int offset = (decoded_path[0] == '/') ? 1 : 0;
    evutil_snprintf(whole_path, len - offset, "%s/%s", o->docroot,
                    decoded_path + offset);
    printf("the whole path of current request is: %s\n", whole_path);

    // TODO: 判断是get还是post请求，分别进行处理
    // if (evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
    // POST request
    if (!evutil_ascii_strcasecmp(cmdtype, "POST")) {
        handle_post_request(req, whole_path);
    // } else if (evhttp_request_get_command(req) == EVHTTP_REQ_GET) {
    // GET request
    } else if (!evutil_ascii_strcasecmp(cmdtype, "GET")) {
        handle_get_request(req, path, whole_path, decoded_path);
    } else {
        // http状态码501
        evhttp_send_error(req, HTTP_NOTIMPLEMENTED,
                          "Not implemented: only handle GET or POST request "
                          "for upload and download file can be processed.");
    }
    goto done;

err:
	evhttp_send_error(req, 404, "Document was not found");
done:
	if (decoded)
		evhttp_uri_free(decoded);
	if (decoded_path)
		free(decoded_path);
	if (whole_path)
		free(whole_path);
}

int 
main(int argc, char* argv[])
{
    int ret = 0;
    struct event *term = NULL;
    struct evhttp_bound_socket *handle = NULL;
    struct event_config *cfg = NULL;
    struct event_base *base = NULL;
    struct evhttp *http = NULL;

    struct options opt = parse_opts(argc, argv);

    cfg = event_config_new();
    base = event_base_new_with_config(cfg);
    if (!base) {
        // fprintf(stderr, "Couldn't create an event_base: exiting\n");
        printf("Couldn't create an event_base: exiting\n");
        ret = 1;
    }
    event_config_free(cfg);
    cfg = NULL;

    http = evhttp_new(base);
    if (!http) {
        // fprintf(stderr, "couldn't create evhttp. Exiting.\n");
        printf("couldn't create evhttp. Exiting.\n");
        ret = 1;
    }

    // TODO: 此处实现https中的ssl功能

    evhttp_set_gencb(http, request_cb, &opt);

    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", opt.port);
    if (!handle) {
        // fprintf(stderr,"couldn't bind to port %d. Exiting.\n", o.port);
        printf("couldn't bind to port %d. Exiting.\n", opt.port);
        ret = 1;
        goto err;
    }

    if (display_listen_sock(handle)) {
        ret = 1;
        goto err;
    }

    term = evsignal_new(base, SIGINT, do_term, base);
    if (!term)
        goto err;
    if (event_add(term, NULL))
        goto err;

    event_base_dispatch(base);

err:
    if (cfg)
        event_config_free(cfg);
    if (http)
        evhttp_free(http);
    if (term)
        event_free(term);
    if (base)
        event_base_free(base);
    // TODO: 此处加入ssl出错的处理
    return ret;
}


