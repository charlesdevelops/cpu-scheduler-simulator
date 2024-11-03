# cpu-scheduler-simulator
https://teaching.csse.uwa.edu.au/units/CITS2002/past-projects/p2023-1/summary.php

# Backup of the above URL (can also be found in backup.html)
Project Description

Consider an operating system's 5-State Model of Process Execution, as introduced in Lecture 7. New processes are admitted to the system and are immediately marked as Ready to run. Each process executes, in turn, until it:
completes its execution (at which time the process Exits),
executes for a finite time-quantum (after which the process is marked as Ready and is queued until it can run again),
requests to read or write from an I/O device (at which time the process is marked as Blocked and queued until its I/O request is satisfied),
sleeps for a requested time (at which time the process is marked as Blocked until the requested time elapses), or
waits for all processes that it has spawned to terminate (at which time the process is marked as Blocked until the other processes terminate).
We'll consider a simplified operating system in which only a single process occupies the single CPU at any one time. The CPU has a clock speed of 2GHz, enabling it to execute two-billion instructions per second. We do not need to consider the speed of accessing RAM.

The CPU is connected to a number of input/output (I/O) devices of differing speeds, using a single high-speed data-bus. Only a single process can use the data-bus at any one time.

Only a single process can access each I/O device (and the data-bus) at any one time. If the data-bus is in use (data is still being transferred) and a second process also needs to access the data-bus, the second process must be queued until the current transfer is complete. When a data transfer completes, all waiting (queued) processes are consider to determine which process can next acquire the data-bus. If multiple processes are waiting to acquire the data-bus, the process that has been waiting the longest for the device with the fastest read speed will next acquire the data-bus. Thus, all processes waiting on devices with high read speeds are serviced before any processes that are waiting on devices with lower read speeds.

The result is a need to support Multiple blocked queues, as introduced in Lecture 7.

It takes 5 microseconds to perform a context-switch - to move one process from Ready → Running.
It also takes 10 microseconds to move a process from Running → Blocked, Running → Ready, and Blocked → Ready.
All other state transitions occur instantly (unrealistic, but keeping the project simple).
It takes 20 microseconds for any process to first acquire the data-bus.
You should commence your coding by starting with myscheduler.c
System configuration

Consider the following sample sysconfig file which defines the characteristics of our hardware and the time-quantum for scheduling. Note that lines commencing with a '#' are comment lines, and the 'words' on each line may be separated by one-or-more space or tab characters. All speeds are in bytes-per-second (Bps), all times are in microseconds (usecs), and all I/O sizes are in bytes (B). We may assume that the format of the sysconfig file will always be correct, and its data values consistent, so we do not need to check for errors.

#            devicename   readspeed      writespeed
#
device       usb3.1       640000000Bps   640000000Bps
device       terminal     10Bps          3000000Bps
device       hd           160000000Bps   80000000Bps
device       ssd          480000000Bps   420000000Bps
#
timequantum  100usec
Commands

Our simple operating system supports a small number of commands (which you can modify and extend with with your own commands). When a command is invoked, a new process is created. Multiple instances of the same command may be executing simultaneously. Each process executes a sequence of system-calls until the process exits.
Consider the following sample command file which defines the supported commands and the sequence of system-calls that they make. Each system-call made by a command is indented by a single tab, and is preceded by the elapsed execution time, in microseconds, of the process (the total time it has occupied the CPU). The times when each process's system-calls are made are guarateed to be ascending. Note that lines commencing with a '#' are comment lines, that the 'words' on each line may be separated by one-or-more space or tab characters, and that the commands do not receive or require any commnad-line arguments. We may assume that the format of the commands file will always be correct, and its data values consistent, so we do not need to check for errors.


#
shortsleep
← tab →10usecs    sleep   1000000usecs
← tab →50usecs    exit
#
longsleep
← tab →10usecs    sleep   5000000usecs
← tab →50usecs    exit
#
cal
← tab →80usecs    write   terminal 2000B
← tab →90usecs    exit
#
copyfile
← tab →200usecs   read    hd       48000B
← tab →300usecs   write   hd       48000B
← tab →600usecs   read    hd       21000B
← tab →700usecs   write   hd       21000B
← tab →1000usecs  exit
#
shell
← tab →100usecs   write   terminal 10B
← tab →120usecs   read    terminal 4B
← tab →220usecs   spawn   cal
← tab →230usecs   wait
← tab →1000usecs  exit
#
The only supported system-calls are spawn, read, write, sleep, wait, and exit. Each is demonstrated above.

System Execution

In the above example, when the command, shortsleep is invoked, a new process is created, and it is added to the Ready queue. When the process moves to the CPU (to the Running state), it executes for 10 microseconds and then issues a sleep system-call, requesting that the process sleeps (does nothing) for 1 second. As the process does not require the CPU while sleeping, it will be moved to a Blocked queue until 1 second elapses, after which time the process is again appended to the Ready queue. After 40 more microseconds of execution time (on the CPU), the process exits.
The execution of the shell command is much more complicated. After executing for 100 microseconds on the CPU, the process writes 10 bytes (its shell prompt) to the terminal. After executing a further 20 microseconds the shell process reads in the name of a required command, executes for another 100 microseconds, and then spawns a new process to execute the cal command. There are now two processes executing simultaneously. The shell process next waits for all of the processes that it has created (here, just the cal process). After executing for 80 microseconds, the new process executing the cal command writes its output to the terminal and exit after executing a further 10 microseconds.

Having read both the sysconfig file and the command file, the system 'automatically' commences by executing the first command in the command file (here that would be the shortsleep command - a quite boring system!)

Execution of the project is complete when there are no more processes (in any state).

Project requirements

Your program, named myscheduler, should accept two command-line arguments providing the name of the sysconfig file and the name of the commands file. Your program is only required to produce a single line of output reporting the total time taken (in microseconds) to complete the emulation (or simulation) of the command sequence in the system (i.e. the time at which there are no more running processes), and the CPU-utilisation (the percentage of the time, as an integer, that the CPU is executing processes) For example:
prompt> ./myscheduler  sysconfig-file  command-file
found 3 devices
time quantum is 120
found 6 commands
what a fun project!
measurements  482400  24
Your program can print out any additional information (debugging?) that you want - just ensure that the last line is of the form "measurements  482400  24" (note that those two measurements are just an example).

Your project must be written in the C11 programming language, in a single source-code file named myscheduler.c.
The project can be successfully completed without using any dynamic memory allocation in C (such as with malloc()). You may choose to use dynamic memory allocation, but will not receive additional marks for doing so.
Your project must not depend upon any libraries (such as 3rd-party downloaded from the internet) other than your system's standard library (providing OS, C, and POSIX functions).
