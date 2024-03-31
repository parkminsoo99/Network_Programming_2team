import sys
from PyQt5.QtWidgets import (
    QApplication,
    QMainWindow,
    QPushButton,
    QMessageBox,
    QVBoxLayout,
    QWidget,
)
from PyQt5.QtGui import QFont
import subprocess
from scapy.all import *
import ifaddr
import threading
import random


# TCP 창 크기를 2052로 설정합니다. 이는 TCP 헤더의 일부로, 데이터의 양을 제어하는데 사용됩니다.
DEFAULT_WINDOW_SIZE = 2052

# Scapy의 L3 통신에 대한 기본 소켓을 설정합니다. L3RawSocket은 IP 페이로드에 대한 직접 제어를 가능하게 합니다.
conf.L3socket = L3RawSocket


# log라는 이름의 함수를 정의합니다. 이 함수는 메시지와 파라미터를 출력하는 역할을 합니다.
def log(msg, params={}):
    # 주어진 파라미터를 문자열로 변환하고, 이를 공백으로 연결합니다.
    formatted_params = " ".join([f"{k}={v}" for k, v in params.items()])
    # 메시지와 변환된 파라미터를 함께 출력합니다.
    print(f"{msg} {formatted_params}")


# 로컬 호스트 어댑터인지 확인하는 함수를 정의합니다.
def is_adapter_localhost(adapter, localhost_ip):
    # 어댑터의 IP가 localhost IP와 같은지 확인하고, 그 결과를 반환합니다.
    return len([ip for ip in adapter.ips if ip.ip == localhost_ip]) > 0


# 패킷이 TCP 연결에 있는지 확인하는 함수를 정의합니다.
def is_packet_on_tcp_conn(server_ip, server_port, client_ip):
    def f(p):
        # 패킷이 서버에서 클라이언트로 가는지, 또는 클라이언트에서 서버로 가는지 확인합니다.
        return is_packet_tcp_server_to_client(server_ip, server_port, client_ip)(
            p
        ) or is_packet_tcp_client_to_server(server_ip, server_port, client_ip)(p)

    return f


# 패킷이 TCP를 통해 서버에서 클라이언트로 이동하는지 확인하는 함수를 정의합니다.
def is_packet_tcp_server_to_client(server_ip, server_port, client_ip):
    def f(p):
        # 패킷에 TCP 계층이 있는지 확인합니다.
        if not p.haslayer(TCP):
            return False

        # IP 및 TCP 정보를 추출합니다.
        src_ip = p[IP].src
        src_port = p[TCP].sport
        dst_ip = p[IP].dst

        # 출발지 IP, 출발지 포트, 목적지 IP가 각각 서버 IP, 서버 포트, 클라이언트 IP인지 확인합니다.
        return src_ip == server_ip and src_port == server_port and dst_ip == client_ip

    return f


# 패킷이 TCP를 통해 클라이언트에서 서버로 이동하는지 확인하는 함수를 정의합니다.
def is_packet_tcp_client_to_server(server_ip, server_port, client_ip):
    def f(p):
        # 패킷에 TCP 계층이 있는지 확인합니다.
        if not p.haslayer(TCP):
            return False

        # IP 및 TCP 정보를 추출합니다.
        src_ip = p[IP].src
        dst_ip = p[IP].dst
        dst_port = p[TCP].dport

        # 출발지 IP, 목적지 IP, 목적지 포트가 각각 클라이언트 IP, 서버 IP, 서버 포트인지 확인합니다.
        return src_ip == client_ip and dst_ip == server_ip and dst_port == server_port

    return f


# TCP 연결을 리셋하는 패킷을 생성하고 보내는 함수입니다.
def send_reset(iface, seq_jitter=0, ignore_syn=True):
    def f(p):
        # 각 필드를 패킷에서 추출합니다.
        src_ip = p[IP].src
        src_port = p[TCP].sport
        dst_ip = p[IP].dst
        dst_port = p[TCP].dport
        seq = p[TCP].seq
        ack = p[TCP].ack
        flags = p[TCP].flags

        # 패킷 정보를 로그로 출력합니다.
        log(
            "Grabbed packet",
            {
                "src_ip": src_ip,
                "dst_ip": dst_ip,
                "src_port": src_port,
                "dst_port": dst_port,
                "seq": seq,
                "ack": ack,
            },
        )

        # 패킷이 SYN 플래그를 가지고 있고, SYN 플래그를 무시하는 설정이면, RST를 보내지 않습니다.
        if "S" in flags and ignore_syn:
            print("Packet has SYN flag, not sending RST")
            return

        # 임의의 jitter를 계산합니다.
        jitter = random.randint(max(-seq_jitter, -seq), seq_jitter)
        if jitter == 0:
            print("jitter == 0, this RST packet should close the connection")

        # RST 패킷의 sequence number를 결정합니다.
        rst_seq = ack + jitter

        # RST 패킷을 생성합니다.
        p = IP(src=dst_ip, dst=src_ip) / TCP(
            sport=dst_port,
            dport=src_port,
            flags="R",
            window=DEFAULT_WINDOW_SIZE,
            seq=rst_seq,
        )

        # RST 패킷 정보를 로그로 출력합니다.
        log(
            "Sending RST packet...",
            {
                "orig_ack": ack,
                "jitter": jitter,
                "seq": rst_seq,
            },
        )

        # RST 패킷을 보냅니다.
        send(p, verbose=0, iface=iface)

    return f


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.init_ui()

    def init_ui(self):
        self.setGeometry(400, 400, 400, 200)
        self.setWindowTitle("Cut-Thorough")
        font = QFont("Verdana", 12)  # Verdana글꼴로 변경

        button1 = QPushButton("동영상 허용")
        button1.clicked.connect(self.run_command)
        button1.setMinimumSize(150, 50)  # 동영상 허용버튼 사이즈를 세팅
        button1.setStyleSheet(
            """
        QPushButton {
            color: white;
            background-color: #5cb85c;
            border-style: outset;
            border-width: 2px;
            border-radius: 10px;
            border-color: beige;
            font: bold 14px;
            padding: 6px;
        }
        QPushButton:pressed {
            background-color: #3e8e41;
            border-style: inset;
        }
        """
        )
        button1.setFont(font)

        button2 = QPushButton("동영상 차단")
        button2.clicked.connect(self.stop_command)
        button2.setMinimumSize(150, 50)  # 동영상 차단버튼 사이즈를 세팅
        button2.setStyleSheet(
            """
        QPushButton {
            color: white;
            background-color: #d9534f;
            border-style: outset;
            border-width: 2px;
            border-radius: 10px;
            border-color: beige;
            font: bold 14px;
            padding: 6px;
        }
        QPushButton:pressed {
            background-color: #C9302C;
            border-style: inset;
        }
        """
        )
        button2.setFont(font)

        button3 = QPushButton("TCP 채팅 서버 차단")
        button3.clicked.connect(self.start_sniffing)
        button3.setMinimumSize(150, 50)  # RST 패킷 스니핑 시작 버튼 사이즈를 세팅
        button3.setStyleSheet(
            """
        QPushButton {
            color: white;
            background-color: #337ab7;
            border-style: outset;
            border-width: 2px;
            border-radius: 10px;
            border-color: beige;
            font: bold 14px;
            padding: 6px;
        }
        QPushButton:pressed {
            background-color: #286090;
            border-style: inset;
        }
        """
        )
        button3.setFont(font)

        layout = QVBoxLayout()
        layout.addWidget(button1)
        layout.addWidget(button2)
        layout.addWidget(button3)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        # 레이아웃에 맞게 윈도우 사이즈를 조정합니다.
        self.resize(self.layout().sizeHint())

    def run_command(self):
        # 동영상 허용 명령어 실행
        command = 'sudo iptables -D INPUT -p tcp --dport 0:65535 -m string --string "GET /" --algo kmp -m string --string ".m3u8" --algo kmp -j DROP;sudo iptables -D INPUT -p tcp --dport 0:65535 -m string --string "GET /" --algo kmp -m string --string ".ts" --algo kmp -j DROP; sudo iptables -D INPUT -p tcp --dport 0:65535 -m string --string "GET /" --algo kmp -m string --string ".m4s" --algo kmp -j DROP; sudo iptables -D INPUT -p tcp --dport 0:65535 -m string --string "GET /" --algo kmp -m string --string ".mpd" --algo kmp -j DROP'

        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        QMessageBox.information(
            self, "Success", "HTTP기반 HLS / DASH 프로토콜 동영상을 허용(지연과 트래픽에 주의하세요.)"
        )

    def stop_command(self):
        # 동영상 차단 명령어 실행
        command = 'sudo iptables -A INPUT -p tcp —dport 0:65535 -m string —string "GET /" —algo kmp -m string —string ".m3u8" —algo kmp -j DROP;sudo iptables -A INPUT -p tcp —dport 0:65535 -m string —string "GET /" —algo kmp -m string —string ".ts" —algo kmp -j DROP; sudo iptables -A INPUT -p tcp —dport 0:65535 -m string —string "GET /" —algo kmp -m string —string ".m4s" —algo kmp -j DROP; sudo iptables -A INPUT -p tcp —dport 0:65535 -m string —string "GET /" —algo kmp -m string —string ".mpd" —algo kmp -j DROP'

        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        QMessageBox.information(
            self,
            "Success",
            "HTTP기반의 HLS / DASH 프로토콜 동영상을 차단(관리자에게 문의하여 HTTPS로 동영상을 요청하세요.)",
        )

    def start_sniffing(self):
        # Scapy로 패킷 스니핑을 시작합니다.
        localhost_ip = "127.0.0.1"
        # 로컬 어댑터를 찾습니다.
        local_ifaces = [
            adapter.name
            for adapter in ifaddr.get_adapters()
            if is_adapter_localhost(adapter, localhost_ip)
        ]

        iface = local_ifaces[0]

        # 패킷 스니핑을 시작합니다.
        log("Starting sniff…")
        t = sniff(
            iface=iface,
            count=100,
            prn=send_reset(iface),
        )
        # 패킷 스니핑이 끝났음을 출력합니다.
        log("Finished sniffing!")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
