////
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
///
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <wchar.h>

#pragma comment(lib, "ws2_32.lib")

/// This function only support C++17
template <class IntegerT>
[[nodiscard]] inline std::wstring
Integer_to_chars(const IntegerT _Raw_value,
                 const int _Base) noexcept // strengthened
{

  using _Unsigned = std::make_unsigned_t<IntegerT>;
  _Unsigned _Value = static_cast<_Unsigned>(_Raw_value);
  std::wstring result;
  if constexpr (std::is_signed_v<IntegerT>) {
    if (_Raw_value < 0) {
      result.push_back('-');
      _Value = static_cast<_Unsigned>(0 - _Value);
    }
  }

  constexpr size_t _Buff_size =
      sizeof(_Unsigned) * CHAR_BIT; // enough for base 2
  wchar_t _Buff[_Buff_size];
  wchar_t *const _Buff_end = _Buff + _Buff_size;
  wchar_t *_RNext = _Buff_end;

  static constexpr wchar_t _Digits[] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
      'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
      'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
  static_assert(std::size(_Digits) == 36);

  switch (_Base) {
  case 10: { // Derived from _UIntegral_to_buff()
    constexpr bool _Use_chunks = sizeof(_Unsigned) > sizeof(size_t);

    if constexpr (_Use_chunks) { // For 64-bit numbers on 32-bit platforms,
                                 // work in chunks to avoid 64-bit divisions.
      while (_Value > 0xFFFF'FFFFU) {
        unsigned long _Chunk =
            static_cast<unsigned long>(_Value % 1'000'000'000);
        _Value = static_cast<_Unsigned>(_Value / 1'000'000'000);

        for (int _Idx = 0; _Idx != 9; ++_Idx) {
          *--_RNext = static_cast<char>('0' + _Chunk % 10);
          _Chunk /= 10;
        }
      }
    }

    using _Truncated =
        std::conditional_t<_Use_chunks, unsigned long, _Unsigned>;

    _Truncated _Trunc = static_cast<_Truncated>(_Value);

    do {
      *--_RNext = static_cast<wchar_t>('0' + _Trunc % 10);
      _Trunc /= 10;
    } while (_Trunc != 0);
    break;
  }

  case 2:
    do {
      *--_RNext = static_cast<wchar_t>('0' + (_Value & 0b1));
      _Value >>= 1;
    } while (_Value != 0);
    break;

  case 4:
    do {
      *--_RNext = static_cast<wchar_t>('0' + (_Value & 0b11));
      _Value >>= 2;
    } while (_Value != 0);
    break;

  case 8:
    do {
      *--_RNext = static_cast<wchar_t>('0' + (_Value & 0b111));
      _Value >>= 3;
    } while (_Value != 0);
    break;

  case 16:
    do {
      *--_RNext = _Digits[_Value & 0b1111];
      _Value >>= 4;
    } while (_Value != 0);
    break;

  case 32:
    do {
      *--_RNext = _Digits[_Value & 0b11111];
      _Value >>= 5;
    } while (_Value != 0);
    break;

  default:
    do {
      *--_RNext = _Digits[_Value % _Base];
      _Value = static_cast<_Unsigned>(_Value / _Base);
    } while (_Value != 0);
    break;
  }

  const ptrdiff_t _Digits_written = _Buff_end - _RNext;

  result.append(_RNext, _Digits_written);
  return result;
}

using io_status_t = int;
enum io_status_e : io_status_t {
  IO_SUCCESS,
  IO_TIMEUP // TIMEOUT
};

io_status_t IOWaitForTimeout(SOCKET sock, int nes, bool for_read) {
  WSAPOLLFD pfd;
  pfd.fd = sock;
  pfd.events = for_read ? POLLIN : POLLOUT;
  int rc = 0;
  do {
    rc = WSAPoll(&pfd, 1, nes * 1000);
  } while (rc == -1 && errno == EINTR);
  if (rc == 0) {
    return IO_TIMEUP;
  }
  if (rc > 0) {
    return IO_SUCCESS;
  }
  return errno;
}

inline void PrintError(int err, const wchar_t *reson = L"unknwon") {
  // print error
  LPWSTR errbuf = nullptr;
  if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_ALLOCATE_BUFFER,
                     nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                     (LPWSTR)&errbuf, 0, nullptr) > 0) {
    fwprintf(stderr, L"Error %s %d: %s\n", reson, err, errbuf);
    LocalFree(errbuf);
  } else {
    fprintf(stderr, "FormatMessageW error\n");
  }
}

bool SockConnectEx(SOCKET sock, const struct sockaddr *name, int namelen,
                   int nes) {
  ULONG flags = 1;
  if (ioctlsocket(sock, FIONBIO, &flags) == SOCKET_ERROR) {
    return false;
  }
  if (connect(sock, name, namelen) != SOCKET_ERROR) {
    return true;
  }
  auto rv = WSAGetLastError();
  if (rv != WSAEWOULDBLOCK && rv != WSAEINPROGRESS) {
    return false;
  }
  rv = IOWaitForTimeout(sock, nes, false);
  if (rv != IO_SUCCESS) {
    return false;
  }
  rv = WSAGetLastError();
  if (rv != 0 && rv != WSAEISCONN) {
    PrintError(rv, L"SockConnectEx");
    return false;
  }
  return true;
}

class WinSocket {
public:
  WinSocket() {
    ///
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    int err = 0;
    if ((err = WSAStartup(wVersionRequested, &wsaData)) != 0) {
      fprintf(stderr, "WSAStartup error %d\n", err);
    } else {
      initialized = true;
    }
  }
  WinSocket(const WinSocket &) = delete;
  WinSocket &operator=(const WinSocket &) = delete;
  ~WinSocket() {
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
    }
    if (initialized) {
      WSACleanup();
    }
  }
  bool IsInitialized() const {
    // return wsocket is initializd.
    return initialized;
  }
  // DialTCP
  bool DialTCP(std::wstring_view address, int port, int timeout = 3000) {
    ADDRINFOW *rhints = NULL;
    ADDRINFOW hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (GetAddrInfoW(address.data(), Integer_to_chars(port, 10).data(), &hints,
                     &rhints) != 0) {
      PrintError(L"GetAddrInfoW()");
      return false;
    }
    auto hi = rhints;
    do {
      sock = socket(rhints->ai_family, SOCK_STREAM, 0);
      if (sock == INVALID_SOCKET) {
        continue;
      }
      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                 sizeof(timeout));
      setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                 sizeof(timeout));
      if (SockConnectEx(sock, hi->ai_addr, (int)hi->ai_addrlen, 30)) {
        break;
      }
      closesocket(sock);
      sock = INVALID_SOCKET;
    } while ((hi = hi->ai_next) != nullptr);

    if (sock == INVALID_SOCKET) {
      PrintError(L"socket/connect()");
      FreeAddrInfoW(rhints); /// Release
      return false;
    }

    FreeAddrInfoW(rhints); /// Release
    return true;
  }
  void PrintError(const wchar_t *reson = L"unknwon") {
    // print error
    auto err = WSAGetLastError();
    ::PrintError(err, reson);
  }
  int Write(const char *data, int datalen) {
    WSABUF wsaData;
    DWORD dwBytes = 0;

    wsaData.len = (u_long)datalen;
    wsaData.buf = (char *)data;
    auto rv = WSASend(sock, &wsaData, 1, &dwBytes, 0, nullptr, nullptr);
    if (rv != SOCKET_ERROR) {
      return dwBytes;
    }
    rv = WSAGetLastError();
    if (rv == WSAEWOULDBLOCK || rv == WSAEINPROGRESS) {
      auto arv = IOWaitForTimeout(sock, 60, false);
      if (arv != IO_SUCCESS) {
        return -1;
      }
      rv = WSASend(sock, &wsaData, 1, &dwBytes, 0, nullptr, nullptr);
      if (rv != SOCKET_ERROR) {
        return dwBytes;
      }
    }
    return rv;
  }
  int Recv(char *buf, size_t len) {
    WSABUF wsaData;
    DWORD dwBytes, flags;
    wsaData.len = (ULONG)len;
    wsaData.buf = buf;

    auto rv = WSARecv(sock, &wsaData, 1, &dwBytes, &flags, nullptr, nullptr);
    if (rv != SOCKET_ERROR) {
      return dwBytes;
    }
    rv = WSAGetLastError();
    if (rv == WSAEWOULDBLOCK || rv == WSAEINPROGRESS) {
      auto arv = IOWaitForTimeout(sock, 60, true);
      if (arv != IO_SUCCESS) {
        return -1;
      }
      rv = WSARecv(sock, &wsaData, 1, &dwBytes, &flags, nullptr, nullptr);
      if (rv != SOCKET_ERROR) {
        return dwBytes;
      }
    }
    return rv;
  }

  using CompleteHandle = std::function<bool(const char *, int)>;
  bool ReadAll(CompleteHandle &&handle) {
    int err = 0;
    for (;;) {
      auto l = Recv(buffer, sizeof(buffer));
      if (l < 0) {
        return false;
      }
      if (l == 0) {
        break;
      }
      if (!handle(buffer, l)) {
        return false;
      }
    }
    return true;
  }

private:
  char buffer[4096];
  SOCKET sock{INVALID_SOCKET};
  bool initialized{false};
};

std::string wstrtou8(std::wstring_view wstr) {
  std::string s;
  auto N = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(),
                               nullptr, 0, nullptr, nullptr);
  if (N > 0) {
    s.resize(N);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &s[0], N,
                        nullptr, nullptr);
  }
  return s;
}

inline std::wstring u8towstr(std::string_view str) {
  std::wstring wstr;
  auto N =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
  if (N > 0) {
    wstr.resize(N);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], N);
  }
  return wstr;
}

std::string NicnameEncode(const std::vector<std::wstring_view> &domains) {
  std::string ec;
  for (const auto &d : domains) {
    ec.append(wstrtou8(d)).append(" ");
  }
  if (ec.size() > 1 && ec.back() == ' ') {
    ec.pop_back(); /// remove end with ' '
  }
  ec.append("\r\n");
  return ec;
}

// whois internal to resolve domain who is
bool whoisInternal(std::wstring_view cinamesrv,
                   const std::vector<std::wstring_view> &domains) {
  constexpr const int cinameport = 43;
  WinSocket ws;
  if (!ws.IsInitialized()) {
    fprintf(stderr, "WSAInitialize error IsInitialized");
    return false;
  }
  if (!ws.DialTCP(cinamesrv, cinameport)) {
    return false;
  }
  auto nd = NicnameEncode(domains);
  if (ws.Write(nd.data(), (int)nd.size()) != (int)nd.size()) {
    fprintf(stderr, "Error write to remote failed");
    return false;
  }
  wprintf(L"NICNAME Server: %s\n", cinamesrv.data());

  auto result = ws.ReadAll([&](const char *data, int len) -> bool {
    //
    if (len > 0) {
      auto w = u8towstr(std::string_view(data, len));
      wprintf(L"%s", w.data());
    }
    return true;
  });

  return true;
}

void usage(const wchar_t *appname) {
  fwprintf(stderr, LR"(usage: %s -h nicnamesrv domain ...
       nicnamesrv default: whois.internic.net
Example:
      whois microsoft.com
)",
           appname);
}

// whois -h hostname domain ...
int wmain(int argc, wchar_t **argv) {
  setlocale(LC_ALL, "");
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }
  std::wstring cinamesrv = L"whois.internic.net";
  std::vector<std::wstring_view> ds;

  for (int i = 1; i < argc; i++) {
    switch (argv[i][0]) {
    case '-':
    case '/':
      if (wcslen(argv[i]) < 2)
        break;
      if (wcscmp(&argv[i][1], L"h") == 0) {
        if (i + 2 < argc) {
          cinamesrv = argv[i + 1];
          i++;
        }
      }
      break;
    default:
      ds.push_back(argv[i]);
      break;
    }
  }
  /// whois
  if (whoisInternal(cinamesrv, ds)) {
    return 1;
  }
  return 0;
}