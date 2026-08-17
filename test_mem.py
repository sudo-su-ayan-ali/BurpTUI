import pexpect
import time
import os

child = pexpect.spawn('./build/burptui', encoding='utf-8')
time.sleep(1)
print("PID:", child.pid)
child.send('\t\t\t')
for i in range(5):
    time.sleep(1)
    os.system(f"ps -p {child.pid} -o rss=")
