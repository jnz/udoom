target remote localhost:3333
monitor reset halt
load
break main
set print pretty on
set pagination off
set disassemble-next-line on
