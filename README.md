# HTTP 2.0 Server with SSL Support
A light-weight Web Server built using HTTP 2.0 Protocol with TLS Suppport

## Features
- ### HTTP/2 Protocol Implementation
  - Binary frame handling
  - HPACK header compression
  - Stream multiplexing

- ### TLS/SSL Security
  - ALPN negotiation for HTTP/2
  - Use Secure SSL Certificates and keys

- ### Performance Optimizations
  - Epoll-based event loop for event-based polling
  - Frame chunking for large file transfers
  - Memory-efficient buffer management
  - Multi-Threading for Server-side Response

- ### Content Handling
  - Static file serving with proper MIME type detection
  - Dynamic directory listing
  - Image/PDF/HTML/JS/CSS support

![Screencast from 2025-06-16 22-26-24 webm](https://github.com/user-attachments/assets/04a0b186-d621-407f-8cb9-6ee3d857ab07)

## Libraries
- Modified version of [libhttp2](https://github.com/chronos-tachyon/libhttp2) for frame encoding/decoding
- OpenSSL for Certificate Management and TLS support

## References
- [RFC 7540](https://datatracker.ietf.org/doc/html/rfc7540)
- [Web Concepts](https://webconcepts.info/concepts/)
- [HTTP/2 and how it works.](https://cabulous.medium.com/http-2-and-how-it-works-9f645458e4b2)
- [The method to epoll's madness](https://copyconstruct.medium.com/the-method-to-epolls-madness-d9d2d6378642)
- [HTTP 2 in action](https://dl.ebooksworld.ir/motoman/Manning.HTTP2.in.Action.www.EBooksWorld.ir.pdf)
