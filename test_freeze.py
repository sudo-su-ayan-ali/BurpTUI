import pexpect
import time

child = pexpect.spawn('./build/burptui', encoding='utf-8')
time.sleep(1)
# Send Tab 3 times to switch to Decoder (Proxy -> History -> Repeater -> Decoder)
child.send('\t\t\t')
time.sleep(1)
print("Is alive after tab?", child.isalive())
