import pexpect
import time
import os

child = pexpect.spawn('./build/burptui', encoding='utf-8')
time.sleep(1)
print("PID:", child.pid)
child.send('\t\t\t')
time.sleep(2)
os.system(f"gdb -batch -ex 'thread apply all bt' -p {child.pid} > bt_all.txt")
