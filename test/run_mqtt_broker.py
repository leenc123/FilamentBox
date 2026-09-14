#!/usr/bin/env python3
"""
FilamentBox 本地测试 MQTT Broker
=================================
基于 amqtt 的轻量级 MQTT 3.1.1 服务端，给 test/bambu_printer_simulator.py
（假打印机）用。只走明文，对应固件 /setup 页的调试模式开关。

启动方式（从工程根目录执行）：

    python test\\run_mqtt_broker.py
    python test\\run_mqtt_broker.py --port 1883 --ws-port 0

默认监听：
    TCP  1883  — MQTT 明文
    WS   8080  — MQTT over WebSocket（传 --ws-port 0 关闭）

功能：
    - 允许匿名连接（测试环境）
    - 完整 Pub/Sub 消息转发（支持 # / + 通配符）
    - 保留消息存储
    - 连接/断开/PUB/SUB 事件日志
    - Ctrl+C 优雅退出
"""

import argparse
import asyncio
import logging
import sys

try:
    from amqtt.broker import Broker
except ImportError:
    print(
        "\n[amqtt] 未安装。请在 mqtt_scripts 虚拟环境中执行：\n"
        "  pip install -r requirements.txt\n",
        file=sys.stderr,
    )
    sys.exit(1)


# ── amqtt 版本说明 ──────────────────────────────────
# 旧版 amqtt 的 Broker.topic_filtering() 有个 bug（topic-check 关闭时仍拦截
# 所有订阅），本文件曾带 monkey-patch；amqtt 0.12 起该方法改为
# _topic_filtering() 且优先检查开关，关闭即直接放行，patch 已不再需要。
# 如订阅被拒（return_code=128），再考虑加回针对性 patch。


# ── 日志配置 ──────────────────────────────────────────────
# amqtt 内部用 logging，统一格式输出
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)-20s] %(levelname)-7s %(message)s",
    datefmt="%H:%M:%S",
)

# 降低 amqtt 内部 DEBUG 噪音，只保留 INFO 及以上
for noisy in ("amqtt.protocol", "amqtt.plugins", "transitions"):
    logging.getLogger(noisy).setLevel(logging.WARNING)

# event_logger_plugin 每个包刷三行 INFO，压到 WARNING 只看真正的异常
logging.getLogger("amqtt.broker.plugins.event_logger_plugin").setLevel(logging.WARNING)

logger = logging.getLogger("mqtt-broker")


# ── 主函数 ────────────────────────────────────────────────
async def _serve(config: dict) -> None:
    # 注意：amqtt 0.12 起 Broker 构造要求已有 running loop，
    # 所以构造 + start 都放在协程里，由 asyncio.run() 驱动。
    broker = Broker(config)
    await broker.start()
    logger.info("Broker 已就绪，等待客户端连接...")
    try:
        await asyncio.Future()
    finally:
        await broker.shutdown()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="NDashboard 本地 MQTT Broker（测试用）"
    )
    parser.add_argument(
        "--port", type=int, default=1883,
        help="MQTT TCP 监听端口（默认 1883）"
    )
    parser.add_argument(
        "--ws-port", type=int, default=8080,
        help="WebSocket 监听端口（默认 8080，传 0 关闭）"
    )
    args = parser.parse_args()

    # amqtt 配置（内联，不依赖外部 YAML）
    listeners = {
        "default": {
            "type": "tcp",
            "bind": f"0.0.0.0:{args.port}",
        }
    }
    if args.ws_port > 0:
        listeners["ws"] = {
            "type": "ws",
            "bind": f"0.0.0.0:{args.ws_port}",
        }

    config = {
        "listeners": listeners,
        "sys_interval": 10,
        "auth": {
            "allow-anonymous": True,
            "plugins": ["auth_anonymous"],
        },
        # 完全禁用主题检查插件（topic_acl / topic_taboo）
        # 仅设 enabled: False 不够，amqtt 仍会加载插件并拒绝所有订阅（return_code=128）
        "topic-check": {
            "enabled": False,
            "plugins": [],
        },
    }

    logger.info("=" * 50)
    logger.info("FilamentBox 测试 Broker 启动")
    logger.info("  TCP 端口 : %d", args.port)
    if args.ws_port > 0:
        logger.info("  WS  端口 : %d", args.ws_port)
    logger.info("  匿名访问 : 已开启")
    logger.info("  Ctrl+C 退出")
    logger.info("=" * 50)

    broker = None
    try:
        asyncio.run(_serve(config))
    except KeyboardInterrupt:
        logger.info("收到退出信号，正在关闭...")
    logger.info("Broker 已停止")


if __name__ == "__main__":
    main()
