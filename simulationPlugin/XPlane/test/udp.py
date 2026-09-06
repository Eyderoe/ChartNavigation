import socket
import struct
import plane_pb2

# 魔数头定义
MAGIC_HEAD = b'@yT '


def main():
    MCAST_GRP = '239.255.73.16'
    MCAST_PORT = 57316
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, 'SO_REUSEPORT'):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.bind(('', MCAST_PORT))
    mreq = struct.pack("4sl", socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    print(f"UDP服务器已启动，监听端口 {MCAST_GRP}:{MCAST_PORT}")

    try:
        while True:
            # 接收数据
            data, addr = sock.recvfrom(4096)  # 缓冲区大小4096字节

            # 检查数据长度
            print(data)
            if len(data) < 5:  # 至少需要魔数头(4字节) + 可用飞机数量(1字节)
                print(f"接收到无效数据包，长度不足: {len(data)}字节")
                continue

            # 检查魔数头
            if data[:4] != MAGIC_HEAD:
                print(f"接收到无效数据包，魔数头不匹配")
                continue

            # 解析可用飞机数量
            available = data[4]
            print(f"\n接收到来自 {addr[0]}:{addr[1]} 的数据包，包含 {available} 架飞机")

            # 解析Planes消息
            planes_data = data[5:]  # 从第5字节开始是Planes消息
            planes = plane_pb2.Planes()

            try:
                planes.ParseFromString(planes_data)

                # 输出每架飞机的信息
                for i, plane in enumerate(planes.planes):
                    print("飞机 {}".format(plane.id))
                    print("位置({:.5f}, {:.5f}) {}m {}° {}ft/min".format(plane.lat, plane.lon, plane.alt, plane.trk,
                                                                          plane.vs))
                    print("航班:{}, 机型:{}\n".format(plane.flight, plane.icao))
            except Exception as e:
                print(f"解析Planes消息失败: {e}")

    except KeyboardInterrupt:
        print("\n程序被用户中断")
    finally:
        sock.close()
        print("UDP服务器已关闭")


if __name__ == "__main__":
    main()
