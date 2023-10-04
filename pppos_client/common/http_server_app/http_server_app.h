#ifndef __HTTP_SERVER_APP_H_
#define __HTTP_SERVER_APP_H_


typedef void (*http_callback_post_t)(char *buf, int len);
typedef void (*http_callback_get_t)(void);

void http_set_callback_post(void *cb);
void http_set_callback_slider(void *cb);
void http_set_callback_get(void *cb);
void start_webserver(void);
void stop_webserver(void);
void dht11_response(char *data, int len);
#endif