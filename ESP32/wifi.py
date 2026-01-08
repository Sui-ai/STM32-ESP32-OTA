import network
import time

def do_connect(ssid, password):
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if not wlan.isconnected():
        print('正在连接到 WiFi:', ssid)
        wlan.connect(ssid, password)
        
        # 设置超时机制，防止死循环卡死
        max_wait = 10 
        while max_wait > 0:
            if wlan.isconnected():
                break
            max_wait -= 1
            print('等待连接...')
            time.sleep(1)
            
        if not wlan.isconnected():
            print('连接失败! 请检查密码或信号。')
            return False
            
    print('网络连接成功!')
    print('网络配置 (IP/netmask/gw/DNS):', wlan.ifconfig())
    return True