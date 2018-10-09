# Install

* nginx configuration params

 `--add-dynamic-module=/PATH/TO/ngx_http_proxy_header_filter_module/ngx_http_proxy_header_filter_module`
 
# Usage
 
 ```conf
 load ngx_http_proxy_header_filter_module.so;
 ###other configurations
 http{
    ###other configurations
    server{
        ###other configurations
        location / {
           ###other configurations
           proxy_pass http://backend;
           proxy_header_filter Set-Cookie "secure" "";
        }
    }
 }
 ```
 
# Directives
 
 **proxy_header_filter** `Header` `Match` `Value` 

 **default:** ``

 **context:** `http, server, location`
