enum StreamState {
    IDLE = 0,
    RESERVED_LOCAL = 1,
    RESERVED_REMOTE = 2,
    OPEN = 3,
    HALF_CLOSED_LOCAL = 4,
    HALF_CLOSED_REMOTE = 5,
    CLOSED = 6
};

const int UNSET = -1;

class Stream {
public:
    int id;
    char weight;
    StreamState state;

    Stream(int id, char weight = 0, StreamState state = IDLE)
        : id(id), weight(weight), state(state) {}

    Stream() : id(UNSET), weight(0), state(IDLE) {}
};