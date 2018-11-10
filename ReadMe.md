# whois for Windows

whois for windows.

## WSAPoll

WSAPoll Wait For timeout or socket is ok.
// https://docs.microsoft.com/zh-cn/windows/desktop/api/winsock2/nf-winsock2-wsapoll

```c
//https://docs.microsoft.com/zh-cn/windows/desktop/api/winsock2/ns-winsock2-pollfd
typedef struct pollfd {
  SOCKET fd;
  SHORT  events;
  SHORT  revents;
} WSAPOLLFD, *PWSAPOLLFD, *LPWSAPOLLFD;
// POLLIN, POLLIN
```

