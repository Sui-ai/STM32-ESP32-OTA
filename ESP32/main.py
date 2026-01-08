import machine
import network
import urequests
import time
import struct
import os
import wifi  # 你的wifi连接文件

# ================= 配置区 =================
ssid = "iPhone"
password = "aaaaaaaa"

# 版本号地址
VERSION_URL = "http://172.20.10.6:8000/version.txt"
# 固件下载地址 (注意：这里要改成 .bin 的地址)
FIRMWARE_URL = "http://172.20.10.6:8000/app.bin"
# 本地保存的文件名
LOCAL_BIN_NAME = "update.bin"

# ESP32 与 STM32 通信接口 (UART2)
UART_ID = 2
BAUD_RATE = 115200
Tx_Pin = 17
Rx_Pin = 16

# 初始化串口
uart = machine.UART(UART_ID, baudrate=BAUD_RATE, tx=Tx_Pin, rx=Rx_Pin, timeout=1000, rxbuf=2048)

# ================= Ymodem 协议常量 =================
SOH = b'\x01'  # 128字节数据包
STX = b'\x02'  # 1024字节数据包
EOT = b'\x04'  # 结束传输
ACK = b'\x06'  # 应答
NAK = b'\x15'  # 重发
CAN = b'\x18'  # 取消
CRC_C = b'\x43' # 'C' 字符

# ================= 工具函数：CRC16-XMODEM =================
# CRC16-XMODEM 查找表 (poly=0x1021)
_CRC16_TABLE = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
]

def calc_crc16(data, crc=0):
    for byte in data:
        crc = (crc << 8) ^ _CRC16_TABLE[((crc >> 8) ^ byte) & 0xFF]
        crc &= 0xFFFF
    return crc

# ================= 核心功能：下载固件 =================
def download_firmware():
    """下载固件到本地 Flash"""
    print(f"[Download] 开始下载: {FIRMWARE_URL}")
    try:
        # 先删除旧文件
        try: os.remove(LOCAL_BIN_NAME)
        except: pass

        response = urequests.get(FIRMWARE_URL, stream=True)
        if response.status_code == 200:
            total_len = 0
            with open(LOCAL_BIN_NAME, 'wb') as f:
                while True:
                    chunk = response.raw.read(1024)
                    if not chunk: break
                    f.write(chunk)
                    total_len += len(chunk)
                    print(f"\r[Download] 已写入: {total_len} bytes", end='')
            print(f"\n[Download] 下载完成，总大小: {total_len}")
            response.close()
            return True
        else:
            print(f"[Download] 服务器错误: {response.status_code}")
            response.close()
            return False
    except Exception as e:
        print(f"[Download] 异常: {e}")
        return False

# ================= 核心功能：Ymodem 发送 =================
def send_packet(uart, type_byte, seq, data):
    """组包并发送"""
    # 1. 头 (SOH/STX)
    pkt = type_byte
    # 2. 序号 (seq, ~seq)
    pkt += struct.pack('BB', seq & 0xFF, (255 - seq) & 0xFF)
    # 3. 数据
    pkt += data
    # 4. CRC
    crc = calc_crc16(data)
    pkt += struct.pack('>H', crc) # 大端序
    
    uart.write(pkt)

def wait_for_char(uart, char, timeout_sec=10):
    """等待特定字符"""
    start = time.time()
    while time.time() - start < timeout_sec:
        if uart.any():
            read_bytes = uart.read(1)
            if read_bytes == char:
                return True
    return False

def ymodem_send_file(uart, filename):
    print("\n[Ymodem] ==== 开始 Ymodem 发送流程 ====")
    
    try:
        file_size = os.stat(filename)[6]
        f = open(filename, 'rb')
    except:
        print("[Ymodem] 文件无法打开")
        return False

    # --- 阶段1: 等待接收端发送 'C' ---
    print("[Ymodem] 等待 STM32 启动 (等待 'C')...")
    if not wait_for_char(uart, CRC_C, timeout_sec=60): # 给足时间让STM32重启
        print("[Ymodem] 超时未收到 'C'")
        f.close()
        return False
    print("[Ymodem] 握手成功，准备发送起始帧")

    # --- 阶段2: 发送起始帧 (Block 0) ---
    # 文件名(不带路径) + \0 + 文件大小字符串 + \0
    short_name = filename.split('/')[-1]
    header_data = short_name.encode('utf-8') + b'\x00' + str(file_size).encode('utf-8') + b'\x00'
    
    if len(header_data) < 128:
        header_data += b'\x00' * (128 - len(header_data))
    
    success = False
    for retry in range(10): 
        # 清空接收缓存，避免旧数据干扰
        while uart.any(): uart.read()
        
        print(f"[Ymodem] 发送起始帧 (第 {retry+1} 次)...")
        send_packet(uart, SOH, 0, header_data)
        
        # 这里的逻辑修改了：不仅仅死等 ACK，而是打印收到的所有内容
        start_wait = time.time()
        got_ack = False
        
        while time.time() - start_wait < 3: # 3秒超时
            if uart.any():
                b = uart.read(1)
                if b == ACK:
                    print(" -> 收到 ACK!")
                    got_ack = True
                    break
                elif b == NAK:
                    print(" -> 收到 NAK (校验失败，STM32要求重发)")
                    # 收到 NAK 后应该立即重发，跳出当前等待循环
                    break 
                elif b == CRC_C:
                    print(" -> 收到 'C' (STM32还在请求开始)")
                else:
                    print(f" -> 收到未知字节: {b}")
            time.sleep(0.01)
            
        if got_ack:
            # ACK 后还需要等待一个 'C' 才能进入正题
            print("等待 STM32 再次发送 'C' 以确认数据传输...")
            if wait_for_char(uart, CRC_C, 5):
                success = True
                break
            else:
                print("ACK后未收到 C")
    
    if not success:
        print("[Ymodem] 起始帧发送彻底失败")
        f.close()
        return False

    # --- 阶段3: 发送文件数据 ---
    print("[Ymodem] 开始发送数据...")
    seq = 1
    total_sent = 0
    
    while True:
        # 读取 1024 字节 (使用 STX)
        data = f.read(1024)
        if not data:
            break # 发完了
            
        pad_len = 1024 - len(data)
        if pad_len > 0:
            data += b'\x1A' * pad_len # CTRL-Z 填充
        
        # 发送数据包
        pack_sent = False
        for retry in range(5):
            send_packet(uart, STX, seq, data)
            
            # 等待应答
            if wait_for_char(uart, ACK, 1):
                pack_sent = True
                # print(f"\rPack {seq} OK", end='')
                break
            else:
                # 某些时候需要处理缓冲区里的垃圾
                while uart.any(): uart.read()
                
        if not pack_sent:
            print(f"\n[Ymodem] 数据包 {seq} 发送失败，终止")
            f.close()
            return False
        
        seq = (seq + 1) % 256
        total_sent += 1024
        print(f"\ntotal_sent:{total_sent}\n")
    f.close()
    print("\n[Ymodem] 数据发送完毕，准备结束")

    # --- 阶段4: 结束传输 (EOT) ---
    # 第一次发送 EOT
    uart.write(EOT)
    # 按照标准 Ymodem，第一次 EOT 接收端会回 NAK
    wait_for_char(uart, NAK, 2) 
    
    # 第二次发送 EOT
    uart.write(EOT)
    # 这次应该回 ACK
    if not wait_for_char(uart, ACK, 2):
        print("[Ymodem] EOT 未收到 ACK")
        return False
    
    # ACK 后接收端再次请求 'C' (准备接收结束帧)
    wait_for_char(uart, CRC_C, 2)

    # --- 阶段5: 发送全空结束帧 (Block 0) ---
    empty_data = b'\x00' * 128
    send_packet(uart, SOH, 0, empty_data)
    if wait_for_char(uart, ACK, 2):
        print("[Ymodem] 升级全部完成！！")
        return True
    
    return False

# ================= 业务逻辑整合 =================

def perform_ota_process(cloud_ver_str):
    """执行完整的 OTA 流程"""
    # 1. 下载
    if not download_firmware():
        send_response(0xFF, "DL_Fail") # 通知STM32下载失败，不要复位
        return

    # 2. 校验文件大小 (简单校验)
    try:
        size = os.stat(LOCAL_BIN_NAME)[6]
        if size == 0: raise Exception("Empty file")
    except:
        send_response(0xFF, "File_Err")
        return

    # 3. 通知 STM32 复位进入 Bootloader
    print(f"[OTA] 准备就绪，通知 STM32 复位 (版本 {cloud_ver_str})")
    send_response(0x01, cloud_ver_str)
    
    # 4. 关键：给 STM32 一点时间处理串口消息并进行复位
    time.sleep(1.0) 
    
    # 5. 清空串口缓冲区，准备接管
    while uart.any(): uart.read()
    
    # 6. 开始 Ymodem
    success = ymodem_send_file(uart, LOCAL_BIN_NAME)
    
    if success:
        print("[OTA] 成功！等待 STM32 运行新固件")
    else:
        print("[OTA] 失败...")

# (这里保留原有的 版本比较函数 和 send_response 函数)
def is_new_version(cloud_ver_str, local_ver_str):
    try:
        c_clean = cloud_ver_str.lower().replace('v', '').strip()
        l_clean = local_ver_str.lower().replace('v', '').strip()
        c_val = float(c_clean)
        l_val = float(l_clean)
        print(f"[VerCheck] Cloud({c_val}) vs Local({l_val})")
        return c_val > l_val
    except Exception as e:
        print(f"[VerError] 解析失败: {e}")
        return False

def send_response(flag, version_str):
    try:
        ver_bytes = version_str.encode('utf-8')
        length = 1 + len(ver_bytes) 
        header = struct.pack('BBBB', 0xAA, 0x55, 0x02, length)
        payload = struct.pack('B', flag) + ver_bytes
        tail = struct.pack('BB', 0x0D, 0x0A)
        packet = header + payload + tail
        uart.write(packet)
        print(f"[UART TX] 发送响应 -> Flag: {flag}, Ver: {version_str}")
    except Exception as e:
        print(f"[UART TX Error] {e}")

def check_cloud_version(local_ver_str):
    print(f"[HTTP] 正在检查更新，本地版本: {local_ver_str}")
    try:
        res = urequests.get(VERSION_URL)
        if res.status_code == 200:
            cloud_ver_str = res.text.strip()
            print(f"[HTTP] 云端版本: {cloud_ver_str}")
            
            if is_new_version(cloud_ver_str, local_ver_str):
                # ============ 变动点 ============
                # 以前是直接发 0x01，现在调用整个流程
                perform_ota_process(cloud_ver_str)
                # ==============================
            else:
                send_response(0x00, cloud_ver_str)
        else:
            print(f"[HTTP] 服务器错误: {res.status_code}")
            send_response(0xFF, "Err_Srv")
        res.close()
    except Exception as e:
        print(f"[HTTP] 请求异常: {e}")
        send_response(0xFF, "Err_Net")

# ================= 主程序 (基本没变) =================
def main():
    print(f"系统就绪! 正在监听 UART{UART_ID}...")
    while uart.any():
        uart.read()
        time.sleep(0.5)
        
    while True:
        try:
            if uart.any():
                time.sleep(0.05) 
                buffer = uart.read()
                if buffer and len(buffer) >= 6:
                    for i in range(len(buffer) - 1):
                        if buffer[i] == 0xAA and buffer[i+1] == 0x55:
                            try:
                                if i + 4 > len(buffer): break 
                                cmd = buffer[i+2]
                                data_len = buffer[i+3]
                                packet_end = i + 4 + data_len + 2
                                if packet_end <= len(buffer):
                                    payload = buffer[i+4 : i+4+data_len]
                                    tail = buffer[i+4+data_len : packet_end]
                                    if tail == b'\x0D\x0A':
                                        local_ver = payload.decode('utf-8')
                                        print(f"[UART RX] 收到查询: {local_ver}")
                                        check_cloud_version(local_ver)
                                        break
                            except Exception as parse_e:
                                print(f"[Parse Error] {parse_e}")
            time.sleep(0.1)
        except Exception as e:
            print(f"[Main Loop Error] {e}")
            time.sleep(1)

if __name__ == '__main__':
    print("等待 3 秒后启动 WiFi...")
    time.sleep(3)
    try:
        if wifi.do_connect(ssid, password):
            main()
        else:
            print("WiFi 连接失败")
    except Exception as e:
        print("启动异常:", e)