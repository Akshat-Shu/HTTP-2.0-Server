#pragma once

enum FdState {
    NEW,
    NONE,
    READY_READ,
    READY_WRITE,
    READ_READ_WRITE,
    CLOSED
};

enum EpollFdType {
    NONE = 0,
    SOCKET = 1,
    CLIENT = 2,
    SERVER = 3
};

class Fd {
public:
    int fd;
    FdState state;
    EpollFdType type = NONE;

    Fd(int fd) : fd(fd), state(NEW) {}

    Fd(int fd, EpollFdType type) : fd(fd), state(NEW), type(type) {}

    void setState(FdState newState) {
        state = newState;
    }

    void setType(EpollFdType newType) {
        type = newType;
    }

    struct find {
        int fd_find;

        bool operator()(const Fd& f) const {
            return f.fd == fd_find;
        }

        bool operator()(const int& f) const {
            return f == fd_find;
        }

        find(int fd) : fd_find(fd) {}
    };

    struct find findByFd() {
        return find(fd);
    }
};