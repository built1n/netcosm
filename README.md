NetCosm
=======

NetCosm is currently under development. Things /WILL/ break, and
features might drift out of existence without prior warning!

## Installation

### Prerequisities:

libgcrypt
libev
linux >= 2.6.27
glibc >= 2.9

### Compiling

    make

This gives you the executable in `build/unix.bin`.

## Todo List

World persistence (partial)
Inventory
Verbs
Game scripting
NPCs

## Internal Design

A child process is spawned for every client that connects.  There are
two pipes created for every child: a pipe for the child to write to,
and a pipe for the master to write to.

### Child-Master Requests

Many operations, such as listing clients, require the help of the
master process. To request an operation, the child writes it's request
data to it's pipe to the parent. The size of the request MUST be less
than PIPE_BUF, which is 4096 on Linux. The format of the request is as
follows:

    | Child PID | Request Length | Request Code | Data (if any) |

The master polls child pipes for data, and processes any requests it
finds. The master then writes it's data to the pipes of any children
involved in the request, signals them with SIGRTMIN+0, and waits for
all of them to signal back with SIGRTMIN+1. The children read the data
from the master, and handle it accordingly.

TODO: the signal-based ACK framework is really flakey. A better idea
would be to have the child poll for data from the master at intervals,
so the whole mess of signal handling can be avoided.

## Design Goals

Handle 100 simultaneous users sending 100 requests/second with 50ms
latency.
