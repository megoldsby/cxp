cxp
===

CXP is a small runtime executive based on the CSP parallel, communication and alternation constructs. 

I wanted an embedded executive that was a real pleasure for me to use, so I wrote CXP.  CXP allows dynamic process creation and destruction and handles process intercommunication and synchronization.  It does preemptive priority scheduling and supports multiple processing units that share memory.

CXP processes are low-overhead and meant to be created in profusion.  You create processes using the `par` construct, of which there are four varieties (explained below).  Processes intercommunicate and synchronize with each other by means of communication over channels, using the `in` and `out` functions.  Communication is synchronous, so it also synchronizes the communicating process pair.

Alternation allows input from any number of different sources (including timeouts). It's like the UNIX `select` operation.

The implementaton language of CXP is C, and the present implementation has 32-bit addresses (compiler switch `-m32` for the BNU C compiler).

CXP bears an obvious debt to the occam language and also to CCSP and CPPCSP2 projects. It shamelessly copies most of the implementation of alternation and also most of the communcation implementation from the latter.

Channels are the only shared variables, and all other variables are owned by one process and accessed only by that process. The system manages the channels, freeing the application programmer from having to worry about the trickier aspects of concurrency. Each channel has exactly one reading process and one writing process, although it is permissible to pass the reading or writing privilege to another process; you may send a (pointer to a) channel over a channel or use it as an argument to a newly created process.

CXP cannot enforce non-sharing of variables, however, or the one-reader one-writer restriction. occam prevents shared access to non-constant variables and prevents aliasing (the accessing of a shared variable by different variables names, which can conceal concurrent access) and multiple senders or receivers on a channel; CPPCSP2, written in C++,  makes at least some attempts at doing something similar.  CXP, on the other hand, leaves such matters entirely to the programmer, so, to quote Kid Sally Palumbo's mother's advice to her son in _The Gang That Couldn't Shoot Straight_, "Watch-a yo' ass!"

The system is structured as a library.  The application calls CXP services and links the CXP object code with the application program.

I intend CXP to run on bare hardware.  Just to get things rolling, however, I have made a prototype implemenation over an operating system; that is, I use operating system constructs to implement the same interface that CXP will use over bare hardware. I hope that the hardware implementations will be suitable for realtime use.

My original operating system target for the prototype was Linux.  The most straightforward implementation is to use a pthread for each (simulated) processor and `swapcontext` for context switching on each processor.  However, the Linux implementation of `swapcontext` is not adequate for preemptive scheduling.  Therefore I made the decision to switch to Solaris for development work and never looked back.  Solaris has been a joy to work with.

The system is presently in what I would call an early alpha state.

Next is an explanation of each of the system's primitives with a few examples.

When a program that uses CXP starts, only the initial process (the C `main` program) is running.  It may create child processes by means of the `par` function.  The parent (the initial process) waits until all the children are done before it proceeds.  Any process may use `par` to create child processes, in a nested fashion.
The actual use of `par` looks like this:

    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 1024, 1024 };
    par(children, args, stacksize, 2);

When and if the child processes terminate, the parent falls through the `par` statement.

The are three more varieties of the parallel function, `par_pri`, `placed_par` and `placed_par_pri`.  `par_pri` imposes priorities on the processes: the first process gets the highest priority, the second the next highest and so on.  The system assigns the priorities automatically.  In order to keep the process descriptor small, we place some limits on process prioirities: there may be up to four nested levels of `pri_par` statements (intervening levels with  non-priority `par` statements don't count), each containing up to eight processes.  Ordinary `par` statments create child processes with the same priority as the parent and may contain any number of processes.

`par` and `pri_par` each create the child processes on the same processing unit as the parent, whereas `placed_par` and `placed_par_pri` specify the processing unitsexplicitly.  For example,

    code_p children[] = { child1, child2, child3 };
    void *args[] = { 10, 22, 31 };
    uint stacksize[] = { 1024, 1024, 1024 };
    uint place[] = { 0, 1, 2 };
    placed_par(children, args, stacksize, place, 2);

The number of processors in the current system is a compile-time constant.

Communication occurs through variables of type `Channel`.  Channels give an important degree of freedom in program construction.  Each channel connects exactly two processes.  When the sending process issues an `out` operation and the receiving process issues an `in` operation, data transfer occurs.  If the `out` precedes the `in`, the sender waits, and if the `in` precedes the `out`, the receiver waits.  The data length may be zero, in which case the communication is purely a synchronization.  It is possible to send a `Channel` variable, or a pointer to a `Channel` variable, over a channel, making dynamic configuration of communciation networks possible.  Below are two processes that send a value back and forth, incrementing it each time.

    Channel chan1, chan2;
    void child1() {
        int x = 0;
        while (true) {
            out(&chan1, &x, sizeof(x));
            in(&chan2, &x, sizeof(x));
            x += 1;
        }
    }
    void child2() {
        int y;
        while (true) {
            in(&chan1, &y, sizeof(y));
            y += 1;
            out(&chan2, &y, sizeof(y));
        }
    }
    init_channel(&chan1);
    init_channel(&chan2);
    code_p children[] = { child1, child2 };
    void *args[] = { NULL, NULL };
    uint stacksize[] = { 1024, 1024 };
    par(children, args, stacksize, 2);
    
Alternation allows a process to wait for any of multiple sources of input. An `Alternation` construct contains an array of "guards"; each guard may be one of three types, channel, timeout or skip.  The process issuing the alternation must be the receiver of any channel used as a guard.  A channel guard becomes ready when the sender to that channel executes an `out` against it.  A timeout guard becomes ready when the system time becomes equal to the value specified in the guard.  A skip guard is always ready.  The process doing the alternation issues a selection against the `Alternation` variable, using either function `fairSelect` or function `priSelect`.

Each selection function waits until a guard becomes ready and then selects a ready guard and returns its index in the guard array.  If it is a channel guard, the process must by convention read from that channel.  The two selection functions differ only in their behavior when two or more guards become ready simultaneously. `priSelect` choses the one with the lowest index, and `fairSelect` chooses the first one it encounters when searching (cyclically) from one past the index selected the last time the same `Alternation` variable was used.

Here is a process that waits for input on either `channel0` or `channel1`:

    Channel channel0, channel1;
    void proc() {

        Guard guards[2];
        init_channel_guard(&guards[0], &channel0);
        init_channel_guard(&guards[1], &channel1);

        Alternation alt;
        init_alt(&alt, guards, 2);

        int selection = fairSelect(&alt);
        int x;
        switch (selection) {
        case 0:
            in(&channel0, &x, sizeof(x));
            break;
        case 1:
            in(&channel1, &x, sizeof(x));
            break;
        }
    ...
    }

Here is a process that waits for an input on `chan` within the next 10 seconds.

    #define NANOS_PER_SEC 1000000000LLU
    Channel chan;
    void proc() {

        Guard guards[2];
        init_alt(&alt, guards, 2);
        init_channel_guard(&guards[0], &chan);
        init_timer_guard(&guards[1], Now() + 10*NANOS_PER_SEC);

        int x = -1;
        int selection = priSelect(&alt);
        switch (selection) {
        case 0:
            in(&chan, &x, sizeof(x));
            break;
        case 1:
            printf("timeout\n");
            break;
        }
    ...
    }
    
As can be seen above, CXP provides a function `Now()` that returns the current system time in nanoseconds.  For waiting for time to pass outside an alternation, CXP has the function `After(Time time)`, which delays
the calling process until the current system time is at least `time`, a 64-bit nanosecond value.

Features I might add in the future:
   - multi-user channels (multiple writers or readers)
   - USB/network driver
   - forked processes
   - extended rendezvous (receiving process can perform an action after reading a value but before freeing sender to proceed)
   - channel ends (separate data structures for writing to and reading from a channel)
   - time slicing 
   - "poisoning" of channels (to make tear-down of  communication networks easier)
   
There are a couple of convenience scripts for making and running programs contained in a single module.  For example,
    ./run1 examples/ring
will (make if necessary and) run the program with source `examples/ring.c`.
    ./make1 examples/ring
will just make it.


------------------------------------------------------------------------------------------------------------


Here's what I did to get started in Solaris:

Download the Solaris Virtualbox template from the Oracle site: http://www.oracle.com/technetwork/server-storage/solaris11/vmtemplates-vmvirtualbox-1949721.html.

Install virtualbox (`sudo apt-get install virtualbox` in Ubuntu) on the host.

Start virtualbox and select "Import Appliance".  Select the `.ova` file you have just downloaded from Oracle.

(If you want, you can instead download a live Solaris 11.2 iso file, burn it to a DVD and install it into an unused partition, but I find it more convenient to work in a virtual machine.)  

Follow the prompts in the installation.

Make sure your virtual Solaris has the GNU development utilities:

    pkg install pkg://solaris/archiver/gnu-tar
    pkg install pkg://solaris/developer/gnu-binutils
    pkg install pkg://solaris/diagnostic/top
    pkg install pkg://solaris/file/gnu-coreutils
    pkg install pkg://solaris/file/gnu-findutils
    pkg install pkg://solaris/text/gawk
    pkg install pkg://solaris/text/gnu-diffutils
    pkg install pkg://solaris/text/gnu-grep
    pkg install pkg://solaris/text/gnu-sed
    pkg install pkg://solaris/developer/build/gnu-make
    pkg install pkg://solaris/developer/build/make
    pkg install pkg://solaris/developer/gcc-48
    pkg install pkg://solaris/system/header
    pkg install pkg://solaris/developer/build/autoconf
    pkg install pkg://solaris/developer/build/automake-110

Download the gcc 4.9.2 compiler from the GNU gcc site and untar it. It will go into directory gcc-4.9.2; cd to that directory and run
    ./contrib/download_prerequisites
which will download various packages required to build gcc.

Create a build directory (say `gcc-build`) and `cd` to it.  In that directory,
    <path/to/gcc-4.9.2>/configure --prefix=/opt/gcc-4.9.2 --disable-tui

(The `--disable-tui` option gets rid of an unresolved symbol).

Then still in the build directory, do

    gmake
    gmake install




    





















  
