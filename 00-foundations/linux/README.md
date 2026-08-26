# Linux for Backend Engineers

> Practical Linux knowledge for systems programming, debugging production incidents, and understanding what happens beneath your application layer.

---

## Table of Contents

1. [Process Management](#process-management)
2. [File Descriptors](#file-descriptors)
3. [Permissions](#permissions)
4. [Users and Groups](#users-and-groups)
5. [Environment Variables](#environment-variables)
6. [systemd](#systemd)
7. [Cron](#cron)
8. [Debugging Commands](#debugging-commands)
9. [The /proc Filesystem](#the-proc-filesystem)
10. [Inodes and Links](#inodes-and-links)
11. [Disk and Memory Tools](#disk-and-memory-tools)
12. [Log Locations](#log-locations)
13. [Interview Q&A](#interview-qa)

---

## Process Management

Every running program is a process. Each process has a unique **PID** (Process ID), a **PPID** (parent PID), an owner, memory mappings, open file descriptors, and a state.

### Process States

```
R  Running (or runnable, waiting for CPU)
S  Interruptible sleep (waiting for I/O, signal, timer)
D  Uninterruptible sleep (waiting for disk I/O — cannot be killed)
Z  Zombie (exited but parent hasn't called wait())
T  Stopped (by SIGSTOP or a debugger)
```

### Viewing Processes

```bash
# Snapshot of all processes
ps aux

# Process tree — shows parent/child relationships
ps auxf

# Example output columns:
# USER  PID  %CPU  %MEM  VSZ    RSS    TTY  STAT  START  TIME  COMMAND
# root  1234  0.1   2.3   512000 94208  ?    Ss    10:00  0:01  /usr/bin/node server.js

# Watch processes in real time (refresh every 2s)
top

# top key bindings:
#   P  sort by CPU
#   M  sort by memory
#   k  kill a process
#   q  quit

# Better version of top
htop
# htop shows a tree view, individual CPU cores, and supports mouse clicks
```

**VSZ vs RSS:**
- **VSZ** (Virtual Size): all memory the process *could* use, including shared libs and memory-mapped files that haven't been loaded yet.
- **RSS** (Resident Set Size): memory actually in RAM right now. Use RSS for real memory pressure.

### Signals

Signals are asynchronous notifications sent to a process. The kernel delivers them; the process handles (or ignores) them.

```bash
# List all signals
kill -l

# Send SIGTERM (15) — polite shutdown request
kill -SIGTERM 1234
kill -15 1234
kill 1234    # SIGTERM is the default

# Send SIGKILL (9) — immediate, unblockable termination
kill -SIGKILL 1234
kill -9 1234

# Send SIGHUP (1) — reload config (convention for daemons)
kill -SIGHUP 1234

# Kill by name
pkill -SIGTERM nginx
killall node

# Send signal to process group
kill -SIGTERM -1234   # negative PID = process group
```

| Signal | Number | Default Action | Catchable | Notes |
|--------|--------|----------------|-----------|-------|
| SIGTERM | 15 | Terminate | Yes | Request graceful shutdown; should clean up |
| SIGKILL | 9 | Terminate | **No** | Cannot be caught, blocked, or ignored |
| SIGHUP | 1 | Terminate | Yes | Terminal closed; daemons use it as "reload" |
| SIGINT | 2 | Terminate | Yes | Ctrl+C from terminal |
| SIGQUIT | 3 | Core dump | Yes | Ctrl+\ — terminate + dump |
| SIGSTOP | 19 | Stop | **No** | Pause process; cannot be caught |
| SIGCONT | 18 | Continue | Yes | Resume a stopped process |
| SIGUSR1/2 | 10/12 | Terminate | Yes | Application-defined; Node.js uses SIGUSR1 for debug |
| SIGCHLD | 17 | Ignore | Yes | Sent to parent when child exits |

**Why SIGKILL can't be caught:** It is handled entirely by the kernel, never delivered to user space. The process has no opportunity to run any code. This means open files may not be flushed, locks may not be released, and temp files may remain.

**SIGHUP and daemons:** When a terminal closes, SIGHUP is sent to its foreground process group. Daemons (already detached from terminals) repurpose SIGHUP as "re-read config without restarting." nginx, sshd, and most Unix daemons follow this convention.

```bash
# Reload nginx config without dropping connections
kill -SIGHUP $(cat /var/run/nginx.pid)
# or
nginx -s reload
```

### Background and Foreground Jobs

```bash
# Run in background
node server.js &

# List jobs in current shell
jobs

# Bring job 1 to foreground
fg %1

# Send running process to background
# (Ctrl+Z to suspend, then:)
bg %1

# nohup — keep running after shell exits (immune to SIGHUP)
nohup node server.js > /var/log/app.log 2>&1 &

# Check if a process is running
kill -0 $PID && echo "running" || echo "not running"
# kill -0 checks existence without sending a signal
```

### Process Priority (nice/renice)

```bash
# Start process with lower priority (nice value 10, range -20 to 19)
nice -n 10 ./cpu-intensive-job.sh

# Change priority of running process
renice -n 5 -p 1234

# Negative nice = higher priority (requires root)
sudo nice -n -10 ./critical-process
```

---

## File Descriptors

A file descriptor (fd) is an integer that identifies an open file, socket, pipe, or device within a process. The kernel maintains a per-process table mapping fd numbers to file descriptions (which include the open flags, offset, and a reference to the underlying file object).

### Standard File Descriptors

```
0  stdin   — standard input
1  stdout  — standard output
2  stderr  — standard error
```

These are opened automatically for every new process (inherited from parent).

```bash
# Redirect stdout to a file
node server.js > output.log

# Redirect stderr to a file
node server.js 2> errors.log

# Redirect both to the same file
node server.js > app.log 2>&1
# 2>&1 means "fd 2 should point to where fd 1 currently points"

# Discard stdout, keep stderr
node server.js > /dev/null

# Discard everything
node server.js > /dev/null 2>&1

# /dev/null: a special file that discards all writes and returns EOF on reads
```

### Pipes and FD Inheritance

A pipe `|` connects stdout of one process to stdin of the next via a kernel buffer (default 64KB on Linux).

```bash
# The shell creates a pipe before forking
ps aux | grep node | grep -v grep

# Named pipe (FIFO) — persists on filesystem
mkfifo /tmp/my-pipe
cat /tmp/my-pipe &   # reader blocks
echo "hello" > /tmp/my-pipe  # writer unblocks reader
```

### Inspecting Open File Descriptors

```bash
# List all open files by a process
lsof -p 1234

# Show file descriptors symbolically
ls -la /proc/1234/fd

# Example output:
# lrwxrwxrwx 1 root root 64 ... 0 -> /dev/pts/0    (stdin: terminal)
# lrwxrwxrwx 1 root root 64 ... 1 -> /var/log/app.log
# lrwxrwxrwx 1 root root 64 ... 2 -> /var/log/app.log
# lrwxrwxrwx 1 root root 64 ... 3 -> socket:[12345]  (TCP socket)
# lrwxrwxrwx 1 root root 64 ... 4 -> pipe:[67890]

# File descriptor limits
ulimit -n          # per-process limit (soft)
ulimit -Hn         # hard limit
cat /proc/sys/fs/file-max   # system-wide limit

# Raise limit for current shell session
ulimit -n 65536

# Set permanently in /etc/security/limits.conf:
# * soft nofile 65536
# * hard nofile 65536
```

**fd exhaustion:** When a process hits its fd limit, `open()`, `socket()`, and `accept()` return `EMFILE: Too many open files`. Common cause: file/socket leak (not closing connections). Diagnose with `lsof -p PID | wc -l`.

---

## Permissions

### The Permission Bits

Every file and directory has an owner (user), a group, and three sets of rwx permissions: owner, group, others.

```
-rwxr-xr-- 1 alice devs 4096 Jan 1 10:00 script.sh
│└──┘└──┘└──┘
│ │    │   └── others: r-- (read only)
│ │    └─────── group:  r-x (read + execute)
│ └──────────── owner:  rwx (read + write + execute)
└────────────── file type: - = regular, d = dir, l = symlink, s = socket
```

### Octal Notation

```
r = 4
w = 2
x = 1

rwx = 7
rw- = 6
r-x = 5
r-- = 4
--- = 0

chmod 755 file   # rwxr-xr-x
chmod 644 file   # rw-r--r--
chmod 600 file   # rw------- (SSH private keys must be 600)
chmod 777 file   # rwxrwxrwx (never do this in production)
```

```bash
# Change permissions
chmod 755 script.sh
chmod u+x script.sh     # add execute for owner
chmod go-w file.txt     # remove write from group and others
chmod -R 755 /var/www   # recursive

# Change owner
chown alice file.txt
chown alice:devs file.txt
chown -R www-data:www-data /var/www/html

# Change group only
chgrp devs project/
```

### umask

`umask` defines which permission bits are *masked out* (subtracted) when creating new files and directories.

```bash
umask          # shows current mask, e.g., 0022
umask 027      # set mask: new files = 640, new dirs = 750

# How it works:
# Default file permissions: 666 (rw-rw-rw-)
# Default dir permissions:  777 (rwxrwxrwx)
# umask 022:
#   file: 666 & ~022 = 666 & 755 = 644 (rw-r--r--)
#   dir:  777 & ~022 = 777 & 755 = 755 (rwxr-xr-x)

# umask 027:
#   file: 666 & ~027 = 640 (rw-r-----)  — others get nothing
#   dir:  777 & ~027 = 750 (rwxr-x---)

# Set in /etc/profile or ~/.bashrc for persistence
```

### Special Bits: setuid, setgid, sticky

```bash
# setuid (4000): execute as the file's owner, not the caller
# Classic example: /usr/bin/passwd (owned by root, needs to write /etc/shadow)
ls -la /usr/bin/passwd
# -rwsr-xr-x 1 root root ... /usr/bin/passwd
#     ^ s = setuid + execute

chmod u+s /usr/bin/my-privileged-tool
chmod 4755 binary    # octal notation

# setgid (2000) on a file: execute with file's group
# setgid on a directory: new files inherit the directory's group
chmod g+s /shared/project-dir
chmod 2755 /shared/project-dir

# Sticky bit (1000) on a directory: only owner can delete their own files
# /tmp uses this so users can't delete each other's temp files
ls -la /tmp
# drwxrwxrwt ... /tmp   (t = sticky + execute)
chmod +t /shared/uploads
chmod 1777 /tmp
```

---

## Users and Groups

```bash
# Show current user and groups
whoami
id
id alice    # show another user's info

# User database
cat /etc/passwd
# Format: username:x:UID:GID:comment:home:shell
# alice:x:1001:1001:Alice Smith:/home/alice:/bin/bash
# x = password is in /etc/shadow

# Group database
cat /etc/group
# Format: groupname:x:GID:members
# devs:x:1002:alice,bob

# Add/modify users
useradd -m -s /bin/bash -G sudo alice    # create with home dir, bash shell, sudo group
usermod -aG docker alice                 # add alice to docker group (append, don't replace)
userdel -r alice                         # delete user and home dir

# Switch users
su - alice       # login shell as alice (reads alice's profile)
sudo -u alice command

# Elevate to root
sudo bash        # interactive root shell

# Password management
passwd alice     # set password (as root)
passwd           # change own password

# Locked/unlocked accounts
passwd -l alice  # lock (disables login)
passwd -u alice  # unlock

# /etc/sudoers — who can run what as root
visudo           # always use visudo, it validates before saving
# alice ALL=(ALL:ALL) ALL
# %devs ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart myapp
```

---

## Environment Variables

Environment variables are key-value pairs inherited by child processes from their parent. They control program behavior without requiring code changes.

```bash
# Show all environment variables
env
printenv

# Show a specific variable
echo $PATH
printenv HOME

# Set a variable in current shell (not inherited by children)
MY_VAR=hello

# Export to child processes
export MY_VAR=hello
export DATABASE_URL="postgres://user:pass@localhost:5432/mydb"

# Set for a single command only
DATABASE_URL="postgres://..." node server.js

# Unset
unset MY_VAR

# PATH: colon-separated list of directories searched for executables
echo $PATH
# /usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Add directory to PATH
export PATH="$HOME/.local/bin:$PATH"
```

### Initialization Files (Load Order)

```
Login shell (ssh, su -, console login):
  /etc/profile
  /etc/profile.d/*.sh
  ~/.bash_profile  (or ~/.profile if .bash_profile missing)
  ~/.bashrc (sourced from .bash_profile typically)

Interactive non-login shell (new terminal tab):
  /etc/bash.bashrc
  ~/.bashrc

Non-interactive shell (scripts):
  Only $BASH_ENV if set

/etc/environment:
  Key=value pairs, no export needed, read by PAM
  Used for system-wide variables (JAVA_HOME, etc.)
```

```bash
# /etc/profile — system-wide for login shells
# /etc/profile.d/ — drop-in scripts, sourced by /etc/profile
# ~/.bashrc — user interactive shell config
# ~/.bash_profile — user login shell config

# Make a variable permanent for one user
echo 'export MY_VAR=hello' >> ~/.bashrc
source ~/.bashrc   # apply without logging out

# System-wide permanent variable
echo 'MY_VAR=hello' >> /etc/environment
# Takes effect at next login (PAM reads this)
```

---

## systemd

systemd is the init system (PID 1) on most modern Linux distributions. It manages services, mounts, timers, and the boot sequence.

### Unit Files

Unit files are INI-style configs that describe a resource systemd manages.

```ini
# /etc/systemd/system/myapp.service

[Unit]
Description=My Node.js Application
Documentation=https://github.com/myorg/myapp
After=network.target postgresql.service
Requires=postgresql.service

[Service]
Type=simple
User=www-data
Group=www-data
WorkingDirectory=/opt/myapp
Environment=NODE_ENV=production
EnvironmentFile=/etc/myapp/env
ExecStart=/usr/bin/node /opt/myapp/server.js
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=myapp

# Resource limits
LimitNOFILE=65536
MemoryLimit=512M

[Install]
WantedBy=multi-user.target
```

**Key directives:**

| Directive | Meaning |
|-----------|---------|
| `After=` | Start order only (no hard dependency) |
| `Requires=` | Hard dependency; if dependency fails, this unit fails |
| `Wants=` | Soft dependency; starts dependency but continues if it fails |
| `Type=simple` | ExecStart process is the main process |
| `Type=forking` | ExecStart forks; parent exits; systemd tracks child |
| `Type=notify` | Process sends sd_notify() when ready |
| `Restart=always` | Always restart, regardless of exit code |
| `Restart=on-failure` | Restart only on non-zero exit or signal |
| `EnvironmentFile=` | Read key=value pairs from a file |
| `WantedBy=multi-user.target` | Enable at runlevel 3 (multi-user, no GUI) |

### systemctl Commands

```bash
# Service management
systemctl start myapp          # start now
systemctl stop myapp           # stop now
systemctl restart myapp        # stop then start
systemctl reload myapp         # send SIGHUP (if supported)
systemctl status myapp         # current state + last log lines

# Boot persistence
systemctl enable myapp         # create symlink → start at boot
systemctl disable myapp        # remove symlink
systemctl enable --now myapp   # enable AND start immediately

# Reload unit file changes
systemctl daemon-reload        # must run after editing unit files

# List units
systemctl list-units --type=service
systemctl list-units --state=failed

# Check if service is active/enabled
systemctl is-active myapp
systemctl is-enabled myapp

# Show unit file
systemctl cat myapp

# Show all properties
systemctl show myapp

# Mask (hard-disable, cannot be started even manually)
systemctl mask myapp
systemctl unmask myapp
```

### journalctl — Reading Logs

```bash
# All logs for a service (most recent last)
journalctl -u myapp

# Follow in real time
journalctl -u myapp -f

# Last 100 lines
journalctl -u myapp -n 100

# Since a specific time
journalctl -u myapp --since "2024-01-15 10:00:00"
journalctl -u myapp --since "1 hour ago"

# Since last boot
journalctl -u myapp -b

# Filter by priority (0=emerg, 3=err, 6=info, 7=debug)
journalctl -u myapp -p err

# Show logs for current boot in reverse order
journalctl -b -r

# Disk usage by journal
journalctl --disk-usage

# Purge old logs
journalctl --vacuum-size=500M
journalctl --vacuum-time=30d
```

### systemd Timers (replacing cron)

```ini
# /etc/systemd/system/cleanup.timer
[Unit]
Description=Run cleanup daily

[Timer]
OnCalendar=daily
Persistent=true   # run if missed while system was off

[Install]
WantedBy=timers.target
```

```bash
systemctl enable --now cleanup.timer
systemctl list-timers   # show all timers and next run time
```

---

## Cron

Cron runs commands on a schedule. Each user has a crontab; system-wide jobs live in `/etc/cron.d/` and `/etc/crontab`.

### Crontab Syntax

```
┌───────────── minute (0–59)
│ ┌───────────── hour (0–23)
│ │ ┌───────────── day of month (1–31)
│ │ │ ┌───────────── month (1–12 or Jan–Dec)
│ │ │ │ ┌───────────── day of week (0–7, 0 and 7 = Sunday)
│ │ │ │ │
* * * * *  command to execute
```

```bash
# Edit your crontab
crontab -e

# List crontab
crontab -l

# Remove crontab
crontab -r

# Examples:
# Run every minute
* * * * * /usr/bin/check-health.sh

# Run at 2:30 AM every day
30 2 * * * /opt/myapp/scripts/backup.sh

# Run every 15 minutes
*/15 * * * * /usr/bin/sync-data.sh

# Run at midnight on the 1st of every month
0 0 1 * * /usr/bin/monthly-report.sh

# Run at 9 AM on weekdays only
0 9 * * 1-5 /usr/bin/send-standup-reminder.sh

# Run at 6 AM and 6 PM
0 6,18 * * * /usr/bin/twice-daily-task.sh

# System crontab (/etc/crontab) has a user field:
30 2 * * * root /usr/bin/system-backup.sh
```

```bash
# Capture output (cron has no terminal — errors go to email or /dev/null)
*/5 * * * * /opt/app/health-check.sh >> /var/log/health-check.log 2>&1

# Environment in cron is minimal — always use full paths
# PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
```

---

## Debugging Commands

### Network Connections

```bash
# ss — socket statistics (modern replacement for netstat)
ss -lntp    # listening TCP sockets with process info
# -l  listening
# -n  numeric (no hostname resolution)
# -t  TCP only
# -p  show process

# Example output:
# State  Recv-Q Send-Q  Local Address:Port   Peer Address:Port  Process
# LISTEN 0      128          0.0.0.0:3000         0.0.0.0:*      users:(("node",pid=1234,fd=20))
# LISTEN 0      128          0.0.0.0:22           0.0.0.0:*      users:(("sshd",pid=567,fd=3))

ss -tnp     # established TCP connections
ss -s       # summary statistics

# Show all UDP sockets
ss -unp

# Find what's on a port
ss -lntp 'sport = :3000'
fuser 3000/tcp    # simpler: just show PID

# netstat (older, may not be installed)
netstat -lntp
netstat -an | grep ESTABLISHED | wc -l   # count established connections
```

### Open Files

```bash
# lsof — list open files
lsof -i          # all network connections
lsof -i :3000    # what's using port 3000
lsof -i tcp      # TCP only
lsof -p 1234     # all files open by PID 1234
lsof -u alice    # all files open by user alice
lsof /var/log/app.log   # what processes have this file open

# Find deleted files still held open (consuming disk space)
lsof | grep deleted
# If a log file is deleted but a process still has it open,
# disk space isn't freed — truncate via /proc/PID/fd/N
> /proc/1234/fd/7   # zero out the file without closing fd
```

### System Calls

```bash
# strace — trace system calls made by a process
strace ls /tmp
strace -p 1234            # attach to running process
strace -p 1234 -e trace=network   # only network syscalls
strace -p 1234 -e openat,read,write -o /tmp/trace.txt
strace -c command         # summarize syscall counts and time

# Useful patterns:
strace -e trace=open,openat command   # what files is it opening?
strace -e trace=connect command       # what connections is it making?
strace -f -p 1234                     # follow forks (trace child processes too)

# ltrace — trace library calls (slower than strace)
ltrace ./my-binary
```

### Network Traffic

```bash
# tcpdump basics
tcpdump -i eth0                        # capture on interface eth0
tcpdump -i any port 3000               # any interface, port 3000
tcpdump -i eth0 host 192.168.1.100    # traffic to/from specific host
tcpdump -i eth0 -w /tmp/capture.pcap  # write to file (open in Wireshark)
tcpdump -i eth0 -n -v                 # verbose, no hostname resolution
tcpdump -i lo port 5432               # PostgreSQL traffic on loopback
tcpdump -i eth0 'tcp[tcpflags] & tcp-syn != 0'  # only SYN packets

# Read a capture file
tcpdump -r /tmp/capture.pcap

# Quick HTTP sniffing
tcpdump -i eth0 -A -s 0 'tcp port 80'   # -A = ASCII output, -s 0 = full packets
```

### Process and System State

```bash
# strace for processes in D state (uninterruptible sleep)
# These processes are blocked on kernel I/O — strace won't help
# Instead, check what they're waiting on:
cat /proc/1234/wchan   # kernel function the process is waiting in

# Check for stuck NFS mounts or hung I/O
dmesg | tail -50
dmesg | grep -i error

# System call summary
perf stat command       # CPU event counters
perf top                # real-time profiling

# Interrupted system calls
strace -e 'trace=!all,signal' -p 1234   # watch signals only
```

---

## The /proc Filesystem

`/proc` is a virtual filesystem — it has no disk backing. The kernel generates its content on read. It exposes live kernel data structures as files.

### Per-Process Information

```bash
# Every running process has a directory /proc/PID/
ls /proc/1234/

# Key entries:
# fd/       — symlinks to every open file descriptor
# fdinfo/   — fd details (position, flags)
# maps      — memory maps (code, heap, stack, shared libs)
# smaps     — detailed per-region memory stats
# status    — human-readable process state
# stat      — machine-readable stats (used by ps/top)
# cmdline   — null-separated argv (the command that started it)
# environ   — null-separated environment variables
# exe       — symlink to the executable
# cwd       — symlink to current working directory
# io        — I/O statistics (bytes read/written)
# net/      — network stats for the process's namespace
# limits    — current resource limits (ulimit values)
# wchan     — kernel function the process is waiting in

# Read the command that started a process
cat /proc/1234/cmdline | tr '\0' ' '

# See environment variables
cat /proc/1234/environ | tr '\0' '\n'

# Check memory maps
cat /proc/1234/maps
# Address           Perms  Offset  Dev  Inode  Pathname
# 55a3b2000000-55a3b2009000 r--p 0 08:01 1234  /usr/bin/node

# File descriptors
ls -la /proc/1234/fd
# 0 -> /dev/pts/0    (stdin)
# 1 -> /var/log/app.log
# 3 -> socket:[12345]

# I/O statistics
cat /proc/1234/io
# rchar: 1234567    (bytes read, including cache)
# wchar: 890123     (bytes written, including cache)
# read_bytes: 4096  (bytes actually read from disk)
# write_bytes: 0
```

### System-Wide /proc Files

```bash
# CPU information
cat /proc/cpuinfo
# processor, vendor_id, model name, cores, flags (sse4, avx, etc.)
grep -c processor /proc/cpuinfo   # number of logical CPUs

# Memory information
cat /proc/meminfo
# MemTotal:    16384000 kB
# MemFree:      2048000 kB
# MemAvailable: 8192000 kB   ← this is what matters (free + reclaimable cache)
# Buffers:       256000 kB   ← kernel I/O buffers
# Cached:       4096000 kB   ← page cache (files read from disk)
# SwapTotal:    2097152 kB
# SwapFree:     2097152 kB

# System uptime
cat /proc/uptime
# 12345.67 49382.68  (uptime seconds, idle seconds)

# Kernel version
cat /proc/version

# Loaded kernel modules
cat /proc/modules

# System calls and IRQ stats
cat /proc/stat

# Network interface stats
cat /proc/net/dev

# TCP connections (raw hex — use ss instead)
cat /proc/net/tcp

# File descriptor limits
cat /proc/sys/fs/file-max    # system-wide max open files
cat /proc/sys/fs/file-nr     # (allocated, free, max)

# TCP tuning parameters
cat /proc/sys/net/ipv4/tcp_max_syn_backlog
cat /proc/sys/net/core/somaxconn

# Modify kernel parameters at runtime
echo 65536 > /proc/sys/fs/file-max
sysctl -w net.core.somaxconn=65535
sysctl -a   # show all parameters
# Persist in /etc/sysctl.conf or /etc/sysctl.d/
```

---

## Inodes and Links

### Inodes

An inode is a data structure on disk that stores file metadata: permissions, owner, timestamps, size, and pointers to data blocks. It does NOT store the filename. Filenames exist only in directory entries, which map name → inode number.

```bash
# Show inode numbers
ls -li /etc/passwd
# 1234567 -rw-r--r-- 1 root root 2048 Jan 1 10:00 /etc/passwd
# └─────┘ inode number

# Show inode usage
df -i
# Filesystem       Inodes  IUsed  IFree  IUse%  Mounted on
# /dev/sda1       6553600  12345  6541255   1%   /

# Inode exhaustion: can't create files even with disk space available
# Diagnose: df -i shows IUse% near 100%
# Common cause: millions of tiny files (email spools, session files, temp files)

# Stat a file — shows inode details
stat /etc/passwd
# File: /etc/passwd
# Size: 2048    Blocks: 8    IO Block: 4096   regular file
# Inode: 1234567    Links: 1
# Access: 2024-01-01 10:00:00
# Modify: 2024-01-01 10:00:00
# Change: 2024-01-01 10:00:00  ← inode metadata change time (ctime)
```

### Hard Links vs Symbolic Links

```bash
# Hard link — creates another directory entry pointing to the same inode
ln /etc/passwd /tmp/passwd-hardlink
# Both names point to inode 1234567
# Deleting one doesn't affect the other
# File data is freed only when link count reaches 0

# Limitations of hard links:
# - Cannot span filesystems (must be on same partition)
# - Cannot link to directories (prevents cycles)

# Symbolic (soft) link — a file that contains a path to another file
ln -s /etc/passwd /tmp/passwd-symlink
# The symlink file contains the string "/etc/passwd"
# It's a separate inode (different number)

ls -la /tmp/passwd-symlink
# lrwxrwxrwx 1 root root 11 ... /tmp/passwd-symlink -> /etc/passwd

# If target is deleted, symlink becomes a dangling link
rm /etc/passwd
cat /tmp/passwd-symlink  # Error: No such file or directory

# Find dangling symlinks
find /var/www -maxdepth 3 -type l ! -exec test -e {} \; -print

# Difference summary:
# Hard link:  same inode, same permissions/owner, no target concept
# Soft link:  different inode, permissions are lrwxrwxrwx, has a target path
```

---

## Disk and Memory Tools

### Disk Usage

```bash
# Filesystem usage (human-readable)
df -h
# Filesystem      Size  Used Avail Use%  Mounted on
# /dev/sda1        50G   23G   25G  48%  /
# tmpfs           3.9G  1.2M  3.9G   1%  /dev/shm

# Which directory is eating disk?
du -sh /*          # top-level directories
du -sh /var/*      # drill down
du -sh /var/log/*  # find the log eating disk

# Sort by size
du -h /var/log | sort -rh | head -20

# Find large files
find / -type f -size +1G 2>/dev/null | sort
find /var/log -type f -name "*.log" -size +100M

# Disk I/O monitoring
iostat -x 1    # extended stats, 1-second interval
# %util near 100% = disk is saturated
# await = average I/O wait time in milliseconds

iotop          # per-process I/O (like top but for I/O)
```

### Memory Tools

```bash
# High-level memory summary
free -h
#               total    used    free   shared  buff/cache   available
# Mem:           15Gi    4.2Gi   2.1Gi  512Mi     9.2Gi       10Gi
# Swap:         2.0Gi    100Mi   1.9Gi

# "available" is the key number — not "free"
# Linux uses free RAM for disk cache; cache is returned to apps on demand
# available = free + reclaimable cache

# vmstat — virtual memory statistics
vmstat 1 10    # 10 samples, 1 second apart
# procs ----memory---- ---swap-- ---io--- -system-- -----cpu-----
# r  b  swpd  free  buff  cache  si  so  bi  bo  in  cs  us sy id wa
# 2  0  1024 2048000 256000 4096000 0 0 0 100 500 1200 15 5 79 1
#
# r  = run queue length (processes waiting for CPU)
# b  = blocked (waiting for I/O)
# si = swap in (KB/s)  ← nonzero means memory pressure
# so = swap out (KB/s) ← nonzero means memory pressure
# us/sy/id/wa = user/system/idle/iowait CPU %

# Detailed per-process memory
cat /proc/1234/status | grep -i vm
# VmPeak:  512000 kB    peak virtual size
# VmSize:  490000 kB    current virtual size
# VmRSS:    94208 kB    resident set size (actual RAM)
# VmSwap:    1024 kB    in swap

# top memory columns
top -o %MEM
# VIRT = virtual memory (includes everything, mapped but not loaded)
# RES  = resident (physical RAM in use)
# SHR  = shared memory (shared libs counted in multiple processes' RES)

# Memory overcommit settings
cat /proc/sys/vm/overcommit_memory
# 0 = heuristic (default): allow reasonable overcommit
# 1 = always allow (dangerous but max performance)
# 2 = never overcommit beyond (overcommit_ratio% of RAM + swap)

# OOM killer
dmesg | grep -i "killed process"   # see if OOM killer fired
# Scores in /proc/PID/oom_score (higher = more likely to be killed)
cat /proc/1234/oom_score

# Page cache size
cat /proc/meminfo | grep -E "Cached|Buffers"
# Drop caches (do not do this in production — hurts performance)
echo 3 > /proc/sys/vm/drop_caches
```

---

## Log Locations

```bash
# System log (older systems use syslog/rsyslog)
/var/log/syslog        # Ubuntu/Debian: general system messages
/var/log/messages      # RHEL/CentOS: general system messages

# Authentication and authorization
/var/log/auth.log      # Ubuntu/Debian: login attempts, sudo, PAM
/var/log/secure        # RHEL/CentOS: same

# Kernel messages
/var/log/kern.log      # kernel ring buffer (also: dmesg)
dmesg -T               # with human-readable timestamps

# Application package manager
/var/log/dpkg.log      # Debian: apt installs/removes
/var/log/yum.log       # RHEL: yum installs

# Web servers
/var/log/nginx/access.log
/var/log/nginx/error.log
/var/log/apache2/access.log
/var/log/apache2/error.log

# PostgreSQL
/var/log/postgresql/postgresql-*.log

# systemd-managed services
journalctl -u servicename
# By default stored in /run/log/journal/ (volatile) or
# /var/log/journal/ (persistent — create dir to enable)

# Monitoring log rotation
cat /etc/logrotate.conf
ls /etc/logrotate.d/

# Last logins
last           # login/logout history from /var/log/wtmp
lastb          # failed login attempts from /var/log/btmp
lastlog        # last login per user
who            # currently logged-in users
w              # logged-in users + what they're doing
```

---

## Interview Q&A

### What happens when you run a command in the shell?

1. **Shell parses** the command line (tokenizes, expands variables, globs).
2. **Shell calls `fork()`** — the kernel creates a near-identical copy of the shell process. The child has the same memory, fds, and environment, but a different PID.
3. **Child calls `exec()`** (specifically `execve()`) with the command path and arguments. The kernel:
   - Finds the file, checks execute permission
   - Reads the interpreter line (`#!/bin/bash`) if a script
   - Replaces the child's memory image with the new program
   - Sets up the stack with `argv`, `envp`, `argc`
   - Sets the instruction pointer to the new program's `main()`
4. **Shell waits** (calls `wait()` or `waitpid()`) for the child to exit.
5. **Child exits**, kernel sends `SIGCHLD` to parent, parent's `wait()` returns.
6. Exit status becomes available via `$?`.

```bash
# Watch the syscalls
strace -e trace=execve,fork,clone,wait4 ls /tmp
```

### How does `kill` work?

`kill` doesn't actually kill anything — it sends a signal. The sequence:

1. User calls `kill -SIGTERM 1234`.
2. The `kill` command calls the `kill(2)` syscall: `kill(1234, SIGTERM)`.
3. The kernel checks that the sender has permission (same UID, or root, or certain specific cases).
4. The kernel sets a pending signal bit in the target process's task_struct.
5. The next time the target process is scheduled (or woken from sleep), the kernel checks for pending signals before returning to user space.
6. If the process has a handler registered (via `signal()` or `sigaction()`), the kernel redirects execution to the handler.
7. If no handler and default action is terminate, the kernel terminates the process.

SIGKILL never reaches the process — the kernel handles it directly in step 5 without giving user space a chance to run.

### What is a zombie process?

A zombie (defunct) process is one that has exited but whose entry in the process table hasn't been removed because the parent hasn't called `wait()` to collect its exit status.

```
State: Z (zombie) in ps output
Example: [python3] <defunct>
```

The zombie holds no memory, no CPU, no file descriptors — just a process table slot and an exit status the kernel preserves for the parent.

**Causes:**
- Parent is buggy and doesn't call `wait()`
- Parent is processing a large batch and hasn't gotten around to it yet

**Consequences:**
- Process table slots are limited (~32768 by default)
- A large number of zombies can prevent new process creation

**Resolution:**
- Fix the parent to call `wait()` (or use `SIGCHLD` handler)
- Kill the parent — orphaned zombies are reparented to `init`/systemd (PID 1), which calls `wait()` automatically
- You cannot kill a zombie with SIGKILL (it has no process to deliver the signal to)

```bash
# Find zombies
ps aux | grep 'Z'
ps -eo pid,ppid,stat,command | grep '^.\{1,\} Z'

# Kill zombie's parent to trigger reparenting to init
kill -SIGTERM $(ps -o ppid= -p <zombie_pid>)
```

### What is the difference between a process and a thread in Linux?

In Linux, both are called **tasks** at the kernel level and created with `clone()`. The difference is which resources they share.

| | Process | Thread (LWP) |
|---|---|---|
| Creation | `fork()` → copies task, COW memory | `clone()` with CLONE_VM, CLONE_FILES, CLONE_SIGHAND |
| Memory | Separate address space (copy-on-write) | Shared address space |
| File descriptors | Separate fd table (copy of parent's) | Shared fd table |
| PID | Own PID | Own PID, same TGID as parent thread |
| Communication | Signals, pipes, shared memory, sockets | Direct: shared memory (variables) |
| Crash isolation | Crash doesn't affect parent | Crash (segfault) kills entire process |

```bash
# Threads appear as separate entries in /proc with same TGID
ls /proc/1234/task/   # one directory per thread
# 1234/  1235/  1236/  (main thread + 2 worker threads)

# ps shows threads with -L flag
ps -Lf -p 1234
# UID   PID  PPID   LWP C NLWP ...
# app  1234  1000  1234 0    3  ...  node server.js
# app  1234  1000  1235 0    3  ...  node server.js
# app  1234  1000  1236 0    3  ...  node server.js
```

The reason Linux treats them uniformly: `clone()` takes flags controlling which namespaces and resources to share. A thread is just a clone that shares everything; a process shares almost nothing.

### What does `2>&1` mean?

It means "redirect file descriptor 2 (stderr) to wherever fd 1 (stdout) currently points."

Order matters:

```bash
# CORRECT: stdout goes to file, then stderr is sent to the same place
command > file.txt 2>&1

# WRONG: stderr goes to current stdout (terminal), then stdout goes to file
# stderr still goes to terminal because &1 was evaluated before the redirect
command 2>&1 > file.txt
```

### What is the difference between `>` and `>>`?

- `>` truncates the file (creates or replaces)
- `>>` appends to the file (creates if absent)

Never use `>` on a live log file — you'll lose existing content and potentially corrupt the file if another process is writing to it. Use logrotate instead.

### How do you find which process is listening on port 443?

```bash
ss -lntp 'sport = :443'
# or
lsof -i :443
# or
fuser 443/tcp

# All three show the PID; then:
ps -p <PID> -o pid,comm,args
```

### What happens when a disk is full?

1. `write()` syscalls return `ENOSPC`.
2. Applications either crash (if they don't handle it), log an error, or enter a retry loop.
3. Databases may corrupt themselves if they can't write WAL entries.
4. SSH sessions may disconnect (sshd can't write to /var/run or /tmp).
5. System logs stop being written.

```bash
# Diagnose
df -h          # check filesystem usage
du -sh /var/log/* | sort -rh   # find large log directories
lsof | grep deleted   # find deleted-but-held-open files

# Quick wins:
> /var/log/app.log     # zero out a log without closing fds
journalctl --vacuum-size=500M   # trim journal
apt clean               # remove cached packages
docker system prune -a  # remove unused Docker images/containers
```

---

## Related Topics

- [DevOps: Linux Deep Dive](../../10-devops/linux/README.md) — production hardening, kernel tuning, security configuration
- [Networking Fundamentals](../../00-foundations/networking/README.md) — TCP/IP, DNS, HTTP at the OS level
- [Operating Systems](../../00-foundations/operating-systems/README.md) — scheduling, virtual memory, synchronization primitives
- [Docker](../../10-devops/docker/README.md) — Linux namespaces and cgroups that power containers
