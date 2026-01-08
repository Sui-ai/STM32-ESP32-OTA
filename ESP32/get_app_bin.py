import network
import urequests
import os
import time

# ================= 配置区域 =================
WIFI_SSID = "iPhone"
WIFI_PASS = "aaaaaaaa"

# 你的本地服务器地址
URL = "http://172.20.10.6:8000/app.bin"

# 保存到ESP32本地的文件名
LOCAL_FILENAME = "stm32_firmware.bin"
# ===========================================

def connect_wifi():
    """连接WiFi"""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if not wlan.isconnected():
        print(f"正在连接到 {WIFI_SSID} ...")
        wlan.connect(WIFI_SSID, WIFI_PASS)
        
        # 等待连接，超时控制
        max_wait = 10
        while max_wait > 0:
            if wlan.isconnected():
                break
            max_wait -= 1
            time.sleep(1)
            
    if wlan.isconnected():
        print("WiFi连接成功!")
        print("IP地址:", wlan.ifconfig()[0])
        return True
    else:
        print("WiFi连接失败，请检查密码或网络")
        return False

def download_file(url, filename):
    """
    流式下载文件并保存到Flash，避免内存溢出
    """
    print(f"开始从 {url} 下载...")
    
    try:
        # stream=True 是关键，允许分块读取
        response = urequests.get(url, stream=True)
        
        if response.status_code == 200:
            print("服务器响应成功，开始写入文件...")
            
            # 以二进制写模式打开/创建文件
            with open(filename, 'wb') as f:
                total_downloaded = 0
                chunk_size = 1024 # 每次下载1KB
                
                while True:
                    # 读取一小块数据
                    chunk = response.raw.read(chunk_size)
                    if not chunk:
                        break # 读取完毕
                    
                    # 写入文件
                    f.write(chunk)
                    total_downloaded += len(chunk)
                    # 打印进度（可选，为了不刷屏每下载10KB打印一次）
                    if total_downloaded % 10240 == 0:
                        print(f"已下载: {total_downloaded} bytes")
            
            print(f"\n下载完成! 总大小: {total_downloaded} bytes")
            response.close()
            return True
            
        else:
            print(f"下载失败，HTTP状态码: {response.status_code}")
            response.close()
            return False
            
    except Exception as e:
        print(f"下载过程中发生错误: {e}")
        return False

def verify_file(filename):
    """检查文件是否存在以及大小"""
    try:
        stat = os.stat(filename)
        size = stat[6] # 索引6是文件大小
        print(f"校验: 本地文件 '{filename}' 存在，大小为 {size} bytes")
        return size
    except OSError:
        print("校验: 文件不存在！")
        return 0

# ================= 主程序 =================
if __name__ == "__main__":
    # 1. 连接网络
    if connect_wifi():
        # 2. 清理旧文件（可选，wb模式会自动覆盖，但为了演示清晰）
        try:
            os.remove(LOCAL_FILENAME)
            print("已删除旧文件")
        except:
            pass
        
        # 3. 下载
        start_time = time.ticks_ms()
        if download_file(URL, LOCAL_FILENAME):
            duration = time.ticks_diff(time.ticks_ms(), start_time) / 1000
            print(f"耗时: {duration:.2f} 秒")
            
            # 4. 最终确认
            file_size = verify_file(LOCAL_FILENAME)
            
            print("-" * 30)
            print(f"请查看电脑上的 app.bin 大小是否也是 {file_size} 字节？")
            print("如果大小一致，我们就可以进入第二阶段（Ymodem发送）了。")
            print("-" * 30)