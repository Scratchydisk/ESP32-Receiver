set remote hardware-watchpoint-limit 2
mon reset halt
flushregs
thb app_main
c