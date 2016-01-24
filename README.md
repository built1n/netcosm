NetCosm
=======

NetCosm is currently under development. Things /WILL/ break, and
features might drift out of existence without prior warning!

## Installation

### Prerequisites:

* openssl
* libev
* linux >= 3.4 (for "packet mode" pipes)
* glibc >= 2.9

### Compiling

    make

This gives you the executable in `build/unix.bin`.

### Running

If you want to listen on a privileged port (below 1024), you will
either have to run the executable as root (not recommended), or set
the CAP_NET_BIND_SERVICE capability on Linux:

    sudo setcap 'cap_net_bind_service=+ep' build/unix.bin

If running as root, you will need an unprivileged user called 'nobody'
on your system in order for things to work.

## Todo List

* World persistence (partial)
* Inventory
* Verbs
* Game scripting
* NPCs

## Internal Design

### Child-Master Requests

A child process is spawned for every client that connects.  There are
two pipes created for every child: a pipe for the child to write to,
and a pipe for the master to write to.

Both of these pipes are created in "packet mode" (see pipe(2)), and
therefore require at least linux 3.4 and glibc 2.9.

Many operations, such as listing clients, require the help of the
master process. To request an operation, the child writes it's request
data to it's pipe to the parent. The size of the request MUST be less
than PIPE_BUF, which is 4096 on Linux. The format of the request is as
follows:

    | Child PID | Request Code | Data (if any) |

The master polls child pipes for data, and processes any requests it
finds. The master then writes it's data to the pipes of any children
involved in the request.

Child processes that serve connected clients poll for input from the
master process while waiting for client input. The format of these
requests is as follows:

    | Request Code | Data (if any) |

Child requests (that is, requests a child sends to the master) are
very reliable. However, requests from the master process to its child
processes are not so reliable. The child process may not be polling
for data, and so would not receive the request.

## Design Goals

Handle 100 simultaneous users sending 100 requests/second with 50ms
latency.

## Known Bugs

* Telnet server implementation is not fully conforming

* Initial account login time is bogus

* Every subsequent connection allocates slightly more memory than the
  last

## Versioning Scheme

Versions are numbered using the MAJOR.MINOR.BUGFIX scheme.

Current stable is 0.3.0.

Major versions mark major milestones (see below), minor versions mark
incremental milestones and compatibility of data files, and bugfix
versions mark major bugfixes that don't warrant a new major or minor
number.

### Roadmap

#### 0.4.0

* Object support
* User inventory support

#### 0.5.0

* Verb support
* World hooks/scripting

#### 0.6.0

* Dynamic world editing
