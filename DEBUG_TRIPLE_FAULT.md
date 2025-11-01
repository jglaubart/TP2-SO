# Debugging Triple Fault After _sti()

## Quick Fixes Applied

1. **scheduler.c**: Added `firstInterrupt` flag to prevent overwriting idle process RSP on first interrupt
2. **libasm.asm**: Fixed `stackInit` to use correct registers (rsi for RIP, rdi for RSP value)

## GDB Debugging Instructions

### 1. Setup GDB for your kernel

```bash
# In one terminal, start QEMU with GDB server
qemu-system-x86_64 -s -S -hda Image/x64BareBonesImage.img

# In another terminal, start GDB
gdb
```

### 2. Load symbols and set breakpoints

```
(gdb) target remote :1234
(gdb) symbol-file Kernel/kernel.elf
(gdb) break _sti
(gdb) break _irq00Handler
(gdb) break schedule
(gdb) continue
```

### 3. When _sti() is hit

```
(gdb) print scheduler
(gdb) print scheduler->currentProcess
(gdb) print scheduler->currentProcess->rsp
(gdb) x/20x scheduler->currentProcess->rsp
```

**Check**: The RSP should point to a valid stack frame with the interrupt frame structure.

### 4. When first timer interrupt fires (_irq00Handler)

```
(gdb) info registers rsp
(gdb) x/30x $rsp
(gdb) stepi  # Step through the handler
```

**Check**: Verify the stack has the interrupt frame at the bottom:
- RSP should have: [saved registers...] [RIP] [CS] [RFLAGS] [RSP] [SS]

### 5. Before calling schedule()

```
(gdb) print rsp
(gdb) print scheduler->currentProcess->rsp
(gdb) x/20x $rsp
```

**Check**: The interrupt stack pointer vs. the saved process RSP.

### 6. Inside schedule() - first interrupt

```
(gdb) print scheduler->firstInterrupt
(gdb) print scheduler->currentProcess
(gdb) print scheduler->currentProcess == scheduler->idleProcess
(gdb) print scheduler->currentProcess->rsp
```

**Check**: 
- `firstInterrupt` should be 1
- `currentProcess` should be idle process
- RSP should NOT be overwritten if it's the first interrupt and idle process

### 7. After schedule() returns (new process stack)

```
(gdb) print $rax  # Returned RSP
(gdb) x/30x $rax  # Inspect the new stack
```

**Check**: The stack should have:
- Top: R15 (0)
- Below: R14...RAX (all 0s except rdi=argc, rsi=argv)
- Then: RIP (function address), CS (0x8), RFLAGS (0x202), RSP (stack_top), SS (0x0)

### 8. Before popState

```
(gdb) print $rsp
(gdb) x/20x $rsp
```

**Check**: RSP should point to R15, ready to pop all registers.

### 9. After popState, before iretq

```
(gdb) print $rsp
(gdb) x/10x $rsp
```

**Check**: RSP should now point to RIP (interrupt frame):
- [RIP] [CS] [RFLAGS] [RSP] [SS]

### 10. On triple fault

If you still get a triple fault, check:

```
(gdb) info registers
(gdb) x/20x $rsp
(gdb) print $rip
(gdb) print $rflags
```

**Common issues to check**:
1. **Stack alignment**: Stack must be 16-byte aligned
2. **Interrupt frame**: Must have exactly 5 values: RIP, CS, RFLAGS, RSP, SS
3. **CS value**: Should be 0x8 (kernel code segment)
4. **RFLAGS**: Should be 0x202 (interrupts enabled)
5. **RIP value**: Should point to valid code (shell module address 0x400000)

### 11. Check process stack initialization

```
(gdb) break stackInit
(gdb) continue
# When hit:
(gdb) print $rdi  # stack_top
(gdb) print $rsi  # function/rip
(gdb) print $rdx  # argc
(gdb) print $rcx  # argv
(gdb) stepi  # Step through stackInit
# After stackInit returns:
(gdb) print $rax  # Returned RSP
(gdb) x/30x $rax  # Inspect the initialized stack
```

## Common Bugs to Look For

1. **Wrong RSP saved**: First interrupt saves kernel interrupt stack instead of process stack
2. **Wrong stack frame**: `stackInit` creates incorrect interrupt frame
3. **Register order mismatch**: Saved registers don't match `popState` order
4. **Invalid RIP**: Function address is wrong or invalid
5. **Stack corruption**: Stack pointer becomes invalid

## Manual Stack Verification

The stack after `stackInit` should look like this (from high to low address):
```
RSP -> [R15=0]
       [R14=0]
       ...
       [RAX=0]
       [RBP=0]
       [RDI=argc]
       [RSI=argv]
       ...
       [RIP=function_address]
       [CS=0x8]
       [RFLAGS=0x202]
       [RSP=stack_top]
       [SS=0x0]
```

After `popState`, RSP should point to RIP.

