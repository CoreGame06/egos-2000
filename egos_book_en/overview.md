The EGOS book              Skip to content 

AppearanceMenuReturn to top























“What I cannot create, I do not understand.” — Richard Feynman (1918-1988)

This little book contains a series of course projects based on egos-2000. The goal is to help students gain experience and confidence in creating their own operating system (OS) or similar computer systems in their future careers.

### Core projects ​

There are 6 core projects that cover key OS concepts for an undergraduate OS course, such as Cornell CS4411. The projects introduce the architectural support for OS one by one, which students have found very helpful. For example, students will play with the timer and scheduler without any knowledge of privilege modes or page tables.OS conceptsArchitectural supportP0: Hello, World!code, data, heap, stackuser-level ISAP1: Cooperative Threadsthread, yield, context switchP2: Preemptive Schedulinginterrupt and exception handlingcontrol register, timerP3: System Call & Protectioninter-process communicationprivilege modeP4: Virtual Memoryprocess, virtual address spacepage table translationP6: File Systemblock, inode, file, directory

### Advanced projects ​

There are 3 advanced projects for a graduate-level OS course, such as Cornell CS5411 or Northeastern CS6640.OS conceptsArchitectural supportP5: I/O Device Drivermemory-mapped I/OI/O bus, direct memory accessP7: Ethernet & TCP/IPI/O interrupt, IRQ, UDPplatform-level interrupt controllerP8: Multicore & Locksboot loader, mutual exclusionatomic memory operation

All the projects can run on the QEMU emulator as well as real RISC-V hardware. Now, let's get our hands dirty and have fun with OS!